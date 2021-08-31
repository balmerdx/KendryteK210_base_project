#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vector>

typedef enum {
    CMD_FLAG                    = (0),
    REPLY_FLAG                  = (1<<7)
}esp32_flag_t;

typedef enum{
    SOCKET_CLOSED               = (0),
    SOCKET_LISTEN               = (1),
    SOCKET_SYN_SENT             = (2),
    SOCKET_SYN_RCVD             = (3),
    SOCKET_ESTABLISHED          = (4),
    SOCKET_FIN_WAIT_1           = (5),
    SOCKET_FIN_WAIT_2           = (6),
    SOCKET_CLOSE_WAIT           = (7),
    SOCKET_CLOSING              = (8),
    SOCKET_LAST_ACK             = (9),
    SOCKET_TIME_WAIT            = (10)
}esp32_socket_enum_t;

typedef enum
{
    WL_IDLE_STATUS              = (0),
    WL_NO_SSID_AVAIL            = (1),
    WL_SCAN_COMPLETED           = (2),
    WL_CONNECTED                = (3),
    WL_CONNECT_FAILED           = (4),
    WL_CONNECTION_LOST          = (5),
    WL_DISCONNECTED             = (6),
    WL_AP_LISTENING             = (7),
    WL_AP_CONNECTED             = (8),
    WL_AP_FAILED                = (9),
    //
    WL_NO_MODULE                = (0xFF)
}esp32_wlan_enum_t;

typedef enum
{
    TCP_MODE                    = (0),
    UDP_MODE                    = (1),
    TLS_MODE                    = (2)
}esp32_socket_mode_enum_t;


size_t esp32_round_up4(size_t s);

void esp32_spi_init();

int8_t esp32_spi_status();

bool esp32_test_invert(uint8_t* data, size_t len);

const char* esp32_spi_firmware_version();

typedef struct __packed
{
    int8_t rssi;
    uint8_t encr;
    uint8_t ssid[33];
} esp32_spi_ap_t;

typedef struct __packed
{
    uint8_t num;
    esp32_spi_ap_t aps[0];
} esp32_spi_aps_list_t;


const esp32_spi_aps_list_t* esp32_spi_scan_networks();

bool esp32_spi_connect_AP(const char *ssid, const char *password, uint8_t retry_times);
uint16_t esp32_spi_ping(const char *dest, uint8_t dest_type, uint8_t ttl);
void esp32_spi_pretty_ip(const uint8_t *ip, char *str_ip);
int8_t esp32_spi_get_host_by_name(const char *hostname, uint8_t *ip);

//use esp32_spi_connect_AP
bool esp32_spi_wifi_set_ssid_and_pass(const char *ssid, const char *passphrase);
void esp32_spi_reset();
