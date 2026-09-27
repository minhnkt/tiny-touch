# tinyTouch Serial Protocol Specification

Tài liệu đặc tả chi tiết giao thức truyền thông qua cổng nối tiếp USB CDC giữa máy chủ (Web Controller / Ứng dụng nền) và thiết bị phần cứng **tinyTouch**.

---

## 1. Tầng vật lý & Kết nối (Physical Layer)

- **Giao diện:** USB CDC-ACM (Cổng COM ảo Native USB của ESP32-S3)
- **Tốc độ truyền (Baud Rate):** `115200` bps
- **Khung truyền (Data Frame):** `8-N-1` (8 data bits, no parity bit, 1 stop bit)
- **Quy cách dòng lệnh:** Định dạng văn bản ASCII UTF-8, mỗi dòng kết thúc bằng `\r\n` hoặc `\n`.

---

## 2. Quy ước thông điệp (Message Format)

### Máy chủ gửi tới thiết bị (Host ➔ Device)
```
<COMMAND> [PARAM1] [PARAM2] ... \n
```

### Thiết bị phản hồi máy chủ (Device ➔ Host)
- Phản hồi thành công:
  ```
  OK <ACTION> [key=value] [key=value] ...
  ```
- Phản hồi lỗi:
  ```
  ERR <REASON> [detail]
  ```
- Sự kiện / Luồng thông báo chủ động (Telemetry / Notification):
  ```
  EVT <EVENT_TYPE> [key=value] ...
  ```

---

## 3. Danh mục tập lệnh chi tiết

### 3.1. Truy vấn trạng thái & Cấu hình hệ thống

#### `STATUS`
Lấy toàn bộ thông số hoạt động hiện hành của thiết bị.
- **Phản hồi:**
  ```text
  OK STATUS mode=<HID|PIV> fps=<count> sensor=<OK|ERR> fw=<version> hosts=<count>
  ```
  - `mode`: Chế độ hiện tại (`HID` hoặc `PIV`).
  - `fps`: Số lượng ngón tay đã đăng ký trong bộ nhớ (0 đến 5).
  - `sensor`: Trạng thái kết nối với cảm biến UART (`OK` hoặc `ERR`).
  - `fw`: Phiên bản firmware hiện hành (vd: `v0.1.28`).
  - `hosts`: Số lượng máy chủ đã ghép nối an toàn (Host Pairing).

#### `SET_MODE <HID|PIV>`
Thay đổi chế độ hoạt động chính.
- `SET_MODE HID`: Chuyển sang chế độ giả lập bàn phím tự gõ mật khẩu.
- `SET_MODE PIV`: Chuyển sang chế độ SmartCard X.509.
- **Phản hồi:** `OK MODE_CHANGED to=<MODE>`

#### `SET_KEY_SEQ <sequence>`
Cài đặt chuỗi phím bấm tự động sau khi gõ xong chuỗi mật khẩu.
- Các preset thông dụng:
  - `SUBMIT_ENTER`: Chỉ bấm phím `Enter`.
  - `SUBMIT_ENTER_SPACE`: Bấm `Enter` sau đó bấm `Space`.
  - `SUBMIT_TAB_ENTER`: Bấm `Tab` sau đó bấm `Enter` (dành cho form có ô mã OTP).
  - `SUBMIT_ENTER_ENTER`: Bấm `Enter` 2 lần.
  - `SUBMIT_NONE`: Không nhấn bất kỳ phím nào sau khi gõ.
- **Phản hồi:** `OK KEY_SEQ_SAVED`

#### `CLEAR_KEYS`
Xóa toàn bộ cấu hình phím kết thúc về mặc định.
- **Phản hồi:** `OK KEYS_CLEARED`

#### `RESET FACTORY`
Khôi phục cài đặt gốc, xóa toàn bộ NVS và danh sách Host Pairing (lưu ý: không tự xóa mẫu vân tay nếu không có lệnh xóa cảm biến).
- **Phản hồi:** `OK FACTORY_RESET_COMPLETE`

---

### 3.2. Quản lý sinh trắc học vân tay

#### `ENROLL <slot_id>`
Bắt đầu quy trình lấy mẫu vân tay cho Slot từ `1` đến `5`.
- **Luồng sự kiện từng bước:**
  1. `OK ENROLL_START slot=<id>`
  2. `EVT ENROLL_STEP step=1/3 status=PLACE_FINGER` (Đặt ngón tay lần 1)
  3. `EVT ENROLL_STEP step=1/3 status=REMOVE_FINGER` (Nhấc ngón tay lên)
  4. `EVT ENROLL_STEP step=2/3 status=PLACE_FINGER` (Đặt ngón tay lần 2)
  5. `EVT ENROLL_STEP step=2/3 status=REMOVE_FINGER`
  6. `EVT ENROLL_STEP step=3/3 status=PLACE_FINGER` (Đặt ngón tay lần 3)
  7. `OK ENROLL_SUCCESS slot=<id>` (Đăng ký thành công)
