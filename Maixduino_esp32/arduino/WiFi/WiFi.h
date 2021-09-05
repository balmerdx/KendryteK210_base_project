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

#ifndef WIFI_H
#define WIFI_H

#include <esp_event.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <lwip/netif.h>

#include <Arduino.h>

enum ESP32_WL_STATUS
{
  WL_IDLE_STATUS = 0,
  WL_CONNECTING,
  WL_CONNECTED,
  WL_DISCONNECTED,
};

#define MAX_SCAN_RESULTS 10

class WiFiClass
{
public:
  WiFiClass();

  void init();
  uint8_t begin(const char* ssid, const char* passw);
  uint8_t beginAP(const char *ssid, const char* passw, uint8_t channel);

  bool hostname(const char* name);

  void disconnect();
  void end();

  uint8_t* macAddress(uint8_t* mac);

  void updateIpInfo();
  uint32_t localIP();
  uint32_t subnetMask();
  uint32_t gatewayIP();
  char* SSID();
  int32_t RSSI();
  uint8_t encryptionType();
  uint8_t* BSSID(uint8_t* bssid);
  //results in scanResultsCount, SSID(i), RSSI(i) etc...
  void scanNetworks();
  uint8_t scanResultsCount() { return _scanResultsCount; }
  char* SSID(uint8_t pos);
  int8_t RSSI(uint8_t pos);
  uint8_t encryptionType(uint8_t pos);
  uint8_t* BSSID(uint8_t pos, uint8_t* bssid);
  uint8_t channel(uint8_t pos);

  uint8_t status();

  int hostByName(const char* hostname, /*IPAddress*/uint32_t& result);

  int ping(/*IPAddress*/uint32_t host, uint8_t ttl);

  time_t getTime();

  void lowPowerMode();
  void noLowPowerMode();

private:
  static void scan_done_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
  static void got_ip_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data);
  static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

private:
  bool _initialized;
  volatile ESP32_WL_STATUS _status;
  EventGroupHandle_t _wifi_event_group;
  wifi_interface_t _interface;

  uint16_t _scanResultsCount = 0;
  wifi_ap_record_t _scanResults[MAX_SCAN_RESULTS];
  wifi_ap_record_t _apRecord;
  esp_netif_ip_info_t _ipInfo;
  uint32_t _dnsServers[2];

  netif_input_fn _staNetifInput;
  netif_input_fn _apNetifInput;

  esp_netif_t *netif_ap = NULL;
  esp_netif_t *netif_sta = NULL;
};

extern WiFiClass WiFi;

#endif // WIFI_H
