# Kế hoạch Triển khai Tính năng Không dây BLE & Quản lý Nguồn Dual-Mode

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bổ sung kết nối không dây Bluetooth Low Energy (BLE) với NimBLE stack cho tinyTouch (ESP32-S3), hỗ trợ gõ phím không dây HOGP, cấu hình qua Web Bluetooth, bộ định tuyến Dual-Mode (ưu tiên USB Type-C, tự động sang BLE khi chạy pin 370 mAh) và quản lý nguồn ngủ phân tầng chống kiệt pin.

**Architecture:** Sử dụng NimBLE stack trong ESP-IDF v5.3 đăng ký 4 GATT Services (HOGP 0x1812, Custom NUS Serial 6E400001, Battery 0x180F, Device Info 0x180A). Lớp trừu tượng Transport Router trong firmware điều phối phím gõ và lệnh console giữa TinyUSB và BLE. Quản lý nguồn 3 tầng (Active -> Light Sleep với Slave Latency 20 -> Deep Sleep sau 15 phút kèm bộ lọc chống thức giả 1.5s). Web Controller nâng cấp hỗ trợ kết nối Web Bluetooth.

**Tech Stack:** ESP-IDF v5.3 (C, FreeRTOS, NimBLE, TinyUSB), Web Bluetooth API (JavaScript / Tailwind CSS / Chrome).

**Spec:** `docs/superpowers/specs/2026-09-28-ble-wireless-design.md`

