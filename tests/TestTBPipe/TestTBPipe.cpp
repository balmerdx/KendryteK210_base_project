#include <stdio.h>
#include <stdlib.h> 
#include <stdarg.h>
#include <memory.h>
#include <typeinfo>
#include <vector>
#include <string>
#include "TBPipe.h"

struct PrefixFeedResult
{
    uint32_t offset;
    uint32_t size;
    bool matched;

    bool operator==(const PrefixFeedResult& r)
    {
        return offset==r.offset && size==r.size && matched==r.matched;
    }

    bool operator!=(const PrefixFeedResult& r) { return !(*this==r);}
};

PrefixFeedResult PrefixFeed(BinPrefixParser* parser, const char* str, int size)
{
    PrefixFeedResult r = {};
    const char* pend = str+size;
    for(const char* p = str; p!=pend; p++)
    {
        BinPrefixParser::Result result = parser->Parse(*p, p==str);
        if(result==BinPrefixParser::Result::PrefixCompleted)
        {
            r.matched = true;
            r.offset = (uint32_t)(p-str);
            r.size = parser->PacketSize();
            break;
        }
    }

    return r;
}


void TestFeed(Esp8266PrefixParser* parser, const char* str, PrefixFeedResult expected = {}, int expected_link_id = 0)
{
    PrefixFeedResult out = PrefixFeed(parser, str, strlen(str));
    if(out!=expected)
    {
        printf("Error!\n");
        printf("str='%s'\n", str);
        //printf("class=%s\n", typeid(*parser).name());
        printf("class=Esp8266PrefixParser\n");
        printf("out.size=%u expected.size=%u\n", out.size, expected.size);
        printf("out.offset=%u expected.offset=%u\n", out.offset, expected.offset);
        printf("out.matched=%s expected.matched=%s\n", out.matched?"True":"False", expected.matched?"True":"False");
        exit(1);
    }

    if(out.matched && parser->LinkID() != expected_link_id)
    {
        printf("Error!\n");
        printf("str='%s'\n", str);
        printf("class=Esp8266PrefixParser\n");
        printf("out.link_id=%u expected.link_id=%u\n", parser->LinkID(), expected_link_id);
        exit(1);
    }

    printf("Test '%s' --passed.\n", str);
}

void TestFeed(StandartPrefixParser* parser, const char* str, PrefixFeedResult expected = {})
{
    char str_hex[100];
    snprintf(str_hex, sizeof(str_hex), "uint16 %u %u", *(uint16_t*)str, *(uint16_t*)(str+2));
    PrefixFeedResult out = PrefixFeed(parser, str, 6);
    if(out!=expected)
    {
        printf("Error!\n");
        printf("%s\n", str_hex);
        //printf("class=%s\n", typeid(*parser).name());
        printf("class=StandartPrefixParser\n");
        printf("out.size=%u expected.size=%u\n", out.size, expected.size);
        printf("out.offset=%u expected.offset=%u\n", out.offset, expected.offset);
        printf("out.matched=%s expected.matched=%s\n", out.matched?"True":"False", expected.matched?"True":"False");
        exit(1);
    }

    printf("Test '%s' --passed.\n", str_hex);
}

void TestEsp8266PrefixParser()
{
    Esp8266PrefixParser parser;
    PrefixFeedResult result;
    
    printf("Start testing Esp8266PrefixParser\n");
    //+IPD,<link ID>,<len>[,<remoteIP>,<remote port>]:
    TestFeed(&parser, "+IPD,1,132:assdas", {10, 132, true}, 1);
    TestFeed(&parser, "+IPD,7,22:xxx", {9, 22, true}, 7);
    TestFeed(&parser, "+IPD,2,790,\"192.168.1.1\",22:rrr", {27, 790, true}, 2);

    TestFeed(&parser, "+IPD,2,790");
    TestFeed(&parser, "XXXYYYZZZAAA");
    TestFeed(&parser, "\n123+IPD,1,132:assdas");
}

