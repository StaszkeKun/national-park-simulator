#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();
void register_guide();
void operate();

int my_id;

int msgid_cashier;
int msgid_guide;

shared_data_t* shared_data;
visitor_data_t* visitors_data;
guide_data_t* guides_data;
guide_data_t* my_data;

int visitors_in_group;
double update_time;
bool clockwise_track = true;

void handle_kill(int sig)
{
    (void)sig;
    end_simulation();
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

    srand(getpid() * time(NULL));

    msgid_cashier = b_msq_get_id(MSG_CASHIER);
    msgid_guide = b_msq_get_id(MSG_GUIDES);

    shared_data = b_shm_attach(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    visitors_data = b_shm_attach(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    guides_data = b_shm_attach(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

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

                if(visitor_pid != -1)
                {
                    visitor_data_t* visitor_data = b_get_visitor_by_pid(visitors_data, visitor_pid);
                    if (visitors_in_group + visitor_data->kids_count + 1 <= GROUP_SIZE)
                    {
                        my_data->groups[my_data->group_count] = visitor_data;
                        my_data->group_count++;
                        visitors_in_group += visitor_data->kids_count + 1;
                        visitor_data->status = VS_FOLLOWING_GUIDE;
                        b_signal(visitor_pid, SIG_WAKE_UP);
                        update_time = b_tick();
                        printf("[GUIDE %d] took %ld to group - departing in %dsec if no more visitors\n", my_id, visitor_pid, GUIDES_GATHER_WAIT);
                    }
                    else
                    {
                        b_msq_send(msgid_cashier, 1, visitor_pid);
                        printf("[GUIDE %d] %ld did't fit group - sending to queue\n", my_id, visitor_pid);
                    }
                }

                //printf("g%d: 1\n", my_id);
                if (visitors_in_group >= GROUP_SIZE) break;
                //printf("g%d: 2\n", my_id);
                if (visitors_in_group > 0 && b_get_time_of_day(shared_data->start_time) > OPEN_TIME) break;
                //printf("g%d: 3\n", my_id);
                if (visitors_in_group > 0 && b_tick() - update_time > GUIDES_GATHER_WAIT) break;

                double now = b_get_time_of_day(shared_data->start_time);
                if (now > OPEN_TIME)
                {
                    b_sleep(CYCLE_TIME - now);
                }
                else
                {
                    b_sleep(GUIDES_GATHER_CHECK_INTERVAL);
                }
            }

            int random = b_randi(0, 100);
            clockwise_track = random < 50 ? true : false;
            my_data->status = clockwise_track ? GS_MOVING_TO_BRIDGE : GS_MOVING_TO_FERRY;

            printf("[GUIDE %d] finished gathering group: %d\n", my_id, visitors_in_group);
            break;
        }
        case GS_MOVING_TO_BRIDGE:
        case GS_MOVING_TO_CASH:
        case GS_MOVING_TO_FERRY:
        case GS_MOVING_TO_TOWER:
        {
            printf("[GUIDE %d] moving\n", my_id);
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

            b_sleep(move_time);

            my_data->status = GS_AT_CASH;
            printf("[GUIDE %d] finished moving\n", my_id);
            break;
        }
        //DEBUG SEPARATE LATER
        case GS_AT_BRIDGE:
        case GS_AT_TOWER:
        case GS_AT_FERRY:
        {
            my_data->status = GS_AT_CASH;
            break;
        }
        case GS_AT_CASH:
        {
            printf("[GUIDE %d] arrived at cashier\n", my_id);
            for(int i = 0; i < my_data->group_count; i++)
            {
                b_msq_send(msgid_cashier, 2, my_data->groups[i]->pid);
            }
            printf("[GUIDE %d] finished leaving report\n", my_id);
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