## Global Constraints
- `CONFIG_BT_ENABLED=y`, `CONFIG_BT_NIMBLE_ENABLED=y`.
- NimBLE Peripheral role duy nhất, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`.
- Cấm tuyệt đối lệnh `OTA BEGIN`, `OTA WRITE`, `OTA COMMIT`, `RESET FACTORY`, `PIV CREATE` qua cổng BLE NUS; chỉ cho phép qua USB vật lý.
- PIV SmartCard CCID duy trì 100% qua USB có dây, không truyền qua BLE.
- BLE Connection Parameters tuân thủ chuẩn Apple BLE Accessory Guidelines: Interval 30-50ms, Slave Latency 20, Timeout 4s.

---

### Task 1: Cấu hình ESP-IDF NimBLE Stack và Build System

**Files:**
- Modify: `firmware/sdkconfig.defaults`
- Modify: `firmware/main/CMakeLists.txt`

**Interfaces:**
- Consumes: ESP-IDF v5.3 BT & NimBLE components.
- Produces: Hệ thống build hỗ trợ NimBLE và các symbol BLE trong `esp_nimble_hci.h`, `nimble/nimble_port.h`.

- [ ] **Step 1: Cập nhật cấu hình `firmware/sdkconfig.defaults`**
Bổ sung các cờ NimBLE tiết kiệm RAM/Flash:
```ini
# Bluetooth & NimBLE
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME="tinyTouch Key"
CONFIG_BT_NIMBLE_SVC_GAP_APPEARANCE=0x03C1
CONFIG_BT_NIMBLE_PINNED_TO_CORE=0
CONFIG_BT_NIMBLE_TASK_STACK_SIZE=4096
```

- [ ] **Step 2: Thêm component `bt` vào `firmware/main/CMakeLists.txt`**
Cập nhật `REQUIRES`:
```cmake
REQUIRES app_update driver esp_partition esp_timer esp_tinyusb mbedtls nvs_flash bt
```

- [ ] **Step 3: Kiểm tra cấu hình và compile thử**
Run: `cd firmware && idf.py reconfigure`
Expected: CMake tạo thành công build files có chứa NimBLE headers.

- [ ] **Step 4: Commit thay đổi cấu hình**
```bash
git add firmware/sdkconfig.defaults firmware/main/CMakeLists.txt
git commit -m "build(firmware): enable nimble bluetooth stack in sdkconfig and cmake"
```

---

### Task 2: Module Dịch vụ BLE (`ble_service.h` và `ble_service.c`)

**Files:**
- Create: `firmware/main/ble_service.h`
- Create: `firmware/main/ble_service.c`
- Modify: `firmware/main/CMakeLists.txt` (thêm `ble_service.c` vào `SRCS`)

**Interfaces:**
- Produces:
  - `void ble_service_init(void);`
  - `bool ble_service_is_connected(void);`
  - `bool ble_service_send_keyboard_report(uint8_t modifier, const uint8_t keycodes[6]);`
  - `void ble_service_send_console_line(const char *line);`
  - `void ble_service_set_battery_level(uint8_t level);`
  - `void ble_service_start_advertising(bool allow_new_pairing);`
  - Callback nhận dòng lệnh từ BLE: `void config_console_feed_ble_input(const uint8_t *data, size_t len);`

- [ ] **Step 1: Viết header `firmware/main/ble_service.h`**
Khai báo đầy đủ các hàm giao tiếp trên và định nghĩa UUID của 4 dịch vụ (HOGP `0x1812`, NUS `6E400001-...`, Battery `0x180F`, Device Info `0x180A`).

- [ ] **Step 2: Viết mã nguồn `firmware/main/ble_service.c`**
Triển khai:
1. GATT Server với NimBLE `ble_gatts_count_cfg` và `ble_gatts_add_svcs`.
2. HID Report Map descriptor cho bàn phím chuẩn 8-byte (Modifier, Reserved, 6 Keycodes).
3. GATT NUS (RX write callback gọi `config_console_feed_ble_input`, TX notify characteristic).
4. GAP Event Handler xử lý `BLE_GAP_EVENT_CONNECT`, `BLE_GAP_EVENT_DISCONNECT`, `BLE_GAP_EVENT_ENC_CHANGE`.
5. Thiết lập Connection Parameters: `itvl_min = 24`, `itvl_max = 40`, `latency = 20`, `supervision_timeout = 400`.
6. Quản lý Advertising: Fingerprint-gated (chỉ cho phép pair mới khi được cấp quyền).

- [ ] **Step 3: Đăng ký `ble_service.c` vào `CMakeLists.txt` và build thử**
Run: `cd firmware && idf.py build`
Expected: Biên dịch thành công `ble_service.o` không có warning/error.

- [ ] **Step 4: Commit module `ble_service`**
```bash
git add firmware/main/ble_service.h firmware/main/ble_service.c firmware/main/CMakeLists.txt
git commit -m "feat(ble): implement nimble ble service for hogp, nus serial and battery"
```

---

### Task 3: Lớp Trừu tượng Dual-Mode Transport trong `touch_pin_hid.c`

**Files:**
- Modify: `firmware/main/touch_pin_hid.c`
- Modify: `firmware/main/touch_pin_hid.h`

**Interfaces:**
- Consumes:
  - `tud_mounted()`, `tud_hid_ready()`, `tud_hid_keyboard_report()` từ TinyUSB.
  - `ble_service_is_connected()`, `ble_service_send_keyboard_report()` từ `ble_service.h`.
- Produces:
  - `bool transport_send_key(uint8_t modifier, uint8_t key);` (thay thế logic `send_key` trực tiếp).
  - `bool transport_is_usb_active(void);`

- [ ] **Step 1: Cập nhật logic gửi phím trong `touch_pin_hid.c`**
Thay thế `send_key()` cũ:
```c
static bool transport_send_report(uint8_t modifier, uint8_t keycode) {
  uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
  if (tud_mounted() && tud_hid_ready()) {
    if (!tud_hid_keyboard_report(0, modifier, keycodes)) return false;
    vTaskDelay(pdMS_TO_TICKS(device_config_typing_delay_ms()));
    tud_hid_keyboard_report(0, 0, NULL);
    vTaskDelay(pdMS_TO_TICKS(device_config_typing_delay_ms()));
    return true;
  }
  if (ble_service_is_connected()) {
    if (!ble_service_send_keyboard_report(modifier, keycodes)) return false;
    vTaskDelay(pdMS_TO_TICKS(device_config_typing_delay_ms()));
    uint8_t empty[6] = {0};
    ble_service_send_keyboard_report(0, empty);
    vTaskDelay(pdMS_TO_TICKS(device_config_typing_delay_ms()));
    return true;
  }
  return false;
}
```

- [ ] **Step 2: Đảm bảo luồng PIV SmartCard không bị ảnh hưởng**
Giữ nguyên kiểm tra `usb_ccid` trong `touch_pin_hid.c` chỉ kích hoạt khi cắm cáp USB Type-C.

- [ ] **Step 3: Biên dịch kiểm tra**
Run: `cd firmware && idf.py build`
Expected: Build PASS.

- [ ] **Step 4: Commit thay đổi Transport Router**
```bash
git add firmware/main/touch_pin_hid.c firmware/main/touch_pin_hid.h
git commit -m "feat(transport): route keyboard keystrokes via usb or ble hogp"
```

---

### Task 4: Tích hợp Console BLE và Cơ chế Zero Trust Hardening

**Files:**
- Modify: `firmware/main/config_console.c`
- Modify: `firmware/main/config_console.h`

**Interfaces:**
- Consumes:
  - `config_console_feed_ble_input(const uint8_t *data, size_t len)` từ `ble_service.c`.
  - `ble_service_send_console_line(const char *line)` từ `ble_service.h`.
- Produces:
  - Phân luồng nguồn lệnh: `COMMAND_SOURCE_CDC` vs `COMMAND_SOURCE_BLE`.
  - Trả kết quả `reply()` về đúng giao diện tương ứng (USB CDC hoặc BLE NUS).
  - Bộ lọc bảo mật: Chặn đứng lệnh `OTA` và `RESET FACTORY` nếu nhận từ BLE.

- [ ] **Step 1: Thêm cờ nguồn lệnh trong `config_console.c`**
Định nghĩa:
```c
typedef enum {
  CMD_SRC_USB_CDC,
  CMD_SRC_BLE_NUS
} cmd_source_t;
static cmd_source_t current_cmd_source = CMD_SRC_USB_CDC;
```

- [ ] **Step 2: Triển khai bộ lọc Zero Trust trong `handle_command()`**
```c
if (current_cmd_source == CMD_SRC_BLE_NUS) {
  if (strncmp(command, "OTA ", 4) == 0 ||
      strcmp(command, "RESET FACTORY") == 0 ||
      strcmp(command, "PIV CREATE") == 0) {
    reply("ERR DISALLOWED_ON_BLE");
    return;
  }
}
```

- [ ] **Step 3: Phản hồi về đúng kênh truyền (`config_console_send_line`)**
Nếu `current_cmd_source == CMD_SRC_BLE_NUS`, gọi `ble_service_send_console_line(line)`, ngược lại gọi `cdc_write_all(...)`.

- [ ] **Step 4: Biên dịch kiểm tra**
Run: `cd firmware && idf.py build`
Expected: Build PASS.

- [ ] **Step 5: Commit tính năng Console Zero Trust BLE**
```bash
git add firmware/main/config_console.c firmware/main/config_console.h
git commit -m "feat(security): route console lines via ble with zero-trust command filtering"
```

---

### Task 5: Quản lý Nguồn Phần mềm & Giấc ngủ Phân tầng

**Files:**
- Create: `firmware/main/power_mgmt.h`
- Create: `firmware/main/power_mgmt.c`
- Modify: `firmware/main/main.c`
- Modify: `firmware/main/fingerprint.c` (thêm hàm `fingerprint_sleep()`)
- Modify: `firmware/main/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `void power_mgmt_init(void);`
  - `void power_mgmt_note_activity(void);`
  - `void power_mgmt_enter_deep_sleep(void);`
  - `void fingerprint_sleep(void);`

