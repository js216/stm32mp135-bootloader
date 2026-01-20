// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file eth.h
 * @brief Ethernet test
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "eth.h"
#include "board.h"
#include "irq.h"
#include "mcp23x17.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_eth.h"
#include <stdint.h>
#include <string.h>

#define ETH_MAC_ADDR0  0x00U
#define ETH_MAC_ADDR1  0x19U
#define ETH_MAC_ADDR2  0xB3U
#define ETH_MAC_ADDR3  0x12U
#define ETH_MAC_ADDR4  0x00U
#define ETH_MAC_ADDR5  0x00U
#define ETH_TIMEOUT_MS 1000U 

#define LAN8742_ADDR              ((uint16_t)0x0000U)
#define LAN8742_BCR               ((uint16_t)0x0000U)
#define LAN8742_BSR               ((uint16_t)0x0001U)
#define LAN8742_PHYI1R            ((uint16_t)0x0002U)
#define LAN8742_PHYI2R            ((uint16_t)0x0003U)
#define LAN8742_PHYSCSR           ((uint16_t)0x001FU)
#define LAN8742_BCR_SOFT_RESET    ((uint16_t)0x8000U)
#define LAN8742_BCR_AUTONEGO_EN   ((uint16_t)0x1000U)
#define LAN8742_BSR_LINK_STATUS   ((uint16_t)0x0004U)
#define LAN8742_BSR_AUTONEGO_CPLT ((uint16_t)0x0020U)
#define LAN8742_PHYSCSR_10BT_HD   ((uint16_t)0x0004U)
#define LAN8742_PHYSCSR_10BT_FD   ((uint16_t)0x0014U)
#define LAN8742_PHYSCSR_100BTX_HD ((uint16_t)0x0008U)
#define LAN8742_PHYSCSR_100BTX_FD ((uint16_t)0x0018U)
#define LAN8742_PHYID1_EXPECT     ((uint16_t)0x0007U)
#define LAN8742_PHYID2_EXPECT     ((uint16_t)0xC131U)

// global variables
static ETH_HandleTypeDef eth_handle;
static ETH_TxPacketConfigTypeDef tx_conf;
static ETH_DMADescTypeDef rx_dma_desc[ETH_RX_DESC_CNT];
static ETH_DMADescTypeDef tx_dma_desc[ETH_TX_DESC_CNT];

void ETH1_IRQHandler(void)
{
   HAL_ETH_IRQHandler(&eth_handle);
}

static void eth_pin_init()
{
   GPIO_InitTypeDef init = {0};
   init.Speed = GPIO_SPEED_FREQ_HIGH;
   init.Mode = GPIO_MODE_AF_PP;
   init.Pull = GPIO_NOPULL;

   // Configure PA1, PA2
   init.Pin =  GPIO_PIN_1 | GPIO_PIN_2;
   init.Alternate = GPIO_AF11_ETH;
   HAL_GPIO_Init(GPIOA, &init);

   // Configure PB11
   init.Pin = GPIO_PIN_11;
   init.Alternate = GPIO_AF11_ETH;
   HAL_GPIO_Init(GPIOB, &init);

   // Configure PC1
   init.Pin = GPIO_PIN_1;
   init.Alternate = GPIO_AF10_ETH;
   HAL_GPIO_Init(GPIOC, &init);

   // Configure PC4 and PC5
   init.Pin = GPIO_PIN_4 | GPIO_PIN_5;
   init.Alternate = GPIO_AF11_ETH;
   HAL_GPIO_Init(GPIOC, &init);

   // Configure PG2, PG13 and PG14
   init.Pin = GPIO_PIN_2 | GPIO_PIN_13 | GPIO_PIN_14;
   HAL_GPIO_Init(GPIOG, &init);

#if (USE_MCP23x17 == 1)
   mcp_init();
   mcp_set_pin_mode(MCP_PIN_9, true);
   mcp_pin_write(MCP_PIN_9, true);
#endif
}

/**
 * @brief Initialize LAN8742 PHY over MDIO.
 *
 * Performs a software reset, enables auto-negotiation, and verifies
 * that the PHY responds over MDIO. Does not wait for link-up,
 * so can be called with no cable connected.
 *
 * @retval 0  PHY responded over MDIO, initialization complete.
 * @retval -1 MDIO access failed.
 */
