#include "utils.h"
#include "constants.h"
#include "config.h"
#include "types.h"

void init();
void end_simulation();

int main()
{
    init();
    end_simulation();
    return 0;
}

void init()
{
    signal(SIGINT, end_simulation);
}

void end_simulation()
{
    exit(EXIT_SUCCESS);
}