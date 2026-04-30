import asyncio
import websockets
import mysql.connector
import json
import datetime

connected_clients = set()

# --- HÀM 1: GHI LỊCH SỬ ---
def log_to_db(method, fingerprint_id=None):
    try:
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        
        user_id = None
        # Nếu có ID vân tay, tra cứu ID của bảng users
        if fingerprint_id is not None:
            cursor.execute("SELECT id FROM users WHERE fingerprint_id = %s", (fingerprint_id,))
            result = cursor.fetchone()
            if result:
                user_id = result[0]
                
        sql = "INSERT INTO access_logs (user_id, method) VALUES (%s, %s)"
        val = (user_id, method)
        cursor.execute(sql, val)
        mydb.commit()
        cursor.close()
        mydb.close()
    except Exception as e:
        print(f"[DB Error] Lỗi lưu lịch sử: {e}")

# --- HÀM 2: LẤY LỊCH SỬ CHO APP ---
def get_logs_from_db():
    try:
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
        cursor.execute(sql)
        results = cursor.fetchall()
        
        for row in results:
            row['opened_at'] = row['opened_at'].strftime("%Y-%m-%d %H:%M:%S")
            if row['full_name'] is None:
                row['full_name'] = "Không xác định"
            
        cursor.close()
        mydb.close()
        return json.dumps({"type": "logs_data", "data": results})
    except Exception as e:
        print(f"[DB Error] Lỗi đọc database: {e}")
        return json.dumps({"type": "logs_data", "data": []})

# --- HÀM 3: QUẢN LÝ NGƯỜI DÙNG (THÊM/XÓA) ---
def manage_user(action, fingerprint_id, full_name=""):
    try:
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        if action == "ENROLL":
            # Lưu hoặc cập nhật người dùng. ON DUPLICATE KEY giúp ghi đè nếu ID đã tồn tại
            sql = "INSERT INTO users (fingerprint_id, full_name) VALUES (%s, %s) ON DUPLICATE KEY UPDATE full_name=%s"
            cursor.execute(sql, (fingerprint_id, full_name, full_name))
        elif action == "DELETE":
            # Xóa người dùng khỏi CSDL
            cursor.execute("DELETE FROM users WHERE fingerprint_id = %s", (fingerprint_id,))
        mydb.commit()
        cursor.close()
        mydb.close()
    except Exception as e:
        print(f"[DB Error] Lỗi quản lý User: {e}")

# --- HÀM 4: LẤY DANH SÁCH NGƯỜI DÙNG ---
def get_users_from_db():
    try:
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor(dictionary=True)
        sql = "SELECT fingerprint_id, full_name FROM users ORDER BY fingerprint_id ASC"
        cursor.execute(sql)
        results = cursor.fetchall()
        cursor.close()
        mydb.close()
        return json.dumps({"type": "users_data", "data": results})
    except Exception as e:
        print(f"[DB Error] Lỗi đọc users: {e}")
        return json.dumps({"type": "users_data", "data": []})

# --- HÀM 5: ĐĂNG NHẬP / ĐĂNG KÝ ---
def verify_login(username, password):
    try:
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        cursor.execute("SELECT * FROM app_accounts WHERE username=%s AND password=%s", (username, password))
        result = cursor.fetchone()
        cursor.close()
        mydb.close()
        return result is not None
    except Exception as e:
        print(f"[DB Error] Lỗi đăng nhập: {e}")
        return False

def register_account(username, password):
    try:
        mydb = mysql.connector.connect(
            host="localhost", user="root", password="Binh2004@", database="smartlock_db"
        )
        cursor = mydb.cursor()
        cursor.execute("SELECT * FROM app_accounts WHERE username=%s", (username,))
        if cursor.fetchone():
            cursor.close()
            mydb.close()
            return False # Đã tồn tại
        cursor.execute("INSERT INTO app_accounts (username, password) VALUES (%s, %s)", (username, password))
        mydb.commit()
        cursor.close()
        mydb.close()
        return True
    except Exception as e:
        print(f"[DB Error] Lỗi đăng ký: {e}")
        return False

