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

pthread_t zombie_thread;
void zombie_cleaner()
{
    while(true)
    {
        wait(NULL);
    }
}

void handle_kill(int sig)
{
    (void)sig;
    end_simulation();
}

shared_data_t* shared_data;
guide_data_t* guides_data;
visitor_data_t* visitors_data;
vector_t* processes;

int main()
{
    check_configuration();
    init();

    processes = vector_new(sizeof(pid_t));

    pid_t new_process;
    new_process = b_execute("./bin/cashier", NULL);
    if (new_process == -1) end_simulation();
    vector_push_back(processes, &new_process);
    for(int i = 0; i < GUIDES_NUMBER; i++)
    {
        char args[sizeof(int) * 3 + 2];
        snprintf(args, sizeof(args), "%d", i);
        new_process = b_execute("./bin/guide", args);
        if (new_process == -1) end_simulation();
        vector_push_back(processes, &new_process);
    }

    while(true)
    {
        b_sleep(b_randf(1, 5));
        new_process = b_execute("./bin/visitor", NULL);
        if (new_process == -1)
        {
            printf("[ERROR]: visitor creation error");
            continue;
        }
        vector_push_back(processes, &new_process);
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
    shared_data->bridge_queue_clockwise = vector_new(sizeof(guide_data_t));
    shared_data->bridge_queue_aclockwise = vector_new(sizeof(guide_data_t));
    shared_data->tower_queue = vector_new(sizeof(visitor_data_t));
    shared_data->ferry_side = true;
    shared_data->ferry_seats = FERRY_LIMIT;
    shared_data->ferry_queue_clockwise = vector_new(sizeof(guide_data_t));
    shared_data->ferry_queue_aclockwise = vector_new(sizeof(guide_data_t));
}

void end_simulation()
{
    pthread_cancel(zombie_thread);
    pthread_join(zombie_thread, NULL);

    vector_free(shared_data->bridge_queue_clockwise);
    vector_free(shared_data->bridge_queue_aclockwise);
    vector_free(shared_data->tower_queue);
    vector_free(shared_data->ferry_queue_clockwise);
    vector_free(shared_data->ferry_queue_aclockwise);

    b_shm_remove(b_shm_get_id(SHM_SHARED_DATA, sizeof(shared_data_t)));
    b_shm_remove(b_shm_get_id(SHM_VISITOR_DATA, sizeof(visitor_data_t) * VISITORS_LIMIT));
    b_shm_remove(b_shm_get_id(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER));
    b_shm_dettach(shared_data);

    b_msq_remove(b_msq_get_id(MSG_CASHIER));
    b_msq_remove(b_msq_get_id(MSG_GUIDES));

    while(processes->length > 0) {
        pid_t pid;
        vector_pop_back(processes, &pid);
        if (b_process_exist(pid)) {
            b_signal(pid, SIGINT);
        }
    }

    vector_free(processes);

    exit(EXIT_SUCCESS);
}