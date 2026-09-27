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
- **Phản hồi (Protocol v7):**
  ```text
  OK STATUS protocol=7 firmware=<version> build=<id> mode=<hid|piv> piv=<ready|unconfigured> sensor=<ready|offline> fingerprints=<count> hosts=<count> enter=<0|1> delay=<ms> led_hid=<start>,<end> led_piv=<start>,<end> ota=<idle|writing|staged>
  ```
  - `mode`: Chế độ hiện tại (`hid` hoặc `piv`).
  - `fingerprints`: Số lượng ngón tay đã đăng ký trong bộ nhớ (0 đến 5).
  - `sensor`: Trạng thái kết nối cảm biến (`ready` hoặc `offline`).
  - `firmware`: Phiên bản firmware hiện hành (vd: `0.1.28`).
  - `hosts`: Số lượng máy chủ đã ghép nối an toàn (Host Pairing).
  - `enter`: Trạng thái tự động gõ phím Enter (`1` = bật, `0` = tắt).
  - `delay`: Độ trễ gõ giữa các ký tự (ms).
  - `led_hid`: Cặp mã màu thở phần cứng cho chế độ HID (`<start_color>,<end_color>`).
  - `led_piv`: Cặp mã màu thở phần cứng cho chế độ PIV (`<start_color>,<end_color>`).
  - `ota`: Trạng thái cập nhật firmware qua Serial.

#### `SET LED_HID <start> <end>` / `SET LED_PIV <start> <end>`
Cấu hình cặp màu thở đèn LED vòng cảm biến cho chế độ HID hoặc PIV (yêu cầu quyền quản trị `AUTH`).
- Bảng mã màu phần cứng (1 đến 7):
  - `1`: Xanh lam (Blue)
  - `2`: Xanh lục (Green)
  - `3`: Xanh ngọc (Cyan)
  - `4`: Đỏ (Red)
  - `5`: Tím (Purple)
  - `6`: Vàng kim (Yellow)
  - `7`: Trắng (White)
- Giá trị mặc định:
  - HID: `SET LED_HID 3 1` (Cyan → Xanh lam)
  - PIV: `SET LED_PIV 6 4` (Vàng kim → Đỏ)
- Thiết bị lưu cấu hình vào Flash NVS và lập tức cập nhật hiệu ứng thở trên vòng cảm biến.
- **Phản hồi:** `OK SET` hoặc `ERR SET`

#### `AUTH`
Yêu cầu mở phiên ủy quyền quản trị (hiệu lực 15 giây) để thực hiện các thao tác nhạy cảm (`SET`, `FINGER`, `HOST`, `PIV CREATE`, `OTA`).
- Thiết bị nhấp nháy đèn LED yêu cầu người dùng chạm vân tay đã đăng ký vào cảm biến để xác nhận.
- **Phản hồi:** `OK AUTH` (thành công) hoặc `ERR AUTH` (thất bại/hết thời gian).

#### `SET MODE <HID|PIV>`
Thay đổi chế độ hoạt động chính (yêu cầu quyền `AUTH`).
- `SET MODE HID`: Chuyển sang chế độ giả lập bàn phím tự gõ mật khẩu (Vòng LED thở theo cấu hình `led_hid`, mặc định Cyan - Blue).
- `SET MODE PIV`: Chuyển sang chế độ SmartCard X.509 (Vòng LED thở theo cấu hình `led_piv`, mặc định Yellow - Red).
- **Phản hồi:** `OK SET MODE` hoặc `ERR SET MODE`

#### `SET TYPE_DELAY <ms>`
Cài đặt thời gian trễ gõ giữa các ký tự bàn phím USB HID (ms, mặc định 25ms).
- **Phản hồi:** `OK SET`

#### `SET SUBMIT_ENTER <0|1>`
Bật (`1`) hoặc tắt (`0`) tính năng tự động nhấn phím Enter sau chuỗi mật khẩu.
- **Phản hồi:** `OK SET`

#### `SET COOLDOWN <ms>`
Đặt thời gian trễ nghỉ chống quét lặp lại sau mỗi lần chạm vân tay (ms).
- **Phản hồi:** `OK SET`

