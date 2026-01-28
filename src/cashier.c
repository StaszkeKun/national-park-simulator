#include <poll.h>
#include "config.h"
#include "utils.h"
#include "constants.h"
#include "types.h"

void init();
void end_simulation();
bool wait_for_visitor();
void sell_ticket(visitor_data_t* visitor_data);
void log_raport(unsigned long pid);
void end_log();

int msgid;
int fifo_regular = -1;
int fifo_vip = -1;
struct pollfd fds[2];
FILE* log_file = NULL;

//shared memory pointers
shared_data_t* shared_data;
visitor_data_t* visitors_data;
guide_data_t* guides_data;

//raport variables
unsigned int visitors_today = 0;
unsigned int kids_today = 0;
unsigned int gold_today = 0;
bool setup_today = false;
unsigned int day = 0;

//kills and logs leaving visitors
pthread_t leaving_thread = 0;
void* print_leaving(void* arg)
{
    (void)arg;
    visitor_data_t* visitor_data;
    while(true)
    {
        long visitor_pid = b_msq_receive(msgid, 2);
        b_raise(SIG_WAKE_UP);
        visitor_data = b_get_visitor_by_pid(visitors_data, visitor_pid);
        if (visitor_data->isVIP)
        {
            printf("[CASHIER]: VIP %ld left\n", visitor_pid);
            visitor_data->status = VS_LEAVING;
            b_signal(visitor_pid, SIGINT);
        }
        else
        {
            printf("[CASHIER]: Visitor %ld with %d kids left\n", visitor_pid, visitor_data->kids_count);
            visitor_data->status = VS_LEAVING;
            b_signal(visitor_pid, SIGINT);
        }
    }
    return NULL;
}

volatile sig_atomic_t kill_requested = 0;
void handle_kill(int sig)
{
    (void)sig;
    kill_requested = 1;
}

int main()
{
    init();

    pid_t current_pid;
    bool night = false;

    while(!kill_requested)
    {
        //setup for new day
        if (b_get_time_of_day(shared_data->start_time) <= OPEN_TIME && !setup_today)
        {
            setup_today = true;
            if (day != 0) end_log();
            if (log_file != NULL) fclose(log_file);
            log_file = NULL;

            night = false;
            visitors_today = 0;
            kids_today = 0;
            gold_today = 0;
            day++;

            if (day > DAYS_LIMIT)
            {
                break;
            }
            printf("//////////DAY %d//////////\n", day);

            char title[100];
            char date[32];
            time_t s = (time_t)shared_data->start_time;
            strftime(date, sizeof(date), "%Y-%m-%d_%H:%M:%S", localtime(&s));
            snprintf(title, sizeof(title), "./logs/%s_day_%d.log", date, day);
            if (mkdir("./logs", 0755) == -1 && errno != EEXIST) perror("[ERROR] can't create logs directory");
            log_file = fopen(title, "w");
        }

        //tell visitors to leave and flush waiting queue
        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME && setup_today)
        {
            printf("//////////NIGHT FELL//////////\n");
            night = true;
            for(int i = 0; i < GUIDES_NUMBER; i++)
            {
                b_signal(guides_data[i].pid, SIG_LEAVE_PARK);
            }
            for(int i = 0; i < VISITORS_LIMIT; i++)
            {
                if (b_process_exist(visitors_data[i].pid))
                {
                    b_signal(visitors_data[i].pid, SIG_WAKE_UP);
                }
            }
            while(true)
            {
                long pid_in_queue = b_msq_receive_nowait(msgid, 1);
                if (pid_in_queue == 0) break;
                visitor_data_t* queue_visitor_data = b_get_visitor_by_pid(visitors_data, pid_in_queue);
                printf("[CASHIER]: Visitor %ld with %d kids dismissed - closing hours\n", pid_in_queue, queue_visitor_data->kids_count);
                queue_visitor_data->status = VS_LEAVING;
                b_signal(pid_in_queue, SIGINT);
            }

            setup_today = false;
        }

        //waits for fifo events
        if (!wait_for_visitor()) continue;

        //VIP queue
        if (fds[1].revents & POLLIN)
        {
            b_fifo_read(fifo_vip, &current_pid, sizeof(pid_t));
        }//regular queue
        else if (fds[0].revents & POLLIN)
        {
            b_fifo_read(fifo_regular, &current_pid, sizeof(pid_t));
        }
        else
        {
            continue;
        }

        visitor_data_t* current_visitor_data = b_get_visitor_by_pid(visitors_data, current_pid);

        if (night)
        {
            printf("[CASHIER]: Visitor %d with %d kids dismissed - closing hours\n", current_pid, current_visitor_data->kids_count);
            current_visitor_data->status = VS_LEAVING;
            b_signal(current_pid, SIGINT);
            continue;
        }

        if (CONSTANT_VISITOR_SPAWN && 1 + current_visitor_data->kids_count + visitors_today > VISITORS_LIMIT)
        {
            printf("[CASHIER]: Visitor %d with %d kids dismissed - daily visitor limit\n", current_pid, current_visitor_data->kids_count);
            current_visitor_data->status = VS_LEAVING;
            b_signal(current_pid, SIGINT);
            continue;
        }

        sell_ticket(current_visitor_data);
    }

    end_simulation();
    return 0;
}

