#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>
//#include <bsp.h>
#include <spi.h>

#include "esp32_spi_balmer.h"
#include "wifi_passw.h"

const size_t buffer_size = 8;
uint8_t send_buffer[buffer_size];
uint8_t receive_buffer[buffer_size];

static void scan_WiFi()
{
    const esp32_spi_aps_list_t* scan = esp32_spi_scan_networks();
    printf("scan->num=%i\n", (int)scan->num);
    for(size_t i=0; i<scan->num; i++)
    {
        const esp32_spi_ap_t& aps = scan->aps[i];
        printf("SSID: %s, RSSI: %d, ENC: %d\r\n", aps.ssid, aps.rssi, aps.encr);
    }
}

static void test_invert()
{
    for(int i=0; i<sizeof(send_buffer); i++)
        send_buffer[i] = i+10;

    if(esp32_test_invert(send_buffer, sizeof(send_buffer)))
        printf("esp32_test_invert ok\n");
    else
        printf("esp32_test_invert fail\n");
}

static bool connect_AP(const char* ssid, const char* pass)
{
    bool status = esp32_spi_connect_AP(ssid, pass, 10);
    printf("Connecting to AP %s status: %s\r\n", (const char*)ssid, status?"Ok":"Fail");

    return status;
}

static void test_connection()
{
    uint8_t ip[4];
    esp32_spi_get_host_by_name("sipeed.com", ip);
    char str_ip[20];
    esp32_spi_pretty_ip(ip, str_ip);
    printf("IP: %s\r\n", str_ip);

    for(int i=0; i<4; i++)
    {
        uint16_t time = esp32_spi_ping_ip(ip, 100);
        printf("ping ip time: %dms\r\n", time);
    }

    for(int i=0; i<4; i++)
    {
        uint16_t time = esp32_spi_ping("sipeed.com", 100);
        printf("ping sipeed.com time: %dms\r\n", time);
    }
}

