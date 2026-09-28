# BÁO CÁO NGHIÊN CỨU HỆ THỐNG TINYTOUCH WEB CONTROLLER

**Đối tượng điều khiển:** Thiết bị USB/BLE cảm biến vân tay tinyTouch (Hardware Biometrics)  
**Giao thức kết nối:** Web Serial API (CDC ACM, 115200 baud, 8-N-1) & Web Bluetooth API (Nordic UART Service - NUS)  
**Nền tảng hỗ trợ:** Chromium Desktop (Chrome, Edge, Opera, Cốc Cốc)

---

## 1. TỔNG QUAN HỆ THỐNG
Website `tinyTouch Web Controller` là một giao diện Web điều khiển thiết bị phần cứng sinh trắc học tinyTouch qua cả 2 phương thức có dây (Web Serial) và không dây (Web Bluetooth).
- **Bản chất phần cứng:** Thiết bị hỗ trợ Dual-Mode trang bị vi điều khiển ESP32-S3, module cảm biến vân tay UART điện dung/quang học và tùy chọn pin LiPo 370 mAh. Mẫu sinh trắc học vân tay được lưu trữ an toàn trong chip nhớ độc lập của module cảm biến. Mật khẩu vận hành theo mô hình Challenge-Response kết hợp máy chủ đã ghép đôi: thiết bị chỉ lưu khóa ghép nối (Pairing Key) trong NVS Flash, không lưu trữ mật khẩu tĩnh dạng plaintext trên chip.
- **Cơ chế giao tiếp:**
  - **Quản trị & Cấu hình:** Trình duyệt kết nối trực tiếp với thiết bị qua cổng COM ảo (USB CDC-ACM) bằng Web Serial API hoặc qua sóng Bluetooth Low Energy bằng Web Bluetooth API (chuẩn Nordic UART Service - NUS) mà không cần cài đặt driver hay ứng dụng nền bên thứ ba.
  - **Vận hành xác thực (Unlock/Login):** Thiết bị tự động đóng vai trò bàn phím ảo (USB HID Keyboard khi cắm cáp Type-C, hoặc BLE HOGP Keyboard khi chạy nguồn pin) để gõ mật khẩu sau khi nhận diện vân tay và xác thực gói tin phản hồi từ máy chủ. Ở chế độ PIV SmartCard, thiết bị đóng vai trò thẻ thông minh phần cứng qua chuẩn USB CCID (chỉ hoạt động qua cáp USB có dây).

---

## 2. NGHIỆP VỤ & CÁC TÍNH NĂNG CHÍNH

### 2.1. Quản lý Kết nối Web Serial & Web Bluetooth
- Hỗ trợ kết nối song song qua Web Serial (`navigator.serial`) và Web Bluetooth (`navigator.bluetooth`).
- **Web Serial (USB CDC):** Tốc độ 115200 baud, hỗ trợ đầy đủ mọi chức năng quản trị, bao gồm cả nạp firmware Serial OTA.
- **Web Bluetooth (BLE NUS):** Kết nối không dây đến thiết bị `tinyTouch Key`, theo dõi mức pin thời gian thực qua Battery Service (`0x180F`), cấu hình màu LED Aura và quản lý các Slot vân tay từ xa.
- **An toàn Zero-Trust trên kênh không dây:** Các lệnh nguy hiểm như nạp firmware (`OTA`), khôi phục cài đặt gốc (`RESET FACTORY`) và tạo khóa thẻ PIV (`PIV CREATE`) bị cấm tuyệt đối trên Web Bluetooth (trả về lỗi `ERR DISALLOWED_ON_BLE`).
- Hiển thị telemetry phần cứng theo thời gian thực (Protocol v7):
  - Chế độ hiện tại (`HID` hoặc `PIV`).
  - Mức pin hiện tại (% và huy hiệu trạng thái pin khi kết nối BLE).
  - Số vân tay đã nạp (0/5 đến 5/5).
  - Trạng thái hoạt động của cảm biến vân tay (`ready` hoặc `offline`).
  - Trạng thái thẻ PIV (`ready` hoặc `unconfigured`).
  - Phiên bản Firmware (`v0.1.28`), mã build và thời điểm biên dịch tự động.
  - Cặp màu thở LED Aura hiện hành (`led_hid` và `led_piv`).
  - Trạng thái tiến trình OTA (`idle`, `writing`, hoặc `staged`).

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
- **Mã PIN mặc định:** **`754321`** (6 chữ số). Hỗ trợ ghép đôi tài khoản macOS trực tiếp qua lệnh hệ thống `sudo sc_auth pair -u $(whoami) -h <HASH>` và mở khóa tức thì qua xác nhận sinh trắc học vân tay.
- **Cấu trúc mật mã bất đối xứng (Asymmetric Crypto):** Lưu trữ 2 cặp khóa RSA 2048-bit và chứng chỉ số X.509:
  - **Slot 9A (Authentication):** Dùng để xác thực người dùng khi đăng nhập hệ thống, mở khóa máy, xác thực lệnh `sudo`, SSH qua PKCS#11.
  - **Slot 9D (Key Management):** Dùng để trao đổi khóa và giải mã dữ liệu bảo mật (như macOS Login Keychain).
