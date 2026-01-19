// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file ctp.c
 * @brief CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "ctp.h"
#include "board.h"
#include "irq.h"
#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_i2c.h"
#include "stm32mp13xx_hal_rcc.h"
#include <stdint.h>

#define CTP_I2C_ADDRESS    0x5DU
#define CTP_TOUCH_DATA_LEN 41U
#define CTP_REG_ID         0x8140U
#define CTP_REG_STATUS     0x814EU

struct ctp_touch_point {
   uint8_t TRACK_ID;
   uint8_t XL;
   uint8_t XH;
   uint8_t YL;
   uint8_t YH;
   uint8_t SL;
   uint8_t SH;
   uint8_t RESERVED;
};

struct ctp_touch_data {
   uint8_t STATUS;
   struct ctp_touch_point TOUCH[5];
};

static void ctp_print_last_touch(void);

static inline uint8_t touch_point_get_num(uint8_t track_id)
{
   return track_id & 0x0FU;
}

static inline uint16_t touch_point_get_x(const struct ctp_touch_point *pt)
{
   return ((uint32_t)(pt->XH) << 8U) | pt->XL;
}

static inline uint16_t touch_point_get_y(const struct ctp_touch_point *pt)
{
   return ((uint32_t)(pt->YH) << 8U) | pt->YL;
}

// global variables
static I2C_HandleTypeDef hi2c5;
static uint8_t touch_buf[CTP_TOUCH_DATA_LEN];
static int last_x = -1;
static int last_y = -1;

// cppcheck-suppress unusedFunction
void ctp_irqhandler(void)
{
   // capture the pending flags
   uint32_t falling_pending = EXTI->FPR1;
   uint32_t rising_pending  = EXTI->RPR1;

   // check for the specific CTP pin
   if (falling_pending & (1U << CTP_INT_PIN_NUM)) {
      EXTI->FPR1 = (1U << CTP_INT_PIN_NUM);
      ctp_print_last_touch();
   }

   // clear all other pending pins
   if (falling_pending)
      EXTI->FPR1 = falling_pending;
   if (rising_pending)
      EXTI->RPR1 = rising_pending;
}

// cppcheck-suppress unusedFunction
void ctp_unused(void)
{
}

static int ctp_read_touch(void)
{
   HAL_StatusTypeDef ret = HAL_ERROR;
   uint8_t zero          = 0;

   // read touch data directly from STATUS register
   ret = HAL_I2C_Mem_Read(&hi2c5, CTP_I2C_ADDRESS << 1U, CTP_REG_STATUS,
                          I2C_MEMADD_SIZE_16BIT, touch_buf, CTP_TOUCH_DATA_LEN,
                          100);
   if (ret != HAL_OK) {
      my_printf("CTP error: read touch data failed\r\n");
      return -1;
   }

   struct ctp_touch_data *data = (struct ctp_touch_data *)touch_buf;
   if (!(((uint32_t)data->STATUS & 0x80U) >> 7U)) {
      return 0;
   }

   // clear status byte
   ret = HAL_I2C_Mem_Write(&hi2c5, CTP_I2C_ADDRESS << 1U, CTP_REG_STATUS,
                           I2C_MEMADD_SIZE_16BIT, &zero, 1, 100);
   if (ret != HAL_OK) {
      my_printf("CTP error: clear STATUS failed\r\n");
      return -1;
   }

   return 1;
}

