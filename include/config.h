#pragma once

#ifdef big_numbers
#define VISITORS_LIMIT 10000
#define GUIDES_NUMBER 25
#define GROUP_SIZE 25
#define BRIDGE_LIMIT 20
#define TOWER_LIMIT 35
#define FERRY_LIMIT 30
#define OPEN_TIME 15
#define CLOSE_TIME 5
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
#endif

#ifdef only_vip
#define VIP_CHANCE 1
#endif

#ifdef big_families
#define KIDS_CHANCE 0.95
#define KIDS_LIMIT 4
#endif


#ifdef no_sleep
#define VISITORS_LIMIT 500
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
#define OPEN_TIME 1
#define CLOSE_TIME 1
#define DAYS_LIMIT 5
#endif

#ifdef big_numbers
#define VISITOR_SPAWN_MIN_INTERVAL 0
#define VISITOR_SPAWN_MAX_INTERVAL 0
#define TICKET_SALE_TIME 0
#endif

#ifdef asleep_guide
#define GUIDE_WAKE_UP_TIME 30 //in seconds
#define CONSTANT_VISITOR_SPAWN false
#define DAYS_LIMIT 1

#define VISITORS_LIMIT 5000
#define VISITOR_SPAWN_MIN_INTERVAL 0
#define VISITOR_SPAWN_MAX_INTERVAL 0
#define TICKET_SALE_TIME 0
#define OPEN_TIME 3600
#define CLOSE_TIME 3600
#define VIP_CHANCE 0
#endif

#ifdef broken_bridge
#define BRIDGE_FIX_TIME 30 //in seconds
#define CONSTANT_VISITOR_SPAWN false
#define DAYS_LIMIT 1

#define VISITORS_LIMIT 3000
#define VISITOR_SPAWN_MIN_INTERVAL 0
#define VISITOR_SPAWN_MAX_INTERVAL 0
#define TICKET_SALE_TIME 0
#define OPEN_TIME 3600
#define CLOSE_TIME 3600
#define VIP_CHANCE 0.2
#define GUIDES_NUMBER 25
#define BRIDGE_CROSS_MIN_TIME 0
#define BRIDGE_CROSS_MAX_TIME 0
#define GUIDES_MOVETIME_MIN 0
#define GUIDES_MOVETIME_MAX 0
#define FERRY_VOYAGE_TIME 0
#define TOWER_ACTION_MIN_TIME 0
#define TOWER_ACTION_MAX_TIME 0
#endif

//DEFAULTS
#ifndef VISITORS_LIMIT
#define VISITORS_LIMIT 100 //number of visitors that can enter in a single day
#endif

#ifndef VISITOR_SPAWN_MIN_INTERVAL
#define VISITOR_SPAWN_MIN_INTERVAL 0.5 //lower boundry of time (in seconds) between visitors arrivals
#endif

#ifndef VISITOR_SPAWN_MAX_INTERVAL
#define VISITOR_SPAWN_MAX_INTERVAL 1 //upper boundry of time (in seconds) between visitors arrivals
#endif

#ifndef TICKET_SALE_TIME
#define TICKET_SALE_TIME 1 //time (in seconds) it takes to process a visitor entry
#endif

#ifndef GUIDES_NUMBER
#define GUIDES_NUMBER 5 //number of guides working
#endif

#ifndef GROUP_SIZE
#define GROUP_SIZE 7 //maximal size of guide's group (visitors + kids)
#endif

#ifndef GUIDES_GATHER_WAIT
#define GUIDES_GATHER_WAIT 2 //time (in seconds) a guide waits without new visitors before starting a tour with a non-full group
#endif

#ifndef GUIDES_GATHER_CHECK_INTERVAL
#define GUIDES_GATHER_CHECK_INTERVAL 1 //time (in seconds) after which guide will recheck queue after seeing it empty
#endif

#ifndef GUIDES_MOVETIME_MIN
#define GUIDES_MOVETIME_MIN 1 //lower boundry of time (in seconds) it takes for a guide to move from one attraction to another
#endif

#ifndef GUIDES_MOVETIME_MAX
#define GUIDES_MOVETIME_MAX 2 //upper boundry of time (in seconds) it takes for a guide to move from one attraction to another
#endif

#ifndef BRIDGE_LIMIT
#define BRIDGE_LIMIT 6 //limit of people allowed on bridge at once
#endif

#ifndef BRIDGE_CROSS_MIN_TIME
#define BRIDGE_CROSS_MIN_TIME 1 //lower boundry of time (in seconds) it takes to cross the bridge
#endif

#ifndef BRIDGE_CROSS_MAX_TIME
#define BRIDGE_CROSS_MAX_TIME 2 //upper boundry of time (in seconds) it takes to cross the bridge
#endif

#ifndef TOWER_LIMIT
#define TOWER_LIMIT 10 //limit of people allowed on tower at once
#endif

#ifndef TOWER_ACTION_MIN_TIME
#define TOWER_ACTION_MIN_TIME 1 //lower boundry of time (in seconds) it takes to go up/ sightsee/ go down the tower attraction
#endif

#ifndef TOWER_ACTION_MAX_TIME
#define TOWER_ACTION_MAX_TIME 2 //upper boundry of time (in seconds) it takes to go up/ sightsee/ go down the tower attraction
#endif

#ifndef FERRY_LIMIT
#define FERRY_LIMIT 10 //limit of people allowed on ferry at once
#endif

#ifndef FERRY_VOYAGE_TIME
#define FERRY_VOYAGE_TIME 3 //time (in seconds) it takes for the ferry to compleate a voyage
#endif

#ifndef VIP_CHANCE
#define VIP_CHANCE 0.2 //chance of a visitor being a VIP
#endif

#ifndef KIDS_CHANCE
#define KIDS_CHANCE 0.35 //chance of a visitor having a child
#endif

#ifndef KIDS_LIMIT
#define KIDS_LIMIT 4 //limit of children a visitor can have
#endif

#ifndef TICKET_PRICE
#define TICKET_PRICE 2 //only needed for daily raports
#endif

#ifndef DAYS_LIMIT
#define DAYS_LIMIT 2 // number of days after which simulation will end
#endif

#ifndef OPEN_TIME
#define OPEN_TIME 20 //duration of day (in seconds)
#endif

#ifndef CLOSE_TIME
#define CLOSE_TIME 20 //duration of night (in seconds)
#endif

#ifndef CONSTANT_VISITOR_SPAWN
#define CONSTANT_VISITOR_SPAWN true // if false only VISITORS_LIMIT visitors will spawn
#endif

#define CYCLE_TIME (OPEN_TIME + CLOSE_TIME)