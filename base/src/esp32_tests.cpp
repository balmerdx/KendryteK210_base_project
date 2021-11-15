#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>

#include "esp32/esp32_spi.h"
#include "wifi_passw.h"
#include "main.h"

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

static bool start_AP(const char* ssid, const char* pass, uint8_t channel)
{
    bool status = esp32_spi_wifi_starting_ap(ssid, pass, channel);
    printf("Start AP %s status: %s\n", ssid, status?"Ok":"Fail");
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

static void test_get_ip_addr()
{
    uint8_t local_ip[4];
    uint8_t subnet_mask[4];
    uint8_t gateway_ip[4];
    if(!esp32_get_ip_addr(local_ip, subnet_mask, gateway_ip))
    {
        printf("esp32_get_ip_addr return false\n");
        return;
    }

    char str_ip[20];
    esp32_spi_pretty_ip(local_ip, str_ip);
    printf("local_ip: %s\n", str_ip);

    esp32_spi_pretty_ip(subnet_mask, str_ip);
    printf("subnet_mask: %s\n", str_ip);

    esp32_spi_pretty_ip(gateway_ip, str_ip);
    printf("gateway_ip: %s\n", str_ip);
}

static void test_socket()
{
    esp32_socket socket = connect_server_port_tcp("dl.sipeed.com", 80);
    if(socket == esp32_spi_bad_socket())
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
    esp32_socket socket = connect_server_port_tcp(site, 80);
    if(socket == esp32_spi_bad_socket())
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

static void read_many_from_socket(esp32_socket socket, uint64_t time_us = 5000000)
{
    const uint32_t LEN = 4000;
    int total = 0;
    uint16_t len_rx = 0;            
    uint8_t tmp_buf[LEN] = {0};
    uint64_t start_time_us = sysctl_get_time_us();
    uint64_t end_time_us = sysctl_get_time_us();
    do{
        bool is_client_alive;
        len_rx = esp32_spi_socket_read(socket, &tmp_buf[0], LEN, &is_client_alive);
        if(!is_client_alive)
        {
            printf("esp32_spi_socket_read is_client_alive=false\n");
            break;
        }
        //if(len_rx>0)
        //    printf("len_rx=%i\n", (int)len_rx);
        if(len_rx>0)
            end_time_us = sysctl_get_time_us();
        total += len_rx;
        if(len_rx < LEN)
            msleep(4);
        else
            usleep(500);
    }while(sysctl_get_time_us()-start_time_us<time_us);

    float dt = (end_time_us-start_time_us)*1e-6f;
    printf("total data read len: %d\r\n", total);
    printf("total time: %f\r\n", dt);
    printf("%.3f mbit/sec\n", (total*8/dt)*1e-6);
}

static void test_download_speed_iperf()
{
    const char* site = "192.168.1.48";
    esp32_socket socket = connect_server_port_tcp(site, 5001);
    if(socket == esp32_spi_bad_socket())
    {
        printf("Cannot connect to server: %s", site);
        return;
    }

    bool connected = esp32_spi_socket_connected(socket);
    printf("test_download_speed_iperf status: %s\r\n", connected?"connected":"disconnected");

    if(connected)
    {
        const char* buf = "B";
        uint32_t len = 0;

        len = esp32_spi_socket_write(socket, (uint8_t*)buf, strlen(buf)+1);
        printf("esp32_spi_socket_write return: %d\r\n", len);

        read_many_from_socket(socket);
    }

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

static void test_upload_speed_iperf()
{
    const char* site = "192.168.1.48";
    esp32_socket socket = connect_server_port_tcp(site, 5001);
    if(socket == esp32_spi_bad_socket())
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
    esp32_socket socket = connect_server_port_tcp(site, 5001);
    if(socket == esp32_spi_bad_socket())
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

    int close_status = esp32_spi_socket_close(socket);
    printf("close socket status: %i\r\n", close_status);
}

void test_server()
{
    uint16_t port = 5001;
    if(!esp32_spi_server_create(port))
    {
        printf("test_server cannot create server\n");
        return;
    }

    printf("test_server created, listening\n");
    while(1)
    {
        bool is_server_alive;
        esp32_socket socket = esp32_spi_server_accept(&is_server_alive);
        if(!is_server_alive)
        {
            printf("Server not alive\n");
            break;
        }
        msleep(1);

        if(socket!=esp32_spi_bad_socket())
        {
            read_many_from_socket(socket);
            int close_status = esp32_spi_socket_close(socket);
            printf("close socket status: %i\n", close_status);
        }
    }

    esp32_spi_server_stop();
}

void test_esp32()
{
    printf( "ESP32 test app\r\n");
    esp32_spi_init();
    esp32_spi_reset();

    test_invert();
    printf("firmware=%s\n", esp32_spi_firmware_version());
    printf("status=%i\n", (int)esp32_spi_status());
    //printf("temperature=%f\n", esp32_spi_get_temperature());
    //scan_WiFi();
    //while(!connect_AP(SSID, PASS));
    start_AP("K210", "12345678", 3);
    test_get_ip_addr(); while(1);
    while(!start_AP("K210", "12345678", 3));
    test_connection();
    //test_download_speed_short();
    //test_socket();
    //test_download_speed_iperf();
    //test_upload_speed_iperf();
    test_server();
    //esp32_spi_reset();

    while(1)
    {
        msleep(1000);
    }
}

void esp32_help()
{
    printf("esp32 test");
    printf("esp32 test");
}

void esp32_OnLineReceived(const char* str)
{

}