- **Luồng hoạt động thực tế (Không truyền mật khẩu qua đường truyền):**
  1. Khi người dùng thực hiện hành động cần xác thực (login, sudo), hệ điều hành gửi gói lệnh APDU chuẩn (`GENERAL AUTHENTICATE`) chứa dữ liệu thử thách ngẫu nhiên tới dongle qua giao thức USB CCID.
  2. **Xác nhận hiện diện sinh trắc học (User Presence):** Thiết bị yêu cầu người dùng chạm ngón tay hợp lệ vào cảm biến để cấp quyền ký số cho Slot 9A/9D.
  3. **Ký số nội bộ trên chip (On-chip RSA Signing):** Khi vân tay hợp lệ, thư viện mbedTLS trên ESP32-S3 trực tiếp thực hiện phép ký số riêng (`mbedtls_rsa_private`) với Private Key nội bộ. Private Key được bảo vệ tuyệt đối trên chip, không bao giờ bị trích xuất hay truyền ra ngoài.
  4. **Mở khóa thành công:** Chữ ký số RSA được trả về cho hệ điều hành. Hệ điều hành dùng Public Key đối soát và cấp quyền truy cập mà hoàn toàn không cần nhập chuỗi mật khẩu tĩnh nào.
- **Kịch bản sử dụng:** Môi trường doanh nghiệp, quản trị viên hệ thống, đăng nhập macOS không mật khẩu, bảo mật khóa SSH.

### 2.3. Tùy biến Màu Thở LED Cảm Biến (Aura Breathing LED)
Cấu hình hiệu ứng ánh sáng nhịp thở cho vòng đèn LED RGB của cảm biến vân tay:
- **Tách biệt theo chế độ:**
  - **HID Mode:** Mặc định Cyan ➔ Blue (mã `3 1`).
  - **PIV Mode:** Mặc định Yellow ➔ Red (mã `6 4`).
- **Bảng 7 màu phần cứng hỗ trợ:** `1`: Blue (Xanh lam), `2`: Green (Xanh lục), `3`: Cyan (Xanh ngọc), `4`: Red (Đỏ), `5`: Purple (Tím), `6`: Yellow (Vàng kim), `7`: White (Trắng).
- **Lưu trữ & Phản hồi:** Cấu hình lưu trữ trong Flash NVS (`CONFIG_VERSION = 7`), cập nhật tức thì trên cảm biến qua lệnh `SET LED_HID` / `SET LED_PIV`. Trên Web Controller, người dùng có thể chọn bảng màu trực quan với chấm chỉ báo (dot indicator) và badge hiển thị động.

### 2.4. Cấu hình Chuỗi Phím Kết Thúc (HID Key Sequence)
Cấu hình chuỗi phím tự động bấm sau khi điền xong mật khẩu:
- **Preset:**
  - `Enter` (Mặc định).
  - `Enter ➔ Space`.
  - `Tab ➔ Enter` (dành cho form có ô 2FA/OTP kế tiếp).
  - `Enter x2` (xác nhận kép).
  - `Không bấm` (chỉ điền mật khẩu).
  - `Tự chọn chuỗi`: Ghép 1 đến 3 phím tự do (`Enter`, `Space`, `Tab`, `Esc`, `Arrow Down`, `Arrow Up`, `Không dùng`).
- **Keystroke Delay:** Khoảng nghỉ giữa các phím (0ms đến 1000ms, mặc định 25ms, tùy chỉnh qua `SET TYPE_DELAY`).

### 2.5. Cấu hình Đánh thức Máy (USB Remote Wakeup)
- Khi máy tính đang ở trạng thái Sleep, chạm vân tay gửi tín hiệu đánh thức USB Resume.
- **Wakeup Delay:** Thời gian chờ màn hình sáng và ô mật khẩu kích hoạt trước khi bắt đầu gõ mật khẩu (0ms, 500ms, 1000ms chuẩn, 1500ms màn hình ngoài, 2000ms).

