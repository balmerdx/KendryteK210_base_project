/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
//balmer temp
#define CONFIG_CONSOLE_UART_NUM 0

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <esp32/rom/uart.h>

#include <driver/periph_ctrl.h>

#include <Arduino.h>

#include <SPIS.h>
#include <WiFi.h>

#include "CommandHandler.h"

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

bool debug = true;

uint8_t* commandBuffer;
uint8_t* responseBuffer;

void dumpBuffer(const char* label, uint8_t data[], int length) {
  printf("%s: ", label);

  int max_print_length = 32;
  int print_length = length;
  if(length > max_print_length)
    print_length = max_print_length;

  for (int i = 0; i < print_length; i++)
  {
    printf("%02x", data[i]);
    if(i%4==3)
      printf(" ");
  }

  if(length > max_print_length)
    printf(" total len=%i", length);

  printf("\r\n");
}

void setDebug(bool d) {
  debug = d;
  if(debug)
    printf("Debug enabled\n");
  else
    printf("Debug disabled\n");
}

void setupWiFi();

void setup() {
  setDebug(debug);

  // put SWD and SWCLK pins connected to SAMD as inputs
  pinMode(15, INPUT);
  pinMode(21, INPUT);

  pinMode(5, INPUT);
  if (debug)  ets_printf("*** WIFI ON\n");

  setupWiFi();
}

#define UNO_WIFI_REV2

void setupWiFi() {
  if (debug)  ets_printf("*** SPIS\n");
  SPIS.begin();
  
  commandBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);
  responseBuffer = (uint8_t*)heap_caps_malloc(SPI_BUFFER_LEN, MALLOC_CAP_DMA);

  if (debug)  ets_printf("*** BEGIN\n");
  CommandHandler.begin();
}

void loop() {
  //if (debug)  ets_printf(".");
  // wait for a command
  memset(commandBuffer, 0x00, SPI_BUFFER_LEN);
  int commandLength = SPIS.transfer(NULL, commandBuffer, SPI_BUFFER_LEN);
  if (debug)  ets_printf("%d", commandLength);
  if (commandLength == 0) {
    vTaskDelay(2 / portTICK_PERIOD_MS);
    return;
  }

  if (debug)
    dumpBuffer("COMMAND", commandBuffer, commandLength);
  //return;//Временно, для теста большой скорости

  // process
  memset(responseBuffer, 0x00, SPI_BUFFER_LEN);
  int responseLength = CommandHandler.handle(commandBuffer, responseBuffer);

  //test code
/*  
  int responseLength = commandLength;
  for(int i=0; i<commandLength; i++)
    //responseBuffer[i] = commandBuffer[i] ^ 0xFF;
    responseBuffer[i] = commandBuffer[i] + 1;
*/    

  SPIS.transfer(responseBuffer, NULL, responseLength);

  if (debug) {
    dumpBuffer("RESPONSE", responseBuffer, responseLength);
  }
}

static void init_nvs_flash()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

}

void TestPins()
{
  int pin1 = 14;
  int pin2 = 13;
  int pin3 = 15;

  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);
  pinMode(pin3, OUTPUT);

  uint8_t idx = 0;
  TickType_t tick = configTICK_RATE_HZ/2; //Half second

  while(1)
  {
    digitalWrite(pin1, (idx&1)?1:0);
    digitalWrite(pin2, (idx&2)?1:0);
    digitalWrite(pin3, (idx&4)?1:0);
    vTaskDelay(tick);
    idx++;
  }
}

#include <driver/spi_common.h>
#include <driver/spi_master.h>

void spi_master_send(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

void TestSpi()
{
  const int PIN_NUM_MISO = -1;
  const int PIN_NUM_MOSI = 13;
  const int PIN_NUM_CLK = 14;
  const int PIN_NUM_CS = 15;
  const spi_dma_chan_t DMA_CHAN = 2;

  esp_err_t ret;
  spi_device_handle_t spi;
  spi_bus_config_t buscfg={
      .mosi_io_num=PIN_NUM_MOSI,
      .miso_io_num=PIN_NUM_MISO,
      .sclk_io_num=PIN_NUM_CLK,
      .quadwp_io_num=-1,
      .quadhd_io_num=-1,
      .max_transfer_sz=0, //4094
      .flags = 0,
      .intr_flags = 0
  };
  spi_device_interface_config_t devcfg={
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode=0,                                //SPI mode 0
      .duty_cycle_pos = 0,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz=SPI_MASTER_FREQ_80M,
      //.clock_speed_hz=8*1000*1000,           //Clock out at 10 KHz
      .input_delay_ns = 0,
      .spics_io_num=PIN_NUM_CS,               //CS pin
      .flags = 0,
      .queue_size=7,                          //We want to be able to queue 7 transactions at a time
      .pre_cb = 0,
      .post_cb = 0
  };

  //Initialize the SPI bus
  ret=spi_bus_initialize(HSPI_HOST, &buscfg, DMA_CHAN);
  ESP_ERROR_CHECK(ret);
  //Attach the LCD to the SPI bus
  ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);

  TickType_t tick = configTICK_RATE_HZ; //Half second
  const int data_size = 32;
  const int data_size_bytes = data_size*4;
  uint32_t* data = (uint32_t*)heap_caps_malloc(data_size_bytes, MALLOC_CAP_DMA);
  uint32_t idx = 0x3000;
  while(1)
  {
    for(int j=0; j<1; j++)
    {
      for(uint32_t i=0; i<data_size; i++)
        data[i] = idx+i;
      printf("%x\n", idx);
      spi_master_send(spi, (const uint8_t*)data, data_size_bytes);
      idx += 0x100;
    }
    vTaskDelay(tick);
  }

  while(1)
  {
    vTaskDelay(tick);
  }
}

void arduino_main(void*) {
  setup();
  //TestPins();
  //TestSpi();
  while (1) {
    loop();
  }
}

extern "C" {
  void app_main() {
    init_nvs_flash();
    WiFi.init();
    xTaskCreatePinnedToCore(arduino_main, "arduino", 8192, NULL, 1, NULL, 1);
  }
}
