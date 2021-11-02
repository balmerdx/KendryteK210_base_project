#include "TBPipe.h"
#include <stdlib.h>
#include <memory.h>
#include "sysctl.h"

//#define USE_BUILDIN_ATOMIC

TBPipe::TBPipe(int buffer_size)
    : buffer_size(buffer_size)
{
    static_assert(sizeof(char)==1);
    static_assert(sizeof(uint32_t)==4);
    static_assert(sizeof(uint64_t)==8);
    static_assert(__atomic_always_lock_free(sizeof(uint32_t),0));

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

TBPipe::otype TBPipe::AvailableBytesInternal(otype read_pos, otype write_pos) const
{
    return buffer_size-1-FreeBytesInternal(read_pos, write_pos);
}

uint32_t TBPipe::Write(uint8_t* bytes, uint32_t count)
{
#ifdef USE_BUILDIN_ATOMIC
    otype read_pos, write_pos;
    __atomic_load(&this->read_pos, &read_pos, __ATOMIC_ACQUIRE);
    __atomic_load(&this->write_pos, &write_pos, __ATOMIC_ACQUIRE);
#else
    otype read_pos = this->read_pos;
    otype write_pos = this->write_pos;
#endif

    TBPipe::otype free_bytes = FreeBytesInternal(read_pos, write_pos);
    //Пускай free_bytes делится на 8 всегда
    free_bytes = free_bytes & ~7ull;
    if(count > free_bytes)
    {
        count = free_bytes;
    }

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

#ifdef USE_BUILDIN_ATOMIC
    __atomic_store(&this->write_pos, &write_pos, __ATOMIC_RELEASE);
#else
    this->write_pos = write_pos;
#endif
    return (uint32_t)count;
}

void TBPipe::ReadStart(uint8_t*& data, uint32_t& size)
{
#ifdef USE_BUILDIN_ATOMIC
    otype read_pos, write_pos;
    __atomic_load(&this->read_pos, &read_pos, __ATOMIC_ACQUIRE);
    __atomic_load(&this->write_pos, &write_pos, __ATOMIC_ACQUIRE);
#else
    otype read_pos = this->read_pos;
    otype write_pos = this->write_pos;
#endif

    if (write_pos == read_pos)
    {
        size = 0;
        data = nullptr;
        read_pos_tmp = read_pos;
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

    read_pos_tmp = read_pos;
}

void TBPipe::ReadEnd()
{
#ifdef USE_BUILDIN_ATOMIC
    __atomic_store(&this->read_pos, &read_pos_tmp, __ATOMIC_RELEASE);
#else
    this->read_pos = read_pos_tmp;
#endif
}

bool TBPipe::ReadExact(uint8_t* data, uint32_t count)
{
#ifdef USE_BUILDIN_ATOMIC
    otype read_pos, write_pos;
    __atomic_load(&this->read_pos, &read_pos, __ATOMIC_ACQUIRE);
    __atomic_load(&this->write_pos, &write_pos, __ATOMIC_ACQUIRE);
#else
    otype read_pos = this->read_pos;
    otype write_pos = this->write_pos;
#endif

    otype alailable = AvailableBytesInternal(read_pos, write_pos);

    if (alailable < count)
        return false;

    if (write_pos < read_pos)
    {
        otype size = buffer_size - read_pos;
        if(size >= count)
        {
            memcpy(data, buffer + read_pos, count);
            read_pos += count;
            count = 0;
        } else
        {
            memcpy(data, buffer + read_pos, size);
            read_pos = 0;
            count -= size;
            data += size;
        }
    }

    if(count > 0) //write_pos >= read_pos
    {
        otype size = write_pos - read_pos;
        if(count > size)
        {
            //assert(0);
            int k=0;
        }

        memcpy(data, buffer + read_pos, count);
        read_pos += count;
        if(read_pos==buffer_size)
            read_pos = 0;
    }

#ifdef USE_BUILDIN_ATOMIC
    __atomic_store(&this->read_pos, &read_pos, __ATOMIC_RELEASE);
#else
    this->read_pos = read_pos;
#endif
    return true;
}

uint32_t TBPipe::FreeBytes() const
{
#ifdef USE_BUILDIN_ATOMIC
    otype read_pos, write_pos;
    __atomic_load(&this->read_pos, &read_pos, __ATOMIC_ACQUIRE);
    __atomic_load(&this->write_pos, &write_pos, __ATOMIC_ACQUIRE);
#endif
    return (uint32_t)FreeBytesInternal(read_pos, write_pos);
}

uint32_t TBPipe::AvailableBytes() const
{
#ifdef USE_BUILDIN_ATOMIC
    otype read_pos, write_pos;
    __atomic_load(&this->read_pos, &read_pos, __ATOMIC_ACQUIRE);
    __atomic_load(&this->write_pos, &write_pos, __ATOMIC_ACQUIRE);
#endif
    return (uint32_t)AvailableBytesInternal(read_pos, write_pos);
}

void TBPipe::Clear()
{
    uint8_t* data;
    uint32_t size;
    ReadStart(data, size);
    ReadEnd();
    ReadStart(data, size);
    ReadEnd();
}
