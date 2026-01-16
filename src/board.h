/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file board.h
 * @brief Board specific definitions: pins, devices, etc.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef BOARD_H
#define BOARD_H

#ifdef EVB // STM32MP135F-DK: Discovery kit with STM32MP135F MPU

#define USE_STPMIC1x 1

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
#define ctp_irqhandler  EXTI5_IRQHandler
#define ctp_unused      EXTI12_IRQHandler

#define LCD_BL_PORT    GPIOE
#define LCD_BL_PIN     GPIO_PIN_12
#define LCD_DISP_PORT  GPIOI
#define LCD_DISP_PIN   GPIO_PIN_7
#define LCD_CLK_PORT   GPIOD
#define LCD_CLK_PIN    GPIO_PIN_9
#define LCD_CLK_AF     GPIO_AF13_LCD
#define LCD_HSYNC_PORT GPIOC
#define LCD_HSYNC_PIN  GPIO_PIN_6
#define LCD_HSYNC_AF   GPIO_AF14_LCD
#define LCD_VSYNC_PORT GPIOG
#define LCD_VSYNC_PIN  GPIO_PIN_4
#define LCD_VSYNC_AF   GPIO_AF11_LCD
#define LCD_DE_PORT    GPIOH
#define LCD_DE_PIN     GPIO_PIN_9
#define LCD_DE_AF      GPIO_AF11_LCD
#define LCD_R3_PORT    GPIOB
#define LCD_R3_PIN     GPIO_PIN_12
#define LCD_R3_AF      GPIO_AF13_LCD
#define LCD_R4_PORT    GPIOD
#define LCD_R4_PIN     GPIO_PIN_14
#define LCD_R4_AF      GPIO_AF14_LCD
#define LCD_R5_PORT    GPIOE
#define LCD_R5_PIN     GPIO_PIN_7
#define LCD_R5_AF      GPIO_AF14_LCD
#define LCD_R6_PORT    GPIOE
#define LCD_R6_PIN     GPIO_PIN_13
#define LCD_R6_AF      GPIO_AF14_LCD
#define LCD_R7_PORT    GPIOE
#define LCD_R7_PIN     GPIO_PIN_9
#define LCD_R7_AF      GPIO_AF14_LCD
#define LCD_G2_PORT    GPIOH
#define LCD_G2_PIN     GPIO_PIN_3
#define LCD_G2_AF      GPIO_AF14_LCD
#define LCD_G3_PORT    GPIOF
#define LCD_G3_PIN     GPIO_PIN_3
#define LCD_G3_AF      GPIO_AF14_LCD
#define LCD_G4_PORT    GPIOD
#define LCD_G4_PIN     GPIO_PIN_5
#define LCD_G4_AF      GPIO_AF14_LCD
#define LCD_G5_PORT    GPIOG
#define LCD_G5_PIN     GPIO_PIN_0
#define LCD_G5_AF      GPIO_AF14_LCD
#define LCD_G6_PORT    GPIOC
#define LCD_G6_PIN     GPIO_PIN_7
#define LCD_G6_AF      GPIO_AF14_LCD
#define LCD_G7_PORT    GPIOA
#define LCD_G7_PIN     GPIO_PIN_15
#define LCD_G7_AF      GPIO_AF11_LCD
#define LCD_B3_PORT    GPIOF
#define LCD_B3_PIN     GPIO_PIN_2
#define LCD_B3_AF      GPIO_AF14_LCD
#define LCD_B4_PORT    GPIOH
#define LCD_B4_PIN     GPIO_PIN_4
#define LCD_B4_AF      GPIO_AF11_LCD
#define LCD_B5_PORT    GPIOE
#define LCD_B5_PIN     GPIO_PIN_0
#define LCD_B5_AF      GPIO_AF14_LCD
#define LCD_B6_PORT    GPIOB
#define LCD_B6_PIN     GPIO_PIN_6
#define LCD_B6_AF      GPIO_AF14_LCD
#define LCD_B7_PORT    GPIOF
#define LCD_B7_PIN     GPIO_PIN_1
#define LCD_B7_AF      GPIO_AF13_LCD

