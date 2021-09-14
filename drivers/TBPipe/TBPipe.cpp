#include "TBPipe.h"
#include <stdlib.h>
#include <memory.h>
#include "sysctl.h"

TBPipe::TBPipe(int buffer_size)
    : buffer_size(buffer_size)
{
    static_assert(sizeof(char)==1);
    static_assert(sizeof(uint32_t)==4);

    buffer = (uint8_t*)malloc(buffer_size*2);

    irq_buffer = buffer;
    user_buffer = irq_buffer + buffer_size;
}

TBPipe::~TBPipe()
{
    free(buffer);
}

void TBPipe::AppendByte(uint8_t byte)
{
    if(irq_buffer_pos < buffer_size)
    {
        irq_buffer[irq_buffer_pos++] = byte;
    } else
    {
        overflow = true;
    }
}

void TBPipe::GetBuffer(uint8_t*& data, uint32_t& size, bool* overflow)
{
    sysctl_disable_irq();

    uint8_t* p_tmp = irq_buffer;
    irq_buffer = user_buffer;
    user_buffer = p_tmp;

    user_buffer_pos = irq_buffer_pos;
    irq_buffer_pos = 0;

    if(overflow)
        *overflow = this->overflow;
    this->overflow = false;

    sysctl_enable_irq();

    data = user_buffer;
    size = user_buffer_pos;
}

