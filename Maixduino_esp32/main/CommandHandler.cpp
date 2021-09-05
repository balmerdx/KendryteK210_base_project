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

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiSSLClient.h>
#include <WiFiUdp.h>
#include "esp_wpa2.h"
#include "Esp32CommandList.h"

#include <driver/spi_common.h>

#include "CommandHandler.h"

#include "Arduino.h"

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

const char FIRMWARE_VERSION[6] = "0.0.1";

// Optional, user-defined X.509 certificate
char CERT_BUF[1300];
bool setCert = 0;

// Optional, user-defined RSA private key
char PK_BUFF[1700];
bool setPSK = 0;

#define MAX_SOCKETS CONFIG_LWIP_MAX_SOCKETS
uint8_t socketTypes[MAX_SOCKETS];
WiFiClient tcpClients[MAX_SOCKETS];
WiFiUDP udps[MAX_SOCKETS];
WiFiSSLClient tlsClients[MAX_SOCKETS];
WiFiServer tcpServers[MAX_SOCKETS];

int invertBytes(const uint8_t command[], uint8_t response[])
{
  const uint32_t* cmd32 = (const uint32_t*)command;
  uint32_t* resp32 = (uint32_t*)response;
  uint32_t size = cmd32[0]>>16;
  if(size>=SPI_BUFFER_LEN)
  {
    resp32[0] = size;
    return 4;
  }

  for(uint32_t i=0; i<size; i++)
  {
    response[i] = command[i+4] ^ 0xFF;
  }

  return size;
}


int setSsidAndPass(const uint8_t command[], uint8_t response[])
{
  const char* ssid = (const char*)(command+1);
  const char* pass = ssid+strlen(ssid)+1;

  WiFi.begin(ssid, pass);

  *(uint32_t*)response = command[0];
  return CESP_RESP_SET_SSID_AND_PASS;
}

int setHostname(const uint8_t command[], uint8_t response[])
{
  const char* hostname = (const char*)(command + 4);
  *(uint32_t*)response = WiFi.hostname(hostname)?1:0;
  return 4;
}

int setPowerMode(const uint8_t command[], uint8_t response[])
{
  if (command[4]) {
    // low power
    WiFi.lowPowerMode();
  } else {
    // no low power
    WiFi.noLowPowerMode();
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setApPassPhrase(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];
  char pass[64 + 1];
  uint8_t channel;

  memset(ssid, 0x00, sizeof(ssid));
  memset(pass, 0x00, sizeof(pass));

  memcpy(ssid, &command[4], command[3]);
  memcpy(pass, &command[5 + command[3]], command[4 + command[3]]);
  channel = command[6 + command[3] + command[4 + command[3]]];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (WiFi.beginAP(ssid, pass, channel) == WL_CONNECTING)?1:0;

  return 6;
}

int setDebug(const uint8_t command[], uint8_t response[])
{
  setDebug(command[1]);
  *(uint32_t*)response = 1;
  return CESP_RESP_SET_DEBUG;
}

extern "C" {
  uint8_t temprature_sens_read();
}

int getTemperature(const uint8_t command[], uint8_t response[])
{
  float temperature = (temprature_sens_read() - 32) / 1.8;
  memcpy(response, &temperature, sizeof(temperature));
  return CESP_RESP_GET_TEMPERATURE;
}

int getConnStatus(const uint8_t command[], uint8_t response[])
{
  uint8_t status = WiFi.status();

  response[0] = CESP_GET_CONN_STATUS;
  response[1] = status;

  return CESP_RESP_GET_CONN_STATUS;
}

int getIPaddr(const uint8_t command[], uint8_t response[])
{
  WiFi.updateIpInfo();
  /*IPAddress*/uint32_t ip = WiFi.localIP();
  /*IPAddress*/uint32_t mask = WiFi.subnetMask();
  /*IPAddress*/uint32_t gwip = WiFi.gatewayIP();

  response[2] = 3; // number of parameters

  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &ip, sizeof(ip));

  response[8] = 4; // parameter 2 length
  memcpy(&response[9], &mask, sizeof(mask));

  response[13] = 4; // parameter 3 length
  memcpy(&response[14], &gwip, sizeof(gwip));

  return 19;
}

int getMACaddr(const uint8_t command[], uint8_t response[])
{
  uint8_t mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  WiFi.macAddress(mac);

  response[2] = 1; // number of parameters
  response[3] = sizeof(mac); // parameter 1 length

  memcpy(&response[4], mac, sizeof(mac));

  return 11;
}

