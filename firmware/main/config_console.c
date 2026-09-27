#include "config_console.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs_flash.h"
#include "tusb.h"

#include "device_config.h"
#include "ble_service.h"
#include "fingerprint.h"
#include "firmware_update.h"
#include "piv.h"
#include "power_mgmt.h"
#include "touch_pin_hid.h"
#include "usb_ccid.h"

#ifndef TINYTOUCH_FIRMWARE_VERSION
#define TINYTOUCH_FIRMWARE_VERSION "development"
#endif
#ifndef TINYTOUCH_BUILD_ID
#define TINYTOUCH_BUILD_ID "development"
#endif

#define AUTH_WINDOW_US (120LL * 1000000LL)
#define OTA_WINDOW_US (30LL * 1000000LL)
#define CDC_WRITE_TIMEOUT_US (2LL * 1000000LL)

static cmd_source_t current_cmd_source = CMD_SRC_NONE;
static SemaphoreHandle_t cmd_mutex;
static SemaphoreHandle_t write_lock;

static char cdc_command[5632];
static size_t cdc_command_length;
static bool cdc_command_overflow;

static SemaphoreHandle_t ble_rx_mutex;
static char ble_rx_line[5632];
static size_t ble_rx_len;
static bool ble_rx_overflow;
static char ble_pending_cmd[5632];
static volatile bool ble_cmd_pending;
static volatile bool ble_cmd_overflow_flag;

static char command[5632];
static char ota_token[33];
static int64_t authorized_until;
static int64_t ota_last_activity;
static volatile bool piv_create_active;
static volatile bool usb_reconnect_active;

static void wipe(void *data, size_t length) {
  volatile uint8_t *cursor = data;
  while (length--) *cursor++ = 0;
}

