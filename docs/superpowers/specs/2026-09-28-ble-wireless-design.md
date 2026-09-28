# Thiết kế Kỹ thuật: Tính năng Không dây BLE & Quản lý Nguồn Dual-Mode cho tinyTouch (ESP32-S3)

- **Ngày tạo:** 2026-09-28
- **Nhánh phát triển:** `feature/ble-wireless`
- **Trạng thái:** Bản thảo thiết kế hoàn chỉnh (Approved by Council)

---

## 1. Mục tiêu & Phạm vi

### 1.1. Mục tiêu
Nâng cấp thiết bị bảo mật vân tay tinyTouch (sử dụng SoC ESP32-S3) từ dạng khóa bảo mật chỉ cắm dây USB Type-C sang thiết bị **Dual-Mode** hỗ trợ kết nối không dây Bluetooth Low Energy (BLE) với nguồn pin LiPo 370 mAh:
1. **Gõ phím không dây (Wireless HID Keyboard):** Tự động gõ mật khẩu/PIN đăng nhập máy tính khi xác thực vân tay thành công qua chuẩn BLE HOGP (HID over GATT Profile).
2. **Cấu hình & Đổi màu Aura LED không dây:** Quản lý thiết bị từ trình duyệt (Google Chrome / Microsoft Edge) qua Web Bluetooth API (GATT Custom Serial).
3. **Dual-Mode mượt mà:** Cắm cáp USB ưu tiên 100% kết nối có dây (TinyUSB HID + CDC) và sạc pin; rút cáp tự động kích hoạt BLE.
4. **Bảo mật PIV SmartCard:** Chế độ PIV SmartCard (Apple macOS CCID login) chỉ chạy qua cáp USB Type-C vật lý để đảm bảo an toàn tuyệt đối, không truyền dữ liệu PIV APDU qua sóng vô tuyến.
5. **Quản lý nguồn thông minh không cần công tắc vật lý:** Tự động điều phối giấc ngủ sâu để bảo vệ cell pin 370 mAh.

---

## 2. Kiến trúc Hệ thống Dual-Mode (USB / BLE)

```
                       ┌───────────────────────────────┐
                       │     Cảm biến vân tay UART     │
                       └──────────────┬────────────────┘
                                      │ (Touch IRQ / UART data)
                                      ▼
                       ┌───────────────────────────────┐
                       │  tinyTouch Firmware (ESP-S3)  │
                       │    - Xử lý vân tay            │
                       │    - Quản lý NVS & Bí mật     │
                       │    - Bộ định tuyến Transport │
                       └──────┬─────────────────┬──────┘
                              │                 │
            [VBUS / USB Mount]│                 │[Chạy Pin / Ngắt USB]
                              ▼                 ▼
             ┌─────────────────────┐   ┌──────────────────────────┐
             │      USB Stack      │   │     BLE NimBLE Stack     │
             │   (TinyUSB Core)    │   │  (Bluetooth LE 5.0 Core) │
             ├─────────────────────┤   ├──────────────────────────┤
             │ • HID Keyboard      │   │ • HOGP Keyboard (0x1812) │
             │ • CDC Serial        │   │ • Custom NUS (6E400001)  │
             │ • CCID PIV SmartCard│   │ • Battery Service(0x180F)│
             │ • VBUS Sạc Pin      │   │ • Device Info (0x180A)   │
             └─────────────────────┘   └──────────────────────────┘
```

