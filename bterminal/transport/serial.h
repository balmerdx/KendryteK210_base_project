#pragma once
#include <stdint.h>
#include <stdio.h>

class SerialPort
{
public:
    SerialPort();
    ~SerialPort();
    //timeout_ms == 0 - nonblocking mode
    //timeout_ms == -1 - wait 1 byte
    bool open(const char* device, uint32_t baudrate = 115200, int32_t timeout_ms = 0);
    void close();
    bool is_open() const { return _fd!=-1;}
    //return readed bytes count
    //return negative if error
    ssize_t read(void* buf, size_t bytes);
    //return write bytes count
    //return negative if error
    ssize_t write(const void* buf, size_t bytes);
protected:
    void configure(uint32_t baudrate, int32_t timeout_ms);
    int _fd = -1;
};
