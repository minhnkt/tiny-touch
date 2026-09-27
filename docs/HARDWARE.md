# tinyTouch Hardware & Wiring Guide

Hướng dẫn chi tiết về phần cứng, sơ đồ chân (Pinout), sơ đồ hàn dây thực tế và file in 3D vỏ bọc cho thiết bị **tinyTouch**.

---

## 1. Danh sách linh kiện (Bill of Materials - BOM)

### 1.1. Vi điều khiển chính (MCU Board)
- **ESP32-S3 SuperMini (Khuyến nghị số 1):**
  - Kích thước siêu nhỏ gọn: `22.5 x 18 mm`.
  - Tích hợp sẵn cổng Type-C Native USB (OTG / CDC / HID).
  - Giá thành rẻ, layout chân dễ hàn trực tiếp với module cảm biến.
- **Seeed Studio XIAO ESP32-S3:**
  - Kích thước: `21 x 17.5 mm`.
  - Phù hợp cho các thiết kế vỏ siêu mỏng.

### 1.2. Cảm biến vân tay UART
- **SW111 / ZW111 (Cảm biến thực tế khuyên dùng & đã kiểm thử):**
  - Cảm biến vân tay điện dung hình tròn thế hệ mới, mặt phẳng cảm ứng siêu nhạy.
  - Tích hợp viền LED RGB phản hồi trực quan (Xanh dương: Chờ quét; Xanh lá: Xác thực thành công; Đỏ: Không khớp).
  - Tốc độ nhận diện < 0.2s, hỗ trợ chân ngắt nhận diện chạm tức thì (`TouchOut`).
  - Giao tiếp giắc 6-pin SH 1.0mm hoặc pad hàn trực tiếp.
- **SW101 / ZW101:**
  - Cảm biến quang học / điện dung dạng chữ nhật, chuẩn lệnh UART Synochip 57600 baud tương thích hoàn toàn firmware.
- **Grow R503 / R502-A:**
  - Cảm biến điện dung dạng tròn tương đương, dùng chung sơ đồ chân UART.

### 1.3. Vỏ bảo vệ (Case)
- In 3D (PLA / PETG / Resin) từ các file thiết kế tại thư mục `docs/hardware/case/`.

---

## 2. Sơ đồ thứ tự chân cảm biến SW111 / SW101 (6-Pin SH1.0)

Cảm biến **SW111** và **SW101** sử dụng đầu nối 6 chân chuẩn khoảng cách `1.0mm` (tính từ Pin 1 đến Pin 6 trên jack cảm biến):

```
         +---------------------------------------+
         |     [CẢM BIẾN VÂN TAY SW111 / SW101]   |
         |                                       |
         |         [1] [2] [3] [4] [5] [6]       |
         +---------------------------------------+
            |   |   |   |   |   |
            |   |   |   |   |   +---> Pin 6: GND (Mass)
            |   |   |   |   +-------> Pin 5: RXD (Nhận lệnh từ ESP32)
            |   |   |   +-----------> Pin 4: TXD (Gửi dữ liệu sang ESP32)
            |   |   +---------------> Pin 3: VCC (Nguồn nuôi module 3.3V)
            |   +-------------------> Pin 2: TouchOut / IRQ (Ngắt chạm)
            +-----------------------> Pin 1: VTouch (Nguồn mạch cảm ứng chạm 3.3V)
```

| Chân (Pin) | Ký hiệu | Màu dây chuẩn | Điện áp / Mức logic | Chức năng chi tiết |
|:----------:|:--------|:--------------|:--------------------|:-------------------|
| **1** | `VTouch` | Tím / Trắng | **3.3V** | Cấp nguồn cho mạch nhận diện cảm ứng ngón tay |
| **2** | `TouchOut` | Vàng / Xanh dương | **3.3V High active** | Xuất mức `HIGH` ngay khi có ngón tay chạm vào bề mặt |
| **3** | `VCC` | Đỏ | **3.3V** | Nguồn chính cấp cho DSP và bộ xử lý vân tay |
| **4** | `TXD` | Xanh lá | **3.3V TTL** | Chân truyền dữ liệu UART của cảm biến |
| **5** | `RXD` | Trắng / Vàng | **3.3V TTL** | Chân nhận lệnh UART của cảm biến |
| **6** | `GND` | Đen | **0V** | Nối đất (Ground chung) |

---

## 3. Sơ đồ hàn dây thực tế (Wiring & Soldering Guide)