### 2.1. Logic Điều phối Bộ định tuyến (Transport Router)
- **Nhận diện trạng thái nguồn:** Kiểm tra `tud_mounted()` và trạng thái kết nối USB CDC.
  - Khi cắm vào máy tính có dữ liệu (`tud_mounted() == true`):
    - Đặt cờ `transport_mode = TRANSPORT_USB`.
    - Phím gõ xuất qua `tud_hid_keyboard_report()`.
    - Lệnh Console nhận qua TinyUSB CDC.
    - BLE ngừng Advertising hoặc chuyển sang chế độ Standby không phát sóng.
  - Khi rút cáp USB hoặc cắm vào củ sạc tường chỉ có VBUS (không mount USB data):
    - Đặt cờ `transport_mode = TRANSPORT_BLE`.
    - Khởi động NimBLE Advertising.
    - Phím gõ xuất qua BLE HID HOGP Input Report Notification.
    - Lệnh Console nhận qua BLE Custom Serial RX Characteristic.

---

## 3. Cấu hình NimBLE & Cấu trúc GATT Services

### 3.1. Cấu hình ESP-IDF v5.3 (`sdkconfig.defaults`)
Sử dụng NimBLE stack thay cho Bluedroid để tiết kiệm ~120KB RAM và ~400KB Flash:
```ini
# Bluetooth & NimBLE Configuration
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS=n
CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME="tinyTouch Key"
CONFIG_BT_NIMBLE_SVC_GAP_APPEARANCE=0x03C1 # Keyboard Appearance
CONFIG_BT_NIMBLE_PINNED_TO_CORE=0
CONFIG_BT_NIMBLE_TASK_STACK_SIZE=4096
CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT=24
CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT=12
```

### 3.2. Cấu trúc GATT Services
Firmware đăng ký 4 GATT Services theo đúng tiêu chuẩn tương thích với macOS, iOS, Windows, Android:

| Service | UUID | Đặc tả & Thuộc tính | Mục đích |
| :--- | :--- | :--- | :--- |
| **Human Interface Device (HOGP)** | `0x1812` | • Protocol Mode (`0x2A4E` - Read/WriteWithoutResponse)<br>• HID Info (`0x2A4A` - Read)<br>• Report Map (`0x2A4B` - Read)<br>• Input Report (`0x2A4D` - Read/Notify + CCCD `0x2902`) | Bàn phím HID gửi keystroke mở khóa |
| **Custom Serial (NUS Compatible)** | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | • RX Char (`6E400002-...` - Write/WriteWithoutResponse)<br>• TX Char (`6E400003-...` - Notify + CCCD `0x2902`) | Giao tiếp dòng lệnh Console với Web Controller |
| **Battery Service** | `0x180F` | • Battery Level (`0x2A19` - Read/Notify 0-100%) | Bắt buộc cho Apple macOS/iOS nhận diện phụ kiện |
| **Device Information** | `0x180A` | • Manufacturer Name (`0x2A29`)<br>• Model Number (`0x2A24`)<br>• PnP ID (`0x2A50` - Vendor ID/Product ID) | Chuẩn hóa phần cứng trên Windows/macOS |

### 3.3. Thông số BLE Connection Parameters (Apple/Windows Compliant)
Để đảm bảo gõ phím tức thì và không bị hệ điều hành ngắt kết nối:
- `Connection Interval Min`: 24 (tương đương 30 ms).
- `Connection Interval Max`: 40 (tương đương 50 ms).
- `Slave Latency`: 20 (cho phép ESP32-S3 ngủ bỏ qua 20 nhịp beacon).
- `Supervision Timeout`: 400 (4000 ms = 4 giây).
- Đảm bảo thỏa mãn quy tắc Apple: `IntervalMax * (SlaveLatency + 1) = 50ms * 21 = 1050ms <= 2000ms`.

---

## 4. Chiến lược Quản lý Nguồn Phần mềm (Pin 370 mAh)

Không có công tắc cơ vật lý, firmware quản lý 3 tầng trạng thái năng lượng:

