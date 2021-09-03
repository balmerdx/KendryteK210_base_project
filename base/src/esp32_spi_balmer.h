#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vector>

typedef enum {
    CMD_FLAG                    = (0),
    REPLY_FLAG                  = (1<<7)
}esp32_flag_t;

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

esp32_wlan_enum_t esp32_spi_status();

bool esp32_test_invert(uint8_t* data, size_t len);

const char* esp32_spi_firmware_version();
float esp32_spi_get_temperature();

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

// Ping a destination IP address or hostname, with a max time-to-live
//      (ttl). Returns a millisecond timing value
uint16_t esp32_spi_ping_ip(uint8_t ip[4], uint8_t ttl);
uint16_t esp32_spi_ping(const char *dest, uint8_t ttl);

void esp32_spi_pretty_ip(const uint8_t ip[4], char *str_ip);
bool esp32_spi_get_host_by_name(const char *hostname, uint8_t ip[4]);

//use esp32_spi_connect_AP
bool esp32_spi_wifi_set_ssid_and_pass(const char *ssid, const char *passphrase);
void esp32_spi_reset();


uint8_t esp32_spi_get_socket();
//Открываем соединение, но не ожидаем, что открытие завершится.
//Более высокоуровневая функция - connect_server_port_tcp
bool esp32_spi_socket_open_ip(uint8_t sock_num, const uint8_t ip[4],
                             uint16_t port, esp32_socket_mode_enum_t conn_mode);
bool esp32_spi_socket_open(uint8_t sock_num, const char* hostname,
                             uint16_t port, esp32_socket_mode_enum_t conn_mode);

int8_t esp32_spi_socket_connect(uint8_t socket_num, uint8_t *dest, uint8_t dest_type, uint16_t port, esp32_socket_mode_enum_t conn_mod);

bool esp32_spi_socket_connected(uint8_t socket_num);
//Пишем данные в сокет.
//return - количество записанных данных.
uint16_t esp32_spi_socket_write(uint8_t socket_num, const void* buffer, uint16_t len);
//Количество данных, доступных для чтения в этом сокете
uint16_t esp32_spi_socket_available(uint8_t socket_num);
//Читаем данные из сокета
//return - количество прочитанных данных.
uint16_t esp32_spi_socket_read(uint8_t socket_num, void* buff, uint16_t size);
int8_t esp32_spi_socket_close(uint8_t socket_num);

const char* wlan_enum_to_str(esp32_wlan_enum_t x);

//return socket
uint8_t connect_server_port_tcp(const char *host, uint16_t port);
