#pragma once
#include <stdint.h>

//Перед этой функцией надо сконфигурировать cs,clk,mosi pins
//Пример 
//    int SPI_SLAVE_CS_PIN = 15;
//    int SPI_SLAVE_CLK_PIN = 13;
//    int SPI_SLAVE_MOSI_PIN = 14;
//    fpioa_set_function(SPI_SLAVE_CS_PIN, FUNC_SPI_SLAVE_SS);
//    fpioa_set_function(SPI_SLAVE_CLK_PIN, FUNC_SPI_SLAVE_SCLK);
//    fpioa_set_function(SPI_SLAVE_MOSI_PIN, FUNC_SPI_SLAVE_D0);
void spi2_slave_config();

//Принимает данные только по 4 байта
//rx_len количество принимаемых байтов, должно быть кратно 4-м
//return false if timeout
bool spi2_slave_receive4(uint8_t* rx_buff, uint32_t rx_len, uint64_t timeout_us);