```
                  ┌─────────────────────────────────────────┐
                  │          Tầng 1: Active Mode            │
                  │  CPU: 160MHz | Đọc cảm biến | Gõ phím   │
                  └───────────────────┬─────────────────────┘
                                      │ Không thao tác sau 5 giây
                                      ▼
                  ┌─────────────────────────────────────────┐
                  │    Tầng 2: Light Sleep (BLE Connected)  │
                  │  CPU: Tickless 40MHz | Slave Latency 20 │
                  │  Tắt 100% LED Aura thở | Dòng: ~8-12mA  │
                  └───────────────────┬─────────────────────┘
                                      │ Không thao tác sau 15 phút
                                      ▼
                  ┌─────────────────────────────────────────┐
                  │          Tầng 3: Deep Sleep             │
                  │  Tắt RF | Ru ngủ cảm biến UART          │
                  │  Dòng tiêu thụ: < 30 µA                 │
                  └───────────────────┬─────────────────────┘
                                      │ Ngắt Touch IRQ / Cắm USB
                                      ▼
                   [Thức dậy: Chạy bộ lọc chống thức giả]
```

### 4.1. Chi tiết 3 Tầng Giấc ngủ
1. **Tầng 1 - Active:** Khi ngón tay chạm cảm biến hoặc nhận lệnh console. LED nháy phản hồi.
2. **Tầng 2 - Light Sleep (0 – 15 phút):**
   - Duy trì kết nối BLE với máy tính.
   - **Tắt hoàn toàn LED Aura thở** khi chạy pin để tránh dòng rò 15–30mA.
   - Khi ngón tay chạm, đánh thức tức thì `< 15ms` gửi phím.
3. **Tầng 3 - Deep Sleep (Sau 15 phút không chạm hoặc Pin < 3.2V):**
   - Gửi lệnh UART ru ngủ cảm biến vân tay (`fingerprint_sleep()`).
   - Tắt Bluetooth radio.
   - Đặt nguồn đánh thức: Chân `TOUCH_IRQ` (RTC GPIO) và chân USB VBUS/D+.
   - Dòng tiêu thụ xuống dưới 50 µA.
4. **Bộ lọc chống thức giả (Anti-Ghost Wakeup Filter):**
   - Khi cọ xát trong túi/balo kích hoạt chân `TOUCH_IRQ`, ESP32-S3 thức dậy nhưng chỉ giữ nguồn cảm biến tối đa **1.5 giây**.
   - Nếu trong 1.5 giây không có dữ liệu vân tay hợp lệ từ UART, chip lập tức quay lại Deep Sleep ngay lập tức để bảo vệ pin.

---

## 5. Cơ chế Bảo mật & Hardening (Zero Trust Wireless)

| Rủi ro | Giải pháp Hardening trong Firmware |
| :--- | :--- |
| **Nghe lén phím bấm (Sniffing)** | Bắt buộc **LE Secure Connections (LE SC)** với mã hóa AES-128 liên tục luân chuyển khóa; lưu bonding an toàn trong ESP32-S3 NVS Flash. |
| **Ghép đôi trái phép (Unauthorized Pairing)** | **Fingerprint-Gated Pairing:** Chỉ mở BLE Advertising cho thiết bị mới ghép đôi trong **30 giây sau khi chạm đúng vân tay quản trị**. Bình thường chỉ Direct Advertising tới thiết bị đã Bond. |
| **Tấn công qua BLE Console** | **Zero Trust Control Plane:**<br>1. **CẤM TUYỆT ĐỐI** lệnh `OTA BEGIN`, `OTA WRITE`, `RESET FACTORY`, `PIV CREATE` qua cổng BLE NUS (các lệnh này chỉ nhận qua cáp USB Type-C).<br>2. Các lệnh cấu hình (`SET LED`, `SET_KEY_SEQ`, `HOST ADD`) qua BLE bắt buộc phải xác thực lệnh `AUTH` kèm chạm vân tay vật lý trong 15 giây. |
| **Bắn phím mù (Ghost Keystroke)** | Chỉ phát phím khi BLE Connection State là `CONNECTED` và `ENCRYPTED`. Đèn LED nháy xanh báo hiệu hoàn tất gõ phím. |

---

## 6. Kiến trúc Web Controller (Web Bluetooth Frontend)

