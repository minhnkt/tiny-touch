#include "ble_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_service";

// Forward declaration of NVS store init provided by NimBLE
void ble_store_config_init(void);

static int ble_service_gap_event(struct ble_gap_event *event, void *arg);

// Weak fallback for config console BLE input (implemented in config_console.c in Task 4)
__attribute__((weak)) void config_console_feed_ble_input(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
}

// -----------------------------------------------------------------------------
// State Variables
// -----------------------------------------------------------------------------
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_is_connected = false;
static uint8_t s_own_addr_type = 0;
static bool s_sync_done = false;
static bool s_adv_enabled = true;
static bool s_allow_new_pairing = false;

// Characteristic Value Handles
static uint16_t s_hid_report_val_handle = 0;
static uint16_t s_nus_rx_val_handle = 0;
static uint16_t s_nus_tx_val_handle = 0;
static uint16_t s_battery_val_handle = 0;

// Cached Characteristic Values
static uint8_t s_protocol_mode = 0x01; // Report Mode
static uint8_t s_battery_level = 100;  // 100%

// -----------------------------------------------------------------------------
// HID Report Map & Static Descriptors
// -----------------------------------------------------------------------------
// Standard 8-byte Keyboard Report Map: 1 modifier, 1 reserved, 6 keycodes
static const uint8_t s_hid_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0 - LeftControl)
    0x29, 0xE7,        //   Usage Maximum (0xE7 - Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs) - Modifier byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Const,Array,Abs) - Reserved byte
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array,Abs) - 6 Keycodes
    0xC0               // End Collection
};

// Report Reference: Report ID = 0x00, Report Type = 0x01 (Input Report)
static const uint8_t s_hid_report_ref[] = {0x00, 0x01};

// HID Information: bcdHID (0x0111), bCountryCode (0x00), Flags (0x02 - Normally Connectable)
static const uint8_t s_hid_info[] = {0x11, 0x01, 0x00, 0x02};

// Device Information
static const char s_manufacturer_name[] = "tinyTouch";
static const char s_model_number[] = "tinyTouch Key";
// PnP ID: Vendor ID Source (0x02 - USB IF), Vendor ID (0x303A), Product ID (0x4001), Product Version (0x0100)
static const uint8_t s_pnp_id[] = {0x02, 0x3A, 0x30, 0x01, 0x40, 0x00, 0x01};

// -----------------------------------------------------------------------------
// NUS 128-bit UUIDs (Little-Endian)
// Base: 6E40xxxx-B5A3-F393-E0A9-E50E24DCCA9E
// -----------------------------------------------------------------------------
static const ble_uuid128_t s_uuid_nus_svc =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);

static const ble_uuid128_t s_uuid_nus_rx =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);

static const ble_uuid128_t s_uuid_nus_tx =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                     0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);

