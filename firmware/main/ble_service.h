#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Service UUIDs (16-bit)
#define BLE_UUID_HOGP_SERVICE        0x1812
#define BLE_UUID_BATTERY_SERVICE     0x180F
#define BLE_UUID_DEVICE_INFO_SERVICE 0x180A

// Nordic UART Service (NUS) UUIDs (128-bit)
#define BLE_UUID_NUS_SERVICE  "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_UUID_NUS_CHAR_RX  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_UUID_NUS_CHAR_TX  "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// HOGP Characteristic UUIDs (16-bit)
#define BLE_UUID_HOGP_REPORT_MAP     0x2A4B
#define BLE_UUID_HOGP_REPORT         0x2A4D
#define BLE_UUID_HOGP_HID_INFO       0x2A4A
#define BLE_UUID_HOGP_HID_CTRL_PT    0x2A4C
#define BLE_UUID_HOGP_PROTO_MODE     0x2A4E
#define BLE_UUID_REPORT_REF_DESC     0x2908

// Battery Characteristic UUID (16-bit)
#define BLE_UUID_BATTERY_LEVEL       0x2A19

// Device Information Characteristic UUIDs (16-bit)
#define BLE_UUID_MANUFACTURER_NAME   0x2A29
#define BLE_UUID_MODEL_NUMBER        0x2A24
#define BLE_UUID_PNP_ID              0x2A50

void ble_service_init(void);
bool ble_service_is_connected(void);
bool ble_service_send_keyboard_report(uint8_t modifier, const uint8_t keycodes[6]);
void ble_service_send_console_line(const char *line);
void ble_service_set_battery_level(uint8_t level);
void ble_service_start_advertising(bool allow_new_pairing);

// Console input handler from BLE NUS RX (weak default provided in ble_service.c)
void config_console_feed_ble_input(const uint8_t *data, size_t len);
