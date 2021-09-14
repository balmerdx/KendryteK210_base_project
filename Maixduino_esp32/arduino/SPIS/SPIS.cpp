#include <string.h>

#include "wiring_digital.h"
#include "WInterrupts.h"
#include "SPIS.h"

SpiSlave::SpiSlave(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin, int readyPin) :
  _hostDevice(hostDevice),
  _dmaChannel(dmaChannel),
  _mosiPin(mosiPin),
  _misoPin(misoPin),
  _sclkPin(sclkPin),
  _csPin(csPin),
  _readyPin(readyPin)
{
}

void SpiSlave::begin()
{
  spi_bus_config_t busCfg;
  spi_slave_interface_config_t slvCfg;

  pinMode(_readyPin, OUTPUT);
  digitalWrite(_readyPin, HIGH);

  _readySemaphore = xSemaphoreCreateCounting(1, 0);

  attachInterrupt(_csPin, onChipSelect, FALLING);

  memset(&busCfg, 0x00, sizeof(busCfg));
  busCfg.mosi_io_num = _mosiPin;
  busCfg.miso_io_num = _misoPin;
  busCfg.sclk_io_num = _sclkPin;

  memset(&slvCfg, 0x00, sizeof(slvCfg));
  slvCfg.mode = 0;
  slvCfg.spics_io_num = _csPin;
  slvCfg.queue_size = 1;
  slvCfg.flags = 0;
  slvCfg.post_setup_cb = SpiSlave::onSetupComplete;
  slvCfg.post_trans_cb = NULL;

  gpio_set_pull_mode((gpio_num_t)_mosiPin, GPIO_FLOATING);
  gpio_set_pull_mode((gpio_num_t)_sclkPin, GPIO_PULLDOWN_ONLY);
  gpio_set_pull_mode((gpio_num_t)_csPin,   GPIO_PULLUP_ONLY);

  spi_slave_initialize(_hostDevice, &busCfg, &slvCfg, _dmaChannel);
}

int SpiSlave::transfer(uint8_t out[], uint8_t in[], size_t len)
{
  spi_slave_transaction_t slvTrans;
  spi_slave_transaction_t* slvRetTrans;

  memset(&slvTrans, 0x00, sizeof(slvTrans));

  slvTrans.length = len * 8;
  slvTrans.trans_len = 0;
  slvTrans.tx_buffer = out;
  slvTrans.rx_buffer = in;

  spi_slave_queue_trans(_hostDevice, &slvTrans, portMAX_DELAY);
  xSemaphoreTake(_readySemaphore, portMAX_DELAY);
  digitalWrite(_readyPin, LOW);
  spi_slave_get_trans_result(_hostDevice, &slvRetTrans, portMAX_DELAY);
  digitalWrite(_readyPin, HIGH);

  return (slvTrans.trans_len / 8);
}

void SpiSlave::onChipSelect()
{
  SPIS.handleOnChipSelect();
}

void SpiSlave::handleOnChipSelect()
{
  digitalWrite(_readyPin, HIGH);
}

void SpiSlave::onSetupComplete(spi_slave_transaction_t*)
{
  SPIS.handleSetupComplete();
}

void SpiSlave::handleSetupComplete()
{
  xSemaphoreGiveFromISR(_readySemaphore, NULL);
}

SpiMaster::SpiMaster(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin) :
  _hostDevice(hostDevice),
  _dmaChannel(dmaChannel),
  _mosiPin(mosiPin),
  _misoPin(misoPin),
  _sclkPin(sclkPin),
  _csPin(csPin)
{
}

void SpiMaster::begin()
{
  esp_err_t ret;
  spi_bus_config_t buscfg={
      .mosi_io_num=_mosiPin,
      .miso_io_num=_misoPin,
      .sclk_io_num=_sclkPin,
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
      .spics_io_num=_csPin,               //CS pin
      .flags = 0,
      .queue_size=7,                          //We want to be able to queue 7 transactions at a time
      .pre_cb = 0,
      .post_cb = 0
  };

  //Initialize the SPI bus
  ret=spi_bus_initialize(_hostDevice, &buscfg, _dmaChannel);
  ESP_ERROR_CHECK(ret);
  //Attach the LCD to the SPI bus
  ret=spi_bus_add_device(_hostDevice, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
}

void SpiMaster::send(const uint8_t* data, size_t len)
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


#ifdef USE_2SPI
//hostDevice = VSPI_HOST, dmaChannel = 1, mosiPin=23, misoPin=-1, sclkPin=18, csPin=5, readyPin=25
SpiSlave SPIS(VSPI_HOST, 1, 23, -1, 18, 5, 25);
//hostDevice = HSPI_HOST, dmaChannel = 2, mosiPin=13, misoPin=-1, sclkPin=14, csPin=15
SpiMaster SPIM(HSPI_HOST, 2, 13, -1, 14, 15);
#else
//hostDevice = VSPI_HOST, dmaChannel = 1, mosiPin=14, misoPin=23, sclkPin=18, csPin=5, readyPin=25
SpiSlave SPIS(VSPI_HOST, 1, 14, 23, 18, 5, 25);
#endif
