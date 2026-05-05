// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file usb_msc.c
 * @brief Minimal USB MSC Bulk-Only Transport device implementation.
 * @copyright 2026 Jakob Kastelic
 */

#include "usb_msc.h"
#include "debug.h"
#include "defaults.h"
#include "irq.h"
#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "core_ca.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_pcd.h"
#include "stm32mp13xx_hal_pcd_ex.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_rcc_ex.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef NAND_FLASH
#include "sd.h"
#else
#include "fmc.h"
#endif

#define MSC_IN_EP       0x81U
#define MSC_OUT_EP      0x01U
#define EP0_SIZE        64U
#define MSC_PACKET_SIZE 64U
#define MSC_BLOCK_SIZE  512U

#define USB_REQ_GET_STATUS        0x00U
#define USB_REQ_CLEAR_FEATURE     0x01U
#define USB_REQ_SET_FEATURE       0x03U
#define USB_REQ_SET_ADDRESS       0x05U
#define USB_REQ_GET_DESCRIPTOR    0x06U
#define USB_REQ_GET_CONFIGURATION 0x08U
#define USB_REQ_SET_CONFIGURATION 0x09U
#define USB_REQ_GET_INTERFACE     0x0AU
#define USB_REQ_SET_INTERFACE     0x0BU

#define USB_DESC_DEVICE        1U
#define USB_DESC_CONFIGURATION 2U
#define USB_DESC_STRING        3U
#define USB_DESC_DEVICE_QUAL   6U

#define MSC_REQ_RESET       0xFFU
#define MSC_REQ_GET_MAX_LUN 0xFEU

#define CBW_SIGNATURE 0x43425355UL
#define CSW_SIGNATURE 0x53425355UL

#define SCSI_TEST_UNIT_READY       0x00U
#define SCSI_REQUEST_SENSE         0x03U
#define SCSI_INQUIRY               0x12U
#define SCSI_MODE_SENSE6           0x1AU
#define SCSI_START_STOP_UNIT       0x1BU
#define SCSI_PREVENT_ALLOW_REMOVAL 0x1EU
#define SCSI_READ_CAPACITY10       0x25U
#define SCSI_READ10                0x28U
#define SCSI_WRITE10               0x2AU
#define SCSI_VERIFY10              0x2FU
#define SCSI_SYNCHRONIZE_CACHE10   0x35U
#define SCSI_MODE_SENSE10          0x5AU

struct setup_pkt {
   uint8_t bm_request_type;
   uint8_t b_request;
   uint16_t w_value;
   uint16_t w_index;
   uint16_t w_length;
};

struct cbw {
   uint32_t sig;
   uint32_t tag;
   uint32_t data_len;
   uint8_t flags;
   uint8_t lun;
   uint8_t cb_len;
   uint8_t cb[16];
};

struct csw {
   uint32_t sig;
   uint32_t tag;
   uint32_t residue;
   uint8_t status;
};

enum bot_state {
   BOT_WAIT_CBW,
   BOT_DATA_IN,
   BOT_DATA_OUT,
   BOT_SEND_CSW,
};

static PCD_HandleTypeDef hpcd;
static uint8_t configured;
static uint8_t ep0_wait_status_out;
static enum bot_state bot_state;
static struct cbw cbw;
static struct csw csw;
static uint32_t data_lba;
static uint32_t data_blocks;
static uint32_t data_sent;
static uint8_t *data_ptr;
static uint32_t data_len;
static uint8_t sense_key;
static uint8_t sense_asc;
static uint8_t sense_ascq;

static uint8_t ep0_status[2] __attribute__((aligned(32)));
static uint8_t cbw_buf[31] __attribute__((aligned(32)));
static uint8_t csw_buf[13] __attribute__((aligned(32)));
static uint8_t block_buf[MSC_BLOCK_SIZE] __attribute__((aligned(32)));
static uint8_t ctrl_buf[64] __attribute__((aligned(32)));

static const uint8_t dev_desc[] = {
    18, 1, 0x00, 0x02, 0, 0, 0, EP0_SIZE, 0x83, 0x04, 0x1d, 0x57, 0x00, 0x01,
    1,  2, 3,    1,
};