#### `SET_KEY_SEQ <sequence>`
Cài đặt chuỗi phím bấm tự động sau khi gõ xong chuỗi mật khẩu.
- Các preset thông dụng:
  - `SUBMIT_ENTER`: Chỉ bấm phím `Enter`.
  - `SUBMIT_ENTER_SPACE`: Bấm `Enter` sau đó bấm `Space`.
  - `SUBMIT_TAB_ENTER`: Bấm `Tab` sau đó bấm `Enter` (dành cho form có ô mã OTP).
  - `SUBMIT_ENTER_ENTER`: Bấm `Enter` 2 lần.
  - `SUBMIT_NONE`: Không nhấn bất kỳ phím nào sau khi gõ.
- **Phản hồi:** `OK KEY_SEQ_SAVED`

#### `PIV CREATE`
Kích hoạt sinh mới cặp khóa RSA-2048 nội bộ và tạo lại chứng chỉ số PIV X.509 lưu vào NVS (yêu cầu quyền `AUTH`).
- Thiết bị phát sự kiện `EVENT PIV_CREATE` và phản hồi `OK PIV CREATE` sau khi hoàn tất.

#### `USB RECONNECT`
Kích hoạt quét lại USB CCID SmartCard trên máy tính chủ (hỗ trợ macOS tự động nhận token sau khi cấu hình).
- **Phản hồi:** `OK USB RECONNECT`

#### `RESET FACTORY`
Khôi phục cài đặt gốc, xóa toàn bộ NVS, danh sách Host Pairing và mẫu vân tay trong cảm biến (yêu cầu quyền `AUTH`).
- **Phản hồi:** `OK RESET FACTORY`

---

### 3.2. Quản lý sinh trắc học vân tay

Tất cả các lệnh quản lý vân tay yêu cầu phiên ủy quyền `AUTH` trước khi thực thi:

#### `FINGER ENROLL <slot_id>`
Bắt đầu quy trình lấy mẫu vân tay cho Slot từ `1` đến `5`.
- **Luồng sự kiện từng bước:**
  1. `OK FINGER`
  2. `EVT ENROLL_STEP step=1/3 status=PLACE_FINGER` (Đặt ngón tay lần 1)
  3. `EVT ENROLL_STEP step=1/3 status=REMOVE_FINGER` (Nhấc ngón tay lên)
  4. `EVT ENROLL_STEP step=2/3 status=PLACE_FINGER` (Đặt ngón tay lần 2)
  5. `EVT ENROLL_STEP step=2/3 status=REMOVE_FINGER`
  6. `EVT ENROLL_STEP step=3/3 status=PLACE_FINGER` (Đặt ngón tay lần 3)
  7. `OK ENROLL_SUCCESS slot=<id>` (Đăng ký thành công)
- **Lỗi có thể xảy ra:** `ERR ENROLL_FAILED reason=<TIMEOUT|MISMATCH|SENSOR_BUSY>`

#### `FINGER DELETE <slot_id>`
Xóa dữ liệu mẫu vân tay tại Slot chỉ định khỏi bộ nhớ cảm biến.
- **Phản hồi:** `OK FINGER` hoặc `ERR FINGER`

#### `FINGER CLEAR`
Xóa toàn bộ các mẫu vân tay trong bộ nhớ cảm biến.
- **Phản hồi:** `OK FINGER` hoặc `ERR FINGER`

---

### 3.3. Giao thức Nạp Firmware Serial OTA

Quy trình nạp nhị phân firmware an toàn qua cổng CDC không cần nút BOOT vật lý:

#### `OTA BEGIN <token> <size> <sha256>`
Khởi tạo phiên nạp OTA (yêu cầu quyền `AUTH`):
- `token`: Chuỗi hex ngẫu nhiên 32 ký tự định danh phiên nạp.
- `size`: Kích thước file nhị phân (bytes).
- `sha256`: Mã băm SHA-256 (64 ký tự hex) của toàn bộ file binary để kiểm tra toàn vẹn sau khi ghi.
- **Phản hồi:** `OK OTA BEGIN next=0`

#### `OTA WRITE <token> <offset> <base64_data>`
Ghi một khối nhị phân (tối đa 3072 bytes mã hóa Base64) vào phân vùng flash:
- **Phản hồi:** `OK OTA WRITE next=<next_offset>`

