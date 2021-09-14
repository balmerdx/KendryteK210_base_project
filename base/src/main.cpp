#include <stdio.h>
#include <unistd.h>
#include <fpioa.h>
#include <gpio.h>
#include <gpiohs.h>
#include "sysctl.h"
#include "syscalls.h"
#include "flash/flash.h"
#include "sleep.h"
#include "main.h"
#include "MessageDebug/MessageDebug.h"
#include "esp32/esp32_spi.h"
#include "esp32/spi2_slave.h"
#include "esp32_tests.h"

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
    if(false)
    {//check pins
        fpioa_function_t ESP32_HSPI_1 = FUNC_GPIOHS16;
        fpioa_function_t ESP32_HSPI_2 = FUNC_GPIOHS17;
        fpioa_function_t ESP32_HSPI_3 = FUNC_GPIOHS18;
        int pin_1 = ESP32_HSPI_1-FUNC_GPIOHS0;
        int pin_2 = ESP32_HSPI_2-FUNC_GPIOHS0;
        int pin_3 = ESP32_HSPI_3-FUNC_GPIOHS0;
        fpioa_set_function(13, ESP32_HSPI_1);
        fpioa_set_function(14, ESP32_HSPI_2);
        fpioa_set_function(15, ESP32_HSPI_3);

        gpiohs_set_drive_mode(pin_1, GPIO_DM_INPUT);
        gpiohs_set_drive_mode(pin_2, GPIO_DM_INPUT);
        gpiohs_set_drive_mode(pin_3, GPIO_DM_INPUT);

        int old_v1 = 0, old_v2 = 0, old_v3 = 0;

        while(1)
        {
            int v1 = gpiohs_get_pin(pin_1)?1:0;
            int v2 = gpiohs_get_pin(pin_2)?1:0;
            int v3 = gpiohs_get_pin(pin_3)?1:0;

            if(v1 != old_v1 || v2 != old_v2 || v3 != old_v3)
            {
                old_v1 = v1;
                old_v2 = v2;
                old_v3 = v3;
                uint64_t tm_ms = sysctl_get_time_us()/1000;
                printf("%lu ms: v1=%i v2=%i v3=%i\n", tm_ms, v1, v2, v3);

            }
            usleep(100);
        }

    }


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

int main(void) 
{
    //Ждем, пока подключится терминал
    msleep(300);

    printf("byte swap %x\n", __builtin_bswap32(0x01020304));

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
