#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <vector>
#include "transport/serial.h"
#include "transport/network_client.h"
#include "image_utils.h"

uint64_t time_us()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;    
}

//https://stackoverflow.com/questions/717572/how-do-you-do-non-blocking-console-i-o-on-linux-in-c
//https://web.archive.org/web/20170407122137/http://cc.byexamples.com/2007/04/08/non-blocking-user-input-in-loop-without-ncurses/
int kbhit()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void test_socket()
{
    NetworkClient client;
    if(!client.open("dl.sipeed.com", 80))
    {
        printf("Cannot open socket\n");
        return;
    }

    const char* buf = "GET /MAIX/MaixPy/assets/Alice.jpg HTTP/1.1\r\nHost: dl.sipeed.com\r\ncache-control: no-cache\r\n\r\n";
    size_t sended_bytes;
    if(!client.write(buf, strlen(buf), sended_bytes))
    {
        printf("Cannot send bytes");
        return;
    }
    
    size_t sum_size = 0;
    for(int i=0; i<100; i++)
    {
        uint8_t buffer[10000];
        size_t readed_bytes;
        if(!client.read(buffer, sizeof(buffer), readed_bytes))
        {
            printf("Cannot read bytes");
            return;
        }

        if(readed_bytes>0)
        {
            sum_size += readed_bytes;
            printf("%i: readed bytes=%lu\n", i, readed_bytes);
        }

        usleep(10000);   
    }

    printf("Read complete size=%lu\n", sum_size);
}

void test_image_from_socket()
{
    NetworkClient client;
    if(!client.open("192.168.1.33", 5001))
    {
        printf("Cannot open socket\n");
        return;
    }

    //int bpp = 3;
    int bpp = 2;
    
    size_t sum_size = 0;
    size_t width = 320;
    size_t height = 240;
    size_t req_size = width*height*bpp;
    std::vector<uint8_t> out;
    out.reserve(req_size);
    uint64_t start_us = time_us();
    while(sum_size<req_size)
    {
        uint8_t buffer[100000];
        size_t readed_bytes;
        if(!client.read(buffer, sizeof(buffer), readed_bytes))
        {
            printf("Cannot read bytes");
            return;
        }

        if(readed_bytes>0)
        {
            sum_size += readed_bytes;
            out.insert(out.end(), buffer, buffer+readed_bytes);
            printf("readed bytes=%lu\n", readed_bytes);
        }

        usleep(10000);   
    }

    if(false)
    {
        uint8_t buf = 3;
        size_t sended_bytes;
        client.write(&buf, 1, sended_bytes);
    }

    uint64_t end_us = time_us();

    float dt_seconds = (end_us-start_us)*1e-6f;
    printf("Read complete size=%lu\n", sum_size);
    printf("Read time=%f \n", dt_seconds);
    printf("Speed %f MB/sec\n", out.size()/dt_seconds*1e-6f);

    if(bpp==2)
    {
        if(true)
        {//swap pixels
            uint16_t* p = (uint16_t*)out.data();
            for(size_t i=0; i<width*height; i+=2)
            {
                std::swap(p[i], p[i+1]);
                
            }
        }

        WriteImage16("out.png", width, height, out.data());

    } else
    {
        std::vector<uint8_t> sw;
        sw.resize(out.size());
        uint8_t* o = (uint8_t*)sw.data();
        uint8_t* r = (uint8_t*)out.data();
        uint8_t* g = r + width*height;
        uint8_t* b = g + width*height;
        for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
        {
            o[0] = *r;
            o[1] = *g;
            o[2] = *b;
            r++; g++; b++;
            o += 3; 
        }

        WriteImage24("out.png", width, height, sw.data());
    }
}

int main()
{
    if(false)
    {
        std::vector<uint16_t> out;
        int width = 320;
        int height = 240;
        out.resize(320*240);
        uint16_t c0 = 0;
        uint16_t c1 = 0x3F<<5;

        for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
        {
            uint16_t c = c0;
            
            if(((x/16)+(y/16))&1)
            {
                c = c1;
            }
            out[x+y*width] = c;
        }

        WriteImage16("out1.png", width, 240, (const uint16_t*)out.data());
        return 0;
    }

    test_image_from_socket();
    return 0;

    char *line = NULL;
    size_t line_len = 0;
    ssize_t nread;

    SerialPort uart;

    if(!uart.open("/dev/ttyUSB0"))
    {
        printf("Cannot open serial port\n");
        return 1;
    }

    while(1)
    {
        usleep(1000);
        if(kbhit())
        {
            nread = getline(&line, &line_len, stdin);
            if(nread>0)
            {
                printf("s=%s", line);
            }
        }

        char buf[1000];
        ssize_t size = uart.read(buf, sizeof(buf)-1);
        if(size>0)
        {
            buf[size] = 0;
            printf("Received = %s\n", buf);
        }
    }

    free(line);

    return 0;
}