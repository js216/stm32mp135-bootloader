// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file dtb.c
 * @brief In-memory DTB patching
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "board.h"
#include "dtb.h"

#ifdef PARSE_DTB

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
   return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
        | ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
}

static inline void fdt_put_be32(uint32_t *p, uint32_t v)
{
   uint8_t *b = (uint8_t *)p;
   b[0] = (uint8_t)(v >> 24);
   b[1] = (uint8_t)(v >> 16);
   b[2] = (uint8_t)(v >> 8);
   b[3] = (uint8_t) v;
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
   const char    *strtab      = (const char *)fdt + off_strings;

   uint32_t *p      = (uint32_t *)((uint8_t *)fdt + off_struct);
   int        depth = 0;
   int        chosen = 0; /* 1 while inside /chosen */
   int        found  = 0; /* count of patched properties */

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
         const char    *pname   = strtab + nameoff;

         if (chosen) {
            if (strcmp(pname, "linux,initrd-start") == 0) {
               if (plen == 8) { fdt_put_be32(p, 0); fdt_put_be32(p + 1, initrd_start); }
               else           { fdt_put_be32(p, initrd_start); }
               found++;
            } else if (strcmp(pname, "linux,initrd-end") == 0) {
               if (plen == 8) { fdt_put_be32(p, 0); fdt_put_be32(p + 1, initrd_end); }
               else           { fdt_put_be32(p, initrd_end); }
               found++;
            }
         }
         p += (plen + 3U) / 4U;
         continue;
      }

      break; /* unknown token */
   }

   if (found < 2) {
      my_printf("dtb_patch: found %d/2 initrd properties — add them to .dts\r\n",
                found);
      return -1;
   }

   my_printf("dtb_patch: initrd 0x%08lx-0x%08lx\r\n",
             (unsigned long)initrd_start, (unsigned long)initrd_end);
   return 0;
}

#endif /* PARSE_DTB */

/* Suppress -Wpedantic empty translation unit warning. */
typedef int dtb_dummy;
