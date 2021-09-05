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

#include <time.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_netif.h>

#include <lwip/apps/sntp.h>

#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/sockets.h>
#include <lwip/ip_addr.h>
#include <lwip/inet_chksum.h>

#include "WiFi.h"

static const char *TAG="wifi";

// Старые значения _eventGroup
// BIT0 - SYSTEM_EVENT_STA_START
// BIT1 - SYSTEM_EVENT_AP_START
// BIT2 - SYSTEM_EVENT_SCAN_DONE

//Для _wifi_event_group
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
const int SCAN_DONE_BIT = BIT2;


WiFiClass::WiFiClass() :
  _initialized(false),
  _status(WL_IDLE_STATUS),
  _interface(WIFI_IF_STA)
{
  _wifi_event_group = xEventGroupCreate();
  memset(&_apRecord, 0x00, sizeof(_apRecord));
  memset(&_ipInfo, 0x00, sizeof(_ipInfo));
  memset(&_dnsServers, 0x00, sizeof(_dnsServers));
}

uint8_t WiFiClass::status()
{
  return _status;
}

int WiFiClass::hostByName(const char* hostname, /*IPAddress*/uint32_t& result)
{
  result = 0xffffffff;

  struct hostent* hostEntry = lwip_gethostbyname(hostname);

  if (hostEntry == NULL) {
    return 0;
  }
  
  memcpy(&result, hostEntry->h_addr_list[0], sizeof(result));

  return 1;
}

int WiFiClass::ping(/*IPAddress*/uint32_t host, uint8_t ttl)
{
  uint32_t timeout = 5000;

  int s = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);

  struct timeval timeoutVal;
  timeoutVal.tv_sec = (timeout / 1000);
  timeoutVal.tv_usec = (timeout % 1000) * 1000;

  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal));
  setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

  struct __attribute__((__packed__)) {
    struct icmp_echo_hdr header;
    uint8_t data[32];
  } request;

  ICMPH_TYPE_SET(&request.header, ICMP_ECHO);
  ICMPH_CODE_SET(&request.header, 0);
  request.header.chksum = 0;
  request.header.id = 0xAFAF;
  request.header.seqno = esp_random()%0xffff;

  for (size_t i = 0; i < sizeof(request.data); i++) {
    request.data[i] = i;
  }

  request.header.chksum = inet_chksum(&request, sizeof(request));

  ip_addr_t addr;
  addr.type = IPADDR_TYPE_V4;
  addr.u_addr.ip4.addr = host;
  //  IP_ADDR4(&addr, ip[0], ip[1], ip[2], ip[3]);

  struct sockaddr_in to;
  struct sockaddr_in from;

  to.sin_len = sizeof(to);
  to.sin_family = AF_INET;
  inet_addr_from_ip4addr(&to.sin_addr, ip_2_ip4(&addr));

  sendto(s, &request, sizeof(request), 0, (struct sockaddr*)&to, sizeof(to));
  unsigned long sendTime = millis();
  unsigned long recvTime = 0;

  do {
    socklen_t fromlen = sizeof(from);

    struct __attribute__((__packed__)) {
      struct ip_hdr ipHeader;
      struct icmp_echo_hdr header;
    } response;

    int rxSize = recvfrom(s, &response, sizeof(response), 0, (struct sockaddr*)&from, (socklen_t*)&fromlen);
    if (rxSize == -1) {
      // time out
      break;
    }

    if (rxSize < sizeof(response)) {
      // too short
      continue;
    }

    if (from.sin_family != AF_INET) {
      // not IPv4
      continue;
    }

    if ((response.header.id == request.header.id) && (response.header.seqno == request.header.seqno)) {
      recvTime = millis();
    }
  } while (recvTime == 0);

  close(s);

  if (recvTime == 0) {
    return -1;
  } else {
    return (recvTime - sendTime);
  }
}

