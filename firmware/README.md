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

Hệ thống hỗ trợ 2 phương pháp nạp:
1. **Serial OTA qua Web Controller / Python script (Khuyên dùng):** Nạp trực tiếp qua cổng CDC không cần tháo vỏ thiết bị hay giữ nút vật lý. Xem hướng dẫn chi tiết tại [docs/BUILD_AND_FLASH.md](../docs/BUILD_AND_FLASH.md).
2. **ROM Bootloader qua `idf.py flash` (Cứu hộ):** Dùng khi nạp mạch trắng hoặc khôi phục thiết bị:

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

Giao thức truyền dòng văn bản kết thúc bằng ký tự `\n` hoặc `\r\n`. Đối với các lệnh thay đổi cấu hình hoặc nạp firmware, thiết bị yêu cầu mở phiên ủy quyền qua lệnh `AUTH` kèm chạm vân tay xác thực (hiệu lực 15 giây):

| Lệnh | Ý nghĩa | Phản hồi mẫu |
|:-----|:--------|:-------------|
| `STATUS` | Lấy telemetry trạng thái thiết bị (Protocol v7) | `OK STATUS protocol=7 firmware=0.1.28 build=... mode=HID piv=ready sensor=ready fingerprints=2 hosts=1 enter=1 delay=25 led_hid=3,1 led_piv=6,4 ota=idle` |
| `AUTH` | Mở phiên ủy quyền quản trị 15s (chạm vân tay xác thực) | `OK AUTH` |
| `SET MODE <HID\|PIV>` | Chuyển chế độ hoạt động chính | `OK SET MODE` |
| `SET LED_HID <start> <end>` | Đặt cặp màu thở LED cho chế độ HID (1-7) | `OK SET` |
| `SET LED_PIV <start> <end>` | Đặt cặp màu thở LED cho chế độ PIV (1-7) | `OK SET` |
| `SET TYPE_DELAY <ms>` | Cài đặt độ trễ gõ giữa các ký tự (ms) | `OK SET` |
| `SET SUBMIT_ENTER <0\|1>` | Bật (1) / Tắt (0) tự động nhấn Enter sau khi gõ mật khẩu | `OK SET` |
| `SET COOLDOWN <ms>` | Đặt thời gian nghỉ chống quét lặp lại (ms) | `OK SET` |
| `FINGER ENROLL <id>` | Bắt đầu quy trình lấy mẫu vân tay cho Slot 1-5 | `OK FINGER` (kèm các sự kiện `EVT ENROLL_STEP`) |
| `FINGER DELETE <id>` | Xóa vân tay tại slot chỉ định khỏi cảm biến | `OK FINGER` |
| `FINGER CLEAR` | Xóa toàn bộ vân tay trong bộ nhớ cảm biến | `OK FINGER` |
| `HOST ADD <id> <secret>` | Đăng ký máy chủ ghép nối an toàn (Challenge-Response) | `OK HOST ADD` |
| `HOST REMOVE <id>` | Xóa máy chủ khỏi danh sách ghép nối | `OK HOST REMOVE` |
| `HOST LIST` | Liệt kê danh sách ID máy chủ đã ghép nối | `OK HOST LIST ids=... capacity=8` |
| `PIV CREATE` | Sinh lại cặp khóa RSA và chứng chỉ PIV X.509 mới | `OK PIV CREATE` |
| `USB RECONNECT` | Kích hoạt quét lại USB CCID SmartCard trên máy chủ | `OK USB RECONNECT` |
| `RESET FACTORY` | Khôi phục cài đặt gốc, xóa sạch NVS và vân tay | `OK RESET FACTORY` |
| `OTA BEGIN <tok> <sz> <hash>` | Bắt đầu phiên nạp firmware OTA | `OK OTA BEGIN next=0` |
| `OTA WRITE <tok> <off> <b64>` | Ghi khối nhị phân firmware Base64 | `OK OTA WRITE next=...` |
| `OTA COMMIT <tok>` | Xác thực hash SHA-256 và kích hoạt firmware mới | `OK OTA COMMIT` |
| `OTA ABORT [tok]` | Hủy bỏ phiên nạp OTA | `OK OTA ABORT` |
