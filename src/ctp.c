// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file ctp.c
 * @brief CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "ctp.h"
#include "irq.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_i2c.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_rcc.h"
#include "printf.h"
#include <string.h>

#ifdef EVB
#define CTP_INT_PORT    GPIOF
#define CTP_INT_PIN     GPIO_PIN_5
#define CTP_SCL_PORT    GPIOD
#define CTP_SCL_PIN     GPIO_PIN_1
#define CTP_SDA_PORT    GPIOH
#define CTP_SDA_PIN     GPIO_PIN_6
#define CTP_AF          GPIO_AF4_I2C5
#define CTP_RST_PORT    GPIOH
#define CTP_RST_PIN     GPIO_PIN_2
#define CTP_INT_IRQn    EXTI5_IRQn
#define CTP_INT_PIN_NUM 5
#define EXTI_IRQHandler EXTI5_IRQHandler
#define unused_handler  EXTI12_IRQHandler
#else
#define CTP_INT_PORT    GPIOH
#define CTP_INT_PIN     GPIO_PIN_12
#define CTP_SCL_PORT    GPIOH
#define CTP_SCL_PIN     GPIO_PIN_13
#define CTP_SDA_PORT    GPIOF
#define CTP_SDA_PIN     GPIO_PIN_3
#define CTP_AF          GPIO_AF4_I2C5
#define CTP_RST_PORT    GPIOB
#define CTP_RST_PIN     GPIO_PIN_7
#define CTP_INT_IRQn    EXTI12_IRQn
#define CTP_INT_PIN_NUM 12
#define EXTI_IRQHandler EXTI12_IRQHandler
#define unused_handler  EXTI5_IRQHandler
#endif

#define CTP_I2C_ADDRESS    0x5D
#define CTP_TOUCH_DATA_LEN 41
#define CTP_REG_ID         0x8140
#define CTP_REG_STATUS     0x814E

#define TOUCH_POINT_GET_NUM(T) ((T) & 0x0F)
#define TOUCH_POINT_GET_X(T)   (((T).XH << 8) | (T).XL)
#define TOUCH_POINT_GET_Y(T)   (((T).YH << 8) | (T).YL)

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

// global variables
static I2C_HandleTypeDef hi2c5;
static uint8_t touch_buf[CTP_TOUCH_DATA_LEN];
static int last_x = -1;
static int last_y = -1;

void EXTI_IRQHandler(void)
{
   if (EXTI->FPR1 & (1 << CTP_INT_PIN_NUM)) {
      EXTI->FPR1 = (1 << CTP_INT_PIN_NUM);
      ctp_print_last_touch();
   }
}

void unused_handler()
{
}

static int ctp_read_touch(void)
{
   HAL_StatusTypeDef ret;
   uint8_t zero = 0;

   // read touch data directly from STATUS register
   ret = HAL_I2C_Mem_Read(&hi2c5,
         CTP_I2C_ADDRESS << 1,
         CTP_REG_STATUS,
         I2C_MEMADD_SIZE_16BIT,
         touch_buf,
         CTP_TOUCH_DATA_LEN,
         100);
   if (ret != HAL_OK) {
      my_printf("CTP error: read touch data failed\r\n");
      return -1;
   }

   struct ctp_touch_data *data = (struct ctp_touch_data *)touch_buf;
   if (!((data->STATUS & 0x80) >> 7)) {
      return 0;
   }

   // clear status byte
   ret = HAL_I2C_Mem_Write(&hi2c5,
         CTP_I2C_ADDRESS << 1,
         CTP_REG_STATUS,
         I2C_MEMADD_SIZE_16BIT,
         &zero,
         1,
         100);
   if (ret != HAL_OK) {
      my_printf("CTP error: clear STATUS failed\r\n");
      return -1;
   }

   return 1;
}

static uint32_t i2c5_calc_timing_100kHz(void)
{
   /* * To get 100kHz (10,000ns period):
    * Total divider needed = 64,000,000 / 100,000 = 640
    * Let's set PRESC = 7 (internal divider of 8)
    * We need SCLL + SCLH to cover 640 / 8 = 80 ticks.
    */
   uint32_t presc = 7;  // 4-bit max (0-15)
   uint32_t scll = 43; // (43+1) * 8 = 352 ticks = 5.5us
   uint32_t sclh = 35; // (35+1) * 8 = 288 ticks = 4.5us
   uint32_t sdel = 2;  // Data setup time
   uint32_t ddel = 2;  // Data hold time

   // Resulting freq calculation:
   // 64,000,000 / ((7+1) * ((43+1) + (35+1))) = 100,000 Hz

   return (presc << 28) | (sdel << 20) | (ddel << 16) | (sclh << 8) | scll;
}

