#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "vector.h"
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
vector_t* processes;
int semid;

pthread_mutex_t processes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t zombie_thread;
void zombie_cleaner()
{
    b_sleep(1);
    while(true)
    {
        pid_t pid = wait(NULL);
        if(pid != -1)
        {
            pthread_mutex_lock(&processes_mutex);
            vector_remove(processes, &pid);
            pthread_mutex_unlock(&processes_mutex);
        }
    }
}

void handle_kill(int sig)
{
    (void)sig;
    end_simulation();
}


int main()
{
    check_configuration();
    init();
    processes = vector_new(sizeof(pid_t));

    pid_t new_process;

    new_process = b_execute("./bin/cashier", NULL);
    if (new_process == -1) end_simulation();

    pthread_mutex_lock(&processes_mutex);
    vector_push_back(processes, &new_process);
    pthread_mutex_unlock(&processes_mutex);

    for(int i = 0; i < GUIDES_NUMBER; i++)
    {
        char args[sizeof(int) * 3 + 2];
        snprintf(args, sizeof(args), "%d", i);
        new_process = b_execute("./bin/guide", args);
        if (new_process == -1) end_simulation();

        pthread_mutex_lock(&processes_mutex);
        vector_push_back(processes, &new_process);
        pthread_mutex_unlock(&processes_mutex);
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

        pthread_mutex_lock(&processes_mutex);
        vector_push_back(processes, &new_process);
        pthread_mutex_unlock(&processes_mutex);
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
    printf("YOU CAN'T REALLOC IN SHARED MEMORY\n");
    printf("ADD OPTION OF VERBOSE LOGGING\n");
    printf("ENSURE JORUNEY TAKES LESS THAN CLOSING TIME (and overall better conditions)\n");
    printf("ADD VIP BEHAVIOUR\n");
    struct sigaction sa;
    sa.sa_handler = handle_kill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    zombie_thread = b_execute_thread(zombie_cleaner);

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
    shared_data->ferry_side = true;
    shared_data->ferry_seats = FERRY_LIMIT;
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
    b_sem_set(semid, MUTEX_FERRY, 1);
    b_sem_set(semid, MUTEX_ALLOC_VISITOR, 1);
    processes = vector_new(sizeof(pid_t));
}

void end_simulation()
{
    while(processes->length > 0) {
        pid_t pid;
        vector_pop_back(processes, &pid);
        if (b_process_exist(pid)) {
            b_signal(pid, SIGINT);
        }
    }

    pthread_cancel(zombie_thread);
    pthread_join(zombie_thread, NULL);

    b_shm_dettach(shared_data);
    b_shm_remove(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    b_shm_remove(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    b_shm_remove(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));

    b_msq_remove(b_msq_get_id(MSG_CASHIER));
    b_msq_remove(b_msq_get_id(MSG_GUIDES));

    b_sem_remove(semid);

    vector_free(processes);

    exit(EXIT_SUCCESS);
}