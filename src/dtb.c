// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file dtb.c
 * @brief In-memory DTB patching
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "dtb.h"
#include "board.h"

#ifdef NAND_FLASH

#include "defaults.h"
#include "printf.h"
#include <stdint.h>
#include <string.h>

/* FDT token values (structure block, big-endian). */
#define FDT_MAGIC      0xD00DFEEDU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE   2U
#define FDT_PROP       3U
#define FDT_NOP        4U
#define FDT_END        9U

static inline uint32_t fdt_be32(const uint32_t *p)
{
   const uint8_t *b = (const uint8_t *)p;
   return ((uint32_t)b[0] << 24U) | ((uint32_t)b[1] << 16U) |
          ((uint32_t)b[2] << 8U) | (uint32_t)b[3];
}

static inline void fdt_put_be32(uint32_t *p, uint32_t v)
{
   uint8_t *b = (uint8_t *)p;
   b[0]       = (uint8_t)(v >> 24U);
   b[1]       = (uint8_t)(v >> 16U);
   b[2]       = (uint8_t)(v >> 8U);
   b[3]       = (uint8_t)v;
}

/*
 * Remove UBI boot tokens from bootargs string in-place.
 * Drops: "ubi.mtd=rootfs", "root=ubi0:rootfs", "rootfstype=ubifs".
 * Preserves all other tokens (e.g. "clk_ignore_unused").
 * The string may only shrink, so patching in-place is safe.
 */
static void strip_ubi_bootargs(char *args)
{
   static const char *const drop[] = {
       "ubi.mtd=rootfs",
       "root=ubi0:rootfs",
       "rootfstype=ubifs",
       NULL,
   };

   char buf[256];
   char *out      = buf;
   char *end      = buf + sizeof(buf) - 1U;
   int need_space = 0;

   char *tok = args;
   while (*tok != '\0') {
      /* skip leading spaces */
      while (*tok == ' ')
         tok++;
      if (*tok == '\0')
         break;

      /* find end of token */
      char *ts = tok;
      while (*tok != ' ' && *tok != '\0')
         tok++;
      const size_t tlen = (size_t)(tok - ts);

      /* check if token should be dropped */
      int drop_it = 0;
      for (int i = 0; drop[i] != NULL; i++) {
         if (strlen(drop[i]) == tlen && memcmp(ts, drop[i], tlen) == 0) {
            drop_it = 1;
            break;
         }
      }
      if (drop_it)
         continue;

      /* append to output */
      if (need_space && out < end)
         *out++ = ' ';
      size_t copy = tlen;
      if ((size_t)(end - out) < copy)
         copy = (size_t)(end - out);
      memcpy(out, ts, copy);
      out += copy;
      need_space = 1;
   }
   *out = '\0';

   /* write back and zero-pad the remainder */
   const size_t new_len = (size_t)(out - buf);
   memcpy(args, buf, new_len + 1U);
}

/* Patch a single property in /chosen. Updates *found and *args_patched. */
static void patch_chosen_prop(const char *pname, uint32_t plen, uint32_t *pdata,
                              uint32_t initrd_start, uint32_t initrd_end,
                              int *found, int *args_patched)
{
   if (strcmp(pname, "linux,initrd-start") == 0) {
      if (plen == 8U) {
         fdt_put_be32(pdata, 0U);
         fdt_put_be32(pdata + 1, initrd_start);
      } else {
         fdt_put_be32(pdata, initrd_start);
      }
      (*found)++;
   } else if (strcmp(pname, "linux,initrd-end") == 0) {
      if (plen == 8U) {
         fdt_put_be32(pdata, 0U);
         fdt_put_be32(pdata + 1, initrd_end);
      } else {
         fdt_put_be32(pdata, initrd_end);
      }
      (*found)++;
   } else if (strcmp(pname, "bootargs") == 0) {
      strip_ubi_bootargs((char *)pdata);
      *args_patched = 1;
   }
}

int dtb_patch_initrd(uint32_t initrd_start, uint32_t initrd_end)
{
   uint32_t *fdt = (uint32_t *)DEF_DTB_ADDR;

   if (fdt_be32(fdt) != FDT_MAGIC) {
      my_printf("dtb_patch: bad FDT magic\r\n");
      return -1;
   }

   /* FDT header layout (each field is one big-endian uint32):
    *  [0] magic  [1] totalsize  [2] off_dt_struct  [3] off_dt_strings ... */
   const uint32_t off_struct  = fdt_be32(fdt + 2);
   const uint32_t off_strings = fdt_be32(fdt + 3);
   const char *strtab         = (const char *)fdt + off_strings;

   uint32_t *p      = (uint32_t *)((uint8_t *)fdt + off_struct);
   int depth        = 0;
   int chosen       = 0; /* 1 while inside /chosen */
   int found        = 0; /* count of patched initrd properties */
   int args_patched = 0;

   for (;;) {
      uint32_t tok = fdt_be32(p++);

      if (tok == FDT_END)
         break;

      if (tok == FDT_NOP)
         continue;

      if (tok == FDT_BEGIN_NODE) {
         const char *name = (const char *)p;
         if (depth == 1 && strcmp(name, "chosen") == 0)
            chosen = 1;
         depth++;
         p += (strlen(name) + 4U) / 4U;
         continue;
      }

      if (tok == FDT_END_NODE) {
         depth--;
         if (chosen && depth == 1)
            chosen = 0;
         continue;
      }

      if (tok == FDT_PROP) {
         const uint32_t plen    = fdt_be32(p++);
         const uint32_t nameoff = fdt_be32(p++);
         if (chosen)
            patch_chosen_prop(strtab + nameoff, plen, p, initrd_start,
                              initrd_end, &found, &args_patched);
         p += (plen + 3U) / 4U;
         continue;
      }

      break; /* unknown token */
   }

   if (found < 2) {
      my_printf(
          "dtb_patch: found %d/2 initrd properties -- add them to .dts\r\n",
          found);
      return -1;
   }
   if (!args_patched) {
      my_printf("dtb_patch: bootargs not found in /chosen\r\n");
      return -1;
   }

   my_printf("dtb_patch: initrd 0x%08lx-0x%08lx\r\n",
             (unsigned long)initrd_start, (unsigned long)initrd_end);
   return 0;
}

#endif /* NAND_FLASH */

/* Suppress -Wpedantic empty translation unit warning. */
typedef int dtb_dummy;

// end file dtb.c
