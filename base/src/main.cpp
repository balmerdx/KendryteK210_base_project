/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <unistd.h>
#include "fpioa.h"
#include "gpio.h"
#include "sysctl.h"
#include "syscalls.h"
#include "flash/flash.h"
#include "sleep.h"
#include "main.h"

//12 - blue
//13 - green
//14 - red
#define PIN_LED 14
#define GPIO_LED 3

void OnLineReceived(const char* str)
{
    printf("str=%s\n", str);
}

void ReceiveChars()
{
    static char str[256] = {};
    static int str_offset = 0;
    static uint64_t last_time = 0;

    uint64_t cur_time = sysctl_get_time_us();
    const uint64_t min_wait_time_us = 100;
    if(cur_time < last_time+min_wait_time_us)
        return;

    while(1)
    {
        int ch = sys_getchar();
        if(ch==EOF)
            break;
        if(ch=='\r')
            continue;
        sys_putchar(ch);
        if(ch=='\n')
        {
            str[str_offset++] = 0;
            OnLineReceived(str);
            str_offset = 0;
            break;
        }
        if(str_offset<sizeof(str)-1)
            str[str_offset++] = ch;
    }

    last_time = sysctl_get_time_us();
}

extern "C"
int main(void) 
{
    fpioa_set_function(PIN_LED, FUNC_GPIO3);

    gpio_init();
    gpio_set_drive_mode(GPIO_LED, GPIO_DM_OUTPUT);
    gpio_pin_value_t led_on = GPIO_PV_HIGH;
    gpio_set_pin(GPIO_LED, led_on);
    printf("enter string line\n");
    int seconds = 0;

    while(1)
    {
        ReceiveChars();

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
    }


    //gpio_set_pin(GPIO_LED, value = !value);
}
