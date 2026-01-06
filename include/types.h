#pragma once

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>
#include "config.h"
#include "vector.h"
#include "constants.h"

typedef struct
{
    //general
    double start_time;

    //bridge managing
    bool bridge_direction; //true (K=>A=>B) //false(B=>A=>K)
    int groups_on_bridge;
    vector_t* bridge_queue_clockwise;
    vector_t* bridge_queue_aclockwise;

    //tower managing
    vector_t* tower_queue;

    //ferry managing
    bool ferry_side; //true (B side) //false (K side)
    int ferry_seats;
    vector_t* ferry_queue_clockwise;
    vector_t* ferry_queue_aclockwise;
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
    bool hasKidsUnder6;
    visitor_status_t status;
} visitor_data_t;

typedef struct
{
    pid_t pid;
    visitor_data_t* group[GROUP_SIZE];
    int group_count;
    bool track;
    guide_status_t status;
} guide_data_t;