- [ ] **Step 1: Viết hàm ru ngủ cảm biến vân tay `fingerprint_sleep()` trong `fingerprint.c`**
Gửi lệnh UART Standby/Sleep tới module quang/điện dung trước khi tắt CPU.

- [ ] **Step 2: Tạo module `power_mgmt.c`**
Triển khai:
1. Theo dõi thời gian không hoạt động `last_activity_time`.
2. Khi chạy pin và không có thao tác sau 15 phút:
   - Gọi `fingerprint_sleep()`.
   - Cấu hình nguồn đánh thức Deep Sleep qua RTC GPIO (`TOUCH_IRQ`) và VBUS pin.
   - Gọi `esp_deep_sleep_start()`.
3. Bộ lọc chống thức giả (Anti-Ghost Wakeup Filter): Khi thức dậy do ngắt `TOUCH_IRQ`, khởi chạy timer 1.5s; nếu không có ngón tay hợp lệ, quay lại Deep Sleep ngay.
4. Tắt LED Aura thở khi `tud_mounted() == false`.

- [ ] **Step 3: Khởi chạy `power_mgmt_init()` trong `main.c`**
- [ ] **Step 4: Biên dịch kiểm tra**
Run: `cd firmware && idf.py build`
Expected: Build PASS.

- [ ] **Step 5: Commit module Quản lý Nguồn**
```bash
git add firmware/main/power_mgmt.c firmware/main/power_mgmt.h firmware/main/fingerprint.c firmware/main/fingerprint.h firmware/main/main.c firmware/main/CMakeLists.txt
git commit -m "feat(power): implement 3-tier software power management and anti-ghost wakeup"
```

