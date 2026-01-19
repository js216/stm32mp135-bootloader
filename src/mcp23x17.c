// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file mpc23x17.h
 * @brief Super-simple MCP23017T-E/ML driver
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "mcp23x17.h"
#include "printf.h"
#include "stm32mp13xx_hal_i2c.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_gpio.h"
#include <stdint.h>
#include <stdbool.h>

#define MCP_I2C_ADDR 0x21
#define REG_IODIRA 0x00
#define REG_IODIRB 0x01
#define REG_GPIOA  0x12
#define REG_GPIOB  0x13

// global variables
static I2C_HandleTypeDef hi2c;

void mcp_init(void)
{
   GPIO_InitTypeDef gpio_initstruct = {0};

   gpio_initstruct.Mode      = GPIO_MODE_AF_OD;
   gpio_initstruct.Pull      = GPIO_PULLUP;
   gpio_initstruct.Speed     = GPIO_SPEED_FREQ_LOW;
   gpio_initstruct.Alternate = GPIO_AF5_I2C1;

   gpio_initstruct.Pin       = GPIO_PIN_12;
   HAL_GPIO_Init(GPIOD, &gpio_initstruct);

   gpio_initstruct.Pin = GPIO_PIN_8;
   HAL_GPIO_Init(GPIOE, &gpio_initstruct);

   __HAL_RCC_I2C1_CLK_ENABLE();

   /* To get 100kHz (10,000ns period):
    * Total divider needed = 64,000,000 / 100,000 = 640
    * Let's set PRESC = 7 (internal divider of 8)
    * We need SCLL + SCLH to cover 640 / 8 = 80 ticks.
    */
   uint32_t presc = 7;  // 4-bit max (0-15)
   uint32_t scll  = 43; // (43+1) * 8 = 352 ticks = 5.5us
   uint32_t sclh  = 35; // (35+1) * 8 = 288 ticks = 4.5us
   uint32_t sdel  = 2;  // Data setup time
   uint32_t ddel  = 2;  // Data hold time

   // Resulting freq calculation:
   // 64,000,000 / ((7+1) * ((43+1) + (35+1))) = 100,000 Hz
   const uint32_t timing =
       (presc << 28U) | (sdel << 20U) | (ddel << 16U) | (sclh << 8U) | scll;

   // initialize hi2c
   hi2c.Instance              = I2C1;
   hi2c.Init.Timing           = timing;
   hi2c.Init.OwnAddress1      = 0;
   hi2c.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
   hi2c.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
   hi2c.Init.OwnAddress2      = 0;
   hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
   hi2c.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
   hi2c.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

   if (HAL_I2C_Init(&hi2c) != HAL_OK) {
      my_printf("CTP init failed: HAL_I2C_Init() failed\r\n");
      return;
   }

}

void mcp_set_pin_mode(enum mcp_pin pin, bool is_output)
{
   uint8_t reg;
   uint8_t bit;
   uint8_t val = 0;
   HAL_StatusTypeDef ret;

   if (pin < 8)
      reg = REG_IODIRA;
   else
      reg = REG_IODIRB;

   bit = (uint8_t)pin % 8;

   ret = HAL_I2C_Mem_Read(&hi2c, MCP_I2C_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
   if (ret != HAL_OK) {
      my_printf("HAL_I2C_Mem_Read() != HAL_OK\r\n");
      return;
   }

   if (is_output)
      val &= ~(1 << bit);
   else
      val |= (1 << bit);

   ret = HAL_I2C_Mem_Write(&hi2c, MCP_I2C_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
   if (ret != HAL_OK) {
      my_printf("HAL_I2C_Mem_Write() != HAL_OK\r\n");
      return;
   }
}

void mcp_pin_write(enum mcp_pin pin, bool is_high)
{
   uint8_t reg;
   uint8_t bit;
   uint8_t val = 0;
   HAL_StatusTypeDef ret;

   if (pin < 8)
      reg = REG_GPIOA;
   else
      reg = REG_GPIOB;

   bit = (uint8_t)pin % 8;

   ret = HAL_I2C_Mem_Read(&hi2c, MCP_I2C_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
   if (ret != HAL_OK) {
      my_printf("HAL_I2C_Mem_Read() != HAL_OK\r\n");
      return;
   }

   if (is_high)
      val |= (1 << bit);
   else
      val &= ~(1 << bit);

   ret = HAL_I2C_Mem_Write(&hi2c, MCP_I2C_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
   if (ret != HAL_OK) {
      my_printf("HAL_I2C_Mem_Read() != HAL_OK\r\n");
      return;
   }
}

// end file mcp23x17.h
