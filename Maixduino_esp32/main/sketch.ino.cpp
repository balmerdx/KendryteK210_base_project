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

#include <esp32/rom/uart.h>

#include <driver/periph_ctrl.h>
#include <driver/uart.h>
#include <driver/adc.h>

#include <esp_bt.h>

#include <Arduino.h>

#include <SPIS.h>
#include <WiFi.h>

#include "CommandHandler.h"

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

int debug = 1;

uint8_t* commandBuffer;
uint8_t* responseBuffer;

void dumpBuffer(const char* label, uint8_t data[], int length) {
  printf("%s: ", label);

  for (int i = 0; i < length; i++) {
    printf("%02x", data[i]);
  }

  printf("\r\n");
}

void setDebug(int d) {
  debug = d;
  printf("Balmer debug=%i\r\n", d);
}

void setupWiFi();
void setupBluetooth();

void setupADC();
void setup() {
  setDebug(debug);

  // put SWD and SWCLK pins connected to SAMD as inputs
  pinMode(15, INPUT);
  pinMode(21, INPUT);

  pinMode(5, INPUT);
  if (digitalRead(5) == LOW) {
    if (debug)  ets_printf("*** BLUETOOTH ON\n");

    setupBluetooth();
  } else {
    if (debug)  ets_printf("*** WIFI ON\n");

    setupWiFi();
  }
  setupADC();
}

#define UNO_WIFI_REV2

void setupBluetooth() {
  periph_module_enable(PERIPH_UART1_MODULE);
  periph_module_enable(PERIPH_UHCI0_MODULE);

#ifdef UNO_WIFI_REV2
  uart_set_pin(UART_NUM_1, 1, 3, 33, 0); // TX, RX, RTS, CTS
#else
  uart_set_pin(UART_NUM_1, 23, 12, 18, 5);
#endif
  uart_set_hw_flow_ctrl(UART_NUM_1, UART_HW_FLOWCTRL_CTS_RTS, 5);

  esp_bt_controller_config_t btControllerConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); 

  btControllerConfig.hci_uart_no = UART_NUM_1;
#ifdef UNO_WIFI_REV2
  btControllerConfig.hci_uart_baudrate = 115200;
#else
  btControllerConfig.hci_uart_baudrate = 912600;
#endif

  esp_bt_controller_init(&btControllerConfig);
  while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);
  esp_bt_sleep_enable();

  vTaskSuspend(NULL);

  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

void setupWiFi() {
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
  if (debug)  ets_printf("*** SPIS\n");
  SPIS.begin();

  if (WiFi.status() == WL_NO_SHIELD) {
    if (debug)  ets_printf("*** NOSHIELD\n");
    while (1); // no shield
  }
  
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
    return;
  }

  if (debug) {
    dumpBuffer("COMMAND", commandBuffer, commandLength);
  }

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

void setupADC(){
  uint8_t channels[] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7};
  adc1_config_width(ADC_WIDTH_BIT_12);
  for(uint8_t i=0; i<sizeof(channels); ++i)
  {
    adc1_config_channel_atten((adc1_channel_t)channels[i], ADC_ATTEN_DB_11);
  }
}

