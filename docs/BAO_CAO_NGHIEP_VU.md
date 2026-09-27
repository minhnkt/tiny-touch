# BÁO CÁO NGHIÊN CỨU HỆ THỐNG TINYTOUCH WEB CONTROLLER

**Đối tượng điều khiển:** Thiết bị USB cảm biến vân tay tinyTouch (Hardware Biometrics)  
**Giao thức kết nối:** Web Serial API (CDC ACM, 115200 baud, 8-N-1)  
**Nền tảng hỗ trợ:** Chromium Desktop (Chrome, Edge, Opera, Cốc Cốc)

---

## 1. TỔNG QUAN HỆ THỐNG
Website `tinyTouch Web Controller` là một giao diện Web Serial điều khiển thiết bị phần cứng tinyTouch.
- **Bản chất phần cứng:** Thiết bị USB dongle trang bị vi điều khiển và cảm biến vân tay điện dung, lưu trữ vân tay và mật khẩu mã hóa trực tiếp trong chip nhớ.
- **Cơ chế giao tiếp:** Trình duyệt kết nối trực tiếp với thiết bị qua cổng COM ảo bằng Web Serial API mà không cần cài đặt phần mềm trung gian.

---

## 2. NGHIỆP VỤ & CÁC TÍNH NĂNG CHÍNH

### 2.1. Quản lý Kết nối Web Serial
- Kiểm tra hỗ trợ `navigator.serial` trên trình duyệt.
- Kết nối cổng COM với tham số `baudRate: 115200`.
- Hiển thị telemetry phần cứng:
  - Chế độ hiện tại (HID hoặc PIV).
  - Số vân tay đã nạp (0/5 đến 5/5).
  - Trạng thái hoạt động của cảm biến vân tay.
  - Phiên bản Firmware (`v1.0.2`, khuyến nghị cập nhật `v1.0.5`).

### 2.2. Chuyển đổi Chế độ Hoạt động (Operation Modes)
1. **HID Mode (Keyboard tự động gõ - Mặc định):**
   - Hoạt động như bàn phím USB tiêu chuẩn (Plug & Play trên Windows, macOS, Linux).
   - Khi quét ngón tay hợp lệ, thiết bị tự động gõ chuỗi mật khẩu tương ứng vào ô nhập liệu (Màn hình đăng nhập, Keychain, 1Password, Terminal `sudo`).
   - Hỗ trợ tối đa 5 ngón tay tương ứng 5 mật khẩu riêng biệt.
2. **PIV Mode (Thẻ SmartCard chuẩn Apple):**
   - Thiết bị nhận diện như SmartCard (chuẩn bảo mật doanh nghiệp/Apple).
   - Xác thực qua chứng chỉ số X.509 phần cứng cho đăng nhập macOS và lệnh `sudo`.

### 2.3. Cấu hình Chuỗi Phím Kết Thúc (HID Key Sequence)
Cấu hình chuỗi phím tự động bấm sau khi điền xong mật khẩu:
- **Preset:**
  - `Enter` (Mặc định).
  - `Enter ➔ Space`.
  - `Tab ➔ Enter` (dành cho form có ô 2FA/OTP kế tiếp).
  - `Enter x2` (xác nhận kép).
  - `Không bấm` (chỉ điền mật khẩu).
  - `Tự chọn chuỗi`: Ghép 1 đến 3 phím tự do (`Enter`, `Space`, `Tab`, `Esc`, `Arrow Down`, `Arrow Up`, `Không dùng`).
- **Keystroke Delay:** Khoảng nghỉ giữa các phím (0ms đến 1000ms, mặc định 100ms).

### 2.4. Cấu hình Đánh thức Máy (USB Remote Wakeup)
- Khi máy tính đang ở trạng thái Sleep, chạm vân tay gửi tín hiệu đánh thức USB Resume.
- **Wakeup Delay:** Thời gian chờ màn hình sáng và ô mật khẩu kích hoạt trước khi bắt đầu gõ mật khẩu (0ms, 500ms, 1000ms chuẩn, 1500ms màn hình ngoài, 2000ms).

### 2.5. Quản lý 5 Ngón Tay (Đa tài khoản)
- Quản lý 5 vị trí vân tay (Slot 1 đến 5).
- Mỗi vị trí gán 1 nhãn tài khoản và 1 mật khẩu lưu mã hóa trong chip.
- Thao tác: Đăng ký vân tay mới, cập nhật mật khẩu, xóa slot.

