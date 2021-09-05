/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <errno.h>
#include <string.h>

#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include "WiFiClient.h"


WiFiClient::WiFiClient() :
  WiFiClient(-1)
{
}

WiFiClient::WiFiClient(int socket) :
  _socket(socket)
{
}

int WiFiClient::connect(const char* host, uint16_t port)
{
  struct hostent* server = gethostbyname(host);

  if (server == NULL) {
      return 0;
  }

  return connect(server->h_addr, port);
}

int WiFiClient::connect(/*IPAddress*/uint32_t ip, uint16_t port)
{
  _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (_socket < 0) {
    _socket = -1;
    return 0;
  }

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t)ip;
  addr.sin_port = htons(port);

  if (::connect(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(_socket);
    _socket = -1;
    return 0;
  }

  int nonBlocking = 1;
  ioctl(_socket, FIONBIO, &nonBlocking);
  return 1;
}

size_t WiFiClient::write(const uint8_t *buf, size_t size)
{
  if (_socket == -1) {
    return 0;
  }

  int result = send(_socket, (void*)buf, size, MSG_DONTWAIT);

  if (result < 0) {
    close(_socket);
    _socket = -1;
    return 0;
  }

  return result;
}

int WiFiClient::available()
{
  if (_socket == -1) {
    return 0;
  }

  int result = 0;

  if (ioctl(_socket, FIONREAD, &result) < 0) {
    printf("WiFiClient::available _socket=%i errno=%i\n", _socket, (int)errno);
    close(_socket);
    _socket = -1;
    return 0;
  }

  return result;
}

int WiFiClient::read(uint8_t* buf, size_t size)
{
  int result = recv(_socket, buf, size, MSG_DONTWAIT);

  if (result <= 0 && errno != EWOULDBLOCK) {
    close(_socket);
    _socket = -1;
    return -1;
  }

  return result;
}

int WiFiClient::peek()
{
  uint8_t b;

  if (recv(_socket, &b, sizeof(b), MSG_PEEK | MSG_DONTWAIT) <= 0) {
    if (errno != EWOULDBLOCK) {
      close(_socket);
      _socket = -1;
    }

    return -1;
  }

  return b;
}

void WiFiClient::stop()
{
  if (_socket != -1) {
    close(_socket);
    _socket = -1;
    printf("WiFiClient::stop _socket=-1\n");
  }
}

uint8_t WiFiClient::connected()
{
  if (_socket != -1) {
    // use peek to update socket state
    peek();
  }

  return (_socket != -1);
}

WiFiClient::operator bool()
{
  return (_socket != -1);
}

bool WiFiClient::operator==(const WiFiClient &other) const
{
  return (_socket == other._socket);
}

/*IPAddress*/uint32_t WiFiClient::remoteIP()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_socket, (struct sockaddr*)&addr, &len);

  return ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
}

uint16_t WiFiClient::remotePort()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_socket, (struct sockaddr*)&addr, &len);

  return ntohs(((struct sockaddr_in *)&addr)->sin_port);
}