async def handler(websocket, *args, **kwargs):
    client_ip = websocket.remote_address[0]
    print(f"[+] Client kết nối: {client_ip}")
    connected_clients.add(websocket)
    
    try:
        async for message in websocket:
            print(f"[Nhận] {message}")
            
            # --- 0. LOGIN / REGISTER ---
            if message.startswith("WEB_CMD:LOGIN:"):
                parts = message.split(":")
                if len(parts) >= 4:
                    user = parts[2]
                    pwd = parts[3]
                    if verify_login(user, pwd):
                        await websocket.send("LOGIN_SUCCESS")
                    else:
                        await websocket.send("LOGIN_FAIL")
                continue
                
            if message.startswith("WEB_CMD:REGISTER:"):
                parts = message.split(":")
                if len(parts) >= 4:
                    user = parts[2]
                    pwd = parts[3]
                    if register_account(user, pwd):
                        await websocket.send("REGISTER_SUCCESS")
                    else:
                        await websocket.send("REGISTER_FAIL")
                continue

            # --- 1. APP YÊU CẦU LỊCH SỬ ---
            if message == "GET_LOGS":
                logs_json = get_logs_from_db()
                await websocket.send(logs_json)
                continue
                
            # --- 1.5 APP YÊU CẦU DANH SÁCH USER ---
            if message == "GET_USERS":
                users_json = get_users_from_db()
                await websocket.send(users_json)
                continue
                
            # --- 2. APP RA LỆNH THÊM VÂN TAY (VD: WEB_CMD:ENROLL_NAME:5:Bo) ---
            if message.startswith("WEB_CMD:ENROLL_NAME:"):
                parts = message.split(":")
                if len(parts) >= 4:
                    f_id = int(parts[2])
                    f_name = parts[3]
                    manage_user("ENROLL", f_id, f_name) # Lưu DB trước
                    
                    # Phát lệnh xuống ESP32 -> STM32 để khởi động cảm biến
                    hw_cmd = f"WEB_CMD:ENROLL:{f_id}"
                    for client in list(connected_clients):
                        if client != websocket:
                            try: await client.send(hw_cmd)
                            except Exception: pass
                continue
                
            # --- 3. APP RA LỆNH XÓA VÂN TAY (VD: WEB_CMD:DELETE_ID:5) ---
            if message.startswith("WEB_CMD:DELETE_ID:"):
                parts = message.split(":")
                if len(parts) >= 3:
                    f_id = int(parts[2])
                    manage_user("DELETE", f_id) # Xóa khỏi DB
                    
                    hw_cmd = f"WEB_CMD:DELETE:{f_id}"
                    for client in list(connected_clients):
                        if client != websocket:
                            try: await client.send(hw_cmd)
                            except Exception: pass
                continue
                
            # --- 4. BẮT SỰ KIỆN MỞ CỬA TỪ STM32 ---
            if message.startswith("DOOR_OPENED:FINGER:"):
                f_id = int(message.split(":")[2])
                log_to_db("Vân tay", fingerprint_id=f_id)
            elif message.startswith("DOOR_OPENED:PIN"):
                log_to_db("Bàn phím Keypad")
            elif message == "WEB_CMD:UNLOCK":
                log_to_db("Mở từ xa qua App")
            
            # BROADCAST CHO TẤT CẢ CÁC THÔNG ĐIỆP CÒN LẠI (Unlock, Báo cáo thành công...)
            for client in list(connected_clients):
                if client != websocket:
                    try:
                        await client.send(message)
                    except Exception:
                        pass
                    
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print(f"[-] Client ngắt kết nối: {client_ip}")
        connected_clients.remove(websocket)

async def main():
    server = await websockets.serve(handler, "0.0.0.0", 3030)
    print("WebSocket Server đang chạy...")
    await server.wait_closed()

if __name__ == "__main__":
    asyncio.run(main())
