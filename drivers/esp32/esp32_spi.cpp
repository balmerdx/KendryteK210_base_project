
#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <sleep.h>
#include <spi.h>
#include <gpiohs.h>

#include "esp32_spi.h"
#include "spi2_slave.h"

#include "../../Maixduino_esp32/main/Esp32CommandList.h"

//Используем SPI2 для передачи данных ESP32->K210
//А SPI0 тогда используется только для передачи K210->ESP32
//Так как передачи однонаправленные, можно добиться сильно большей тактовой частоты
#define USE_SPI_SLAVE

static const bool debug = false;

#define ESP32_RST       FUNC_GPIOHS11
#define ESP32_RDY       FUNC_GPIOHS12
#define ESP32_CS        FUNC_GPIOHS10
#define ESP32_MOSI      FUNC_GPIOHS13
#define ESP32_MISO      FUNC_GPIOHS14
#define ESP32_SCLK      FUNC_GPIOHS15

#define ESP32_RST_PIN   8
#define ESP32_RDY_PIN   9

#define ESP32_CS_PIN    25
#define ESP32_SCLK_PIN  27

#ifdef USE_SPI_SLAVE
#define ESP32_MOSI_PIN  26
#else
#define ESP32_MOSI_PIN  28
#define ESP32_MISO_PIN  26
#endif

#ifdef USE_SPI_SLAVE
void spi_slave_init()
{
    int SPI_SLAVE_CS_PIN = 15;
    int SPI_SLAVE_CLK_PIN = 13;
    int SPI_SLAVE_MOSI_PIN = 14;
    fpioa_set_function(SPI_SLAVE_CS_PIN, FUNC_SPI_SLAVE_SS);
    fpioa_set_function(SPI_SLAVE_CLK_PIN, FUNC_SPI_SLAVE_SCLK);
    fpioa_set_function(SPI_SLAVE_MOSI_PIN, FUNC_SPI_SLAVE_D0);
    spi2_slave_config();
}
#endif

//В последних 4-х байтах 4094 какойто мусор при передаче. Уменьшим
//#define BUFFER_SIZE 4094
#define BUFFER_SIZE 4090

static uint8_t cs_num, rst_num, rdy_num;

static uint8_t rx_buffer[BUFFER_SIZE];
static uint8_t tx_buffer[BUFFER_SIZE];

extern "C"
{
void spi_send_data_normal(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8_t *tx_buff, size_t tx_len);
}

void dump_buffer(const char* label, const uint8_t data[], int length) {
  printf("%s: ", label);

  int max_print_length = 32;
  int print_length = length;
  if(length > max_print_length)
    print_length = max_print_length;

  for (int i = 0; i < print_length; i++)
  {
    printf("%02x", data[i]);
    if(i%4==3)
      printf(" ");
  }

  if(length > max_print_length)
    printf(" total len=%i", length);

  printf("\r\n");
}


void esp32_spi_init()
{
    fpioa_set_function(ESP32_CS_PIN, ESP32_CS);
    fpioa_set_function(ESP32_RST_PIN, ESP32_RST);
    fpioa_set_function(ESP32_RDY_PIN, ESP32_RDY);

    cs_num = ESP32_CS - FUNC_GPIOHS0;
    rdy_num = ESP32_RDY - FUNC_GPIOHS0;
    rst_num = ESP32_RST - FUNC_GPIOHS0;

    gpiohs_set_drive_mode(rdy_num, GPIO_DM_INPUT); //ready

#ifdef USE_SPI_SLAVE
    spi_slave_init();

    fpioa_set_function(ESP32_SCLK_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(ESP32_MOSI_PIN, FUNC_SPI0_D0);
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 32, 1);
    //spi_set_clk_rate(SPI_DEVICE_0, 35*1000000); //Для пинов на плате максимальная скорость
    //Немного уменьшили, почему изсенение частоты (как ниже) ухудшает тайминги
    //sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL); sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    spi_set_clk_rate(SPI_DEVICE_0, 30*1000000);
#else
    fpioa_set_function(ESP32_SCLK_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(ESP32_MOSI_PIN, FUNC_SPI0_D0);
    fpioa_set_function(ESP32_MISO_PIN, FUNC_SPI0_D1);
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 32, 1);
    spi_set_clk_rate(SPI_DEVICE_0, 9000000);
#endif    

    gpiohs_set_drive_mode(cs_num, GPIO_DM_OUTPUT);
    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
}

