#pragma once
/*
Этот класс предназначен для того, что-бы принимать данные по UART либо WiFi в текстовой и бинарной форме.
1. Данные принимаются в прерывании по байтикам.
   Возможно в будущем сделаю, чтобы бинарные данные принимались по DMA,
   но это усложнит код. Тогда потребуется отдельно подписываться на таймер и следить за timeout.
2. Изначально система находится в режиме приёма текстовых данных.
   Когда приходит \n это признак того, что строчка завершилась и её надо обрабатывать.
   \r игнорируется.

   Переключение в бинарный режим происходит по префиксу.
   Для своих целей буду использовать префикс 0x00, 0x01, size_hi, size_lo (всегда четыре байта байта).
   Но надо поддержать и префикс вида '\n+IPD,0,149:' Он произвольной длинны. Начинается с \n+IPD, и заканчивается :.
   Потом идут бинарные данные. Префикс всегда начинается либо после \n, либо после другого пакета бинарных данных.
*/

#include "TBPipeStruct.h"

class BinPrefixParser
{
public:
    enum class Result
    {
        //Префикс не найден
        NotMatched,
        //Префикс найден и интерпретацию текстовых данных надо прекратить.
        PrefixMatched,
        //Префикс полностью прочитан, пора читать бинарные данные.
        PrefixCompleted
    };
    
    //Возвращает true, если префикс нашёлся в потоке команд.
    //data - символ, который пришёл по Uart/WiFi
    //is_first - true если это первый элемент в строке
    virtual Result Parse(uint8_t data, bool is_first) = 0;
    //Возвращает - количество байт, которые надо прочитать в бинарном формате.
    //Валидно только в момент, когда Parse вернул PrefixCompleted
    virtual uint32_t PacketSize() = 0;
};

class TBPipe
{
public:
    //Вариант, когда используя malloc создаются буфера.
    //Общий размер выделяемой памяти buffer_size*2
    TBPipe(int buffer_size);
    ~TBPipe();
    //Вызывается в прерывании
    void AppendByte(uint8_t byte);

    //!!! можно вызывать только из нулевого ядра!
    //Из того-же, где происходит прерывание.
    //
    void GetBuffer(uint8_t*& data, uint32_t& size, bool* overflow = nullptr);

    int BufferSize() const { return buffer_size; }
protected:
    int buffer_size;
    uint8_t* buffer;
    uint8_t* irq_buffer;
    uint8_t* user_buffer;

    uint32_t irq_buffer_pos = 0;
    uint32_t user_buffer_pos = 0;

    bool overflow = false;
};

class TBParse
{
public:
    TBParse(uint32_t buffer_size, BinPrefixParser* parser);
    ~TBParse();
    //Добавляем новые данные, желательно добавлять сразу большими кусками.
    //Если timeout_ms!=0 то очищаются старые данные и BinPrefixParser приводится в начальное состояние.
    void Append(uint8_t* data, uint32_t size);

    //В случае бинарных данных - это очередной пакет размером size
    //В случае текстовых данных - в конце, вместо \r\n будет символ 0
    TBMessage NextMessage();

    void SetParser(BinPrefixParser* p);

    void SetTimeout(uint32_t timeout_ms);
    uint32_t Timeout() const { return timeout_ms; }
protected:
    uint8_t* buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos = 0;
    uint32_t buffer_amount = 0;
    uint32_t bin_message_size = 0;
    uint32_t timeout_ms = 0;
    uint64_t last_append_time = 0;

    BinPrefixParser* parser = nullptr;
};

//Parsers
class Esp8266PrefixParser : public BinPrefixParser
{
public:
    Esp8266PrefixParser();
    //+IPD,<link ID>,<len>[,<remoteIP>,<remote port>]:
    Result Parse(uint8_t data, bool is_first) override;
    uint32_t PacketSize() override { return packet_size; }

    uint32_t LinkID() const { return link_id; }
protected:
    enum class State
    {
        Prefix,
        LinkID,
        Len,
        Ending,
        NotPassed
    };

    State state = State::Prefix;
    //Переменная имеет разный смысл, в зависимости от state
    int offset = 0;
    char num_buf[8];
    uint32_t link_id;
    uint32_t packet_size;
};

class StandartPrefixParser : public BinPrefixParser
{
public:
    StandartPrefixParser();
    //0x00, 0x01, size_hi, size_lo
    Result Parse(uint8_t data, bool is_first) override;
    uint32_t PacketSize() override { return data_size; }
protected:
    uint32_t data_size = 0;
    uint32_t found_idx = 0;
};
