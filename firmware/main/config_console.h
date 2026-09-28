#pragma once

#include <stddef.h>
#include <stdint.h>

#define CONFIG_CONSOLE_STACK_SIZE 8192

typedef enum {
  CMD_SRC_USB_CDC = 0,
  CMD_SRC_BLE_NUS = 1,
  CMD_SRC_NONE = 2,
  CMD_SRC_BROADCAST = 2
} cmd_source_t;

void config_console_start(void);
void config_console_send_line(const char *line);
void config_console_feed_ble_input(const uint8_t *data, size_t len);