static bool esp32_spi_wait_for_ready(gpio_pin_value_t value, uint64_t timeout_us)
{
    uint64_t tm = sysctl_get_time_us();
    while ((sysctl_get_time_us() - tm) < timeout_us)
    {
        if (gpiohs_get_pin(rdy_num) == value)
            return true;
        usleep(30);
    }
    return false;
}

size_t esp32_round_up4(size_t s)
{
    return (s + 3) & ~(size_t)3;
}

//esp32_transfer - use for test only!
//max tx_len, rx_len = 4096-4
//tx_len, rx_len - принудительно делаются кратными 4-м байтам.
//Предполагается, что размера буфера для этого будет достаточно.
bool esp32_transfer(const uint8_t* tx_buffer, size_t tx_len, uint8_t* rx_buffer, size_t rx_len)
{
    tx_len = esp32_round_up4(tx_len);
    rx_len = esp32_round_up4(rx_len);

    int8_t ready = esp32_spi_wait_for_ready(GPIO_PV_LOW, 10 * 1000000);
    if(ready<0)
    {
        if(debug) printf("esp32_spi_wait_for_ready(0) fail\n");
        return false;
    }

    gpiohs_set_pin(cs_num, GPIO_PV_LOW);
    ready = esp32_spi_wait_for_ready(GPIO_PV_HIGH, 1000000);
    if(ready<0)
    {
        if(debug) printf("esp32_spi_wait_for_ready(1) fail\n");
        gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
        return false;
    }

    spi_send_data_normal(SPI_DEVICE_0, SPI_CHIP_SELECT_0, tx_buffer, tx_len);

    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);

    if(rx_len==0)
        return true;

    //get response
    const uint64_t response_timeout_us = 10 * 1000000;
#ifdef USE_SPI_SLAVE
    if(!spi2_slave_receive4(rx_buffer, rx_len, response_timeout_us))
    {
        if(debug) printf("response spi2_slave_receive4 fail\n");
        return false;
    }
#else
    ready = esp32_spi_wait_for_ready(GPIO_PV_LOW, response_timeout_us);
    if(ready<0)
    {
        if(debug) printf("response esp32_spi_wait_for_ready(0) fail\n");
        return false;
    }

    gpiohs_set_pin(cs_num, GPIO_PV_LOW);
    ready = esp32_spi_wait_for_ready(GPIO_PV_HIGH, 1000000);
    if(ready<0)
    {
        if(debug) printf("response esp32_spi_wait_for_ready(1) fail\n");
        gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
        return false;
    }

    spi_receive_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_0, NULL, 0, rx_buffer, rx_len);
    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
#endif    

    if(debug)
        dump_buffer("receive ", rx_buffer, rx_len);

    return true;
}

//Укороченный вариант. Посылает команду размером 4 байта.
//Предполагается что первый байт - это команда, остальные три байта могут быть параметрами.
//Принимает всегда в rx_buffer
bool esp32_transfer_no_param(uint32_t command, size_t rx_len)
{
    *(uint32_t*)tx_buffer = command;
    return esp32_transfer(tx_buffer, 4, rx_buffer, rx_len);
}

int esp32_test_invert(uint8_t* data, size_t len)
{
    if(len&3)
        return false;
    *(uint32_t*)tx_buffer = (len<<16)|CESP_INVERT_BYTES;
    memcpy(tx_buffer+4, data, len);
    if(!esp32_transfer(tx_buffer, len+4, rx_buffer, len))
        return -1;

    for(size_t i=0; i<len; i++)
    if((data[i]^0xFF)!=rx_buffer[i])
    {
        return (int)i;
    }
    return (int)len;
}

void esp32_set_debug(bool enable)
{
    esp32_transfer_no_param(CESP_SET_DEBUG|(enable?0x100:0), CESP_RESP_SET_DEBUG);
}

const char* esp32_spi_firmware_version()
{
    esp32_transfer_no_param(CESP_GET_FW_VERSION, CESP_RESP_FW_VERSION);
    rx_buffer[CESP_RESP_FW_VERSION] = 0;
    return (const char*)rx_buffer;
}

