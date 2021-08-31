#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vector>

typedef enum
{
    SET_NET_CMD                 = (0x10),
    SET_PASSPHRASE_CMD          = (0x11),
    GET_CONN_STATUS_CMD         = (0x20),
    GET_IPADDR_CMD              = (0x21),
    GET_MACADDR_CMD             = (0x22),
    GET_CURR_SSID_CMD           = (0x23),
    GET_CURR_RSSI_CMD           = (0x25),
    GET_CURR_ENCT_CMD           = (0x26),
    SCAN_NETWORKS               = (0x27),
    GET_SOCKET_CMD              = (0x3F),
    GET_STATE_TCP_CMD           = (0x29),
    DATA_SENT_TCP_CMD           = (0x2A),
    AVAIL_DATA_TCP_CMD          = (0x2B),
    GET_DATA_TCP_CMD            = (0x2C),
    START_CLIENT_TCP_CMD        = (0x2D),
    STOP_CLIENT_TCP_CMD         = (0x2E),
    GET_CLIENT_STATE_TCP_CMD    = (0x2F),
    DISCONNECT_CMD              = (0x30),
    GET_IDX_RSSI_CMD            = (0x32),
    GET_IDX_ENCT_CMD            = (0x33),
    REQ_HOST_BY_NAME_CMD        = (0x34),
    GET_HOST_BY_NAME_CMD        = (0x35),
    START_SCAN_NETWORKS         = (0x36),
    GET_FW_VERSION_CMD          = (0x37),
    PING_CMD                    = (0x3E),
    SEND_DATA_TCP_CMD           = (0x44),
    GET_DATABUF_TCP_CMD         = (0x45),
    GET_ADC_VAL_CMD             = (0x53),
    SAMPLE_ADC_CMD              = (0x54),
    SOFT_RESET_CMD              = (0x55),
    START_CMD                   = (0xE0),
    END_CMD                     = (0xEE),
    ERR_CMD                     = (0xEF)
}esp32_cmd_enum_t;

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

int8_t esp32_spi_connect_AP(const uint8_t *ssid, const uint8_t *password, uint8_t retry_times);
uint16_t esp32_spi_ping(const uint8_t *dest, uint8_t dest_type, uint8_t ttl);
void esp32_spi_pretty_ip(uint8_t *ip, uint8_t *str_ip);
int8_t esp32_spi_get_host_by_name(const uint8_t *hostname, uint8_t *ip);

//use esp32_spi_connect_AP
bool esp32_spi_wifi_set_ssid_and_pass(const char *ssid, const char *passphrase);