static const uint8_t cfg_desc[] = {
    9,  2, 32, 0, 1, 1, 0, 0x80, 50,
    9,  4, 0,  0, 2, 8, 6, 0x50, 0,
    7,  5, MSC_IN_EP,  2, MSC_PACKET_SIZE, 0, 0,
    7,  5, MSC_OUT_EP, 2, MSC_PACKET_SIZE, 0, 0,
};

static const uint8_t qual_desc[] = {
    10, 6, 0x00, 0x02, 0, 0, 0, EP0_SIZE, 1, 0,
};

static const uint8_t lang_desc[] = {4, USB_DESC_STRING, 0x09, 0x04};

static uint16_t rd16(const uint8_t *p)
{
   return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32be(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
          ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t rd16be(const uint8_t *p)
{
   return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static void wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static void wr32be(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void set_sense(uint8_t key, uint8_t asc, uint8_t ascq)
{
   sense_key  = key;
   sense_asc  = asc;
   sense_ascq = ascq;
}

static uint32_t storage_blocks(void)
{
#ifndef NAND_FLASH
   return sd_block_count();
#else
   return FMC_DDR_BUF_SIZE / MSC_BLOCK_SIZE;
#endif
}

static int storage_read(uint32_t lba, uint8_t *buf)
{
#ifndef NAND_FLASH
   return sd_read_blocks(lba, buf, 1U);
#else
   memcpy(buf, (const void *)(FMC_DDR_BUF_ADDR + (lba * MSC_BLOCK_SIZE)),
          MSC_BLOCK_SIZE);
   L1C_CleanInvalidateDCacheAll();
   return 0;
#endif
}

static int storage_write(uint32_t lba, const uint8_t *buf)
{
#ifndef NAND_FLASH
   return sd_write_blocks(lba, buf, 1U);
#else
   memcpy((void *)(FMC_DDR_BUF_ADDR + (lba * MSC_BLOCK_SIZE)), buf,
          MSC_BLOCK_SIZE);
   fmc_note_usb_write(lba, 1U);
   L1C_CleanDCacheAll();
   return 0;
#endif
}

static void ep0_send(const uint8_t *buf, uint16_t len, uint16_t req_len)
{
   if (len > req_len)
      len = req_len;
   L1C_CleanDCacheAll();
   ep0_wait_status_out = 1U;
   (void)HAL_PCD_EP_Transmit(&hpcd, 0x80U, (uint8_t *)buf, len);
}

static void ep0_zlp(void)
{
   ep0_wait_status_out = 0U;
   (void)HAL_PCD_EP_Transmit(&hpcd, 0x80U, ctrl_buf, 0U);
}

static void ep0_stall(void)
{
   (void)HAL_PCD_EP_SetStall(&hpcd, 0x80U);
   (void)HAL_PCD_EP_SetStall(&hpcd, 0x00U);
}

static uint16_t make_string(uint8_t idx)
{
   const char *s = "";
   if (idx == 1U)
      s = "Stanford Research Systems";
   else if (idx == 2U)
      s = "STM32MP135 MSC";
   else if (idx == 3U)
#ifdef EVB
      s = "002800425115";
#else
      s = "001E00065113";
#endif
   else
      return 0U;

   uint16_t n = 2U;
   while (*s != '\0' && n < sizeof(ctrl_buf) - 1U) {
      ctrl_buf[n++] = (uint8_t)*s++;
      ctrl_buf[n++] = 0U;
   }
   ctrl_buf[0] = (uint8_t)n;
   ctrl_buf[1] = USB_DESC_STRING;
   return n;
}

static void bot_recv_cbw(void)
{
   bot_state = BOT_WAIT_CBW;
   L1C_CleanInvalidateDCacheAll();
   (void)HAL_PCD_EP_Receive(&hpcd, MSC_OUT_EP, cbw_buf, sizeof(cbw_buf));
}

static void open_msc_eps(void)
{
   (void)HAL_PCD_EP_Open(&hpcd, MSC_IN_EP, MSC_PACKET_SIZE, EP_TYPE_BULK);
   (void)HAL_PCD_EP_Open(&hpcd, MSC_OUT_EP, MSC_PACKET_SIZE, EP_TYPE_BULK);
   bot_recv_cbw();
}

static void send_csw(uint8_t status)
{
   csw.sig = CSW_SIGNATURE;
   wr32(&csw_buf[0], csw.sig);
   wr32(&csw_buf[4], csw.tag);
   wr32(&csw_buf[8], csw.residue);
   csw_buf[12] = status;
   bot_state   = BOT_SEND_CSW;
   L1C_CleanDCacheAll();
   (void)HAL_PCD_EP_Transmit(&hpcd, MSC_IN_EP, csw_buf, sizeof(csw_buf));
}

static void data_in_start(uint8_t *buf, uint32_t len)
{
   uint32_t xfer = len;
   data_ptr      = buf;
   data_len      = len;
   data_sent     = 0U;
   bot_state     = BOT_DATA_IN;
   if (xfer > cbw.data_len)
      xfer = cbw.data_len;
   data_len = xfer;
   if (xfer == 0U) {
      send_csw(0U);
      return;
   }
   L1C_CleanDCacheAll();
   (void)HAL_PCD_EP_Transmit(&hpcd, MSC_IN_EP, data_ptr, xfer);
}

static void data_in_next_block(void)
{
   if (data_blocks == 0U) {
      send_csw(0U);
      return;
   }
   if (storage_read(data_lba, block_buf) != 0) {
      set_sense(0x03U, 0x11U, 0x00U);
      send_csw(1U);
      return;
   }
   data_lba++;
   data_blocks--;
   csw.residue = (csw.residue >= MSC_BLOCK_SIZE) ? csw.residue - MSC_BLOCK_SIZE
                                                 : 0U;
   L1C_CleanDCacheAll();
   (void)HAL_PCD_EP_Transmit(&hpcd, MSC_IN_EP, block_buf, MSC_BLOCK_SIZE);
}

static void scsi_good_no_data(void)
{
   if (cbw.data_len != 0U)
      csw.residue = cbw.data_len;
   send_csw(0U);
}

static void scsi_fail(void)
{
   csw.residue = cbw.data_len;
   send_csw(1U);
}

static void handle_cbw(void)
{
   memcpy(&cbw, cbw_buf, sizeof(cbw));
   csw.tag     = cbw.tag;
   csw.residue = cbw.data_len;

   if (cbw.sig != CBW_SIGNATURE || cbw.cb_len == 0U || cbw.cb_len > 16U) {
      (void)HAL_PCD_EP_SetStall(&hpcd, MSC_IN_EP);
      (void)HAL_PCD_EP_SetStall(&hpcd, MSC_OUT_EP);
      return;
   }

   uint8_t op       = cbw.cb[0];
   uint32_t blocks  = 0U;
   uint32_t lba     = 0U;
   uint32_t capacity = storage_blocks();
   set_sense(0U, 0U, 0U);
   memset(ctrl_buf, 0, sizeof(ctrl_buf));

   switch (op) {
      case SCSI_TEST_UNIT_READY:
      case SCSI_START_STOP_UNIT:
      case SCSI_PREVENT_ALLOW_REMOVAL:
      case SCSI_VERIFY10:
      case SCSI_SYNCHRONIZE_CACHE10: scsi_good_no_data(); break;

      case SCSI_INQUIRY:
         ctrl_buf[0] = 0x00U;
         ctrl_buf[1] = 0x80U;
         ctrl_buf[2] = 0x05U;
         ctrl_buf[3] = 0x02U;
         ctrl_buf[4] = 31U;
         memcpy(&ctrl_buf[8], "SRS     ", 8);
         memcpy(&ctrl_buf[16], "STM32MP135 MSC   ", 16);
         memcpy(&ctrl_buf[32], "0001", 4);
         csw.residue = (cbw.data_len > 36U) ? cbw.data_len - 36U : 0U;
         data_in_start(ctrl_buf, 36U);
         break;

      case SCSI_REQUEST_SENSE:
         ctrl_buf[0]  = 0x70U;
         ctrl_buf[2]  = sense_key;
         ctrl_buf[7]  = 10U;
         ctrl_buf[12] = sense_asc;
         ctrl_buf[13] = sense_ascq;
         csw.residue  = (cbw.data_len > 18U) ? cbw.data_len - 18U : 0U;
         data_in_start(ctrl_buf, 18U);
         break;

      case SCSI_READ_CAPACITY10:
         if (capacity == 0U) {
            set_sense(0x02U, 0x3AU, 0x00U);
            scsi_fail();
            break;
         }
         wr32be(&ctrl_buf[0], capacity - 1U);
         wr32be(&ctrl_buf[4], MSC_BLOCK_SIZE);
         csw.residue = (cbw.data_len > 8U) ? cbw.data_len - 8U : 0U;
         data_in_start(ctrl_buf, 8U);
         break;

      case SCSI_MODE_SENSE6:
         ctrl_buf[0] = 3U;
         csw.residue = (cbw.data_len > 4U) ? cbw.data_len - 4U : 0U;
         data_in_start(ctrl_buf, 4U);
         break;

      case SCSI_MODE_SENSE10:
         ctrl_buf[1] = 6U;
         csw.residue = (cbw.data_len > 8U) ? cbw.data_len - 8U : 0U;
         data_in_start(ctrl_buf, 8U);
         break;

      case SCSI_READ10:
         lba    = rd32be(&cbw.cb[2]);
         blocks = rd16be(&cbw.cb[7]);
         if (blocks == 0U) {
            scsi_good_no_data();
            break;
         }
         if (lba + blocks > capacity || cbw.data_len < blocks * MSC_BLOCK_SIZE) {
            set_sense(0x05U, 0x21U, 0x00U);
            scsi_fail();
            break;
         }
         data_lba    = lba;
         data_blocks = blocks;
         bot_state   = BOT_DATA_IN;
         data_in_next_block();
         break;

      case SCSI_WRITE10:
         lba    = rd32be(&cbw.cb[2]);
         blocks = rd16be(&cbw.cb[7]);
         if (blocks == 0U) {
            scsi_good_no_data();
            break;
         }
         if (lba + blocks > capacity || cbw.data_len < blocks * MSC_BLOCK_SIZE) {
            set_sense(0x05U, 0x21U, 0x00U);
            scsi_fail();
            break;
         }
         data_lba    = lba;
         data_blocks = blocks;
         bot_state   = BOT_DATA_OUT;
         L1C_CleanInvalidateDCacheAll();
         (void)HAL_PCD_EP_Receive(&hpcd, MSC_OUT_EP, block_buf, MSC_BLOCK_SIZE);
         break;

      default:
         set_sense(0x05U, 0x20U, 0x00U);
         scsi_fail();
         break;
   }
}

static void handle_setup(void)
{
   uint8_t *s = (uint8_t *)hpcd.Setup;
   struct setup_pkt setup = {
       s[0], s[1], rd16(&s[2]), rd16(&s[4]), rd16(&s[6]),
   };
   uint8_t desc_type = (uint8_t)(setup.w_value >> 8);
   uint8_t desc_idx  = (uint8_t)setup.w_value;
   const uint8_t *buf = NULL;
   uint16_t len       = 0U;

   if ((setup.bm_request_type & 0x60U) == 0x20U) {
      if (setup.b_request == MSC_REQ_GET_MAX_LUN &&
          (setup.bm_request_type & 0x80U) != 0U) {
         ctrl_buf[0] = 0U;
         ep0_send(ctrl_buf, 1U, setup.w_length);
      } else if (setup.b_request == MSC_REQ_RESET &&
                 (setup.bm_request_type & 0x80U) == 0U) {
         (void)HAL_PCD_EP_ClrStall(&hpcd, MSC_IN_EP);
         (void)HAL_PCD_EP_ClrStall(&hpcd, MSC_OUT_EP);
         bot_recv_cbw();
         ep0_zlp();
      } else {
         ep0_stall();
      }
      return;
   }

   switch (setup.b_request) {
      case USB_REQ_GET_DESCRIPTOR:
         if (desc_type == USB_DESC_DEVICE) {
            buf = dev_desc;
            len = sizeof(dev_desc);
         } else if (desc_type == USB_DESC_CONFIGURATION) {
            buf = cfg_desc;
            len = sizeof(cfg_desc);
         } else if (desc_type == USB_DESC_DEVICE_QUAL) {
            buf = qual_desc;
            len = sizeof(qual_desc);
         } else if (desc_type == USB_DESC_STRING && desc_idx == 0U) {
            buf = lang_desc;
            len = sizeof(lang_desc);
         } else if (desc_type == USB_DESC_STRING) {
            len = make_string(desc_idx);
            buf = ctrl_buf;
         }
         if (buf != NULL && len != 0U)
            ep0_send(buf, len, setup.w_length);
         else
            ep0_stall();
         break;

      case USB_REQ_SET_ADDRESS:
         (void)HAL_PCD_SetAddress(&hpcd, (uint8_t)setup.w_value);
         ep0_zlp();
         break;

      case USB_REQ_SET_CONFIGURATION:
         configured = (uint8_t)setup.w_value;
         if (configured != 0U)
            open_msc_eps();
         ep0_zlp();
         break;

      case USB_REQ_GET_CONFIGURATION:
         ctrl_buf[0] = configured;
         ep0_send(ctrl_buf, 1U, setup.w_length);
         break;

      case USB_REQ_GET_INTERFACE:
         ctrl_buf[0] = 0U;
         ep0_send(ctrl_buf, 1U, setup.w_length);
         break;

      case USB_REQ_SET_INTERFACE: ep0_zlp(); break;

      case USB_REQ_GET_STATUS:
         ep0_status[0] = 0U;
         ep0_status[1] = 0U;
         ep0_send(ep0_status, 2U, setup.w_length);
         break;

      case USB_REQ_CLEAR_FEATURE:
         if ((setup.bm_request_type & 0x1FU) == 0x02U &&
             setup.w_value == 0U) {
            (void)HAL_PCD_EP_ClrStall(&hpcd, (uint8_t)setup.w_index);
            ep0_zlp();
         } else {
            ep0_zlp();
         }
         break;

      case USB_REQ_SET_FEATURE:
         if ((setup.bm_request_type & 0x1FU) == 0x02U &&
             setup.w_value == 0U) {
            (void)HAL_PCD_EP_SetStall(&hpcd, (uint8_t)setup.w_index);
            ep0_zlp();
         } else {
            ep0_stall();
         }
         break;

      default: ep0_stall(); break;
   }
}

void OTG_IRQHandler(void)
{
   HAL_PCD_IRQHandler(&hpcd);
}

void HAL_PCD_MspInit(PCD_HandleTypeDef *ph)
{
   (void)ph;
   RCC_PeriphCLKInitTypeDef pclk = {0};
   pclk.PeriphClockSelection     = RCC_PERIPHCLK_USBPHY | RCC_PERIPHCLK_USBO;
   pclk.UsbphyClockSelection     = RCC_USBPHYCLKSOURCE_HSE;
   pclk.UsboClockSelection       = RCC_USBOCLKSOURCE_PHY;
   if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK)
      ERROR("USB clocks");

   __HAL_RCC_USBPHY_CLK_ENABLE();
   __HAL_RCC_USBO_CLK_ENABLE();
   __HAL_RCC_USBPHY_FORCE_RESET();
   __HAL_RCC_USBPHY_RELEASE_RESET();
   __HAL_RCC_USBO_FORCE_RESET();
   __HAL_RCC_USBO_RELEASE_RESET();

   SET_BIT(PWR->CR3, PWR_CR3_USB33DEN);
   for (uint32_t n = 0; (READ_BIT(PWR->CR3, PWR_CR3_USB33RDY) == 0U) &&
                        n < 100000U;
        n++) {
      __asm__ volatile("nop");
   }

   CLEAR_BIT(USBPHYC->MISC, USBPHYC_MISC_SWITHOST);

   IRQ_SetPriority(OTG_IRQn, PRIO_USB);
   IRQ_Enable(OTG_IRQn);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *ph)
{
   (void)ph;
   configured      = 0U;
   bot_state       = BOT_WAIT_CBW;
   (void)HAL_PCD_EP_Open(&hpcd, 0x00U, EP0_SIZE, EP_TYPE_CTRL);
   (void)HAL_PCD_EP_Open(&hpcd, 0x80U, EP0_SIZE, EP_TYPE_CTRL);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *ph)
{
   (void)ph;
   handle_setup();
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *ph, uint8_t epnum)
{
   (void)ph;
   if (epnum == 0U) {
      if (ep0_wait_status_out != 0U) {
         ep0_wait_status_out = 0U;
         (void)HAL_PCD_EP_Receive(&hpcd, 0U, ctrl_buf, 0U);
      } else {
         (void)USB_EP0_OutStart(hpcd.Instance,
                                (uint8_t)hpcd.Init.dma_enable,
                                (uint8_t *)hpcd.Setup);
      }
      return;
   }
   if (epnum != (MSC_IN_EP & 0x7FU))
      return;

   if (bot_state == BOT_DATA_IN && data_blocks != 0U) {
      data_in_next_block();
   } else if (bot_state == BOT_DATA_IN) {
      send_csw(0U);
   } else if (bot_state == BOT_SEND_CSW) {
      bot_recv_cbw();
   }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *ph, uint8_t epnum)
{
   (void)ph;
   if (epnum != (MSC_OUT_EP & 0x7FU))
      return;

   uint32_t rx = HAL_PCD_EP_GetRxCount(&hpcd, MSC_OUT_EP);
   if (bot_state == BOT_WAIT_CBW && rx == sizeof(cbw_buf)) {
      L1C_CleanInvalidateDCacheAll();
      handle_cbw();
   } else if (bot_state == BOT_DATA_OUT && rx == MSC_BLOCK_SIZE) {
      L1C_CleanInvalidateDCacheAll();
      if (storage_write(data_lba, block_buf) != 0) {
         set_sense(0x03U, 0x0CU, 0x00U);
         send_csw(1U);
         return;
      }
      data_lba++;
      data_blocks--;
      csw.residue = (csw.residue >= MSC_BLOCK_SIZE) ? csw.residue - MSC_BLOCK_SIZE
                                                    : 0U;
      if (data_blocks == 0U) {
         send_csw(0U);
      } else {
         (void)HAL_PCD_EP_Receive(&hpcd, MSC_OUT_EP, block_buf, MSC_BLOCK_SIZE);
      }
   } else {
      bot_recv_cbw();
   }
}

void usb_msc_init(void)
{
   hpcd.Instance = USB_OTG_HS;
   hpcd.Init.dev_endpoints = 4U;
   hpcd.Init.speed = PCD_SPEED_FULL;
   hpcd.Init.dma_enable = 0U;
   hpcd.Init.phy_itface = PCD_PHY_UTMI;
   hpcd.Init.Sof_enable = 0U;
   hpcd.Init.low_power_enable = 0U;
   hpcd.Init.lpm_enable = 0U;
   hpcd.Init.battery_charging_enable = 0U;
#ifdef EVB
   hpcd.Init.vbus_sensing_enable = 1U;
#else
   hpcd.Init.vbus_sensing_enable = 0U;
#endif
   hpcd.Init.use_dedicated_ep1 = 0U;
   hpcd.Init.use_external_vbus = 0U;

   if (HAL_PCD_Init(&hpcd) != HAL_OK)
      ERROR("USB PCD init");
   (void)HAL_PCDEx_SetRxFiFo(&hpcd, 0x200U);
   (void)HAL_PCDEx_SetTxFiFo(&hpcd, 0U, 0x40U);
   (void)HAL_PCDEx_SetTxFiFo(&hpcd, 1U, 0x200U);
   HAL_Delay(250U);
   if (HAL_PCD_Start(&hpcd) != HAL_OK)
      ERROR("USB PCD start");
}