static void ctp_pin_setup(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* --- INT pin: output low for I2C address select --- */
   GPIO_InitStruct.Pin = CTP_INT_PIN;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(CTP_INT_PORT, &GPIO_InitStruct);
   HAL_GPIO_WritePin(CTP_INT_PORT, CTP_INT_PIN, GPIO_PIN_RESET);

   /* --- RST pin: output, hold low --- */
   GPIO_InitStruct.Pin = CTP_RST_PIN;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(CTP_RST_PORT, &GPIO_InitStruct);
   HAL_GPIO_WritePin(CTP_RST_PORT, CTP_RST_PIN, GPIO_PIN_RESET);

   HAL_Delay(10);

   /* --- release reset while INT still low --- */
   HAL_GPIO_WritePin(CTP_RST_PORT, CTP_RST_PIN, GPIO_PIN_SET);
   HAL_Delay(50);

   /* --- I2C pins: AF open-drain --- */
   GPIO_InitStruct.Pin = CTP_SCL_PIN;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
   GPIO_InitStruct.Pull = GPIO_PULLUP;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   GPIO_InitStruct.Alternate = CTP_AF;
   HAL_GPIO_Init(CTP_SCL_PORT, &GPIO_InitStruct);

   GPIO_InitStruct.Pin = CTP_SDA_PIN;
   HAL_GPIO_Init(CTP_SDA_PORT, &GPIO_InitStruct);

   /* --- INT pin: switch to EXTI after reset --- */
   GPIO_InitStruct.Pin = CTP_INT_PIN;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(CTP_INT_PORT, &GPIO_InitStruct);
}

void ctp_init(void)
{
   HAL_StatusTypeDef ret;

   ctp_pin_setup();

   // enable I2C5 clock
   __HAL_RCC_I2C5_CLK_ENABLE();

   // initialize hi2c5
   hi2c5.Instance = I2C5;
   hi2c5.Init.Timing = i2c5_calc_timing_100kHz();
   hi2c5.Init.OwnAddress1 = 0;
   hi2c5.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
   hi2c5.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
   hi2c5.Init.OwnAddress2 = 0;
   hi2c5.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
   hi2c5.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
   hi2c5.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

   ret = HAL_I2C_Init(&hi2c5);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: HAL_I2C_Init() failed\r\n");
      return;
   }

   // registers used to read device ID
   uint8_t reg[2];
   reg[0] = (uint8_t)(CTP_REG_ID >> 8);
   reg[1] = (uint8_t)(CTP_REG_ID & 0xFF);

   /* send read request for device ID registers */
   ret = HAL_I2C_Master_Transmit(&hi2c5,
         CTP_I2C_ADDRESS << 1,
         reg,
         2,
         100);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: ID addr write failed\r\n");
      return;
   }

   /* read out device ID */
   ret = HAL_I2C_Master_Receive(&hi2c5,
         CTP_I2C_ADDRESS << 1,
         touch_buf,
         3,
         100);
   if (ret != HAL_OK) {
      my_printf("CTP init failed: ID read failed\r\n");
      return;
   }

   /* verify the ID equals "911" in ASCII */
   if (touch_buf[0] != '9' || touch_buf[1] != '1' || touch_buf[2] != '1') {
      my_printf("CTP init failed: bad ID %c%c%c\r\n", touch_buf[0], touch_buf[1], touch_buf[2]);
      return;
   }

   // enable touch interrupts
   IRQ_SetPriority(CTP_INT_IRQn, PRIO_GPIO);
   IRQ_Enable(CTP_INT_IRQn);
}

static void ctp_print_last_touch(void)
{
   int ret = ctp_read_touch();
   if (ret < 0) return;
   if (ret == 0) {
      last_x = last_y = -1;
      return;
   }

   struct ctp_touch_data *data = (struct ctp_touch_data *)touch_buf;
   int num = TOUCH_POINT_GET_NUM(data->STATUS);
   if (num == 0) {
      last_x = last_y = -1;
      return;
   }

   last_x = TOUCH_POINT_GET_X(data->TOUCH[0]);
   last_y = TOUCH_POINT_GET_Y(data->TOUCH[0]);
   my_printf("CTP: touch #%d at (%d,%d)\r\n", 1, last_x, last_y);
}
