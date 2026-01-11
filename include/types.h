#pragma once

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>
#include "config.h"
#include "ringbuffer.h"
#include "constants.h"

typedef struct
{
    //general
    double start_time;
    pid_t cashier_pid;

    //bridge managing
    bool bridge_direction; //true (K=>A=>B) //false(B=>A=>K)
    int groups_on_bridge;
    float bridge_crosstime;
    
    //tower managing
    
    //ferry managing
    bool ferry_side; //true (B side) //false (K side)
    int ferry_seats;
    
    ringbuffer_t bridge_queue_clockwise;
    ringbuffer_t bridge_queue_aclockwise;
    ringbuffer_t tower_queue;
    ringbuffer_t ferry_queue_clockwise;
    ringbuffer_t ferry_queue_aclockwise;
} shared_data_t;

typedef struct
{
    pthread_t tid;
    int age;
} kid_data_t;

typedef struct
{
    pid_t pid;
    bool isVIP;
    kid_data_t kids[KIDS_LIMIT];
    int kids_count;
    int asigned_guide;
    bool slowed;
    visitor_status_t status;
} visitor_data_t;

typedef struct
{
    pid_t pid;
    visitor_data_t* groups[GROUP_SIZE];
    int group_count;
    guide_status_t status;
} guide_data_t;