int getCurrSSID(const uint8_t command[], uint8_t response[])
{
  // ssid
  const char* ssid = WiFi.SSID();
  uint8_t ssidLen = strlen(ssid);

  response[2] = 1; // number of parameters
  response[3] = ssidLen; // parameter 1 length

  memcpy(&response[4], ssid, ssidLen);

  return (5 + ssidLen);
}

int getCurrBSSID(const uint8_t command[], uint8_t response[])
{
  uint8_t bssid[6];

  WiFi.BSSID(bssid);

  response[2] = 1; // number of parameters
  response[3] = 6; // parameter 1 length

  memcpy(&response[4], bssid, sizeof(bssid));

  return 11;
}

int getCurrRSSI(const uint8_t command[], uint8_t response[])
{
  int32_t rssi = WiFi.RSSI();

  response[2] = 1; // number of parameters
  response[3] = sizeof(rssi); // parameter 1 length

  memcpy(&response[4], &rssi, sizeof(rssi));

  return 9;
}

int getCurrEnct(const uint8_t command[], uint8_t response[])
{
  uint8_t encryptionType = WiFi.encryptionType();

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = encryptionType;

  return 6;
}

int scanNetworks(const uint8_t command[], uint8_t response[])
{
  WiFi.scanNetworks();

  response[0] = CESP_SCAN_NETWORKS;
  response[1] = WiFi.scanResultsCount();
  response[2] = 0;
  response[3] = 0;

  return CESP_RESP_SCAN_NETWORKS;
}

int scanNetworksResult(const uint8_t command[], uint8_t response[])
{
  int responseLength = 0;
  uint8_t numNetworks = WiFi.scanResultsCount();
  response[responseLength++] = numNetworks;

  for (int i = 0; i < numNetworks; i++) {
    response[responseLength++] = WiFi.RSSI(i);
    response[responseLength++] = WiFi.encryptionType(i);

    const char* ssid = WiFi.SSID(i);
    int ssidMaxLen = 33;
    memcpy(&response[responseLength], ssid, ssidMaxLen);
    responseLength += ssidMaxLen;
  }

  return responseLength;
}


