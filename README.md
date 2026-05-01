
#  Embedded System Smart Lock - STM32 & Python

![C](https://img.shields.io/badge/Language-C-blue.svg)
![Python](https://img.shields.io/badge/Language-Python-yellow.svg)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-orange.svg)
![Flet](https://img.shields.io/badge/UI-Flet-00d2b4.svg)
![MySQL](https://img.shields.io/badge/Database-MySQL-4479A1.svg)

Dự án Hệ thống Khóa cửa Thông minh (Smart Lock) toàn diện, kết hợp giữa phần cứng nhúng thời gian thực sử dụng vi điều khiển **STM32F103** và phần mềm quản lý trên máy tính (Desktop App) được xây dựng bằng **Python (Flet)**.

## 🌟 Các tính năng nổi bật

### 🛠 Phần cứng (STM32 + FreeRTOS)
- **Bảo mật đa lớp:** Hỗ trợ mở khóa bằng **Vân tay** (Cảm biến AS608) và **Mã PIN** (Bàn phím ma trận 4x4).
- **Hệ điều hành thời gian thực (RTOS):** Quản lý đa tác vụ với FreeRTOS, đảm bảo hệ thống phản hồi cực nhanh và ổn định mà không bị gián đoạn.
- **Giao tiếp trực quan:** Hiển thị thông báo, trạng thái cửa trực tiếp qua màn hình LCD I2C.
- **Cơ chế Auto-lock:** Tự động khóa cửa sau một khoảng thời gian mở khóa an toàn.
- **Bảo mật mã PIN:** Mã PIN gốc được mã hóa và lưu trữ an toàn ngay trong bộ nhớ Flash của STM32, không lo mất dữ liệu khi mất điện.

### 💻 Phần mềm Quản lý (Desktop App)
- **Giao diện Glassmorphism:** Giao diện quản lý cực kỳ hiện đại, mượt mà và trực quan.
- **Mở khóa từ xa:** Cho phép Admin mở khóa cửa trực tiếp từ màn hình máy tính thông qua WebSocket.
- **Quản lý người dùng (CRUD):** Dễ dàng thêm vân tay mới, xóa vân tay từ xa thông qua ứng dụng mà không cần thao tác vật lý phức tạp trên khóa.
- **Lưu trữ Lịch sử ra vào:** Mọi hoạt động mở cửa (bằng vân tay nào, mã PIN, hoặc mở từ xa) đều được lưu trữ vào cơ sở dữ liệu **MySQL** và có thể trích xuất xem lại ngay trên App.
- **Đổi mã PIN từ xa:** Quản trị viên có thể đổi mã PIN của cửa nhanh chóng qua giao diện phần mềm.

---

## 🏗 Kiến trúc Hệ thống

Hệ thống được chia thành 2 phân hệ chính hoạt động song song và giao tiếp với nhau qua UART & WebSocket:

1. **`HTN_stm32/` (Phần cứng - C/C++)**: Chứa mã nguồn cho vi điều khiển STM32. Bao gồm các Drivers cho Cảm biến vân tay, Keypad, LCD, Servo và logic điều khiển chính chạy trên nền FreeRTOS.
2. **`software app server/` (Phần mềm - Python)**: 
   - `server.py`: Đóng vai trò là cầu nối (Bridge) và Backend, xử lý kết nối MySQL và lắng nghe WebSockets.
   - `app.py`: Giao diện người dùng (Client) được xây dựng bằng Flet.

---

## 🔌 Yêu cầu Phần cứng

- Vi điều khiển **STM32F103C8T6** (Bluepill)
- Cảm biến vân tay quang học **AS608**
- Màn hình **LCD 16x2** (kèm Module I2C)
- Bàn phím ma trận (Matrix Keypad 4x4)
- Động cơ **Servo SG90** / MG996R (Chốt khóa)
- Còi chip (Buzzer) báo hiệu
- Module chuyển đổi USB to TTL (CH340/CP2102) để giao tiếp UART với máy tính.

---

## ⚙️ Hướng dẫn Cài đặt & Sử dụng

### 1. Nạp code cho STM32
1. Mở thư mục `HTN_stm32/` bằng **STM32CubeIDE**.
2. Build Project (`Project` -> `Build All`).
3. Sử dụng ST-Link để nạp chương trình xuống board STM32.

### 2. Cài đặt Cơ sở dữ liệu (MySQL)
1. Cài đặt **MySQL Server**.
2. Tạo một Database có tên `smartlock_db`.
3. Tạo bảng `users`, `access_logs` và `app_accounts` theo cấu trúc hệ thống.
4. (Hoặc chạy file script SQL nếu có sẵn trong tương lai).

### 3. Chạy Phần mềm Quản lý (Desktop App)
Mở Terminal tại thư mục `software app server/app/` và chạy các lệnh sau:

```bash
# Cài đặt các thư viện Python cần thiết
pip install flet websockets mysql-connector-python

# Bật Server Backend (Bắt buộc chạy trước)
python server.py

# Bật Giao diện App quản lý
python app.py

