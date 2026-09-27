# tinyTouch Firmware

Mã nguồn firmware chính thức cho thiết bị bảo mật sinh trắc học **tinyTouch**, chạy trên vi điều khiển **ESP32-S3** với framework **ESP-IDF v5.0+**.

---

## Kiến trúc Firmware

Firmware kết hợp nhiều thành phần cấp thấp:
- **TinyUSB Dual-Interface:**
  - **USB CDC (Serial Port):** Giao tiếp điều khiển, cấu hình, nhận diện sự kiện thời gian thực (115200 baud).
  - **USB HID Keyboard:** Bàn phím ảo tự động gõ mật khẩu siêu tốc. Hỗ trợ nhận diện trạng thái Caps Lock qua HID Output Report để tự động bù trừ ký tự hoa/thường.
- **Fingerprint Sensor Driver (UART):** Giao tiếp UART (mặc định 57600 baud) với các module cảm biến vân tay quang/điện dung chuẩn Grow (R503, R502-A, R307,...). Hỗ trợ ngắt chạm cảm ứng (Touch Wakeup).
- **Non-Volatile Storage (NVS):** Lưu trữ cấu hình thiết bị, danh sách Host Pairing, chuỗi phím kết thúc (ending key sequence), và khóa mã hóa.
- **Challenge-Response Security Engine:** Xác thực hai chiều máy chủ - phần cứng qua hàm băm HMAC SHA-256 trước khi xuất mật khẩu.

---

## Cấu trúc thư mục

```
firmware/
├── CMakeLists.txt         # ESP-IDF project definition
├── partitions.csv         # Bảng phân vùng bộ nhớ Flash
├── sdkconfig.defaults     # Cấu hình TinyUSB, CPU clock, flash size
├── sdkconfig              # Cấu hình hiện tại sau build
├── dependencies.lock      # ESP Component Manager lockfile
├── ota_flash.py           # Tiện ích nạp firmware từ xa / qua CDC
├── main/                  # Mã nguồn C chính
│   ├── main.c             # Khởi tạo hệ thống, USB stack, task scheduler
│   ├── config_console.c   # Bộ phân tích lệnh Serial CDC (CLI parser)
│   ├── touch_pin_hid.c    # USB HID Keyboard & Caps Lock compensation
│   ├── device_config.c    # Quản lý cấu hình NVS Flash
│   ├── fingerprint.c      # Driver giao tiếp cảm biến UART
│   ├── piv.c              # Xử lý giao thức SmartCard PIV
│   └── ...
└── README.md
```

---

## Yêu cầu môi trường

- **Hệ điều hành:** macOS (Apple Silicon / Intel), Linux, hoặc Windows (WSL2)
- **ESP-IDF:** v5.0 trở lên (khuyến nghị v5.1.x hoặc v5.2.x)
- **Python:** 3.8+
- **Cáp USB:** Type-C có truyền dữ liệu kết nối trực tiếp cổng USB Native của ESP32-S3

---

## Hướng dẫn cài đặt & Build

### 1. Kích hoạt môi trường ESP-IDF

```bash
# Nạp biến môi trường ESP-IDF
source ~/esp-idf/export.sh
# Hoặc sử dụng alias nếu đã cấu hình
get_idf
```

### 2. Cấu hình Target

Chỉ cần chạy lệnh này một lần khi khởi tạo dự án:

```bash
cd firmware
idf.py set-target esp32s3
```

### 3. Build mã nguồn

```bash
idf.py build
```
File nhị phân sau khi build thành công sẽ nằm tại: `firmware/build/tiny_touch_unified.bin`.

### 4. Nạp firmware (Flash)

Xác định cổng Serial của ESP32-S3:
- macOS: `ls /dev/cu.usbmodem*`
- Linux: `ls /dev/ttyACM*`

Nạp firmware qua lệnh:
```bash
idf.py -p /dev/cu.usbmodem101 flash monitor
```

*(Nhấn `Ctrl + ]` để thoát Serial Monitor)*

---

## Bảng phân vùng Flash (`partitions.csv`)

| Tên phân vùng | Loại | Phụ loại | Offset | Kích thước | Mô tả |
|:--------------|:-----|:---------|:-------|:-----------|:------|
| `nvs`         | data | nvs      | 0x9000 | 24 KB      | Lưu cấu hình thiết bị & host pairing |
| `otadata`     | data | ota      | 0xf000 | 8 KB       | Trạng thái chuyển đổi OTA |
| `phy_init`    | data | phy      | 0x11000| 4 KB       | Dữ liệu cấu hình PHY |
| `ota_0`       | app  | ota_0    | 0x20000| 1920 KB    | Bản firmware chính hiện hành |
| `ota_1`       | app  | ota_1    | 0x200000| 1920 KB   | Bản firmware dự phòng OTA |

> **Lưu ý bảo toàn dữ liệu:** Quá trình nạp firmware thông thường (`idf.py flash` hoặc cập nhật OTA) chỉ ghi đè lên phân vùng `ota_0`/`ota_1`. Mẫu vân tay (lưu trên cảm biến) và dữ liệu ghép nối NVS hoàn toàn được bảo toàn nguyên vẹn.

---

## Danh mục lệnh Console Serial CDC (115200 baud)

Giao thức truyền dòng văn bản kết thúc bằng ký tự `\n` hoặc `\r\n`:

| Lệnh | Ý nghĩa | Phản hồi mẫu |
|:-----|:--------|:-------------|
| `STATUS` | Lấy telemetry trạng thái thiết bị | `OK STATUS mode=HID fps=1 sensor=OK fw=v0.1.28 hosts=1` |
| `ENROLL <id>` | Bắt đầu quy trình lấy mẫu vân tay cho Slot `<id>` (1-5) | `OK ENROLL_START slot=1` theo sau là các bước quét |
| `VERIFY` | Thử quét và nhận diện vân tay | `OK VERIFIED slot=1` hoặc `ERR NOT_MATCH` |
| `LIST` | Liệt kê danh sách slot đã đăng ký | `OK LIST 1:Admin, 2:Work` |
| `DELETE <id>` | Xóa vân tay tại slot chỉ định | `OK DELETED slot=1` |
| `SET_MODE <HID\|PIV>` | Chuyển chế độ hoạt động | `OK MODE_CHANGED to=HID` |
| `SET_KEY_SEQ <sequence>` | Cài đặt chuỗi phím kết thúc (vd: `SUBMIT_ENTER`) | `OK KEY_SEQ_SAVED` |
| `CLEAR_KEYS` | Xóa chuỗi phím kết thúc | `OK KEYS_CLEARED` |
| `HOST LIST` | Liệt kê các máy chủ đã ghép nối | `OK HOSTS count=1` |
| `RESET FACTORY` | Khôi phục cài đặt gốc | `OK FACTORY_RESET_COMPLETE` |