uint8_t WiFiClass::begin(const char* ssid, const char* passw)
{
  wifi_config_t wifiConfig = {};
  strlcpy((char*)wifiConfig.sta.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  if(passw)
    strlcpy((char*)wifiConfig.sta.password, passw, sizeof(wifiConfig.sta.password));

  wifiConfig.sta.scan_method = WIFI_FAST_SCAN;
  _interface = WIFI_IF_STA;

  int bits = xEventGroupWaitBits(_wifi_event_group, CONNECTED_BIT, 0, 1, 0);
  if (bits & CONNECTED_BIT) {
      xEventGroupClearBits(_wifi_event_group, CONNECTED_BIT);
      ESP_ERROR_CHECK( esp_wifi_disconnect() );
      xEventGroupWaitBits(_wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_RATE_MS);
  }

  esp_wifi_set_mode(WIFI_MODE_STA);
  _status = WL_CONNECTING;
  if (esp_wifi_set_config(WIFI_IF_STA, &wifiConfig) != ESP_OK) {
    _status = WL_DISCONNECTED;
    return _status;
  }

  esp_wifi_connect();

  return _status;
}

uint8_t WiFiClass::beginAP(const char *ssid, const char* passw, uint8_t channel)
{
  //!!!!untested code!!!!!
  wifi_config_t wifiConfig = {};
  bool has_passw = passw && passw[0];

  strlcpy((char*)wifiConfig.ap.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  if(has_passw)
    strlcpy((char*)wifiConfig.ap.password, passw, sizeof(wifiConfig.sta.password));
  wifiConfig.ap.channel = channel;
  wifiConfig.ap.authmode = has_passw?WIFI_AUTH_WPA_WPA2_PSK:WIFI_AUTH_OPEN;//or  WIFI_AUTH_WEP
  wifiConfig.ap.max_connection = 4;

  _status = WL_CONNECTING;

  _interface = WIFI_IF_AP;

  esp_wifi_set_mode(WIFI_MODE_AP);

  if (esp_wifi_set_config(WIFI_IF_AP, &wifiConfig) != ESP_OK) {
    _status = WL_DISCONNECTED;
  } else {
    xEventGroupWaitBits(_wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
  }

  return _status;
}

bool WiFiClass::hostname(const char* name)
{
  return esp_netif_set_hostname(_interface == WIFI_IF_AP?netif_ap:netif_sta, name)==ESP_OK;
}

void WiFiClass::disconnect()
{
  esp_wifi_disconnect();
}

void WiFiClass::end()
{
  esp_wifi_stop();
}

uint8_t* WiFiClass::macAddress(uint8_t* mac)
{
  uint8_t macTemp[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  esp_wifi_get_mac(_interface, macTemp);

  mac[0] = macTemp[5];
  mac[1] = macTemp[4];
  mac[2] = macTemp[3];
  mac[3] = macTemp[2];
  mac[4] = macTemp[1];
  mac[5] = macTemp[0];

  return mac;
}

uint32_t WiFiClass::localIP()
{
  return _ipInfo.ip.addr;
}

uint32_t WiFiClass::subnetMask()
{
  return _ipInfo.netmask.addr;
}

uint32_t WiFiClass::gatewayIP()
{
  return _ipInfo.gw.addr;
}

char* WiFiClass::SSID()
{
  return (char*)_apRecord.ssid;
}

int32_t WiFiClass::RSSI()
{
  if (_interface == WIFI_IF_AP) {
    return 0;
  } else {
    esp_wifi_sta_get_ap_info(&_apRecord);

    return _apRecord.rssi;
  }
}

uint8_t WiFiClass::encryptionType()
{
  uint8_t encryptionType = _apRecord.authmode;

  if (encryptionType == WIFI_AUTH_OPEN) {
    encryptionType = 7;
  } else if (encryptionType == WIFI_AUTH_WEP) {
    encryptionType = 5;
  } else if (encryptionType == WIFI_AUTH_WPA_PSK) {
    encryptionType = 2;
  } else if (encryptionType == WIFI_AUTH_WPA2_PSK || encryptionType == WIFI_AUTH_WPA_WPA2_PSK) {
    encryptionType = 4;
  } else {
    // unknown?
    encryptionType = 255;
  }

  return encryptionType;
}

uint8_t* WiFiClass::BSSID(uint8_t* bssid)
{
  if (_interface == WIFI_IF_AP) {
    return macAddress(bssid);
  } else {
    bssid[0] = _apRecord.bssid[5];
    bssid[1] = _apRecord.bssid[4];
    bssid[2] = _apRecord.bssid[3];
    bssid[3] = _apRecord.bssid[2];
    bssid[4] = _apRecord.bssid[1];
    bssid[5] = _apRecord.bssid[0];

    return bssid;
  }
}

void WiFiClass::scanNetworks()
{
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

  wifi_scan_config_t config;

  config.ssid = 0;
  config.bssid = 0;
  config.channel = 0;
  config.show_hidden = false;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min = 100;
  config.scan_time.active.max = 300;

  xEventGroupClearBits(_wifi_event_group, SCAN_DONE_BIT);

  if (esp_wifi_scan_start(&config, false) != ESP_OK)
    return;

  xEventGroupWaitBits(_wifi_event_group, SCAN_DONE_BIT, false, true, portMAX_DELAY);

  uint16_t numNetworks;
  esp_wifi_scan_get_ap_num(&numNetworks);

  if (numNetworks > MAX_SCAN_RESULTS)
    numNetworks = MAX_SCAN_RESULTS;

  esp_wifi_scan_get_ap_records(&numNetworks, _scanResults);
  _scanResultsCount = numNetworks;
}

char* WiFiClass::SSID(uint8_t pos)
{
  return (char*)_scanResults[pos].ssid;
}

int8_t WiFiClass::RSSI(uint8_t pos)
{
  return _scanResults[pos].rssi;
}

uint8_t WiFiClass::encryptionType(uint8_t pos)
{
  uint8_t encryptionType = _scanResults[pos].authmode;

  if (encryptionType == WIFI_AUTH_OPEN) {
    encryptionType = 7;
  } else if (encryptionType == WIFI_AUTH_WEP) {
    encryptionType = 5;
  } else if (encryptionType == WIFI_AUTH_WPA_PSK) {
    encryptionType = 2;
  } else if (encryptionType == WIFI_AUTH_WPA2_PSK || encryptionType == WIFI_AUTH_WPA_WPA2_PSK) {
    encryptionType = 4;
  } else {
    // unknown?
    encryptionType = 255;
  }

  return encryptionType;
}

uint8_t* WiFiClass::BSSID(uint8_t pos, uint8_t* bssid)
{
  const uint8_t* tempBssid = _scanResults[pos].bssid;

  bssid[0] = tempBssid[5];
  bssid[1] = tempBssid[4];
  bssid[2] = tempBssid[3];
  bssid[3] = tempBssid[2];
  bssid[4] = tempBssid[1];
  bssid[5] = tempBssid[0];

  return bssid;
}

uint8_t WiFiClass::channel(uint8_t pos)
{
  return _scanResults[pos].primary;
}

unsigned long WiFiClass::getTime()
{
  time_t now;

  time(&now);

  if (now < 946684800) {
    return 0;
  }

  return now;
}

void WiFiClass::lowPowerMode()
{
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

void WiFiClass::noLowPowerMode()
{
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void WiFiClass::updateIpInfo()
{
  int bits = xEventGroupWaitBits(_wifi_event_group, CONNECTED_BIT, 0, 1, 0);
  esp_netif_t * netif = netif_ap;
  wifi_mode_t mode;

  esp_wifi_get_mode(&mode);
  if (WIFI_MODE_STA == mode) {
      bits = xEventGroupWaitBits(_wifi_event_group, CONNECTED_BIT, 0, 1, 0);
      if (bits & CONNECTED_BIT) {
          netif = netif_sta;
      } else {
          ESP_LOGE(TAG, "sta has no IP");
          return;
      }
    }

    esp_netif_get_ip_info(netif, &_ipInfo);
}

void WiFiClass::init()
{
  esp_log_level_set(TAG, ESP_LOG_WARN);
  ESP_ERROR_CHECK(esp_netif_init());
  _wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK( esp_event_loop_create_default() );
  netif_ap = esp_netif_create_default_wifi_ap();
  assert(netif_ap);
  netif_sta = esp_netif_create_default_wifi_sta();
  assert(netif_sta);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      WIFI_EVENT_SCAN_DONE,
                                                      &scan_done_handler,
                                                      NULL,
                                                      NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      WIFI_EVENT_STA_DISCONNECTED,
                                                      &disconnect_handler,
                                                      NULL,
                                                      NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &got_ip_handler,
                                                      NULL,
                                                      NULL));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  /*
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char*)"0.pool.ntp.org");
  sntp_setservername(1, (char*)"1.pool.ntp.org");
  sntp_setservername(2, (char*)"2.pool.ntp.org");
  sntp_init();
  */

  _initialized = true;
  _status = WL_IDLE_STATUS;
}

void WiFiClass::scan_done_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
  xEventGroupSetBits(WiFi._wifi_event_group, SCAN_DONE_BIT);
/*  
  uint16_t sta_number = 0;
  uint8_t i;
  wifi_ap_record_t *ap_list_buffer;

  esp_wifi_scan_get_ap_num(&sta_number);
  if (sta_number==0) {
      ESP_LOGE(TAG, "No AP found");
      return;
  }

  if(sta_number > MAX_SCAN_RESULTS)
    sta_number = MAX_SCAN_RESULTS;

  if (esp_wifi_scan_get_ap_records(&sta_number,(wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
      for(i=0; i<sta_number; i++) {
          ESP_LOGI(TAG, "[%s][rssi=%d]", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
      }
  }
*/  
}

void WiFiClass::got_ip_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
  xEventGroupClearBits(WiFi._wifi_event_group, DISCONNECTED_BIT);
  xEventGroupSetBits(WiFi._wifi_event_group, CONNECTED_BIT);
  WiFi._status = WL_CONNECTED;
}

void WiFiClass::disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  ESP_LOGI(TAG, "sta disconnect");
  xEventGroupClearBits(WiFi._wifi_event_group, CONNECTED_BIT);
  xEventGroupSetBits(WiFi._wifi_event_group, DISCONNECTED_BIT);
  WiFi._status = WL_DISCONNECTED;
}

WiFiClass WiFi;