#### `OTA COMMIT <token>`
Hoàn tất nạp, thiết bị tự động đối soát mã băm SHA-256 của toàn bộ ảnh vừa ghi trong Flash. Nếu khớp, thiết bị chuyển trạng thái boot sang phân vùng mới và khởi động lại:
- **Phản hồi:** `OK OTA COMMIT` (hoặc `ERR OTA COMMIT` nếu sai mã hash)

#### `OTA ABORT [token]`
Hủy bỏ phiên nạp hiện hành và dọn dẹp bộ nhớ tạm.
- **Phản hồi:** `OK OTA ABORT`

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
2. Firmware kiểm tra điều kiện hiện diện sinh trắc học (**Biometric User Presence**): Yêu cầu người dùng chạm vân tay hợp lệ trong cửa sổ thời gian cho phép, hoặc phiên xác thực PIN thành công.
3. Khi điều kiện thỏa mãn, thư viện mbedTLS trên ESP32-S3 trực tiếp thực hiện phép ký số nội bộ (`mbedtls_rsa_private`) với Private Key tương ứng trong bộ nhớ chip.
4. Chữ ký số RSA (256 bytes) được đóng gói trong phản hồi APDU (Tag `0x7C` / `0x82`) gửi lại cho hệ điều hành đối soát với Public Key. Private Key không bao giờ bị xuất ra ngoài thiết bị.

### 5.3. Quy trình ghép đôi (Pairing) PIV SmartCard trên macOS

Mã PIN mặc định của thẻ PIV trong firmware: **`754321`** (6 chữ số; ở bản gốc là `111111`).

