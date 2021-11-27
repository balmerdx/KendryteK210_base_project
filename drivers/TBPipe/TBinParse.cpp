#include "TBinParse.h"

#include <stdlib.h>
#include <memory.h>

TBinParse::TBinParse(uint32_t buffer_size)
    : buffer_size(buffer_size)
{
    buffer = (uint8_t*)malloc(buffer_size);
}

TBinParse::~TBinParse()
{
    free(buffer);
}

void TBinParse::Append(uint8_t* data, uint32_t size)
{
    if(size==0)
        return;

    if(buffer_amount + size > buffer_size)
    {
        parse_failed = true;
        return;
    }

    memcpy(buffer+buffer_amount, data, size);
    buffer_amount += size;
}

void TBinParse::AppendInplace(uint32_t size)
{
    if(size==0)
        return;

    if(buffer_amount + size > buffer_size)
    {
        parse_failed = true;
        return;
    }

    buffer_amount += size;
}


TBinMessage TBinParse::NextMessage()
{
    TBinMessage message = {};
    if(parse_failed)
        return message;
    if(bin_message_size)
    {
MessageHeaderPassed:        
        if(buffer_pos + bin_message_size <= buffer_amount)
        {
            message.size = bin_message_size;
            message.data = buffer + buffer_pos;
            message.command = bin_message_command;
            buffer_pos += bin_message_size;
            bin_message_size = 0;
            return message;
        }
    } else
    if(buffer_amount - buffer_pos >= 8)
    {
        uint64_t head; //Тут падало, т.к. buffer_pos misaligned offset
        memcpy(&head, buffer + buffer_pos, 8);
        if(ParseHeader(head))
        {
            buffer_pos += 8;
            if(bin_message_size+8 > buffer_size)
            {
                bin_message_size = 0;
                bin_message_command = 0;
                parse_failed = true;    
            } else
            {
                goto MessageHeaderPassed;
            }
        } else
        {
            parse_failed = true;
        }
    }

    //Если конца сообщения не найдено, значит это последнее неполное сообщение.
    //Переносим его в начало буфера.
    uint32_t rest_size = buffer_amount-buffer_pos;
    memmove(buffer, buffer + buffer_pos, rest_size);
    buffer_amount = rest_size;
    buffer_pos = 0;
    return message;
}

bool TBinParse::ParseHeader(uint64_t header)
{
    uint32_t data_size = (uint32_t)header;
    uint16_t packet_id = (uint16_t)(header>>32);
    uint16_t header_crc = (uint16_t)(header>>48);

    uint16_t calculated_crc = 0xFFFF 
                ^ (data_size&0xFFFF) 
                ^ ((data_size>>16)&0xFFFF)
                ^ packet_id;
    bool ok = calculated_crc == header_crc;
    if(ok)
    {
        bin_message_size = data_size;
        bin_message_command = packet_id;
    } else
    {
        bin_message_size = 0;
        bin_message_command = 0;
    }

    return ok;
}

uint64_t TBinFillHeader(uint16_t packet_id, uint32_t data_size)
{
    uint64_t head = data_size | (((uint64_t)packet_id)<<32);
    uint64_t crc = 0xFFFF 
                ^ (data_size&0xFFFF) 
                ^ ((data_size>>16)&0xFFFF)
                ^ packet_id;
    return head | (crc<<48);
}

void TBinParse::Clear()
{
    buffer_amount = 0;
    buffer_pos = 0;
    bin_message_size = 0;
    bin_message_command = 0;
}