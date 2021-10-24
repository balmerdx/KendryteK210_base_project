#pragma once
#include <stdint.h>

struct TBinMessage
{
    uint8_t* data;
    uint32_t size;
    uint16_t command;
};


class TBinParse
{
public:
    explicit TBinParse(uint32_t buffer_size);
    ~TBinParse();

    uint32_t MaxAppendSize() const { return buffer_size-buffer_amount; }
    uint32_t BytesInBuffer() const { return buffer_amount; }
    void Append(uint8_t* data, uint32_t size);
    TBinMessage NextMessage();

    //Выставляется если не сошлось CRC либо размер команды больше, чем размер буфера
    bool ParseFailed() const { return parse_failed; }
    //
    void Reset();
protected:
    bool ParseHeader(uint64_t header);

    uint8_t* buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos = 0;
    uint32_t buffer_amount = 0;
    uint32_t bin_message_size = 0;
    uint16_t bin_message_command = 0;

    bool parse_failed = false;
};

//Не экономим на размерах! Пускай data_size будет 4 байта.
//packet_id - 2 байта и ещё 2 байта crc
uint64_t TBinFillHeader(uint16_t packet_id, uint32_t data_size);