void TestStandartPrefixParser()
{
    StandartPrefixParser parser;
    PrefixFeedResult result;
    uint16_t data[3] = {};

    printf("Start testing StandartPrefixParser\n");
    data[0] = 0x0100;
    data[1] = 132;
    TestFeed(&parser, (char*)data, {3, 132, true});

    data[0] = 0x0100;
    data[1] = 2000;
    TestFeed(&parser, (char*)data, {3, 2000, true});

    data[0] = 0x0000;
    data[1] = 2000;
    TestFeed(&parser, (char*)data, {0, 0, false});

    TestFeed(&parser, "+IPD,2,790", {0, 0, false});
}

void TestTBPipe()
{
    int buffer_size = 300;
    TBPipe pipe(buffer_size);

    std::vector<uint8_t> data;

    printf("TestTBPipe started\n");
    for(int sz=0; sz<=buffer_size; sz++)
    {
        data.resize(sz);
        for(size_t i=0; i<sz; i++)
        {
            data[i] = rand();
            pipe.AppendByte(data[i]);
        }

        uint8_t* out_data;
        uint32_t out_size;
        bool overflow;
        pipe.GetBuffer(out_data, out_size, &overflow);

        if(sz!=out_size)
        {
            printf("Error! in_size=%i out_size=%u\n", sz, out_size);
            exit(1);
        }

        if(overflow)
        {
            printf("Error! Unexpected overflow. size=%i\n", sz);
            exit(1);
        }

        for(int i=0; i<sz; i++)
        {
            if(data[i]!=out_data[i])
            {
                printf("Error! Data not mathced size=%i idx=%i in=%i out=%i\n", sz, i, (int)data[i], (int)out_data[i]);
                exit(1);
            }
        }
    }

    printf("TestTBPipe buffer normal --passed\n");

    for(int sz=buffer_size+1; sz<buffer_size+30; sz++)
    {
        data.resize(sz);
        for(size_t i=0; i<sz; i++)
        {
            data[i] = rand();
            pipe.AppendByte(data[i]);
        }

        uint8_t* out_data;
        uint32_t out_size;
        bool overflow;
        pipe.GetBuffer(out_data, out_size, &overflow);

        if(out_size!=buffer_size)
        {
            printf("Error! in_size=%i out_size=%u\n", sz, out_size);
            exit(1);
        }

        if(!overflow)
        {
            printf("Error! Unexpected overflow==false. size=%i\n", sz);
            exit(1);
        }

        for(int i=0; i<out_size; i++)
        {
            if(data[i]!=out_data[i])
            {
                printf("Error! Data not mathced size=%i idx=%i in=%i out=%i\n", sz, i, (int)data[i], (int)out_data[i]);
                exit(1);
            }
        }
    }

    printf("TestTBPipe buffer overflow --passed\n");
}

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
protected:
    void AddData(const uint8_t* data, uint32_t size);
    void AddMessage(const uint8_t* data, uint32_t size, bool text);

    uint8_t* buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos = 0;

    uint8_t* temp_buffer;
    uint32_t temp_buffer_size;
    bool ending_cr_lf = true;

    std::vector<TBMessage> messages;
};

FillBuffer::FillBuffer(uint32_t buffer_size)
    : buffer_size(buffer_size)
    , temp_buffer_size(buffer_size)
{
    buffer = new uint8_t[buffer_size];
    temp_buffer = new uint8_t[temp_buffer_size];
}

FillBuffer::~FillBuffer()
{
    Clear();
    delete[] buffer;
    delete[] temp_buffer;

}

void FillBuffer::Clear()
{
    for(TBMessage& m : messages)
        delete[] m.data;
    messages.clear();
    buffer_pos = 0;
}

void FillBuffer::AddData(const uint8_t* data, uint32_t size)
{
    if(buffer_pos>=buffer_size)
        return;
    uint32_t remain_size = buffer_size - buffer_pos;
    size = std::min(remain_size, size);
    memcpy(buffer+buffer_pos, data, size);
    buffer_pos += size;
}

void FillBuffer::Print(const char *__restrict fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    int ret = vsnprintf((char*)(temp_buffer), temp_buffer_size, fmt, arg);
    if(ret>0)
    {
        AddMessage(temp_buffer, ret, true);
        uint32_t rest_size = temp_buffer_size - ret;
        if(ending_cr_lf)
            ret += snprintf((char*)(temp_buffer+ret), rest_size, "\r\n");
        else
            ret += snprintf((char*)(temp_buffer+ret), rest_size, "\n");

        AddData(temp_buffer, ret);
    }
    va_end(arg);
}

