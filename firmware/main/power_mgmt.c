#include "power_mgmt.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fingerprint.h"
#include "touch_pin_hid.h"

static const char *TAG = "power_mgmt";
static const int64_t IDLE_SLEEP_TIMEOUT_US = 15LL * 60LL * 1000000LL;
static volatile int64_t last_activity_time;

void power_mgmt_note_activity(void) {
  last_activity_time = esp_timer_get_time();
}

void power_mgmt_enter_deep_sleep(void) {
  ESP_LOGI(TAG, "Entering deep sleep, enabling ext0 wakeup on GPIO 2");
  fingerprint_sleep();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
  esp_deep_sleep_start();
}

static void power_mgmt_task(void *arg) {
  (void)arg;
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (transport_is_usb_active()) {
      continue;
    }
    if ((esp_timer_get_time() - last_activity_time) >= IDLE_SLEEP_TIMEOUT_US) {
      ESP_LOGI(TAG, "Inactivity timeout (15m) reached on battery power");
      power_mgmt_enter_deep_sleep();
    }
  }
}

void power_mgmt_init(void) {
  last_activity_time = esp_timer_get_time();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    ESP_LOGI(TAG, "Woke up from deep sleep via EXT0 (touch sensor)");
    if (!transport_is_usb_active()) {
      int64_t start_us = esp_timer_get_time();
      bool ghost = false;
      // 1.5s observation window
      for (int i = 0; i < 15; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!fingerprint_present_hint()) {
          ghost = true;
          break;
        }
      }
      if (ghost || last_activity_time <= start_us) {
        ESP_LOGI(TAG, "Anti-ghost wakeup filter triggered, returning to deep sleep");
        power_mgmt_enter_deep_sleep();
      }
    }
  }

  BaseType_t created = xTaskCreate(power_mgmt_task, "power_mgmt", 3072, NULL, 2, NULL);
  configASSERT(created == pdPASS);
}
