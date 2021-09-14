#pragma once
#include <stdint.h>
#include <stddef.h>
/*
    У нас будет два варианта достучаться до машинки.
    1. UART
    2. WiFi
*/
class Transport
{
public:
    virtual ~Transport();

    //return false if fail
    virtual bool read(void* buffer, size_t buffer_size, size_t& readed_bytes) = 0;
    virtual bool write(const void* buffer, size_t buffer_size, size_t& sended_bytes) = 0;

    virtual bool is_connected() = 0;
};

Transport* TransportCreateSerial(const char* device, uint32_t baudrate = 115200);

Transport* TransportCreateNetwork(const char* host, uint16_t port);
Transport* TransportCreateNetwork(uint32_t ipv4, uint16_t port);