// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file setup.c
 * @brief Driver and board low-level setup
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "setup.h"
#include "board.h"
#include "cmd.h"
#include "debug.h"
#include "irq.h"
#include "irq_ctrl.h"
#include "mmu_stm32mp13xx.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_rcc_ex.h"
#include "stm32mp13xx_hal_uart.h"
#include "stm32mp13xx_hal_uart_ex.h"
#include "stm32mp13xx_ll_etzpc.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include "usbd_msc.h"
#include "usbd_msc_storage.h"
#include <stdint.h>

#if (USE_STPMIC1x == 1)
#include "stm32mp13xx_disco_stpmic1.h"
#endif

// global variables
UART_HandleTypeDef huart4;
USBD_HandleTypeDef usbd_device;

// cppcheck-suppress unusedFunction
void UART4_IRQHandler(void)
{
   uint32_t isr = huart4.Instance->ISR;
   uint32_t cr1 = huart4.Instance->CR1;

   // take character out, if any
   if ((isr & (USART_ISR_RXNE_RXFNE)) && (cr1 & USART_CR1_RXNEIE)) {
      char byte = (char)(huart4.Instance->RDR & 0xFFU);
      cmd_take_char(byte);
   }

   // clear other interrupt flags
   uint32_t error_mask =
       (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE);
   if (isr & error_mask) {
      huart4.Instance->ICR =
          (USART_ICR_ORECF | USART_ICR_NECF | USART_ICR_FECF | USART_ICR_PECF);
   }

   // handle IDLE line, if used
   if (isr & USART_ISR_IDLE) {
      huart4.Instance->ICR = USART_ICR_IDLECF;
   }
}

// cppcheck-suppress unusedFunction
void _putchar(char ch)
{
   /* Wait until TXE (Transmit Data Register Empty) flag is set */
   while (!(UART4->ISR & USART_ISR_TXE)) {
      /* busy wait */
   }

   /* Write character to Transmit Data Register */
   UART4->TDR = (uint8_t)ch;

   /* Wait until transmission complete */
   while (!(UART4->ISR & USART_ISR_TC)) {
      /* busy wait */
   }
}

