#include <syslog.h>
#include <sysctl.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>
//#include <bsp.h>
#include <spi.h>

#include "esp32_spi_balmer.h"

//#define SSID "Balmer_IOT"
#define SSID "auto"
#define PASS "GHSTMeHQ"

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
    uint16_t time = esp32_spi_ping("sipeed.com", 1, 100);
    printf("time: %dms\r\n", time);
    
    uint8_t ip[4];
    esp32_spi_get_host_by_name("sipeed.com", ip);
    char str_ip[20];
    esp32_spi_pretty_ip(ip, str_ip);
    printf("IP: %s\r\n", str_ip);
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

    test_invert();
    printf("firmware=%s\n", esp32_spi_firmware_version());
    printf("status=%i\n", (int)esp32_spi_status());
    scan_WiFi();

    esp32_spi_wifi_set_ssid_and_pass(SSID, PASS);
    while(!connect_AP(SSID, PASS));
    test_connection();

    //esp32_spi_reset();

    while(1)
    {
        msleep(1000);
    }
}

/*
#include "esp32/esp32_spi.h"
#include "esp32/esp32_spi_io.h"
#define SPI_HARD

#define ESP32_CS        FUNC_GPIOHS10
#define ESP32_RST       FUNC_GPIOHS11
#define ESP32_RDY       FUNC_GPIOHS12
#define ESP32_MOSI      FUNC_GPIOHS13
#define ESP32_MISO      FUNC_GPIOHS14
#define ESP32_SCLK      FUNC_GPIOHS15

const int BYTES_PER_SPI = 4;
const int buffer_size = 16;
uint8_t send_buffer[buffer_size];
uint8_t receive_buffer[buffer_size];
extern "C"
{
extern uint8_t cs_num, rst_num, rdy_num;
void spi_send_data_normal(spi_device_num_t spi_num, spi_chip_select_t chip_select, const uint8_t *tx_buff, size_t tx_len);
}

#ifdef SPI_HARD
static void init_spi()
{
    fpioa_set_function(25, ESP32_CS);
    fpioa_set_function(8, ESP32_RST);
    fpioa_set_function(9, ESP32_RDY);
    //fpioa_set_function(28, ESP32_MOSI);
    //fpioa_set_function(26, ESP32_MISO);
    //fpioa_set_function(27, ESP32_SCLK);

    cs_num = ESP32_CS - FUNC_GPIOHS0;
    rdy_num = ESP32_RDY - FUNC_GPIOHS0;
    rst_num = ESP32_RST;

    gpiohs_set_drive_mode(rdy_num, GPIO_DM_INPUT); //ready

    fpioa_set_function(27, FUNC_SPI0_SCLK);
    fpioa_set_function(28, FUNC_SPI0_D0);
    fpioa_set_function(26, FUNC_SPI0_D1);
    //fpioa_set_function(SPI_MASTER_INT_PIN, FUNC_GPIOHS0 + SPI_MASTER_INT_IO);

    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8*BYTES_PER_SPI, 1);
    //spi_set_clk_rate(SPI_DEVICE_0, 1000000);
    spi_set_clk_rate(SPI_DEVICE_0, 9000000);

    gpiohs_set_drive_mode(cs_num, GPIO_DM_OUTPUT);
    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);

    //gpiohs_set_drive_mode(SPI_MASTER_INT_IO, GPIO_DM_INPUT_PULL_UP);
    //gpiohs_set_pin_edge(SPI_MASTER_INT_IO, GPIO_PE_FALLING);
    //gpiohs_set_irq(SPI_MASTER_INT_IO, 5, spi_slave_ready_irq);
}

#else
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

    //gpiohs_set_drive_mode(ESP32_RST-FUNC_GPIOHS0, GPIO_DM_INPUT);
}
#endif

static int8_t esp32_spi_wait_for_ready(gpio_pin_value_t value, uint64_t timeout_us)
{
    uint64_t tm = sysctl_get_time_us();
    while ((sysctl_get_time_us() - tm) < 10 * 1000 * 1000) //10s
    {
        if (gpiohs_get_pin(rdy_num) == value)
            return 0;
        usleep(30);
    }

    return -1;
}


void send_and_receive()
{
    for(int i=0; i<sizeof(send_buffer); i++)
        send_buffer[i] = i+1;

    int8_t ready = esp32_spi_wait_for_ready(GPIO_PV_LOW, 10 * 1000000);
    if(ready<0)
    {
        printf("esp32_spi_wait_for_ready(0) fail\n");
        return;
    }

    gpiohs_set_pin(cs_num, GPIO_PV_LOW);
    ready = esp32_spi_wait_for_ready(GPIO_PV_HIGH, 1000000);
    if(ready<0)
    {
        printf("esp32_spi_wait_for_ready(1) fail\n");
        gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
        return;
    }

#ifdef SPI_HARD
    spi_send_data_normal(SPI_DEVICE_0, SPI_CHIP_SELECT_0, send_buffer, sizeof(send_buffer));
#else    
    soft_spi_rw_len(send_buffer, NULL, sizeof(send_buffer));
#endif    

    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);

    //get response
    ready = esp32_spi_wait_for_ready(GPIO_PV_LOW, 10 * 1000000);
    if(ready<0)
    {
        printf("response esp32_spi_wait_for_ready(0) fail\n");
        return;
    }

    gpiohs_set_pin(cs_num, GPIO_PV_LOW);
    ready = esp32_spi_wait_for_ready(GPIO_PV_HIGH, 1000000);
    if(ready<0)
    {
        printf("response esp32_spi_wait_for_ready(1) fail\n");
        gpiohs_set_pin(cs_num, GPIO_PV_HIGH);
        return;
    }

#ifdef SPI_HARD
    spi_receive_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_0, NULL, 0, receive_buffer, sizeof(receive_buffer));
#else    
    soft_spi_rw_len(NULL, receive_buffer, sizeof(receive_buffer));
#endif    
    gpiohs_set_pin(cs_num, GPIO_PV_HIGH);

    printf("receive ");
    for(int i=0;i<sizeof(receive_buffer);i++)
        printf("%02x", (int)receive_buffer[i]);
    printf("\n");
}

int main(void)
{
    msleep(300);
    printf( "Kendryte " __DATE__ " " __TIME__ "\r\n");
    printf( "ESP32 test app\r\n");

    init_spi();

    send_and_receive();
    while(1)
    {
        msleep(1000);
    }
}
*/
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