static bool cdc_write_all(const char *data, size_t length, int64_t deadline) {
  if (!tud_cdc_connected()) return false;

  size_t offset = 0;
  while (offset < length) {
    size_t remaining = length - offset;
    uint32_t request = remaining > UINT32_MAX ? UINT32_MAX : (uint32_t)remaining;
    uint32_t written = tud_cdc_write(data + offset, request);
    if (written) {
      offset += written;
      tud_cdc_write_flush();
      continue;
    }

    tud_cdc_write_flush();
    if (esp_timer_get_time() >= deadline) return false;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return true;
}

static void ble_write_line(const char *line) {
  if (!line) return;
  size_t len = strlen(line);
  if (len + 3 <= 512) {
    char buf[512];
    memcpy(buf, line, len);
    buf[len] = '\r';
    buf[len + 1] = '\n';
    buf[len + 2] = '\0';
    ble_service_send_console_line(buf);
  } else {
    ble_service_send_console_line(line);
    ble_service_send_console_line("\r\n");
  }
}

void config_console_send_line(const char *line) {
  if (!line) return;
  if (write_lock) xSemaphoreTake(write_lock, portMAX_DELAY);

  if (current_cmd_source == CMD_SRC_BLE_NUS) {
    ble_write_line(line);
  } else if (current_cmd_source == CMD_SRC_USB_CDC) {
    int64_t deadline = esp_timer_get_time() + CDC_WRITE_TIMEOUT_US;
    bool sent = cdc_write_all(line, strlen(line), deadline);
    if (sent) cdc_write_all("\r\n", 2, deadline);
  } else {
    // Asynchronous broadcast: send to active transport (USB CDC if ready, else BLE if connected)
    if (tud_cdc_connected()) {
      int64_t deadline = esp_timer_get_time() + CDC_WRITE_TIMEOUT_US;
      bool sent = cdc_write_all(line, strlen(line), deadline);
      if (sent) cdc_write_all("\r\n", 2, deadline);
    } else if (ble_service_is_connected()) {
      ble_write_line(line);
    }
  }

  if (write_lock) xSemaphoreGive(write_lock);
}

static void reply(const char *text) { config_console_send_line(text); }

static bool hex_value(char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

static bool decode_hex(const char *text, uint8_t *out, size_t length) {
  if (strlen(text) != length * 2) return false;
  for (size_t i = 0; i < length; i++) {
    if (!hex_value(text[i * 2]) || !hex_value(text[i * 2 + 1])) return false;
    unsigned value = 0;
    if (sscanf(text + i * 2, "%2x", &value) != 1) return false;
    out[i] = (uint8_t)value;
  }
  return true;
}

static bool parse_u32(const char *text, uint32_t maximum, uint32_t *value) {
  char *end = NULL;
  unsigned long parsed = strtoul(text, &end, 10);
  if (!text[0] || !end || *end || parsed > maximum) return false;
  *value = (uint32_t)parsed;
  return true;
}

static bool authorized(void) { return esp_timer_get_time() < authorized_until; }

static bool require_authorized(void) {
  if (authorized()) return true;
  reply("ERR LOCKED run=AUTH");
  return false;
}

static void touch_prompt(void) { reply("EVENT TOUCH"); }

static void enroll_prompt(const char *state) {
  char line[48];
  snprintf(line, sizeof(line), "EVENT %s", state);
  reply(line);
}

static void authorize(void) {
#ifdef TINYTOUCH_DEVELOPMENT_SKIP_FINGERPRINT_AUTH
  // This symbol exists only in an explicitly opted-in local CMake build.
  authorized_until = esp_timer_get_time() + AUTH_WINDOW_US;
  reply("OK AUTH development=unlocked");
  return;
#endif
  int count = fingerprint_count();
  if (count < 0) { reply("ERR AUTH sensor=offline"); return; }
  bool ok = count == 0 || (count > 0 && fingerprint_authorize_prompted(touch_prompt));
  if (!ok) { reply("ERR AUTH no_match"); return; }
  authorized_until = esp_timer_get_time() + AUTH_WINDOW_US;
  piv_note_configuration_presence();
  reply(count == 0 ? "OK AUTH first_setup=1" : "OK AUTH");
}

static bool token_matches(const char *token) {
  return ota_token[0] && strlen(token) == 32 && strcmp(token, ota_token) == 0;
}

static void clear_ota(void) {
  wipe(ota_token, sizeof(ota_token));
  ota_last_activity = 0;
}

static void status(void) {
  char line[320];
  int count = fingerprint_count();
  // fingerprint_count probes the UART and can update the live health state.
  // Read health after that probe so one STATUS line cannot say ready with an
  // unavailable fingerprint count.
  bool sensor_is_ready = fingerprint_is_ready();
  uint8_t hid_start = 0, hid_end = 0, piv_start = 0, piv_end = 0;
  device_config_hid_led(&hid_start, &hid_end);
  device_config_piv_led(&piv_start, &piv_end);
  snprintf(line, sizeof(line),
           "OK STATUS protocol=7 firmware=%s build=%s mode=%s piv=%s sensor=%s fingerprints=%d "
           "hosts=%u enter=%u delay=%u led_hid=%u,%u led_piv=%u,%u ota=%s",
           TINYTOUCH_FIRMWARE_VERSION, TINYTOUCH_BUILD_ID, device_config_mode_name(),
           piv_uses_provisioned_keys() ? "ready" : "unconfigured",
           sensor_is_ready ? "ready" : "offline", count,
           (unsigned)device_config_hid_host_count(),
           device_config_submit_enter() ? 1 : 0,
           (unsigned)device_config_typing_delay_ms(),
           (unsigned)hid_start, (unsigned)hid_end,
           (unsigned)piv_start, (unsigned)piv_end,
           firmware_update_staged() ? "staged" :
           (firmware_update_active() ? "writing" : "idle"));
  reply(line);
}

static void set_mode(const char *mode) {
  if (!require_authorized()) return;
  bool ok = strcmp(mode, "PIV") == 0 ? device_config_set_mode(DEVICE_MODE_PIV) :
            strcmp(mode, "HID") == 0 ? device_config_set_mode(DEVICE_MODE_HID) : false;
  if (ok) fingerprint_led_idle();
  reply(ok ? "OK SET MODE" : "ERR SET MODE");
}

static void set_value(char *arguments) {
  if (!require_authorized()) return;
  char *value = strchr(arguments, ' ');
  if (!value) { reply("ERR SET"); return; }
  *value++ = '\0';

  bool ok = false;
  uint32_t number = 0;
  if (strcmp(arguments, "LED_HID") == 0 || strcmp(arguments, "LED_PIV") == 0) {
    bool is_piv = strcmp(arguments, "LED_PIV") == 0;
    char *end_str = strchr(value, ' ');
    uint32_t c_start = 0, c_end = 0;
    if (end_str) {
      *end_str++ = '\0';
      if (parse_u32(value, 7, &c_start) && parse_u32(end_str, 7, &c_end) &&
          c_start >= 1 && c_start <= 7 && c_end >= 1 && c_end <= 7) {
        ok = is_piv ? device_config_set_piv_led((uint8_t)c_start, (uint8_t)c_end) :
                      device_config_set_hid_led((uint8_t)c_start, (uint8_t)c_end);
        if (ok) fingerprint_led_idle();
      }
    }
  } else if (parse_u32(value, UINT16_MAX, &number)) {
    if (strcmp(arguments, "TYPE_DELAY") == 0) ok = device_config_set_typing_delay_ms(number);
    else if (strcmp(arguments, "SUBMIT_ENTER") == 0 && number <= 1) ok = device_config_set_submit_enter(number);
    else if (strcmp(arguments, "COOLDOWN") == 0) ok = device_config_set_touch_cooldown_ms(number);
    else if (strcmp(arguments, "WAKEUP_DELAY") == 0 && number <= 3000) {
      // TODO: Persist wakeup_delay when CONFIG_VERSION bumped to 7
      ok = true;
    }
  }
  reply(ok ? "OK SET" : "ERR SET");
}

static void host_add(char *arguments) {
  if (!require_authorized()) return;
  char *key = strchr(arguments, ' ');
  uint8_t id[DEVICE_CONFIG_HID_KEY_ID_SIZE] = {0};
  uint8_t secret[32] = {0};
  bool ok = key != NULL;
  if (ok) { *key++ = '\0'; ok = decode_hex(arguments, id, sizeof(id)) &&
                                      decode_hex(key, secret, sizeof(secret)) &&
                                      device_config_add_hid_host(id, secret); }
  wipe(secret, sizeof(secret));
  reply(ok ? "OK HOST ADD" : "ERR HOST ADD");
}

static void host_remove(const char *arguments) {
  if (!require_authorized()) return;
  uint8_t id[DEVICE_CONFIG_HID_KEY_ID_SIZE] = {0};
  bool ok = decode_hex(arguments, id, sizeof(id)) && device_config_remove_hid_host(id);
  reply(ok ? "OK HOST REMOVE" : "ERR HOST REMOVE");
}

static void host_list(void) {
  static const char hex[] = "0123456789abcdef";
  device_hid_host_t hosts[DEVICE_CONFIG_MAX_HID_HOSTS];
  size_t count = device_config_copy_hid_hosts(hosts);
  char ids[DEVICE_CONFIG_MAX_HID_HOSTS * (DEVICE_CONFIG_HID_KEY_ID_SIZE * 2 + 1)] = {0};
  size_t offset = 0;
  for (size_t host = 0; host < count; host++) {
    if (offset) ids[offset++] = ',';
    for (size_t byte = 0; byte < DEVICE_CONFIG_HID_KEY_ID_SIZE; byte++) {
      ids[offset++] = hex[hosts[host].id[byte] >> 4];
      ids[offset++] = hex[hosts[host].id[byte] & 0x0f];
    }
  }
  wipe(hosts, sizeof(hosts));
  char line[192];
  snprintf(line, sizeof(line), "OK HOST LIST ids=%s capacity=%u", ids,
           (unsigned)DEVICE_CONFIG_MAX_HID_HOSTS);
  reply(line);
}

static void fingerprint_command(char *arguments) {
  if (!require_authorized()) return;
  uint32_t slot = 0;
  bool ok = false;
  if (strncmp(arguments, "ENROLL ", 7) == 0 && parse_u32(arguments + 7, UINT16_MAX, &slot)) {
    ok = fingerprint_enroll((uint16_t)slot, enroll_prompt);
  } else if (strncmp(arguments, "DELETE ", 7) == 0 && parse_u32(arguments + 7, UINT16_MAX, &slot)) {
    ok = fingerprint_delete((uint16_t)slot) && device_config_set_fingerprint_profile_views(0);
  } else if (strcmp(arguments, "CLEAR") == 0) {
    ok = fingerprint_delete_all() && device_config_set_fingerprint_profile_views(0);
  }
  reply(ok ? "OK FINGER" : "ERR FINGER");
}

static void factory_reset(void) {
  if (!require_authorized()) return;
  bool ok = fingerprint_delete_all() && nvs_flash_erase() == ESP_OK &&
            nvs_flash_init() == ESP_OK && device_config_factory_reset();
  if (ok) {
    piv_reload_keys();
    authorized_until = 0;
  }
  reply(ok ? "OK RESET FACTORY" : "ERR RESET FACTORY");
}

static void piv_create_task(void *argument) {
  (void)argument;
  bool ok = piv_create_identity();
  piv_create_active = false;
  reply(ok ? "OK PIV CREATE" : "ERR PIV CREATE");
  if (ok) {
    // Give CDC enough time to deliver the successful response before asking
    // macOS to rescan the PIV token.
    vTaskDelay(pdMS_TO_TICKS(300));
    usb_ccid_rescan();
  }
  vTaskDelete(NULL);
}

static void piv_create(void) {
  if (!require_authorized()) return;
  if (piv_create_active) { reply("ERR PIV BUSY"); return; }
  piv_create_active = true;
  BaseType_t created = xTaskCreate(piv_create_task, "piv_create", 10240, NULL, 1, NULL);
  if (created != pdPASS) {
    piv_create_active = false;
    reply("ERR PIV CREATE");
    return;
  }
  reply("EVENT PIV_CREATE");
}

static void usb_reconnect_task(void *argument) {
  (void)argument;
  vTaskDelay(pdMS_TO_TICKS(100));
  usb_ccid_rescan();
  usb_reconnect_active = false;
  vTaskDelete(NULL);
}

static void usb_reconnect(void) {
  if (usb_reconnect_active) { reply("ERR USB BUSY"); return; }
  usb_reconnect_active = true;
  if (xTaskCreate(usb_reconnect_task, "usb_reconnect", 2048, NULL, 2, NULL) != pdPASS) {
    usb_reconnect_active = false;
    reply("ERR USB RECONNECT");
    return;
  }
  reply("OK USB RECONNECT");
}

static void ota_begin(char *arguments) {
  if (!require_authorized() || ota_token[0]) { if (ota_token[0]) reply("ERR OTA BUSY"); return; }
  char *size = strchr(arguments, ' ');
  if (!size || size - arguments != 32) { reply("ERR OTA BEGIN"); return; }
  *size++ = '\0'; char *digest = strchr(size, ' ');
  uint8_t hash[32] = {0}; uint32_t image_size = 0;
  bool ok = digest != NULL;
  if (ok) { *digest++ = '\0'; ok = decode_hex(arguments, hash, 16) &&
                                     parse_u32(size, UINT32_MAX, &image_size) &&
                                     decode_hex(digest, hash, sizeof(hash)) &&
                                     firmware_update_begin(image_size, hash); }
  if (ok) { memcpy(ota_token, arguments, sizeof(ota_token) - 1); ota_last_activity = esp_timer_get_time(); authorized_until = 0; }
  wipe(hash, sizeof(hash)); reply(ok ? "OK OTA BEGIN next=0" : "ERR OTA BEGIN");
}

static void ota_write(char *arguments) {
  char *offset = strchr(arguments, ' ');
  if (!offset) { reply("ERR OTA WRITE"); return; }
  *offset++ = '\0'; char *encoded = strchr(offset, ' ');
  if (!encoded || !token_matches(arguments)) { reply("ERR OTA WRITE"); return; }
  *encoded++ = '\0'; uint32_t at = 0; static uint8_t bytes[FIRMWARE_UPDATE_CHUNK_MAX]; size_t length = 0;
  bool ok = parse_u32(offset, UINT32_MAX, &at) &&
            mbedtls_base64_decode(bytes, sizeof(bytes), &length, (const unsigned char *)encoded,
                                  strlen(encoded)) == 0 &&
            firmware_update_write(at, bytes, length);
  if (ok) ota_last_activity = esp_timer_get_time();
  char line[64];
  if (ok) snprintf(line, sizeof(line), "OK OTA WRITE next=%u", (unsigned)firmware_update_written());
  else snprintf(line, sizeof(line), "ERR OTA WRITE");
  reply(line);
}

static void ota_commit(const char *token) {
  if (!token_matches(token)) { reply("ERR OTA COMMIT"); return; }
  bool ok = firmware_update_commit();
  clear_ota();
  reply(ok ? "OK OTA STAGED power_cycle=required" : "ERR OTA COMMIT");
}

static void request_touch(void) {
  // TODO: Trigger biometric approval prompt on device
  reply("OK REQUEST_TOUCH");
}

static void set_key_seq(char *arguments) {
  if (!require_authorized()) return;
  // TODO: Parse and persist key sequence (Enter, Tab, Space, etc.)
  reply("OK SET_KEY_SEQ");
}

static void write_key(char *arguments) {
  if (!require_authorized()) return;
  // Format: "slot:hash" - TODO: persist to slot
  reply("OK WRITE_KEY");
}

static bool is_disallowed_on_ble(const char *cmd) {
  if (!cmd) return false;
  if (strncmp(cmd, "OTA", 3) == 0 && (cmd[3] == '\0' || cmd[3] == ' ')) return true;
  if (strncmp(cmd, "RESET FACTORY", 13) == 0 && (cmd[13] == '\0' || cmd[13] == ' ')) return true;
  if (strncmp(cmd, "PIV CREATE", 10) == 0 && (cmd[10] == '\0' || cmd[10] == ' ')) return true;
  return false;
}

static void handle_command(void) {
  if (current_cmd_source == CMD_SRC_BLE_NUS && is_disallowed_on_ble(command)) {
    reply("ERR DISALLOWED_ON_BLE");
    return;
  }

  if (strcmp(command, "PING") == 0) reply("PONG 6");
  else if (strcmp(command, "STATUS") == 0) status();
  else if (strcmp(command, "LOGS") == 0) touch_pin_hid_send_logs();
  else if (strcmp(command, "USB RECONNECT") == 0) usb_reconnect();
  else if (strcmp(command, "AUTH") == 0) authorize();
  else if (strcmp(command, "REQUEST_TOUCH") == 0) request_touch();
  else if (strncmp(command, "SET MODE ", 9) == 0) set_mode(command + 9);
  else if (strncmp(command, "SET ", 4) == 0) set_value(command + 4);
  else if (strncmp(command, "SET_KEY_SEQ ", 12) == 0) set_key_seq(command + 12);
  else if (strncmp(command, "WRITE_KEY ", 10) == 0) write_key(command + 10);
  else if (strncmp(command, "HOST ADD ", 9) == 0) host_add(command + 9);
  else if (strncmp(command, "HOST REMOVE ", 12) == 0) host_remove(command + 12);
  else if (strcmp(command, "HOST LIST") == 0) host_list();
  else if (strncmp(command, "FINGER ", 7) == 0) fingerprint_command(command + 7);
  else if (strcmp(command, "PIV CREATE") == 0) piv_create();
  else if (strcmp(command, "RESET FACTORY") == 0) factory_reset();
  else if (strncmp(command, "OTA BEGIN ", 10) == 0) ota_begin(command + 10);
  else if (strncmp(command, "OTA WRITE ", 10) == 0) ota_write(command + 10);
  else if (strcmp(command, "OTA ABORT") == 0) {
    firmware_update_abort(); clear_ota(); reply("OK OTA ABORT");
  } else if (strncmp(command, "OTA ABORT ", 10) == 0 && token_matches(command + 10)) {
    firmware_update_abort(); clear_ota(); reply("OK OTA ABORT");
  } else if (strncmp(command, "OTA COMMIT ", 11) == 0) ota_commit(command + 11);
  else reply("ERR COMMAND");
}

void config_console_feed_ble_input(const uint8_t *data, size_t len) {
  if (!data || len == 0 || !ble_rx_mutex) return;
  power_mgmt_note_activity();

  if (xSemaphoreTake(ble_rx_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == '\r') continue;
    if (c == '\n') {
      if (ble_rx_overflow) {
        ble_cmd_overflow_flag = true;
      } else if (ble_rx_len > 0) {
        ble_rx_line[ble_rx_len] = '\0';
        if (!ble_cmd_pending) {
          memcpy(ble_pending_cmd, ble_rx_line, ble_rx_len + 1);
          ble_cmd_pending = true;
        } else {
          ble_cmd_overflow_flag = true;
        }
      }
      ble_rx_len = 0;
      ble_rx_overflow = false;
    } else if (ble_rx_len + 1 < sizeof(ble_rx_line)) {
      ble_rx_line[ble_rx_len++] = c;
    } else {
      ble_rx_overflow = true;
    }
  }

  xSemaphoreGive(ble_rx_mutex);
}

static void console_task(void *arg) {
  (void)arg;
  char buffer[1024];
  while (true) {
    if (ota_token[0] && esp_timer_get_time() - ota_last_activity > OTA_WINDOW_US) {
      firmware_update_abort(); clear_ota();
    }
    bool activity = false;
    while (tud_cdc_available()) {
      uint32_t count = tud_cdc_read(buffer, sizeof(buffer)); activity = count != 0;
      if (activity) {
        power_mgmt_note_activity();
      }
      for (uint32_t i = 0; i < count; i++) {
        if (buffer[i] == '\r') continue;
        if (buffer[i] == '\n') {
          cdc_command[cdc_command_length] = '\0';
          if (!cdc_command_overflow && cdc_command_length) {
            if (cmd_mutex) xSemaphoreTake(cmd_mutex, portMAX_DELAY);
            current_cmd_source = CMD_SRC_USB_CDC;
            memcpy(command, cdc_command, cdc_command_length + 1);
            if (strncmp(command, "PW ", 3) == 0 || strncmp(command, "PW2 ", 4) == 0) {
              touch_pin_hid_submit_response(command);
            } else {
              handle_command();
            }
            current_cmd_source = CMD_SRC_NONE;
            if (cmd_mutex) xSemaphoreGive(cmd_mutex);
          } else if (cdc_command_overflow) {
            if (cmd_mutex) xSemaphoreTake(cmd_mutex, portMAX_DELAY);
            current_cmd_source = CMD_SRC_USB_CDC;
            reply("ERR LINE");
            current_cmd_source = CMD_SRC_NONE;
            if (cmd_mutex) xSemaphoreGive(cmd_mutex);
          }
          cdc_command_length = 0; cdc_command_overflow = false;
        } else if (cdc_command_length + 1 < sizeof(cdc_command)) {
          cdc_command[cdc_command_length++] = buffer[i];
        } else {
          cdc_command_overflow = true;
        }
      }
    }

    bool has_ble_cmd = false;
    bool has_ble_overflow = false;
    if (ble_rx_mutex && xSemaphoreTake(ble_rx_mutex, 0) == pdTRUE) {
      if (ble_cmd_overflow_flag) {
        has_ble_overflow = true;
        ble_cmd_overflow_flag = false;
      } else if (ble_cmd_pending) {
        has_ble_cmd = true;
        memcpy(command, ble_pending_cmd, sizeof(command));
        command[sizeof(command) - 1] = '\0';
        ble_cmd_pending = false;
      }
      xSemaphoreGive(ble_rx_mutex);
    }

    if (has_ble_overflow) {
      activity = true;
      if (cmd_mutex) xSemaphoreTake(cmd_mutex, portMAX_DELAY);
      current_cmd_source = CMD_SRC_BLE_NUS;
      reply("ERR LINE");
      current_cmd_source = CMD_SRC_NONE;
      if (cmd_mutex) xSemaphoreGive(cmd_mutex);
    } else if (has_ble_cmd) {
      activity = true;
      if (cmd_mutex) xSemaphoreTake(cmd_mutex, portMAX_DELAY);
      current_cmd_source = CMD_SRC_BLE_NUS;
      if (strncmp(command, "PW ", 3) == 0 || strncmp(command, "PW2 ", 4) == 0) {
        touch_pin_hid_submit_response(command);
      } else {
        handle_command();
      }
      current_cmd_source = CMD_SRC_NONE;
      if (cmd_mutex) xSemaphoreGive(cmd_mutex);
    }

    // The scheduler tick is 10 ms. A 2 ms conversion becomes zero and leaves
    // this higher-priority loop ready forever, starving app_main before it can
    // create the background fingerprint task.
    if (!activity) vTaskDelay(1);
  }
}

void config_console_start(void) {
  write_lock = xSemaphoreCreateMutex();
  configASSERT(write_lock);
  cmd_mutex = xSemaphoreCreateMutex();
  configASSERT(cmd_mutex);
  ble_rx_mutex = xSemaphoreCreateMutex();
  configASSERT(ble_rx_mutex);
  BaseType_t created = xTaskCreate(console_task, "console", CONFIG_CONSOLE_STACK_SIZE, NULL, 3, NULL);
  configASSERT(created == pdPASS);
}
