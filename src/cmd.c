// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file cmd.c
 * @brief Command line interface
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "cmd.h"
#include "boot.h"
#include "ddr.h"
#include "defaults.h"
#include "diag.h"
#include "printf.h"
#include "sd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RXBUF_SIZE   64
#define CMD_MAX_LEN  32
#define HISTORY_SIZE 8

struct cmd {
   const char *name;
   const char *usage;
   const char *desc;
   void (*fn)(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
};

static const struct cmd cmd_list[] = {
    {"help     ", "           ", "Display this help message",   cmd_help     },
    {"print_ddr", "[N [L]]    ", "Print N words from DDR @ L",  ddr_print_cmd},
    {"load_sd  ", "[N [L [M]]]", "Load N blks into DDR addr M", load_sd_cmd  },
    {"jump     ", "[L]        ", "Jump to DDR address L",       boot_jump    },
    {"diag     ", "           ", "Run all diagnostic tests",    diag_all     },
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

void cmd_help(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   my_printf("Available commands:\r\n");
   for (size_t i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]); i++) {
      if (cmd_list[i].usage[0] != '\0')
         my_printf("  %s %s - %s\r\n", cmd_list[i].name, cmd_list[i].usage,
                   cmd_list[i].desc);
      else
         my_printf("  %s - %s\r\n", cmd_list[i].name, cmd_list[i].desc);
   }

   my_printf("\r\nDefaults:\r\n");
   my_printf("  print_ddr: N=%u, L=0x%08X\r\n", DEF_PRINT_LEN, DEF_ENTRY_ADDR);
   my_printf("  load_sd  : N=%u, L=%u, M=0x%X\r\n", DEF_LOAD_LEN, DEF_LOAD_BLK,
             DEF_LOAD_ADDR);
   my_printf("  jump     : L=0x%08X\r\n", DEF_ENTRY_ADDR);
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
   match->fn(argc, arg1, arg2, arg3);
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
