import asyncio
import websockets
import mysql.connector
import json
import datetime

connected_clients = set()   # Tập hợp các client kết nối

# HÀM 1: GHI LỊCH SỬ
def log_to_db(method, fingerprint_id=None):  # Hàm ghi lịch sử
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect( 
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        
        user_id = None
            # Nếu có ID vân tay, tra cứu ID của bảng users
        if fingerprint_id is not None:  # Nếu có ID vân tay
            cursor.execute("SELECT id FROM users WHERE fingerprint_id = %s", (fingerprint_id,))
            result = cursor.fetchone()
            if result:
                user_id = result[0]
                
        sql = "INSERT INTO access_logs (user_id, method) VALUES (%s, %s)"
        val = (user_id, method)
        cursor.execute(sql, val)    # Thực thi lệnh SQL
        mydb.commit()   # Commit database
        cursor.close()  # Đóng cursor
        mydb.close()    # Đóng database
    except Exception as e:  # Lỗi lưu lịch sử
        print(f"[DB Error] Lỗi lưu lịch sử: {e}")

# LẤY LỊCH SỬ CHO APP
def get_logs_from_db():  # Hàm lấy lịch sử
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor(dictionary=True)
        sql = """
            SELECT a.id, a.method, a.opened_at, u.full_name 
            FROM access_logs a 
            LEFT JOIN users u ON a.user_id = u.id 
            ORDER BY a.opened_at DESC LIMIT 20
        """
        cursor.execute(sql)    # Thực thi lệnh SQL
        results = cursor.fetchall()    # Lấy kết quả
        
        for row in results:    # Duyệt kết quả
            row['opened_at'] = row['opened_at'].strftime("%Y-%m-%d %H:%M:%S")
            if row['full_name'] is None:    # Nếu không có tên
                row['full_name'] = "Không xác định"
            
        cursor.close()  # Đóng cursor
        mydb.close()    # Đóng database
        return json.dumps({"type": "logs_data", "data": results})
    except Exception as e:    # Lỗi đọc database
        print(f"[DB Error] Lỗi đọc database: {e}")
        return json.dumps({"type": "logs_data", "data": []})

# HÀM 3: QUẢN LÝ NGƯỜI DÙNG (THÊM/XÓA)
def manage_user(action, fingerprint_id, full_name=""):  # Hàm quản lý người dùng
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect(   # Kết nối database
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        if action == "ENROLL":  # Thêm người dùng
            # Lưu hoặc cập nhật người dùng. ON DUPLICATE KEY giúp ghi đè nếu ID đã tồn tại
            sql = "INSERT INTO users (fingerprint_id, full_name) VALUES (%s, %s) ON DUPLICATE KEY UPDATE full_name=%s"
            cursor.execute(sql, (fingerprint_id, full_name, full_name)) # Thực thi lệnh SQL
        elif action == "DELETE":    
            # Xóa người dùng khỏi CSDL
            cursor.execute("DELETE FROM users WHERE fingerprint_id = %s", (fingerprint_id,))
        mydb.commit()  # Commit database
        cursor.close()  # Đóng cursor
        mydb.close()  # Đóng database
    except Exception as e:    # Lỗi quản lý người dùng
        print(f"[DB Error] Lỗi quản lý User: {e}")

# HÀM 4: LẤY DANH SÁCH NGƯỜI DÙNG
def get_users_from_db():  # Hàm lấy danh sách người dùng
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect(   # Kết nối database
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor(dictionary=True)
        sql = "SELECT fingerprint_id, full_name FROM users ORDER BY fingerprint_id ASC"
        cursor.execute(sql)  # Thực thi lệnh SQL
        results = cursor.fetchall() 
        cursor.close()  # Đóng cursor
        mydb.close()  # Đóng database
        return json.dumps({"type": "users_data", "data": results})
    except Exception as e:    # Lỗi đọc database
        print(f"[DB Error] Lỗi đọc users: {e}")
        return json.dumps({"type": "users_data", "data": []})

# HÀM 5: ĐĂNG NHẬP / ĐĂNG KÝ
def verify_login(username, password):  # Hàm xác nhận đăng nhập
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect(   # Kết nối database
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()  
        cursor.execute("SELECT * FROM app_accounts WHERE username=%s AND password=%s", (username, password))  # Thực thi lệnh SQL
        result = cursor.fetchone()  # Lấy kết quả
        cursor.close()  # Đóng cursor
        mydb.close()  # Đóng database
        return result is not None  # Trả về kết quả
    except Exception as e:  # Lỗi đăng nhập
        print(f"[DB Error] Lỗi đăng nhập: {e}")
        return False

def register_account(username, password):  # Hàm đăng ký tài khoản
    try:    # Hàm try-except để xử lý lỗi
        mydb = mysql.connector.connect(   # Kết nối database
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()  
        cursor.execute("SELECT * FROM app_accounts WHERE username=%s", (username,))  # Thực thi lệnh SQL
        if cursor.fetchone():   # Lấy kết quả
            cursor.close()  # Đóng cursor
            mydb.close()  # Đóng database
            return False # Đã tồn tại
        cursor.execute("INSERT INTO app_accounts (username, password) VALUES (%s, %s)", (username, password))
        mydb.commit()   # Commit database
        cursor.close()  # Đóng cursor
        mydb.close()  # Đóng database
        return True   # Trả về thành công
    except Exception as e:  # Lỗi đăng ký
        print(f"[DB Error] Lỗi đăng ký: {e}")
        return False    

async def handler(websocket, *args, **kwargs):  # Hàm xử lý kết nối WebSocket
    client_ip = websocket.remote_address[0]  # Lấy địa chỉ IP của client
    print(f"[+] Client kết nối: {client_ip}")  # In thông tin kết nối
    connected_clients.add(websocket)  # Thêm client vào danh sách
    
    try:
        async for message in websocket:  # Lấy tin nhắn từ client
            print(f"[Nhận] {message}")  # In tin nhắn
            
            # --- 0. LOGIN / REGISTER ---
            if message.startswith("WEB_CMD:LOGIN:"):  # Kiểm tra tin nhắn đăng nhập
                parts = message.split(":")  # Tách tin nhắn thành các phần
                if len(parts) >= 4:  # Kiểm tra số lượng phần
                    user = parts[2]  # Lấy tên người dùng
                    pwd = parts[3]  # Lấy mật khẩu
                    if verify_login(user, pwd):  # Kiểm tra đăng nhập
                        await websocket.send("LOGIN_SUCCESS")  # Gửi thông báo đăng nhập thành công
                    else:
                        await websocket.send("LOGIN_FAIL")  # Gửi thông báo đăng nhập thất bại
                continue
                
            if message.startswith("WEB_CMD:REGISTER:"):  # Kiểm tra tin nhắn đăng ký
                parts = message.split(":")  # Tách tin nhắn thành các phần
                if len(parts) >= 4:  # Kiểm tra số lượng phần
                    user = parts[2]  # Lấy tên người dùng
                    pwd = parts[3]  # Lấy mật khẩu
                    if register_account(user, pwd):  # Kiểm tra đăng ký
                        await websocket.send("REGISTER_SUCCESS")  # Gửi thông báo đăng ký thành công
                    else:
                        await websocket.send("REGISTER_FAIL")  # Gửi thông báo đăng ký thất bại
                continue

            # 1. APP YÊU CẦU LỊCH SỬ
            if message == "GET_LOGS":  # Kiểm tra tin nhắn yêu cầu lịch sử
                logs_json = get_logs_from_db()  # Lấy danh sách lịch sử
                await websocket.send(logs_json)  # Gửi danh sách lịch sử
                continue
                
            # 1.5 APP YÊU CẦU DANH SÁCH USER
            if message == "GET_USERS":  # Kiểm tra tin nhắn yêu cầu danh sách người dùng
                users_json = get_users_from_db()  # Lấy danh sách người dùng
                await websocket.send(users_json)  # Gửi danh sách người dùng
                continue
                
            # 2. APP RA LỆNH THÊM VÂN TAY (VD: WEB_CMD:ENROLL_NAME:5:Bo)
            if message.startswith("WEB_CMD:ENROLL_NAME:"):  # Kiểm tra tin nhắn ra lệnh thêm vân tay
                parts = message.split(":")  # Tách tin nhắn thành các phần
                if len(parts) >= 4:  # Kiểm tra số lượng phần
                    f_id = int(parts[2])  # Lấy ID vân tay
                    f_name = parts[3]  # Lấy tên vân tay
                    manage_user("ENROLL", f_id, f_name) # Lưu DB trước
                    
                    # Phát lệnh xuống ESP32 -> STM32 để khởi động cảm biến
                    hw_cmd = f"WEB_CMD:ENROLL:{f_id}"
                    for client in list(connected_clients):  # Lặp qua các client
                        if client != websocket:  # Nếu không phải client hiện tại
                            try: await client.send(hw_cmd)  # Gửi lệnh thêm vân tay
                            except Exception: pass  # Bỏ qua lỗi
                continue
                
            # 3. APP RA LỆNH XÓA VÂN TAY
            if message.startswith("WEB_CMD:DELETE_ID:"):  # Kiểm tra tin nhắn ra lệnh xóa vân tay
                parts = message.split(":")  # Tách tin nhắn thành các phần
                if len(parts) >= 3:  # Kiểm tra số lượng phần
                    f_id = int(parts[2])  # Lấy ID vân tay
                    manage_user("DELETE", f_id) # Xóa khỏi DB
                    
                    hw_cmd = f"WEB_CMD:DELETE:{f_id}"  # Tạo lệnh xóa vân tay
                    for client in list(connected_clients):  # Lặp qua các client
                        if client != websocket:  # Nếu không phải client hiện tại
                            try: await client.send(hw_cmd)  # Gửi lệnh xóa vân tay
                            except Exception: pass  # Bỏ qua lỗi
                continue
                
            # 4. BẮT SỰ KIỆN MỞ CỬA TỪ STM32
            if message.startswith("DOOR_OPENED:FINGER:"):  # Kiểm tra tin nhắn mở cửa bằng vân tay
                f_id = int(message.split(":")[2])  # Lấy ID vân tay
                log_to_db("Vân tay", fingerprint_id=f_id)  # Ghi vào database
            elif message.startswith("DOOR_OPENED:PIN"):  # Kiểm tra tin nhắn mở cửa bằng bàn phím Keypad
                log_to_db("Bàn phím Keypad")  # Ghi vào database
            elif message == "WEB_CMD:UNLOCK":  # Kiểm tra tin nhắn mở cửa từ xa qua App
                log_to_db("Mở từ xa qua App")  # Ghi vào database
            
            # BROADCAST CHO TẤT CẢ CÁC THÔNG ĐIỆP CÒN LẠI (Unlock, Báo cáo thành công...)
            for client in list(connected_clients):  # Lặp qua các client
                if client != websocket:  # Nếu không phải client hiện tại
                    try:
                        await client.send(message)  # Gửi tin nhắn
                    except Exception:
                        pass
                    
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print(f"[-] Client ngắt kết nối: {client_ip}")  # In thông tin ngắt kết nối
        connected_clients.remove(websocket)  # Xóa client khỏi danh sách

async def main():
    server = await websockets.serve(handler, "0.0.0.0", 3030)  # Tạo server WebSocket
    print("WebSocket Server đang chạy...")  # In thông báo server đang chạy
    await server.wait_closed()  # Đợi server đóng

if __name__ == "__main__":  # Kiểm tra nếu file được chạy trực tiếp
    asyncio.run(main())  # Chạy hàm main
