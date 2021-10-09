#include <stdio.h>
#include <stdlib.h>
#include <sysctl.h>
#include <gpiohs.h>
#include <dvp.h>
#include <fpioa.h>
#include <iomem.h>
#include <sleep.h>
#include "main.h"
#include "camera/gc0328/include/gc0328.h"
#include "camera/gc0328/include/cambus.h"

#include "esp32/esp32_spi.h"
#include "wifi_passw.h"

uint8_t last_socket;
void NetInit();
//Клиент подсоединился к серверу
bool NetConnected();
void NetSend(const void* addr, uint32_t bytes_size);

void TestSquares(uint16_t* out, int width, int height, int div = 16)
{
    uint16_t c0 = 0;
    uint16_t c1 = 0x3F<<5;

    for(int y=0; y<height; y++)
    for(int x=0; x<width; x++)
    {
        uint16_t c = c0;
        
        if(((x/div)+(y/div))&1)
        {
            c = c1;
        }
        out[x+y*width] = c;
    }
}


//По хорошему перенумеровку GPIO надо бы собрать в одном месте
//Иначе будут потом ошибки
#define DCX_GPIONUM             (2)
#define RST_GPIONUM             (0)

volatile bool g_dvp_finish_flag = false;

static int dvp_irq(void *ctx)
{
    if (dvp_get_interrupt(DVP_STS_FRAME_FINISH))
    {
        dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
        dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
        g_dvp_finish_flag = true;
    }
    else
    {
        dvp_start_convert();
        dvp_clear_interrupt(DVP_STS_FRAME_START);
    }
    return 0;
}

static void io_mux_init(void)
{
    /* Init DVP IO map and function settings */
    fpioa_set_function(42, FUNC_CMOS_RST);
    fpioa_set_function(44, FUNC_CMOS_PWDN);
    fpioa_set_function(46, FUNC_CMOS_XCLK);
    fpioa_set_function(43, FUNC_CMOS_VSYNC);
    fpioa_set_function(45, FUNC_CMOS_HREF);
    fpioa_set_function(47, FUNC_CMOS_PCLK);
    fpioa_set_function(41, FUNC_SCCB_SCLK);
    fpioa_set_function(40, FUNC_SCCB_SDA);

    /* Init SPI IO map and function settings */
    fpioa_set_function(38, (fpioa_function_t)(FUNC_GPIOHS0 + DCX_GPIONUM));
    fpioa_set_function(36, FUNC_SPI0_SS3);
    fpioa_set_function(39, FUNC_SPI0_SCLK);
    fpioa_set_function(37, (fpioa_function_t)(FUNC_GPIOHS0 + RST_GPIONUM));

    sysctl_set_spi0_dvp_data(1);
}

static void io_set_power(void)
{
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}

