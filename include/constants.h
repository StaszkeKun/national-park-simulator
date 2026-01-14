#pragma once

#define TICKET_REGULAR_PATH "/tmp/regular_fifo"
#define TICKET_VIP_PATH "/tmp/vip_fifo"

#define SIG_WAKE_UP SIGUSR1
#define SIG_LEAVE_PARK SIGUSR2
#define SIG_LEAVE_TOWER SIGRTMIN

typedef enum
{
    VS_NONE=0,
    VS_AWAITING_TICKET,
    VS_AWAITING_GUIDE,
    VS_AWAITING_START,
    VS_FOLLOWING_GUIDE,
    VS_AT_BRIDGE_QUEUE,
    VS_AT_BRIDGE,
    VS_AT_TOWER_QUEUE,
    VS_GOING_UP_TOWER,
    VS_AT_TOWER,
    VS_GOING_DOWN_TOWER,
    VS_AT_FERRY_BOARDING,
    VS_AWAITING_FERRY_START,
    VS_LEAVING
} visitor_status_t;

typedef enum
{
    GS_NONE=0,
    GS_GATHERING_GROUP,
    GS_MOVING_TO_BRIDGE,
    GS_AT_BRIDGE,
    GS_MOVING_TO_TOWER,
    GS_AT_TOWER,
    GS_MOVING_TO_FERRY,
    GS_AT_FERRY,
    GS_MOVING_TO_CASH,
    GS_AT_CASH
} guide_status_t;

enum SharedMemoryIds
{
    SHM_SHARED_DATA,
    SHM_GUIDES_DATA,
    SHM_VISITOR_DATA
};

#define NSEMS 7
enum SemaphoreIds
{
    SEM_BRIDGE,
    SEM_TOWER,
    SEM_FERRY,
    MUTEX_BRIDGE,
    MUTEX_TOWER,
    MUTEX_FERRY,
    MUTEX_ALLOC_VISITOR
};

enum MessageQueueIds
{
    MSG_CASHIER,
    MSG_GUIDES
};