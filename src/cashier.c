#include <poll.h>
#include "config.h"
#include "utils.h"
#include "constants.h"
#include "types.h"

void init();
void end_simulation();
bool wait_for_visitor();
void sell_ticket(visitor_data_t* visitor_data);
void log_today();

int msgid;
int fifo_regular = -1;
int fifo_vip = -1;
struct pollfd fds[2];
pid_t current_pid;
shared_data_t* shared_data;
visitor_data_t* visitors_data;
guide_data_t* guides_data;
unsigned long visitors_pids[VISITORS_LIMIT];
unsigned int visitors_today = 0;
unsigned int gold_today = 0;
bool setup_today = false;
unsigned int day = 0;

pthread_t leaving_thread = 0;
void* print_leaving(void* arg)
{
    (void)arg;
    visitor_data_t* visitor_data;
    while(true)
    {
        long visitor_pid = b_msq_receive(msgid, 2);
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

void handle_wake_up(int sig)
{
    (void)sig;
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

    while(!kill_requested)
    {
        if (b_get_time_of_day(shared_data->start_time) <= OPEN_TIME && !setup_today)
        {
            visitors_today = 0;
            gold_today = 0;
            day++;
            setup_today = true;
            printf("//////////DAY %d//////////\n", day);
        }

        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME && setup_today)
        {
            printf("//////////NIGHT FELL//////////\n");
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

            log_today();
            setup_today = false;
        }

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

        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
        {
            printf("[CASHIER]: Visitor %d with %d kids dismissed - closing hours\n", current_pid, current_visitor_data->kids_count);
            current_visitor_data->status = VS_LEAVING;
            b_signal(current_pid, SIGINT);
            continue;
        }

        if (1 + current_visitor_data->kids_count + visitors_today > VISITORS_LIMIT)
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

    b_fifo_create(TICKET_REGULAR_PATH);
    b_fifo_create(TICKET_VIP_PATH);
    fifo_regular = b_fifo_open(TICKET_REGULAR_PATH, O_RDONLY);
    fifo_vip = b_fifo_open(TICKET_VIP_PATH, O_RDONLY);

    fds[0].fd = fifo_regular;
    fds[0].events = POLLIN;
    fds[1].fd = fifo_vip;
    fds[1].events = POLLIN;

    leaving_thread = b_execute_thread(print_leaving);
}

void end_simulation()
{
    if (leaving_thread)
    {
        pthread_cancel(leaving_thread);
        pthread_join(leaving_thread, NULL);
    }

    b_shm_dettach(shared_data);
    b_shm_dettach(visitors_data);
    b_shm_dettach(guides_data);

    if (fifo_regular >= 0) b_fifo_close(fifo_regular);
    b_fifo_delete(TICKET_REGULAR_PATH);
    if (fifo_vip >= 0) b_fifo_close(fifo_vip);
    b_fifo_delete(TICKET_VIP_PATH);

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

    int sum = 0;
    int sold = 0;
    if (visitor_data->isVIP)
    {
        b_signal(visitor_data->pid, SIG_WAKE_UP);
        printf("[CASHIER]: let VIP %d in\n", visitor_data->pid);
        visitors_pids[visitors_today] = visitor_data->pid;
        visitors_today++;
        visitor_data->status = VS_AWAITING_GUIDE; //this tells VIP to choose a direction and start going by themselves
        b_signal(visitor_data->pid, SIG_WAKE_UP);
        return;
    }

    sum += TICKET_PRICE;
    sold++;
    visitors_pids[visitors_today] = visitor_data->pid;
    visitors_today++;
    for(int i = 0; i < visitor_data->kids_count; i++)
    {
        if (visitor_data->kids[i].age >= 7)
        {
            sum += TICKET_PRICE;
            sold++;
        }
        visitors_pids[visitors_today] = visitor_data->kids[i].tid;
        visitors_today++;
    }

    gold_today += sum;

    printf("[CASHIER]: sold %d ticket(s) to %d with %d kids\n", sold, visitor_data->pid, visitor_data->kids_count);
    printf("[CASHIER]: let visitor %d in\n", visitor_data->pid);
    for(int i = 0; i < visitor_data->kids_count; i++)
    {
        printf("[CASHIER]: let kid %ld in under visitor %d protection\n", visitor_data->kids[i].tid, visitor_data->pid);
    }
    visitor_data->status = VS_AWAITING_GUIDE;
    b_msq_send(msgid, 1, visitor_data->pid);
    b_signal(visitor_data->pid, SIG_WAKE_UP);
}

void log_today()
{
    while(1)
    {
        bool all_ready = true;
        for(int i = 0; i < GUIDES_NUMBER; i++)
        {
            if (guides_data[i].group_count > 0 && guides_data[i].status == GS_GATHERING_GROUP)
            {
                all_ready = false;
            }
        }

        if (all_ready) break;

        b_wait_for_wakeup();
    }

    char title[100];
    char date[32];
    time_t s = (time_t)shared_data->start_time;
    strftime(date, sizeof(date), "%Y-%m-%d_%H:%M:%S", localtime(&s));
    snprintf(title, sizeof(title), "./logs/%s_day_%d.log", date, day);
    if (mkdir("./logs", 0755) == -1 && errno != EEXIST) perror("[ERROR] can't create logs directory");
    FILE* file = fopen(title, "w");
    if (file == NULL)
    {
        perror("[ERROR]: logging error");
        printf("[LOG ERROR BACKUP]: day: %d\n", day);
        printf("[LOG ERROR BACKUP]: gold earned: %d\n", gold_today);
        printf("[LOG ERROR BACKUP]: visitors entered today: %d\n", visitors_today);
        printf("[LOG ERROR BACKUP]: visitors entered today:\n");
        for(unsigned int i = 0; i < visitors_today; i++)
        {
            printf("[LOG ERROR BACKUP]: %ld\n", visitors_pids[i]);
        }
        printf("[LOG ERROR BACKUP]: end\n");
        return;
    }

    fprintf(file, "gold earned: %d\n", gold_today);
    fprintf(file, "visitors entries today: %d\n", visitors_today);
    fprintf(file, "visitors entries today:\n");
    for(unsigned int i = 0; i < visitors_today; i++)
    {
        fprintf(file, "%ld\n", visitors_pids[i]);
    }

    fclose(file);
}