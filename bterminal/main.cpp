#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <string.h>
#include <vector>
#include "transport/serial.h"
#include "transport/network_client.h"
#include "image_utils.h"

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
    
    size_t sum_size = 0;
    size_t width = 320;
    size_t height = 240;
    size_t req_size = width*height*2;
    std::vector<uint8_t> out;
    out.reserve(req_size);
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

    printf("Read complete size=%lu\n", sum_size);

    if(true)
    {//swap pixels
        uint16_t* p = (uint16_t*)out.data();
        for(size_t i=0; i<width*height; i+=2)
        {
            std::swap(p[i], p[i+1]);
            
        }
    }

    writeImage("out.png", width, height, (const uint16_t*)out.data());
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

        writeImage("out1.png", width, 240, (const uint16_t*)out.data());
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