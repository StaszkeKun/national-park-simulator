#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();
void operate();
int register_visitor();
void create_visitor();
bool check_if_first_secure(void* buf, int mutex);

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
    while(true)
    {
        pause();
    }
}

int semid;
int myid;
int msgid_cashier;
int msgid_guide;

shared_data_t* shared_data;
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

    shared_data = b_shm_attach(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    visitors_data = b_shm_attach(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));

    fifo_regular = b_fifo_open(TICKET_REGULAR_PATH, O_WRONLY);
    fifo_vip = b_fifo_open(TICKET_VIP_PATH, O_WRONLY);

    msgid_cashier = b_msq_get_id(MSG_CASHIER);
    msgid_guide = b_msq_get_id(MSG_GUIDES);

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
        case VS_AT_BRIDGE_QUEUE:
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
        case VS_AT_BRIDGE:
        {
            b_sem_p(semid, SEM_BRIDGE, 1 + my_data->kids_count);

            b_sleep(shared_data->bridge_crosstime);

            b_sem_v(semid, SEM_BRIDGE, 1 + my_data->kids_count);

            b_msq_send(msgid_guide, my_data->asigned_guide + 1, my_data->kids_count + 1);

            my_data->status = VS_FOLLOWING_GUIDE;
            break;
        }
        case VS_AT_TOWER_QUEUE:
        {
            if (!my_data->tower_allowed)
            {
                b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);

                my_data->status = VS_FOLLOWING_GUIDE;

                break;
            }

            b_sem_p(semid, MUTEX_TOWER, 1);

            ringbuffer_push_back(&shared_data->tower_queue, my_data->pid);
            b_sem_v(semid, MUTEX_TOWER, 1);

            while(!check_if_first_secure(&shared_data->tower_queue, MUTEX_TOWER))
            {
                b_wait_for_wakeup();
            }

            b_sem_p(semid, MUTEX_TOWER, 1);

            ringbuffer_pop_front(&shared_data->tower_queue, NULL);

            if (shared_data->tower_queue.count > 0)
            {
                pid_t new_first;
                ringbuffer_at(&shared_data->tower_queue, 0, &new_first);
                b_signal(new_first, SIG_WAKE_UP);
            }

            b_sem_v(semid, MUTEX_TOWER, 1);

            my_data->status = VS_GOING_UP_TOWER;

            break;
        }
        case VS_GOING_UP_TOWER:
        {
            b_sem_p(semid, SEM_TOWER, 1 + my_data->kids_count);

            b_sleep(shared_data->tower_uptime);

            my_data->status = VS_AT_TOWER;

            break;
        }
        case VS_AT_TOWER:
        {
            b_sleep(shared_data->tower_seetime);

            my_data->status = VS_GOING_DOWN_TOWER;

            break;
        }
        case VS_GOING_DOWN_TOWER:
        {
            b_sleep(shared_data->tower_downtime);

            b_sem_v(semid, SEM_TOWER, 1 + my_data->kids_count);

            b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);

            my_data->status = VS_FOLLOWING_GUIDE;

            break;
        }
        case VS_AT_FERRY_BOARDING:
        {
            b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
            my_data->status = VS_AWAITING_FERRY_START;
            break;
        }
        case VS_AWAITING_FERRY_START:
        {
            b_wait_for_wakeup();
            b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
            my_data->status = VS_FOLLOWING_GUIDE;
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
    my_data->asigned_guide = -1;
    my_data->slowed = false;
    my_data->tower_allowed = true;
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
            if (age < 6) my_data->tower_allowed = false;
            my_data->kids_count++;
        }
    }

    my_data->status = VS_NONE;
}

bool check_if_first_secure(void* buf, int mutex)
{
    b_sem_p(semid, mutex, 1);

    pid_t first_element;
    ringbuffer_at(buf, 0, &first_element);
    if (first_element == my_data->pid)
    {
        b_sem_v(semid, mutex, 1);
        return true;
    }

    b_sem_v(semid, mutex, 1);
    return false;
}