// -----------------------------------------------------------------------------
// GATT Access Callbacks
// -----------------------------------------------------------------------------
static int ble_service_chr_access_hogp(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
  uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    switch (uuid16) {
      case BLE_UUID_HOGP_REPORT_MAP:
        return os_mbuf_append(ctxt->om, s_hid_report_map, sizeof(s_hid_report_map)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      case BLE_UUID_HOGP_REPORT: {
        uint8_t zero_report[8] = {0};
        return os_mbuf_append(ctxt->om, zero_report, sizeof(zero_report)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      case BLE_UUID_HOGP_HID_INFO:
        return os_mbuf_append(ctxt->om, s_hid_info, sizeof(s_hid_info)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      case BLE_UUID_HOGP_PROTO_MODE:
        return os_mbuf_append(ctxt->om, &s_protocol_mode, sizeof(s_protocol_mode)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      default:
        return BLE_ATT_ERR_UNLIKELY;
    }
  } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    switch (uuid16) {
      case BLE_UUID_HOGP_PROTO_MODE: {
        uint8_t mode = 0;
        if (OS_MBUF_PKTLEN(ctxt->om) == 1) {
          os_mbuf_copydata(ctxt->om, 0, 1, &mode);
          s_protocol_mode = mode;
          return 0;
        }
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      case BLE_UUID_HOGP_HID_CTRL_PT:
        // Accept control point suspend / resume commands
        return 0;
      default:
        return BLE_ATT_ERR_UNLIKELY;
    }
  }

  return BLE_ATT_ERR_UNLIKELY;
}

static int ble_service_dsc_access_report_ref(uint16_t conn_handle, uint16_t attr_handle,
                                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    return os_mbuf_append(ctxt->om, s_hid_report_ref, sizeof(s_hid_report_ref)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static int ble_service_chr_access_nus(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > 0) {
      uint8_t buf[256];
      uint16_t copy_len = (len < sizeof(buf)) ? len : (sizeof(buf) - 1);
      os_mbuf_copydata(ctxt->om, 0, copy_len, buf);
      config_console_feed_ble_input(buf, copy_len);
    }
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static int ble_service_chr_access_battery(uint16_t conn_handle, uint16_t attr_handle,
                                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    return os_mbuf_append(ctxt->om, &s_battery_level, 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static int ble_service_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
  uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    switch (uuid16) {
      case BLE_UUID_MANUFACTURER_NAME:
        return os_mbuf_append(ctxt->om, s_manufacturer_name, strlen(s_manufacturer_name)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      case BLE_UUID_MODEL_NUMBER:
        return os_mbuf_append(ctxt->om, s_model_number, strlen(s_model_number)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      case BLE_UUID_PNP_ID:
        return os_mbuf_append(ctxt->om, s_pnp_id, sizeof(s_pnp_id)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      default:
        return BLE_ATT_ERR_UNLIKELY;
    }
  }
  return BLE_ATT_ERR_UNLIKELY;
}

// -----------------------------------------------------------------------------
// GATT Service Definitions
// -----------------------------------------------------------------------------
static const struct ble_gatt_svc_def s_gatt_services[] = {
    // 1. Human Interface Device (HOGP) Service: 0x1812
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Protocol Mode: 0x2A4E (Read / Write Without Response)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_PROTO_MODE),
                .access_cb = ble_service_chr_access_hogp,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // HID Information: 0x2A4A (Read)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_HID_INFO),
                .access_cb = ble_service_chr_access_hogp,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Report Map: 0x2A4B (Read, Encrypted)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_REPORT_MAP),
                .access_cb = ble_service_chr_access_hogp,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                // HID Control Point: 0x2A4C (Write Without Response)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_HID_CTRL_PT),
                .access_cb = ble_service_chr_access_hogp,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // Input Report: 0x2A4D (Read / Notify, Encrypted) with Report Reference Descriptor
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_HOGP_REPORT),
                .access_cb = ble_service_chr_access_hogp,
                .val_handle = &s_hid_report_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_UUID_REPORT_REF_DESC),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                        .access_cb = ble_service_dsc_access_report_ref,
                    },
                    {0} // Terminator
                },
            },
            {0} // Terminator
        },
    },

    // 2. Nordic UART Service (NUS Compatible) Custom Serial
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_uuid_nus_svc.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX: 6E400002-... (Write / Write Without Response)
                .uuid = &s_uuid_nus_rx.u,
                .access_cb = ble_service_chr_access_nus,
                .val_handle = &s_nus_rx_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // TX: 6E400003-... (Notify)
                .uuid = &s_uuid_nus_tx.u,
                .access_cb = ble_service_chr_access_nus,
                .val_handle = &s_nus_tx_val_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            {0} // Terminator
        },
    },

    // 3. Battery Service: 0x180F
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_UUID_BATTERY_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Battery Level: 0x2A19 (Read / Notify)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_BATTERY_LEVEL),
                .access_cb = ble_service_chr_access_battery,
                .val_handle = &s_battery_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0} // Terminator
        },
    },

    // 4. Device Information Service: 0x180A
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_UUID_DEVICE_INFO_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Manufacturer Name String: 0x2A29 (Read)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_MANUFACTURER_NAME),
                .access_cb = ble_service_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Model Number String: 0x2A24 (Read)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_MODEL_NUMBER),
                .access_cb = ble_service_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // PnP ID: 0x2A50 (Read)
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_PNP_ID),
                .access_cb = ble_service_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0} // Terminator
        },
    },

    {0} // Service table terminator
};

