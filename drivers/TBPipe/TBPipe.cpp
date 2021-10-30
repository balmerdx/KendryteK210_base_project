#include "TBPipe.h"
#include <stdlib.h>
#include <memory.h>
#include "sysctl.h"

TBPipe::TBPipe(int buffer_size)
    : buffer_size(buffer_size)
{
    static_assert(sizeof(char)==1);
    static_assert(sizeof(uint32_t)==4);
    static_assert(sizeof(uint64_t)==8);

    buffer = (uint8_t*)malloc(buffer_size);
}

TBPipe::~TBPipe()
{
    free(buffer);
}

TBPipe::otype TBPipe::FreeBytesInternal(TBPipe::otype read_pos, TBPipe::otype write_pos) const
{
    if (write_pos < read_pos)
        return read_pos - write_pos - 1;
    else
        return buffer_size + read_pos - write_pos - 1;
}

uint32_t TBPipe::Write(uint8_t* bytes, uint32_t count)
{
    otype read_pos = this->read_pos;
    otype write_pos = this->write_pos;

    TBPipe::otype free_bytes = FreeBytesInternal(read_pos, write_pos);
    if(count > free_bytes)
        count = free_bytes;
    if(count==0)
        return (uint32_t)count;

    if (write_pos < read_pos)
    {
        memcpy(buffer+write_pos, bytes, count);
        write_pos += count;
    } else
    {
        otype count_to_end = buffer_size - write_pos;
        if(count_to_end >= count)
        {
            memcpy(buffer+write_pos, bytes, count);
            write_pos += count;
        } else
        {
            memcpy(buffer+write_pos, bytes, count_to_end);
            memcpy(buffer, bytes+count_to_end, count-count_to_end);
            write_pos = count-count_to_end;
        }
    }

    if(write_pos==buffer_size)
        write_pos = 0;

    this->write_pos = write_pos;
    return (uint32_t)count;
}

void TBPipe::Read(uint8_t*& data, uint32_t& size)
{
    otype read_pos = this->read_pos;
    otype write_pos = this->write_pos;

    if (write_pos == read_pos)
    {
        size = 0;
        data = nullptr;
        return;
    }

    data = buffer + read_pos;

    if (write_pos < read_pos)
    {
        size = buffer_size - read_pos;
        read_pos = 0;
    } else
    {
        size = write_pos - read_pos;
        read_pos += size;
        if(read_pos==buffer_size)
            read_pos = 0;
    }

    this->read_pos = read_pos;
}

uint32_t TBPipe::FreeBytes() const
{
    return (uint32_t)FreeBytesInternal(read_pos, write_pos);
}

uint32_t TBPipe::AvailableBytes() const
{
    return (uint32_t)(buffer_size-FreeBytesInternal(read_pos, write_pos));
}
