#include "FillBuffer.h"
#include <stdio.h>
#include <stdlib.h> 
#include <stdarg.h>
#include <memory.h>


FillBuffer::FillBuffer(uint32_t buffer_size)
    : buffer_size(buffer_size)
    , temp_buffer_size(buffer_size)
{
    buffer = new uint8_t[buffer_size];
    temp_buffer = new uint8_t[temp_buffer_size];
}

FillBuffer::~FillBuffer()
{
    Clear();
    delete[] buffer;
    delete[] temp_buffer;

}

void FillBuffer::Clear()
{
    for(TBMessage& m : messages)
        delete[] m.data;
    messages.clear();
    buffer_pos = 0;
}

void FillBuffer::AddData(const uint8_t* data, uint32_t size)
{
    if(buffer_pos>=buffer_size)
        return;
    uint32_t remain_size = buffer_size - buffer_pos;
    size = std::min(remain_size, size);
    memcpy(buffer+buffer_pos, data, size);
    buffer_pos += size;
}

void FillBuffer::Print(const char *__restrict fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    int ret = vsnprintf((char*)(temp_buffer), temp_buffer_size, fmt, arg);
    if(ret>0)
    {
        AddMessage(temp_buffer, ret, true);
        uint32_t rest_size = temp_buffer_size - ret;
        if(ending_cr_lf)
            ret += snprintf((char*)(temp_buffer+ret), rest_size, "\r\n");
        else
            ret += snprintf((char*)(temp_buffer+ret), rest_size, "\n");

        AddData(temp_buffer, ret);
    }
    va_end(arg);
}

void FillBuffer::AddBinary(const uint8_t* data, uint32_t size)
{
    if(esp8266_binary_format)
    {
        char str[32];
        //+IPD,<link ID>,<len>:
        int str_size = snprintf(str, sizeof(str), "+IPD,%i,%u:", esp8266_link_id, size);
        AddData((uint8_t*)str, str_size);
    } else
    {
        uint16_t p16[2];
        p16[0] = 0x0100;
        p16[1] = size;
        AddData((uint8_t*)p16, 4);
    }

    AddData(data, size);
    AddMessage(data, size, false);
}

void FillBuffer::AddStringAsBinary(const char* data)
{
    AddBinary((const uint8_t*) data, strlen(data)+1);
}

void FillBuffer::AddMessage(const uint8_t* data, uint32_t size, bool text)
{
    TBMessage message;
    message.is_text = text;
    message.size = size;
    if(text)
    {
        message.data = new uint8_t[size+1];
        message.data[size] = 0;
    }
    else
    {
        message.data = new uint8_t[size+1];
    }

    memcpy(message.data, data, size);
    messages.push_back(message);
}