// -----------------------------------------------------------------------------
// GAP Advertising Configuration
// -----------------------------------------------------------------------------
static void ble_service_setup_and_start_adv(void) {
  if (!s_sync_done) {
    return;
  }

  if (ble_gap_adv_active()) {
    ble_gap_adv_stop();
  }

  const char *device_name = ble_svc_gap_device_name();
  if (!device_name) {
    device_name = "tinyTouch Key";
  }

  // Primary Advertising Payload (28 bytes: Flags 3, Name 15, Appearance 4, UUIDs 6)
  struct ble_hs_adv_fields fields = {0};
  fields.flags = (s_allow_new_pairing ? BLE_HS_ADV_F_DISC_GEN : 0) | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.name = (uint8_t *)device_name;
  fields.name_len = strlen(device_name);
  fields.name_is_complete = 1;
  fields.appearance = 0x03C1; // Keyboard
  fields.appearance_is_present = 1;

  static const ble_uuid16_t adv_uuids[] = {
      BLE_UUID16_INIT(BLE_UUID_HOGP_SERVICE),
      BLE_UUID16_INIT(BLE_UUID_BATTERY_SERVICE)
  };
  fields.uuids16 = (ble_uuid16_t *)adv_uuids;
  fields.num_uuids16 = 2;
  fields.uuids16_is_complete = 0;

  int rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
    return;
  }

  // Scan Response Data: NUS 128-bit Service UUID + Tx Power Level (21 bytes total)
  struct ble_hs_adv_fields rsp_fields = {0};
  static const ble_uuid128_t rsp_uuids[] = {
      BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                       0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
  };
  rsp_fields.uuids128 = (ble_uuid128_t *)rsp_uuids;
  rsp_fields.num_uuids128 = 1;
  rsp_fields.uuids128_is_complete = 1;
  rsp_fields.tx_pwr_lvl_is_present = 1;
  rsp_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set scan response fields: %d", rc);
    return;
  }

  struct ble_gap_adv_params adv_params = {0};
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = s_allow_new_pairing ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON;
  adv_params.itvl_min = 32; // 20 ms
  adv_params.itvl_max = 64; // 40 ms

  // Duration: 30 seconds (30000 ms) for fingerprint-gated pairing, indefinite for bonded reconnection
  int32_t duration_ms = s_allow_new_pairing ? 30000 : BLE_HS_FOREVER;

  rc = ble_gap_adv_start(s_own_addr_type, NULL, duration_ms, &adv_params, ble_service_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
  } else {
    ESP_LOGI(TAG, "BLE advertising started (pairable=%d, duration=%ld ms)", (int)s_allow_new_pairing, (long)duration_ms);
  }
}

// -----------------------------------------------------------------------------
// GAP Event Handler
// -----------------------------------------------------------------------------
static int ble_service_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "BLE connect event status=%d handle=%u", event->connect.status, event->connect.conn_handle);
      if (event->connect.status == 0) {
        s_conn_handle = event->connect.conn_handle;
        s_is_connected = true;

        // Apply Apple/Windows compliant connection parameters:
        // itvl_min = 24 (30ms), itvl_max = 40 (50ms), latency = 20, supervision_timeout = 400 (4000ms)
        struct ble_gap_upd_params params = {
            .itvl_min = 24,
            .itvl_max = 40,
            .latency = 20,
            .supervision_timeout = 400,
            .min_ce_len = 0,
            .max_ce_len = 0,
        };
        ble_gap_update_params(s_conn_handle, &params);

        // Initiate security / encryption
        ble_gap_security_initiate(s_conn_handle);
      } else {
        s_is_connected = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        if (s_adv_enabled) {
          ble_service_setup_and_start_adv();
        }
      }
      return 0;

    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "BLE disconnect event reason=%d", event->disconnect.reason);
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      s_is_connected = false;
      if (s_adv_enabled) {
        ble_service_setup_and_start_adv();
      }
      return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
      ESP_LOGD(TAG, "BLE conn updated status=%d", event->conn_update.status);
      return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
      ESP_LOGI(TAG, "BLE encryption change status=%d", event->enc_change.status);
      return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
      ESP_LOGI(TAG, "BLE advertising complete reason=%d", event->adv_complete.reason);
      if (event->adv_complete.reason == BLE_HS_ETIMEOUT && s_allow_new_pairing && !s_is_connected) {
        // Pairing window expired; switch to non-discoverable mode
        s_allow_new_pairing = false;
        if (s_adv_enabled) {
          ble_service_setup_and_start_adv();
        }
      }
      return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
      ESP_LOGI(TAG, "BLE subscribe attr_handle=%u cur_notify=%d",
               event->subscribe.attr_handle, event->subscribe.cur_notify);
      return 0;

    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(TAG, "BLE MTU update: conn_handle=%u mtu=%u",
               event->mtu.conn_handle, event->mtu.value);
      return 0;

    default:
      return 0;
  }
}

