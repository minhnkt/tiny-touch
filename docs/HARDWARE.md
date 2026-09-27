# tinyTouch Hardware & Wiring Guide

Hướng dẫn chi tiết về phần cứng, sơ đồ nối dây (Pinout), linh kiện và file in 3D vỏ bọc cho thiết bị **tinyTouch**.

---

## 1. Danh sách linh kiện (Bill of Materials - BOM)

1. **Vi điều khiển chính:**
   - **ESP32-S3 SuperMini** (Khuyến nghị vì giá rẻ, kích thước chỉ `22.5 x 18 mm`, có sẵn cổng Type-C Native USB).
   - Hoặc **Seeed Studio XIAO ESP32-S3** (Kích thước siêu nhỏ `21 x 17.5 mm`).
2. **Cảm biến vân tay UART:**
   - **Grow R503** (Cảm biến điện dung tròn, có vòng LED RGB hiển thị trạng thái, điện áp hoạt động `3.3V`).
   - Hoặc **Grow R502-A** (Kích thước tương đương, đầu nối 6 chân chuẩn `MX 1.0mm`).
3. **Vỏ bảo vệ (Case):**
   - In 3D bằng vật liệu PLA, PETG hoặc Resin (file thiết kế sẵn trong thư mục `docs/hardware/case/`).

---

## 2. Sơ đồ nối dây (Pinout Diagram)

### 2.1. Cấu hình tiêu chuẩn: ESP32-S3 SuperMini & Grow R503

Cáp cảm biến R503 thường có 6 dây màu chuẩn:

| Màu dây R503 | Tín hiệu cảm biến | Chân ESP32-S3 SuperMini | Chức năng |
|:-------------|:------------------|:------------------------|:----------|
| **Đỏ (Red)** | `VCC`             | **3.3V**                | Cấp nguồn cho module cảm biến |
| **Đen (Black)** | `GND`           | **GND**                 | Mass chung |
| **Vàng (Yellow)** | `RX`          | **GPIO 43** (TX)        | ESP32 gửi lệnh điều khiển sang cảm biến |
| **Xanh lá (Green)** | `TX`        | **GPIO 44** (RX)        | Cảm biến gửi dữ liệu và phản hồi về ESP32 |
| **Xanh dương (Blue)** | `WAKEUP`  | **GPIO 5** (Tùy chọn)   | Tín hiệu ngắt báo chạm ngón tay (Touch Sense) |
| **Trắng (White)** | `Touch Power` | **3.3V** (Tùy chọn)     | Nguồn cấp cho vòng cảm ứng chạm |

> **Mẹo lắp ráp:** Nếu không sử dụng tính năng đánh thức khi chạm (`Touch WAKEUP`), bạn chỉ cần hàn 4 dây cơ bản: **Đỏ (3.3V)**, **Đen (GND)**, **Vàng (GPIO 43)** và **Xanh lá (GPIO 44)**.

---

### 2.2. Cấu hình Seeed Studio XIAO ESP32-S3

| Màu dây R503 | Tín hiệu cảm biến | Chân XIAO ESP32-S3 | Ghi chú |
|:-------------|:------------------|:-------------------|:--------|
| **Đỏ**       | `VCC`             | **3V3**            | Nguồn 3.3V |
| **Đen**      | `GND`             | **GND**            | Mass chung |
| **Vàng**     | `RX`              | **GPIO 43 (D6/TX)** | UART TX |
| **Xanh lá**  | `TX`              | **GPIO 44 (D7/RX)** | UART RX |
| **Xanh dương** | `WAKEUP`        | **GPIO 1 (D0)**    | Ngắt chạm ngón tay |
| **Trắng**    | `Touch Power`     | **3V3**            | Cấp nguồn cảm ứng |

---

## 3. File in 3D vỏ thiết bị (3D Printed Case)

Tất cả các file thiết kế 3D hoàn chỉnh nằm trong thư mục [`docs/hardware/case/`](hardware/case/):

- **Định dạng STL (sẵn sàng đưa vào slicer in):**
  - `case_top.stl`: Nắp trên có lỗ tròn gắn khít vòng cảm biến R503.
  - `case_bottom.stl`: Thân dưới chứa board mạch ESP32-S3 và khe cắm cổng Type-C.
- **Định dạng STEP (dành cho chỉnh sửa trên Fusion 360 / SolidWorks):**
  - `tinytouch-supermini-v1-bottom.step` & `tinytouch-supermini-v2-bottom.step`
  - `tinytouch-v1-top.step` & `tinytouch-v2-top.step`
  - `tinytouch-xiao-v1-bottom.step`

### Thông số in 3D khuyến nghị:
- **Công nghệ in:** FDM hoặc SLA/Resin (Resin cho độ nét và bề mặt mịn nhất).
- **Độ dày lớp in (Layer Height):** `0.12mm` đến `0.16mm`.
- **Mật độ lấp đầy (Infill):** `30%` đến `50%`.
- **Vật liệu:** PLA+ hoặc PETG (chống co ngót tốt).
