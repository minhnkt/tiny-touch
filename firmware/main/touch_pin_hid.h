#pragma once

#include <stdbool.h>
#include <stdint.h>

void touch_pin_hid_start(void);
bool touch_pin_hid_submit_response(const char *response);
void touch_pin_hid_usb_attached(void);
void touch_pin_hid_send_logs(void);
void touch_pin_hid_log_event(const char *event, int value);

bool transport_is_usb_active(void);
bool transport_send_key(uint8_t modifier, uint8_t key);
