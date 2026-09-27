#pragma once

#include <stdbool.h>

void power_mgmt_init(void);
void power_mgmt_note_activity(void);
void power_mgmt_enter_deep_sleep(void);
