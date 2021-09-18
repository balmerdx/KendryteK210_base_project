#include <stdio.h>
#include <sysctl.h>
#include <gpiohs.h>
#include <dvp.h>
#include <fpioa.h>
#include <iomem.h>
#include "main.h"
#include "camera/gc0328/include/gc0328.h"
#include "camera/gc0328/include/cambus.h"

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
    io_set_power();
    io_mux_init();

    uint32_t width = 320;
    uint32_t height = 240;
    uint32_t bpp = 2;
    void* image_addr = iomem_malloc(width * height * bpp);

    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    //dvp_set_output_enable(DVP_OUTPUT_AI, 1);
    dvp_set_output_enable(DVP_OUTPUT_DISPLAY, 1);
    dvp_set_image_format(DVP_CFG_YUV_FORMAT);
    dvp_set_image_size(320, 240);
    cambus_init(8, 2, 41, 40, 0, 0); //DVP SCL(41) SDA(40) pin ->software i2c

    gc0328_reset();
    gc0328_init();
    //gc0328_set_framesize(FRAMESIZE_QVGA);

    dvp_set_display_addr((uint32_t)(uint64_t)image_addr);
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
        time_count ++;
        if(time_count % 100 == 0)
        {
            time_now = sysctl_get_time_us();
            printf("SPF:%fms \n", (time_now - time_last)/1000.0/100);
            time_last = time_now;
        }
    }
}