// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file reg.h
 * @brief SoC register monitoring and printouts.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "reg.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include <inttypes.h>
#include <stdint.h>

static void reg_print(const char *name, uint32_t address, uint32_t value)
{
   my_printf("%-16s [0x%08" PRIX32 "] : 0x%08" PRIX32 "\r\n", name, address,
             value);
}

static void reg_tim(void)
{
   TIM_TypeDef *const tim_list[] = {TIM1, TIM2, TIM3,  TIM4,  TIM5, TIM6,
                                    TIM7, TIM8, TIM12, TIM13, TIM14};

   const char *const tim_name[] = {"TIM1",  "TIM2",  "TIM3", "TIM4",
                                   "TIM5",  "TIM6",  "TIM7", "TIM8",
                                   "TIM12", "TIM13", "TIM14"};

   uint32_t i = 0;

   for (i = 0U; i < (sizeof(tim_list) / sizeof(tim_list[0])); i++) {
      TIM_TypeDef *tim = tim_list[i];

      my_printf("\r\n%s @ 0x%08" PRIX32 "\r\n", tim_name[i], (uint32_t)tim);

      reg_print("CR1", (uint32_t)&tim->CR1, tim->CR1);
      reg_print("CR2", (uint32_t)&tim->CR2, tim->CR2);
      reg_print("SMCR", (uint32_t)&tim->SMCR, tim->SMCR);
      reg_print("DIER", (uint32_t)&tim->DIER, tim->DIER);
      reg_print("SR", (uint32_t)&tim->SR, tim->SR);
      reg_print("EGR", (uint32_t)&tim->EGR, tim->EGR);
      reg_print("CCMR1", (uint32_t)&tim->CCMR1, tim->CCMR1);
      reg_print("CCMR2", (uint32_t)&tim->CCMR2, tim->CCMR2);
      reg_print("CCER", (uint32_t)&tim->CCER, tim->CCER);
      reg_print("CNT", (uint32_t)&tim->CNT, tim->CNT);
      reg_print("PSC", (uint32_t)&tim->PSC, tim->PSC);
      reg_print("ARR", (uint32_t)&tim->ARR, tim->ARR);
      reg_print("RCR", (uint32_t)&tim->RCR, tim->RCR);
      reg_print("CCR1", (uint32_t)&tim->CCR1, tim->CCR1);
      reg_print("CCR2", (uint32_t)&tim->CCR2, tim->CCR2);
      reg_print("CCR3", (uint32_t)&tim->CCR3, tim->CCR3);
      reg_print("CCR4", (uint32_t)&tim->CCR4, tim->CCR4);
      reg_print("BDTR", (uint32_t)&tim->BDTR, tim->BDTR);
      reg_print("DCR", (uint32_t)&tim->DCR, tim->DCR);
      reg_print("DMAR", (uint32_t)&tim->DMAR, tim->DMAR);
      reg_print("CCMR3", (uint32_t)&tim->CCMR3, tim->CCMR3);
      reg_print("CCR5", (uint32_t)&tim->CCR5, tim->CCR5);
      reg_print("CCR6", (uint32_t)&tim->CCR6, tim->CCR6);
      reg_print("AF1", (uint32_t)&tim->AF1, tim->AF1);
      reg_print("AF2", (uint32_t)&tim->AF2, tim->AF2);
      reg_print("TISEL", (uint32_t)&tim->TISEL, tim->TISEL);
      reg_print("VERR", (uint32_t)&tim->VERR, tim->VERR);
      reg_print("IPIDR", (uint32_t)&tim->IPIDR, tim->IPIDR);
      reg_print("SIDR", (uint32_t)&tim->SIDR, tim->SIDR);
   }
}

