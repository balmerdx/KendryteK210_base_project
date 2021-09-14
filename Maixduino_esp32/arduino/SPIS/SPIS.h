#pragma once

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_slave.h>
#include <driver/spi_master.h>

#define USE_2SPI

//Spi Slave
class SpiSlave {
public:
  SpiSlave(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin, int readyPin);

  void begin();

  //return - bytes acrually transfered
  int transfer(uint8_t out[], uint8_t in[], size_t len);

private:
  static void onChipSelect();
  void handleOnChipSelect();

  static void onSetupComplete(spi_slave_transaction_t*);
  void handleSetupComplete();

private:
  spi_host_device_t _hostDevice;
  int _dmaChannel;
  int _mosiPin;
  int _misoPin;
  int _sclkPin;
  int _csPin;
  int _readyPin;

  intr_handle_t _csIntrHandle;

  SemaphoreHandle_t _readySemaphore;
};

class SpiMaster {
public:
  SpiMaster(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin);
  void begin();
  void send(const uint8_t* data, size_t len);
private:
  spi_device_handle_t spi;
  spi_host_device_t _hostDevice;
  int _dmaChannel;
  int _mosiPin;
  int _misoPin;
  int _sclkPin;
  int _csPin;
};

extern SpiSlave SPIS;
#ifdef USE_2SPI
extern SpiMaster SPIM;
#endif
