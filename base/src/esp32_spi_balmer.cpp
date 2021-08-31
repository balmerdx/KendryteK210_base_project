
#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <sleep.h>
//#include <bsp.h>
#include <spi.h>
#include <gpiohs.h>

#include "esp32_spi_balmer.h"
#include "../../Maixduino_esp32/main/Esp32CommandList.h"

#define ESP32_CS        FUNC_GPIOHS10
#define ESP32_RST       FUNC_GPIOHS11
#define ESP32_RDY       FUNC_GPIOHS12
#define ESP32_MOSI      FUNC_GPIOHS13
#define ESP32_MISO      FUNC_GPIOHS14
#define ESP32_SCLK      FUNC_GPIOHS15

#define BUFFER_SIZE 4094

static uint8_t cs_num, rst_num, rdy_num;
static bool debug = true;

static uint8_t rx_buffer[BUFFER_SIZE];
static uint8_t tx_buffer[BUFFER_SIZE];

extern "C"
{
void spi_send_data_normal(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8_t *tx_buff, size_t tx_len);
}

void esp32_spi_init()
{
    fpioa_set_function(25, ESP32_CS);
    fpioa_set_function(8, ESP32_RST);
    fpioa_set_function(9, ESP32_RDY);

    cs_num = ESP32_CS - FUNC_GPIOHS0;
    rdy_num = ESP32_RDY - FUNC_GPIOHS0;
    rst_num = ESP32_RST - FUNC_GPIOHS0;

    gpiohs_set_drive_mode(rdy_num, GPIO_DM_INPUT); //ready

    fpioa_set_function(27, FUNC_SPI0_SCLK);
    fpioa_set_function(28, FUNC_SPI0_D0);
    fpioa_set_function(26, FUNC_SPI0_D1);

    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 32, 1);
    spi_set_clk_rate(SPI_DEVICE_0, 9000000);

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
bool esp32_transfer(uint8_t* tx_buffer, size_t tx_len, uint8_t* rx_buffer, size_t rx_len)
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
    ready = esp32_spi_wait_for_ready(GPIO_PV_LOW, 10 * 1000000);
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

    if(debug)
    {
        printf("receive ");
        for(size_t i=0;i<rx_len;i++)
            printf("%02x", (int)rx_buffer[i]);
        printf("\n");
    }

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

bool esp32_test_invert(uint8_t* data, size_t len)
{
    if(len&3)
        return false;
    *(uint32_t*)tx_buffer = (len<<16)|CESP_INVERT_BYTES;
    memcpy(tx_buffer+4, data, len);
    esp32_transfer(tx_buffer, len+4, rx_buffer, len);

    bool match = true;
    for(size_t i=0; i<len; i++)
    if((data[i]^0xFF)!=rx_buffer[i])
    {
        match = false;
        break;        
    }
    return match;
}

const char* esp32_spi_firmware_version()
{
    esp32_transfer_no_param(CESP_GET_FW_VERSION, CESP_RESP_FW_VERSION);
    rx_buffer[CESP_RESP_FW_VERSION] = 0;
    return (const char*)rx_buffer;
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

int8_t esp32_spi_status()
{
    if(!esp32_transfer_no_param(CESP_GET_CONN_STATUS, CESP_RESP_GET_CONN_STATUS))
        return 0;
    return rx_buffer[1];
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
/*
int8_t esp32_spi_connect_AP(const uint8_t *ssid, const uint8_t *password, uint8_t retry_times)
{
#if ESP32_SPI_DEBUG
    printf("Connect to AP--> ssid: %s password:%s\r\n", ssid, password);
#endif
    esp32_spi_wifi_set_ssid_and_pass(ssid, password);

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

    if (stat == WL_CONNECT_FAILED || stat == WL_CONNECTION_LOST || stat == WL_DISCONNECTED)
    {
#if ESP32_SPI_DEBUG
        printf("Failed to connect to ssid: %s\r\n", ssid);
#endif

        return false;
    }

    if (stat == WL_NO_SSID_AVAIL)
    {
#if ESP32_SPI_DEBUG
        printf("No such ssid: %s\r\n", ssid);
#endif

        return false;
    }

#if ESP32_SPI_DEBUG
    printf("Unknown error 0x%02X", stat);
#endif

    return false;
}

uint16_t esp32_spi_ping(const uint8_t *dest, uint8_t dest_type, uint8_t ttl)
{

}

void esp32_spi_pretty_ip(uint8_t *ip, uint8_t *str_ip)
{

}

int8_t esp32_spi_get_host_by_name(const uint8_t *hostname, uint8_t *ip)
{

}
*/