#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "types.h"
#include "constants.h"
#include "utils.h"
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

void check_configuration();
void init();
void end_simulation();

//shared memory pointers
shared_data_t* shared_data;
guide_data_t* guides_data;
visitor_data_t* visitors_data;

int semid;
sem_t visitor_sem; //limits spawn of visitors to not overflow ringbuffer

volatile sig_atomic_t kill_requested = 0;
void handle_kill(int sig)
{
    (void)sig;
    kill_requested = 1;
}

void handle_child(int sig)
{
    (void)sig;

    b_t_sem_v(&visitor_sem);
}

int main()
{
    check_configuration();
    init();

    pid_t new_process = b_execute("./bin/cashier", NULL);
    if (new_process == -1) end_simulation();

    for(int i = 0; i < GUIDES_NUMBER; i++)
    {
        char args[sizeof(int) * 3 + 2];
        snprintf(args, sizeof(args), "%d", i);
        new_process = b_execute("./bin/guide", args);
        if (new_process == -1) end_simulation();
    }

    while(!kill_requested)
    {
        b_sleep(b_randf(VISITOR_SPAWN_MIN_INTERVAL, VISITOR_SPAWN_MAX_INTERVAL), (volatile sig_atomic_t*[]){&kill_requested}, 1);
        b_t_sem_p(&visitor_sem, (volatile sig_atomic_t*[]){&kill_requested}, 1);
        if (kill_requested) break;
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

void init()
{
    struct sigaction sa;
    sa.sa_handler = handle_kill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGXCPU, &sa, NULL);

    sa.sa_handler = handle_child;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT; //this makes waiting for child processes unnecessary
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_WAKE_UP, &sa, NULL);

    setpgid(0, 0);

    sem_init(&visitor_sem, 0, VISITORS_LIMIT);

    srand(time(NULL));

    //shared memory initialization
    //careful with queues capacity as it is for now hardcoded to accept max of VISITOR_LIMIT as capacity, could cause SEGFAULT otherwise
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
    shared_data->ferry_vipqueue_clockwise.capacity = VISITORS_LIMIT;
    shared_data->ferry_vipqueue_clockwise.count = 0;
    shared_data->ferry_vipqueue_clockwise.head_idx = 0;
    shared_data->ferry_vipqueue_clockwise.tail_idx = 0;
    shared_data->ferry_vipqueue_aclockwise.capacity = VISITORS_LIMIT;
    shared_data->ferry_vipqueue_aclockwise.count = 0;
    shared_data->ferry_vipqueue_aclockwise.head_idx = 0;
    shared_data->ferry_vipqueue_aclockwise.tail_idx = 0;

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
    b_signal(-getpgrp(), SIGINT);

    b_shm_dettach(shared_data);
    b_shm_remove(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    b_shm_remove(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    b_shm_remove(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

    b_msq_remove(b_msq_get_id(MSG_CASHIER));
    b_msq_remove(b_msq_get_id(MSG_GUIDES));

    b_sem_remove(semid);
    sem_destroy(&visitor_sem);

    exit(EXIT_SUCCESS);
}

void check_configuration()
{
    if (BRIDGE_LIMIT >= GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: BRIDGE_LIMIT must be lower than GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (TOWER_LIMIT >= 2 * GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: TOWER_LIMIT must be lower than 2*GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (FERRY_LIMIT * 2 >= 3 * GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: FERRY_LIMIT must be lower than 1.5*GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    if (OPEN_TIME <= 0 || CLOSE_TIME <= 0)
    {
        errno = EDOM;
        perror("[ERROR]: OPEN_TIME and CLOSE_TIME can't be zero or less");
        exit(EXIT_FAILURE);
    }

    if (KIDS_LIMIT + 1 > GROUP_SIZE)
    {
        errno = EDOM;
        perror("[ERROR]: KIDS_LIMIT has to be lower than GROUP_SIZE");
        exit(EXIT_FAILURE);
    }

    struct rlimit rl;
    if (getrlimit(RLIMIT_CPU, &rl) != 0)
    {
        perror("[ERROR]: getrlimit error");
    }
    else
    {
        if (rl.rlim_cur != RLIM_INFINITY)
        {
            fprintf(stderr, "[WARNING]: simulation will close after %ld seconds due to limits\n", rl.rlim_cur);
        }
    }

    if (getrlimit(RLIMIT_NPROC, &rl) != 0)
    {
        perror("[ERROR]: getrlimit error");
    }
    else
    {
        if (rl.rlim_cur != RLIM_INFINITY && rl.rlim_cur <= 1 + 2 + GUIDES_NUMBER)
        {
            errno = ENOMEM;
            perror("[ERROR]: limits on number of processes is to little");
            exit(EXIT_FAILURE);
        }

        if (rl.rlim_cur != RLIM_INFINITY && rl.rlim_cur < 1 + 2 + GUIDES_NUMBER + VISITORS_LIMIT)
        {
            fprintf(stderr, "[WARNING]: due to limits there could be errors when there is too many visitors\n");
        }
    }

    /* this is obsolete and was used to warn about sanitizers memory overhead
    struct sysinfo info;
    if (sysinfo(&info) != 0)
    {
        perror("[ERROR]: sysinfo() error");
    }
    else
    {
        if ((2 + GUIDES_NUMBER + VISITORS_LIMIT)*3000UL > info.freeram * info.mem_unit * 8 / 10 / 1024)
        {
            errno = ENOMEM;
            perror("[ERROR]: VISITORS_LIMIT is too much for available RAM");
            exit(EXIT_FAILURE);
        }

        if ((2 + GUIDES_NUMBER + VISITORS_LIMIT)*3000UL > 2*1024*1024)
        {
            fprintf(stderr, "[WARNING]: this amount of visitors could take at most around %ldMB\n", (2 + GUIDES_NUMBER + VISITORS_LIMIT)*3000UL/1024);
        }
    }*/

    if (VISITORS_LIMIT <= 0)
    {
        errno = EDOM;
        perror("[ERROR]: VISITORS_LIMIT can't be zero or less");
        exit(EXIT_FAILURE);
    }

    if (GUIDES_NUMBER <= 0 || GROUP_SIZE <= 0)
    {
        errno = EDOM;
        perror("[ERROR]: GUIDES_NUMBER and GROUP_SIZE can't be zero or less");
        exit(EXIT_FAILURE);
    }

    if (TICKET_PRICE < 0)
    {
        fprintf(stderr, "[WARNING]: ticket price is less than zero\n");
    }
}