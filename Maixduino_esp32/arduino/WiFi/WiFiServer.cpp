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

#include <lwip/sockets.h>

#include "WiFiClient.h"
#include "WiFiServer.h"

WiFiServer::WiFiServer() :
  WiFiServer(0)
{
}

WiFiServer::WiFiServer(uint16_t port) :
  _port(port),
  _socket(-1)
{
}

bool WiFiServer::begin()
{
  _socket = lwip_socket(AF_INET, SOCK_STREAM, 0);

  if (_socket < 0) {
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t)0;
  addr.sin_port = htons(_port);

  if (lwip_bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    lwip_close(_socket);
    _socket = -1;
    return false;
  }

  if (lwip_listen(_socket, 1) < 0) {
    lwip_close(_socket);
    _socket = -1;
    return false;
  }

  int nonBlocking = 1;
  lwip_ioctl(_socket, FIONBIO, &nonBlocking);

  return true;
}

WiFiClient WiFiServer::accept()
{
  if(_socket==-1)
    return WiFiClient(-1);

  int result = lwip_accept(_socket, NULL, 0);
  if (result <= 0 && errno != EWOULDBLOCK) {
    close(_socket);
    _socket = -1;
  }

  return WiFiClient(result);
}

WiFiServer::operator bool()
{
  return (_port != 0 && _socket != -1);
}

void WiFiServer::stop()
{
  if (_socket != -1) {
    close(_socket);
    _socket = -1;
  }
}
