#pragma once
#include <stdint.h>
//16 bit per pixel
int WriteImage16(const char* filename, int width, int height, const void* buffer);
//24 bit per pixel
int WriteImage24(const char* filename, int width, int height, const void* buffer);