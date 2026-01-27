#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();
void operate();
void register_visitor();
void create_visitor();
bool try_pass_bridge_vip();
bool try_board_ferry_vip();
bool check_if_first_secure(void* buf, int mutex);

volatile sig_atomic_t kill_requested = 0;
void handle_kill(int sig)
{
    (void)sig;
    kill_requested = 1;
    b_raise(SIG_WAKE_UP);
}

volatile sig_atomic_t leave_park = 0;
void handle_leave_park(int sig)
{
    (void)sig;
    leave_park = 1;
}

void* kid_thread(void* arg)
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    (void)arg;
    while(true)
    {
        pause();
    }
    return NULL;
}

int semid;
int msgid_cashier;
int msgid_guide;

//shared memory pointers
shared_data_t* shared_data = NULL;
visitor_data_t* visitors_data = NULL; //whole visitor_data array
visitor_data_t* my_data = NULL; //pointer to specific visitor_data point with this visitor's data

int fifo_regular = -1;
int fifo_vip = -1;

//vip control variables
bool vip_clockwise_track;
bool ferry_leader;

volatile sig_atomic_t leave_tower = 0;
void handle_leave_tower(int sig)
{
    (void)sig;
    leave_tower = 1;
}

int main()
{
    init();

    while(!kill_requested)
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

    sa.sa_handler = handle_leave_park;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_LEAVE_PARK, &sa, NULL);

    sa.sa_handler = handle_leave_tower;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_LEAVE_TOWER, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGXCPU, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_WAKE_UP);
    sigprocmask(SIG_BLOCK, &set, NULL);

    srand(getpid() * time(NULL));

    if (kill_requested) return;
    fifo_regular = b_fifo_open(TICKET_REGULAR_PATH, O_WRONLY);
    if (kill_requested) return;
    fifo_vip = b_fifo_open(TICKET_VIP_PATH, O_WRONLY);

    if (kill_requested) return;
    shared_data = b_shm_attach(b_shm_get_id_ifexist(SHM_SHARED_DATA, sizeof(shared_data_t)));
    visitors_data = b_shm_attach(b_shm_get_id_ifexist(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    if (shared_data == NULL || visitors_data == NULL) end_simulation();

    if (kill_requested) return;
    msgid_cashier = b_msq_get_id(MSG_CASHIER);
    msgid_guide = b_msq_get_id(MSG_GUIDES);

    if (kill_requested) return;
    semid = b_sem_get_id();
    register_visitor();
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

    if (visitors_data != NULL) b_shm_dettach(visitors_data);
    if (shared_data != NULL) b_shm_dettach(shared_data);

    if (fifo_regular >= 0) b_fifo_close(fifo_regular);
    if (fifo_vip >= 0) b_fifo_close(fifo_vip);

    exit(EXIT_SUCCESS);
}

void operate()
{
    //VIP needs redesign but works for now
    switch (my_data->status)
    {
        //initial state
        case VS_NONE:
        {
            my_data->status = VS_AWAITING_TICKET;
            if (my_data->isVIP)
            {
                b_fifo_write(fifo_vip, &my_data->pid, sizeof(pid_t));
            }
            else
            {
                b_fifo_write(fifo_regular, &my_data->pid, sizeof(pid_t));
            }

            break;
        }
        //vip choose direction, wait otherwise
        case VS_AWAITING_GUIDE:
        {
            if (my_data->isVIP)
            {
                printf("[VIP %d]: chosing a direction\n", my_data->pid);
                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: moving to bridge\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_AT_BRIDGE;
                    break;
                }
                else
                {
                    printf("[VIP %d]: moving to ferry\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_AWAITING_FERRY_START;
                    break;
                }
            }
            else
            {
                b_wait_for_wakeup();
            }
            break;
        }
        //wait for guide commands
        case VS_AWAITING_TICKET:
        case VS_AWAITING_START:
        case VS_AT_BRIDGE_QUEUE:
        {
            b_wait_for_wakeup();
            break;
        }
        case VS_FOLLOWING_GUIDE:
        {
            leave_park = 0;
            leave_tower = 0;
            b_wait_for_wakeup();
            break;
        }
        //wait for semaphore - cross - release semahpore - check in with guide
        //vip manage entry on bridge on their own
        case VS_AT_BRIDGE:
        {
            if (my_data->isVIP)
            {
                printf("[VIP %d]: arrived at bridge\n", my_data->pid);
                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: tries adding to bridge queue\n", my_data->pid);
                    b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    ringbuffer_push_back(&shared_data->bridge_queue_clockwise, my_data->pid);
                    b_sem_v(semid, MUTEX_BRIDGE, 1);
                    if (kill_requested) break;
                    printf("[VIP %d]: joined queue at bridge\n", my_data->pid);

                    while(!check_if_first_secure(&shared_data->bridge_queue_clockwise, MUTEX_BRIDGE))
                    {
                        printf("[VIP %d]: is not first in line\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                        {
                            b_sem_p(semid, MUTEX_BRIDGE, 1, NULL, 0);
                            size_t pos_queue = ringbuffer_contains(&shared_data->bridge_queue_clockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->bridge_queue_clockwise, pos_queue);
                            b_sem_v(semid, MUTEX_BRIDGE, 1);
                            printf("[VIP %d]: moving to cashier\n", my_data->pid);
                            b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                            b_msq_send(msgid_cashier, 2, my_data->pid);
                            my_data->status = VS_AWAITING_TICKET;
                            break;
                        }
                    }
                }
                else
                {
                    printf("[VIP %d]: tries adding to bridge queue\n", my_data->pid);
                    b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    ringbuffer_push_back(&shared_data->bridge_queue_aclockwise, my_data->pid);
                    b_sem_v(semid, MUTEX_BRIDGE, 1);
                    if (kill_requested) break;
                    printf("[VIP %d]: joined queue at bridge\n", my_data->pid);

                    while(!check_if_first_secure(&shared_data->bridge_queue_aclockwise, MUTEX_BRIDGE))
                    {
                        printf("[VIP %d]: is not first in line\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                        {
                            b_sem_p(semid, MUTEX_BRIDGE, 1, NULL, 0);
                            size_t pos_queue = ringbuffer_contains(&shared_data->bridge_queue_aclockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->bridge_queue_aclockwise, pos_queue);
                            b_sem_v(semid, MUTEX_BRIDGE, 1);
                            printf("[VIP %d]: moving to cashier\n", my_data->pid);
                            b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                            b_msq_send(msgid_cashier, 2, my_data->pid);
                            my_data->status = VS_AWAITING_TICKET;
                            break;
                        }
                    }
                }

                if (kill_requested) break;
                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME && my_data->status == VS_AWAITING_TICKET) break;

                while(!try_pass_bridge_vip())
                {
                    printf("[VIP %d]: couldn't pass\n", my_data->pid);
                    b_wait_for_wakeup();
                    if (kill_requested) break;
                    if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                    {
                        b_sem_p(semid, MUTEX_BRIDGE, 1, NULL, 0);
                        if (vip_clockwise_track)
                        {
                            size_t pos_queue = ringbuffer_contains(&shared_data->bridge_queue_clockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->bridge_queue_clockwise, pos_queue);
                            if (shared_data->bridge_queue_clockwise.count > 0)
                            {
                                pid_t next;
                                ringbuffer_at(&shared_data->bridge_queue_clockwise, 0, &next);
                                b_signal(next, SIG_WAKE_UP);
                            }
                        }
                        else
                        {
                            size_t pos_queue = ringbuffer_contains(&shared_data->bridge_queue_aclockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->bridge_queue_aclockwise, pos_queue);
                            if (shared_data->bridge_queue_aclockwise.count > 0)
                            {
                                pid_t next;
                                ringbuffer_at(&shared_data->bridge_queue_aclockwise, 0, &next);
                                b_signal(next, SIG_WAKE_UP);
                            }
                        }
                        b_sem_v(semid, MUTEX_BRIDGE, 1);
                        printf("[VIP %d]: moving to cashier\n", my_data->pid);
                        b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                        b_msq_send(msgid_cashier, 2, my_data->pid);
                        my_data->status = VS_AWAITING_TICKET;
                        break;
                    }
                }

                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME && my_data->status == VS_AWAITING_TICKET) break;

                b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                if (vip_clockwise_track)
                {
                    printf("[VIP %d - clockwise]: started crossing bridge\n", my_data->pid);
                    ringbuffer_pop_front(&shared_data->bridge_queue_clockwise, NULL);
                    if (shared_data->bridge_queue_clockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->bridge_queue_clockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }
                else
                {
                    printf("[VIP %d - aclockwise]: started crossing bridge\n", my_data->pid);
                    ringbuffer_pop_front(&shared_data->bridge_queue_aclockwise, NULL);
                    if (shared_data->bridge_queue_aclockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->bridge_queue_aclockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }

                b_sem_v(semid, MUTEX_BRIDGE, 1);

                b_sleep(shared_data->bridge_crosstime, (volatile sig_atomic_t* []){&kill_requested}, 1);
                printf("[VIP %d]: crossed bridge\n", my_data->pid);

                b_sem_v(semid, SEM_BRIDGE, 1);

                b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                shared_data->groups_on_bridge--;
                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: try signaling opposite group\n", my_data->pid);
                    if (shared_data->bridge_queue_aclockwise.count > 0)
                    {
                        printf("[VIP %d]: signaling opposite group\n", my_data->pid);
                        pid_t next_first;
                        ringbuffer_at(&shared_data->bridge_queue_aclockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }
                else
                {
                    printf("[VIP %d]: try signaling opposite group\n", my_data->pid);
                    if (shared_data->bridge_queue_clockwise.count > 0)
                    {
                        printf("[VIP %d]: signaling opposite group\n", my_data->pid);
                        pid_t next_first;
                        ringbuffer_at(&shared_data->bridge_queue_clockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }

                b_sem_v(semid, MUTEX_BRIDGE, 1);
                printf("[VIP %d]: left bridge\n", my_data->pid);

                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: moving to tower\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_GOING_UP_TOWER;
                }
                else
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                }

                break;
            }
            else
            {
                b_sem_p(semid, SEM_BRIDGE, 1 + my_data->kids_count, (volatile sig_atomic_t* []){&kill_requested}, 1);

                b_sleep(shared_data->bridge_crosstime, (volatile sig_atomic_t* []){&kill_requested}, 1);

                b_sem_v(semid, SEM_BRIDGE, 1 + my_data->kids_count);

                my_data->status = VS_FOLLOWING_GUIDE;
                b_msq_send(msgid_guide, my_data->asigned_guide + 1, my_data->kids_count + 1);
                break;
            }
        }
        //manage entry on the tower without guide's help
        case VS_AT_TOWER_QUEUE:
        {
            if (!my_data->tower_allowed)
            {
                my_data->status = VS_FOLLOWING_GUIDE;

                b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);

                break;
            }

            b_sem_p(semid, MUTEX_TOWER, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

            ringbuffer_push_back(&shared_data->tower_queue, my_data->pid);

            b_sem_v(semid, MUTEX_TOWER, 1);

            if (leave_tower || leave_park)
            {
                b_sem_p(semid, MUTEX_TOWER, 1, NULL, 0);
                size_t pos_queue = ringbuffer_contains(&shared_data->tower_queue, my_data->pid);
                ringbuffer_erase(&shared_data->tower_queue, pos_queue);
                b_sem_v(semid, MUTEX_TOWER, 1);
                my_data->status = VS_FOLLOWING_GUIDE;
                b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
                break;
            }

            while(!check_if_first_secure(&shared_data->tower_queue, MUTEX_TOWER))
            {
                b_wait_for_wakeup();
                if (kill_requested) break;
                if (leave_tower || leave_park)
                {
                    b_sem_p(semid, MUTEX_TOWER, 1, NULL, 0);
                    size_t pos_queue = ringbuffer_contains(&shared_data->tower_queue, my_data->pid);
                    ringbuffer_erase(&shared_data->tower_queue, pos_queue);
                    b_sem_v(semid, MUTEX_TOWER, 1);
                    my_data->status = VS_FOLLOWING_GUIDE;
                    b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
                    break;
                }
            }

            b_sem_p(semid, MUTEX_TOWER, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

            ringbuffer_pop_front(&shared_data->tower_queue, NULL);

            for(size_t i = 0; i < shared_data->tower_queue.count; i++)
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
            b_sem_p(semid, SEM_TOWER, 1 + my_data->kids_count, (volatile sig_atomic_t* []){&kill_requested}, 1);

            b_sleep(shared_data->tower_uptime, (volatile sig_atomic_t* []){&kill_requested, &leave_park, &leave_tower}, 3);

            my_data->status = VS_AT_TOWER;

            break;
        }
        case VS_AT_TOWER:
        {
            b_sleep(shared_data->tower_seetime, (volatile sig_atomic_t* []){&kill_requested, &leave_park, &leave_tower}, 3);
            my_data->status = VS_GOING_DOWN_TOWER;

            break;
        }
        case VS_GOING_DOWN_TOWER:
        {
            b_sleep(shared_data->tower_downtime, (volatile sig_atomic_t* []){&kill_requested, &leave_park, &leave_tower}, 3);

            b_sem_v(semid, SEM_TOWER, 1 + my_data->kids_count);

            if (my_data->isVIP)
            {
                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: moving to ferry\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_AWAITING_FERRY_START;
                }
                else
                {
                    printf("[VIP %d]: moving to bridge\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_AT_BRIDGE;
                }
            }
            else
            {
                my_data->status = VS_FOLLOWING_GUIDE;
                b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
            }

            break;
        }
        case VS_AT_FERRY_BOARDING:
        {
            my_data->status = VS_AWAITING_FERRY_START;
            b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
            break;
        }
        //vip manage to board/steer ferry on their own
        case VS_AWAITING_FERRY_START:
        {
            if (my_data->isVIP)
            {
                printf("[VIP %d]: arrived at ferry\n", my_data->pid);

                if (kill_requested) break;

                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: tries adding to ferry vip queue\n", my_data->pid);
                    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    ringbuffer_push_back(&shared_data->ferry_vipqueue_clockwise, my_data->pid);
                    printf("[VIP %d]: joined vip queue at ferry\n", my_data->pid);
                    b_sem_v(semid, MUTEX_FERRY, 1);

                    if (kill_requested) break;
                    while(!check_if_first_secure(&shared_data->ferry_vipqueue_clockwise, MUTEX_FERRY))
                    {
                        printf("[VIP %d]: is not first in line\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                        {
                            b_sem_p(semid, MUTEX_FERRY, 1, NULL, 0);
                            size_t pos_queue = ringbuffer_contains(&shared_data->ferry_vipqueue_clockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->ferry_vipqueue_clockwise, pos_queue);
                            b_sem_v(semid, MUTEX_FERRY, 1);
                            printf("[VIP %d]: moving to cashier\n", my_data->pid);
                            b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                            b_msq_send(msgid_cashier, 2, my_data->pid);
                            my_data->status = VS_AWAITING_TICKET;
                            break;
                        }
                    }
                }
                else
                {
                    printf("[VIP %d]: tries adding to ferry vip queue\n", my_data->pid);
                    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    ringbuffer_push_back(&shared_data->ferry_vipqueue_aclockwise, my_data->pid);
                    printf("[VIP %d]: joined vip queue at ferry\n", my_data->pid);
                    b_sem_v(semid, MUTEX_FERRY, 1);

                    if (kill_requested) break;
                    while(!check_if_first_secure(&shared_data->ferry_vipqueue_aclockwise, MUTEX_FERRY))
                    {
                        printf("[VIP %d]: is not first in line\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                        if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                        {
                            b_sem_p(semid, MUTEX_FERRY, 1, NULL, 0);
                            size_t pos_queue = ringbuffer_contains(&shared_data->ferry_vipqueue_aclockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->ferry_vipqueue_aclockwise, pos_queue);
                            b_sem_v(semid, MUTEX_FERRY, 1);
                            printf("[VIP %d]: moving to cashier\n", my_data->pid);
                            b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                            b_msq_send(msgid_cashier, 2, my_data->pid);
                            my_data->status = VS_AWAITING_TICKET;
                            break;
                        }
                    }
                }

                if (kill_requested) break;
                if (my_data->status == VS_AWAITING_TICKET) break;

                while(!try_board_ferry_vip())
                {
                    printf("[VIP %d]: couldnt board\n", my_data->pid);
                    b_wait_for_wakeup();
                    if (kill_requested) break;
                    if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                    {
                        b_sem_p(semid, MUTEX_FERRY, 1, NULL, 0);
                        if (vip_clockwise_track)
                        {
                            size_t pos_queue = ringbuffer_contains(&shared_data->ferry_vipqueue_clockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->ferry_vipqueue_clockwise, pos_queue);
                            if (shared_data->ferry_vipqueue_clockwise.count > 0)
                            {
                                pid_t next;
                                ringbuffer_at(&shared_data->ferry_vipqueue_clockwise, 0, &next);
                                b_signal(next, SIG_WAKE_UP);
                            }
                        }
                        else
                        {
                            size_t pos_queue = ringbuffer_contains(&shared_data->ferry_vipqueue_aclockwise, my_data->pid);
                            ringbuffer_erase(&shared_data->ferry_vipqueue_aclockwise, pos_queue);
                            if (shared_data->ferry_vipqueue_aclockwise.count > 0)
                            {
                                pid_t next;
                                ringbuffer_at(&shared_data->ferry_vipqueue_aclockwise, 0, &next);
                                b_signal(next, SIG_WAKE_UP);
                            }
                        }
                        b_sem_v(semid, MUTEX_FERRY, 1);
                        printf("[VIP %d]: moving to cashier\n", my_data->pid);
                        b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                        b_msq_send(msgid_cashier, 2, my_data->pid);
                        my_data->status = VS_AWAITING_TICKET;
                        break;
                    }
                }

                if (kill_requested) break;
                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME && my_data->status == VS_AWAITING_TICKET) break;

                b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                printf("[VIP %d]: boarded ferry\n", my_data->pid);
                if (vip_clockwise_track)
                {
                    ringbuffer_pop_front(&shared_data->ferry_vipqueue_clockwise, NULL);
                    if (shared_data->ferry_vipqueue_clockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->ferry_vipqueue_clockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                    else if (shared_data->ferry_queue_clockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->ferry_queue_clockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }
                else
                {
                    ringbuffer_pop_front(&shared_data->ferry_vipqueue_aclockwise, NULL);
                    if (shared_data->ferry_vipqueue_aclockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->ferry_vipqueue_aclockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                    else if (shared_data->ferry_queue_aclockwise.count > 0)
                    {
                        pid_t next_first;
                        ringbuffer_at(&shared_data->ferry_queue_aclockwise, 0, &next_first);
                        b_signal(next_first, SIG_WAKE_UP);
                    }
                }

                if(kill_requested) break;

                shared_data->ferry_groups_boarded++;

                b_sem_v(semid, MUTEX_FERRY, 1);

                if (ferry_leader)
                {
                    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    if (vip_clockwise_track && (shared_data->ferry_queue_clockwise.count != 0 || shared_data->ferry_vipqueue_clockwise.count != 0))
                    {
                        b_sem_v(semid, MUTEX_FERRY, 1);
                        printf("[VIP %d - ferry captain]: waiting for more passangers\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                    }
                    else if (!vip_clockwise_track && (shared_data->ferry_queue_aclockwise.count != 0 || shared_data->ferry_vipqueue_aclockwise.count != 0))
                    {
                        b_sem_v(semid, MUTEX_FERRY, 1);
                        printf("[VIP %d - ferry captain]: waiting for more passangers\n", my_data->pid);
                        b_wait_for_wakeup();
                        if (kill_requested) break;
                    }
                    else b_sem_v(semid, MUTEX_FERRY, 1);

                    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                    shared_data->ferry_side = 2;

                    printf("[VIP %d - ferry captain]: ferry starts\n", my_data->pid);
                    b_sleep(FERRY_VOYAGE_TIME, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    printf("[VIP %d - ferry captain]: ferry stops\n", my_data->pid);

                    for(int i = 1; i < shared_data->ferry_seats_taken; i++)
                    {
                        if (b_process_exist(shared_data->ferry_seats[i])) b_signal(shared_data->ferry_seats[i], SIG_WAKE_UP);
                    }

                    shared_data->ferry_seats_taken = 0;
                    shared_data->ferry_side = 1 - vip_clockwise_track;

                    b_sem_v(semid, MUTEX_FERRY, 1);
                }
                else
                {
                    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                    if (vip_clockwise_track)
                    {
                        if (shared_data->ferry_queue_clockwise.count == 0 && shared_data->ferry_vipqueue_clockwise.count == 0)
                        {
                            printf("[VIP %d]: no more waiting for ferry - alerting captain\n", my_data->pid);
                            b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
                        }
                    }
                    else
                    {
                        if (shared_data->ferry_queue_aclockwise.count == 0 && shared_data->ferry_vipqueue_aclockwise.count == 0)
                        {
                            printf("[VIP %d]: no more waiting for ferry - alerting captain\n", my_data->pid);
                            b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
                        }
                    }
                    b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);

                    b_sem_v(semid, MUTEX_FERRY, 1);
                    b_wait_for_wakeup();
                    if (kill_requested) break;
                }

                b_sem_v(semid, SEM_FERRY, 1);

                if (shared_data->ferry_vipqueue_clockwise.count > 0)
                {
                    pid_t next_first;
                    ringbuffer_at(&shared_data->ferry_vipqueue_clockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }
                else if (shared_data->ferry_queue_clockwise.count > 0)
                {
                    pid_t next_first;
                    ringbuffer_at(&shared_data->ferry_queue_clockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }

                if (shared_data->ferry_vipqueue_aclockwise.count > 0)
                {
                    pid_t next_first;
                    ringbuffer_at(&shared_data->ferry_vipqueue_aclockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }
                else if (shared_data->ferry_queue_aclockwise.count > 0)
                {
                    pid_t next_first;
                    ringbuffer_at(&shared_data->ferry_queue_aclockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }

                if (b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                    break;
                }

                if (vip_clockwise_track)
                {
                    printf("[VIP %d]: moving to cashier\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    b_msq_send(msgid_cashier, 2, my_data->pid);
                    my_data->status = VS_AWAITING_TICKET;
                }
                else
                {
                    printf("[VIP %d]: moving to tower\n", my_data->pid);
                    b_sleep(b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX), (volatile sig_atomic_t* []){&kill_requested}, 1);
                    my_data->status = VS_GOING_UP_TOWER;
                }
            }
            else
            {
                b_wait_for_wakeup();
                if (kill_requested) break;
                my_data->status = VS_FOLLOWING_GUIDE;
                b_msq_send(msgid_guide, my_data->asigned_guide + 1, 1 + my_data->kids_count);
            }
            break;
        }
        case VS_LEAVING:
        {
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

void register_visitor()
{
    int id = 0;
    b_sem_p(semid, MUTEX_ALLOC_VISITOR, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    while(true)
    {
        if (id >= VISITORS_LIMIT)
        {
            errno = EAGAIN;
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
}

void create_visitor()
{
    my_data->pid = getpid();
    my_data->kids_count = 0;
    my_data->asigned_guide = -1;
    my_data->slowed = false;
    my_data->tower_allowed = true;
    my_data->isVIP = false;

    if (b_randf(0, 1) < VIP_CHANCE)
    {
        my_data->isVIP = true;
        if (b_randf(0, 1) < 0.5) vip_clockwise_track = true;
        else vip_clockwise_track = false;
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

bool try_pass_bridge_vip()
{
    b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    if (shared_data->bridge_direction != vip_clockwise_track)
    {
        if (shared_data->groups_on_bridge == 0)
        {
            shared_data->bridge_direction = vip_clockwise_track;
            if (vip_clockwise_track) printf("[VIP %d - clockwise]: changing bridge direction\n", my_data->pid);
            else printf("[VIP %d - aclockwise]: changing bridge direction\n", my_data->pid);
        }
        else
        {
            b_sem_v(semid, MUTEX_BRIDGE, 1);
            return false;
        }
    }

    shared_data->groups_on_bridge++;

    b_sem_p(semid, SEM_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    b_sem_v(semid, MUTEX_BRIDGE, 1);
    return true;
}

bool try_board_ferry_vip()
{
    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    if (shared_data->ferry_side != vip_clockwise_track && shared_data->ferry_seats_taken == 0)
    {
        printf("[VIP %d]: no one on the other side calling the ferry\n", my_data->pid);
        shared_data->ferry_side = 2;
        b_sleep(FERRY_VOYAGE_TIME, (volatile sig_atomic_t* []){&kill_requested}, 1);
        shared_data->ferry_side = vip_clockwise_track;
    }

    if (shared_data->ferry_side != vip_clockwise_track)
    {
        b_sem_v(semid, MUTEX_FERRY, 1);
        return false;
    }

    if (b_sem_check(semid, SEM_FERRY) < 1 || shared_data->ferry_seats_taken >= FERRY_LIMIT)
    {
        printf("[VIP %d]: couldn't fit on ferry - alerting captain\n", my_data->pid);
        b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
        b_sem_v(semid, MUTEX_FERRY, 1);
        return false;
    }

    b_sem_p(semid, SEM_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    shared_data->ferry_seats[shared_data->ferry_seats_taken] = my_data->pid;

    if (shared_data->ferry_seats_taken == 0) ferry_leader = true;
    else ferry_leader = false;

    shared_data->ferry_seats_taken++;

    b_sem_v(semid, MUTEX_FERRY, 1);
    return true;
}

bool check_if_first_secure(void* buf, int mutex)
{
    b_sem_p(semid, mutex, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

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