static void reg_gpio(void)
{
   GPIO_TypeDef *const gpio_list[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE,
                                      GPIOF, GPIOG, GPIOH, GPIOI};

   const char *const gpio_name[] = {"GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE",
                                    "GPIOF", "GPIOG", "GPIOH", "GPIOI"};

   uint32_t i = 0;

   for (i = 0U; i < (sizeof(gpio_list) / sizeof(gpio_list[0])); i++) {
      GPIO_TypeDef *gpio = gpio_list[i];

      my_printf("\r\n%s @ 0x%08" PRIX32 "\r\n", gpio_name[i], (uint32_t)gpio);

      reg_print("MODER", (uint32_t)&gpio->MODER, gpio->MODER);
      reg_print("OTYPER", (uint32_t)&gpio->OTYPER, gpio->OTYPER);
      reg_print("OSPEEDR", (uint32_t)&gpio->OSPEEDR, gpio->OSPEEDR);
      reg_print("PUPDR", (uint32_t)&gpio->PUPDR, gpio->PUPDR);
      reg_print("IDR", (uint32_t)&gpio->IDR, gpio->IDR);
      reg_print("ODR", (uint32_t)&gpio->ODR, gpio->ODR);
      reg_print("BSRR", (uint32_t)&gpio->BSRR, gpio->BSRR);
      reg_print("LCKR", (uint32_t)&gpio->LCKR, gpio->LCKR);
      reg_print("AFR[0]", (uint32_t)&gpio->AFR[0], gpio->AFR[0]);
      reg_print("AFR[1]", (uint32_t)&gpio->AFR[1], gpio->AFR[1]);
      reg_print("BRR", (uint32_t)&gpio->BRR, gpio->BRR);
      reg_print("SECCFGR", (uint32_t)&gpio->SECCFGR, gpio->SECCFGR);
      reg_print("HWCFGR10", (uint32_t)&gpio->HWCFGR10, gpio->HWCFGR10);
      reg_print("HWCFGR9", (uint32_t)&gpio->HWCFGR9, gpio->HWCFGR9);
      reg_print("HWCFGR8", (uint32_t)&gpio->HWCFGR8, gpio->HWCFGR8);
      reg_print("HWCFGR7", (uint32_t)&gpio->HWCFGR7, gpio->HWCFGR7);
      reg_print("HWCFGR6", (uint32_t)&gpio->HWCFGR6, gpio->HWCFGR6);
      reg_print("HWCFGR5", (uint32_t)&gpio->HWCFGR5, gpio->HWCFGR5);
      reg_print("HWCFGR4", (uint32_t)&gpio->HWCFGR4, gpio->HWCFGR4);
      reg_print("HWCFGR3", (uint32_t)&gpio->HWCFGR3, gpio->HWCFGR3);
      reg_print("HWCFGR2", (uint32_t)&gpio->HWCFGR2, gpio->HWCFGR2);
      reg_print("HWCFGR1", (uint32_t)&gpio->HWCFGR1, gpio->HWCFGR1);
      reg_print("HWCFGR0", (uint32_t)&gpio->HWCFGR0, gpio->HWCFGR0);
      reg_print("VERR", (uint32_t)&gpio->VERR, gpio->VERR);
      reg_print("IPIDR", (uint32_t)&gpio->IPIDR, gpio->IPIDR);
      reg_print("SIDR", (uint32_t)&gpio->SIDR, gpio->SIDR);
   }
}

