#pragma once
#include <stdint.h>
#include <stddef.h>

class NetworkClient
{
public:
    NetworkClient();
    ~NetworkClient();

    bool open(const char* host, uint16_t port);
    bool open(uint32_t ipv4_addr, uint16_t port);
    void close();
    bool is_open() const { return _socket != -1; };
    bool read(void* buffer, size_t buffer_size, size_t& readed_bytes);
    bool write(const void* buffer, size_t buffer_size, size_t& sended_bytes);
protected:
    int _socket = -1;
};