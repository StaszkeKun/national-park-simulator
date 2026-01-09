#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();
void operate();
int register_visitor();
void create_visitor();

void handle_wake_up(int sig)
{
    (void)sig;
}

void handle_kill(int sig)
{
    (void)sig;
    end_simulation();
}

void kid_thread()
{
    while(1)
    {
        pause();
    }
}

int semid;
int myid;
int msgid_cashier;

visitor_data_t* visitors_data;
visitor_data_t* my_data = NULL;

int fifo_regular = -1;
int fifo_vip = -1;

int main()
{
    init();

    while(true)
    {
        operate();
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

    sa.sa_handler = handle_wake_up;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_WAKE_UP, &sa, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_WAKE_UP);
    sigprocmask(SIG_BLOCK, &set, NULL);

    srand(getpid() * time(NULL));

    visitors_data = b_shm_attach(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));

    fifo_regular = b_fifo_open(TICKET_REGULAR_PATH, O_WRONLY);
    fifo_vip = b_fifo_open(TICKET_VIP_PATH, O_WRONLY);

    msgid_cashier = b_msq_get_id(MSG_CASHIER);

    semid = b_sem_get_id();
    myid = register_visitor();
}

void end_simulation()
{
    if (my_data != NULL)
    {
        for(int i = 0; i < my_data->kids_count; i ++)
        {
            pthread_cancel(my_data->kids[i].tid);
            pthread_join(my_data->kids[i].tid, NULL);
        }
    }

    b_shm_dettach(visitors_data);

    if(fifo_regular >= 0) b_fifo_close(fifo_regular);
    if(fifo_vip >= 0) b_fifo_close(fifo_vip);

    exit(EXIT_SUCCESS);
}

void operate()
{
    switch (my_data->status)
    {
        case VS_NONE:
        {
            if (my_data->isVIP)
            {
                b_fifo_write(fifo_vip, &my_data->pid, sizeof(pid_t));
            }
            else
            {
                b_fifo_write(fifo_regular, &my_data->pid, sizeof(pid_t));
            }
            my_data->status = VS_AWAITING_TICKET;

            break;
        }
        case VS_AWAITING_GUIDE:
        {
            if (my_data->isVIP)
            {
                printf("[VIP]: %d chosing a direction\n", my_data->pid);
                my_data->status = VS_FOLLOWING_GUIDE;
            }
            else
            {
                b_wait_for_wakeup();
            }
            break;
        }
        case VS_AWAITING_TICKET:
        case VS_AWAITING_START:
        {
            b_wait_for_wakeup();
            break;
        }
        case VS_FOLLOWING_GUIDE:
        {
            if (my_data->isVIP)
            {
                printf("[VIP]: %d moving\n", my_data->pid);
                b_sleep(5);
                my_data->status = VS_LEAVING;
            }
            else
            {
                b_wait_for_wakeup();
            }
            break;
        }
        case VS_AT_BRIDGE_QUEUE:
        case VS_AT_BRIDGE:
        case VS_AT_TOWER_QUEUE:
        case VS_GOING_UP_TOWER:
        case VS_AT_TOWER:
        case VS_GOING_DOWN_TOWER:
        case VS_AT_FERRY_QUEUE:
        case VS_AWAITING_FERRY_START:
        case VS_AT_FERRY_VOYAGE:
        {
            end_simulation();
            break;
        }
        case VS_LEAVING:
        {
            if (my_data->isVIP)
            {
                printf("[VIP]: %d moving\n", my_data->pid);
                b_sleep(5);
                b_msq_send(msgid_cashier, 2, my_data->pid);
            }

            end_simulation();
            break;
        }
        default:
        {
            perror("[ERROR]: invalid visitor state");
            end_simulation();
        }
    }
}

int register_visitor()
{
    int id = 0;
    b_sem_p(semid, MUTEX_ALLOC_VISITOR, 1);

    while(true)
    {
        if (id >= VISITORS_LIMIT)
        {
            perror("[ERROR]: visitor didn't fit in visitor data shared memory");
            b_sem_v(semid, MUTEX_ALLOC_VISITOR, 1);
            end_simulation();
        }

        if (!b_process_exist(visitors_data[id].pid))
        {
            my_data = &visitors_data[id];
            create_visitor();
            break;
        }

        id++;
    }

    b_sem_v(semid, MUTEX_ALLOC_VISITOR, 1);
    return id;
}

void create_visitor()
{
    my_data->pid = getpid();
    my_data->kids_count = 0;
    my_data->slowed = false;
    my_data->isVIP = false;

    if (b_randf(0, 1) <= VIP_CHANCE)
    {
        my_data->isVIP = true;
    }
    else
    {
        while(my_data->kids_count < KIDS_LIMIT && b_randf(0, 1) <= KIDS_CHANCE)
        {
            pthread_t tid = b_execute_thread(kid_thread);
            my_data->kids[my_data->kids_count].tid = tid;
            int age = b_randi(1, 15);
            my_data->kids[my_data->kids_count].age = age;
            if (age < 12) my_data->slowed = true;
            my_data->kids_count++;
        }
    }

    my_data->status = VS_NONE;
}