### 2.6. Nhật ký Serial (Diagnostic Console)
- Màn hình terminal hiển thị luồng dữ liệu TX/RX giữa máy tính và thiết bị qua baudrate 115200.
- Nút xóa log, tự động cuộn theo thời gian thực.

### 2.7. Các Hộp Thoại Tương Tác (Modals)
1. **Yêu cầu chạm vân tay:** Xác thực chủ sở hữu trước khi cho phép thay đổi cấu hình bảo mật.
2. **Cài đặt mật khẩu:** Hộp thoại nhập mật khẩu bảo vệ cho từng ngón tay.
3. **Trình nạp Firmware (Web Flasher):** Cập nhật firmware trực tiếp qua Bootloader Web Serial với thanh tiến trình và thông báo bảo toàn dữ liệu.

---

## 3. CẤU TRÚC BẢN CLONE LOCAL
- `index.html`: Chuyển hướng tự động vào `controller/index.html`.
- `controller/index.html`: Bản cài đặt hoàn chỉnh chạy offline/local:
  - Tích hợp Tailwind CSS (CDN/Local styling), giao diện Dark Theme cao cấp đồng nhất với bản gốc.
  - Hỗ trợ Web Serial thật khi cắm phần cứng tinyTouch.
  - Tích hợp **Hardware Simulator Mode (Giả lập phần cứng)**: Cho phép trải nghiệm toàn bộ tính năng kết nối, chuyển chế độ, đổi chuỗi phím, quản lý 5 ngón tay và nạp firmware kể cả khi chưa cắm thiết bị thật.

---

## 4. ĐẶC TẢ GIAO THỨC SERIAL (SERIAL COMMAND PROTOCOL)

Baudrate: `115200`, Data Bits: `8`, Parity: `None`, Stop Bits: `1`.

| Mã lệnh (TX) | Dữ liệu / Tham số | Phản hồi từ thiết bị (RX) | Mục đích nghiệp vụ |
|---|---|---|---|
| `GET_INFO` | *(Trống)* | `OK:INFO:tinyTouch\|FW=...\|MODE=...` | Đọc telemetry phần cứng khi vừa kết nối |
| `SET_MODE` | `HID` hoặc `PIV` | `OK:MODE_UPDATED:<MODE>` | Đổi chế độ hoạt động USB Keyboard / SmartCard |
| `SET_KEY_SEQ` | Danh sách phím cách bởi dấu phẩy | `OK:KEY_SEQ_STORED:[<KEYS>]` | Cấu hình macro chuỗi phím sau mật khẩu |
| `SET_KEY_DELAY` | Số nguyên ms (0-1000) | `OK:KEY_DELAY_STORED:<N>ms` | Lưu độ trễ gõ giữa các ký tự |
| `SET_WAKEUP_DELAY` | Số nguyên ms (0-3000) | `OK:WAKEUP_DELAY_STORED:<N>ms` | Lưu thời gian chờ đánh thức USB Resume |
| `ENROLL_FINGER` | Slot ID (1-5) | `OK:ENROLL_SUCCESS:SLOT_<N>` | Quét nạp mẫu vân tay mới vào vị trí slot |
| `DELETE_FINGER` | Slot ID (1-5) | `OK:DELETE_SUCCESS:SLOT_<N>` | Xóa vân tay và mật khẩu tương ứng của slot |
| `WRITE_KEY` | `<SLOT>:<HASH>` | `OK:KEY_STORED_SECURELY:SLOT_<N>` | Ghi mật khẩu mã hóa vào flash bảo mật |
| `ENTER_BOOTLOADER` | *(Trống)* | `OK:BOOTLOADER_READY:<ID>` | Khởi động lại MCU vào chế độ DFU bootloader |
| `PROBE_CHIP` | *(Trống)* | `OK:CHIP_ID:<ARCH>\|FLASH:<SZ>` | Kiểm tra ID chip nhớ trước khi nạp FW |
| `FLASH_BLOCK` | Địa chỉ phân vùng hex | `OK:BLOCK_ACK:<ADDR>` | Nạp từng khối nhị phân firmware |
| `FLASH_DONE` | *(Trống)* | `OK:FIRMWARE_VERIFIED_AND_REBOOTED:<VER>` | Xác thực chữ ký firmware và reboot thiết bị |
| `FACTORY_RESET` | *(Trống)* | `OK:DEVICE_RESTORED_TO_FACTORY_DEFAULTS` | Xóa sạch bộ nhớ và đưa về cấu hình gốc |