static void reg_rcc(void)
{
   RCC_TypeDef *rcc = RCC;

   my_printf("\r\nRCC @ 0x%08" PRIX32 "\r\n", (uint32_t)rcc);

   reg_print("SECCFGR", (uint32_t)&rcc->SECCFGR, rcc->SECCFGR);
   reg_print("MP_SREQSETR", (uint32_t)&rcc->MP_SREQSETR, rcc->MP_SREQSETR);
   reg_print("MP_SREQCLRR", (uint32_t)&rcc->MP_SREQCLRR, rcc->MP_SREQCLRR);
   reg_print("MP_APRSTCR", (uint32_t)&rcc->MP_APRSTCR, rcc->MP_APRSTCR);
   reg_print("MP_APRSTSR", (uint32_t)&rcc->MP_APRSTSR, rcc->MP_APRSTSR);
   reg_print("PWRLPDLYCR", (uint32_t)&rcc->PWRLPDLYCR, rcc->PWRLPDLYCR);
   reg_print("MP_GRSTCSETR", (uint32_t)&rcc->MP_GRSTCSETR, rcc->MP_GRSTCSETR);
   reg_print("BR_RSTSCLRR", (uint32_t)&rcc->BR_RSTSCLRR, rcc->BR_RSTSCLRR);
   reg_print("MP_RSTSSETR", (uint32_t)&rcc->MP_RSTSSETR, rcc->MP_RSTSSETR);
   reg_print("MP_RSTSCLRR", (uint32_t)&rcc->MP_RSTSCLRR, rcc->MP_RSTSCLRR);
   reg_print("MP_IWDGFZSETR", (uint32_t)&rcc->MP_IWDGFZSETR,
             rcc->MP_IWDGFZSETR);
   reg_print("MP_IWDGFZCLRR", (uint32_t)&rcc->MP_IWDGFZCLRR,
             rcc->MP_IWDGFZCLRR);
   reg_print("MP_CIER", (uint32_t)&rcc->MP_CIER, rcc->MP_CIER);
   reg_print("MP_CIFR", (uint32_t)&rcc->MP_CIFR, rcc->MP_CIFR);
   reg_print("BDCR", (uint32_t)&rcc->BDCR, rcc->BDCR);
   reg_print("RDLSICR", (uint32_t)&rcc->RDLSICR, rcc->RDLSICR);
   reg_print("OCENSETR", (uint32_t)&rcc->OCENSETR, rcc->OCENSETR);
   reg_print("OCENCLRR", (uint32_t)&rcc->OCENCLRR, rcc->OCENCLRR);
   reg_print("OCRDYR", (uint32_t)&rcc->OCRDYR, rcc->OCRDYR);
   reg_print("HSICFGR", (uint32_t)&rcc->HSICFGR, rcc->HSICFGR);
   reg_print("CSICFGR", (uint32_t)&rcc->CSICFGR, rcc->CSICFGR);
   reg_print("MCO1CFGR", (uint32_t)&rcc->MCO1CFGR, rcc->MCO1CFGR);
   reg_print("MCO2CFGR", (uint32_t)&rcc->MCO2CFGR, rcc->MCO2CFGR);
   reg_print("DBGCFGR", (uint32_t)&rcc->DBGCFGR, rcc->DBGCFGR);
   reg_print("RCK12SELR", (uint32_t)&rcc->RCK12SELR, rcc->RCK12SELR);
   reg_print("RCK3SELR", (uint32_t)&rcc->RCK3SELR, rcc->RCK3SELR);
   reg_print("RCK4SELR", (uint32_t)&rcc->RCK4SELR, rcc->RCK4SELR);
   reg_print("PLL1CR", (uint32_t)&rcc->PLL1CR, rcc->PLL1CR);
   reg_print("PLL1CFGR1", (uint32_t)&rcc->PLL1CFGR1, rcc->PLL1CFGR1);
   reg_print("PLL1CFGR2", (uint32_t)&rcc->PLL1CFGR2, rcc->PLL1CFGR2);
   reg_print("PLL1FRACR", (uint32_t)&rcc->PLL1FRACR, rcc->PLL1FRACR);
   reg_print("PLL1CSGR", (uint32_t)&rcc->PLL1CSGR, rcc->PLL1CSGR);
   reg_print("PLL2CR", (uint32_t)&rcc->PLL2CR, rcc->PLL2CR);
   reg_print("PLL2CFGR1", (uint32_t)&rcc->PLL2CFGR1, rcc->PLL2CFGR1);
   reg_print("PLL2CFGR2", (uint32_t)&rcc->PLL2CFGR2, rcc->PLL2CFGR2);
   reg_print("PLL2FRACR", (uint32_t)&rcc->PLL2FRACR, rcc->PLL2FRACR);
   reg_print("PLL2CSGR", (uint32_t)&rcc->PLL2CSGR, rcc->PLL2CSGR);
   reg_print("PLL3CR", (uint32_t)&rcc->PLL3CR, rcc->PLL3CR);
   reg_print("PLL3CFGR1", (uint32_t)&rcc->PLL3CFGR1, rcc->PLL3CFGR1);
   reg_print("PLL3CFGR2", (uint32_t)&rcc->PLL3CFGR2, rcc->PLL3CFGR2);
   reg_print("PLL3FRACR", (uint32_t)&rcc->PLL3FRACR, rcc->PLL3FRACR);
   reg_print("PLL3CSGR", (uint32_t)&rcc->PLL3CSGR, rcc->PLL3CSGR);
   reg_print("PLL4CR", (uint32_t)&rcc->PLL4CR, rcc->PLL4CR);
   reg_print("PLL4CFGR1", (uint32_t)&rcc->PLL4CFGR1, rcc->PLL4CFGR1);
   reg_print("PLL4CFGR2", (uint32_t)&rcc->PLL4CFGR2, rcc->PLL4CFGR2);
   reg_print("PLL4FRACR", (uint32_t)&rcc->PLL4FRACR, rcc->PLL4FRACR);
   reg_print("PLL4CSGR", (uint32_t)&rcc->PLL4CSGR, rcc->PLL4CSGR);
   reg_print("MPCKSELR", (uint32_t)&rcc->MPCKSELR, rcc->MPCKSELR);
   reg_print("ASSCKSELR", (uint32_t)&rcc->ASSCKSELR, rcc->ASSCKSELR);
   reg_print("MSSCKSELR", (uint32_t)&rcc->MSSCKSELR, rcc->MSSCKSELR);
   reg_print("CPERCKSELR", (uint32_t)&rcc->CPERCKSELR, rcc->CPERCKSELR);
   reg_print("RTCDIVR", (uint32_t)&rcc->RTCDIVR, rcc->RTCDIVR);
   reg_print("MPCKDIVR", (uint32_t)&rcc->MPCKDIVR, rcc->MPCKDIVR);
   reg_print("AXIDIVR", (uint32_t)&rcc->AXIDIVR, rcc->AXIDIVR);
   reg_print("MLAHBDIVR", (uint32_t)&rcc->MLAHBDIVR, rcc->MLAHBDIVR);
   reg_print("APB1DIVR", (uint32_t)&rcc->APB1DIVR, rcc->APB1DIVR);
   reg_print("APB2DIVR", (uint32_t)&rcc->APB2DIVR, rcc->APB2DIVR);
   reg_print("APB3DIVR", (uint32_t)&rcc->APB3DIVR, rcc->APB3DIVR);
   reg_print("APB4DIVR", (uint32_t)&rcc->APB4DIVR, rcc->APB4DIVR);
   reg_print("APB5DIVR", (uint32_t)&rcc->APB5DIVR, rcc->APB5DIVR);
   reg_print("APB6DIVR", (uint32_t)&rcc->APB6DIVR, rcc->APB6DIVR);
   reg_print("TIMG1PRER", (uint32_t)&rcc->TIMG1PRER, rcc->TIMG1PRER);
   reg_print("TIMG2PRER", (uint32_t)&rcc->TIMG2PRER, rcc->TIMG2PRER);
   reg_print("TIMG3PRER", (uint32_t)&rcc->TIMG3PRER, rcc->TIMG3PRER);
   reg_print("DDRITFCR", (uint32_t)&rcc->DDRITFCR, rcc->DDRITFCR);
   reg_print("I2C12CKSELR", (uint32_t)&rcc->I2C12CKSELR, rcc->I2C12CKSELR);
   reg_print("I2C345CKSELR", (uint32_t)&rcc->I2C345CKSELR, rcc->I2C345CKSELR);
   reg_print("SPI2S1CKSELR", (uint32_t)&rcc->SPI2S1CKSELR, rcc->SPI2S1CKSELR);
   reg_print("SPI2S23CKSELR", (uint32_t)&rcc->SPI2S23CKSELR,
             rcc->SPI2S23CKSELR);
   reg_print("SPI45CKSELR", (uint32_t)&rcc->SPI45CKSELR, rcc->SPI45CKSELR);
   reg_print("UART12CKSELR", (uint32_t)&rcc->UART12CKSELR, rcc->UART12CKSELR);
   reg_print("UART35CKSELR", (uint32_t)&rcc->UART35CKSELR, rcc->UART35CKSELR);
   reg_print("UART4CKSELR", (uint32_t)&rcc->UART4CKSELR, rcc->UART4CKSELR);
   reg_print("UART6CKSELR", (uint32_t)&rcc->UART6CKSELR, rcc->UART6CKSELR);
   reg_print("UART78CKSELR", (uint32_t)&rcc->UART78CKSELR, rcc->UART78CKSELR);
   reg_print("LPTIM1CKSELR", (uint32_t)&rcc->LPTIM1CKSELR, rcc->LPTIM1CKSELR);
   reg_print("LPTIM23CKSELR", (uint32_t)&rcc->LPTIM23CKSELR,
             rcc->LPTIM23CKSELR);
   reg_print("LPTIM45CKSELR", (uint32_t)&rcc->LPTIM45CKSELR,
             rcc->LPTIM45CKSELR);
   reg_print("SAI1CKSELR", (uint32_t)&rcc->SAI1CKSELR, rcc->SAI1CKSELR);
   reg_print("SAI2CKSELR", (uint32_t)&rcc->SAI2CKSELR, rcc->SAI2CKSELR);
   reg_print("FDCANCKSELR", (uint32_t)&rcc->FDCANCKSELR, rcc->FDCANCKSELR);
   reg_print("SPDIFCKSELR", (uint32_t)&rcc->SPDIFCKSELR, rcc->SPDIFCKSELR);
   reg_print("ADC12CKSELR", (uint32_t)&rcc->ADC12CKSELR, rcc->ADC12CKSELR);
   reg_print("SDMMC12CKSELR", (uint32_t)&rcc->SDMMC12CKSELR,
             rcc->SDMMC12CKSELR);
   reg_print("ETH12CKSELR", (uint32_t)&rcc->ETH12CKSELR, rcc->ETH12CKSELR);
   reg_print("USBCKSELR", (uint32_t)&rcc->USBCKSELR, rcc->USBCKSELR);
   reg_print("QSPICKSELR", (uint32_t)&rcc->QSPICKSELR, rcc->QSPICKSELR);
   reg_print("FMCCKSELR", (uint32_t)&rcc->FMCCKSELR, rcc->FMCCKSELR);
   reg_print("RNG1CKSELR", (uint32_t)&rcc->RNG1CKSELR, rcc->RNG1CKSELR);
   reg_print("STGENCKSELR", (uint32_t)&rcc->STGENCKSELR, rcc->STGENCKSELR);
   reg_print("DCMIPPCKSELR", (uint32_t)&rcc->DCMIPPCKSELR, rcc->DCMIPPCKSELR);
   reg_print("SAESCKSELR", (uint32_t)&rcc->SAESCKSELR, rcc->SAESCKSELR);
   reg_print("APB1RSTSETR", (uint32_t)&rcc->APB1RSTSETR, rcc->APB1RSTSETR);
   reg_print("APB1RSTCLRR", (uint32_t)&rcc->APB1RSTCLRR, rcc->APB1RSTCLRR);
   reg_print("APB2RSTSETR", (uint32_t)&rcc->APB2RSTSETR, rcc->APB2RSTSETR);
   reg_print("APB2RSTCLRR", (uint32_t)&rcc->APB2RSTCLRR, rcc->APB2RSTCLRR);
   reg_print("APB3RSTSETR", (uint32_t)&rcc->APB3RSTSETR, rcc->APB3RSTSETR);
   reg_print("APB3RSTCLRR", (uint32_t)&rcc->APB3RSTCLRR, rcc->APB3RSTCLRR);
   reg_print("APB4RSTSETR", (uint32_t)&rcc->APB4RSTSETR, rcc->APB4RSTSETR);
   reg_print("APB4RSTCLRR", (uint32_t)&rcc->APB4RSTCLRR, rcc->APB4RSTCLRR);
   reg_print("APB5RSTSETR", (uint32_t)&rcc->APB5RSTSETR, rcc->APB5RSTSETR);
   reg_print("APB5RSTCLRR", (uint32_t)&rcc->APB5RSTCLRR, rcc->APB5RSTCLRR);
   reg_print("APB6RSTSETR", (uint32_t)&rcc->APB6RSTSETR, rcc->APB6RSTSETR);
   reg_print("APB6RSTCLRR", (uint32_t)&rcc->APB6RSTCLRR, rcc->APB6RSTCLRR);
   reg_print("AHB2RSTSETR", (uint32_t)&rcc->AHB2RSTSETR, rcc->AHB2RSTSETR);
   reg_print("AHB2RSTCLRR", (uint32_t)&rcc->AHB2RSTCLRR, rcc->AHB2RSTCLRR);
   reg_print("AHB4RSTSETR", (uint32_t)&rcc->AHB4RSTSETR, rcc->AHB4RSTSETR);
   reg_print("AHB4RSTCLRR", (uint32_t)&rcc->AHB4RSTCLRR, rcc->AHB4RSTCLRR);
   reg_print("AHB5RSTSETR", (uint32_t)&rcc->AHB5RSTSETR, rcc->AHB5RSTSETR);
   reg_print("AHB5RSTCLRR", (uint32_t)&rcc->AHB5RSTCLRR, rcc->AHB5RSTCLRR);
   reg_print("AHB6RSTSETR", (uint32_t)&rcc->AHB6RSTSETR, rcc->AHB6RSTSETR);
   reg_print("AHB6RSTCLRR", (uint32_t)&rcc->AHB6RSTCLRR, rcc->AHB6RSTCLRR);
   reg_print("MP_APB1ENSETR", (uint32_t)&rcc->MP_APB1ENSETR,
             rcc->MP_APB1ENSETR);
   reg_print("MP_APB1ENCLRR", (uint32_t)&rcc->MP_APB1ENCLRR,
             rcc->MP_APB1ENCLRR);
   reg_print("MP_APB2ENSETR", (uint32_t)&rcc->MP_APB2ENSETR,
             rcc->MP_APB2ENSETR);
   reg_print("MP_APB2ENCLRR", (uint32_t)&rcc->MP_APB2ENCLRR,
             rcc->MP_APB2ENCLRR);
   reg_print("MP_APB3ENSETR", (uint32_t)&rcc->MP_APB3ENSETR,
             rcc->MP_APB3ENSETR);
   reg_print("MP_APB3ENCLRR", (uint32_t)&rcc->MP_APB3ENCLRR,
             rcc->MP_APB3ENCLRR);
   reg_print("MP_S_APB3ENSETR", (uint32_t)&rcc->MP_S_APB3ENSETR,
             rcc->MP_S_APB3ENSETR);
   reg_print("MP_S_APB3ENCLRR", (uint32_t)&rcc->MP_S_APB3ENCLRR,
             rcc->MP_S_APB3ENCLRR);
   reg_print("MP_NS_APB3ENSETR", (uint32_t)&rcc->MP_NS_APB3ENSETR,
             rcc->MP_NS_APB3ENSETR);
   reg_print("MP_NS_APB3ENCLRR", (uint32_t)&rcc->MP_NS_APB3ENCLRR,
             rcc->MP_NS_APB3ENCLRR);
   reg_print("MP_APB4ENSETR", (uint32_t)&rcc->MP_APB4ENSETR,
             rcc->MP_APB4ENSETR);
   reg_print("MP_APB4ENCLRR", (uint32_t)&rcc->MP_APB4ENCLRR,
             rcc->MP_APB4ENCLRR);
   reg_print("MP_S_APB4ENSETR", (uint32_t)&rcc->MP_S_APB4ENSETR,
             rcc->MP_S_APB4ENSETR);
   reg_print("MP_S_APB4ENCLRR", (uint32_t)&rcc->MP_S_APB4ENCLRR,
             rcc->MP_S_APB4ENCLRR);
   reg_print("MP_NS_APB4ENSETR", (uint32_t)&rcc->MP_NS_APB4ENSETR,
             rcc->MP_NS_APB4ENSETR);
   reg_print("MP_NS_APB4ENCLRR", (uint32_t)&rcc->MP_NS_APB4ENCLRR,
             rcc->MP_NS_APB4ENCLRR);
   reg_print("MP_APB5ENSETR", (uint32_t)&rcc->MP_APB5ENSETR,
             rcc->MP_APB5ENSETR);
   reg_print("MP_APB5ENCLRR", (uint32_t)&rcc->MP_APB5ENCLRR,
             rcc->MP_APB5ENCLRR);
   reg_print("MP_APB6ENSETR", (uint32_t)&rcc->MP_APB6ENSETR,
             rcc->MP_APB6ENSETR);
   reg_print("MP_APB6ENCLRR", (uint32_t)&rcc->MP_APB6ENCLRR,
             rcc->MP_APB6ENCLRR);
   reg_print("MP_AHB2ENSETR", (uint32_t)&rcc->MP_AHB2ENSETR,
             rcc->MP_AHB2ENSETR);
   reg_print("MP_AHB2ENCLRR", (uint32_t)&rcc->MP_AHB2ENCLRR,
             rcc->MP_AHB2ENCLRR);
   reg_print("MP_S_AHB4ENSETR", (uint32_t)&rcc->MP_S_AHB4ENSETR,
             rcc->MP_S_AHB4ENSETR);
   reg_print("MP_S_AHB4ENCLRR", (uint32_t)&rcc->MP_S_AHB4ENCLRR,
             rcc->MP_S_AHB4ENCLRR);
   reg_print("MP_NS_AHB4ENSETR", (uint32_t)&rcc->MP_NS_AHB4ENSETR,
             rcc->MP_NS_AHB4ENSETR);
   reg_print("MP_NS_AHB4ENCLRR", (uint32_t)&rcc->MP_NS_AHB4ENCLRR,
             rcc->MP_NS_AHB4ENCLRR);
   reg_print("MP_AHB5ENSETR", (uint32_t)&rcc->MP_AHB5ENSETR,
             rcc->MP_AHB5ENSETR);
   reg_print("MP_AHB5ENCLRR", (uint32_t)&rcc->MP_AHB5ENCLRR,
             rcc->MP_AHB5ENCLRR);
   reg_print("MP_AHB6ENSETR", (uint32_t)&rcc->MP_AHB6ENSETR,
             rcc->MP_AHB6ENSETR);
   reg_print("MP_AHB6ENCLRR", (uint32_t)&rcc->MP_AHB6ENCLRR,
             rcc->MP_AHB6ENCLRR);
   reg_print("MP_S_AHB6ENSETR", (uint32_t)&rcc->MP_S_AHB6ENSETR,
             rcc->MP_S_AHB6ENSETR);
   reg_print("MP_S_AHB6ENCLRR", (uint32_t)&rcc->MP_S_AHB6ENCLRR,
             rcc->MP_S_AHB6ENCLRR);
   reg_print("MP_NS_AHB6ENSETR", (uint32_t)&rcc->MP_NS_AHB6ENSETR,
             rcc->MP_NS_AHB6ENSETR);
   reg_print("MP_NS_AHB6ENCLRR", (uint32_t)&rcc->MP_NS_AHB6ENCLRR,
             rcc->MP_NS_AHB6ENCLRR);
   reg_print("MP_APB1LPENSETR", (uint32_t)&rcc->MP_APB1LPENSETR,
             rcc->MP_APB1LPENSETR);
   reg_print("MP_APB1LPENCLRR", (uint32_t)&rcc->MP_APB1LPENCLRR,
             rcc->MP_APB1LPENCLRR);
   reg_print("MP_APB2LPENSETR", (uint32_t)&rcc->MP_APB2LPENSETR,
             rcc->MP_APB2LPENSETR);
   reg_print("MP_APB2LPENCLRR", (uint32_t)&rcc->MP_APB2LPENCLRR,
             rcc->MP_APB2LPENCLRR);
   reg_print("MP_APB3LPENSETR", (uint32_t)&rcc->MP_APB3LPENSETR,
             rcc->MP_APB3LPENSETR);
   reg_print("MP_APB3LPENCLRR", (uint32_t)&rcc->MP_APB3LPENCLRR,
             rcc->MP_APB3LPENCLRR);
   reg_print("MP_S_APB3LPENSETR", (uint32_t)&rcc->MP_S_APB3LPENSETR,
             rcc->MP_S_APB3LPENSETR);
   reg_print("MP_S_APB3LPENCLRR", (uint32_t)&rcc->MP_S_APB3LPENCLRR,
             rcc->MP_S_APB3LPENCLRR);
   reg_print("MP_NS_APB3LPENSETR", (uint32_t)&rcc->MP_NS_APB3LPENSETR,
             rcc->MP_NS_APB3LPENSETR);
   reg_print("MP_NS_APB3LPENCLRR", (uint32_t)&rcc->MP_NS_APB3LPENCLRR,
             rcc->MP_NS_APB3LPENCLRR);
   reg_print("MP_APB4LPENSETR", (uint32_t)&rcc->MP_APB4LPENSETR,
             rcc->MP_APB4LPENSETR);
   reg_print("MP_APB4LPENCLRR", (uint32_t)&rcc->MP_APB4LPENCLRR,
             rcc->MP_APB4LPENCLRR);
   reg_print("MP_S_APB4LPENSETR", (uint32_t)&rcc->MP_S_APB4LPENSETR,
             rcc->MP_S_APB4LPENSETR);
   reg_print("MP_S_APB4LPENCLRR", (uint32_t)&rcc->MP_S_APB4LPENCLRR,
             rcc->MP_S_APB4LPENCLRR);
   reg_print("MP_NS_APB4LPENSETR", (uint32_t)&rcc->MP_NS_APB4LPENSETR,
             rcc->MP_NS_APB4LPENSETR);
   reg_print("MP_NS_APB4LPENCLRR", (uint32_t)&rcc->MP_NS_APB4LPENCLRR,
             rcc->MP_NS_APB4LPENCLRR);
   reg_print("MP_APB5LPENSETR", (uint32_t)&rcc->MP_APB5LPENSETR,
             rcc->MP_APB5LPENSETR);
   reg_print("MP_APB5LPENCLRR", (uint32_t)&rcc->MP_APB5LPENCLRR,
             rcc->MP_APB5LPENCLRR);
   reg_print("MP_APB6LPENSETR", (uint32_t)&rcc->MP_APB6LPENSETR,
             rcc->MP_APB6LPENSETR);
   reg_print("MP_APB6LPENCLRR", (uint32_t)&rcc->MP_APB6LPENCLRR,
             rcc->MP_APB6LPENCLRR);
   reg_print("MP_AHB2LPENSETR", (uint32_t)&rcc->MP_AHB2LPENSETR,
             rcc->MP_AHB2LPENSETR);
   reg_print("MP_AHB2LPENCLRR", (uint32_t)&rcc->MP_AHB2LPENCLRR,
             rcc->MP_AHB2LPENCLRR);
   reg_print("MP_AHB4LPENSETR", (uint32_t)&rcc->MP_AHB4LPENSETR,
             rcc->MP_AHB4LPENSETR);
   reg_print("MP_AHB4LPENCLRR", (uint32_t)&rcc->MP_AHB4LPENCLRR,
             rcc->MP_AHB4LPENCLRR);
   reg_print("MP_S_AHB4LPENSETR", (uint32_t)&rcc->MP_S_AHB4LPENSETR,
             rcc->MP_S_AHB4LPENSETR);
   reg_print("MP_S_AHB4LPENCLRR", (uint32_t)&rcc->MP_S_AHB4LPENCLRR,
             rcc->MP_S_AHB4LPENCLRR);
   reg_print("MP_NS_AHB4LPENSETR", (uint32_t)&rcc->MP_NS_AHB4LPENSETR,
             rcc->MP_NS_AHB4LPENSETR);
   reg_print("MP_NS_AHB4LPENCLRR", (uint32_t)&rcc->MP_NS_AHB4LPENCLRR,
             rcc->MP_NS_AHB4LPENCLRR);
   reg_print("MP_AHB5LPENSETR", (uint32_t)&rcc->MP_AHB5LPENSETR,
             rcc->MP_AHB5LPENSETR);
   reg_print("MP_AHB5LPENCLRR", (uint32_t)&rcc->MP_AHB5LPENCLRR,
             rcc->MP_AHB5LPENCLRR);
   reg_print("MP_AHB6LPENSETR", (uint32_t)&rcc->MP_AHB6LPENSETR,
             rcc->MP_AHB6LPENSETR);
   reg_print("MP_AHB6LPENCLRR", (uint32_t)&rcc->MP_AHB6LPENCLRR,
             rcc->MP_AHB6LPENCLRR);
   reg_print("MP_S_AHB6LPENSETR", (uint32_t)&rcc->MP_S_AHB6LPENSETR,
             rcc->MP_S_AHB6LPENSETR);
   reg_print("MP_S_AHB6LPENCLRR", (uint32_t)&rcc->MP_S_AHB6LPENCLRR,
             rcc->MP_S_AHB6LPENCLRR);
   reg_print("MP_NS_AHB6LPENSETR", (uint32_t)&rcc->MP_NS_AHB6LPENSETR,
             rcc->MP_NS_AHB6LPENSETR);
   reg_print("MP_NS_AHB6LPENCLRR", (uint32_t)&rcc->MP_NS_AHB6LPENCLRR,
             rcc->MP_NS_AHB6LPENCLRR);
   reg_print("MP_S_AXIMLPENSETR", (uint32_t)&rcc->MP_S_AXIMLPENSETR,
             rcc->MP_S_AXIMLPENSETR);
   reg_print("MP_S_AXIMLPENCLRR", (uint32_t)&rcc->MP_S_AXIMLPENCLRR,
             rcc->MP_S_AXIMLPENCLRR);
   reg_print("MP_NS_AXIMLPENSETR", (uint32_t)&rcc->MP_NS_AXIMLPENSETR,
             rcc->MP_NS_AXIMLPENSETR);
   reg_print("MP_NS_AXIMLPENCLRR", (uint32_t)&rcc->MP_NS_AXIMLPENCLRR,
             rcc->MP_NS_AXIMLPENCLRR);
   reg_print("MP_MLAHBLPENSETR", (uint32_t)&rcc->MP_MLAHBLPENSETR,
             rcc->MP_MLAHBLPENSETR);
   reg_print("MP_MLAHBLPENCLRR", (uint32_t)&rcc->MP_MLAHBLPENCLRR,
             rcc->MP_MLAHBLPENCLRR);
   reg_print("APB3SECSR", (uint32_t)&rcc->APB3SECSR, rcc->APB3SECSR);
   reg_print("APB4SECSR", (uint32_t)&rcc->APB4SECSR, rcc->APB4SECSR);
   reg_print("APB5SECSR", (uint32_t)&rcc->APB5SECSR, rcc->APB5SECSR);
   reg_print("APB6SECSR", (uint32_t)&rcc->APB6SECSR, rcc->APB6SECSR);
   reg_print("AHB2SECSR", (uint32_t)&rcc->AHB2SECSR, rcc->AHB2SECSR);
   reg_print("AHB4SECSR", (uint32_t)&rcc->AHB4SECSR, rcc->AHB4SECSR);
   reg_print("AHB5SECSR", (uint32_t)&rcc->AHB5SECSR, rcc->AHB5SECSR);
   reg_print("AHB6SECSR", (uint32_t)&rcc->AHB6SECSR, rcc->AHB6SECSR);
   reg_print("VERR", (uint32_t)&rcc->VERR, rcc->VERR);
   reg_print("IDR", (uint32_t)&rcc->IDR, rcc->IDR);
   reg_print("SIDR", (uint32_t)&rcc->SIDR, rcc->SIDR);
}

void reg_dump(int x0, uint32_t x1, uint32_t x2, uint32_t x3)
{
   (void)x0;
   (void)x1;
   (void)x2;
   (void)x3;

   reg_tim();
   reg_gpio();
   reg_rcc();
}

// end file reg.c
