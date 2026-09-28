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
static const int64_t ANTI_GHOST_TIMEOUT_US = 1500LL * 1000LL;

static portMUX_TYPE activity_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t last_activity_time;
static volatile bool is_ghost_check_pending;
static int64_t wake_time;

void power_mgmt_note_activity(void) {
  portENTER_CRITICAL(&activity_lock);
  last_activity_time = esp_timer_get_time();
  portEXIT_CRITICAL(&activity_lock);
}

static int64_t get_last_activity_time(void) {
  portENTER_CRITICAL(&activity_lock);
  int64_t t = last_activity_time;
  portEXIT_CRITICAL(&activity_lock);
  return t;
}

void power_mgmt_enter_deep_sleep(void) {
  ESP_LOGI(TAG, "Entering deep sleep, enabling ext0 wakeup on GPIO 2");
  fingerprint_sleep();

  // If GPIO 2 is still held HIGH (continuous pressure in bag), wait for release
  // with a bounded timeout (500ms) to avoid rapid microsecond sleep/wake thrashing loops.
  int64_t wait_start = esp_timer_get_time();
  while (fingerprint_present_hint() && (esp_timer_get_time() - wait_start) < 500000LL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
  esp_deep_sleep_start();
}

static void power_mgmt_task(void *arg) {
  (void)arg;

  // Anti-ghost observation window (non-blocking for app_main)
  if (is_ghost_check_pending) {
    int64_t deadline = wake_time + ANTI_GHOST_TIMEOUT_US;
    while (esp_timer_get_time() < deadline) {
      if (transport_is_usb_active()) {
        is_ghost_check_pending = false;
        break;
      }
      if (get_last_activity_time() > wake_time) {
        ESP_LOGI(TAG, "Authenticated activity confirmed during anti-ghost window");
        is_ghost_check_pending = false;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (is_ghost_check_pending) {
      if (get_last_activity_time() <= wake_time && !transport_is_usb_active()) {
        ESP_LOGI(TAG, "No authenticated activity within 1.5s, ghost wakeup entering deep sleep");
        power_mgmt_enter_deep_sleep();
      }
      is_ghost_check_pending = false;
    }
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (transport_is_usb_active()) {
      // Refresh activity timer while on USB power so unplugging starts a fresh 15m idle window
      power_mgmt_note_activity();
      continue;
    }
    if ((esp_timer_get_time() - get_last_activity_time()) >= IDLE_SLEEP_TIMEOUT_US) {
      ESP_LOGI(TAG, "Inactivity timeout (15m) reached on battery power");
      power_mgmt_enter_deep_sleep();
    }
  }
}

void power_mgmt_init(void) {
  power_mgmt_note_activity();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    ESP_LOGI(TAG, "Woke up from deep sleep via EXT0 (touch sensor)");
    if (!transport_is_usb_active()) {
      is_ghost_check_pending = true;
      wake_time = esp_timer_get_time();
    }
  }

  BaseType_t created = xTaskCreate(power_mgmt_task, "power_mgmt", 3072, NULL, 2, NULL);
  configASSERT(created == pdPASS);
}
