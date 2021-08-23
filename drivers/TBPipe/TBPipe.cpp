#include "TBPipe.h"
#include <stdlib.h>
#include <memory.h>
#include "sysctl.h"

static StandartPrefixParser standart_parser;

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

TBParse::TBParse(uint32_t buffer_size)
    : buffer_size(buffer_size)
    , parser(&standart_parser)
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

void TBParse::SetTimeout(uint32_t timeout_ms)
{
    this->timeout_ms = timeout_ms;
}

void TBParse::Append(uint8_t* data, uint32_t size)
{
    uint64_t cur_time = sysctl_get_time_us();
    if(timeout_ms!=0 && cur_time > last_append_time + timeout_ms)
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