### 3.1. Hàn nối với ESP32-S3 SuperMini (Cấu hình người dùng thực tế)

Mạch **ESP32-S3 SuperMini** kết nối với **SW111**:

```
 [SW111 Cảm biến]                             [ESP32-S3 SuperMini]
  Pin 1 (VTouch)  ----+
                      |---------------------->  Chân 3V3
  Pin 3 (VCC)     ----+
  Pin 2 (TouchOut) -------------------------->  GPIO 2
  Pin 4 (TXD)      -------------------------->  GPIO 44 (RXD)
  Pin 5 (RXD)      -------------------------->  GPIO 43 (TXD)
  Pin 6 (GND)      -------------------------->  Chân GND
```

#### Bảng chi tiết mối hàn:
| Chân cảm biến SW111 | Chân ESP32-S3 SuperMini | Hướng dẫn thực hành hàn |
|:--------------------|:------------------------|:------------------------|
| **Pin 1 (VTouch) & Pin 3 (VCC)** | **3V3** | Chập chung 2 dây này lại và hàn vào duy nhất 1 chân **3V3** của SuperMini. |
| **Pin 2 (TouchOut / IRQ)** | **GPIO 2** | Hàn vào chân **2** (nằm ngay sát chân 3V3 trên hàng chân SuperMini). |
| **Pin 4 (Sensor TXD)** | **GPIO 44** | Hàn vào chân **44** (Cổng UART RX của ESP32). |
| **Pin 5 (Sensor RXD)** | **GPIO 43** | Hàn vào chân **43** (Cổng UART TX của ESP32). |
| **Pin 6 (GND)** | **GND** | Hàn vào chân **GND** của SuperMini. |

> **Lưu ý quan trọng khi hàn:**
> - Cảm biến SW111 chạy hoàn toàn ở mức điện áp **3.3V**. **Tuyệt đối không cấp nguồn 5V** vào Pin 1 hoặc Pin 3 vì sẽ gây hỏng cảm biến.
> - Chân **GPIO 43/44** là chân UART_NUM_1 mặc định đã được cấu hình trong `firmware/main/fingerprint.c`.
> - Việc hàn chân **TouchOut** vào **GPIO 2** giúp thiết bị nhận biết ngay lập tức khi ngón tay vừa đặt vào mà không cần firmware phải gửi lệnh polling liên tục, tiết kiệm năng lượng và tăng độ nhạy gõ phím lên mức tối đa.

---

### 3.2. Hàn nối với Seeed Studio XIAO ESP32-S3

| Chân cảm biến SW111 / SW101 | Chân Seeed XIAO ESP32-S3 | Chức năng |
|:----------------------------|:-------------------------|:----------|
| **Pin 1 (VTouch) & Pin 3 (VCC)** | **3V3** | Nguồn cấp 3.3V |
| **Pin 2 (TouchOut)** | **GPIO 1 (D0 / D1)** | Ngắt chạm ngón tay |
| **Pin 4 (Sensor TXD)** | **GPIO 44 (D7 / RX)** | Cảm biến truyền về ESP32 |
| **Pin 5 (Sensor RXD)** | **GPIO 43 (D6 / TX)** | ESP32 phát lệnh sang cảm biến |
| **Pin 6 (GND)** | **GND** | Mass chung |

---

## 4. File in 3D vỏ thiết bị (3D Printed Case)

Tất cả các file thiết kế 3D hoàn chỉnh nằm trong thư mục [`docs/hardware/case/`](hardware/case/):

- **File STL (sẵn sàng đưa vào phần mềm Slicer):**
  - `case_top.stl`: Nắp trên có lỗ gắn vừa khít vòng cảm biến tròn SW111 / R503.
  - `case_bottom.stl`: Thân dưới giữ bo mạch ESP32-S3 SuperMini với khe cắm Type-C.
- **File STEP (chỉnh sửa kích thước trên Fusion 360 / SolidWorks):**
  - `tinytouch-supermini-v1-bottom.step` & `tinytouch-supermini-v2-bottom.step`
  - `tinytouch-v1-top.step` & `tinytouch-v2-top.step`
  - `tinytouch-xiao-v1-bottom.step`

### Cài đặt in 3D khuyến nghị:
- **Độ dày lớp in (Layer Height):** `0.12mm` - `0.16mm` (hoặc in máy SLA/Resin để bề mặt mịn nhất).
- **Infill:** `40%` - `50%`.
- **Vật liệu:** Nhựa PLA+ hoặc PETG chịu nhiệt tốt.
