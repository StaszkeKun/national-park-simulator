#pragma once

#ifdef no_sleep
#define OPEN_TIME 1
#define CLOSE_TIME 1
#elif defined(big_numbers)
#define OPEN_TIME 60
#define CLOSE_TIME 60
#else
#define OPEN_TIME 25
#define CLOSE_TIME 25
#endif

#define CYCLE_TIME (OPEN_TIME + CLOSE_TIME)

#ifdef big_numbers
#define VISITORS_LIMIT 1000
#define GUIDES_NUMBER 10
#define GROUP_SIZE 25
#define BRIDGE_LIMIT 20
#define TOWER_LIMIT 35
#define FERRY_LIMIT 30
#else
#define VISITORS_LIMIT 100
#define GUIDES_NUMBER 5
#define GROUP_SIZE 7
#define BRIDGE_LIMIT 6
#define TOWER_LIMIT 10
#define FERRY_LIMIT 10
#endif

#ifdef only_vip
#define VIP_CHANCE 1
#else
#define VIP_CHANCE 0.05
#endif

#ifdef big_families
#define KIDS_CHANCE 0.95
#define KIDS_LIMIT 4
#else
#define KIDS_CHANCE 0.35
#define KIDS_LIMIT 4
#endif

#define TICKET_PRICE 2

#ifdef no_sleep
#define GUIDES_GATHER_WAIT 0
#define GUIDES_GATHER_CHECK_INTERVAL 0
#define GUIDES_MOVETIME_MIN 0
#define GUIDES_MOVETIME_MAX 0
#define BRIDGE_CROSS_MIN_TIME 0
#define BRIDGE_CROSS_MAX_TIME 0
#define TOWER_ACTION_MIN_TIME 0
#define TOWER_ACTION_MAX_TIME 0
#define FERRY_VOYAGE_TIME 0
#define VISITOR_SPAWN_MIN_INTERVAL 0
#define VISITOR_SPAWN_MAX_INTERVAL 0
#define TICKET_SALE_TIME 0
#else
#define GUIDES_GATHER_WAIT 2
#define GUIDES_GATHER_CHECK_INTERVAL 1
#define GUIDES_MOVETIME_MIN 1
#define GUIDES_MOVETIME_MAX 2
#define BRIDGE_CROSS_MIN_TIME 1
#define BRIDGE_CROSS_MAX_TIME 2
#define TOWER_ACTION_MIN_TIME 1
#define TOWER_ACTION_MAX_TIME 2
#define FERRY_VOYAGE_TIME 3
#ifdef big_numbers
#define VISITOR_SPAWN_MIN_INTERVAL 0.05
#define VISITOR_SPAWN_MAX_INTERVAL 0.05
#else
#define VISITOR_SPAWN_MIN_INTERVAL 1
#define VISITOR_SPAWN_MAX_INTERVAL 2
#endif
#define TICKET_SALE_TIME 1
#endif