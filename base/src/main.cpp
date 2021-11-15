#include <stdio.h>
#include <unistd.h>
#include <fpioa.h>
#include <gpio.h>
#include <gpiohs.h>
#include <sysctl.h>
#include <syscalls.h>
#include <sleep.h>
#include "main.h"
#include "MessageDebug/MessageDebug.h"
#include "esp32/esp32_spi.h"
#include "esp32/spi2_slave.h"
#include "stm32_program/stm32_program.h"

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
}

bool esp32_transfer(const uint8_t* tx_buffer, size_t tx_len, uint8_t* rx_buffer, size_t rx_len);
void spi_slave_init();

void TestSlaveSpi()
{
    const int data_size = 32;
    uint32_t data[data_size];
    spi_slave_init();

    uint64_t tm_start = sysctl_get_time_us();
    char buf[32];
    while(1)
    {
        uint64_t timeout_us = 3000*1000;
        if(!spi2_slave_receive4((uint8_t*)data, data_size*4, timeout_us))
        {
            printf("Timeout\n");
            continue;
        }

        snprintf(buf, sizeof(buf), "%lu us", sysctl_get_time_us()-tm_start);

        dump_buffer(buf, (const uint8_t*)data, data_size*4);
        msleep(10);
    }
}

void TestFastSpi()
{
    esp32_spi_init();
    printf("Test fast spi.\n");

    char buffer[32];
    int idx = 0;

    while(1)
    {
        //snprintf(buffer, sizeof(buffer), "Hello, SPI %i", idx++);
        //printf("%s\n", buffer);
        //esp32_transfer((uint8_t*)buffer, strlen(buffer)+1, nullptr, 0);
        for(int i=0; i<sizeof(buffer);i++)
            buffer[i] = i+idx;
        printf("%i\n", idx);
        idx++;
        esp32_transfer((uint8_t*)buffer, sizeof(buffer), nullptr, 0);
        msleep(1000);
    }

}

void print_clocks()
{
    struct TS
    {
        const char* name;
        sysctl_clock_t clock;
    };

    TS t[] = 
    {
        {"IN0", SYSCTL_CLOCK_IN0},
        {"PLL0",SYSCTL_CLOCK_PLL0},
        {"PLL1",SYSCTL_CLOCK_PLL1},
        {"PLL2",SYSCTL_CLOCK_PLL2},
        {"CPU", SYSCTL_CLOCK_CPU},
        {"DMA", SYSCTL_CLOCK_DMA},
        {"FFT", SYSCTL_CLOCK_FFT},
        {"ACLK", SYSCTL_CLOCK_ACLK},
        {"HCLK", SYSCTL_CLOCK_HCLK},
        {"SRAM0", SYSCTL_CLOCK_SRAM0},
        {"SRAM1", SYSCTL_CLOCK_SRAM1},
        {"ROM", SYSCTL_CLOCK_ROM},
        {"DVP", SYSCTL_CLOCK_DVP},
        {"APB0", SYSCTL_CLOCK_APB0},
        {"APB1", SYSCTL_CLOCK_APB1},
        {"APB2", SYSCTL_CLOCK_APB2},
        {"AI", SYSCTL_CLOCK_AI},
        {"I2S0", SYSCTL_CLOCK_I2S0},
        {"I2S1", SYSCTL_CLOCK_I2S1},
        {"I2S2", SYSCTL_CLOCK_I2S2},
        {"WDT0", SYSCTL_CLOCK_WDT0},
        {"WDT1", SYSCTL_CLOCK_WDT1},
        {"SPI0", SYSCTL_CLOCK_SPI0},
        {"SPI1", SYSCTL_CLOCK_SPI1},
        {"SPI2", SYSCTL_CLOCK_SPI2},
        {"SPI3", SYSCTL_CLOCK_SPI3}, //IN0/PLL0
        {"I2C0", SYSCTL_CLOCK_I2C0},
        {"I2C1", SYSCTL_CLOCK_I2C1},
        {"I2C2", SYSCTL_CLOCK_I2C2},
        //{"TIMER0", SYSCTL_CLOCK_TIMER0}, //IN0/PLL0
        //{"TIMER1", SYSCTL_CLOCK_TIMER1}, //IN0/PLL0
        //{"TIMER2", SYSCTL_CLOCK_TIMER2}, //IN0/PLL0
        //{"GPIO", SYSCTL_CLOCK_GPIO}, //APB0
        //{"UART1", SYSCTL_CLOCK_UART1}, //APB0
        //{"UART2", SYSCTL_CLOCK_UART2}, //APB0
        //{"UART3", SYSCTL_CLOCK_UART3}, //APB0
        //{"SHA", SYSCTL_CLOCK_SHA}, //APB0
        {"AES", SYSCTL_CLOCK_AES}, //APB1
        {"OTP", SYSCTL_CLOCK_OTP}, //APB1
        {"RTC", SYSCTL_CLOCK_RTC}, //IN0
    };

    for(size_t i=0; i<sizeof(t)/sizeof(t[0]); i++)
    {
        TS& s = t[i];
        uint32_t freq = sysctl_clock_get_freq(s.clock);
        printf("clock %s = %u KHz\n", s.name, freq/1000);
    }
    //
}

void stm32_uart_test()
{
    printf("stm32_uart_test\n");
    stm32p_init();
    stm32p_test_loop();
}

int main(void) 
{
    //Ждем, пока подключится терминал
    msleep(300);
    
    //stm32_uart_test();
    //print_clocks();
    //camera_test();   while(1);

    //TestFastSpi();
    //TestSlaveSpi();

    test_esp32();//В ней используется вывод в консольку 115200

    fpioa_set_function(PIN_LED, FUNC_GPIO3);
    MessageDebugInit(256, 500000);

    gpio_init();
    gpio_set_drive_mode(GPIO_LED, GPIO_DM_OUTPUT);
    gpio_pin_value_t led_on = GPIO_PV_HIGH;
    gpio_set_pin(GPIO_LED, led_on);
    printf("Kendryte " __DATE__ " " __TIME__ "\n");
    printf("enter string line\n");
    int seconds = 0;

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
