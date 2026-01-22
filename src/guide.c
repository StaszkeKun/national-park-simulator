#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();
void register_guide();
void operate();
void move();
bool try_pass_bridge();
bool try_board_ferry();
bool check_if_first_secure(void* buf, int mutex);

int my_id;

int msgid_cashier;
int msgid_guide;

int semid;

shared_data_t* shared_data;
visitor_data_t* visitors_data;
guide_data_t* guides_data;
guide_data_t* my_data;

int visitors_in_group;
int visitors_checkins;
double update_time;
bool clockwise_track = true;
bool ferry_leader = false;

volatile sig_atomic_t kill_requested = 0;
void handle_kill(int sig)
{
    (void)sig;
    kill_requested = 1;
    b_raise(SIG_WAKE_UP);
}

void handle_wake_up(int sig)
{
    (void)sig;
}

void handle_leave_park(int sig)
{
    (void)sig;
}

void handle_leave_tower(int sig)
{
    (void)sig;
    for(int i = 0; i < my_data->group_count; i++)
    {
        b_signal(my_data->groups[i]->pid, SIG_LEAVE_TOWER);
    }
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        perror("[ERROR]: guide ID not provided");
        exit(EXIT_FAILURE);
    }

    my_id = atoi(argv[1]);
    if(my_id < 0 || my_id >= GUIDES_NUMBER)
    {
        perror("[ERROR]: ivalid guide ID");
        exit(EXIT_FAILURE);
    }

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

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_WAKE_UP);
    sigprocmask(SIG_BLOCK, &set, NULL);

    srand(getpid() * time(NULL));

    msgid_cashier = b_msq_get_id(MSG_CASHIER);
    msgid_guide = b_msq_get_id(MSG_GUIDES);

    shared_data = b_shm_attach(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    visitors_data = b_shm_attach(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    guides_data = b_shm_attach(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

    semid = b_sem_get_id();

    register_guide();
}

void end_simulation()
{
    b_shm_dettach(shared_data);
    b_shm_dettach(visitors_data);
    b_shm_dettach(guides_data);

    exit(EXIT_SUCCESS);
}

void operate()
{
    switch (my_data->status)
    {
        case GS_GATHERING_GROUP:
        {
            visitors_in_group = 0;
            while(true)
            {
                long visitor_pid = b_msq_receive_nowait(msgid_cashier, 1);

                if(visitor_pid != 0)
                {
                    visitor_data_t* visitor_data = b_get_visitor_by_pid(visitors_data, visitor_pid);
                    if (visitors_in_group + visitor_data->kids_count + 1 <= GROUP_SIZE)
                    {
                        my_data->groups[my_data->group_count] = visitor_data;
                        visitor_data->asigned_guide = my_id;
                        my_data->group_count++;
                        visitors_in_group += visitor_data->kids_count + 1;
                        visitor_data->status = VS_FOLLOWING_GUIDE;
                        b_signal(visitor_pid, SIG_WAKE_UP);
                        update_time = b_tick();
                        printf("[GUIDE %d]: took %ld to group - departing in %dsec if no more visitors\n", my_id, visitor_pid, GUIDES_GATHER_WAIT);
                    }
                    else
                    {
                        b_msq_send(msgid_cashier, 1, visitor_pid);
                        printf("[GUIDE %d]: %ld did't fit group - sending to queue\n", my_id, visitor_pid);
                    }
                }

                if (visitors_in_group >= GROUP_SIZE) break;
                if (visitors_in_group > 0 && b_tick() - update_time > GUIDES_GATHER_WAIT) break;
                if (kill_requested) break;

                if (visitors_in_group > 0 && b_get_time_of_day(shared_data->start_time) > OPEN_TIME)
                {
                    my_data->status = GS_MOVING_TO_CASH;
                    break;
                }

                if (visitor_pid == 0)
                {
                    double now = b_get_time_of_day(shared_data->start_time);
                    if (now > OPEN_TIME)
                    {
                        b_sleep(CYCLE_TIME - now, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    }
                    else
                    {
                        b_sleep(GUIDES_GATHER_CHECK_INTERVAL, (volatile sig_atomic_t* []){&kill_requested}, 1);
                    }
                }
            }

            int random = b_randi(0, 100);
            clockwise_track = random < 50 ? true : false;
            my_data->status = clockwise_track ? GS_MOVING_TO_BRIDGE : GS_MOVING_TO_FERRY;

            printf("[GUIDE %d]: finished gathering group: %d\n", my_id, visitors_in_group);
            break;
        }
        case GS_MOVING_TO_BRIDGE:
        {
            printf("[GUIDE %d]: moving to bridge\n", my_id);

            move();

            my_data->status = GS_AT_BRIDGE;
            break;
        }
        case GS_MOVING_TO_FERRY:
        {
            printf("[GUIDE %d]: moving to ferry\n", my_id);

            move();

            my_data->status = GS_AT_FERRY;
            break;
        }
        case GS_MOVING_TO_TOWER:
        {
            printf("[GUIDE %d]: moving to tower\n", my_id);

            move();

            my_data->status = GS_AT_TOWER;
            break;
        }
        case GS_MOVING_TO_CASH:
        {
            printf("[GUIDE %d]: moving to cashier\n", my_id);

            move();

            my_data->status = GS_AT_CASH;
            break;
        }
        case GS_AT_BRIDGE:
        {
            printf("[GUIDE %d]: arrived at bridge\n", my_id);
            for(int i = 0; i < my_data->group_count; i++)
            {
                my_data->groups[i]->status = VS_AT_BRIDGE_QUEUE;
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }

            if (clockwise_track)
            {
                printf("[GUIDE %d]: tries adding to bridge queue\n", my_id);
                b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                ringbuffer_push_back(&shared_data->bridge_queue_clockwise, my_data->pid);
                printf("[GUIDE %d]: joined queue at bridge\n", my_id);
                b_sem_v(semid, MUTEX_BRIDGE, 1);

                while(!check_if_first_secure(&shared_data->bridge_queue_clockwise, MUTEX_BRIDGE))
                {
                    printf("[GUIDE %d]: is not first in line\n", my_id);
                    b_wait_for_wakeup();
                    printf("[GUIDE %d]: wake up check if first\n", my_id);
                    if(kill_requested) break;
                }
            }
            else
            {
                printf("[GUIDE %d]: tries adding to bridge queue\n", my_id);
                b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                ringbuffer_push_back(&shared_data->bridge_queue_aclockwise, my_data->pid);
                printf("[GUIDE %d]: joined queue at bridge\n", my_id);
                b_sem_v(semid, MUTEX_BRIDGE, 1);

                while(!check_if_first_secure(&shared_data->bridge_queue_aclockwise, MUTEX_BRIDGE))
                {
                    printf("[GUIDE %d]: is not first in line\n", my_id);
                    b_wait_for_wakeup();
                    printf("[GUIDE %d]: wake up check if first\n", my_id);
                    if(kill_requested) break;
                }
            }

            while(!try_pass_bridge())
            {
                printf("[GUIDE %d]: couldn't pass\n", my_id);
                b_wait_for_wakeup();
                printf("[GUIDE %d]: wake up trypass\n", my_id);
                if(kill_requested) break;
            }

            b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

            printf("[GUIDE %d]: started crossing bridge\n", my_id);
            if (clockwise_track)
            {
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
            printf("[GUIDE %d]: crossed bridge\n", my_id);

            b_sem_v(semid, SEM_BRIDGE, 1);

            visitors_checkins = 0;

            for(int i = 0; i < my_data->group_count; i++)
            {
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }

            if (kill_requested) break;
            while(visitors_checkins < visitors_in_group)
            {
                long message = b_msq_receive(msgid_guide, my_id + 1);
                if (kill_requested) break;
                visitors_checkins += message;
                printf("[GUIDE %d]: waiting for group to cross %d/%d\n", my_id, visitors_checkins, visitors_in_group);
            }

            b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
            shared_data->groups_on_bridge--;
            if (clockwise_track)
            {
                printf("[GUIDE %d]: try signaling opposite group\n", my_id);
                if (shared_data->bridge_queue_aclockwise.count > 0)
                {
                    printf("[GUIDE %d]: signaling opposite group\n", my_id);
                    pid_t next_first;
                    ringbuffer_at(&shared_data->bridge_queue_aclockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }
            }
            else
            {
                printf("[GUIDE %d]: try signaling opposite group\n", my_id);
                if (shared_data->bridge_queue_clockwise.count > 0)
                {
                    printf("[GUIDE %d]: signaling opposite group\n", my_id);
                    pid_t next_first;
                    ringbuffer_at(&shared_data->bridge_queue_clockwise, 0, &next_first);
                    b_signal(next_first, SIG_WAKE_UP);
                }
            }

            b_sem_v(semid, MUTEX_BRIDGE, 1);
            printf("[GUIDE %d]: left bridge\n", my_id);

            if (clockwise_track)
            {
                my_data->status = GS_MOVING_TO_TOWER;
            }
            else
            {
                my_data->status = GS_MOVING_TO_CASH;
            }

            break;
        }
        case GS_AT_TOWER:
        {
            printf("[GUIDE %d]: arrived at tower\n", my_id);
            for(int i = 0; i < my_data->group_count; i++)
            {
                my_data->groups[i]->status = VS_AT_TOWER_QUEUE;
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }


            for(int i = 0; i < my_data->group_count; i++)
            {
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }

            if (kill_requested) break;
            visitors_checkins = 0;
            while(visitors_checkins < visitors_in_group)
            {
                long msg = b_msq_receive(msgid_guide, my_id+1);
                if (kill_requested) break;
                visitors_checkins += msg;
                printf("[GUIDE %d]: waiting fo group to sightsee tower %d/%d\n", my_id, visitors_checkins, visitors_in_group);
            }
            printf("[GUIDE %d]: left tower\n", my_id);

            if (clockwise_track)
            {
                my_data->status = GS_MOVING_TO_FERRY;
            }
            else
            {
                my_data->status = GS_MOVING_TO_BRIDGE;
            }

            break;
        }
        case GS_AT_FERRY:
        {
            printf("[GUIDE %d]: arrived at ferry\n", my_id);

            if (clockwise_track)
            {
                printf("[GUIDE %d]: tries adding to ferry queue\n", my_id);
                b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                ringbuffer_push_back(&shared_data->ferry_queue_clockwise, my_data->pid);
                printf("[GUIDE %d]: joined queue at ferry\n", my_id);
                b_sem_v(semid, MUTEX_FERRY, 1);

                if(kill_requested) break;
                while(!check_if_first_secure(&shared_data->ferry_queue_clockwise, MUTEX_FERRY) && shared_data->ferry_vipqueue_clockwise.count == 0)
                {
                    printf("[GUIDE %d]: is not first in line\n", my_id);
                    b_wait_for_wakeup();
                    printf("[GUIDE %d]: wake up check if first\n", my_id);
                    if(kill_requested) break;
                }
            }
            else
            {
                printf("[GUIDE %d]: tries adding to ferry queue\n", my_id);
                b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);
                ringbuffer_push_back(&shared_data->ferry_queue_aclockwise, my_data->pid);
                printf("[GUIDE %d]: joined queue at ferry\n", my_id);
                b_sem_v(semid, MUTEX_FERRY, 1);

                if(kill_requested) break;
                while(!check_if_first_secure(&shared_data->ferry_queue_aclockwise, MUTEX_FERRY) && shared_data->ferry_vipqueue_aclockwise.count == 0)
                {
                    printf("[GUIDE %d]: is not first in line\n", my_id);
                    b_wait_for_wakeup();
                    printf("[GUIDE %d]: wake up check if first\n", my_id);
                    if(kill_requested) break;
                }
            }

            if(kill_requested) break;
            while(!try_board_ferry())
            {
                printf("[GUIDE %d]: couldnt board\n", my_id);
                b_wait_for_wakeup();
                printf("[GUIDE %d]: wake up tryboard\n", my_id);
                if(kill_requested) break;
            }

            b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

            printf("[GUIDE %d]: boarded ferry\n", my_id);
            if (clockwise_track)
            {
                ringbuffer_pop_front(&shared_data->ferry_queue_clockwise, NULL);
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
                ringbuffer_pop_front(&shared_data->ferry_queue_aclockwise, NULL);
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

            b_sem_v(semid, MUTEX_FERRY, 1);

            visitors_checkins = 0;

            for(int i = 0; i < my_data->group_count; i++)
            {
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }

            if (kill_requested) break;
            while(visitors_checkins < visitors_in_group)
            {

                long checkin = b_msq_receive(msgid_guide, my_id + 1);
                if (kill_requested) break;
                visitors_checkins += checkin;
                printf("[GUIDE %d]: waiting for group to board %d/%d\n", my_id, visitors_checkins, visitors_in_group);
            }

            b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

            shared_data->ferry_groups_boarded++;
            printf("[GUIDE %d]: group boarded\n", my_id);


            if (ferry_leader)
            {
                if (clockwise_track && (shared_data->ferry_queue_clockwise.count != 0 || shared_data->ferry_vipqueue_clockwise.count != 0))
                {
                    b_sem_v(semid, MUTEX_FERRY, 1);
                    printf("[GUIDE %d - ferry captain]: waiting for more passangers\n", my_id);
                    b_wait_for_wakeup();
                    if (kill_requested) break;
                }
                else if (!clockwise_track && (shared_data->ferry_queue_aclockwise.count != 0 || shared_data->ferry_vipqueue_aclockwise.count != 0))
                {
                    b_sem_v(semid, MUTEX_FERRY, 1);
                    printf("[GUIDE %d - ferry captain]: waiting for more passangers\n", my_id);
                    b_wait_for_wakeup();
                    if (kill_requested) break;
                }
                else b_sem_v(semid, MUTEX_FERRY, 1);

                b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

                shared_data->ferry_side = 2;

                printf("[GUIDE %d - ferry captain]: ferry starts\n", my_id);
                b_sleep(FERRY_VOYAGE_TIME, (volatile sig_atomic_t* []){&kill_requested}, 1);
                printf("[GUIDE %d - ferry captain]: ferry stops\n", my_id);

                for(int i = 1; i < shared_data->ferry_seats_taken; i++)
                {
                    if (b_process_exist(shared_data->ferry_seats[i])) b_signal(shared_data->ferry_seats[i], SIG_WAKE_UP);
                }

                shared_data->ferry_seats_taken = 0;
                shared_data->ferry_side = 1 - clockwise_track;

                b_sem_v(semid, MUTEX_FERRY, 1);
            }
            else
            {
                if (clockwise_track)
                {
                    if (shared_data->ferry_queue_clockwise.count == 0 && shared_data->ferry_vipqueue_clockwise.count == 0)
                    {
                        printf("[GUIDE %d]: no more waiting for ferry - alerting captain\n", my_id);
                        b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
                    }
                }
                else
                {
                    if (shared_data->ferry_queue_aclockwise.count == 0 && shared_data->ferry_vipqueue_aclockwise.count == 0)
                    {
                        printf("[GUIDE %d]: no more waiting for ferry - alerting captain\n", my_id);
                        b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
                    }
                }
                b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);

                b_sem_v(semid, MUTEX_FERRY, 1);
                b_wait_for_wakeup();
                if (kill_requested) break;
            }

            for(int i = 0; i < my_data->group_count; i++) b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);

            visitors_checkins = 0;

            for(int i = 0; i < my_data->group_count; i++)
            {
                b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
            }

            if (kill_requested) break;
            while(visitors_checkins < visitors_in_group)
            {
                long msg = b_msq_receive(msgid_guide, my_id+1);
                if (kill_requested) break;
                visitors_checkins += msg;
                printf("[GUIDE %d]: waiting fo group to disboard %d/%d\n", my_id, visitors_checkins, visitors_in_group);
            }

            b_sem_v(semid, SEM_FERRY, 1 + visitors_in_group);

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

            if (clockwise_track) my_data->status = GS_MOVING_TO_CASH;
            else my_data->status = GS_MOVING_TO_TOWER;

            printf("[GUIDE %d]: left ferry\n", my_id);
            break;
        }
        case GS_AT_CASH:
        {
            printf("[GUIDE %d]: arrived at cashier\n", my_id);
            for(int i = 0; i < my_data->group_count; i++)
            {
                b_msq_send(msgid_cashier, 2, my_data->groups[i]->pid);
            }
            printf("[GUIDE %d]: finished leaving report\n", my_id);
            //clean up for next tour
            my_data->group_count = 0;
            my_data->status = GS_GATHERING_GROUP;
            b_signal(shared_data->cashier_pid, SIG_WAKE_UP);

            break;
        }
        default:
        {
            perror("[ERROR]: guide invalid state");
            end_simulation();
        }
    }
}

void register_guide()
{
    guides_data[my_id].group_count = 0;
    guides_data[my_id].status = GS_GATHERING_GROUP;
    guides_data[my_id].pid = getpid();
    my_data = &guides_data[my_id];
}

void move()
{
    bool group_slowed = false;
    for(int i = 0; i < my_data->group_count; i++)
    {
        if (my_data->groups[i]->slowed)
        {
            group_slowed = true;
            break;
        }
    }

    float move_time = b_randf(GUIDES_MOVETIME_MIN, GUIDES_MOVETIME_MAX);

    if (group_slowed) move_time *= 1.5;

    b_sleep(move_time, (volatile sig_atomic_t* []){&kill_requested}, 1);

    printf("[GUIDE %d]: finished moving\n", my_id);
}

bool try_pass_bridge()
{
    b_sem_p(semid, MUTEX_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    if (shared_data->bridge_direction != clockwise_track)
    {
        if (shared_data->groups_on_bridge == 0)
        {
            shared_data->bridge_direction = clockwise_track;
        }
        else
        {
            b_sem_v(semid, MUTEX_BRIDGE, 1);
            return false;
        }
    }

    shared_data->groups_on_bridge++;

    b_sem_p(semid, SEM_BRIDGE, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    for(int i = 0; i < my_data->group_count; i++)
    {
        my_data->groups[i]->status = VS_AT_BRIDGE;
        b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
    }

    b_sem_v(semid, MUTEX_BRIDGE, 1);
    return true;
}

bool try_board_ferry()
{
    b_sem_p(semid, MUTEX_FERRY, 1, (volatile sig_atomic_t* []){&kill_requested}, 1);

    if (shared_data->ferry_side != clockwise_track && shared_data->ferry_seats_taken == 0)
    {
        printf("[GUIDE %d]: no one on the other side calling the ferry\n", my_id);
        b_sleep(FERRY_VOYAGE_TIME, (volatile sig_atomic_t* []){&kill_requested}, 1);
        shared_data->ferry_side = clockwise_track;
    }

    if (shared_data->ferry_side != clockwise_track)
    {
        b_sem_v(semid, MUTEX_FERRY, 1);
        return false;
    }

    if (b_sem_check(semid, SEM_FERRY) < 1 + visitors_in_group  || shared_data->ferry_seats_taken >= FERRY_LIMIT)
    {
        printf("[GUIDE %d]: couldn't fit on ferry - alerting captain\n", my_id);
        b_signal(shared_data->ferry_seats[0], SIG_WAKE_UP);
        b_sem_v(semid, MUTEX_FERRY, 1);
        return false;
    }

    b_sem_p(semid, SEM_FERRY, 1 + visitors_in_group, (volatile sig_atomic_t* []){&kill_requested}, 1);

    shared_data->ferry_seats[shared_data->ferry_seats_taken] = my_data->pid;

    if (shared_data->ferry_seats_taken == 0) ferry_leader = true;
    else ferry_leader = false;

    shared_data->ferry_seats_taken++;

    for(int i = 0; i < my_data->group_count; i++)
    {
        my_data->groups[i]->status = VS_AT_FERRY_BOARDING;
        b_signal(my_data->groups[i]->pid, SIG_WAKE_UP);
    }

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