int startServerTcp(const uint8_t command[], uint8_t response[])
{
  uint32_t ip = 0;
  uint16_t port;
  uint8_t socket;
  uint8_t type;

  if (command[2] == 3) {
    memcpy(&port, &command[4], sizeof(port));
    port = ntohs(port);
    socket = command[7];
    type = command[9];
  } else {
    memcpy(&ip, &command[4], sizeof(ip));
    memcpy(&port, &command[9], sizeof(port));
    port = ntohs(port);
    socket = command[12];
    type = command[14];
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (type == 0x00) {
    tcpServers[socket] = WiFiServer(port);

    tcpServers[socket].begin();

    socketTypes[socket] = 0x00;
    response[4] = 1;
  } else if (type == 0x01 && udps[socket].begin(port)) {
    socketTypes[socket] = 0x01;
    response[4] = 1;
  } else if (type == 0x03 && udps[socket].beginMulticast(ip, port)) {
    socketTypes[socket] = 0x01;
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int availSocketData(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[1];
  uint16_t available = 0;

  if (socketTypes[socket] == 0x00) {
    if (tcpServers[socket]) {
      WiFiClient client = tcpServers[socket].available();

      available = 255;

      if (client) {
        // try to find existing socket slot
        for (int i = 0; i < MAX_SOCKETS; i++) {
          if (i == socket) {
            continue; // skip this slot
          }

          if (socketTypes[i] == 0x00 && tcpClients[i] == client) {
            available = i;
            break;
          }
        }

        if (available == 255) {
          // book keep new slot

          for (int i = 0; i < MAX_SOCKETS; i++) {
            if (i == socket) {
              continue; // skip this slot
            }

            if (socketTypes[i] == 255) {
              socketTypes[i] = 0x00;
              tcpClients[i] = client;

              available = i;
              break;
            }
          }
        }
      }
    } else {
      available = tcpClients[socket].available();
    }
  } else if (socketTypes[socket] == 0x01) {
    available = udps[socket].available();

    if (available <= 0) {
      available = udps[socket].parsePacket();
    }
  } else if (socketTypes[socket] == 0x02) {
    available = tlsClients[socket].available();
  }

  *(uint32_t*)response = available;

  return CESP_RESP_AVAIL_SOCKET_DATA;
}

int startSocketClient(const uint8_t command[], uint8_t response[])
{
  const char* host = NULL;
  uint32_t ip;
  uint16_t port;
  uint8_t socket;
  uint8_t type;

  memcpy(&port, command+2, sizeof(port));
  socket = command[4];
  type = command[5];

  //printf("startSocketClient type=%i port=%u\n", (int)type, (int)port);
  if (command[1] == 0) {
    memcpy(&ip, command+8, sizeof(ip));
    //printf("ip=%u.%u.%u.%u\n", ip&0xFF, (ip>>8)&0xFF, (ip>>16)&0xFF, (ip>>24)&0xFF);
  } else {
    host = (const char*)(command+8);
  }
  int result = 0;
  if (type == 0x00) {//TCP mode
    if (host) {
      result = tcpClients[socket].connect(host, port);
    } else {
      result = tcpClients[socket].connect(ip, port);
    }

    if (result)
      socketTypes[socket] = 0x00;
  } else if (type == 0x01) {//UDP Mode
    result = udps[socket].begin();
    if(result){
      if (host) {
        result = udps[socket].beginPacket(host, port);
      } else {
        result = udps[socket].beginPacket(ip, port);
      }
    }

    if (result)
      socketTypes[socket] = 0x01;
  } else if (type == 0x02) {//TLS Mode
    if (host) {
      if (setCert && setPSK) {
        tlsClients[socket].setCertificate(CERT_BUF);
        tlsClients[socket].setPrivateKey(PK_BUFF);
      }
      result = tlsClients[socket].connect(host, port);
    } else {
      if (setCert && setPSK) {
        tlsClients[socket].setCertificate(CERT_BUF);
        tlsClients[socket].setPrivateKey(PK_BUFF);
      }
      result = tlsClients[socket].connect(ip, port);
    }

    if (result) {
      socketTypes[socket] = 0x02;
    }
  }

  *(uint32_t*)response = result?1:0;
  return CESP_RESP_START_SOCKET_CLIENT;
}

int closeSocket(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[1];

  if (socketTypes[socket] == 0x00) {
    tcpClients[socket].stop();

    socketTypes[socket] = 255;
  } else if (socketTypes[socket] == 0x01) {
    udps[socket].stop();

    socketTypes[socket] = 255;
  } else if (socketTypes[socket] == 0x02) {
    tlsClients[socket].stop();

    socketTypes[socket] = 255;
  }

  *(uint32_t*)response = 0;
  return CESP_RESP_CLOSE_SOCKET;
}

int getSocketState(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[1];

  uint8_t connected = 0;

  if ((socketTypes[socket] == 0x00) && tcpClients[socket].connected()) {
    connected = 1;
  } else if ((socketTypes[socket] == 0x02) && tlsClients[socket].connected()) {
    connected = 1;
  }

  *(uint32_t*)response = connected;

  return CESP_RESP_GET_SOCKET_STATE;
}

int disconnectWiFi(const uint8_t command[], uint8_t response[])
{
  WiFi.disconnect();

  *(uint32_t*)response = 1;
  return CESP_RESP_DISCONNECT_WIFI;
}

int getIdxRSSI(const uint8_t command[], uint8_t response[])
{
  // RSSI
  int32_t rssi = WiFi.RSSI(command[4]);

  response[2] = 1; // number of parameters
  response[3] = sizeof(rssi); // parameter 1 length

  memcpy(&response[4], &rssi, sizeof(rssi));

  return 9;
}

int getIdxEnct(const uint8_t command[], uint8_t response[])
{
  uint8_t encryptionType = WiFi.encryptionType(command[4]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = encryptionType;

  return 6;
}

int reqHostByName(const uint8_t command[], uint8_t response[])
{
  uint32_t* resp32 = (uint32_t*)response;
  const char* host = (const char*)(command+1);
  uint32_t resolvedHostname = /*IPAddress(255, 255, 255, 255)*/0xffffffff;
  resp32[0] = WiFi.hostByName(host, resolvedHostname)?1:0;
  resp32[1] = resolvedHostname;
  return CESP_RESP_REQ_HOST_BY_NAME;
}

int getFwVersion(const uint8_t command[], uint8_t response[])
{
  memset(response, 0, CESP_RESP_FW_VERSION);
  memcpy(response, FIRMWARE_VERSION, sizeof(FIRMWARE_VERSION));
  return CESP_RESP_FW_VERSION;
}

int sendUDPdata(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (udps[socket].endPacket()) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int getRemoteData(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];
  
  /*IPAddress*/uint32_t ip = /*IPAddress(0, 0, 0, 0)*/0;
  uint16_t port = 0;

  if (socketTypes[socket] == 0x00) {
    ip = tcpClients[socket].remoteIP();
    port = tcpClients[socket].remotePort();
  } else if (socketTypes[socket] == 0x01) {
    ip = udps[socket].remoteIP();
    port = udps[socket].remotePort();
  } else if (socketTypes[socket] == 0x02) {
    ip = tlsClients[socket].remoteIP();
    port = tlsClients[socket].remotePort();
  }

  response[2] = 2; // number of parameters

  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &ip, sizeof(ip));

  response[8] = 2; // parameter 2 length
  response[9] = (port >> 8) & 0xff;
  response[10] = (port >> 0) & 0xff;

  return 12;
}

int getTime(const uint8_t command[], uint8_t response[])
{
  unsigned long now = WiFi.getTime();

  response[2] = 1; // number of parameters
  response[3] = sizeof(now); // parameter 1 length

  memcpy(&response[4], &now, sizeof(now));

  return 5 + sizeof(now);
}

int getIdxBSSID(const uint8_t command[], uint8_t response[])
{
  uint8_t bssid[6];

  WiFi.BSSID(command[4], bssid);

  response[2] = 1; // number of parameters
  response[3] = 6; // parameter 1 length
  memcpy(&response[4], bssid, sizeof(bssid));

  return 11;
}

int getIdxChannel(const uint8_t command[], uint8_t response[])
{
  uint8_t channel = WiFi.channel(command[4]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = channel;

  return 6;
}

int sendSocketData(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;
  uint16_t written = 0;

  socket = command[1];
  memcpy(&length, command+2, sizeof(length));
  const uint8_t* data = command+4;

  if ((socketTypes[socket] == 0x00) && tcpServers[socket]) {
    written = tcpServers[socket].write(data, length);
  } else if (socketTypes[socket] == 0x00) {
    written = tcpClients[socket].write(data, length);
  } else if (socketTypes[socket] == 0x02) {
    written = tlsClients[socket].write(data, length);
  }
  
  *(uint32_t*)response = written;
  return CESP_RESP_SEND_SOCKET_DATA;
}

int readSocketData(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;
  int read = 0;

  socket = command[1];
  memcpy(&length, &command[2], sizeof(length));
  uint8_t* out = response + 4;

  if(length > SPI_BUFFER_LEN-4)
    length = SPI_BUFFER_LEN-4;
  if(debug)
    printf("readSocketData socket=%i length=%i socketTypes=%i\n", (int)socket, (int)length, (int)socketTypes[socket]);

  if (socketTypes[socket] == 0x00) {
    read = tcpClients[socket].read(out, length);
  } else if (socketTypes[socket] == 0x01) {
    read = udps[socket].read(out, length);
  } else if (socketTypes[socket] == 0x02) {
    read = tlsClients[socket].read(out, length);
  }

  if (read < 0)
    read = 0;
  *(uint32_t*)response = read;
  return (4 + length);
}

int insertDataBuf(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;

  socket = command[5];
  memcpy(&length, &command[6], sizeof(length));
  length = ntohs(length);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (udps[socket].write(&command[8], length) != 0) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int ping(const uint8_t command[], uint8_t response[])
{
  uint32_t ip;
  uint8_t ttl;
  int16_t result;

  ttl = command[1];
  memcpy(&ip, &command[4], sizeof(ip));

  result = WiFi.ping(ip, ttl);

  response[0] = CESP_PING;
  response[1] = 0;
  memcpy(&response[2], &result, sizeof(result));

  return CESP_RESP_PING;
}

int getSocket(const uint8_t command[], uint8_t response[])
{
  uint8_t result = 255;

  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (socketTypes[i] == 255) {
      result = i;
      break;
    }
  }

  *(uint32_t*)response = result;
  return CESP_RESP_GET_SOCKET;
}

int setPinMode(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];
  uint8_t mode = command[6];

  pinMode(pin, mode);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setDigitalWrite(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];
  uint8_t value = command[6];

  digitalWrite(pin, value);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetIdentity(const uint8_t command[], uint8_t response[]) {
  char identity[32 + 1];

  memset(identity, 0x00, sizeof(identity));
  memcpy(identity, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)identity, strlen(identity));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetUsername(const uint8_t command[], uint8_t response[]) {
  char username[32 + 1];

  memset(username, 0x00, sizeof(username));
  memcpy(username, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetPassword(const uint8_t command[], uint8_t response[]) {
  char password[32 + 1];

  memset(password, 0x00, sizeof(password));
  memcpy(password, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetCACert(const uint8_t command[], uint8_t response[]) {
  // not yet implemented (need to decide if writing in the filesystem is better than loading every time)
  // keep in mind size limit for messages
  return 0;
}

int wpa2EntSetCertKey(const uint8_t command[], uint8_t response[]) {
  // not yet implemented (need to decide if writing in the filesystem is better than loading every time)
  // keep in mind size limit for messages
  return 0;
}

int wpa2EntEnable(const uint8_t command[], uint8_t response[]) {

  esp_wifi_sta_wpa2_ent_enable();

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setClientCert(const uint8_t command[], uint8_t response[]){
  ets_printf("*** Called setClientCert\n");

  memset(CERT_BUF, 0x00, sizeof(CERT_BUF));
  memcpy(CERT_BUF, &command[4], sizeof(CERT_BUF));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  setCert = 1;

  return 6;
}

int setCertKey(const uint8_t command[], uint8_t response[]){
  ets_printf("*** Called setCertKey\n");

  memset(PK_BUFF, 0x00, sizeof(PK_BUFF));
  memcpy(PK_BUFF, &command[4], sizeof(PK_BUFF));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  setPSK = 1;

  return 6;
}

int softReset(const uint8_t command[], uint8_t response[]){
  esp_restart();
  return 0;
}

typedef int (*CommandHandlerType)(const uint8_t command[], uint8_t response[]);

const CommandHandlerType commandHandlers[] =
{
  // 0x00 -> 0x0f
  invertBytes, setDebug, getFwVersion, getTemperature,
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  // 0x10 -> 0x1f
  //0123
  disconnectWiFi, setSsidAndPass, scanNetworks, scanNetworksResult,
  //4567
  setApPassPhrase, NULL, setHostname, setPowerMode,
  //89ab
  NULL, NULL, NULL, NULL,
  //cdef
  NULL, NULL, NULL, NULL,

  // 0x20 -> 0x2f
  getConnStatus, getIPaddr, getMACaddr, getCurrSSID,
  getCurrBSSID, getCurrRSSI, getCurrEnct, NULL,
  //89ab
  startServerTcp, NULL, NULL, availSocketData,
  //cdef
  NULL, startSocketClient, closeSocket, getSocketState,

  // 0x30 -> 0x3f
  NULL, NULL, getIdxRSSI, getIdxEnct,
  reqHostByName, NULL, NULL, getFwVersion,
  NULL, sendUDPdata, getRemoteData, getTime,
  getIdxBSSID, getIdxChannel, ping, getSocket,

  // 0x40 -> 0x4f
  setClientCert, setCertKey, NULL, NULL,
  sendSocketData, readSocketData, insertDataBuf, NULL,
  NULL, NULL, wpa2EntSetIdentity, wpa2EntSetUsername,
  wpa2EntSetPassword, wpa2EntSetCACert, wpa2EntSetCertKey, wpa2EntEnable,

  // 0x50 -> 0x5f
  setPinMode, setDigitalWrite, NULL, NULL,
  softReset,
};

#define NUM_COMMAND_HANDLERS (sizeof(commandHandlers) / sizeof(commandHandlers[0]))

CommandHandlerClass::CommandHandlerClass()
{
}

void CommandHandlerClass::begin()
{
  pinMode(0, OUTPUT);

  for (int i = 0; i < MAX_SOCKETS; i++) {
    socketTypes[i] = 255;
  }
}

int CommandHandlerClass::handle(const uint8_t command[], uint8_t response[])
{
  int responseLength = 0;

  if (command[0] < NUM_COMMAND_HANDLERS) {
    CommandHandlerType commandHandlerType = commandHandlers[command[0]];

    if (commandHandlerType) {
      responseLength = commandHandlerType(command, response);
    }
  }

  //round up 4
  responseLength = (responseLength+3)&~3;

  return responseLength;
}

CommandHandlerClass CommandHandler;
