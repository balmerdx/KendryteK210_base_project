#pragma once

#include "TBPipe/TBPipeStruct.h"

/*
Надстройка над TBPipe/TBParse для приёма данных по debug uart.
Подписывается на прерывание, и заполняет буфер.
После вызова MessageDebugQuant можно вызывать MessageDebugNext
и вычитывать принятые сообщения.
*/
void MessageDebugInit(int buffer_size = 256, uint32_t baud_rate = 115200);
void MessageDebugQuant();
TBMessage MessageDebugNext();