float esp32_spi_get_temperature()
{
    esp32_transfer_no_param(CESP_GET_TEMPERATURE, CESP_RESP_FW_VERSION);
    return *(float*)rx_buffer;
}

const esp32_spi_aps_list_t* esp32_spi_scan_networks()
{
    std::vector<esp32_spi_ap_t> out;

    if(!esp32_transfer_no_param(CESP_SCAN_NETWORKS, CESP_RESP_SCAN_NETWORKS))
        return NULL;
    size_t num = rx_buffer[1];
    rx_buffer[0] = 0; //see esp32_spi_aps_list_t
    if(num)
    {
        size_t rx_len = sizeof(esp32_spi_ap_t)*num + sizeof(esp32_spi_aps_list_t);
        if(!esp32_transfer_no_param(CESP_SCAN_NETWORKS_RESULT, rx_len))
        {
            return NULL;
        }
    }

    return (const esp32_spi_aps_list_t*)rx_buffer;
}

esp32_wlan_enum_t esp32_spi_status()
{
    if(!esp32_transfer_no_param(CESP_GET_CONN_STATUS, CESP_RESP_GET_CONN_STATUS))
        return WL_IDLE_STATUS;
    return (esp32_wlan_enum_t)rx_buffer[1];
}

void strncpy_s(char * dest, const char * src, size_t destsz)
{
    if(dest==NULL || destsz==0)
        return;
    if(src==NULL)
        *dest = 0;
    for(size_t i=0; i<destsz;i++)
    {
        dest[i] = src[i];
        if(src[i]==0)
            return;
    }

    dest[destsz-1] = 0;
}

bool esp32_spi_wifi_set_ssid_and_pass(const char *ssid, const char *passphrase)
{
    size_t len = 0;
    char* cbuf = (char*)tx_buffer;
    tx_buffer[len++] = CESP_SET_SSID_AND_PASS;
    strncpy_s(cbuf+len, ssid, BUFFER_SIZE-len);
    len += strlen(cbuf+len)+1;
    strncpy_s(cbuf+len, passphrase, BUFFER_SIZE-len);
    len += strlen(cbuf+len)+1;
    return esp32_transfer(tx_buffer, len, rx_buffer, CESP_RESP_SET_SSID_AND_PASS);
}

bool esp32_spi_connect_AP(const char *ssid, const char *password, uint8_t retry_times)
{
#if ESP32_SPI_DEBUG
    printf("Connect to AP--> ssid: %s password:%s\r\n", ssid, password);
#endif
    if(!esp32_spi_wifi_set_ssid_and_pass(ssid, password))
        return false;

    int8_t stat = -1;

    for (uint8_t i = 0; i < retry_times; i++)
    {
        stat = esp32_spi_status();

        if (stat == -1)
        {
#if ESP32_SPI_DEBUG
            printf("%s get status error \r\n", __func__);
#endif
            esp32_spi_reset();
            return false;
        }
        else if (stat == WL_CONNECTED)
            return true;
        sleep(1);
    }
    stat = esp32_spi_status();

    if (stat == WL_DISCONNECTED)
    {
#if ESP32_SPI_DEBUG
        printf("Failed to connect to ssid: %s\r\n", ssid);
#endif

        return false;
    }

#if ESP32_SPI_DEBUG
    printf("Unknown error 0x%02X", stat);
#endif

    return false;
}

void esp32_spi_reset()
{
#if ESP32_SPI_DEBUG
    printf("Reset ESP32\r\n");
#endif

#if ESP32_HAVE_IO0
    gpiohs_set_drive_mode(ESP32_SPI_IO0_HS_NUM, GPIO_DM_OUTPUT); //gpio0
    gpiohs_set_pin(ESP32_SPI_IO0_HS_NUM, 1);
#endif

    //here we sleep 1s
    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);

    if ((int8_t)rst_num > 0)
    {
        gpiohs_set_drive_mode(rst_num, GPIO_DM_OUTPUT);
        gpiohs_set_pin(rst_num, GPIO_PV_LOW);
        msleep(500);
        gpiohs_set_pin(rst_num, GPIO_PV_HIGH);
        msleep(800);
        gpiohs_set_drive_mode(rst_num, GPIO_DM_INPUT);
    }
    else
    {
        //soft reset
        esp32_transfer_no_param(CESP_SOFT_RESET, 0);
        msleep(1500);
    }

