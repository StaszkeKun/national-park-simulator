#pragma once

#define TICKET_REGULAR_PATH "/tmp/regular_fifo"
#define TICKET_VIP_PATH "/tmp/vip_fifo"

#define SIG_LEAVE_TOWER SIGUSR1
#define SIG_LEAVE_PARK SIGUSR2
#define SIF_WAKE_UP SIGRTMIN

typedef enum
{
    VS_NONE=0,
    VS_AWAITING_TICKET,
    VS_AWAITING_GUIDE,
    VS_AWAITING_START,
    VS_FOLLOWING_GUIDE,
    VS_AT_BRIDGE_QUEUE,
    VS_AT_BRIDGE,
    VS_GOING_UP_TOWER,
    VS_AT_TOWER,
    VS_GOING_DOWN_TOWER,
    VS_AT_RIVER_QUEUE,
    VS_WAITING_FOR_FERRY_START,
    VS_AT_FERRY_VOYAGE,
    VS_LEAVING
} visitor_status_t;

typedef enum
{
    GS_NONE=0,
    GS_GATHERING,
    GS_MOVING_TO_BRIDGE,
    GS_AT_BRIDGE,
    GS_MOVING_TO_TOWER,
    GS_AT_TOWER,
    GS_MOVING_TO_RIVER,
    GS_AT_RIVER,
    GS_MOVING_TO_CASH
} guide_status_t;

enum SharedMemoryIds
{
    SHM_SHARED_DATA,
    SHM_VISITORS_DATA,
    SHM_GUIDES_DATA
};

#define NSEMS 3
enum SemaphoreIds
{
    SEM_BRIDGE,
    SEM_TOWER,
    SEM_RIVER
};

enum MessageQueueIds
{
    MSG_CASHIER,
    MSG_GUIDES,
    MSG_BRIDGE,
    MSG_FERRY
};