#include "network_client.h"

#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

NetworkClient::NetworkClient()
{

}

NetworkClient::~NetworkClient()
{
    close();
}

void NetworkClient::close()
{
    if(_socket != -1)
    {
        ::close(_socket);
        _socket = -1;
    }
}


bool NetworkClient::open(const char* host, uint16_t port)
{
    struct hostent* server = gethostbyname(host);
    if(server==nullptr)
        return false;

    if(server->h_length<=0)
        return false;

    if(server->h_addrtype!=AF_INET)
        return false;

    return open(*(uint32_t*)(server->h_addr_list[0]), port);
}

bool NetworkClient::open(uint32_t ipv4_addr, uint16_t port)
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (_socket < 0) {
        _socket = -1;
        return false;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ipv4_addr;
    addr.sin_port = htons(port);

    if (::connect(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close();
        return false;
    }

    int nonBlocking = 1;
    ioctl(_socket, FIONBIO, &nonBlocking);
    return true;
}

bool NetworkClient::read(void* buffer, size_t buffer_size, size_t& readed_bytes)
{
    if (_socket == -1) {
        readed_bytes = 0;
        return false;
    }

    ssize_t result = recv(_socket, buffer, buffer_size, MSG_DONTWAIT);

    if (result <= 0) {
        if(errno != EAGAIN)
        {
            close();
            readed_bytes = 0;
            return false;
        } else
        {
            readed_bytes = 0;
            return true;
        }
    }

    readed_bytes = (size_t)result;
    return true;
}

bool NetworkClient::write(const void* buffer, size_t buffer_size, size_t& sended_bytes)
{
    if (_socket == -1) {
        sended_bytes = 0;
        return false;
    }

    //int result = send(_socket, buffer, buffer_size, MSG_DONTWAIT);
    int result = send(_socket, buffer, buffer_size, 0);

    if (result < 0) {
        if(errno == EAGAIN) {
            sended_bytes = 0;
            return true;
        }
        close();
        sended_bytes = 0;
        return false;
    }

    sended_bytes = (size_t)result;
    return true;
}
