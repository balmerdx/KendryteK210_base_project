#include "TBPipe/TBPipe.h"
#include "TBPipe/TBParse.h"
#include "MessageDebug.h"
#include "uart.h"
#include <new>

#define DEBUG_UART_NUM    UART_DEVICE_3

struct MessageDebugData
{
    StandartPrefixParser standart_parser;
    TBPipe keyboard_pipe;
    TBParse keyboard_parse;

    MessageDebugData(int buffer_size)
        : keyboard_pipe(buffer_size)
        , keyboard_parse(buffer_size, &standart_parser)
    {
    }
};


static int KeyboardIrqCallback(void *ctx)
{
    TBPipe* pipe = (TBPipe*)ctx;
    char c;
    while(uart_receive_data(DEBUG_UART_NUM, &c, 1))
    {
        pipe->AppendByte(c);
    }

    return 0;
}

static uint8_t data alignas(MessageDebugData) [sizeof(MessageDebugData)];
static MessageDebugData* ptr = nullptr;
//Сообщения, которые приходят по Debug Uart
void MessageDebugInit(int buffer_size, uint32_t baud_rate)
{
    ptr = new((void*)data) MessageDebugData(buffer_size);
    uart_configure(DEBUG_UART_NUM, baud_rate, UART_BITWIDTH_8BIT, UART_STOP_1, UART_PARITY_NONE);
    uart_irq_register(DEBUG_UART_NUM, UART_RECEIVE, KeyboardIrqCallback, &ptr->keyboard_pipe, 1);
}

void MessageDebugQuant()
{
    uint8_t* data;
    uint32_t size;
    ptr->keyboard_pipe.GetBuffer(data, size);
    ptr->keyboard_parse.Append((uint8_t*)data, size);
}

TBMessage MessageDebugNext()
{
    return ptr->keyboard_parse.NextMessage();
}
