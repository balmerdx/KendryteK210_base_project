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
    //uint32_t - достаточно как на Intel x64 так и на Kendryte K210
    typedef uint32_t otype;

    //Вариант, когда используя malloc создаются буфера.
    //Общий размер выделяемой памяти buffer_size*2
    TBPipe();
    TBPipe(int buffer_size);
    ~TBPipe();

    //Самый простой init. Предполагаем, что вызывается сразу после конструктора без параметров.
    //Для переинициализации не подходит.
    void Init(int buffer_size);
    //bytes - байты для записи
    //count - количество записываемых байт.
    //return - количество байт, которое действительно записенно
    uint32_t Write(uint8_t* bytes, uint32_t count);

    //Вызывается из основного кода
    //Возвращает указатель на непрерывный буфер читаемых данных.
    //Это возможно не все данные, находящиеся в буфере.
    //После окончания чтения - вызывать ReadEnd
    void ReadStart(uint8_t*& data, uint32_t& size);
    void ReadEnd();

    //Прочитать именно count количество байт.
    //data - буфер, размером не менее, чем count для записи инфомации.
    //count - количество читаемых байт
    //return true если есть достаточно данных, и они скопированны в буфер data.
    bool ReadExact(uint8_t* data, uint32_t count);

    otype BufferSize() const { return buffer_size; }
    uint32_t FreeBytes() const;
    uint32_t AvailableBytes() const;

    //Пытается прочитать все данные, которые были в буфере.
    //После этого AvailableBytes будет нулевым, если за это время не успели записать новых данных.
    void Clear();
protected:
    TBPipe::otype FreeBytesInternal(otype read_pos, otype write_pos) const;
    TBPipe::otype AvailableBytesInternal(otype read_pos, otype write_pos) const;

    uint8_t* buffer;
    otype buffer_size;
    volatile otype read_pos = 0;
    volatile otype write_pos = 0;

    volatile otype read_pos_tmp = 0;
};