#if ESP32_HAVE_IO0
    gpiohs_set_drive_mode(ESP32_SPI_IO0_HS_NUM, GPIO_DM_INPUT); //gpio0
#endif
}

//Converts a bytearray IP address to a dotted-quad string for printing
void esp32_spi_pretty_ip(const uint8_t ip[4], char *str_ip)
{
    sprintf(str_ip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return;
}

bool esp32_spi_get_host_by_name(const char *hostname, uint8_t ip[4])
{
    size_t len = 0;
    char* cbuf = (char*)tx_buffer;
    tx_buffer[len++] = CESP_REQ_HOST_BY_NAME;
    strncpy_s(cbuf+len, hostname, BUFFER_SIZE-len);
    len += strlen(cbuf+len)+1;

    if(!esp32_transfer(tx_buffer, len, rx_buffer, CESP_RESP_REQ_HOST_BY_NAME))
        return false;

    memcpy(ip, rx_buffer+4, 4);
    return rx_buffer[0]?true:false;
}

uint16_t esp32_spi_ping_ip(uint8_t ip[4], uint8_t ttl)
{
    tx_buffer[0] = CESP_PING;
    tx_buffer[1] = ttl;
    tx_buffer[2] = 0;
    tx_buffer[3] = 0;
    memcpy(tx_buffer+4, ip, 4);

    if(!esp32_transfer(tx_buffer, 8, rx_buffer, CESP_RESP_PING))
        return 0xFFFF;

    uint16_t result;
    memcpy(&result, rx_buffer+2, sizeof(result));
    return result;
}

uint16_t esp32_spi_ping(const char *dest, uint8_t ttl)
{
    uint8_t ip[4];
    if(!esp32_spi_get_host_by_name(dest, ip))
        return 0xFFFF;
    return esp32_spi_ping_ip(ip, ttl);
}

///////////////////////////////////////////////////////////////////////////////
#define ENUM_TO_STR(x) \
    case (x):          \
        return (#x)

const char* wlan_enum_to_str(esp32_wlan_enum_t x)
{
    switch (x)
    {
        ENUM_TO_STR(WL_IDLE_STATUS);
        ENUM_TO_STR(WL_CONNECTING);
        ENUM_TO_STR(WL_CONNECTED);
        ENUM_TO_STR(WL_DISCONNECTED);
    }
    return "unknown";
}

/*
dest_type 
            0 ip array
            1 hostname
return socket_num
return 255 if fail
*/
static esp32_socket esp32_spi_socket_open_internal(const uint8_t *dest, uint8_t dest_type,
                             uint16_t port, esp32_socket_mode_enum_t conn_mode)
{
    size_t len = 0;
    tx_buffer[0] = CESP_START_SOCKET_CLIENT;
    tx_buffer[1] = dest_type;
    memcpy(tx_buffer+2, &port, 2);
    len += 4;
    tx_buffer[len+0] = conn_mode;
    tx_buffer[len+1] = 0;
    tx_buffer[len+2] = 0;
    tx_buffer[len+3] = 0;
    len += 4;

    if(dest_type)
    {
        //hostname
        char* buf = (char*)(tx_buffer+len);
        strncpy_s(buf, (const char*)dest, BUFFER_SIZE-len);
        len += strlen(buf)+1;
    } else
    {
        //ip address
        memcpy(tx_buffer+len, dest, 4);
        len += 4;
    }

    if(!esp32_transfer(tx_buffer, len, rx_buffer, CESP_RESP_START_SOCKET_CLIENT))
        return esp32_socket::bad;

    return (esp32_socket)rx_buffer[0];
}

esp32_socket esp32_spi_socket_open_ip(const uint8_t ip[4],
                             uint16_t port, esp32_socket_mode_enum_t conn_mode)
{
    return esp32_spi_socket_open_internal(ip, 0, port, conn_mode);
}

esp32_socket esp32_spi_socket_open(const char* hostname,
                             uint16_t port, esp32_socket_mode_enum_t conn_mode)

{
    return esp32_spi_socket_open_internal((const uint8_t*)hostname, 1, port, conn_mode);
}

bool esp32_spi_socket_connected(esp32_socket socket_num)
{
    uint32_t sn = (uint32_t)socket_num;
    if(!esp32_transfer_no_param(CESP_GET_SOCKET_STATE|(sn<<8), CESP_RESP_GET_SOCKET_STATE))
        return false;
    return rx_buffer[0];
}

uint16_t esp32_spi_socket_available(esp32_socket socket_num)
{
    uint32_t sn = (uint32_t)socket_num;
    if(!esp32_transfer_no_param(CESP_AVAIL_SOCKET_DATA|(sn<<8), CESP_RESP_AVAIL_SOCKET_DATA))
        return 0;
    return *(uint16_t*)rx_buffer;
}

uint32_t esp32_spi_max_write_size()
{
    return BUFFER_SIZE-4;
}

uint16_t esp32_spi_socket_write(esp32_socket socket_num, const void* buffer, uint16_t len, bool* is_client_alive)
{
    if(len > esp32_spi_max_write_size())
        len = esp32_spi_max_write_size();

    tx_buffer[0] = CESP_SEND_SOCKET_DATA;
    tx_buffer[1] = (uint8_t)socket_num;
    memcpy(tx_buffer+2, &len, 2);
    memcpy(tx_buffer+4, buffer, len);

    if(!esp32_transfer(tx_buffer, len+4, rx_buffer, CESP_RESP_SEND_SOCKET_DATA))
        return 0;
    if(is_client_alive)
        *is_client_alive = rx_buffer[2]?true:false;
    return *(uint16_t*)rx_buffer;
}

uint16_t esp32_spi_socket_read(esp32_socket socket_num, void* buff, uint16_t size, bool* is_client_alive)
{
    if(size > BUFFER_SIZE-4)
    {
        if(is_client_alive)
            *is_client_alive = true; //Т.к. в реальности не знаем, какой статус.
        return 0;
    }
    tx_buffer[0] = CESP_READ_SOCKET_DATA;
    tx_buffer[1] = (uint8_t)socket_num;
    memcpy(tx_buffer+2, &size, 2);

    if(!esp32_transfer(tx_buffer, 4, rx_buffer, size+4))
    {
        printf("esp32_spi_socket_read fail\n");
        return 0;
    }
    memcpy(buff, rx_buffer+4, size);
    if(is_client_alive)
        *is_client_alive = rx_buffer[2]?true:false;
    return *(uint16_t*)rx_buffer;
}

bool esp32_spi_socket_close(esp32_socket socket_num)
{
    uint32_t sn = (uint32_t)socket_num;
    if(!esp32_transfer_no_param(CESP_CLOSE_SOCKET|(sn<<8), CESP_RESP_CLOSE_SOCKET))
        return false;
    return rx_buffer[0]?true:false;
}

esp32_socket connect_server_port_tcp(const char *host, uint16_t port)
{
    uint8_t ip[4];
    if (!esp32_spi_get_host_by_name(host, ip))
    {
        return esp32_spi_bad_socket();
    }

    return esp32_spi_socket_open_ip(ip, port, TCP_MODE);
}


bool esp32_spi_server_create(uint16_t port)
{
    uint32_t port32 = port;
    if(!esp32_transfer_no_param(CESP_TCP_SERVER_CREATE|(port32<<16), CESP_RESP_TCP_SERVER_CREATE))
        return 0;
    return rx_buffer[0]?true:false;

}

void esp32_spi_server_stop()
{
    esp32_transfer_no_param(CESP_TCP_SERVER_STOP, CESP_RESP_TCP_SERVER_STOP);
}

esp32_socket esp32_spi_server_accept(bool* is_server_alive)
{
    if(!esp32_transfer_no_param(CESP_TCP_SERVER_ACCEPT, CESP_RESP_TCP_SERVER_ACCEPT))
    {
        printf("esp32_spi_server_accept Transfer failed\n");
        return esp32_spi_bad_socket();
    }

    if(is_server_alive)
        *is_server_alive = rx_buffer[1]?true:false;
    return (esp32_socket)rx_buffer[0];
}
