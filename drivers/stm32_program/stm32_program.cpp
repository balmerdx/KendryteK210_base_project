#include "stm32_program.h"
#include <new>
#include <uart.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <sleep.h>
#include <sysctl.h>

#include "TBPipe/TBPipe.h"

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
    ProgramData(int buffer_size)
        : pipe(buffer_size)
    {
    }
};
static uint8_t data alignas(ProgramData) [sizeof(ProgramData)];
static ProgramData* ptr = nullptr;
static const uint32_t default_timeout_ms = 10;

static int UartIrqCallback(void *ctx)
{
    TBPipe* pipe = (TBPipe*)ctx;
    char c;
    while(uart_receive_data(UART_NUM, &c, 1))
    {
        pipe->AppendByte(c);
    }

    return 0;
}


static void uart_init(void* ctx)
{
    fpioa_set_function(10, (fpioa_function_t)(FUNC_UART1_RX + UART_NUM * 2));
    fpioa_set_function(3, (fpioa_function_t)(FUNC_UART1_TX + UART_NUM * 2));

    uart_init(UART_NUM);
    uint32_t baud_rate = 115200;
    //uint32_t baud_rate = 9200;
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
static bool uart_receive(uint8_t*& data, uint32_t& size, size_t min_receive, uint32_t timeout_ms)
{
    uint64_t timeout_us  = timeout_ms*1000;
    uint64_t start_time = sysctl_get_time_us();
    uint32_t prev_bytes = ptr->pipe.AvailableBytes();
    while(1)
    {
        uint32_t cur_bytes = ptr->pipe.AvailableBytes();
        if(cur_bytes >= min_receive)
            break;

        if(prev_bytes < cur_bytes)
        {
            prev_bytes = cur_bytes;
            start_time = sysctl_get_time_us();
        }

        if(sysctl_get_time_us()-start_time>timeout_us)
            return false;
    }

    ptr->pipe.GetBuffer(data, size);
    return true;
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
    ptr->pipe.GetBuffer(data, size);
    print_hex(data, size);
}

static bool stm32p_wait_ack()
{
    uint8_t* data;
    uint32_t size;
    if(!uart_receive(data, size, 1, default_timeout_ms))
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
    printf("stm32p_activate_bootloader\n");
    stm32_restart(true);
    msleep(30);
    uint8_t* data;
    uint32_t size;
    ptr->pipe.GetBuffer(data, size);

    uint8_t buf = STM32_ACTIVATE;
    uart_send(&buf, 1);

    return stm32p_wait_ack();
}

bool stm32p_read_memory(uint32_t address, uint8_t* read_data, uint32_t read_size)
{
    if(read_size>256 || read_size==0)
        return false;

    if(!stm32p_send_cmd(STM32_CMD_READ_MEMORY))
        return false;

    uint8_t buf[5];
	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
    uart_send(buf, 5);

    if(!stm32p_wait_ack())
        return false;

    stm32p_send_cmd((uint8_t)(read_size-1), false);

    uint8_t* data;
    uint32_t size;
    if(!uart_receive(data, size, read_size+1, default_timeout_ms))
        return false;
    if(data[0] != STM32_ACK)
        return false;
    memcpy(read_data, data+1, read_size);
    return true;
}

void stm32p_test_loop()
{
    printf("Start stm32p_test_loop\n");
    if(!stm32p_activate_bootloader())
        while(1);
    printf("stm32p_activate_bootloader success\n");

    stm32p_send_cmd(STM32_CMD_GET, false);
    printf("STM32_CMD_GET\n");
    print_received(10);

    uint32_t address = 0x08000000;
    uint8_t read_data[256];
    uint32_t read_size = 256;
    if(stm32p_read_memory(address, read_data, read_size))
    {
        printf("stm32p_read_memory success\n");
        print_hex(read_data, read_size);
    } else
    {
        printf("stm32p_read_memory fail\n");
    }

    while(1)
    {
        print_received(10);
    }
}