void sysclk_init(void)
{
   HAL_RCC_DeInit();
   RCC_ClkInitTypeDef rcc_clkinitstructure;
   RCC_OscInitTypeDef rcc_oscinitstructure;

   /* Enable all available oscillators except LSE */
   rcc_oscinitstructure.OscillatorType =
       (RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE |
        RCC_OSCILLATORTYPE_CSI | RCC_OSCILLATORTYPE_LSI);

   rcc_oscinitstructure.HSIState = RCC_HSI_ON;
   rcc_oscinitstructure.HSEState = RCC_HSE_ON;
   rcc_oscinitstructure.LSEState = RCC_LSE_OFF;
   rcc_oscinitstructure.LSIState = RCC_LSI_ON;
   rcc_oscinitstructure.CSIState = RCC_CSI_ON;

   rcc_oscinitstructure.HSICalibrationValue = 0x00; // Default reset value
   rcc_oscinitstructure.CSICalibrationValue = 0x10; // Default reset value
   rcc_oscinitstructure.HSIDivValue         = RCC_HSI_DIV1; // Default value

   /* PLL configuration */
   rcc_oscinitstructure.PLL.PLLState  = RCC_PLL_ON;
   rcc_oscinitstructure.PLL.PLLSource = RCC_PLL12SOURCE_HSE;
   rcc_oscinitstructure.PLL.PLLM      = 3;
   rcc_oscinitstructure.PLL.PLLN      = 81;
   rcc_oscinitstructure.PLL.PLLP      = 1;
   rcc_oscinitstructure.PLL.PLLQ      = 2;
   rcc_oscinitstructure.PLL.PLLR      = 2;
   rcc_oscinitstructure.PLL.PLLFRACV  = 0x800;
   rcc_oscinitstructure.PLL.PLLMODE   = RCC_PLL_FRACTIONAL;

   rcc_oscinitstructure.PLL2.PLLState  = RCC_PLL_ON;
   rcc_oscinitstructure.PLL2.PLLSource = RCC_PLL12SOURCE_HSE;
   rcc_oscinitstructure.PLL2.PLLM      = 3;
   rcc_oscinitstructure.PLL2.PLLN      = 66;
   rcc_oscinitstructure.PLL2.PLLP      = 2;
   rcc_oscinitstructure.PLL2.PLLQ      = 2;
   rcc_oscinitstructure.PLL2.PLLR      = 1;
   rcc_oscinitstructure.PLL2.PLLFRACV  = 0x1400;
   rcc_oscinitstructure.PLL2.PLLMODE   = RCC_PLL_FRACTIONAL;

   rcc_oscinitstructure.PLL3.PLLState  = RCC_PLL_ON;
   rcc_oscinitstructure.PLL3.PLLSource = RCC_PLL3SOURCE_HSE;
   rcc_oscinitstructure.PLL3.PLLM      = 2;
   rcc_oscinitstructure.PLL3.PLLN      = 34;
   rcc_oscinitstructure.PLL3.PLLP      = 2;
   rcc_oscinitstructure.PLL3.PLLQ      = 17;
   rcc_oscinitstructure.PLL3.PLLR      = 2;
   rcc_oscinitstructure.PLL3.PLLRGE    = RCC_PLL3IFRANGE_1;
   rcc_oscinitstructure.PLL3.PLLFRACV  = 0x1a04;
   rcc_oscinitstructure.PLL3.PLLMODE   = RCC_PLL_FRACTIONAL;

   rcc_oscinitstructure.PLL4.PLLState  = RCC_PLL_ON;
   rcc_oscinitstructure.PLL4.PLLSource = RCC_PLL4SOURCE_HSE;
   rcc_oscinitstructure.PLL4.PLLM      = 2;
   rcc_oscinitstructure.PLL4.PLLN      = 50;
   rcc_oscinitstructure.PLL4.PLLP      = 12;
   rcc_oscinitstructure.PLL4.PLLQ      = 25;
   rcc_oscinitstructure.PLL4.PLLR      = 6;
   rcc_oscinitstructure.PLL4.PLLRGE    = RCC_PLL4IFRANGE_1;
   rcc_oscinitstructure.PLL4.PLLFRACV  = 0;
   rcc_oscinitstructure.PLL4.PLLMODE   = RCC_PLL_INTEGER;

   /* Enable access to RTC and backup registers */
   SET_BIT(PWR->CR1, PWR_CR1_DBP);

   if (HAL_RCC_OscConfig(&rcc_oscinitstructure) != HAL_OK) {
      ERROR("HAL RCC Osc configuration error");
   }

   /* Select PLLx as MPU, AXI and MCU clock sources */
   rcc_clkinitstructure.ClockType =
       (RCC_CLOCKTYPE_MPU | RCC_CLOCKTYPE_ACLK | RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK4 | RCC_CLOCKTYPE_PCLK5 | RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK6 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3);

   rcc_clkinitstructure.MPUInit.MPU_Clock     = RCC_MPUSOURCE_PLL1;
   rcc_clkinitstructure.MPUInit.MPU_Div       = RCC_MPU_DIV2;
   rcc_clkinitstructure.AXISSInit.AXI_Clock   = RCC_AXISSOURCE_PLL2;
   rcc_clkinitstructure.AXISSInit.AXI_Div     = RCC_AXI_DIV1;
   rcc_clkinitstructure.MLAHBInit.MLAHB_Clock = RCC_MLAHBSSOURCE_PLL3;
   rcc_clkinitstructure.MLAHBInit.MLAHB_Div   = RCC_MLAHB_DIV1;
   rcc_clkinitstructure.APB1_Div              = RCC_APB1_DIV2;
   rcc_clkinitstructure.APB2_Div              = RCC_APB2_DIV2;
   rcc_clkinitstructure.APB3_Div              = RCC_APB3_DIV2;
   rcc_clkinitstructure.APB4_Div              = RCC_APB4_DIV2;
   rcc_clkinitstructure.APB5_Div              = RCC_APB5_DIV4;
   rcc_clkinitstructure.APB6_Div              = RCC_APB6_DIV2;

   /* Mark all RCC registers as non-secure */
   RCC->SECCFGR = 0x00000000;

   if (HAL_RCC_ClockConfig(&rcc_clkinitstructure) != HAL_OK) {
      ERROR("HAL RCC Clk configuration error");
   }

   /*
Note : The activation of the I/O Compensation Cell is recommended with
communication  interfaces (GPIO, SPI, FMC, QSPI ...)  when  operating at  high
frequencies(please refer to product datasheet) The I/O Compensation Cell
activation  procedure requires :
- The activation of the CSI clock
- The activation of the SYSCFG clock
- Enabling the I/O Compensation Cell : setting bit[0] of register SYSCFG_CCCSR

To do this please uncomment the following code
*/

   /*
      __HAL_RCC_CSI_ENABLE() ;
      __HAL_RCC_SYSCFG_CLK_ENABLE() ;
      HAL_EnableCompensationCell();
      */
}

