#pragma once
#include <stdint.h>
#include <unistd.h>

//int usleep(uint64_t usec); //defined in unistd
int msleep(uint64_t msec);
unsigned int sleep(unsigned int seconds);
