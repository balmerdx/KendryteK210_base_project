#include "stm32_program.h"
#include <new>
#include <uart.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>
#include <sysctl.h>
#include <algorithm>

#include "TBPipe/TBPipe.h"
#include "TBPipe/TBParse.h"

#define UART_NUM    UART_DEVICE_1
#define STM32_RST   FUNC_GPIOHS3
#define STM32_BOOT0 FUNC_GPIOHS4

enum STM32_COMMANDS
{
    STM32_CMD_GET = 0x00,
    STM32_CMD_READ_MEMORY = 0x11,
    STM32_CMD_WRITE_MEMORY = 0x31,
    STM32_CMD_EXTENDED_ERASE =0x44,
};

enum STM32_CONSTANT
{
    STM32_ACTIVATE = 0x7F,
    STM32_ACK = 0x79,
    STM32_NACK = 0x1F,
};

struct ProgramData
{
    TBPipe pipe;
    TBParse parse;
    NullPrefixParser null_parser;

    ProgramData(int buffer_size)
        : pipe(buffer_size)
        , parse(buffer_size, &null_parser)
    {
    }
};
static uint8_t data alignas(ProgramData) [sizeof(ProgramData)];
static ProgramData* ptr = nullptr;
static const uint32_t default_timeout_ms = 10;
static bool bootloader_activated = false;

static int UartIrqCallback(void *ctx)
{
    //TBPipe* pipe = (TBPipe*)ctx;
    char buf[32];
    while(1)
    {
        int ret = uart_receive_data(UART_NUM, buf, sizeof(buf));
        if(ret==0)
            break;
        ptr->pipe.Write((uint8_t*)buf, ret);

        //pipe->Write((uint8_t*)buf, ret);
    }

    return 0;
}


static void uart_init(void* ctx)
{
    fpioa_set_function(10, (fpioa_function_t)(FUNC_UART1_RX + UART_NUM * 2));
    fpioa_set_function(3, (fpioa_function_t)(FUNC_UART1_TX + UART_NUM * 2));

    uart_init(UART_NUM);
    //uint32_t baud_rate = 115200;
    uint32_t baud_rate = 200000;
    uart_configure(UART_NUM, baud_rate, UART_BITWIDTH_8BIT, UART_STOP_1, UART_PARITY_EVEN);
    uart_set_send_trigger(UART_NUM, UART_SEND_FIFO_0);
    uart_set_receive_trigger(UART_NUM, UART_RECEIVE_FIFO_1);

    uart_irq_register(UART_NUM, UART_RECEIVE, UartIrqCallback, ctx, 1);
}

static void uart_send(const uint8_t *buffer, size_t buf_len)
{
    uart_send_data(UART_NUM, (const char*)buffer, buf_len);
}

/*
    Получаем данные, не менее, чем min_receive байт.
    Если прошло более, чем timeout_ms и мы данных не получили, то return false
*/
static bool uart_receive(uint8_t* data, uint32_t size, uint32_t timeout_ms)
{
    uint64_t timeout_us  = timeout_ms*1000;
    uint64_t start_time = sysctl_get_time_us();
    uint32_t prev_bytes = ptr->pipe.AvailableBytes();
    while(1)
    {
        uint32_t cur_bytes = ptr->pipe.AvailableBytes();
        if(cur_bytes >= size)
            break;

        if(prev_bytes < cur_bytes)
        {
            prev_bytes = cur_bytes;
            start_time = sysctl_get_time_us();
        }

        if(sysctl_get_time_us()-start_time>timeout_us)
            return false;
    }

    return ptr->pipe.ReadExact(data, size);
}

static inline void set_stm32_restart_pin(gpio_pin_value_t v)
{
    gpiohs_set_pin(STM32_RST-FUNC_GPIOHS0, v);
}

static inline void set_stm32_boot_pin(gpio_pin_value_t v)
{
    gpiohs_set_pin(STM32_BOOT0-FUNC_GPIOHS0, v);
}

static void stm32_restart(bool boot_mode = false)
{
    set_stm32_boot_pin(boot_mode?GPIO_PV_HIGH:GPIO_PV_LOW);
    set_stm32_restart_pin(GPIO_PV_LOW);
    msleep(30);
    set_stm32_restart_pin(GPIO_PV_HIGH);
}

void stm32p_restart()
{
    stm32_restart(false);
    ptr->pipe.Clear();
    bootloader_activated = false; 
}

void stm32p_init()
{
    int buffer_size = 300;
    ptr = new((void*)data) ProgramData(buffer_size);

    uart_init(&ptr->pipe);

    fpioa_set_function(11, STM32_RST);
    set_stm32_restart_pin(GPIO_PV_HIGH);
    gpiohs_set_drive_mode(STM32_RST-FUNC_GPIOHS0, GPIO_DM_OUTPUT);

    fpioa_set_function(12, STM32_BOOT0);
    set_stm32_boot_pin(GPIO_PV_LOW);
    gpiohs_set_drive_mode(STM32_BOOT0-FUNC_GPIOHS0, GPIO_DM_OUTPUT);
}

static void print_hex(uint8_t* data, uint32_t size)
{
    bool ended = true;
    for(uint32_t i=0; i<size; i++)
    {
        if(!ended && i%16==0)
        {
            printf("\n");
            ended = true;
        }

        if(i%2==0 && !ended)
            printf(" %02x", (int)data[i]);
        else
            printf("%02x", (int)data[i]);
        ended = false;
    }
    if(!ended)
        printf("\n");
}