- **Lỗi có thể xảy ra:** `ERR ENROLL_FAILED reason=<TIMEOUT|MISMATCH|SENSOR_BUSY>`

#### `VERIFY`
Kích hoạt cảm biến để kiểm tra nhận diện ngón tay thử nghiệm.
- **Phản hồi thành công:** `OK VERIFIED slot=<id>`
- **Phản hồi thất bại:** `ERR NOT_MATCH`

#### `LIST`
Liệt kê danh sách các Slot vân tay đang có dữ liệu trong cảm biến.
- **Phản hồi:** `OK LIST slots=[1,2,5]`

#### `DELETE <slot_id>`
Xóa dữ liệu mẫu vân tay tại Slot chỉ định khỏi bộ nhớ cảm biến.
- **Phản hồi:** `OK DELETED slot=<id>`

---

## 4. Giao thức ghép nối bảo mật (Host Pairing & Challenge-Response)

Để ngăn chặn việc mật khẩu lưu tĩnh trên chip có thể bị đọc trộm nếu thiết bị rơi vào tay kẻ gian, tinyTouch hỗ trợ cơ chế ghép nối xác thực hai chiều giữa thiết bị và máy tính chủ:

```
[Web Controller / Host]                      [tinyTouch Dongle]
         |                                           |
         | -------- HOST PAIR key_id=<id> ---------> | (Lưu key_id vào NVS)
         | <------- OK HOST PAIRED ----------------- |
         |                                           |
         |         === KHI QUÉT VÂN TAY ===          |
         |                                           |
         | <--- CHALLENGE key_id=<id> nonce=<hex> -- | (Sinh nonce ngẫu nhiên)
         |                                           |
    (Tính HMAC-SHA256)                               |
         |                                           |
         | --- RESPONSE sig=<hmac> key_id=<id> ----> |
         |                                           | (Kiểm tra chữ ký)
         |                                           | ---> Tự động gõ phím HID!
```

1. **Host Pairing:** Trình duyệt sinh một cặp khóa bí mật cục bộ bằng `window.crypto.subtle`. Khi bấm "Ghép nối", `key_id` được ghi vào bộ nhớ NVS của ESP32.
2. **Challenge-Response:** Khi người dùng chạm ngón tay hợp lệ, tinyTouch phát sinh một giá trị ngẫu nhiên `nonce` 32-byte gửi lên cổng Serial.
3. Trình duyệt nhận `CHALLENGE`, dùng khóa bí mật trong `localStorage` tính toán mã băm HMAC-SHA256 và gửi phản hồi `RESPONSE`.
4. Sau khi xác thực đúng chữ ký từ máy tính chủ đã ghép nối, thiết bị mới kích hoạt bàn phím HID gõ chuỗi mật khẩu mở khóa.

---

## 5. Giao thức SmartCard PIV (USB CCID APDU)

Khi ở chế độ PIV (`SET_MODE PIV`), ngoài giao diện USB CDC để cấu hình, thiết bị kích hoạt lớp giao diện **USB CCID** mô phỏng thẻ thông minh NIST PIV (FIPS 201 / SP 800-73) với Application Identifier (AID) `A0 00 00 03 08 00 00 10 00`.

### 5.1. Cấu trúc Slot khóa & Chứng chỉ X.509
- **Slot 9A (Authentication Key):** Cặp khóa RSA 2048-bit phục vụ xác thực người dùng, mở khóa màn hình máy tính (macOS CryptoTokenKit / Windows SmartCard Logon), SSH và lệnh `sudo`.
- **Slot 9D (Key Management Key):** Cặp khóa RSA 2048-bit dùng để giải mã dữ liệu bảo mật (như macOS Login Keychain wrapper).

### 5.2. Luồng xác thực APDU không truyền mật khẩu
1. Hệ điều hành gửi lệnh APDU `GENERAL AUTHENTICATE` (INS `0x87`, P1 `0x07`, P2 `0x9A` hoặc `0x9D`) mang dữ liệu thử thách ngẫu nhiên tới thiết bị qua USB CCID.
2. Firmware kiểm tra điều kiện hiện diện sinh trắc học (**Biometric User Presence**): Yêu cầu người dùng chạm vân tay hợp lệ trong cửa sổ thời gian cho phép.
3. Khi vân tay hợp lệ, thư viện mbedTLS trên ESP32-S3 trực tiếp thực hiện phép ký số nội bộ (`mbedtls_rsa_private`) với Private Key tương ứng trong bộ nhớ chip.
4. Chữ ký số RSA (256 bytes) được đóng gói trong phản hồi APDU (Tag `0x7C` / `0x82`) gửi lại cho hệ điều hành đối soát với Public Key. Private Key không bao giờ bị xuất ra ngoài thiết bị.

