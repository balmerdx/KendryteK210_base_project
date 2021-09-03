#pragma once

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_slave.h>

class SPISClass {

  public:
    SPISClass(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin, int readyPin);

    int begin();
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

extern SPISClass SPIS;
