#pragma once
#include <stdint.h>
struct TBMessage
{
    //data - гарантированно align 4 byte для бинарных данных
    //data - гарантированно заканчивается нулевым символом для тексовых данных.
    uint8_t* data;
    uint32_t size;
    bool is_text;
};