> **Tùy biến mã PIN trước khi build:**
> Để thay đổi mã PIN này trước khi biên dịch lại firmware, cần sửa đồng bộ tại:
> - `firmware/main/piv.c` (Dòng 566 - 568: `expected_pin[8]`).
> - `firmware/main/touch_pin_hid.c` (Dòng 377: `piv_pin[]`).
> - `controller/index.html` (Dòng 2222, 2253, 2265: hiển thị và nút sao chép PIN trên Web UI).
> Chi tiết xem tại [BUILD_AND_FLASH.md](./BUILD_AND_FLASH.md#2-tùy-biến-mã-pin-smartcard-piv-tùy-chọn-trước-khi-build).

#### Các bước khởi tạo và liên kết tài khoản:

1. **Chuyển thiết bị sang chế độ PIV:**
   - Trên Web Controller: Vào tab **Cấu hình** -> Chọn chế độ **PIV (SmartCard)** -> Thiết bị khởi động lại giao diện USB CCID.

2. **Kiểm tra nhận diện SmartCard trên macOS:**
   ```bash
   security list-smartcards
   ```
   Hệ thống phản hồi token dạng: `com.apple.pivtoken:<ID>`

3. **Lấy mã băm chứng chỉ (Public Key Hash):**
   ```bash
   sc_auth identities
   ```
   Kết quả trả về danh sách Unpaired identities cùng mã Hash 40 ký tự (ví dụ: `48678DC8E216F1DBA1D7D04874AAC512360BB5D8`).

4. **Thực hiện ghép đôi tài khoản:**
   ```bash
   sudo sc_auth pair -u $(whoami) -h <MÃ_HASH_Ở_BƯỚC_3>
   ```
   - Nhập mật khẩu tài khoản macOS cho lệnh `sudo`.
   - Khi popup **SmartCard Agent** xuất hiện trên màn hình: Nhập PIN **`754321`**.
   - Nếu hệ thống hỏi xác nhận: Nhập lại mật khẩu macOS để hoàn tất gắn kết chứng chỉ vào tài khoản.

5. **Xác minh ghép đôi thành công:**
   ```bash
   sc_auth list $(whoami)
   ```
   Hiển thị mã hash chứng chỉ đã được liên kết với người dùng.

6. **Cơ chế mở khóa màn hình:**
   - Tại màn hình khóa macOS, khi cắm tinyTouch ở chế độ PIV, hệ điều hành tự động chọn phương thức SmartCard.
   - Khi chạm ngón tay đã đăng ký vào cảm biến, thiết bị tự động gửi mã PIN `754321` qua bàn phím USB và cấp quyền ký RSA để mở khóa máy tính tức thì mà không cần gõ phím.

---

## 6. Giao thức không dây Bluetooth Low Energy (BLE Wireless)

Bên cạnh cổng USB CDC/HID vật lý, tinyTouch tích hợp ngăn xếp Bluetooth Low Energy (Apache NimBLE) để hỗ trợ vận hành không dây song song (Dual-Mode Wireless) và quản lý nguồn pin tối ưu.

### 6.1. Kiến trúc GATT Services (NimBLE Stack)

Thiết bị công bố 4 GATT Primary Services tiêu chuẩn:

| Service | UUID | Đặc tính (Characteristic) | UUID Char | Quyền hạn / Flags | Mô tả |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Human Interface Device (HOGP)** | `0x1812` | Protocol Mode | `0x2A4E` | Read, WriteWithoutResponse | Chế độ báo cáo (Report Mode: `0x01`) |
| | | HID Information | `0x2A4A` | Read | Phiên bản `0x0111`, cờ kết nối `0x02` |
| | | Report Map | `0x2A4B` | Read (Encrypted) | HID Keyboard Descriptor tiêu chuẩn (8 bytes) |
| | | HID Control Point | `0x2A4C` | WriteWithoutResponse | Điều khiển Suspend / Resume |
| | | Input Report | `0x2A4D` | Read, Notify (Bắt buộc mã hóa `READ_ENC \| NOTIFY_INDICATE_ENC`) | Báo cáo bàn phím 8 bytes (1 modifier, 1 reserved, 6 keycodes) kèm Descriptor Report Reference (`0x2908`) |
| **Nordic UART Service (NUS)** | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | NUS RX | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Write, WriteWithoutResponse | Tiếp nhận chuỗi lệnh Console từ Web Bluetooth |
| | | NUS TX | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Notify | Thiết bị gửi phản hồi / Telemetry lên máy chủ |
| **Battery Service** | `0x180F` | Battery Level | `0x2A19` | Read, Notify | Tỷ lệ phần trăm pin còn lại (0 - 100%) |
| **Device Information** | `0x180A` | Manufacturer Name | `0x2A29` | Read | `tinyTouch` |
| | | Model Number | `0x2A24` | Read | `tinyTouch Key` |
| | | PnP ID | `0x2A50` | Read | Vendor ID Source `0x02`, VID `0x303A`, PID `0x4001`, Rev `0x0100` |

### 6.2. Thông số kết nối tối ưu cho Apple / Windows (Connection Parameters)

Để cân bằng giữa thời gian đáp ứng khi gõ phím (< 50ms) và tiết kiệm điện năng trên pin LiPo 370 mAh, thiết bị tự động đàm phán thông số kết nối tuân thủ chặt chẽ *Apple Accessory Design Guidelines*:

- **Connection Interval:** 30ms đến 50ms (`itvl_min = 24`, `itvl_max = 40`).
- **Slave Latency:** `20` kết nối (cho phép chip ngủ bỏ qua chu kỳ rỗi khi không gõ phím).
- **Supervision Timeout:** 4000ms (`supervision_timeout = 400`).
- **Bảo mật & Ghép đôi:**
  - Hỗ trợ BLE Security Manager (SM) bonding lưu vào Flash NVS qua `ble_store_config_init()`.
  - Toàn bộ kênh truyền HID Input Report và Report Map bắt buộc mã hóa AES-128 CCM (`BLE_GAP_CHR_F_READ_ENC`, `BLE_GAP_CHR_F_NOTIFY_INDICATE_ENC`).

### 6.3. Cơ chế Bảo mật Zero-Trust Control Plane

Kênh truyền nối tiếp BLE NUS mở ra khả năng điều khiển không dây, tuy nhiên để ngăn chặn các cuộc tấn công không dây từ xa qua sóng Bluetooth:

1. **Danh sách các lệnh cấm tuyệt đối trên BLE NUS:**
   - `OTA BEGIN`, `OTA WRITE`, `OTA COMMIT` (Quy trình nạp firmware).
   - `RESET FACTORY` (Khôi phục cài đặt gốc, xóa chìa khóa bảo mật).
   - `PIV CREATE` (Sinh lại cặp khóa RSA và chứng chỉ bảo mật).
2. **Phản hồi từ chối từ thiết bị:**
   Khi nhận các lệnh trên qua kênh BLE NUS, bộ phân giải dòng lệnh lập tức chặn lại và phản hồi:
   ```text
   ERR DISALLOWED_ON_BLE
   ```
   *Các thao tác này bắt buộc phải thực hiện thông qua kết nối có dây USB CDC vật lý.*
3. **Quyền quản trị `AUTH`:** Mọi tác vụ cấu hình thông qua BLE vẫn chịu ràng buộc của phiên ủy quyền `AUTH` với xác thực sinh trắc học vân tay trực tiếp trên cảm biến.

### 6.4. Quản lý nguồn 3 tầng (3-Tier Power Management) cho Pin 370 mAh

Thiết bị thiết kế tối ưu cho pin LiPo dung lượng 370 mAh với kiến trúc quản lý nguồn 3 tầng tự động:

```
[Hoạt động chạm / gõ phím]
           │
           ▼
┌────────────────────────────────────────┐
│ Tầng 1: Active Mode                    │
│ - ESP32-S3 CPU & RF hoạt động đầy đủ   │
│ - Cảm biến vân tay sẵn sàng quét       │
└────────────────────────────────────────┘
           │
           │ (Chạy nguồn pin LiPo, không cắm USB)
           ▼
┌────────────────────────────────────────┐
│ Tầng 2: Light Sleep (Tiết kiệm nguồn)  │
│ - Tự động TẮT đèn LED Aura thở         │
│ - Giảm tiêu thụ ~15-20mA tĩnh          │
│ - BLE duy trì chu kỳ Slave Latency 20  │
└────────────────────────────────────────┘
           │
           │ (Sau 15 phút không hoạt động trên pin)
           ▼
┌────────────────────────────────────────┐
│ Tầng 3: Deep Sleep                     │
│ - Cảm biến nhận lệnh ngủ 0x33          │
│ - ESP32-S3 tắt toàn bộ CPU & Radio     │
│ - Bật ngắt phần cứng EXT0 trên GPIO 2  │
│ - Tiêu thụ tĩnh < 30 µA                │
└────────────────────────────────────────┘
```

#### Chi tiết các tầng:
1. **Tầng 1 (Active Mode):** Khi cắm USB hoặc đang trong phiên xác thực vân tay. Mọi thành phần hoạt động hết công suất.
2. **Tầng 2 (Light Sleep):** Khi hoạt động bằng nguồn pin và không cắm sạc USB (`!transport_is_usb_active()`), vòng đèn Aura Breathing tự động tắt hoàn toàn để triệt tiêu dòng tiêu thụ tĩnh của 4 bóng LED RGB.
3. **Tầng 3 (Deep Sleep):**
   - Sau **15 phút** không có thao tác (`IDLE_SLEEP_TIMEOUT_US = 15m`), thiết bị gửi mã lệnh `0x33` chuyển cảm biến sang trạng thái ngủ sâu, sau đó kích hoạt ngắt RTC Wakeup `EXT0` trên **GPIO 2** (chân TouchOut của cảm biến SW111) rồi đưa vi điều khiển vào Deep Sleep.
   - **Chống kẹt nút / áp lực liên tục:** Trước khi vào Deep Sleep, nếu GPIO 2 vẫn bị giữ mức cao (ví dụ bị đè trong túi), thiết bị chờ giải phóng với giới hạn 500ms để chống vòng lặp thức/ngủ liên tục microsecond.

#### Anti-Ghost Wakeup Filter (1.5 Giây)
- Khi thiết bị thức dậy từ Deep Sleep thông qua ngắt cảm ứng `EXT0` lúc đang chạy pin, bộ hẹn giờ Anti-Ghost lập tức mở cửa sổ quan sát **1.5 giây** (`ANTI_GHOST_TIMEOUT_US = 1500ms`).
- Nếu trong 1.5 giây này người dùng **không** thực hiện quét vân tay hợp lệ (không có sự kiện xác thực sinh trắc học thành công) và không cắm cáp USB, thiết bị xác định đây là kích hoạt ngoài ý muốn (chạm quẹt ngẫu nhiên hoặc va chạm trong balo) và ngay lập tức quay trở lại chế độ Deep Sleep.
- Cơ chế này bảo vệ hoàn hảo dung lượng pin 370 mAh, ngăn ngừa cạn kiệt pin sau nhiều ngày cất giữ.



