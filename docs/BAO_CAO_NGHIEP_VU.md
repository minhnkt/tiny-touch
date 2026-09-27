# BÁO CÁO NGHIÊN CỨU HỆ THỐNG TINYTOUCH WEB CONTROLLER

**Đối tượng điều khiển:** Thiết bị USB cảm biến vân tay tinyTouch (Hardware Biometrics)  
**Giao thức kết nối:** Web Serial API (CDC ACM, 115200 baud, 8-N-1)  
**Nền tảng hỗ trợ:** Chromium Desktop (Chrome, Edge, Opera, Cốc Cốc)

---

## 1. TỔNG QUAN HỆ THỐNG
Website `tinyTouch Web Controller` là một giao diện Web Serial điều khiển thiết bị phần cứng tinyTouch.
- **Bản chất phần cứng:** Thiết bị USB dongle trang bị vi điều khiển ESP32-S3 và module cảm biến vân tay UART điện dung/quang học. Mẫu sinh trắc học vân tay được lưu trữ an toàn trong chip nhớ độc lập của module cảm biến. Mật khẩu vận hành theo mô hình Challenge-Response kết hợp máy chủ đã ghép đôi: thiết bị chỉ lưu khóa ghép nối (Pairing Key) trong NVS Flash, không lưu trữ mật khẩu tĩnh dạng plaintext trên chip.
- **Cơ chế giao tiếp:**
  - **Quản trị & Cấu hình:** Trình duyệt kết nối trực tiếp với thiết bị qua cổng COM ảo (USB CDC-ACM) bằng Web Serial API mà không cần cài đặt driver hay phần mềm trung gian.
  - **Vận hành xác thực (Unlock/Login):** Thiết bị tự động đóng vai trò bàn phím USB HID Keyboard để gõ mật khẩu (sau khi nhận diện vân tay và xác thực gói tin phản hồi từ máy chủ), hoặc đóng vai trò thẻ thông minh phần cứng qua chuẩn USB CCID (ở chế độ PIV SmartCard).

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

Thiết bị tinyTouch hỗ trợ hai chế độ hoạt động riêng biệt, phục vụ các kịch bản bảo mật khác nhau:

#### 1. Chế độ HID Mode (USB Keyboard tự gõ mật khẩu qua Challenge-Response):
- **Bản chất:** Giả lập bàn phím USB tiêu chuẩn (USB HID Keyboard Class), tương thích Plug & Play trên Windows, macOS, Linux, ChromeOS.
- **Luồng hoạt động thực tế:**
  1. Khi người dùng chạm ngón tay hợp lệ, module cảm biến đối soát sinh trắc học và trả về Slot ID tương ứng (1 đến 5).
  2. **Thử thách an toàn (Challenge-Response):** Thiết bị sinh giá trị `nonce` ngẫu nhiên và gửi gói tin sự kiện `EV <nonce> <slot> <mac>` lên máy tính chủ qua cổng USB CDC ảo.
  3. **Phản hồi từ máy chủ:** Máy tính chủ (đã ghép nối với `Pairing Key` bí mật) mã hóa mật khẩu tương ứng thành gói `PW` (`PW <nonce> <iv> <ciphertext> <mac>`) gửi lại cho dongle.
  4. **Giải mã & Gõ phím:** ESP32-S3 giải mã mật khẩu trong RAM, kiểm tra trạng thái Caps Lock của máy tính (qua USB HID Output Report) để tự động đảo ký tự hoa/thường, sau đó giả lập thao tác gõ phím siêu tốc kèm chuỗi phím kết thúc (`Enter`, `Tab`,...).
  5. **Xóa sạch bộ nhớ:** Chuỗi mật khẩu và khóa phiên trong RAM của MCU được xóa sạch (`secure_wipe`) ngay sau khi gõ xong.
- **Kịch bản sử dụng:** Mở khóa màn hình máy tính, tự động điền form đăng nhập, 1Password, Bitwarden, lệnh Terminal `sudo`.