void pmic_init(void)
{
#if (USE_STPMIC1x == 1)
   BSP_PMIC_Init();
   BSP_PMIC_InitRegulators();

   STPMU1_Regulator_Voltage_Set(STPMU1_BUCK2, 1350);
   STPMU1_Regulator_Enable(STPMU1_BUCK2);
   HAL_Delay(1);
   STPMU1_Regulator_Enable(STPMU1_VREFDDR);
   HAL_Delay(1);
#endif
}

void perclk_init(void)
{
   RCC_PeriphCLKInitTypeDef pclk = {0};

   pclk.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
   pclk.CkperClockSelection  = RCC_CKPERCLKSOURCE_HSE;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("CKPER");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
   pclk.Eth1ClockSelection   = RCC_ETH1CLKSOURCE_PLL4;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("ETH1");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_ETH2;
   pclk.Eth2ClockSelection   = RCC_ETH2CLKSOURCE_PLL4;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("ETH2");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1;
   pclk.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLL4;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("SDMMC1");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_SDMMC2;
   pclk.Sdmmc2ClockSelection = RCC_SDMMC2CLKSOURCE_PLL4;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("SDMMC2");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_STGEN;
   pclk.StgenClockSelection  = RCC_STGENCLKSOURCE_HSE;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("STGEN");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
   pclk.I2c4ClockSelection   = RCC_I2C4CLKSOURCE_HSI;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("I2C4");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_I2C5;
   pclk.I2c5ClockSelection   = RCC_I2C5CLKSOURCE_HSI;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("I2C5");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_ADC2;
   pclk.Adc2ClockSelection   = RCC_ADC2CLKSOURCE_PER;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("ADC2");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_I2C12;
   pclk.I2c12ClockSelection  = RCC_I2C12CLKSOURCE_HSI;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("I2C12");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_USART2;
   pclk.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("USART2");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_UART4;
   pclk.Uart4ClockSelection  = RCC_UART4CLKSOURCE_HSI;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("UART4");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_SAES;
   pclk.SaesClockSelection   = RCC_SAESCLKSOURCE_ACLK;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("SAES");
   }

   pclk.PeriphClockSelection = RCC_PERIPHCLK_LPTIM3;
   pclk.Lptim3ClockSelection = RCC_LPTIM3CLKSOURCE_PCLK3;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
      ERROR("LPTIM3");
   }
}

void etzpc_init(void)
{
   __HAL_RCC_ETZPC_CLK_ENABLE();

   // unsecure SYSRAM
   LL_ETZPC_SetSecureSysRamSize(ETZPC, 0);

   // unsecure peripherals
   LL_ETZPC_Set_All_PeriphProtection(
       ETZPC, LL_ETZPC_PERIPH_PROTECTION_READ_WRITE_NONSECURE);
}