void camera_test()
{
    printf("Camera test\n");
    //Совсем небольшое повышение частоты, с 390 МГц до 403 МГц
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_clock_enable(SYSCTL_CLOCK_AI);
    io_set_power();
    io_mux_init();
    NetInit();

    uint32_t width = 320;
    uint32_t height = 240;
    uint32_t bpp_display = 2;
    uint32_t bpp_ai = 3;
    uint32_t image_bytes_size = width * height * bpp_display;
    uint32_t ai_bytes_size = width * height * bpp_ai;

    bool send_ai = true;

    void* image_addr = iomem_malloc(image_bytes_size);
    void* ai_addr = iomem_malloc(ai_bytes_size);
    void* image_to_net_addr = malloc(send_ai?ai_bytes_size:image_bytes_size);

    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    dvp_set_output_enable(DVP_OUTPUT_AI, 1);
    dvp_set_output_enable(DVP_OUTPUT_DISPLAY, 1);
    dvp_set_image_format(DVP_CFG_YUV_FORMAT);
    dvp_set_image_size(width, height);
    cambus_init(8, 2, 41, 40, 0, 0); //DVP SCL(41) SDA(40) pin ->software i2c

    gc0328_reset();
    gc0328_init();
    gc0328_set_framesize(FRAMESIZE_QVGA);

    dvp_set_display_addr((uint32_t)(uint64_t)image_addr);
    dvp_set_ai_addr((uint32_t)(uint64_t)ai_addr, (uint32_t)((uint64_t)ai_addr + 320 * 240), (uint32_t)((uint64_t)ai_addr + 320 * 240 * 2));
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
    dvp_disable_auto();
    /* DVP interrupt config */
    printf("DVP interrupt config\n");
    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, dvp_irq, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);

    printf("System start\n");
    uint64_t time_last = sysctl_get_time_us();
    uint64_t time_now = sysctl_get_time_us();
    int time_count = 0;
    while (1)
    {
        g_dvp_finish_flag = false;
        dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
        dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);
        while (!g_dvp_finish_flag)
            ;

        if(NetConnected())
        {
            if(send_ai)
            {
               
                memcpy(image_to_net_addr, ai_addr, ai_bytes_size);
                NetSend(image_to_net_addr, ai_bytes_size);
            } else
            {
                memcpy(image_to_net_addr, image_addr, image_bytes_size);
                //TestSquares((uint16_t*)image_to_net_addr, 320, 240, 1);
                NetSend(image_to_net_addr, image_bytes_size);
            }
        }


        time_count ++;
        if(time_count % 100 == 0)
        {
            time_now = sysctl_get_time_us();
            printf("SPF:%fms \n", (time_now - time_last)/1000.0/100);
            time_last = time_now;
        }
    }
}

void NetInit()
{
    printf( "NetInit start server\n");
    esp32_spi_init();
    esp32_spi_reset();
    //esp32_set_debug(true);
    while(!esp32_spi_connect_AP(SSID, PASS, 10))
    {
        printf("Connecting to AP %s\n", (const char*)SSID);
    }
    printf("Connected\n");

    uint16_t port = 5001;
    if(!esp32_spi_server_create(port))
    {
        printf("NetSend cannot create server\n");
        while(1);
    }

    last_socket = esp32_spi_bad_socket();
}

bool NetConnected()
{
    bool is_server_alive;
    uint8_t socket = esp32_spi_server_accept(&is_server_alive);
    if(!is_server_alive)
    {
        printf("NetConnected Server not alive\n");
        esp32_spi_server_stop();
        return false;
    }

    last_socket = socket;

    return last_socket != esp32_spi_bad_socket();
}

void NetSend(const void* addr, uint32_t bytes_size)
{
    if(last_socket==esp32_spi_bad_socket())
        return;

    const uint8_t* addr8 = (const uint8_t*)addr;
    uint32_t offset = 0;

    uint64_t timeout_us = 1000*1000;
    uint64_t time_start = sysctl_get_time_us();

    while(offset < bytes_size)
    {
        uint16_t len = esp32_spi_max_write_size();
        uint32_t len_to_end = bytes_size - offset;
        if(len_to_end < len)
            len = len_to_end;
        bool is_client_alive;
        uint16_t sended_bytes = esp32_spi_socket_write(last_socket, addr8+offset, len, &is_client_alive);

        if(!is_client_alive)
        {
            printf("NetSend is_client_alive==false\n");
            break;
        }
        offset += sended_bytes;
        usleep(300);

        uint64_t time_now = sysctl_get_time_us();

        if(time_now-time_start > timeout_us)
            break;
    }

    if(false)
    {
        while(1)
        {
            bool is_client_alive;
            uint8_t buf[1];
            uint16_t count = esp32_spi_socket_read(last_socket, buf, sizeof(buf), &is_client_alive);
            if(!is_client_alive)
                break;
            if(count>0)
                break;
            usleep(300);
        }
        printf("NetSend byte received\n");
    }

    esp32_spi_socket_close(last_socket);
    last_socket = esp32_spi_bad_socket();
}