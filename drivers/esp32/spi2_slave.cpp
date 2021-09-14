#include <sysctl.h>
#include <spi.h>

#include "spi2_slave.h"

bool spi2_slave_receive4(uint8_t* rx_buff, uint32_t rx_len, uint64_t timeout_us)
{
    uint32_t* rx_buff32 = (uint32_t*)rx_buff;
    uint64_t start_cycle = read_cycle();
    uint64_t timeout_cycles = timeout_us * (sysctl_clock_get_freq(SYSCTL_CLOCK_CPU)/1000000);
    volatile spi_t *spi_handle = spi[2];
    size_t i = 0;
    size_t v_rx_len = rx_len/4;
    while(v_rx_len)
    {
        size_t fifo_len = spi_handle->rxflr;
        fifo_len = fifo_len < v_rx_len ? fifo_len : v_rx_len;
        for(size_t index = 0; index < fifo_len; index++)
            rx_buff32[i++] = spi_handle->dr[0];
        v_rx_len -= fifo_len;

        if((read_cycle()-start_cycle)>timeout_cycles)
        {
            return false;
        }
    }

    //Swap endian
    for(size_t i=0; i<rx_len; i++)
    {
        rx_buff32[i] = __builtin_bswap32(rx_buff32[i]);
    }

    return true;
}

void spi2_slave_config()
{
    size_t data_bit_length = 32;
    uint8_t work_mode = 6;
    uint8_t slv_oe = 10;
    uint8_t dfs = 16;
    sysctl_reset(SYSCTL_RESET_SPI2);
    sysctl_clock_enable(SYSCTL_CLOCK_SPI2);
    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_SPI2, 0);

    uint32_t data_width = data_bit_length / 8;
    volatile spi_t *spi_handle = spi[2];
    spi_handle->ssienr = 0x00;
    spi_handle->ctrlr0 = (0x0 << work_mode) | (0x1 << slv_oe) | ((data_bit_length - 1) << dfs);
    spi_handle->dmatdlr = 0x04;
    spi_handle->dmardlr = 0x03;
    spi_handle->dmacr = 0x00;
    spi_handle->txftlr = 0x00;
    spi_handle->rxftlr = 0x08 / data_width - 1;
    spi_handle->imr = 0x00;
    spi_handle->ssienr = 0x01;
    //spi_handle->endian = 0; //endian not working!!!
}