#### 2. Chế độ PIV Mode (Thẻ SmartCard chuẩn NIST PIV / Apple Native):
- **Bản chất:** Khai báo lớp thiết bị USB CCID (Chip/Smart Card Interface Devices). Hệ điều hành (macOS qua CryptoTokenKit, Windows SmartCard Logon, Linux pcscd) tự động nhận diện thiết bị như một thẻ thông minh vật lý, không cần cài driver riêng.
- **Cấu trúc mật mã bất đối xứng (Asymmetric Crypto):** Lưu trữ 2 cặp khóa RSA 2048-bit và chứng chỉ số X.509:
  - **Slot 9A (Authentication):** Dùng để xác thực người dùng khi đăng nhập hệ thống, mở khóa máy, xác thực lệnh `sudo`, SSH qua PKCS#11.
  - **Slot 9D (Key Management):** Dùng để trao đổi khóa và giải mã dữ liệu bảo mật (như macOS Login Keychain).
- **Luồng hoạt động thực tế (Không truyền mật khẩu qua đường truyền):**
  1. Khi người dùng thực hiện hành động cần xác thực (login, sudo), hệ điều hành gửi gói lệnh APDU chuẩn (`GENERAL AUTHENTICATE`) chứa dữ liệu thử thách ngẫu nhiên tới dongle qua giao thức USB CCID.
  2. **Xác nhận hiện diện sinh trắc học (User Presence):** Thiết bị yêu cầu người dùng chạm ngón tay hợp lệ vào cảm biến để cấp quyền ký số cho Slot 9A/9D.
  3. **Ký số nội bộ trên chip (On-chip RSA Signing):** Khi vân tay hợp lệ, thư viện mbedTLS trên ESP32-S3 trực tiếp thực hiện phép ký số riêng (`mbedtls_rsa_private`) với Private Key nội bộ. Private Key được bảo vệ tuyệt đối trên chip, không bao giờ bị trích xuất hay truyền ra ngoài.
  4. **Mở khóa thành công:** Chữ ký số RSA được trả về cho hệ điều hành. Hệ điều hành dùng Public Key đối soát và cấp quyền truy cập mà hoàn toàn không cần nhập chuỗi mật khẩu tĩnh nào.
- **Kịch bản sử dụng:** Môi trường doanh nghiệp, quản trị viên hệ thống, đăng nhập macOS không mật khẩu, bảo mật khóa SSH.

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
- Mỗi vị trí tương ứng với 1 mẫu vân tay lưu trên cảm biến và 1 tài khoản/mật khẩu bảo mật qua cơ chế ghép nối máy chủ (Host Pairing).
- Thao tác: Đăng ký vân tay mới, cập nhật mật khẩu máy chủ, xóa slot.

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
| `WRITE_KEY` | `<SLOT>:<HASH>` | `OK WRITE_KEY` | Lệnh ghi mật khẩu legacy (Thực tế bảo mật qua `HOST ADD` và cơ chế Challenge-Response `EV`/`PW`) |
| `HOST ADD` | `<ID> <KEY>` | `OK HOST ADD` | Đăng ký khóa ghép nối an toàn với máy tính chủ (Host Pairing) |
| `HOST LIST` | *(Trống)* | `OK HOST LIST ids=...` | Liệt kê danh sách ID máy chủ đã ghép nối |
| `ENTER_BOOTLOADER` | *(Trống)* | `OK:BOOTLOADER_READY:<ID>` | Khởi động lại MCU vào chế độ DFU bootloader |
| `PROBE_CHIP` | *(Trống)* | `OK:CHIP_ID:<ARCH>\|FLASH:<SZ>` | Kiểm tra ID chip nhớ trước khi nạp FW |
| `FLASH_BLOCK` | Địa chỉ phân vùng hex | `OK:BLOCK_ACK:<ADDR>` | Nạp từng khối nhị phân firmware |
| `FLASH_DONE` | *(Trống)* | `OK:FIRMWARE_VERIFIED_AND_REBOOTED:<VER>` | Xác thực chữ ký firmware và reboot thiết bị |
| `FACTORY_RESET` | *(Trống)* | `OK:DEVICE_RESTORED_TO_FACTORY_DEFAULTS` | Xóa sạch bộ nhớ và đưa về cấu hình gốc |