#else

#define USE_STPMIC1x 0

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
#define ctp_irqhandler  EXTI12_IRQHandler
#define ctp_unused      EXTI5_IRQHandler

#define LCD_BL_PORT    GPIOB
#define LCD_BL_PIN     GPIO_PIN_15
#define LCD_DISP_PORT  GPIOG
#define LCD_DISP_PIN   GPIO_PIN_7
#define LCD_CLK_PORT   GPIOD
#define LCD_CLK_PIN    GPIO_PIN_9
#define LCD_CLK_AF     GPIO_AF13_LCD
#define LCD_HSYNC_PORT GPIOE
#define LCD_HSYNC_PIN  GPIO_PIN_1
#define LCD_HSYNC_AF   GPIO_AF9_LCD
#define LCD_VSYNC_PORT GPIOE
#define LCD_VSYNC_PIN  GPIO_PIN_12
#define LCD_VSYNC_AF   GPIO_AF9_LCD
#define LCD_DE_PORT    GPIOG
#define LCD_DE_PIN     GPIO_PIN_6
#define LCD_DE_AF      GPIO_AF13_LCD
#define LCD_R3_PORT    GPIOD
#define LCD_R3_PIN     GPIO_PIN_9
#define LCD_R3_AF      GPIO_AF13_LCD
#define LCD_R4_PORT    GPIOE
#define LCD_R4_PIN     GPIO_PIN_3
#define LCD_R4_AF      GPIO_AF13_LCD
#define LCD_R5_PORT    GPIOF
#define LCD_R5_PIN     GPIO_PIN_5
#define LCD_R5_AF      GPIO_AF14_LCD
#define LCD_R6_PORT    GPIOF
#define LCD_R6_PIN     GPIO_PIN_0
#define LCD_R6_AF      GPIO_AF13_LCD
#define LCD_R7_PORT    GPIOF
#define LCD_R7_PIN     GPIO_PIN_6
#define LCD_R7_AF      GPIO_AF13_LCD
#define LCD_G2_PORT    GPIOF
#define LCD_G2_PIN     GPIO_PIN_7
#define LCD_G2_AF      GPIO_AF14_LCD
#define LCD_G3_PORT    GPIOE
#define LCD_G3_PIN     GPIO_PIN_6
#define LCD_G3_AF      GPIO_AF14_LCD
#define LCD_G4_PORT    GPIOG
#define LCD_G4_PIN     GPIO_PIN_5
#define LCD_G4_AF      GPIO_AF11_LCD
#define LCD_G5_PORT    GPIOG
#define LCD_G5_PIN     GPIO_PIN_0
#define LCD_G5_AF      GPIO_AF14_LCD
#define LCD_G6_PORT    GPIOA
#define LCD_G6_PIN     GPIO_PIN_12
#define LCD_G6_AF      GPIO_AF14_LCD
#define LCD_G7_PORT    GPIOA
#define LCD_G7_PIN     GPIO_PIN_15
#define LCD_G7_AF      GPIO_AF11_LCD
#define LCD_B3_PORT    GPIOG
#define LCD_B3_PIN     GPIO_PIN_15
#define LCD_B3_AF      GPIO_AF14_LCD
#define LCD_B4_PORT    GPIOB
#define LCD_B4_PIN     GPIO_PIN_2
#define LCD_B4_AF      GPIO_AF14_LCD
#define LCD_B5_PORT    GPIOH
#define LCD_B5_PIN     GPIO_PIN_9
#define LCD_B5_AF      GPIO_AF9_LCD
#define LCD_B6_PORT    GPIOF
#define LCD_B6_PIN     GPIO_PIN_4
#define LCD_B6_AF      GPIO_AF13_LCD
#define LCD_B7_PORT    GPIOB
#define LCD_B7_PIN     GPIO_PIN_6
#define LCD_B7_AF      GPIO_AF14_LCD

#endif

#endif // BOARD_H
