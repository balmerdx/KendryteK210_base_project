#pragma once
/*
Этот класс предназначен для того, что-бы принимать данные по UART в прерываниии и читать их в основном коде.
Это lock-free ring buffer.
Т.е. читать можно даже из другого потока чем записанно.
Предполагается, что из буфера будет вычитываться быстрее, чем записывается.
*/

#include <stdint.h>
class TBPipe
{
public:
    //uint64_t - что-бы случайно не пересекалось при чтении-записи на K210
    typedef uint64_t otype;

    //Вариант, когда используя malloc создаются буфера.
    //Общий размер выделяемой памяти buffer_size*2
    TBPipe(int buffer_size);
    ~TBPipe();
    //bytes - байты для записи
    //count - количество записываемых байт.
    //return - количество байт, которое действительно записенно
    uint32_t Write(uint8_t* bytes, uint32_t count);
    //Вызывается из основного кода
    //Возвращает указатель на непрерывный буфер читаемых данных.
    //Это возможно не все данные, находящиеся в буфере.
    void Read(uint8_t*& data, uint32_t& size);

    int BufferSize() const { return buffer_size; }
    uint32_t FreeBytes() const;
    uint32_t AvailableBytes() const;
protected:
    TBPipe::otype FreeBytesInternal(otype read_pos, otype write_pos) const;

    uint8_t* buffer;
    otype buffer_size;
    volatile otype read_pos = 0;
    volatile otype write_pos = 0;
};

