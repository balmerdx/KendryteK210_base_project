#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>
//#include <bsp.h>
#include <spi.h>

#include "esp32_spi_balmer.h"
#include "wifi_passw.h"

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
    const size_t buffer_size = 4000;
    uint8_t send_buffer[buffer_size];

    esp32_set_debug(true);
    bool break_next = false;
    for(int size=4; size<=buffer_size; size*=2)
    {
        for(int i=0; i<size; i++)
            send_buffer[i] = i+10;

        int result = esp32_test_invert(send_buffer, size);
        printf("esp32_test_invert %i\n", size);
        if(result!=size)
        {
            printf("esp32_test_invert fail %i!=%i\n", size, result);
            while(1);
        }

        if(break_next)
            break;

        if(size*2>buffer_size)
        {
            size = buffer_size/2;
            break_next = true;
        }
    }

    esp32_set_debug(false);
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

    for(int i=0; i<2; i++)
    {
        uint16_t time = esp32_spi_ping_ip(ip, 100);
        printf("ping ip time: %dms\r\n", time);
    }

    for(int i=0; i<2; i++)
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
    const uint32_t LEN = 1024; // its max buffer length with which esp32 can read fast enough from internet
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
            uint64_t start_time_us = sysctl_get_time_us();
            do{
                int max_retry = 10;
                while(1)
                {
                    len = esp32_spi_socket_available(socket);
                    printf("bytes available %d\r\n", len);
                    if(len>0)
                        break;
                    msleep(300);
                    max_retry--;
                    if(max_retry<0)
                        break;
                }
                if(max_retry<0)
                    break;

                len1 = esp32_spi_socket_read(socket, &tmp_buf[0], len > LEN ? LEN:len);
                //strncat((char*)read_buf, (char*)tmp_buf, len1);
                total += len1;
                connected = esp32_spi_socket_connected(socket);
                msleep(65);
           }while(sysctl_get_time_us()-start_time_us<5000000 && connected);

           printf("total data read len: %d\r\n", total);
        }
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

