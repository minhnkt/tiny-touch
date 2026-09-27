# tinyTouch: Biometric USB Security Dongle

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-red.svg)](https://www.espressif.com/)
[![Web Serial](https://img.shields.io/badge/Web%20Serial-Supported-blue.svg)](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)

**tinyTouch** là thiết bị bảo mật sinh trắc học USB mã nguồn mở nhỏ gọn sử dụng vi điều khiển **ESP32-S3** và cảm biến vân tay UART điện dung/quang học (**SW111**, **SW101**, Grow R503, R502-A,...). Thiết bị hỗ trợ tự động điền mật khẩu qua bàn phím USB (HID Keyboard) sau khi quét vân tay chính chủ hoặc đóng vai trò thẻ thông minh bảo mật (PIV SmartCard).

> **Phần cứng thực tế khuyên dùng:** Bo mạch **ESP32-S3 SuperMini** kết hợp cảm biến vân tay tròn **SW111** (vòng LED RGB, phản hồi xúc giác siêu nhạy < 0.2s).

---

## Tính năng nổi bật

- **Chế độ kép (Dual Mode):**
  - **HID Mode (USB Keyboard):** Tự động gõ mật khẩu siêu tốc ngay khi chạm ngón tay hợp lệ (hỗ trợ màn hình khóa macOS/Windows, 1Password, Bitwarden, Terminal `sudo`).
  - **PIV Mode (SmartCard):** Giả lập thẻ thông minh chuẩn NIST PIV / Apple Native SmartCard cho xác thực chứng chỉ số X.509 phần cứng (PIN mặc định `754321`, liên kết tài khoản qua `sc_auth`).
- **Tùy biến Aura Breathing LED:** Cá nhân hóa màu sắc nhịp thở LED cảm biến vân tay riêng biệt cho từng chế độ HID (mặc định Xanh ngọc ➔ Xanh lam) và PIV (mặc định Vàng kim ➔ Đỏ hổ phách) từ bảng 7 màu RGB phần cứng.
- **Tự động xử lý Caps Lock:** Tự động phát hiện trạng thái Caps Lock của máy chủ qua USB HID Output Report và đảo ngược ký tự hoa/thường, đảm bảo không bao giờ gõ sai mật khẩu.
- **Web Serial Controller:** Giao diện web phong cách macOS Glass (hỗ trợ Light/Dark mode) kết nối trực tiếp thiết bị qua Web Serial API (Chrome/Edge), tích hợp sẵn công cụ nạp firmware Serial OTA và quản trị mã PIN PIV với SVG icon động.
- **Web Host Challenge-Response Pairing:** Cơ chế ghép nối máy chủ an toàn sử dụng Web Crypto API (HMAC SHA-256), mật khẩu được bảo vệ và chỉ nhả qua thiết bị khi có phản hồi xác thực hợp lệ từ máy chủ đã ghép nối.
- **USB Remote Wakeup:** Chạm vân tay để đánh thức máy tính từ chế độ ngủ (Sleep) và tự động chờ màn hình sáng trước khi gõ.
- **Bộ nhớ bảo mật:** Cấu hình lưu trữ trong NVS Flash mã hóa, mẫu vân tay lưu độc lập trong bộ nhớ của cảm biến.

---

## Cấu trúc dự án

Dự án được tinh gọn thành hai thành phần chính:

```
tiny-touch/
├── controller/              # Web Serial Controller UI
│   ├── index.html           # Ứng dụng Web Controller chính (Apple System Settings Glass UI)
│   └── README.md            # Hướng dẫn chi tiết Web Controller
│
├── firmware/                # Mã nguồn Firmware ESP32-S3 (ESP-IDF v5.0+)
│   ├── CMakeLists.txt       # Cấu hình build ESP-IDF
│   ├── partitions.csv       # Bảng phân vùng Flash (NVS, OTA_0, OTA_1)
│   ├── sdkconfig.defaults   # Cấu hình mặc định ESP32-S3 & TinyUSB
│   ├── ota_flash.py         # Script hỗ trợ nạp firmware OTA qua Web/Serial
│   ├── main/                # Source code C (TinyUSB HID, CDC Console, UART sensor)
│   └── README.md            # Hướng dẫn build, nạp và lệnh Console
│
├── docs/                    # Tài liệu kỹ thuật chi tiết
│   ├── BUILD_AND_FLASH.md   # Hướng dẫn biên dịch & nạp firmware (OTA & ROM Bootloader)
│   ├── HARDWARE.md          # Sơ đồ nối dây & hàn chi tiết (SW111/SW101), BOM, file in 3D
│   ├── PROTOCOL.md          # Đặc tả giao thức Serial CDC & Challenge-Response
│   ├── BAO_CAO_NGHIEP_VU.md # Báo cáo phân tích nghiệp vụ hệ thống
│   └── hardware/case/       # File thiết kế vỏ 3D (STL và STEP)
│
├── LICENSE                  # Giấy phép mã nguồn mở MIT
├── VERSION                  # Phiên bản hiện tại (v0.1.28)
└── README.md                # Tài liệu tổng quan dự án
```

---

## Sơ đồ hàn dây cảm biến SW111 & ESP32-S3 SuperMini

Cấu hình phần cứng tối ưu thực tế (cảm biến **SW111** giắc 6-pin SH1.0):

```
 [Cảm biến SW111]                              [ESP32-S3 SuperMini]
  Pin 1 (VTouch)  ----+
                      |---------------------->  Chân 3V3 (Nguồn 3.3V)
  Pin 3 (VCC)     ----+
  Pin 2 (TouchOut) -------------------------->  GPIO 2 (Ngắt chạm)
  Pin 4 (Sensor TXD) ------------------------>  GPIO 44 (UART RX)
  Pin 5 (Sensor RXD) ------------------------>  GPIO 43 (UART TX)
  Pin 6 (GND)      -------------------------->  Chân GND
```

*Xem chi tiết hướng dẫn mối hàn và sơ đồ chân Seeed XIAO tại [docs/HARDWARE.md](docs/HARDWARE.md).*

---

## Bắt đầu nhanh (Quick Start)

### 1. Sử dụng Web Controller

1. Mở trình duyệt Chrome, Edge, Brave hoặc bất kỳ trình duyệt nào hỗ trợ Web Serial API.
2. Mở file [controller/index.html](controller/index.html) hoặc trang web đã triển khai.
3. Cắm thiết bị **tinyTouch** vào cổng USB.
4. Bấm **Kết nối**, chọn cổng COM của thiết bị (`tinyTouch CDC` hoặc `usbmodem*`, baudrate `115200`).
5. Quản lý vân tay (Slot #1 - #5), chuyển chế độ HID/PIV, cài đặt chuỗi phím kết thúc và ghép nối máy chủ (Host Pairing).

### 2. Build & Nạp Firmware

Yêu cầu môi trường [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v5.0 trở lên:

```bash
# Kích hoạt ESP-IDF
source ~/esp-idf/export.sh

# Di chuyển vào thư mục firmware
cd firmware

# Chọn target ESP32-S3
idf.py set-target esp32s3

# Build firmware
idf.py build

# Nạp firmware vào thiết bị (thay cổng COM tương ứng)
idf.py -p /dev/cu.usbmodem101 flash
```

Chi tiết hướng dẫn nạp firmware (bao gồm nạp Serial OTA qua Web Controller và nạp ROM Bootloader) xem tại [docs/BUILD_AND_FLASH.md](docs/BUILD_AND_FLASH.md) và [firmware/README.md](firmware/README.md).

---

## Giấy phép (License)

Dự án được phân phối dưới giấy phép mã nguồn mở **MIT License**. Xem chi tiết tại [LICENSE](LICENSE).
