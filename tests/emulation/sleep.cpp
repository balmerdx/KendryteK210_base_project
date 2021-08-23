#include "sleep.h"

#include <time.h>


int msleep(uint64_t msec)
{
    usleep(msec*1000);
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    usleep(seconds*1000000);
    return 0;
}
