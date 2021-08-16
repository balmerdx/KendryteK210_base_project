#include <stdio.h>
#include <stdlib.h> 
#include <memory.h>
#include <assert.h>
#include <typeinfo>
#include <vector>
#include <string>
#include "TBPipe.h"
#include "FillBuffer.h"

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

void CheckMessages(TBParse& parse, FillBuffer& fill, size_t start=0, size_t count=0)
{
    const std::vector<TBMessage>& orig_messages = fill.Messages();
    if(count==0)
    {
        if(orig_messages.size()<start)
            count = 0;
        else
            count = orig_messages.size()-start;
    }

    TBMessage message;
    for(size_t i=start; i<start+count; i++)
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

int CheckAvailableMessages(TBParse& parse, FillBuffer& fill)
{
    const std::vector<TBMessage>& orig_messages = fill.Messages();
    int count = 0;
    while(1)
    {
        TBMessage message = parse.NextMessage();
        if(message.data == nullptr)
            break;
        CheckMessage(orig_messages[count], message);
        count++;
    }

    return count;
}

void TestTBParse(bool use_esp8266)
{
    if(use_esp8266)
        printf("TestTBParse Esp8266 started\n");
    else
        printf("TestTBParse standart started\n");

    Esp8266PrefixParser esp8266_parser;

    const uint32_t buffer_size = 100;
    TBParse parse(buffer_size);
    if(use_esp8266)
        parse.SetParser(&esp8266_parser);

    FillBuffer fill(buffer_size*5);
    fill.SetEsp8266BinaryFormat(use_esp8266);
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

    for(int is_binary2=0; is_binary2<2; is_binary2++)
    {
        if(is_binary2)
            printf("Binary cross boundary ");
        else
            printf("Text cross boundary ");
        for(int is_binary=0; is_binary<2; is_binary++)
        {
            printf(".");
            std::vector<uint8_t> message;
            int prefix_size = use_esp8266?10:4;
            for(int sz=buffer_size-30; sz<buffer_size-prefix_size; sz++)
            {
                fill.Clear();
                if(is_binary)
                {
                    message.resize(sz);
                    for(int i=0; i<sz; i++)
                        message[i] = rand();
                    fill.AddBinary(message.data(), message.size());
                } else
                {
                    message.resize(sz+1);
                    for(int i=0; i<sz; i++)
                        message[i] = '0'+rand()%40;
                    message[sz] = 0;
                    fill.Print("%s", (char*)message.data());
                }

                if(is_binary2)
                    fill.AddStringAsBinary("Binary message cross boundary");
                else
                    fill.Print("Binary message cross boundary");

                if(fill.DataSize()>buffer_size)
                {
                    parse.Append(fill.Data(), buffer_size);
                    //CheckMessages(parse, fill, 0, 1);
                    int count_checked = CheckAvailableMessages(parse, fill);
                    parse.Append(fill.Data()+buffer_size, fill.DataSize()-buffer_size);
                    CheckMessages(parse, fill, count_checked);
                } else
                {
                    CheckMessages(parse, fill);
                }
            }
        }
        printf("--passed\n");
    }
}

int main()
{
    TestEsp8266PrefixParser();
    TestStandartPrefixParser();
    TestTBPipe();
    TestTBParse(false);
    TestTBParse(true);
    return 0;
}