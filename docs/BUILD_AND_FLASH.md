# Hướng dẫn Nạp Firmware tinyTouch (ESP32-S3)

Tài liệu hướng dẫn chi tiết quy trình biên dịch và nạp firmware cho thiết bị bảo mật sinh trắc học **tinyTouch**.

Hệ thống hỗ trợ **2 phương pháp nạp firmware**:
1. **Phương pháp nạp Serial OTA (Khuyên dùng):** Nạp qua cổng USB CDC Console bằng giao thức OTA nhúng, không cần tháo vỏ, không cần giữ nút BOOT vật lý.
2. **Phương pháp nạp ROM Bootloader (Cứu hộ):** Dùng `idf.py flash` qua USB-Serial/JTAG để nạp trực tiếp vào Flash (dùng khi cài đặt lần đầu hoặc khi thiết bị treo cứng/bootloop).

---

## Chuẩn bị môi trường & Biên dịch (Build)

### 1. Kích hoạt môi trường ESP-IDF v5.3

```bash
# Kích hoạt ESP-IDF (phiên bản v5.0+ hoặc v5.3)
source ~/esp/esp-idf-v5.3/export.sh
```

### 2. Biên dịch Firmware

```bash
# Biên dịch mã nguồn firmware
idf.py -C firmware build
```

Sau khi biên dịch hoàn tất, file nhị phân sẵn sàng tại:
- **Đường dẫn binary:** `firmware/build/tiny_touch_unified.bin`
- **Kích thước:** ~462 KB
- **Bảo mật:** Đã được tự động ký Secure Boot bằng private key nội bộ (`firmware/secure_boot_signing_key.pem`).

---

## Phương pháp 1: Nạp Serial OTA (Khuyên dùng thường nhật)

Phương pháp này nạp trực tiếp qua cổng CDC Serial khi thiết bị đang chạy bình thường.

### Cơ chế hoạt động:
- Giao tiếp qua lệnh Console: `OTA BEGIN`, `OTA WRITE` (chia nhỏ dữ liệu thành các chunk Base64 3072 bytes), và `OTA COMMIT`.
- **Bảo mật sinh trắc học:** Bắt buộc người dùng phải xác thực vân tay (`AUTH` -> `EVENT TOUCH`) trước khi cấp quyền ghi vào phân vùng `ota_0`/`ota_1`.
- Giữ nguyên toàn bộ cấu hình máy chủ (Host Pairing) và mẫu vân tay trong bộ nhớ.

---

### Cách 1.1: Nạp qua giao diện Web Controller (Dễ nhất)

1. **Khởi chạy máy chủ web cục bộ:**
   ```bash
   python3 -m http.server 8000
   ```
2. **Truy cập giao diện Web Controller:**
   - Mở trình duyệt Chrome/Edge/Brave tại: [http://localhost:8000](http://localhost:8000) (hoặc trang Web Controller trực tuyến).
3. **Kết nối thiết bị:**
   - Bấm **Kết nối**, chọn cổng Serial của tinyTouch (`/dev/cu.usbmodem*`).
4. **Mở modal nạp OTA:**
   - Cách A: Nhấn nút **Nâng cấp** ngay tại widget *Firmware & Device Info* trên Dashboard.
   - Cách B: Chuyển sang tab **Cài đặt hệ thống** -> nhấn nút **Nạp Firmware (Serial OTA)**.
5. **Chọn file firmware:**
   - Chọn file `firmware/build/tiny_touch_unified.bin`.
   - Trình duyệt sẽ tự động tính mã băm SHA-256 đối soát an toàn.
6. **Thực hiện nạp:**
   - Bấm **Bắt đầu nạp OTA**.
   - Khi thiết bị yêu cầu, **chạm ngón tay đã đăng ký vào cảm biến** để xác thực cấp quyền.
   - Theo dõi thanh tiến trình nạp (0% - 100%).
   - Sau khi hoàn tất thông báo thành công: **Rút USB ra và cắm lại** để khởi động vào phiên bản firmware mới.

---

### Cách 1.2: Nạp qua CLI Script Python (Dành cho dòng lệnh)

Khi không sử dụng trình duyệt web, bạn có thể nạp trực tiếp qua script Python tích hợp sẵn trong repo:

```bash
# Đảm bảo tắt Web Controller hoặc các app đang chiếm cổng Serial
~/.espressif/python_env/idf5.3_py3.9_env/bin/python firmware/ota_flash.py /dev/cu.usbmodemXXXXX firmware/build/tiny_touch_unified.bin
```

*(Thay `/dev/cu.usbmodemXXXXX` bằng cổng thực tế của máy, ví dụ `/dev/cu.usbmodem21101`)*

**Luồng thực thi của script:**
1. Mở cổng Serial tốc độ 115200 baud.
2. Kiểm tra trạng thái thiết bị (`STATUS`).
3. Gửi lệnh `AUTH`, khi màn hình hiện:
   ```text
   👉 Chạm ngón tay vào cảm biến vân tay để xác nhận!...
   ```
   Chạm ngón tay đã đăng ký vào cảm biến để mở khóa session OTA.
4. Tự động stream các khối dữ liệu Base64 3072 bytes.
5. Gửi `OTA COMMIT` và hoàn tất.
6. **Rút USB ra và cắm lại** để thiết bị nạp phiên bản mới.

---

## Phương pháp 2: Nạp ROM Bootloader (Phục hồi / Cứu hộ)

Sử dụng khi:
- Nạp firmware lần đầu cho mạch trắng.
- Thiết bị bị treo cứng, bootloop, hoặc firmware hỏng không nhận diện cổng USB CDC.

### Các bước thực hiện:

1. **Đưa ESP32-S3 về chế độ ROM Bootloader (nếu bị bootloop):**
   - Nhấn giữ nút **BOOT** trên bo mạch ESP32-S3 SuperMini.
   - Cắm cáp USB vào máy tính (hoặc nhấn nhả nút **RESET** trong khi vẫn giữ nút **BOOT**).
   - Thả nút **BOOT**.

2. **Xác định cổng Serial Native USB-Serial/JTAG:**
   - macOS:
     ```bash
     ls /dev/cu.usbmodem*
     ```
   - Linux:
     ```bash
     ls /dev/ttyACM*
     ```

3. **Nạp toàn bộ Flash:**
   ```bash
   source ~/esp/esp-idf-v5.3/export.sh
   idf.py -C firmware -p /dev/cu.usbmodemXXXXX flash
   ```

4. **Theo dõi log hoạt động:**
   ```bash
   idf.py -C firmware -p /dev/cu.usbmodemXXXXX monitor
   ```
   *(Bấm `Ctrl + ]` để thoát monitor).*
