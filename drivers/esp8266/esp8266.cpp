#include "esp8266.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>
#include <algorithm>
#include "fpioa.h"
#include "gpio.h"
#include "uart.h"
#include "sysctl.h"
#include "sleep.h"
#include "atomic.h"
#include "TBPipe/TBPipe.h"

#define PIN_WIFI_TX 6
#define PIN_WIFI_RX 7
#define PIN_WIFI_EN 8

#define GPIO_WIFI_EN 0
#define UART_NUM    UART_DEVICE_1
/*
Первым делам проверим, что корректно работает.
AT должно вернуть AT\r\n\r\nOK\r\n
После этого перекллючаем на ATE0 чтобы не было эха от команд.
Потом пробуем переключить на скорость 1000000, текоретически должно работать без проблемм. --ok
Подключится к сети. --ok
AT+CWMODE_DEF=1
AT+CWJAP_CUR="auto","GHSTMeHQ"
AT+CWJAP_CUR?
Переделать на прерывание. --ok

https://aliexpress.ru/item/32854509350.html - купить? Наверно нет, 
выгоднее купить Sipeed M1W с антеннами https://aliexpress.ru/item/1005001913472405.html
либо купить ESP32, они поновее и побыстрее работать должны https://aliexpress.ru/item/1005002958382495.html
Антеннки https://aliexpress.ru/item/4001119374612.html

Переслать данные. --ok
Большинство команд блокирующие, кроме приходящих данных.
UDP server
https://gist.github.com/mgub/3f4d2f074305d4d84344

echo "test from nc" | nc -4u -w0 192.168.1.45 4445 - тоже работает (даже с адресом 0.0.0.0)

Удалить spinlock_t lock_irq = SPINLOCK_INIT;spinlock_trylock/spinlock_unlock, подумать чем бы их заменить. --ok
Заменили на enable/disable irq

WifiSendUdp должна принимать link_id, проверить, что каждому из клиентов передаёт требуемое.
Проверить работу с несколькоми клиентами параллельно.
Проверить latency передачи данных.
Проверить скорость передачи данных.
Попробовать сделать отдельную WiFi сеть и подключиться к ней.
AT+PING="192.168.1.1"

Разобрались, почему не работает, оказывается постоянное sys_getchar в цикле ломало другой uart

https://github.com/espressif/esptool - esp flash
*/

static bool WifiInitCommands();
void WifiSetBaudrate(uint32_t big_baud_rate);
static char* WifiReceive(int timeout_ms);


struct Esp8266Data
{
    Esp8266PrefixParser prefix_parser;
    TBPipe pipe;
    TBParse parse;

    Esp8266Data(int buffer_size)
        : pipe(buffer_size)
        , parse(buffer_size, &prefix_parser)
    {
    }
};


static int WifiIrqCallback(void *ctx)
{
    TBPipe* pipe = (TBPipe*)ctx;
    char c;
    while(uart_receive_data(UART_NUM, &c, 1))
    {
        pipe->AppendByte(c);
    }

    return 0;
}

static uint8_t data alignas(Esp8266Data) [sizeof(Esp8266Data)];
static Esp8266Data* ptr = nullptr;
//Сообщения, которые приходят по Debug Uart

const int default_timeout_ms = 10;

bool StartFrom(const char* str, const char* start)
{
    size_t len1 = strlen(str);
    size_t len2 = strlen(start);
    return memcmp(str, start, len1<len2?len1:len2)==0;
}

void WifiInit()
{
    int buffer_size = 1024;
    uint32_t baud_rate = 115200;

    fpioa_set_function(PIN_WIFI_EN, (fpioa_function_t)(FUNC_GPIO0+GPIO_WIFI_EN));
    fpioa_set_function(PIN_WIFI_TX, (fpioa_function_t)(FUNC_UART1_RX+UART_NUM*2));
    fpioa_set_function(PIN_WIFI_RX, (fpioa_function_t)(FUNC_UART1_TX+UART_NUM*2));

    ptr = new((void*)data) Esp8266Data(buffer_size);
    ptr->parse.SetTimeout(5000);
    uart_init(UART_NUM);
    uart_configure(UART_NUM, baud_rate, UART_BITWIDTH_8BIT, UART_STOP_1, UART_PARITY_NONE);
    uart_set_receive_trigger(UART_NUM, UART_RECEIVE_FIFO_1);
    uart_irq_register(UART_NUM, UART_RECEIVE, WifiIrqCallback, &ptr->pipe, 1);

    gpio_init();
    gpio_set_drive_mode(GPIO_WIFI_EN, GPIO_DM_OUTPUT);
    gpio_set_pin(GPIO_WIFI_EN, GPIO_PV_LOW);
    msleep(10);
    gpio_set_pin(GPIO_WIFI_EN, GPIO_PV_HIGH);
    return;
    WifiInitCommands();
}