void init()
{
    struct sigaction sa;
    sa.sa_handler = handle_kill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGXCPU, &sa, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_WAKE_UP);
    sigprocmask(SIG_BLOCK, &set, NULL);

    msgid = b_msq_get_id(MSG_CASHIER);

    shared_data = b_shm_attach(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    visitors_data = b_shm_attach(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    guides_data = b_shm_attach(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

    shared_data->cashier_pid = getpid();

    fifo_regular = b_fifo_open(TICKET_REGULAR_PATH, O_RDONLY);
    fifo_vip = b_fifo_open(TICKET_VIP_PATH, O_RDONLY);

    fds[0].fd = fifo_regular;
    fds[0].events = POLLIN;
    fds[1].fd = fifo_vip;
    fds[1].events = POLLIN;

    leaving_thread = b_execute_thread(print_leaving);

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

void end_simulation()
{
    if (kill_requested)
    {
        end_log();
        if (log_file != NULL) fclose(log_file);
    }
    else b_signal(getppid(), SIGINT);

    if (leaving_thread)
    {
        pthread_cancel(leaving_thread);
        pthread_join(leaving_thread, NULL);
    }

    b_shm_dettach(shared_data);
    b_shm_dettach(visitors_data);
    b_shm_dettach(guides_data);

    if (fifo_regular >= 0) b_fifo_close(fifo_regular);
    if (fifo_vip >= 0) b_fifo_close(fifo_vip);

    exit(EXIT_SUCCESS);
}

bool wait_for_visitor()
{
    int ret = poll(fds, 2, -1);
    if (ret == -1)
    {
        if (errno == EINTR)
        {
            return false;
        }
        perror("[ERROR]: poll error");
        end_simulation();
    }
    return true;
}

void sell_ticket(visitor_data_t* visitor_data)
{
    b_sleep(TICKET_SALE_TIME, (volatile sig_atomic_t*[]){&kill_requested}, 1);

    if (kill_requested) return;

    int sum = 0;
    int sold = 0;
    if (visitor_data->isVIP)
    {
        b_signal(visitor_data->pid, SIG_WAKE_UP);
        printf("[CASHIER]: let VIP %d in\n", visitor_data->pid);
        log_raport(visitor_data->pid);
        visitors_today++;
        visitor_data->status = VS_AWAITING_GUIDE; //this tells VIP to choose a direction and start going by themselves
        b_signal(visitor_data->pid, SIG_WAKE_UP);
        return;
    }

    sum += TICKET_PRICE;
    sold++;
    log_raport(visitor_data->pid);
    visitors_today++;
    for(int i = 0; i < visitor_data->kids_count; i++)
    {
        if (visitor_data->kids[i].age >= 7)
        {
            sum += TICKET_PRICE;
            sold++;
        }
        log_raport(visitor_data->kids[i].tid);
        kids_today++;
    }

    gold_today += sum;

    printf("[CASHIER]: sold %d ticket(s) to %d with %d kids\n", sold, visitor_data->pid, visitor_data->kids_count);
    printf("[CASHIER]: let visitor %d in\n", visitor_data->pid);
    for(int i = 0; i < visitor_data->kids_count; i++)
    {
        printf("[CASHIER]: let kid %ld in under visitor %d protection\n", visitor_data->kids[i].tid, visitor_data->pid);
    }
    visitor_data->status = VS_AWAITING_GUIDE;

    while(b_msq_available_slots(msgid) <= 1)
    {
        b_wait_for_wakeup(); //this helps leave 1 message slot available preventing deadlocks
        if (kill_requested) return;
    }

    b_msq_send(msgid, 1, visitor_data->pid, &kill_requested);
    b_signal(visitor_data->pid, SIG_WAKE_UP);
}

void log_raport(unsigned long pid)
{
    if (log_file == NULL)
    {
        printf("[LOG ERROR BACKUP]: %ld\n", pid);
        return;
    }

    fprintf(log_file, "%ld\n", pid);
}

void end_log()
{
    if (log_file == NULL)
    {
        printf("[LOG ERROR BACKUP]: day: %d\n", day);
        printf("[LOG ERROR BACKUP]: gold earned: %d\n", gold_today);
        printf("[LOG ERROR BACKUP]: visitors entries today: (%d + %d kids)\n", visitors_today, kids_today);
        printf("[LOG ERROR BACKUP]: end\n");
        return;
    }

    fprintf(log_file, "gold earned: %d\n", gold_today);
    fprintf(log_file, "visitors entries today: (%d + %d kids)\n", visitors_today, kids_today);
}