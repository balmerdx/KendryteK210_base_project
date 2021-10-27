#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h> //__packed
#include <vector>

typedef enum {
    CMD_FLAG                    = (0),
    REPLY_FLAG                  = (1<<7)
}esp32_flag_t;

typedef enum
{
    WL_IDLE_STATUS = 0,
    WL_CONNECTING,
    WL_CONNECTED,
    WL_DISCONNECTED,
}esp32_wlan_enum_t;

typedef enum
{
    TCP_MODE                    = (0),
    UDP_MODE                    = (1),
    TLS_MODE                    = (2)
}esp32_socket_mode_enum_t;

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

enum class esp32_socket : uint8_t
{
    bad = 0xFF
};

inline esp32_socket esp32_spi_bad_socket() { return esp32_socket::bad; }

//Максимальное количество данных, которое можно передавать в esp32_spi_socket_write
uint32_t esp32_spi_max_write_size();

size_t esp32_round_up4(size_t s);

void dump_buffer(const char* label, const uint8_t data[], int length);

void esp32_spi_init();

esp32_wlan_enum_t esp32_spi_status();

//Возвращает количество корректно инвертированных байтов
//-1 - не удалось получить ответ
int esp32_test_invert(uint8_t* data, size_t len);
void esp32_set_debug(bool enable);

const char* esp32_spi_firmware_version();
float esp32_spi_get_temperature();

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


//Открываем соединение, но не ожидаем, что открытие завершится.
//Более высокоуровневая функция - connect_server_port_tcp
//return socket
esp32_socket esp32_spi_socket_open_ip(const uint8_t ip[4],
                             uint16_t port, esp32_socket_mode_enum_t conn_mode);
esp32_socket esp32_spi_socket_open(const char* hostname,
                             uint16_t port, esp32_socket_mode_enum_t conn_mode);

bool esp32_spi_socket_connected(esp32_socket socket_num);
//Пишем данные в сокет.
//return - количество записанных данных.
//Даже, если *is_client_alive == false нужно вызывать esp32_spi_socket_close
uint16_t esp32_spi_socket_write(esp32_socket socket_num, const void* buffer, uint16_t len, bool* is_client_alive = nullptr);
//Количество данных, доступных для чтения в этом сокете
uint16_t esp32_spi_socket_available(esp32_socket socket_num);
//Читаем данные из сокета
//return - количество прочитанных данных.
uint16_t esp32_spi_socket_read(esp32_socket socket_num, void* buff, uint16_t size, bool* is_client_alive = nullptr);
bool esp32_spi_socket_close(esp32_socket socket_num);

const char* wlan_enum_to_str(esp32_wlan_enum_t x);

//return socket
esp32_socket connect_server_port_tcp(const char *host, uint16_t port);

bool esp32_spi_server_create(uint16_t port);
void esp32_spi_server_stop();
//return socket
esp32_socket esp32_spi_server_accept(bool* is_server_alive = nullptr);