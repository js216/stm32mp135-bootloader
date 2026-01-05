// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file cmd.c
 * @brief Command line interface
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "stm32mp135fxx_ca7.h"
#include "cmd.h"
#include "boot.h"
#include "ddr.h"
#include "defaults.h"
#include "diag.h"
#include "printf.h"
#include "sd.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RXBUF_SIZE   64
#define CMD_MAX_LEN  32
#define HISTORY_SIZE 8

struct cmd_defaults {
   const char *label;   /* logical item name, e.g. "linux", "dtb" */
   uint32_t len_blocks; /* length in SD-card blocks */
   uint32_t sd_block;   /* starting SD-card block */
   uint32_t dest_addr;  /* destination memory address */
};

struct cmd {
   const char *name;
   const char *syntax;
   const char *summary;
   const struct cmd_defaults *defaults;
   size_t num_defaults;
   void (*handler)(int argc, uint32_t, uint32_t, uint32_t);
};

static const struct cmd cmd_list[] = {
    {
     .name         = "help",
     .syntax       = "",
     .summary      = "Display this help message",
     .defaults     = NULL,
     .num_defaults = 0,
     .handler      = cmd_help,
     },

    {
     .name         = "reset",
     .syntax       = "",
     .summary      = "Reset the system",
     .defaults     = NULL,
     .num_defaults = 0,
     .handler      = cmd_reset,
     },

    {
     .name    = "print_ddr",
     .syntax  = "[length_blocks [start_addr]]",
     .summary = "Print words from DDR memory",
     .defaults =
            (const struct cmd_defaults[]){
                {"ddr", DEF_PRINT_LEN, 0, DEF_LINUX_ADDR},
            }, .num_defaults = 1,
     .handler      = ddr_print_cmd,
     },

    {
     .name    = "load_sd",
     .syntax  = "[length_blocks [sd_block [dest_addr]]]",
     .summary = "Load blocks from SD card into DDR memory",
     .defaults =
            (const struct cmd_defaults[]){
                {"image", DEF_LINUX_LEN, DEF_LINUX_BLK, DEF_LINUX_ADDR},
            },                                               .num_defaults = 1,
     .handler      = load_sd_cmd,
     },

    {
     .name    = "jump",
     .syntax  = "[target_addr]",
     .summary = "Jump to a DDR memory address",
     .defaults =
            (const struct cmd_defaults[]){
                {"target", 0, 0, DEF_LINUX_ADDR},
            },              .num_defaults = 1,
     .handler      = boot_jump,
     },

    {
     .name         = "diag",
     .syntax       = "",
     .summary      = "Run all diagnostic tests",
     .defaults     = NULL,
     .num_defaults = 0,
     .handler      = diag_all,
     },

    {
     .name    = "two",
     .syntax  = "",
     .summary = "Load Linux image and DTB from SD card and jump to it",
     .defaults =
            (const struct cmd_defaults[]){
                {"linux", DEF_LINUX_LEN, DEF_LINUX_BLK, DEF_LINUX_ADDR},
                {"dtb", DEF_DTB_LEN, DEF_DTB_BLK, DEF_DTB_ADDR},
            },                        .num_defaults = 2,
     .handler      = cmd_load_two,
     },
};

