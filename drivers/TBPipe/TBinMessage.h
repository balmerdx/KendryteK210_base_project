#pragma once
#include <stdint.h>

struct TBinMessage
{
    uint8_t* data;
    uint32_t size;
    uint16_t command;

    //Check message validness
    bool is_bad() const { return command==0; }
};
