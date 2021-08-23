#include "sysctl.h"
#include <sys/time.h>

void sysctl_enable_irq()
{

}

void sysctl_disable_irq()
{
    
}

uint64_t sysctl_get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;    
}