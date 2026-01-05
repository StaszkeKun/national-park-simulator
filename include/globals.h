#pragma once

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>
#include "config.h"
#include "vector.h"

typedef struct
{
    //general
    double start_time;

    //bridge managing
    bool bridge_direction;
    int groups_on_bridge;
    vector_t* bridge_queue_clockwise;
    vector_t* bridge_queue_aclockwise;

    //tower managing
    vector_t* tower_queue;

    //ferry managing
    int space_on_ferry;
    vector_t* ferry_queue_clockwise;
    vector_t* ferry_queue_aclockwise;
} shared_data_t;

typedef struct
{
    pid_t pid;
    bool isVIP;
    pthread_t kids[CHILD_LIMIT];
    int kids_count;
    int slowest_speed;
    bool hasKidsUnder6;
} visitor_data_t;

typedef struct {
    pid_t pid;
    visitor_data_t group[GROUP_SIZE];
    int group_count;
    bool track;
} guide_data_t;

typedef struct {
    pid_t pid;
} cashier_data_t;

// microsecond-precision UNIX epoch
double tick()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

double get_time_of_day(double start_time)
{
    return fmod(tick() - start_time, CYCLE_TIME);
}