#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "types.h"
#include "constants.h"
#include "utils.h"
#include <sys/wait.h>

void check_configuration();
void init();
void end_simulation();

shared_data_t* shared_data;
guide_data_t* guides_data;
visitor_data_t* visitors_data;
int semid;
pid_t new_process;

volatile sig_atomic_t end = 0;
pthread_t zombie_thread = 0;
void zombie_cleaner()
{
    while(!end)
    {
        wait(NULL);
    }

    while(errno != ECHILD)
    {
        wait(NULL);
    }
}

void handle_kill(int sig)
{
    (void)sig;
    end_simulation();
}

volatile sig_atomic_t stop_handled = 0;
void handle_stop(int sig)
{
    (void)sig;
    if (stop_handled) return;

    stop_handled = 1;

    b_signal(-getpgrp(), SIGSTOP);

    stop_handled = 0;
    pause();
}

volatile sig_atomic_t cont_handled = 0;
void handle_continue(int sig)
{
    (void)sig;
    if (cont_handled) return;

    cont_handled = 1;

    b_signal(-getpgrp(), SIGCONT);

    cont_handled = 0;
}

int main()
{
    check_configuration();
    init();

    new_process = b_execute("./bin/cashier", NULL);
    if (new_process == -1) end_simulation();

    zombie_thread = b_execute_thread(zombie_cleaner); //starts thread here because there is at least one child now, this avoids CPU burn

    for(int i = 0; i < GUIDES_NUMBER; i++)
    {
        char args[sizeof(int) * 3 + 2];
        snprintf(args, sizeof(args), "%d", i);
        new_process = b_execute("./bin/guide", args);
        if (new_process == -1) end_simulation();
    }

    while(true)
    {
        b_sleep(b_randf(VISITOR_SPAWN_MIN_INTERVAL, VISITOR_SPAWN_MAX_INTERVAL));
        new_process = b_execute("./bin/visitor", NULL);
        if (new_process == -1)
        {
            perror("[ERROR]: visitor creation error");
            continue;
        }
    }

    end_simulation();
    return 0;
}

void check_configuration()
{
    //TODO: more conditions (checking with limits, checking if makes sense (max_kids + 1 > max_group))
    if (BRIDGE_LIMIT >= GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: BRIGE_LIMIT must be lower than GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (TOWER_LIMIT >= 2 * GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: TOWER_LIMIT must be lower than 2*GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (FERRY_LIMIT >= 1.5 * GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: RIVER_LIMIT must be lower than 1.5*GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (OPEN_TIME < 0 || CLOSE_TIME < 0)
    {
        errno = EDOM;
        perror("[ERROR]: OPEN_TIME and CLOSE_TIME can't be negative");
        exit(EXIT_FAILURE);
    }
}

void init()
{
    printf("ADD VIP BEHAVIOUR\n");
    printf("ADD SIGNAL1/2 FUNCTIONALITY\n");
    printf("ENSURE JORUNEY TAKES LESS THAN CLOSING TIME (and overall better conditions)\n");
    printf("ADD COMMENTS\n");
    printf("DECIDE HOW TO RESOLVE STOP SYSTEM TIME ISSUE\n");
    printf("DECIDE ON HANDLING MORE WEIRD SIGNALS\n");
    struct sigaction sa;
    sa.sa_handler = handle_kill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSTOP, &sa, NULL);

    sa.sa_handler = handle_continue;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCONT, &sa, NULL);

    setpgid(0, 0);

    srand(time(NULL));
    shared_data = b_shm_attach(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    shared_data->start_time = b_tick();
    shared_data->bridge_direction = true;
    shared_data->groups_on_bridge = 0;
    shared_data->bridge_queue_clockwise.capacity = VISITORS_LIMIT;
    shared_data->bridge_queue_clockwise.count = 0;
    shared_data->bridge_queue_clockwise.head_idx = 0;
    shared_data->bridge_queue_clockwise.tail_idx = 0;
    shared_data->bridge_queue_aclockwise.capacity = VISITORS_LIMIT;
    shared_data->bridge_queue_aclockwise.count = 0;
    shared_data->bridge_queue_aclockwise.head_idx = 0;
    shared_data->bridge_queue_aclockwise.tail_idx = 0;
    shared_data->bridge_crosstime = b_randf(BRIDGE_CROSS_MIN_TIME, BRIDGE_CROSS_MAX_TIME);
    shared_data->tower_queue.capacity = VISITORS_LIMIT;
    shared_data->tower_queue.count = 0;
    shared_data->tower_queue.head_idx = 0;
    shared_data->tower_queue.tail_idx = 0;
    shared_data->tower_uptime = b_randf(TOWER_ACTION_MIN_TIME, TOWER_ACTION_MAX_TIME);
    shared_data->tower_seetime = b_randf(TOWER_ACTION_MIN_TIME, TOWER_ACTION_MAX_TIME);
    shared_data->tower_downtime = b_randf(TOWER_ACTION_MIN_TIME, TOWER_ACTION_MAX_TIME);
    shared_data->ferry_side = true;
    shared_data->ferry_seats_taken = 0;
    shared_data->ferry_groups_boarded = 0;
    shared_data->ferry_queue_clockwise.capacity = VISITORS_LIMIT;
    shared_data->ferry_queue_clockwise.count = 0;
    shared_data->ferry_queue_clockwise.head_idx = 0;
    shared_data->ferry_queue_clockwise.tail_idx = 0;
    shared_data->ferry_queue_aclockwise.capacity = VISITORS_LIMIT;
    shared_data->ferry_queue_aclockwise.count = 0;
    shared_data->ferry_queue_aclockwise.head_idx = 0;
    shared_data->ferry_queue_aclockwise.tail_idx = 0;

    semid = b_sem_get_id();
    b_sem_set(semid, SEM_BRIDGE, BRIDGE_LIMIT);
    b_sem_set(semid, SEM_TOWER, TOWER_LIMIT);
    b_sem_set(semid, SEM_FERRY, FERRY_LIMIT);
    b_sem_set(semid, MUTEX_BRIDGE, 1);
    b_sem_set(semid, MUTEX_TOWER, 1);
    b_sem_set(semid, MUTEX_FERRY, 1);
    b_sem_set(semid, MUTEX_ALLOC_VISITOR, 1);
}

void end_simulation()
{
    if (end) return;

    end = 1;
    b_signal(-getpgrp(), SIGINT);

    if (zombie_thread)
    {
        pthread_join(zombie_thread, NULL);
    }

    b_shm_dettach(shared_data);
    b_shm_remove(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    b_shm_remove(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    b_shm_remove(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

    b_msq_remove(b_msq_get_id(MSG_CASHIER));
    b_msq_remove(b_msq_get_id(MSG_GUIDES));

    b_sem_remove(semid);

    exit(EXIT_SUCCESS);
}