### 2.6. Quản lý 5 Ngón Tay (Đa tài khoản)
- Quản lý 5 vị trí vân tay (Slot 1 đến 5).
- Mỗi vị trí tương ứng với 1 mẫu vân tay lưu trên cảm biến và 1 tài khoản/mật khẩu bảo mật qua cơ chế ghép nối máy chủ (Host Pairing).
- Thao tác: Đăng ký vân tay mới (`FINGER ENROLL`), xóa slot (`FINGER DELETE`), xóa sạch (`FINGER CLEAR`).

### 2.7. Nhật ký Serial (Diagnostic Console)
- Màn hình terminal hiển thị luồng dữ liệu TX/RX giữa máy tính và thiết bị qua baudrate 115200.
- Nút xóa log, tự động cuộn theo thời gian thực.

### 2.8. Trình nạp Firmware Serial OTA (Web Serial Flasher)
- Cho phép cập nhật firmware nhị phân `tiny_touch_unified.bin` trực tiếp qua giao thức Serial OTA mà không cần đưa MCU vào chế độ DFU Bootloader hay tháo vỏ thiết bị.
- **Cơ chế an toàn:**
  1. Xác thực quyền chủ sở hữu qua sinh trắc học vân tay (`AUTH`).
  2. Chia nhỏ file nhị phân thành các chunk Base64 3072 bytes ghi tuần tự vào Flash (`OTA BEGIN`, `OTA WRITE`).
  3. Thiết bị tự động tính toán và đối soát mã băm SHA-256 của toàn bộ ảnh firmware trước khi kích hoạt chuyển đổi phân vùng khởi động (`OTA COMMIT`).
  4. Bảo toàn nguyên vẹn dữ liệu NVS (cấu hình, host pairing) và mẫu vân tay trong cảm biến.

---

## 3. KIẾN TRÚC & PHÂN BỔ ỨNG DỤNG WEB CONTROLLER
Ứng dụng Web Controller được thiết kế theo tiêu chí Zero Build Tools, vận hành độc lập và giao tiếp trực tiếp 100% với phần cứng:
- `index.html`: Điểm điều hướng chính, tự động chuyển tiếp người dùng vào `controller/index.html`.
- `controller/index.html`: Ứng dụng Web Controller hoàn chỉnh (Pure HTML5 / Modern Vanilla JS / CSS3):
  - **Apple Glass UI:** Giao diện phong cách Apple System Settings (macOS Tahoe/Sequoia) với hiệu ứng kính mờ (frosted glass), tương thích đầy đủ Light Mode và Dark Mode.
  - **Tương tác phần cứng thực tế qua Web Serial API:** Kết nối trực tiếp cổng USB CDC-ACM của ESP32-S3 ở tốc độ `115200` baud (8-N-1). Hệ thống không dùng giả lập (Simulator), mọi lệnh và phản hồi đều gắn liền với trạng thái phần cứng thực tế.
  - **Tab Chế độ hoạt động (Mode) & Aura LED Badge:** Chuyển đổi linh hoạt giữa HID Keyboard và PIV SmartCard, hiển thị badge màu LED nhịp thở động tương ứng với cấu hình phần cứng.
  - **Bộ chọn Aura Breathing Palette:** Lựa chọn bảng màu nhịp thở LED cảm biến vân tay từ 7 màu phần cứng với giao diện dot indicator tối giản, đồng bộ trực tiếp vào NVS Flash của chip.
  - **Quản lý PIV SmartCard & PIN:** Hiển thị mã PIN mặc định `754321`, nút ẩn/hiện và sao chép mã PIN với biểu tượng SVG động, tích hợp sẵn câu lệnh ghép đôi macOS `sc_auth pair`.
  - **Tích hợp Web Serial OTA Flasher:** Nạp trực tiếp file nhị phân `tiny_touch_unified.bin` qua giao thức Serial OTA an toàn với xác thực sinh trắc học vân tay và kiểm tra hash SHA-256 tự động.
  - **Tự động gắn nhãn Build Timestamp:** Hiển thị thời gian build và phiên bản firmware đồng bộ theo thời gian thực.
  - **Tích hợp Web Crypto API:** Quản lý khóa ghép nối bảo mật (Host Pairing) bằng mã băm HMAC-SHA256 trong `localStorage`, tự động phản hồi gói tin thử thách của thiết bị để giải mã và gõ mật khẩu an toàn.
  - **Serial Terminal Drawer:** Cửa sổ giám sát console tích hợp cho phép quan sát trực tiếp luồng dữ liệu TX/RX thời gian thực.
  - **Tính di động cao (Portability):** Có thể triển khai trên bất kỳ nền tảng tĩnh nào (GitHub Pages, Cloudflare Pages, máy chủ web nội bộ) hoặc mở trực tiếp file trên trình duyệt Chromium mà không cần backend hay driver cài đặt.