// -----------------------------------------------------------------------------
// NimBLE Callbacks
// -----------------------------------------------------------------------------
static void ble_service_on_sync(void) {
  int rc = ble_hs_util_ensure_addr(0);
  assert(rc == 0);

  rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to infer address type: %d", rc);
    return;
  }

  s_sync_done = true;
  ESP_LOGI(TAG, "NimBLE host synchronized; own_addr_type=%u", s_own_addr_type);

  if (s_adv_enabled) {
    ble_service_setup_and_start_adv();
  }
}

static void ble_service_on_reset(int reason) {
  ESP_LOGW(TAG, "NimBLE stack reset; reason=%d", reason);
}

static void ble_service_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
  char buf[BLE_UUID_STR_LEN];
  switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
      ESP_LOGD(TAG, "Registered service %s with handle=%d",
               ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
      break;
    case BLE_GATT_REGISTER_OP_CHR:
      ESP_LOGD(TAG, "Registered characteristic %s with def_handle=%d val_handle=%d",
               ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
               ctxt->chr.def_handle, ctxt->chr.val_handle);
      break;
    case BLE_GATT_REGISTER_OP_DSC:
      ESP_LOGD(TAG, "Registered descriptor %s with handle=%d",
               ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
      break;
    default:
      assert(0);
      break;
  }
}

static void ble_service_host_task(void *param) {
  ESP_LOGI(TAG, "NimBLE host task started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

// -----------------------------------------------------------------------------
// Public APIs
// -----------------------------------------------------------------------------
void ble_service_init(void) {
  static bool s_initialized = false;
  if (s_initialized) {
    return;
  }
  s_initialized = true;

  esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
    return;
  }

  // Initialize NimBLE host configuration
  ble_hs_cfg.reset_cb = ble_service_on_reset;
  ble_hs_cfg.sync_cb = ble_service_on_sync;
  ble_hs_cfg.gatts_register_cb = ble_service_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  // LE Secure Connections and Bonding
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  int rc = ble_gatts_count_cfg(s_gatt_services);
  assert(rc == 0);

  rc = ble_gatts_add_svcs(s_gatt_services);
  assert(rc == 0);

  ble_svc_gap_device_name_set("tinyTouch Key");
  ble_svc_gap_device_appearance_set(0x03C1);

  ble_store_config_init();

  nimble_port_freertos_init(ble_service_host_task);
  ESP_LOGI(TAG, "BLE service initialized");
}

bool ble_service_is_connected(void) {
  return s_is_connected;
}

bool ble_service_send_keyboard_report(uint8_t modifier, const uint8_t keycodes[6]) {
  if (!s_is_connected || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return false;
  }

  // Zero-trust check: keystrokes only transmitted when connection is encrypted
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(s_conn_handle, &desc) != 0 || !desc.sec_state.encrypted) {
    ESP_LOGW(TAG, "Keyboard report rejected: BLE connection not encrypted");
    return false;
  }

  uint8_t report[8] = {0};
  report[0] = modifier;
  report[1] = 0x00; // Reserved
  if (keycodes) {
    memcpy(&report[2], keycodes, 6);
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));
  if (!om) {
    return false;
  }

  int rc = ble_gatts_notify_custom(s_conn_handle, s_hid_report_val_handle, om);
  return (rc == 0);
}

void ble_service_send_console_line(const char *line) {
  if (!s_is_connected || s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !line) {
    return;
  }

  size_t len = strlen(line);
  uint16_t mtu = ble_att_mtu(s_conn_handle);
  uint16_t chunk_max = (mtu > 3) ? (mtu - 3) : 20;
  if (chunk_max > 128) {
    chunk_max = 128;
  }

  const uint8_t *ptr = (const uint8_t *)line;
  while (len > 0) {
    size_t send_len = (len > chunk_max) ? chunk_max : len;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(ptr, send_len);
    if (!om) {
      break;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_nus_tx_val_handle, om);
    if (rc != 0) {
      os_mbuf_free_chain(om);
      break;
    }
    ptr += send_len;
    len -= send_len;
  }
}

void ble_service_set_battery_level(uint8_t level) {
  if (level > 100) {
    level = 100;
  }
  s_battery_level = level;
  if (s_is_connected && s_conn_handle != BLE_HS_CONN_HANDLE_NONE && s_battery_val_handle != 0) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&s_battery_level, 1);
    if (om) {
      ble_gatts_notify_custom(s_conn_handle, s_battery_val_handle, om);
    }
  }
}

void ble_service_start_advertising(bool allow_new_pairing) {
  s_adv_enabled = true;
  s_allow_new_pairing = allow_new_pairing;
  ble_service_setup_and_start_adv();
}
