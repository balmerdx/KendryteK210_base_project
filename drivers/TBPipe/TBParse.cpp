#include "TBParse.h"
#include <stdlib.h>
#include <memory.h>

#ifdef __riscv
#include "sysctl.h"
static uint64_t time_us()
{
    return sysctl_get_time_us();
}
#else
#include <sys/time.h>
static uint64_t time_us()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}
#endif

BinPrefixParser::~BinPrefixParser()
{
    
}

TBParse::TBParse(uint32_t buffer_size, BinPrefixParser* parser)
    : buffer_size(buffer_size)
    , parser(parser)
{
    buffer = (uint8_t*)malloc(buffer_size);
}

TBParse::~TBParse()
{
    free(buffer);
}

void TBParse::SetParser(BinPrefixParser* p)
{
    parser = p;
}

void TBParse::SetTimeoutMs(uint32_t timeout_ms)
{
    this->timeout_us = timeout_ms*1000;
}

void TBParse::Append(uint8_t* data, uint32_t size)
{
    if(size==0)
        return;
    uint64_t cur_time = time_us();
    if(timeout_us!=0 && cur_time > last_append_time + timeout_us)
    {
        //Очищаем буфер по timeout
        buffer_amount = 0;
    }

    last_append_time = cur_time;

    uint32_t size_to_copy = size;
    if(buffer_amount + size > buffer_size)
        size_to_copy = buffer_size - buffer_amount;

    memcpy(buffer+buffer_amount, data, size_to_copy);
    buffer_amount += size_to_copy;
}

TBMessage TBParse::NextMessage()
{
    TBMessage message = {};
    uint32_t message_start = buffer_pos;
    if(bin_message_size)
    {
        if(buffer_pos + bin_message_size <= buffer_amount)
        {
            message.is_text = false;
            message.size = bin_message_size;
            message.data = buffer + buffer_pos;    
            buffer_pos += bin_message_size;
            bin_message_size = 0;
            return message;
        }
    } else
    while(buffer_pos < buffer_amount)
    {
        uint8_t c = buffer[buffer_pos++];
        BinPrefixParser::Result result;
        result = parser->Parse(c, buffer_pos-1==message_start);
        if(result==BinPrefixParser::Result::PrefixCompleted)
        {
            bin_message_size = parser->PacketSize();
            if(buffer_pos + bin_message_size <= buffer_amount)
            {
                message.is_text = false;
                message.size = bin_message_size;
                message.data = buffer + buffer_pos;    
                buffer_pos += bin_message_size;
                bin_message_size = 0;
                return message;
            } else
            {
                //Сообщения ещё нет в буфере, переходим в режим ожидания.
                message_start = buffer_pos;
                break;
            }
        } else
        if(result==BinPrefixParser::Result::PrefixMatched)
        {

        } else
        if(c=='\r' || c=='\n')
        {
            buffer[buffer_pos-1] = 0;
            message.size = buffer_pos - message_start - 1;
            if(message.size==0)
            {
                message_start = buffer_pos;
                continue;
            }

            message.is_text = true;
            message.data = buffer + message_start;
            return message;
        }
    }

    //Если конца сообщения не найдено, значит это последнее неполное сообщение.
    //Переносим его в начало буфера.
    uint32_t rest_size = buffer_amount-message_start;
    memmove(buffer, buffer + message_start, rest_size);
    buffer_amount = rest_size;
    buffer_pos = 0;
    return message;
}

StandartPrefixParser::StandartPrefixParser()
{
}

BinPrefixParser::Result StandartPrefixParser::Parse(uint8_t data, bool is_first)
{
    if(is_first)
        found_idx = 0;

    switch(found_idx)
    {
    default:
    case 0:
        if(data==0)
            found_idx = 1;
        break;
    case 1:
        if(data==1)
            found_idx = 2;
        else
            found_idx = 0;
        break;
    case 2:
        data_size = (uint32_t)data;
        found_idx = 3;
        break;
    case 3:
        data_size += ((uint32_t)data)<<8;
        found_idx = 0;
        return Result::PrefixCompleted;
    }

    if(found_idx==0)
        return Result::NotMatched;

    return Result::PrefixMatched;
}

Esp8266PrefixParser::Esp8266PrefixParser()
{
}

BinPrefixParser::Result Esp8266PrefixParser::Parse(uint8_t data, bool is_first)
{
    static const char* prefix = "+IPD,";
    if(is_first)
    {
        offset = 0;
        state = State::Prefix;
    }

    switch(state)
    {
    case State::Prefix:
        if(prefix[offset]==data)
        {
            offset++;
            if(prefix[offset]==0)
            {
                state = State::LinkID;
                offset = 0;
            }
        } else
        {
            state = State::NotPassed;
        }
        break;
    case State::LinkID:
        if(data==',')
        {
            num_buf[offset] = 0;
            link_id = atoi(num_buf);
            offset = 0;
            state = State::Len;
        } else
        {
            //Не делаем никаких проверок отностительно числа.
            //Предполагаем, что тут могут быть только цифры.
            if(offset+1<sizeof(num_buf))
                num_buf[offset++] = data;
        }
        return Result::PrefixMatched;
    case State::Len:
        if(data==',' || data==':')
        {
            num_buf[offset] = 0;
            packet_size = atoi(num_buf);
            state = State::Ending;
            if(data==':')
            {
                return Result::PrefixCompleted;
            }
        } else
        {
            //Не делаем никаких проверок отностительно числа.
            //Предполагаем, что тут могут быть только цифры.
            if(offset+1<sizeof(num_buf))
                num_buf[offset++] = data;
        }
        return Result::PrefixMatched;
    case State::Ending:
        if(data==':')
            return Result::PrefixCompleted;
        return Result::PrefixMatched;
    case State::NotPassed:
        break;
    }

    return Result::NotMatched;
}