// character ring buffer
static volatile uint8_t rx_buf[RXBUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

// cmd buffer
static char line_buf[CMD_MAX_LEN];
static size_t line_len = 0;

// command ring buffer
static char history[HISTORY_SIZE][CMD_MAX_LEN];
static int history_head  = 0;
static int history_count = 0;
static int history_index = -1;

static void cmd_prompt(void)
{
   line_len = 0;
   memset(line_buf, 0, CMD_MAX_LEN);
   my_printf("> ");
}

void cmd_init(void)
{
   rx_head = 0;
   rx_tail = 0;
   my_printf("\r\n");
   cmd_prompt();
}

void cmd_take_char(char byte)
{
   uint8_t next = (rx_head + 1) % RXBUF_SIZE;
   if (next != rx_tail) {
      rx_buf[rx_head] = byte;
      rx_head         = next;
   }
}

static void line_erase(void)
{
   my_printf("\x1B[2K\x1B[0G");
   cmd_prompt();
}

static void line_load(const char *src)
{
   line_erase();
   strncpy(line_buf, src, CMD_MAX_LEN);
   line_buf[CMD_MAX_LEN - 1] = '\0';
   line_len                  = strlen(line_buf);
   my_printf("%s", line_buf);
}

static void history_add(const char *line)
{
   line_buf[line_len] = '\0';

   if (line[0] == '\0') {
      return;
   }

   strncpy(history[history_head], line, CMD_MAX_LEN);
   history[history_head][CMD_MAX_LEN - 1] = '\0';

   history_head = (history_head + 1) % HISTORY_SIZE;

   if (history_count < HISTORY_SIZE) {
      history_count++;
   }

   history_index = -1;
}

static void history_prev(void)
{
   if (history_count == 0) {
      return;
   }

   if (history_index < 0) {
      history_index = (history_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
   } else {
      int oldest = (history_head - history_count + HISTORY_SIZE) % HISTORY_SIZE;
      if (history_index != oldest) {
         history_index = (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
      }
   }

   line_load(history[history_index]);
}

static void history_next(void)
{
   if (history_index < 0) {
      return;
   }

   history_index = (history_index + 1) % HISTORY_SIZE;

   if (history_index == history_head) {
      history_index = -1;
      line_load("");
   } else {
      line_load(history[history_index]);
   }
}

static inline int my_isspace(int c)
{
   return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
          c == '\v';
}

static int parse_args_uint32(uint32_t *arg1, uint32_t *arg2, uint32_t *arg3)
{
   char *p   = line_buf;
   char *end = NULL;
   int count = 0;

   // skip command
   while (*p && !my_isspace((unsigned char)*p))
      p++;
   while (*p && my_isspace((unsigned char)*p))
      p++;

   if (*p) {
      *arg1 = strtoul(p, &end, 0);
      if (end == p)
         return count; // no valid number
      count++;
      p = end;

      while (*p && my_isspace((unsigned char)*p))
         p++;
      if (*p) {
         *arg2 = strtoul(p, &end, 0);
         if (end != p) {
            count++;
            p = end;

            while (*p && my_isspace((unsigned char)*p))
               p++;
            if (*p) {
               *arg3 = strtoul(p, &end, 0);
               if (end != p)
                  count++;
            }
         }
      }
   }

   return count;
}

static void execute_command(void)
{
   if (line_len == 0) {
      cmd_prompt();
      return;
   }

   line_buf[line_len] = '\0';

   // find first space to separate command from arguments
   char *space    = strchr(line_buf, ' ');
   size_t cmd_len = space ? (size_t)(space - line_buf) : line_len;

   size_t found            = 0;
   const struct cmd *match = NULL;

   for (size_t i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++) {
      if (strncmp(line_buf, cmd_list[i].name, cmd_len) == 0) {
         found++;
         match = &cmd_list[i];
      }
   }

   if (found == 0) {
      my_printf("Unknown command '%s'.\r\n", line_buf);
      cmd_help(0, 0, 0, 0);
      cmd_prompt();
      return;
   }

   if (found > 1) {
      my_printf("Ambiguous command '%.*s'.\r\n", (int)cmd_len, line_buf);
      cmd_prompt();
      return;
   }

   // parse up to 3 uint32_t args
   uint32_t arg1 = 0;
   uint32_t arg2 = 0;
   uint32_t arg3 = 0;
   int argc      = parse_args_uint32(&arg1, &arg2, &arg3);

   // call command function
   match->handler(argc, arg1, arg2, arg3);
   cmd_prompt();
}

static void cmd_tab_completion(void)
{
   size_t matches    = 0;
   const char *match = NULL;

   for (size_t i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++) {
      if (strncmp(line_buf, cmd_list[i].name, line_len) == 0) {
         matches++;
         match = cmd_list[i].name;
      }
   }

   if (matches == 1) {
      // unique match: complete the rest of the command
      size_t len = strlen(match);
      while (line_len < len) {
         char c               = match[line_len];
         line_buf[line_len++] = c;
         my_printf("%c", c);
      }
   } else if (matches > 1) {
      // multiple matches: list options
      my_printf("\r\n");
      for (size_t i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++) {
         if (strncmp(line_buf, cmd_list[i].name, line_len) == 0)
            my_printf("%s  ", cmd_list[i].name);
      }
      my_printf("\r\n> %s", line_buf); // reprint prompt + current input
   }
}

static int try_handle_escape(char byte)
{
   static int esc_state = 0;

   if (esc_state == 1) {
      if (byte == '[') {
         esc_state = 2;
         return 1;
      }
      esc_state = 0;
      return 1;
   }

   if (esc_state == 2) {
      if (byte == 'A') /* Up arrow */
         history_prev();
      else if (byte == 'B') /* Down arrow */
         history_next();
      esc_state = 0;
      return 1;
   }

   if (byte == 0x1B) { /* ESC */
      esc_state = 1;
      return 1;
   }

   return 0;
}

void cmd_poll(void)
{
   // process all pending chars from ring buffer
   while (rx_tail != rx_head) {
      char byte = rx_buf[rx_tail];
      rx_tail   = (rx_tail + 1) % RXBUF_SIZE;

      if (try_handle_escape(byte))
         continue;

      if (byte == '\r' || byte == '\n') {
         my_printf("\r\n");
         history_add(line_buf);
         execute_command();
      }

      else if ((byte == '\b' || byte == 0x7F)) {
         if (line_len > 0) {
            line_len--;
            line_buf[line_len] = '\0';
            my_printf("\b \b");
         }
      }

      else if (byte >= 0x20 && byte <= 0x7E) {
         if (line_len < CMD_MAX_LEN - 1) {
            line_buf[line_len++] = byte;
            my_printf("%c", byte);
         }
      }

      else if (byte == '\t') {
         cmd_tab_completion();
      }

      else if (byte == 0x0C) { // Ctrl-L
         my_printf("%c", byte);
         cmd_prompt();
      }

      else if (byte < 0x20 || byte > 0x7E) {
         my_printf("^%c",
                   (uint8_t)byte ^ 0x40U); // Ctrl codes displayed as ^A..^Z
      }

      else {
         // all other characters ignored
      }
   }
}

void cmd_help(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   my_printf("Available commands:\r\n\r\n");

   for (size_t i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++) {
      const struct cmd *c = &cmd_list[i];

      my_printf("  %s %s\r\n", c->name, c->syntax);
      my_printf("    %s\r\n", c->summary);

      /* Defaults, if any */
      if (c->num_defaults > 0 && c->defaults != NULL) {
         my_printf("    defaults:\r\n");
         for (size_t j = 0; j < c->num_defaults; j++) {
            const struct cmd_defaults *d = &c->defaults[j];
            my_printf("      %s:", d->label);

            /* Only print nonzero fields */
            if (d->len_blocks != 0)
               my_printf(" len_blocks=%" PRIu32, d->len_blocks);
            if (d->sd_block != 0)
               my_printf(" sd_block=%" PRIu32, d->sd_block);
            if (d->dest_addr != 0)
               my_printf(" dest_addr=0x%08" PRIX32, d->dest_addr);

            my_printf("\r\n");
         }
      }

      my_printf("\r\n");
   }
}

void cmd_reset(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)argc; (void)arg1; (void)arg2; (void)arg3;

    my_printf("System reset requested...\r\n");

    /* Ensure previous writes complete */
    __asm__ volatile ("dsb sy");  // data synchronization barrier

    /* Trigger a reset: set SYSRESETREQ via RCC MP_GRSTCSETR */
    RCC->MP_GRSTCSETR = RCC_MP_GRSTCSETR_MPSYSRST;  // reset entire system

    /* Wait indefinitely for reset to occur */
    while (1) {
        __asm__ volatile("wfe");  // wait for event (low-power idle)
    }
}

void cmd_load_two(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   sd_read(DEF_LINUX_BLK, DEF_LINUX_LEN, DEF_LINUX_ADDR);
   sd_read(DEF_DTB_BLK, DEF_DTB_LEN, DEF_DTB_ADDR);
   boot_jump(0, 0, 0, 0);
}
