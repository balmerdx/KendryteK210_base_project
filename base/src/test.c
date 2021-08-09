#include <stdio.h>
#include <math.h>
int z = 12;

typedef union 
{
    struct
    {
        uint16_t a : 2;
        uint16_t b : 3;
        uint16_t c : 11;
    } abc;
    uint16_t reg;
} Reg;

Reg SetRegExample()
{
    Reg r;
    r = (Reg){ .abc = {.a = 2, .b = 1} };
    return r;
}


void test_print(int k)
{
    z--;
    printf("%i %i\n", k, z);
}

const char* test_get_text()
{
    return "test_get_text";
}

float test_fmad(float a, float b, float c)
{
    //return a*b+c;
    return fmaxf(a,b);
}

int test_conv(float a)
{
    return a;
}

int test_loop(int count)
{
    volatile int counter = count + 1;
    do
    {
        counter--;
    } while (counter!=0);

    return counter;
}