static void test_socket()
{
    uint8_t socket = connect_server_port_tcp("dl.sipeed.com", 80);
    if(socket == 0xff)
        return;

    bool connected = esp32_spi_socket_connected(socket);
    printf("open socket status: %s\r\n", connected?"connected":"disconnected");
    #define LEN 1024 // its max buffer length with which esp32 can read fast enough from internet
    uint8_t read_buf[LEN];

    if(connected)
    {
        const char* buf = "GET /MAIX/MaixPy/assets/Alice.jpg HTTP/1.1\r\nHost: dl.sipeed.com\r\ncache-control: no-cache\r\n\r\n";
        uint32_t len = 0;
        
        for(int i=0; i<5; i++)
        {
            len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
            printf("esp32_spi_socket_write return: %d\r\n", len);
            if(len)
                break;
            msleep(200);
        }
        
        int total = 0;

        msleep(300);

        if(len > 0)
        {
            uint16_t len1 = 0;            
            uint8_t tmp_buf[LEN] = {0};
            do{
                while(1)
                {
                    len = esp32_spi_socket_available(socket);
                    printf("bytes available %d\r\n", len);
                    if(len>0)
                        break;
                    msleep(300);
                }

                len1 = esp32_spi_socket_read(socket, &tmp_buf[0], len > LEN ? LEN:len);
                strncat((char*)read_buf, (char*)tmp_buf, len1);
                total += len1;
                connected = esp32_spi_socket_connected(socket);
                msleep(65);
           }while(len > LEN && connected);

            printf("total data read len: %d\r\n", total);
        }
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}


int main(void)
{
    msleep(300);
    printf( "Kendryte " __DATE__ " " __TIME__ "\r\n");
    printf( "ESP32 test app\r\n");
/*
    for(size_t s = 10; s<20; s++)
    {
        size_t o = esp32_round_up4(s);
        printf("s=%lu roundup(s)=%lu\n", s, o);
    }

    printf("sizeof(esp32_spi_aps_list_t)=%lu\n", sizeof(esp32_spi_aps_list_t));
*/  
    esp32_spi_init();
    esp32_spi_reset();

    test_invert();
    printf("firmware=%s\n", esp32_spi_firmware_version());
    printf("status=%i\n", (int)esp32_spi_status());
    scan_WiFi();

    esp32_spi_wifi_set_ssid_and_pass(SSID, PASS);
    while(!connect_AP(SSID, PASS));
    test_connection();
    test_socket();
    //esp32_spi_reset();

    while(1)
    {
        msleep(1000);
    }
}

/*
#include "esp32/esp32_spi.h"
#include "esp32/esp32_spi_io.h"

#define ESP32_CS        FUNC_GPIOHS10
#define ESP32_RST       FUNC_GPIOHS11
#define ESP32_RDY       FUNC_GPIOHS12
#define ESP32_MOSI      FUNC_GPIOHS13
#define ESP32_MISO      FUNC_GPIOHS14
#define ESP32_SCLK      FUNC_GPIOHS15

//#define SSID "Balmer_IOT"
#define SSID "auto"
#define PASS "GHSTMeHQ"

static uint8_t read_buf[0xffff] = {0};

static void init_spi()
{
    // INIT SPI
    // #iomap at MaixDuino
    fpioa_set_function(25, ESP32_CS);
    fpioa_set_function(8, ESP32_RST);
    fpioa_set_function(9, ESP32_RDY);
    fpioa_set_function(28, ESP32_MOSI);
    fpioa_set_function(26, ESP32_MISO);
    fpioa_set_function(27, ESP32_SCLK);


    esp32_spi_config_io(ESP32_CS - FUNC_GPIOHS0, ESP32_RST, ESP32_RDY - FUNC_GPIOHS0,
                        ESP32_MOSI - FUNC_GPIOHS0, ESP32_MISO - FUNC_GPIOHS0, ESP32_SCLK - FUNC_GPIOHS0);

    esp32_spi_init();
}

static void scan_WiFi()
{
    esp32_spi_aps_list_t *scan = esp32_spi_scan_networks();
    esp32_spi_ap_t *aps;
    for(int i=0;i<scan->aps_num;i++)
    {
        aps = scan->aps[i];
        printf("SSID: %s, RSSI: %d, ENC: %d\r\n", aps->ssid, aps->rssi, aps->encr);
    }
}

static int connect_AP(const uint8_t* ssid, const uint8_t* pass)
{
    int status = esp32_spi_connect_AP(ssid, pass, 10);
    printf("Connecting to AP %s status: %d\r\n", (const char*)ssid, status);

    return status;
}

static void test_connection()
{
    uint16_t time = esp32_spi_ping((uint8_t*)"sipeed.com", 1, 100);
    printf("time: %dms\r\n", time);
    
    uint8_t ip[4];
    esp32_spi_get_host_by_name((const uint8_t*)"sipeed.com", ip);
    uint8_t str_ip[20];
    esp32_spi_pretty_ip(ip, str_ip);
    printf("IP: %s\r\n", str_ip);
}

static void test_socket()
{
    _DEBUG();
    uint8_t socket = connect_server_port("dl.sipeed.com", 80);
    if(socket == 0xff)
        return;

    esp32_socket_enum_t status = esp32_spi_socket_status(socket);
    printf("open socket status: %s\r\n", socket_enum_to_str(status));
    #define LEN 1024 // its max buffer length with which esp32 can read fast enough from internet

    if(status == SOCKET_ESTABLISHED)
    {
        const char* buf = "GET /MAIX/MaixPy/assets/Alice.jpg HTTP/1.1\r\nHost: dl.sipeed.com\r\ncache-control: no-cache\r\n\r\n";
        uint32_t len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
        printf("len value: %d\r\n", len);
        int total = 0;

        msleep(300);

        if(len > 0)
        {
            uint16_t len1 = 0;            
            uint8_t tmp_buf[LEN] = {0};
            do{
                while(1)
                {
                    len = esp32_spi_socket_available(socket);
                    printf("bytes available %d\r\n", len);
                    if(len>0)
                        break;
                    msleep(300);
                }

                len1 = esp32_spi_socket_read(socket, &tmp_buf[0], len > LEN ? LEN:len);
                strncat((char*)read_buf, (char*)tmp_buf, len1);
                total += len1;
                status = esp32_spi_socket_status(socket);
                msleep(65);
           }while(len > LEN && status != -1);

            printf("total data read len: %d\r\n", total);
        }
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

int main(void)
{
    LOGI(__func__, "Kendryte " __DATE__ " " __TIME__ "\r\n");
    LOGI(__func__, "ESP32 test app\r\n");

    init_spi();
    // Get esp32 firmware version
    printf("version: %s\r\n", esp32_spi_firmware_version());

    scan_WiFi();
    while(connect_AP((const uint8_t*)SSID, (const uint8_t*)PASS) != 0);

    test_connection();

    test_socket();
    while(1)
    {
        msleep(1000);
    }
}
*/
/*
#include <stdio.h>
#include <unistd.h>
#include "fpioa.h"
#include "gpio.h"
#include "sysctl.h"
#include "syscalls.h"
#include "flash/flash.h"
#include "sleep.h"
#include "main.h"
#include "MessageDebug/MessageDebug.h"
#include "esp32/esp32_spi.h"
#include "esp32/esp32_spi_io.h"

//12 - blue
//13 - green
//14 - red
#define PIN_LED 14
#define GPIO_LED 3

void OnLineReceived(const char* str)
{
    if(strcmp(str, "conn")==0)
    {
        return;
    }

    //char* result = WifiSendCommandAndReceive(str);
    //printf("%s\n", convert_rn(result));
}


int main(void) 
{
    //Ждем, пока подключится терминал
    msleep(300);

    fpioa_set_function(PIN_LED, FUNC_GPIO3);

    MessageDebugInit(256, 500000);

    gpio_init();
    gpio_set_drive_mode(GPIO_LED, GPIO_DM_OUTPUT);
    gpio_pin_value_t led_on = GPIO_PV_HIGH;
    gpio_set_pin(GPIO_LED, led_on);
    printf("enter string line\n");
    int seconds = 0;

    hard_spi_config_io();
    esp32_spi_init(FUNC_GPIOHS10-FUNC_GPIOHS0, FUNC_GPIOHS11-FUNC_GPIOHS0, FUNC_GPIOHS12-FUNC_GPIOHS0, 1);
    printf("esp32 spi init complete\n");

    while(1)
    {
        msleep(1);
        gpio_pin_value_t new_led_on = ((sysctl_get_time_us()/1000000)%2)?GPIO_PV_HIGH:GPIO_PV_LOW;
        if(led_on!=new_led_on)
        {
            led_on = new_led_on;
            gpio_set_pin(GPIO_LED, led_on);
            if(seconds%3==0)
            {
                printf("seconds=%i ", seconds);
                fflush(stdout);
            }
            else
                printf("seconds=%i\n", seconds);
            seconds++;
        }

        MessageDebugQuant();

        
        while(1)
        {
            TBMessage message = MessageDebugNext();
            if(message.size==0)
                break;
            if(message.is_text)
            {
                OnLineReceived((const char*)message.data);
                //printf("text=%s\n", (const char*)message.data);
            } else
            {
                printf("bin size=%u ", message.size);
                for(uint32_t i=0; i<message.size; i++)
                    if(i==0)
                        printf("%02x", message.data[i]);
                    else
                        printf(",%02x", message.data[i]);
                printf("\n");
            }
        }
    }
}
*/