---

### Task 6: Cập nhật Web Controller (Giao diện Web Bluetooth)

**Files:**
- Modify: `controller/index.html`

**Interfaces:**
- Consumes: Web Bluetooth API (`navigator.bluetooth`).
- Produces:
  - UI nút "Kết nối Bluetooth" bên cạnh "Kết nối USB".
  - Hiển thị thông báo mức Pin (Battery %).
  - Trừu tượng hóa hàm `sendSerialCommand` gửi qua NUS Serial nếu đang kết nối BLE.

- [ ] **Step 1: Thêm nút Kết nối Bluetooth trong Navigation Header**
Bổ sung nút `btnConnectBle` với icon Bluetooth bên cạnh nút `btnConnect` (USB).

- [ ] **Step 2: Triển khai Web Bluetooth Driver trong JS**
1. Hàm `connectBle()` gọi `navigator.bluetooth.requestDevice` với service NUS `6e400001-b5a3-f393-e0a9-e50e24dcca9e` và `battery_service`.
2. Lắng nghe `characteristicvaluechanged` trên TX Characteristic để nhận dữ liệu console.
3. Gửi lệnh qua RX Characteristic (`writeValueWithoutResponse`).
4. Đọc mức pin từ Battery Level Characteristic (`0x2A19`).

- [ ] **Step 3: Tích hợp định tuyến lệnh `sendSerialCommand()`**
Nếu kết nối Bluetooth, gửi qua BLE NUS; nếu kết nối Serial, gửi qua Web Serial port.

- [ ] **Step 4: Kiểm tra cú pháp JavaScript của Web Controller**
Run: `node -c <(sed -n '/<script>/,/<\/script>/p' controller/index.html | sed '1d;$d')`
Expected: JavaScript cú pháp hợp lệ (PASS).

- [ ] **Step 5: Commit giao diện Web Controller**
```bash
git add controller/index.html
git commit -m "feat(controller): add web bluetooth connectivity and battery telemetry"
```

---

### Task 7: Kiểm thử Tổng hợp & Đồng bộ Tài liệu

**Files:**
- Modify: `docs/PROTOCOL.md`
- Modify: `README.md`

- [ ] **Step 1: Biên dịch toàn bộ Firmware Unified Binary**
Run: `cd firmware && idf.py build`
Expected: Tạo file `firmware/build/tiny_touch_unified.bin` thành công với dung lượng < 1024KB.

- [ ] **Step 2: Cập nhật tài liệu kỹ thuật `docs/PROTOCOL.md`**
Bổ sung đặc tả Bluetooth Low Energy (UUID, GATT Table, bảo mật Zero Trust).

- [ ] **Step 3: Cập nhật `README.md` giới thiệu tính năng Dual-Mode không dây**
- [ ] **Step 4: Commit tài liệu hoàn thiện**
```bash
git add docs/PROTOCOL.md README.md
git commit -m "docs: document ble wireless services and dual-mode operation"
```