static void ctp_pin_setup(void)
{
   GPIO_InitTypeDef gpio_initstruct = {0};

   /* --- INT pin: output low for I2C address select --- */
   gpio_initstruct.Pin   = CTP_INT_PIN;
   gpio_initstruct.Mode  = GPIO_MODE_OUTPUT_PP;
   gpio_initstruct.Pull  = GPIO_NOPULL;
   gpio_initstruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(CTP_INT_PORT, &gpio_initstruct);
   HAL_GPIO_WritePin(CTP_INT_PORT, CTP_INT_PIN, GPIO_PIN_RESET);

   /* --- RST pin: output, hold low --- */
   gpio_initstruct.Pin   = CTP_RST_PIN;
   gpio_initstruct.Mode  = GPIO_MODE_OUTPUT_PP;
   gpio_initstruct.Pull  = GPIO_NOPULL;
   gpio_initstruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(CTP_RST_PORT, &gpio_initstruct);
   HAL_GPIO_WritePin(CTP_RST_PORT, CTP_RST_PIN, GPIO_PIN_RESET);

   HAL_Delay(10);

   /* --- release reset while INT still low --- */
   HAL_GPIO_WritePin(CTP_RST_PORT, CTP_RST_PIN, GPIO_PIN_SET);
   HAL_Delay(50);

   /* --- I2C pins: AF open-drain --- */
   gpio_initstruct.Pin       = CTP_SCL_PIN;
   gpio_initstruct.Mode      = GPIO_MODE_AF_OD;
   gpio_initstruct.Pull      = GPIO_PULLUP;
   gpio_initstruct.Speed     = GPIO_SPEED_FREQ_LOW;
   gpio_initstruct.Alternate = CTP_AF;
   HAL_GPIO_Init(CTP_SCL_PORT, &gpio_initstruct);

   gpio_initstruct.Pin = CTP_SDA_PIN;
   HAL_GPIO_Init(CTP_SDA_PORT, &gpio_initstruct);

   /* --- INT pin: switch to EXTI after reset --- */
   gpio_initstruct.Pin  = CTP_INT_PIN;
   gpio_initstruct.Mode = GPIO_MODE_IT_FALLING;
   gpio_initstruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(CTP_INT_PORT, &gpio_initstruct);
}

void ctp_init(void)
{
   HAL_StatusTypeDef ret = HAL_ERROR;

   ctp_pin_setup();

   __HAL_RCC_I2C5_CLK_ENABLE();

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

   // initialize hi2c5
   hi2c5.Instance              = I2C5;
   hi2c5.Init.Timing           = timing;
   hi2c5.Init.OwnAddress1      = 0;
   hi2c5.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
   hi2c5.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
   hi2c5.Init.OwnAddress2      = 0;
   hi2c5.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
   hi2c5.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
   hi2c5.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

   ret = HAL_I2C_Init(&hi2c5);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: HAL_I2C_Init() failed\r\n");
      return;
   }

   // registers used to read device ID
   uint8_t reg[2];
   reg[0] = (uint8_t)(CTP_REG_ID >> 8U);
   reg[1] = (uint8_t)(CTP_REG_ID & 0xFFU);

   /* send read request for device ID registers */
   ret = HAL_I2C_Master_Transmit(&hi2c5, CTP_I2C_ADDRESS << 1U, reg, 2, 100);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: ID addr write failed\r\n");
      return;
   }

   /* read out device ID */
   ret =
       HAL_I2C_Master_Receive(&hi2c5, CTP_I2C_ADDRESS << 1U, touch_buf, 3, 100);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: ID read failed\r\n");
      return;
   }

   /* verify the ID equals "911" in ASCII */
   if (touch_buf[0] != '9' || touch_buf[1] != '1' || touch_buf[2] != '1') {
      my_printf("CTP init failed: bad ID %c%c%c\r\n", touch_buf[0],
                touch_buf[1], touch_buf[2]);
      return;
   }

   // enable touch interrupts
   IRQ_SetPriority(CTP_INT_IRQn, PRIO_GPIO);
   IRQ_Enable(CTP_INT_IRQn);
}

static void ctp_print_last_touch(void)
{
   int ret = ctp_read_touch();
   if (ret < 0)
      return;
   if (ret == 0) {
      last_x = last_y = -1;
      return;
   }

   struct ctp_touch_data *data = (struct ctp_touch_data *)touch_buf;
   int num                     = touch_point_get_num(data->STATUS);
   if (num == 0) {
      last_x = last_y = -1;
      return;
   }

   last_x = touch_point_get_x(&data->TOUCH[0]);
   last_y = touch_point_get_y(&data->TOUCH[0]);
   my_printf("CTP: touch #%d at (%d,%d)\r\n", 1, last_x, last_y);
}