### 6.1. Giao diện Người dùng (UI)
- Nút kết nối ở thanh Header mở rộng thành nút đôi hoặc Dropdown:
  - **"Kết nối USB Serial"** (dành cho cài đặt nâng cao, nạp firmware OTA, cấu hình PIV).
  - **"Kết nối Bluetooth (BLE)"** (dành cho kết nối không dây cấu hình màu LED, kiểm tra pin, xem trạng thái).
- Hiển thị badge mức pin thiết bị (đọc từ BLE Battery Service `0x180F`).
- Cảnh báo thân thiện: Thông báo rõ Web Bluetooth yêu cầu trình duyệt Chrome/Edge và không hỗ trợ trên Safari.

### 6.2. Module `web_ble.js` / Tích hợp Frontend
- Sử dụng `navigator.bluetooth.requestDevice`:
  ```javascript
  const device = await navigator.bluetooth.requestDevice({
    filters: [{ namePrefix: 'tinyTouch' }],
    optionalServices: [
      '6e400001-b5a3-f393-e0a9-e50e24dcca9e', // NUS Custom Serial
      'battery_service'
    ]
  });
  ```
- Trừu tượng hóa lớp truyền nhận Serial: Hàm `sendSerialCommand(cmd)` tự động định tuyến qua `writer.write()` (nếu kết nối Web Serial) hoặc qua `rxChar.writeValueWithoutResponse()` (nếu kết nối Web Bluetooth).

---

## 7. Cấu trúc File & Kế hoạch Tích hợp Codebase

### 7.1. Các file thêm mới và chỉnh sửa

1. **`firmware/main/CMakeLists.txt`:**
   - Thêm `bt` vào danh sách `REQUIRES`.
   - Thêm `ble_service.c` vào danh sách `SRCS`.
2. **`firmware/sdkconfig.defaults`:**
   - Bổ sung cấu hình NimBLE, Secure Connections, và cấu hình bộ nhớ buffer BLE.
3. **`firmware/main/ble_service.h` & `ble_service.c`:**
   - Khởi tạo stack NimBLE, GAP Advertising, HOGP Service, NUS Custom Serial, Battery Service.
   - Cung cấp API gửi phím BLE: `ble_send_keyboard_report(...)`.
   - Cung cấp API gửi/nhận dòng lệnh Console qua BLE.
4. **`firmware/main/touch_pin_hid.c`:**
   - Thay thế việc gọi trực tiếp `tud_hid_keyboard_report` bằng bộ định tuyến:
     `transport_send_key(modifier, key)` tự động chọn USB hoặc BLE.
5. **`firmware/main/config_console.c`:**
   - Hỗ trợ nguồn dòng lệnh từ BLE NUS song song với TinyUSB CDC.
   - Thêm bộ lọc bảo mật: Chặn `OTA` và `RESET FACTORY` khi `source == SOURCE_BLE`.
6. **`controller/index.html`:**
   - Bổ sung UI nút bấm "Kết nối Bluetooth".
   - Tích hợp lớp Web Bluetooth Driver giao tiếp với NUS Serial và Battery Service.

---

## 8. Tự đánh giá Spec (Spec Self-Review)

1. **Placeholder scan:** Không có "TBD", "TODO", hay định nghĩa mơ hồ.
2. **Internal consistency:**
   - Đảm bảo PIV SmartCard duy trì trên USB CCID có dây, không mâu thuẫn với luồng BLE.
   - Khớp thông số Connection Parameters (`30-50ms`, `Latency 20`) với khuyến cáo của Apple BLE Accessory Design Guidelines.
3. **Scope check:** Phạm vi tập trung chính xác vào kết nối BLE (HOGP + NUS) và quản lý nguồn pin 370 mAh.
4. **Ambiguity check:** Cơ chế phân định nguồn lệnh (chặn lệnh nhạy cảm qua BLE) được quy định rõ ràng.
