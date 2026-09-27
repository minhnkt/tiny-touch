# tinyTouch Web Controller

Giao diện điều khiển và quản trị trực quan cho thiết bị bảo mật **tinyTouch**, chạy trực tiếp trên nền tảng Web thông qua **Web Serial API**.

---

## Điểm nhấn công nghệ

- **Zero Build Tools / 100% Portable:** Được viết hoàn toàn bằng Pure Vanilla HTML5, CSS3 và Modern JavaScript (ES6+). Không phụ thuộc vào npm, Webpack, hay framework nặng nề — chỉ cần mở file là chạy.
- **Thiết kế chuẩn Apple macOS Glass:** Giao diện lấy cảm hứng trực tiếp từ Apple System Settings (macOS Tahoe/Sequoia), bố cục 2 cột (Sidebar + Main View), hiệu ứng kính mờ (blur backdrop), typography chuẩn SF Pro.
- **Hỗ trợ Light / Dark Mode:** Nút chuyển đổi giao diện sáng/tối linh hoạt với lưu trữ trạng thái tự động trong `localStorage`.
- **Giao tiếp phần cứng trực tiếp (Web Serial API):** Kết nối cổng ảo USB CDC của ESP32-S3 ở tốc độ `115200 baud (8-N-1)`. Không cần cài đặt bất kỳ driver hay ứng dụng nền nào.
- **Tùy biến Aura Breathing LED Palette:** Lựa chọn bảng màu nhịp thở LED cảm biến vân tay riêng biệt cho từng chế độ (HID Mode và PIV Mode) với palette 7 màu trực quan và dot preview tối giản.
- **Quản lý PIV SmartCard & PIN:** Hiển thị mã PIN PIV mặc định (`754321`), hỗ trợ nút ẩn/hiện và sao chép mã PIN với biểu tượng SVG động, tích hợp sẵn hướng dẫn ghép đôi macOS (`sc_auth`).
- **Xác thực Web Host Pairing (Web Crypto API):** Hỗ trợ ghép nối an toàn với máy tính qua cơ chế Challenge-Response bảo mật (HMAC SHA-256), tự động phản hồi phần cứng khi nhận diện vân tay để mở khóa mật khẩu.
- **Web Serial OTA Firmware Flasher:** Hỗ trợ nạp firmware trực tiếp qua cổng Serial từ file `tiny_touch_unified.bin` với xác thực sinh trắc học vân tay và kiểm tra hash SHA-256 tự động.
- **Build Timestamp Automation:** Tự động hiển thị thời gian build và phiên bản firmware đồng bộ theo thời gian thực.
- **Serial Terminal Drawer thời gian thực:** Cửa sổ Terminal tích hợp ngay chân trang cho phép theo dõi toàn bộ log RX/TX, tự động cuộn và có thể đóng/mở nhanh.

---

## Yêu cầu trình duyệt

Cần trình duyệt hỗ trợ [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API):
- **Khuyến nghị:** Google Chrome, Microsoft Edge, Brave, Opera, Cốc Cốc (phiên bản máy tính để bàn).
- *Lưu ý:* Safari và Firefox hiện chưa hỗ trợ tiêu chuẩn Web Serial API của W3C.

---

## Hướng dẫn sử dụng

### 1. Khởi chạy cục bộ (Local)

Bạn có thể mở trực tiếp file `index.html` trên trình duyệt Chrome, hoặc chạy một web server tĩnh nhẹ:

```bash
# Sử dụng Python 3 có sẵn
python3 -m http.server 8000

# Hoặc dùng Node.js
npx serve controller
```
Sau đó truy cập: `http://localhost:8000` (hoặc `http://localhost:8000/controller/`).

### 2. Các bước vận hành thiết bị

1. **Kết nối:** Cắm tinyTouch vào cổng USB, bấm nút **Kết nối** ở góc trên bên phải, chọn cổng Serial tương ứng (`tinyTouch CDC` hoặc `usbmodem*`).
2. **Đăng ký vân tay (Enroll):**
   - Chọn thẻ **Vân tay & Mật khẩu**.
   - Bấm **Đăng ký** tại Slot mong muốn (hỗ trợ tối đa 5 Slot).
   - Đặt và nhấc ngón tay 3 lần theo hướng dẫn hoạt họa trực quan trên màn hình.
3. **Cấu hình mật khẩu & Ghép nối Host:**
   - Điền mật khẩu mong muốn vào ô tương ứng với Slot vân tay.
   - Bấm **Ghép nối máy tính này (Host Pairing)** để sinh khóa xác thực mã hóa an toàn qua Web Crypto API.
4. **Cài đặt phím kết thúc (Ending Keys):**
   - Tại thẻ **Hệ thống**, chọn hành vi sau khi gõ mật khẩu (`Enter`, `Enter ➔ Space`, `Tab ➔ Enter`, hoặc `Không nhấn`).
5. **Chuyển chế độ hoạt động & Tùy biến LED Aura:**
   - Tại thẻ **Chế độ**, dễ dàng chuyển đổi giữa **HID Mode** (gõ bàn phím) và **PIV Mode** (thẻ SmartCard).
   - Tùy chỉnh màu sắc nhịp thở LED cảm biến vân tay riêng biệt cho từng chế độ từ bảng 7 màu RGB với dot preview trực quan.
6. **Quản lý PIV SmartCard & PIN:**
   - Xem và sao chép mã PIN PIV mặc định (`754321`) với biểu tượng SVG động, thực hiện lệnh ghép đôi macOS qua `sc_auth pair`.
7. **Nạp Firmware (Serial OTA):**
   - Bấm **Nâng cấp** trên Dashboard hoặc vào tab **Cài đặt hệ thống**, chọn file `firmware/build/tiny_touch_unified.bin` và chạm vân tay xác thực để nạp firmware trực tiếp không cần tháo vỏ thiết bị.