char* WifiSendCommandAndReceive(const char* cmd, int timeout_ms)
{
    usleep(200);
    printf("%s\n", cmd);
    uart_send_data(UART_NUM, cmd, strlen(cmd));
    uart_send_data(UART_NUM, "\r\n", 2);

    return WifiReceive(timeout_ms);
}

char* WifiReceive(int timeout_ms)
{
    const int buffer_size = 1024;
    static char text_buffer[buffer_size];
    int buffer_pos = 0;

    if(timeout_ms<0)
        timeout_ms = default_timeout_ms;
    const uint64_t timeout_us = 1000*timeout_ms;
    uint64_t start_time = sysctl_get_time_us();

    while(1)
    {
        uint8_t* data;
        uint32_t size;
        ptr->pipe.GetBuffer(data, size);
        ptr->parse.Append((uint8_t*)data, size);
        TBMessage msg = ptr->parse.NextMessage();
        uint64_t dt = sysctl_get_time_us() - start_time;
        if(msg.size)
        {
            int size_to_copy = std::min(buffer_size-2-buffer_pos, (int)msg.size);
            memcpy(text_buffer+buffer_pos, msg.data, size_to_copy);
            buffer_pos += size_to_copy;
            text_buffer[buffer_pos++] = '\n';
            start_time = sysctl_get_time_us();
        }

        if(msg.size==2 && msg.data[0]=='O' && msg.data[1]=='K')
            break;

        if(dt>=timeout_us)
        {
            //printf("timeout pos=%i dt=%i\n", recv_buffer_pos, (int)dt);
            break;
        }
        msleep(1);
    }
    
    text_buffer[buffer_pos] = 0;

    return text_buffer;
}

static char hex_char(uint8_t data)
{
    if(data<10)
        return data+'0';
    return data-10+'A';
}

char* convert_rn(const char* in)
{
    const int buffer_size = 128;
    static char buffer[buffer_size];
    int pos = 0;
    auto add = [&pos](char c)
    {
        if(pos<buffer_size-1)
            buffer[pos++] = c;
    };
    for(;*in;in++)
    {
        if(*in=='\r')
        {
            add('\\');
            add('r');
            continue;
        }

        if(*in=='\n')
        {
            add('\\');
            add('n');
            continue;
        }

        uint8_t d = *in;
        if(d<32 || d>=128)
        {
            add('\\');
            add('x');
            add(hex_char(d>>4));
            add(hex_char(d&0xF));
            continue;
        }

        add(*in);
    }

    buffer[pos] = 0;
    return buffer;
}


bool WifiSendCommandAndCheck(const char* cmd, const char* result, int timeout_ms)
{
    char* out = WifiSendCommandAndReceive(cmd, timeout_ms);
    if(strcmp(out, result)==0)
        return true;
    printf("WiFi command '%s' failed '%s'\n", cmd, convert_rn(out));
    return false;
}

//Проверяет, что в конце есть OK и обрезает его
static bool ResultIsOk(char* result)
{
    static const char* ok = "\nOK\n";
    size_t ok_len = strlen(ok);
    size_t len = strlen(result);
    if(len<ok_len)
        return false;
    if(strcmp(result+len-ok_len, ok)==0)
    {
        result[len-ok_len] = 0;
        return true;
    }

    return false;
}

bool WifiSendCommandAndOk(const char* cmd, char*& result, int timeout_ms)
{
    result = WifiSendCommandAndReceive(cmd, timeout_ms);
    return ResultIsOk(result);
}

bool WifiInitCommands()
{
    char* result;
    uint64_t start_time = sysctl_get_time_us();
    uint64_t min_wait_time = 200000;
    while(1)
    {
        result = WifiReceive(1);
        uint64_t dt = sysctl_get_time_us()-start_time;
        if(result[0]==0)
        {
            if(dt < min_wait_time)
                continue;
            break;
        }
        printf("%i us %s\n", (int)dt, convert_rn(result));
    }
    uint64_t dt = sysctl_get_time_us()-start_time;
    result = WifiSendCommandAndReceive("AT");
    printf("%i us %s\n", (int)dt, convert_rn(result));
    if(!WifiSendCommandAndCheck("AT", "AT\nOK\n"))
        return false;
    if(!WifiSendCommandAndCheck("ATE0", "ATE0\nOK\n"))
        return false;
    if(!WifiSendCommandAndCheck("AT", "OK\n"))
        return false;
    if(!WifiSendCommandAndCheck("AT+CIPMUX=1", "OK\n"))
        return false;

    //WifiSetBaudrate(2000000);//С этой командой всё работает, но виснет jtag

    WifiCWMode mode;
    if(WifiGetCWModeDef(&mode))
    {
        if(mode!=WifiCWMode_Station)
            WifiSetCWModeDef(WifiCWMode_Station);
    }

    return true;
}

