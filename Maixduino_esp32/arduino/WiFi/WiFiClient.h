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
#pragma once
#include <Arduino.h>

class WiFiClient {

public:
  WiFiClient();
  WiFiClient(int socket);

  virtual int connect(/*IPAddress*/uint32_t ip, uint16_t port);
  virtual int connect(const char* host, uint16_t port);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();
  bool operator==(const WiFiClient &other) const;

  virtual /*IPAddress*/uint32_t remoteIP();
  virtual uint16_t remotePort();

private:
  int _socket;
};

