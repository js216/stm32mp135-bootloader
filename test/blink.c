// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file main.c
 * @brief Application entry point
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "stm32mp135fxx_ca7.h"

#include "core_ca.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>

#define HSE_VALUE 24000000U
#define HSI_VALUE 64000000U

void _putchar(char ch)
{
   while (!(UART4->ISR & USART_ISR_TXE))
      ;
   UART4->TDR = (uint8_t)ch;
   while (!(UART4->ISR & USART_ISR_TC))
      ;
}

static uint32_t get_tick(void)
{
   const uint64_t pl1 = PL1_GetCurrentPhysicalValue();
   if ((RCC->STGENCKSELR & RCC_STGENCKSELR_STGENSRC) ==
       RCC_STGENCKSELR_STGENSRC_0) {
      return (uint32_t)(pl1 / (HSE_VALUE / 1000UL));
   } else {
      return (uint32_t)(pl1 / (HSI_VALUE / 1000UL));
   }
}

static void toggle_pin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
   const uint32_t odr = GPIOx->ODR;
   GPIOx->BSRR        = ((odr & GPIO_Pin) << 16) | (~odr & GPIO_Pin);
}

static void blink(void)
{
   static uint32_t last_blink = 0;
   uint32_t now               = get_tick();
   if (now - last_blink >= 500) {
      last_blink = now;
      toggle_pin(GPIOA, 1 << 13U);
      _putchar('.');
   }
}

int main(void)
{
   my_printf("Blinking: ");
   while (1)
      blink();
}

// end file blink.c