void WifiSetBaudrate(uint32_t big_baud_rate)
{
    const size_t buffer_size = 64;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "AT+UART_CUR=%u,8,1,0,0", big_baud_rate);
    if(!WifiSendCommandAndCheck(buffer, "OK\n"))
        return;
    uart_configure(UART_NUM, big_baud_rate, UART_BITWIDTH_8BIT, UART_STOP_1, UART_PARITY_NONE);
    msleep(5);
    printf("new baudrate is=%i\n", big_baud_rate);
    char* result = WifiSendCommandAndReceive("AT+UART_CUR?");
    printf("r='%s'\n", convert_rn(result));
}

bool WifiSetCWModeDef(WifiCWMode mode)
{
    char cmd[]="AT+CWMODE_DEF=?";
    cmd[sizeof(cmd)/sizeof(cmd[0])-2] = mode+'0';
    return WifiSendCommandAndCheck(cmd, "\nOK\n", 10);
}

bool WifiGetCWModeDef(WifiCWMode* mode)
{
    char* result = WifiSendCommandAndReceive("AT+CWMODE_DEF?");
    printf("WifiGetCWModeDef='%s'\n", convert_rn(result));

    const char* prefix = "+CWMODE_DEF:";
    int len = strlen(prefix);
    if(memcmp(result, prefix, len)!=0)
        return false;
    char c = result[len];
    printf("c=%c\n", c);
    if(c>='1' && c<='3')
    {
        *mode = (WifiCWMode)(c-'0');
        return true;
    }

    return false;
}

void WifiConnect(const char* wifiname, const char* wifi_password, bool cur)
{
    const size_t buffer_size = 64;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "AT+CWJAP_%s=\"%s\",\"%s\"", cur?"CUR":"DEF",wifiname, wifi_password);
    char* result = WifiSendCommandAndReceive(buffer, 5000);
    printf("WifiConnect %s\n", convert_rn(result));
}

bool WifiTestConnected(bool cur)
{
    char* result;
    if(!WifiSendCommandAndOk(cur?"AT+CWJAP_CUR?":"AT+CWJAP_DEF?", result))
    {
        return false;
    }
    
    printf("WifiTestConnected %s\n", convert_rn(result));
    if(StartFrom(result, "+CWJAP_CUR"))
        return true;
    return false;
}

bool WifiStartUdpServer()
{
    char* result;
    bool ok = WifiSendCommandAndOk("AT+CIPSTART=0,\"UDP\",\"0.0.0.0\",22333,22333,2", result);
    //bool ok = WifiSendCommandAndOk("AT+CIPSTART=0,\"UDP\",\"0.0.0.0\",4445", result);
    //bool ok = WifiSendCommandAndOk("AT+CIPSTART=0,\"UDP\",\"192.168.1.48\",4445,4446,2", result);
    if(!ok)
        printf("CIPSTART  %s\n", convert_rn(result));
    return ok;
}

bool WifiStartTcpServer()
{
    char* result;
    bool ok = WifiSendCommandAndOk("AT+CIPSERVER=1,333", result);
    if(!ok)
        printf("CIPSTART  %s\n", convert_rn(result));
    return ok;
}


bool WifiStopServer()
{
    char* result;
    bool ok = WifiSendCommandAndOk("AT+CIPCLOSE=0", result);
    if(!ok)
        printf("CIPCLOSE %s\n", convert_rn(result));
    return ok;
}

bool WifiSend(const char* data, int size, uint8_t link_id)
{
    usleep(200);
    char cmd[64];
    //snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%i,%i,\"192.168.1.48\",22333", (int)link_id, size);
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%i,%i", (int)link_id, size); //Для варианта UDP сервера
    //printf("%s\n", cmd);
    uart_send_data(UART_NUM, cmd, strlen(cmd));
    uart_send_data(UART_NUM, "\r\n", 2);
    char* result = WifiReceive(default_timeout_ms);
    //printf("CIPSEND %s\n", convert_rn(result));
    uint64_t start_time = sysctl_get_time_us();
    uint64_t wait_time = 20000;
    while(1)
    {
        uint8_t* data;
        uint32_t size;
        ptr->pipe.GetBuffer(data, size);
        if(size>0)
        {
            if(data[0]=='>')
                break;
        }

        msleep(1);
        uint64_t dt = sysctl_get_time_us()-start_time;
        if(dt>wait_time)
        {
            printf("> timeout\n");
            break;
        }
    }

    uart_send_data(UART_NUM, data, size);

    result = WifiReceive(default_timeout_ms);
    //printf("CIPSEND %s\n", convert_rn(result));
    return true;
}

/*
WifiReceiveBuffer* WifiNextRecivedData()
{
    if(!wifi_receive_complete)
        return 0;
    wifi_receive_complete = false;
    return &bin_receive_buffer;
}
*/