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

#undef AUTOBOOT
#undef REG_PRINTOUT

#define USE_STPMIC1x 1
#define USE_MCP23x17 1

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
#define CTP_INT_PIN_NUM 5U
#define ctp_irqhandler  EXTI5_IRQHandler
#define ctp_unused      EXTI12_IRQHandler

#define LCD_WIDTH  480U // LCD PIXEL WIDTH
#define LCD_HEIGHT 272U // LCD PIXEL HEIGHT
#define LCD_HSYNC  41U  // Horizontal synchronization
#define LCD_HBP    13U  // Horizontal back porch
#define LCD_HFP    32U  // Horizontal front porch
#define LCD_VSYNC  10U  // Vertical synchronization
#define LCD_VBP    2U   // Vertical back porch
#define LCD_VFP    2U   // Vertical front porch

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

#define ETH_CLK_SRC     HAL_ETH1_REF_CLK_RX_CLK_PIN
#define ETH_RX_CLK_PORT GPIOA
#define ETH_RX_CLK_PIN  GPIO_PIN_1
#define ETH_RX_CLK_AF   GPIO_AF11_ETH
#define ETH_MDIO_PORT   GPIOA
#define ETH_MDIO_PIN    GPIO_PIN_2
#define ETH_MDIO_AF     GPIO_AF11_ETH
#define ETH_TX_EN_PORT  GPIOB
#define ETH_TX_EN_PIN   GPIO_PIN_11
#define ETH_TX_EN_AF    GPIO_AF11_ETH
#define ETH_CRS_DV_PORT GPIOC
#define ETH_CRS_DV_PIN  GPIO_PIN_1
#define ETH_CRS_DV_AF   GPIO_AF10_ETH
#define ETH_RXD0_PORT   GPIOC
#define ETH_RXD0_PIN    GPIO_PIN_4
#define ETH_RXD0_AF     GPIO_AF11_ETH
#define ETH_RXD1_PORT   GPIOC
#define ETH_RXD1_PIN    GPIO_PIN_5
#define ETH_RXD1_AF     GPIO_AF11_ETH
#define ETH_MDC_PORT    GPIOG
#define ETH_MDC_PIN     GPIO_PIN_2
#define ETH_MDC_AF      GPIO_AF11_ETH
#define ETH_TXD0_PORT   GPIOG
#define ETH_TXD0_PIN    GPIO_PIN_13
#define ETH_TXD0_AF     GPIO_AF11_ETH
#define ETH_TXD1_PORT   GPIOG
#define ETH_TXD1_PIN    GPIO_PIN_14
#define ETH_TXD1_AF     GPIO_AF11_ETH
#define ETH_NRST_PIN    MCP_PIN_9

#else

#undef AUTOBOOT
#undef REG_PRINTOUT

#define USE_STPMIC1x 0
#define USE_MCP23x17 0

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
#define CTP_INT_PIN_NUM 12U
#define ctp_irqhandler  EXTI12_IRQHandler
#define ctp_unused      EXTI5_IRQHandler

#define LCD_WIDTH  800U
#define LCD_HEIGHT 480U
#define LCD_HSYNC  1U
#define LCD_HBP    8U
#define LCD_HFP    8U
#define LCD_VSYNC  1U
#define LCD_VBP    16U
#define LCD_VFP    16U

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

#define ETH_CLK_SRC HAL_ETH1_REF_CLK_RCC
#undef ETH_RX_CLK_PORT
#undef ETH_RX_CLK_PIN
#undef ETH_RX_CLK_AF
#define ETH_CLK_PORT      GPIOA
#define ETH_CLK_PIN       GPIO_PIN_11
#define ETH_CLK_AF        GPIO_AF11_ETH
#define ETH_MDIO_PORT     GPIOG
#define ETH_MDIO_PIN      GPIO_PIN_3
#define ETH_MDIO_AF       GPIO_AF11_ETH
#define ETH_TX_EN_PORT    GPIOB
#define ETH_TX_EN_PIN     GPIO_PIN_11
#define ETH_TX_EN_AF      GPIO_AF11_ETH
#define ETH_CRS_DV_PORT   GPIOA
#define ETH_CRS_DV_PIN    GPIO_PIN_7
#define ETH_CRS_DV_AF     GPIO_AF11_ETH
#define ETH_RXD0_PORT     GPIOC
#define ETH_RXD0_PIN      GPIO_PIN_4
#define ETH_RXD0_AF       GPIO_AF11_ETH
#define ETH_RXD1_PORT     GPIOC
#define ETH_RXD1_PIN      GPIO_PIN_5
#define ETH_RXD1_AF       GPIO_AF11_ETH
#define ETH_MDC_PORT      GPIOG
#define ETH_MDC_PIN       GPIO_PIN_2
#define ETH_MDC_AF        GPIO_AF11_ETH
#define ETH_TXD0_PORT     GPIOG
#define ETH_TXD0_PIN      GPIO_PIN_13
#define ETH_TXD0_AF       GPIO_AF11_ETH
#define ETH_TXD1_PORT     GPIOG
#define ETH_TXD1_PIN      GPIO_PIN_14
#define ETH_TXD1_AF       GPIO_AF11_ETH
#define ETH_PHY_INTN_PORT GPIOG
#define ETH_PHY_INTN_PIN  GPIO_PIN_12
#define ETH_PHY_INTN_AF   GPIO_AF11_ETH
#define ETH_NRST_PORT     GPIOG
#define ETH_NRST_PIN      GPIO_PIN_11

#endif

#endif // BOARD_H

// end file board.h
