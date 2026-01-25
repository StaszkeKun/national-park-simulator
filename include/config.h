#pragma once

#ifdef no_sleep
#define OPEN_TIME 1
#define CLOSE_TIME 1
#elif defined(big_numbers)
#define OPEN_TIME 60
#define CLOSE_TIME 60
#else
#define OPEN_TIME 25 //duration of day (in seconds)
#define CLOSE_TIME 25 //duration of night (in seconds)
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
#define VISITORS_LIMIT 200 //number of visitors that can enter in a single day
#define GUIDES_NUMBER 5 //number of guides working
#define GROUP_SIZE 7 //maximal size of guide's group (visitors + kids)
#define BRIDGE_LIMIT 6 //limit of people allowed on bridge at once
#define TOWER_LIMIT 10 //limit of people allowed on tower at once
#define FERRY_LIMIT 10 //limit of people allowed on ferry at once
#endif

#ifdef only_vip
#define VIP_CHANCE 1
#else
#define VIP_CHANCE 0.05 //chance of a visitor being a VIP
#endif

#ifdef big_families
#define KIDS_CHANCE 0.95
#define KIDS_LIMIT 4
#else
#define KIDS_CHANCE 0.35 //chance of a visitor having a child
#define KIDS_LIMIT 4 //limit of children a visitor can have
#endif

#define TICKET_PRICE 2 //only needed for daily raports

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
#define GUIDES_GATHER_WAIT 2 //time (in seconds) a guide waits without new visitors before starting a tour with a non-full group
#define GUIDES_GATHER_CHECK_INTERVAL 1 //time (in seconds) after which guide will recheck queue after seeing it empty
#define GUIDES_MOVETIME_MIN 1 //lower boundry of time (in seconds) it takes for a guide to move from one attraction to another
#define GUIDES_MOVETIME_MAX 2 //upper boundry of time (in seconds) it takes for a guide to move from one attraction to another
#define BRIDGE_CROSS_MIN_TIME 1 //lower boundry of time (in seconds) it takes to cross the bridge
#define BRIDGE_CROSS_MAX_TIME 2 //upper boundry of time (in seconds) it takes to cross the bridge
#define TOWER_ACTION_MIN_TIME 1 //lower boundry of time (in seconds) it takes to go up/ sightsee/ go down the tower attraction
#define TOWER_ACTION_MAX_TIME 2 //upper boundry of time (in seconds) it takes to go up/ sightsee/ go down the tower attraction
#define FERRY_VOYAGE_TIME 3 //time (in seconds) it takes for the ferry to compleate a voyage
#ifdef big_numbers
#define VISITOR_SPAWN_MIN_INTERVAL 0.05
#define VISITOR_SPAWN_MAX_INTERVAL 0.05
#else
#define VISITOR_SPAWN_MIN_INTERVAL 0.5 //lower boundry of time (in seconds) between visitors arrivals
#define VISITOR_SPAWN_MAX_INTERVAL 1 //upper boundry of time (in seconds) between visitors arrivals
#endif
#define TICKET_SALE_TIME 1 //time (in seconds) it takes to process a visitor entry
#endif