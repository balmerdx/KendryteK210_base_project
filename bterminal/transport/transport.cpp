#include "transport.h"
#include "serial.h"
#include "network_client.h"

Transport::~Transport()
{
}

class TransportSerial : public Transport
{
public:
    TransportSerial();
    ~TransportSerial() override;
    
    bool read(void* buffer, size_t buffer_size, size_t& readed_bytes) override;
    bool write(const void* buffer, size_t buffer_size, size_t& readed_bytes) override;
    bool is_connected() override;

    SerialPort serial;
};

Transport* TransportCreateSerial(const char* device, uint32_t baudrate)
{
    TransportSerial* p = new TransportSerial();
    if(!p->serial.open(device, baudrate))
    {
        delete p;
        return nullptr;
    }

    return p;
}

TransportSerial::TransportSerial()
{
}

TransportSerial::~TransportSerial()
{
}

bool TransportSerial::read(void* buffer, size_t buffer_size, size_t& readed_bytes)
{
    ssize_t ret = serial.read(buffer, buffer_size);
    if(ret<0)
    {
        readed_bytes = 0;
        return false;
    }

    readed_bytes = ret;
    return true;
}

bool TransportSerial::write(const void* buffer, size_t buffer_size, size_t& sended_bytes)
{
    ssize_t ret = serial.write(buffer, buffer_size);
    if(ret<0)
    {
        sended_bytes = 0;
        return false;
    }

    sended_bytes = ret;
    return true;
}

bool TransportSerial::is_connected()
{
    return serial.is_open();
}


class TransportNetwork : public Transport
{
public:
    TransportNetwork();
    ~TransportNetwork() override;
    
    bool read(void* buffer, size_t buffer_size, size_t& readed_bytes) override;
    bool write(const void* buffer, size_t buffer_size, size_t& readed_bytes) override;
    bool is_connected() override;

    NetworkClient net;
};

TransportNetwork::TransportNetwork()
{
}

TransportNetwork::~TransportNetwork()
{
}

bool TransportNetwork::read(void* buffer, size_t buffer_size, size_t& readed_bytes)
{
    return net.read(buffer, buffer_size, readed_bytes);
}

bool TransportNetwork::write(const void* buffer, size_t buffer_size, size_t& readed_bytes)
{
    return net.write(buffer, buffer_size, readed_bytes);
}

bool TransportNetwork::is_connected()
{
    return net.is_open();
}

Transport* TransportCreateNetwork(const char* host, uint16_t port)
{
    TransportNetwork* p = new TransportNetwork;
    if(!p->net.open(host, port))
    {
        delete p;
        return nullptr;
    }

    return p;
}

Transport* TransportCreateNetwork(uint32_t ipv4, uint16_t port)
{
    TransportNetwork* p = new TransportNetwork;
    if(!p->net.open(ipv4, port))
    {
        delete p;
        return nullptr;
    }

    return p;
}