#include <stdio.h>
#include <stdlib.h> 
#include <memory.h>
#include <assert.h>
#include <typeinfo>
#include <vector>
#include <string>

#include "TBinParse.h"

class BinBuffer
{
public:
    BinBuffer(uint32_t buffer_size = 0x20000);
    ~BinBuffer();

    void Clear();
    uint8_t* Data() { return buffer.data();}
    uint32_t DataSize() { return (uint32_t)buffer.size(); }
    const std::vector<TBinMessage>& Messages() const { return messages; }

    void AddRawData(const void* data, uint32_t data_size);
    void AddPacket(uint16_t command, const void* data, uint32_t data_size);
    void AddPacket(uint16_t command, const std::vector<uint8_t>& data);
protected:    
    std::vector<uint8_t> buffer;
    std::vector<TBinMessage> messages;
};

BinBuffer::BinBuffer(uint32_t buffer_size)
{
    buffer.reserve(buffer_size);
}

BinBuffer::~BinBuffer()
{
    Clear();
}

void BinBuffer::Clear()
{
    buffer.clear();

    for(const TBinMessage& m : messages)
        free(m.data);
    messages.clear();
}

void BinBuffer::AddRawData(const void* data, uint32_t data_size)
{
    const uint8_t* data8 = (const uint8_t*)data;
    buffer.insert(buffer.end(), data8, data8+data_size);
}

void BinBuffer::AddPacket(uint16_t command, const void* data, uint32_t data_size)
{
    uint64_t header = TBinFillHeader(command, data_size);
    AddRawData(&header, sizeof(uint64_t));
    AddRawData(data, data_size);

    TBinMessage m;
    m.command = command;
    m.data = (uint8_t*)malloc(data_size);
    m.size = data_size;
    memcpy(m.data, data, data_size);
    messages.push_back(m);
}

void BinBuffer::AddPacket(uint16_t command, const std::vector<uint8_t>& data)
{
    AddPacket(command, data.data(), (uint32_t)data.size());
}

static void Fill(std::vector<uint8_t>& data, uint32_t size, uint8_t first_value = 1)
{
    data.resize(size);
    for(uint32_t i=0; i<size; i++)
        data[i] = (uint8_t)(i + first_value);
}

void CheckMessage(const TBinMessage& orig, const TBinMessage& received)
{
    bool data_matched = false;
    bool not_matched_filled = false;
    uint32_t not_matched_offset = 0;
    if(orig.size == received.size)
    {
        for(uint32_t i = 0; i < orig.size; i++)
        {
            if(orig.data[i] != received.data[i])
            {
                not_matched_filled = true;
                not_matched_offset = i;
                break;
            }
        }

        data_matched = !not_matched_filled;
    }

    if(orig.command != received.command || !data_matched)
    {
        printf("Message not matched!\n");
        printf("orig command=%i, received command=%i\n",
            (int)orig.command, (int)received.command);
        printf("orig size=%u received size=%u\n", orig.size, received.size);
        
        if(not_matched_filled)
        {
            printf("orig[%u]=%i received[%u]=%i\n",
                not_matched_offset, (int)orig.data[not_matched_offset],
                not_matched_offset, (int)received.data[not_matched_offset]
                );
        }

        exit(1);
    }
}


void TestTBinParseSingle()
{
    printf("TestTBinParseSingle\n");

    for(int size=0; size<1020; size+=4)
    {
        BinBuffer buf;
        TBinParse parse(1024);
        
        std::vector<uint8_t> data;
        Fill(data, size, 100);
        buf.AddPacket(3, data);

        parse.Append(buf.Data(), buf.DataSize());

        TBinMessage received = parse.NextMessage();
        CheckMessage(buf.Messages()[0], received);
    }

    printf("TestTBinParseSingle --passed\n");
}

void TestTBinParseBigMessage()
{
    printf("TestTBinParseBigMessage\n");
    BinBuffer buf;
    TBinParse parse(1024);
    
    std::vector<uint8_t> data;
    Fill(data, 2000, 1);
    buf.AddPacket(3, data);

    parse.Append(buf.Data(), 100);

    TBinMessage received = parse.NextMessage();
    if(received.size!=0 || !parse.ParseFailed())
    {
        printf("TestTBinParseBigMessage --failed\n");
        exit(1);
    }

    printf("TestTBinParseBigMessage --passed\n");
}

void TestTBinParseBadCRC()
{
    printf("TestTBinParseBadCRC\n");
    
    for(int bit_idx = 0; bit_idx < 64; bit_idx++)
    {
        BinBuffer buf;
        TBinParse parse(1024);
        std::vector<uint8_t> data;
        Fill(data, 32, 100);
        buf.AddPacket(3, data);

        uint64_t& header = *(uint64_t*)buf.Data();
        header = header ^ (1ull<<bit_idx);

        parse.Append(buf.Data(), buf.DataSize());

        TBinMessage received = parse.NextMessage();
        if(received.size!=0 || !parse.ParseFailed())
        {
            printf("TestTBinParseBadCRC --failed\n");
            exit(1);
        }
    }

    printf("TestTBinParseBadCRC --passed\n");
}

void TestTBinParseMultipleMessages()
{
    //Три сообщения подряд. Причем второе сообщение будем увеличивать по размерам,
    //Чтобы пройти все варианты - от двух.
    printf("TestTBinParseMultipleMessages\n");

    const uint32_t buf_size = 250;
    for(uint32_t size=0; size<buf_size-8; size+=4)
    {
        BinBuffer buf;
        TBinParse parse(buf_size);
        
        std::vector<uint8_t> data;
        Fill(data, 32);
        buf.AddPacket(3, data);
        Fill(data, size);
        buf.AddPacket(4, data);
        Fill(data, 24);
        buf.AddPacket(5, data);

        uint32_t append_offset = 0;
        uint32_t message_idx = 0;
        const std::vector<TBinMessage>& orig = buf.Messages();

        while(1)
        {
            uint32_t append_size = std::min(parse.MaxAppendSize(), buf.DataSize()-append_offset);
            parse.Append(buf.Data()+append_offset, append_size);
            append_offset += append_size;

            while(1)
            {
                TBinMessage received = parse.NextMessage();
                if(received.command == 0)
                    break;
                if(message_idx >= orig.size())
                {
                    printf("Extra message idx=%u\n", message_idx);
                    exit(1);
                }

                CheckMessage(orig[message_idx], received);
                message_idx++;
            }
    
            if(append_size==0)
                break;
        }

        if(parse.BytesInBuffer())
        {
            printf("Buffer not empty size=%u\n", parse.BytesInBuffer());
            exit(1);
        }
    }


    printf("TestTBinParseMultipleMessages --passed\n");
}

void TestTBinParse()
{
    TestTBinParseSingle();
    TestTBinParseBigMessage();
    TestTBinParseBadCRC();
    TestTBinParseMultipleMessages();
}