void FillBuffer::AddBinary(const uint8_t* data, uint32_t size)
{
    uint16_t p16[2];
    p16[0] = 0x0100;
    p16[1] = size;
    AddData((uint8_t*)p16, 4);
    AddData(data, size);
    AddMessage(data, size, false);
}

void FillBuffer::AddStringAsBinary(const char* data)
{
    AddBinary((const uint8_t*) data, strlen(data)+1);
}

void FillBuffer::AddMessage(const uint8_t* data, uint32_t size, bool text)
{
    TBMessage message;
    message.is_text = text;
    message.size = size;
    if(text)
    {
        message.data = new uint8_t[size+1];
        message.data[size] = 0;
    }
    else
    {
        message.data = new uint8_t[size+1];
    }

    memcpy(message.data, data, size);
    messages.push_back(message);
}

void CheckMessage(const TBMessage& orig, const TBMessage& received)
{
    bool data_matched = false;
    if(orig.size == received.size)
        data_matched = memcmp(orig.data, received.data, orig.size)==0;

    if(orig.is_text != received.is_text || !data_matched)
    {
        printf("Message not matched!\n");
        printf("orig is_text=%s, received is_text=%s\n",
            orig.is_text?"True":"False", received.is_text?"True":"False");
        printf("orig size=%u received size=%u\n", orig.size, received.size);
        if(orig.is_text)
        {
            std::string s((char*)orig.data, orig.size);
            printf("orig='%s'\n", s.c_str());
        }
        if(received.is_text)
        {
            std::string s((char*)received.data, received.size);
            printf("rciv='%s'\n", s.c_str());
        }

        exit(1);
    }
}

void CheckMessages(TBParse& parse, FillBuffer& fill)
{
    const std::vector<TBMessage>& orig_messages = fill.Messages();
    TBMessage message;
    for(size_t i=0; i<orig_messages.size(); i++)
    {
        message = parse.NextMessage();
        CheckMessage(orig_messages[i], message);
    }

    message = parse.NextMessage();
    if(message.size != 0 || message.data != nullptr)
    {
        printf("Error. Extra message!");
        exit(1);
    }

}

void TestTBParse()
{
    printf("TestTBParse started\n");

    const uint32_t buffer_size = 100;
    TBParse parse(buffer_size);
    FillBuffer fill(buffer_size*5);
    TBMessage message;

    printf("Single text message ");
    fill.Print("Hello");
    parse.Append(fill.Data(), fill.DataSize());
    CheckMessages(parse, fill);
    printf("--passed\n");

    printf("Multiple text message ");
    fill.Clear();
    fill.Print("Hello");
    fill.Print("Hello2");
    fill.Print("Hello long string, looong loong string");
    parse.Append(fill.Data(), fill.DataSize());
    CheckMessages(parse, fill);
    printf("--passed\n");

    printf("Single bin message ");
    std::string str;
    for(int i=0; i<50; i++)
    {
        printf(".");
        str.append(1, '0'+i%40);
        fill.Clear();
        //Добавить тут тест на разные длины сообщений (т.к. при длинне сообщения 10 и 13 были ошибки)
        fill.AddStringAsBinary(str.c_str());
        parse.Append(fill.Data(), fill.DataSize());
        CheckMessages(parse, fill);
    }
    printf("--passed\n");

    printf("Multiple bin message ");
    fill.Clear();
    fill.AddStringAsBinary("Binary message");
    fill.AddStringAsBinary("Next message");
    fill.AddStringAsBinary("Third message");
    parse.Append(fill.Data(), fill.DataSize());
    CheckMessages(parse, fill);
    printf("--passed\n");

    printf("Mix bin/text messages ");
    fill.Clear();
    fill.AddStringAsBinary("Binary message");
    fill.Print("Hello text");
    fill.AddStringAsBinary("Next binary");
    fill.Print("Hello text2");
    fill.AddStringAsBinary("Third binary");
    parse.Append(fill.Data(), fill.DataSize());
    CheckMessages(parse, fill);
    printf("--passed\n");

    //Добавить тут тест частично помещающиеся в буфер сообщения.
}


int main()
{
    TestEsp8266PrefixParser();
    TestStandartPrefixParser();
    TestTBPipe();
    TestTBParse();
    return 0;
}