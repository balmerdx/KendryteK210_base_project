#pragma once
#include <stdint.h>

void sysctl_enable_irq();
void sysctl_disable_irq();

uint64_t sysctl_get_time_us();