void gpio_init(void)
{
   /* Enable all GPIO clocks */
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOF_CLK_ENABLE();
   __HAL_RCC_GPIOG_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPIOI_CLK_ENABLE();

   /* Mark all GPIO pins as non-secure */
   GPIOA->SECCFGR = 0x00000000;
   GPIOB->SECCFGR = 0x00000000;
   GPIOC->SECCFGR = 0x00000000;
   GPIOD->SECCFGR = 0x00000000;
   GPIOE->SECCFGR = 0x00000000;
   GPIOF->SECCFGR = 0x00000000;
   GPIOG->SECCFGR = 0x00000000;
   GPIOH->SECCFGR = 0x00000000;
   GPIOI->SECCFGR = 0x00000000;
}

void uart4_init(void)
{
   __HAL_RCC_UART4_CLK_ENABLE();
   __HAL_RCC_UART4_FORCE_RESET();
   __HAL_RCC_UART4_RELEASE_RESET();
   __HAL_RCC_GPIOD_CLK_ENABLE();

   GPIO_InitTypeDef gpioi;
   gpioi.Pin       = GPIO_PIN_6;
   gpioi.Mode      = GPIO_MODE_AF_PP;
   gpioi.Pull      = GPIO_PULLUP;
   gpioi.Speed     = GPIO_SPEED_FREQ_HIGH;
   gpioi.Alternate = GPIO_AF8_UART4;
   HAL_GPIO_Init(GPIOD, &gpioi);

   gpioi.Pin       = GPIO_PIN_8;
   gpioi.Mode      = GPIO_MODE_AF_PP;
   gpioi.Pull      = GPIO_PULLUP;
   gpioi.Speed     = GPIO_SPEED_FREQ_HIGH;
   gpioi.Alternate = GPIO_AF8_UART4;
   HAL_GPIO_Init(GPIOD, &gpioi);

   IRQ_SetPriority(UART4_IRQn, PRIO_UART);
   IRQ_Enable(UART4_IRQn);

   huart4.Instance                    = UART4;
   huart4.Init.BaudRate               = 115200;
   huart4.Init.WordLength             = UART_WORDLENGTH_8B;
   huart4.Init.StopBits               = UART_STOPBITS_1;
   huart4.Init.Parity                 = UART_PARITY_NONE;
   huart4.Init.Mode                   = UART_MODE_TX_RX;
   huart4.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
   huart4.Init.OverSampling           = UART_OVERSAMPLING_8;
   huart4.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
   huart4.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
   huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

   if (HAL_UART_Init(&huart4) != HAL_OK)
      ERROR("UART4");
   if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) !=
       HAL_OK)
      ERROR("FIFO TX Threshold");
   if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) !=
       HAL_OK)
      ERROR("FIFO RX Threshold");
   if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
      ERROR("Disable FIFO");

   __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
}

void usb_init(void)
{
   USBD_Init(&usbd_device, &MSC_Desc, 0);
   USBD_RegisterClass(&usbd_device, USBD_MSC_CLASS);
   USBD_MSC_RegisterStorage(&usbd_device, &USBD_MSC_fops);
   USBD_Start(&usbd_device);
}

void gic_init(void)
{
   // enable Group 0 and 1 interrupts
   GICDistributor->CTLR |= 0x03U;
   GICInterface->CTLR |= 0x03U;
   GICInterface->PMR = 0xFFU;
}

void mmu_init(void)
{
#ifdef MMU_USE
   /* Create Translation Table */
   MMU_CreateTranslationTable();

   /* Enable MMU */
   MMU_Enable();
#endif

   /* Enable Caches */
#ifdef CACHE_USE
   L1C_EnableCaches();
#endif
   L1C_EnableBTAC();
}

// end file setup.c