static int eth_phy_init(void)
{
   uint32_t v;
   uint32_t id1, id2;
   uint32_t t0;

   HAL_ETH_SetMDIOClockRange(&eth_handle);

   /* Reset PHY */
   if (HAL_ETH_WritePHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_BCR, LAN8742_BCR_SOFT_RESET) != HAL_OK) {
      my_printf("PHY reset write failed\r\n");
      return -1;
   }

   /* Wait for reset to complete (typically a few ms) */
   t0 = HAL_GetTick();
   do {
      if (HAL_ETH_ReadPHYRegister(&eth_handle, LAN8742_ADDR,
	       LAN8742_BCR, &v) != HAL_OK) {
	 my_printf("PHY BCR read failed\r\n");
	 return -1;
      }
      if ((HAL_GetTick() - t0) > ETH_TIMEOUT_MS) {
	 my_printf("PHY reset timeout\r\n");
	 return -1;
      }
   } while (v & LAN8742_BCR_SOFT_RESET);

   /* Enable auto-negotiation */
   v |= LAN8742_BCR_AUTONEGO_EN;
   if (HAL_ETH_WritePHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_BCR, v) != HAL_OK) {
      my_printf("Enable auto-negotiation failed\r\n");
      return -1;
   }

   /* Optional: read PHY ID to verify MDIO communication */
   if (HAL_ETH_ReadPHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_PHYI1R, &id1) != HAL_OK ||
	 HAL_ETH_ReadPHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_PHYI2R, &id2) != HAL_OK) {
      my_printf("PHY ID read failed\r\n");
      return -1;
   }

   // Check ID of the PHY is correct
   if ((id1 != LAN8742_PHYID1_EXPECT) || (id2 != LAN8742_PHYID2_EXPECT)) {
      my_printf("Unexpected PHY ID: 0x%04lX 0x%04lX\r\n", id1, id2);
      return -1;
   }

   return 0;
}

/**
 * @brief Read current link speed and duplex status from LAN8742 PHY.
 *
 * Reads the PHY basic status register to verify link is up, then reads the
 * LAN8742 vendor-specific status register to determine negotiated speed and
 * duplex mode. This function does not modify PHY configuration.
 *
 * @param speed_100   Output: set to 1 if link speed is 100 Mbps, 0 if 10 Mbps.
 * @param full_duplex Output: set to 1 if link is full-duplex, 0 if half-duplex.
 *
 * @retval 0  link is up and status fields were read successfully.
 * @retval -1 MDIO access failed or link is currently down.
 */
static int eth_phy_status(int *speed_100, int *full_duplex)
{
   uint32_t v, phy_status;

   /* Read basic status register */
   if (HAL_ETH_ReadPHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_BSR, &v) != HAL_OK) {
      my_printf("PHY BSR read failed\r\n");
      return -1;
   }

   if ((v & LAN8742_BSR_LINK_STATUS) == 0u) {
      my_printf("Link is down (no cable or remote inactive)\r\n");
      return -1;
   }

   /* Read vendor-specific status register */
   if (HAL_ETH_ReadPHYRegister(&eth_handle, LAN8742_ADDR,
	    LAN8742_PHYSCSR, &phy_status) != HAL_OK) {
      my_printf("PHY PHYSCSR read failed\r\n");
      return -1;
   }

   /* Determine speed */
   *speed_100 = (phy_status & (LAN8742_PHYSCSR_100BTX_FD |
	    LAN8742_PHYSCSR_100BTX_HD)) ? 1 : 0;

   /* Determine duplex */
   *full_duplex = (phy_status & (LAN8742_PHYSCSR_10BT_FD |
	    LAN8742_PHYSCSR_100BTX_FD)) ? 1 : 0;

   /* Print full status to user */
   my_printf("Ethernet link is up\r\n");
   my_printf("  Speed: %s Mbps\r\n", *speed_100 ? "100" : "10");
   my_printf("  Duplex: %s\r\n", *full_duplex ? "full" : "half");
   my_printf("  BSR = 0x%04lX, PHYSCSR = 0x%04lX\r\n", v, phy_status);

   return 0;
}

void eth_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   eth_pin_init();

   static uint8_t mac[6];

   eth_handle.Instance = ETH;
   mac[0] = ETH_MAC_ADDR0;
   mac[1] = ETH_MAC_ADDR1;
   mac[2] = ETH_MAC_ADDR2;
   mac[3] = ETH_MAC_ADDR3;
   mac[4] = ETH_MAC_ADDR4;
   mac[5] = ETH_MAC_ADDR5;
   eth_handle.Init.MACAddr = &mac[0];
   eth_handle.Init.MediaInterface = HAL_ETH_RMII_MODE;
   eth_handle.Init.TxDesc = tx_dma_desc;
   eth_handle.Init.RxDesc = rx_dma_desc;
   eth_handle.Init.RxBuffLen = 1536;
   eth_handle.Init.ClockSelection = HAL_ETH1_REF_CLK_RX_CLK_PIN;

   if (HAL_ETH_Init(&eth_handle) != HAL_OK) {
      my_printf("HAL_ETH_Init(&eth_handle) != HAL_OK\r\n");
      return;
   }

   memset(&tx_conf, 0 , sizeof(ETH_TxPacketConfigTypeDef));
   tx_conf.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
   tx_conf.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
   tx_conf.CRCPadCtrl = ETH_CRC_PAD_INSERT;

   IRQ_SetPriority(ETH1_IRQn, PRIO_ETH);
   IRQ_Enable(ETH1_IRQn);

   __HAL_RCC_ETH1CK_CLK_ENABLE();
   __HAL_RCC_ETH1MAC_CLK_ENABLE();
   __HAL_RCC_ETH1TX_CLK_ENABLE();
   __HAL_RCC_ETH1RX_CLK_ENABLE();

   if (eth_phy_init() != 0) {
      my_printf("eth_phy_init() != 0\r\n");
      return;
   }
}

// end file eth.c