static void test_download_speed()
{
    const char* site = "dl.sipeed.com";
    uint8_t socket = connect_server_port_tcp(site, 80);
    if(socket == 0xff)
    {
        printf("Cannot connect to server: %s", site);
        return;
    }

    bool connected = esp32_spi_socket_connected(socket);
    printf("open socket status: %s\r\n", connected?"connected":"disconnected");
    const uint32_t LEN = 1024; // its max buffer length with which esp32 can read fast enough from internet

    if(connected)
    {
        const char* buf = "GET /MAIX/MaixPy/assets/Alice.jpg HTTP/1.1\r\nHost: dl.sipeed.com\r\ncache-control: no-cache\r\n\r\n";
        uint32_t len = 0;
        
        len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
        printf("esp32_spi_socket_write return: %d\r\n", len);
       
        int total = 0;

        if(len > 0)
        {
            uint16_t len_rx = 0;            
            uint8_t tmp_buf[LEN] = {0};
            uint64_t start_time_us = sysctl_get_time_us();
            uint64_t end_time_us = sysctl_get_time_us();
            do{
                len_rx = esp32_spi_socket_read(socket, &tmp_buf[0], len > LEN ? LEN:len);
                if(len_rx>0)
                    end_time_us = sysctl_get_time_us();
                total += len_rx;
                msleep(1);
            }while(sysctl_get_time_us()-start_time_us<5000000);

            float dt = (end_time_us-start_time_us)*1e-6f;
            printf("total data read len: %d\r\n", total);
            printf("total time: %f\r\n", dt);
            printf("%.3f mbit/sec\n", (total*8/dt)*1e-6);
        }
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

static void test_download_speed_iperf()
{
    const char* site = "192.168.1.48";
    uint8_t socket = connect_server_port_tcp(site, 5001);
    if(socket == 0xff)
    {
        printf("Cannot connect to server: %s", site);
        return;
    }

    bool connected = esp32_spi_socket_connected(socket);
    printf("test_download_speed_iperf status: %s\r\n", connected?"connected":"disconnected");
    const uint32_t LEN = 4000;

    if(connected)
    {
        const char* buf = "B";
        uint32_t len = 0;

        len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
        printf("esp32_spi_socket_write return: %d\r\n", len);

        int total = 0;
        
        {
            uint16_t len_rx = 0;            
            uint8_t tmp_buf[LEN] = {0};
            uint64_t start_time_us = sysctl_get_time_us();
            uint64_t end_time_us = sysctl_get_time_us();
            do{
                len_rx = esp32_spi_socket_read(socket, &tmp_buf[0], LEN);
                //if(len_rx>0)
                //    printf("len_rx=%i\n", (int)len_rx);
                if(len_rx>0)
                    end_time_us = sysctl_get_time_us();
                total += len_rx;
                if(len_rx < LEN)
                    msleep(4);
                else
                    usleep(500);
            }while(sysctl_get_time_us()-start_time_us<5000000);

            float dt = (end_time_us-start_time_us)*1e-6f;
            printf("total data read len: %d\r\n", total);
            printf("total time: %f\r\n", dt);
            printf("%.3f mbit/sec\n", (total*8/dt)*1e-6);
        }
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

static void test_upload_speed_iperf()
{
    const char* site = "192.168.1.48";
    uint8_t socket = connect_server_port_tcp(site, 5001);
    if(socket == 0xff)
    {
        printf("Cannot connect to server: %s", site);
        return;
    }

    bool connected = esp32_spi_socket_connected(socket);
    printf("test_upload_speed_iperf status: %s\r\n", connected?"connected":"disconnected");
    const uint32_t LEN = 4000;

    if(connected)
    {
        int total = 0;
        //const char* message = "Hello, from K210!";
        //esp32_spi_socket_write(socket, message, strlen(message));

        {
            uint16_t len_tx = 0;            
            uint8_t tmp_buf[LEN] = {0};
            uint64_t start_time_us = sysctl_get_time_us();
            uint64_t end_time_us = sysctl_get_time_us();
            do{
                len_tx = esp32_spi_socket_write(socket, &tmp_buf[0], LEN);
                //if(len_tx>0)
                //    printf("len_tx=%i\n", (int)len_tx);
                total += len_tx;
                msleep(1);
            }while(sysctl_get_time_us()-start_time_us<15000000);
            end_time_us = sysctl_get_time_us();

            float dt = (end_time_us-start_time_us)*1e-6f;
            printf("total data write len: %d\r\n", total);
            printf("total time: %f\r\n", dt);
            printf("%.3f mbit/sec\n", (total*8/dt)*1e-6);
        }

    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

static void test_download_speed_short()
{
    const char* site = "192.168.1.48";
    uint8_t socket = connect_server_port_tcp(site, 5001);
    if(socket == 0xff)
    {
        printf("Cannot connect to server: %s", site);
        return;
    }

    bool connected = esp32_spi_socket_connected(socket);
    printf("test_download_speed_iperf status: %s\r\n", connected?"connected":"disconnected");
    #define LEN 4000

    if(connected)
    {
        const char* buf = "GET /MAIX/MaixPy/assets/Alice.jpg HTTP/1.1\r\nHost: dl.sipeed.com\r\ncache-control: no-cache\r\n\r\n";
        uint32_t len = 0;

        len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
        printf("esp32_spi_socket_write return: %d\r\n", len);

        int total = 0;
        
        {
            uint16_t len_rx = 0;            
            uint8_t tmp_buf[LEN] = {0};
            uint64_t start_time_us = sysctl_get_time_us();
            uint64_t end_time_us = sysctl_get_time_us();
            do{
                len_rx = esp32_spi_socket_read(socket, &tmp_buf[total], LEN-1-total);
                //if(len_rx>0)
                //    printf("len_rx=%i\n", (int)len_rx);
                if(len_rx>0)
                    end_time_us = sysctl_get_time_us();
                total += len_rx;

                if(total>0)
                    break;
                msleep(1);
            }while(sysctl_get_time_us()-start_time_us<5000000);

            float dt = (end_time_us-start_time_us)*1e-6f;
            printf("total data read len: %d\r\n", total);
            printf("total time: %f\r\n", dt);
            printf("%.3f mbit/sec\n", (total*8/dt)*1e-6);
            printf("message: %s\r\n", (const char*)tmp_buf);
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

    esp32_spi_init();
    esp32_spi_reset();

    test_invert();
    printf("firmware=%s\n", esp32_spi_firmware_version());
    printf("status=%i\n", (int)esp32_spi_status());
    printf("temperature=%f\n", esp32_spi_get_temperature());
    scan_WiFi();

    while(!connect_AP(SSID, PASS));
    test_connection();
    //test_download_speed_short();
    //test_socket();
    test_download_speed_iperf();
    //test_upload_speed_iperf();
    //esp32_spi_reset();

    while(1)
    {
        msleep(1000);
    }
}

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