void print_received(uint64_t msec=0)
{
    if(msec>0)
        msleep(msec);
    uint8_t* data;
    uint32_t size;
    for(int i=0; i<2; i++)
    {
        ptr->pipe.ReadStart(data, size);
        if(size>0)
            print_hex(data, size);
        ptr->pipe.ReadEnd();
    }
}

static bool stm32p_wait_ack(uint32_t timeout_ms = default_timeout_ms)
{
    uint8_t data[1];
    if(!uart_receive(data, 1, timeout_ms))
        return false;

    return data[0] == STM32_ACK;
}

static bool stm32p_send_cmd(uint8_t cmd, bool wait_ask=true)
{
    uint8_t buffer[2];

    buffer[0] = cmd;
    buffer[1] = cmd ^ 0xFF;
    uart_send(buffer, 2);

    if(!wait_ask)
        return true;
    return stm32p_wait_ack();
}

bool stm32p_activate_bootloader()
{
    stm32_restart(true);
    msleep(30);
    ptr->pipe.Clear();

    uint8_t buf = STM32_ACTIVATE;
    uart_send(&buf, 1);

    bool ok = stm32p_wait_ack();
    if(ok)
        bootloader_activated = true;
    return ok;
}

static bool stm32p_send_address(uint32_t address)
{
    uint8_t buf[5];
	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
    uart_send(buf, 5);

    if(!stm32p_wait_ack())
        return false;
    return true;
}

bool stm32p_read_memory(uint32_t address, uint8_t* read_data, uint32_t read_size)
{
    if(read_size==0)
        return true;
    if(read_size>256)
        return false;

    if(!stm32p_send_cmd(STM32_CMD_READ_MEMORY))
        return false;

    if(!stm32p_send_address(address))
        return false;

    stm32p_send_cmd((uint8_t)(read_size-1), false);

    uint8_t data[1];
    if(!uart_receive(data, 1, default_timeout_ms))
        return false;
    if(data[0] != STM32_ACK)
        return false;
    if(!uart_receive(read_data, read_size, default_timeout_ms))
        return false;
    return true;
}

bool stm32p_write_memory(uint32_t address, const uint8_t* write_data, uint32_t write_size)
{
    if(write_size==0)
        return true;
    if(write_size>256)
        return false;
    if(write_size&3)
        return false;

    if(!stm32p_send_cmd(STM32_CMD_WRITE_MEMORY))
        return false;
    if(!stm32p_send_address(address))
        return false;
    uint8_t write_size_m1 = (uint8_t)(write_size-1);
    uint8_t cs = 0;
    cs ^= write_size_m1;
    for(uint32_t i=0; i<write_size; i++)
        cs ^= write_data[i];
    uart_send(&write_size_m1, 1);
    uart_send(write_data, write_size);
    uart_send(&cs, 1);

    return stm32p_wait_ack(100);
}

bool stm32p_erase(uint32_t start_page, uint32_t num_pages)
{
    //Предполагаем, что у нас 64 КБ памяти всего
    //По 2 КБ одна страница
    const int max_num_pages = 32;
    if(num_pages>max_num_pages)
        return false;

    if(!stm32p_send_cmd(STM32_CMD_EXTENDED_ERASE))
        return false;

    uint8_t buf[2+2*max_num_pages+1];
    int i = 0;
    uint8_t cs = 0;

    buf[i++] = (num_pages-1) >> 8;
    buf[i++] = (num_pages-1);

    for(int j=0; j<num_pages; j++)
    {
        int page_num = start_page + j;
        buf[i++] = page_num >> 8;
        buf[i++] = page_num;
    }

    for(int j=0; j<i; j++)
        cs ^= buf[j];
    buf[i++] = cs;

    uart_send(buf, i);

    return stm32p_wait_ack(3000);
}

void stm32p_get_print()
{
    stm32p_send_cmd(STM32_CMD_GET, false);
    printf("STM32_CMD_GET\n");
    print_received(10);
}

__attribute((weak))
void stm32_log_callback(uint8_t* data, uint32_t size)
{
}

void stm32p_log_receive()
{
    if(bootloader_activated)
        return;

    uint8_t* data;
    uint32_t size;
    ptr->pipe.ReadStart(data, size);
    ptr->parse.Append(data, size);
    ptr->pipe.ReadEnd();

    while(1)
    {
        TBMessage m = ptr->parse.NextMessage();
        if(m.size==0)
            break;
        //printf("stm32_log_callback size=%u\n", m.size);
        stm32_log_callback(m.data, m.size);
    }
}

void stm32p_test_loop()
{
    printf("Start stm32p_test_loop\n");
    if(!stm32p_activate_bootloader())
        while(1);
    printf("stm32p_activate_bootloader success\n");

    stm32p_get_print();
/*
    if(stm32p_erase(0, 1))
        printf("erase success\n");
    else
        printf("erase fail\n");
*/
    uint32_t page_size = 2048;
    uint32_t address_start_flash = 0x08000000;

    if(true)  
    {
        uint8_t write_data[256];
        uint32_t write_size = 256;
        for(uint32_t i=0; i<write_size; i++)
            write_data[i] = 0x30-i;
        if(stm32p_write_memory(address_start_flash, write_data, write_size))
            printf("write memory success\n");
        else
            printf("write memory fail\n");
    }

    uint32_t address = address_start_flash+page_size*0;
    uint8_t read_data[256];
    uint32_t read_size = 256;

    for(int i=0; i<4; i++)
    {
        printf("%x\n", address);
        if(stm32p_read_memory(address, read_data, read_size))
        {
            print_hex(read_data, read_size);
        } else
        {
            printf("stm32p_read_memory fail\n");
        }

        address += read_size;
    }

    while(1)
    {
        print_received(10);
    }
}