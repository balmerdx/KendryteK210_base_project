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

//12 - blue
//13 - green
//14 - red
#define PIN_LED 14
#define GPIO_LED 3

void test_print(int k);
const char* test_get_text();
int asm_get_int();
const char* asm_get_str();
float test_fmad(float a, float b, float c);
int test_conv(float a);
int asm_loop(int count);
float asm_loop_float(int count, float a, float b, float c);

float fa = 1.23f;
float fb = 2.34f;
float fc = 3.45f;

typedef int (*additional_f)(float);
additional_f f = (additional_f)0x80070000;

int main(void) 
{
    fpioa_set_function(PIN_LED, FUNC_GPIO3);

    gpio_init();
    gpio_set_drive_mode(GPIO_LED, GPIO_DM_OUTPUT);
    gpio_pin_value_t value = GPIO_PV_HIGH;
    gpio_set_pin(GPIO_LED, value);
    int k = 0;
    printf("enter string line\n");
    char str[128] = {};

    while(0)
    {
        gets(str);
        printf("line=%s\n", str);
        if(str[0]=='x')
        {
            float in = 1;
            sscanf(str+1, "%f", &in);
            int out = f(in);
            printf("in=%f out=%i\n", in, out);
        }
    }


    while (1)
    {
        sleep(1);
        gpio_set_pin(GPIO_LED, value = !value);
        k++;
        //test_print(k);
        //printf("asm_get_int=%i\n", asm_get_int());
        //printf("asm_get_str=%s\n", asm_get_str());
        //printf("%f\n", test_fmad(fa, fb, fc));
        //printf("%i\n", test_conv(fa));

        uint64_t time_last = sysctl_get_time_us();
        float ret = asm_loop_float(1000000, 0.1f, 0.2f, 0.3f);
        uint64_t time_now = sysctl_get_time_us();
        printf("%i us %f\n", (int)(time_now-time_last), ret);
    }
}
