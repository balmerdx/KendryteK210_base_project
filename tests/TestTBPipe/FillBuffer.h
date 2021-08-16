#pragma once
#include <stdint.h>
#include <vector>
#include "TBPipe.h"

class FillBuffer
{
public:
    FillBuffer(uint32_t buffer_size);
    ~FillBuffer();

    void SetEndingCRLF(bool ending_cr_lf) { this->ending_cr_lf = ending_cr_lf; }
    void Print(const char *__restrict fmt, ...) __attribute__((format (printf, 2, 3)));
    void AddBinary(const uint8_t* data, uint32_t size);
    void AddStringAsBinary(const char* data);

    void Clear();

    const std::vector<TBMessage>& Messages() { return messages; }

    uint8_t* Data() { return buffer;}
    uint32_t DataSize() { return buffer_pos; }

    void SetEsp8266BinaryFormat(bool esp8266_binary_format) { this->esp8266_binary_format = esp8266_binary_format; }
    void SetEsp8266LinkId(int link_id) { esp8266_link_id = link_id; }
    int Esp8266LinkId() const { return esp8266_link_id; }
protected:
    void AddData(const uint8_t* data, uint32_t size);
    void AddMessage(const uint8_t* data, uint32_t size, bool text);

    uint8_t* buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos = 0;

    uint8_t* temp_buffer;
    uint32_t temp_buffer_size;
    bool ending_cr_lf = true;
    bool esp8266_binary_format = false;
    int esp8266_link_id = 1;

    std::vector<TBMessage> messages;
};
