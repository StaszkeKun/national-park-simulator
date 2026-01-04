#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "vector.h"
#include "globals.h"

void check_configuration();
void init();
void end_simulation();

int main()
{
    printf("%lf\n", tick());
    check_configuration();
    exit(EXIT_SUCCESS);
}

void check_configuration()
{
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

    if (RIVER_LIMIT >= 1.5 * GROUP_SIZE)
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

}

void end_simulation()
{

}