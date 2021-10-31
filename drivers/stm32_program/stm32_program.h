#pragma once
#include <stdint.h>

/*
   Program STM32G0 over UART
   
   Warning!!!! Before program - set OB_USER_nBOOT_SEL
    if(FLASH->OPTR&OB_USER_nBOOT_SEL)
    {
      FLASH_OBProgramInitTypeDef ob_config;
      HAL_FLASHEx_OBGetConfig(&ob_config);
      HAL_FLASH_Unlock();
      HAL_FLASH_OB_Unlock();

      ob_config.OptionType = OPTIONBYTE_USER;
      ob_config.USERType = OB_USER_nBOOT_SEL;
      ob_config.USERConfig = 0;//OB_USER_nBOOT_SEL;
      HAL_FLASHEx_OBProgram(&ob_config);
      HAL_FLASH_OB_Launch();
    }
*/

//Init uart
void stm32p_init();
//Restart into bootloader mode and activate it.
bool stm32p_activate_bootloader();
void stm32p_get_print();

bool stm32p_read_memory(uint32_t address, uint8_t* read_data, uint32_t read_size);
bool stm32p_write_memory(uint32_t address, const uint8_t* write_data, uint32_t write_size);
bool stm32p_erase(uint32_t start_page, uint32_t num_pages);
//Restart into normal mode. Start program from flash.
void stm32p_restart();

void stm32_log_callback(uint8_t* data, uint32_t size);
//Вызывет stm32_log_callback несколько раз, если есть сообщения от STM32.
void stm32p_log_receive();

void stm32p_test_loop();