---

## 4. ĐẶC TẢ GIAO THỨC SERIAL (SERIAL COMMAND PROTOCOL)

Baudrate: `115200`, Data Bits: `8`, Parity: `None`, Stop Bits: `1`.

| Mã lệnh (TX) | Dữ liệu / Tham số | Phản hồi từ thiết bị (RX) | Mục đích nghiệp vụ |
|---|---|---|---|
| `STATUS` | *(Trống)* | `OK STATUS protocol=7 firmware=... mode=... piv=... sensor=... fingerprints=... hosts=... enter=... delay=... led_hid=... led_piv=... ota=...` | Đọc toàn bộ telemetry hệ thống và thông số phần cứng |
| `AUTH` | *(Trống)* | `OK AUTH` (yêu cầu chạm vân tay) | Mở phiên ủy quyền 15 giây trước khi thực thi lệnh nhạy cảm |
| `SET MODE <MODE>` | `HID` hoặc `PIV` | `OK SET MODE` | Chuyển chế độ hoạt động USB Keyboard hoặc SmartCard |
| `SET LED_HID <s > <e>` | Cặp mã màu 1-7 | `OK SET` | Cấu hình màu thở LED cho chế độ HID (mặc định 3 1) |
| `SET LED_PIV <s > <e>` | Cặp mã màu 1-7 | `OK SET` | Cấu hình màu thở LED cho chế độ PIV (mặc định 6 4) |
| `SET TYPE_DELAY <ms>` | Số nguyên ms (0-1000) | `OK SET` | Cài đặt độ trễ gõ giữa các ký tự (mặc định 25ms) |
| `SET SUBMIT_ENTER <0\|1>`| `1` (bật) hoặc `0` (tắt) | `OK SET` | Tự động nhấn Enter sau chuỗi mật khẩu |
| `SET COOLDOWN <ms>` | Số nguyên ms | `OK SET` | Đặt thời gian nghỉ chống quét lặp lại sau mỗi lần chạm |
| `FINGER ENROLL <id>` | Slot ID (1-5) | `OK FINGER` (kèm `EVT ENROLL_STEP`) | Bắt đầu quy trình quét mẫu vân tay mới vào slot |
| `FINGER DELETE <id>` | Slot ID (1-5) | `OK FINGER` | Xóa dữ liệu mẫu vân tay tại slot chỉ định |
| `FINGER CLEAR` | *(Trống)* | `OK FINGER` | Xóa toàn bộ các mẫu vân tay trong cảm biến |
| `HOST ADD <id> <key>` | ID hex 16B & Key hex 32B | `OK HOST ADD` | Đăng ký máy chủ ghép nối bảo mật (Host Pairing) |
| `HOST REMOVE <id>` | ID hex 16B | `OK HOST REMOVE` | Xóa máy chủ khỏi danh sách ghép nối |
| `HOST LIST` | *(Trống)* | `OK HOST LIST ids=... capacity=8` | Liệt kê danh sách ID các máy chủ đã ghép nối |
| `PIV CREATE` | *(Trống)* | `EVENT PIV_CREATE` ➔ `OK PIV CREATE` | Sinh lại cặp khóa RSA và chứng chỉ PIV X.509 mới |
| `USB RECONNECT` | *(Trống)* | `OK USB RECONNECT` | Kích hoạt quét lại USB CCID SmartCard trên máy chủ |
| `RESET FACTORY` | *(Trống)* | `OK RESET FACTORY` | Khôi phục cài đặt gốc, xóa sạch NVS và vân tay |
| `OTA BEGIN <tok> <sz> <hash>` | Token, Size, SHA256 | `OK OTA BEGIN next=0` | Khởi tạo phiên nạp firmware OTA |
| `OTA WRITE <tok> <off> <b64>` | Token, Offset, Base64 | `OK OTA WRITE next=...` | Ghi từng khối nhị phân firmware vào Flash |
| `OTA COMMIT <tok>` | Token | `OK OTA COMMIT` | Kiểm tra SHA-256 và kích hoạt firmware mới |
| `OTA ABORT [tok]` | Token (tùy chọn) | `OK OTA ABORT` | Hủy bỏ phiên nạp OTA |

