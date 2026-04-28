import flet as ft
import websockets
import asyncio
import json

async def main(page: ft.Page):
    # Cấu hình giao diện
    page.title = "Smart Lock App"
    page.theme_mode = ft.ThemeMode.DARK
    page.horizontal_alignment = ft.CrossAxisAlignment.CENTER
    page.vertical_alignment = ft.MainAxisAlignment.CENTER
    page.window_width = 450
    page.window_height = 800

    title_text = ft.Text("ĐIỀU KHIỂN KHÓA", size=30, weight=ft.FontWeight.BOLD, color=ft.Colors.BLUE)
    status_text = ft.Text("Đang kết nối Server...", size=16, color=ft.Colors.YELLOW)
    lock_icon = ft.Icon(icon=ft.Icons.LOCK, size=150, color=ft.Colors.RED)

    outgoing_queue = asyncio.Queue()

    # --- POPUP 1: LỊCH SỬ MỞ CỬA ---
    logs_table = ft.DataTable(
        columns=[
            ft.DataColumn(ft.Text("Thời gian")),
            ft.DataColumn(ft.Text("Người mở")),
            ft.DataColumn(ft.Text("Cách mở")),
        ],
        rows=[],
    )
    def close_logs_dialog(e):
        page.pop_dialog()

    logs_dialog = ft.AlertDialog(
        title=ft.Text("Lịch Sử Mở Cửa", weight=ft.FontWeight.BOLD),
        content=ft.Container(content=ft.Column([logs_table], scroll=ft.ScrollMode.AUTO), height=400, width=320),
        actions=[ft.TextButton("Đóng", on_click=close_logs_dialog)],
    )

    # --- POPUP 2: QUẢN LÝ VÂN TAY ---
    name_input = ft.TextField(label="Tên (VD: Bố)", expand=True)
    id_input = ft.TextField(label="ID Vân tay", width=110, keyboard_type=ft.KeyboardType.NUMBER)
    enroll_status = ft.Text("Điền Tên và ID để thao tác", color=ft.Colors.GREY)
    
    users_table = ft.DataTable(
        columns=[
            ft.DataColumn(ft.Text("ID")),
            ft.DataColumn(ft.Text("Tên Người Dùng")),
        ],
        rows=[],
    )

    async def btn_enroll_clicked(e):
        if not name_input.value or not id_input.value:
            enroll_status.value = "Vui lòng nhập đủ Tên và ID!"
            enroll_status.color = ft.Colors.RED
            page.update()
            return
        
        enroll_status.value = "Đang chờ đặt tay lên cửa..."
        enroll_status.color = ft.Colors.YELLOW
        page.update()
        
        # Ra lệnh Đăng ký
        await outgoing_queue.put(f"WEB_CMD:ENROLL_NAME:{id_input.value}:{name_input.value}")

    async def btn_delete_clicked(e):
        if not id_input.value:
            enroll_status.value = "Vui lòng nhập ID cần xóa!"
            enroll_status.color = ft.Colors.RED
            page.update()
            return
            
        # Ra lệnh Xóa
        await outgoing_queue.put(f"WEB_CMD:DELETE_ID:{id_input.value}")
        enroll_status.value = f"Đã gửi lệnh xóa ID {id_input.value}!"
        enroll_status.color = ft.Colors.GREEN
        page.update()

    def close_mgmt_dialog(e):
        page.pop_dialog()

    mgmt_dialog = ft.AlertDialog(
        title=ft.Text("Quản lý Người Dùng", weight=ft.FontWeight.BOLD),
        content=ft.Container(
            content=ft.Column([
                ft.Row([name_input, id_input]),
                ft.Container(height=10),
                ft.Row([
                    ft.ElevatedButton("Thêm", icon=ft.Icons.FINGERPRINT, bgcolor=ft.Colors.GREEN_700, color=ft.Colors.WHITE, on_click=btn_enroll_clicked),
                    ft.ElevatedButton("Xóa", icon=ft.Icons.DELETE, bgcolor=ft.Colors.RED_700, color=ft.Colors.WHITE, on_click=btn_delete_clicked),
                ], alignment=ft.MainAxisAlignment.SPACE_BETWEEN),
                ft.Container(height=10),
                enroll_status,
                ft.Divider(),
                ft.Text("Danh sách đã lưu trong Hệ thống:", weight=ft.FontWeight.BOLD),
                ft.Container(content=ft.Column([users_table], scroll=ft.ScrollMode.AUTO), height=150)
            ], tight=True),
            width=320
        ),
        actions=[ft.TextButton("Đóng", on_click=close_mgmt_dialog)],
    )

    # --- KẾT NỐI WEBSOCKET ---
    async def connect_server():
        uri = "ws://192.168.137.1:3030" 
        try:
            async with websockets.connect(uri) as ws:
                status_text.value = "Đã kết nối với Khóa!"
                status_text.color = ft.Colors.GREEN
                page.update()
                
                async def listen_messages():
                    async for msg in ws:
                        # 1. Nhận dữ liệu Lịch sử
                        if "logs_data" in msg:
                            parsed_msg = json.loads(msg)
                            logs_table.rows.clear()
                            for row in parsed_msg["data"]:
                                logs_table.rows.append(
                                    ft.DataRow(cells=[
                                        ft.DataCell(ft.Text(row["opened_at"])),
                                        ft.DataCell(ft.Text(row["full_name"], color=ft.Colors.GREEN_400)),
                                        ft.DataCell(ft.Text(row["method"])),
                                    ])
                                )
                            page.show_dialog(logs_dialog)
                            status_text.value = "Tải lịch sử thành công!"
                            page.update()
                            
                        # 1.5 Nhận danh sách người dùng
                        elif "users_data" in msg:
                            parsed_msg = json.loads(msg)
                            users_table.rows.clear()
                            for row in parsed_msg["data"]:
                                users_table.rows.append(
                                    ft.DataRow(cells=[
                                        ft.DataCell(ft.Text(str(row["fingerprint_id"]))),
                                        ft.DataCell(ft.Text(row["full_name"], color=ft.Colors.BLUE_400)),
                                    ])
                                )
                            page.update()
                            
                        # 2. STM32 báo Đăng ký thành công
                        elif "ENROLL_SUCCESS" in msg:
                            f_id = msg.split(":")[1].strip()
                            enroll_status.value = f"Tuyệt vời! Đã đăng ký thành công ID {f_id}!"
                            enroll_status.color = ft.Colors.GREEN
                            page.update()
                            
                        # 3. STM32 báo Mở cửa thành công
                        elif "DOOR_OPENED" in msg:
                            lock_icon.icon = ft.Icons.LOCK_OPEN
                            lock_icon.color = ft.Colors.GREEN
                            
                            if "FINGER" in msg:
                                f_id = msg.split(":")[2].strip()
                                status_text.value = f"CỬA ĐANG MỞ (Vân tay số {f_id})"
                            elif "PIN" in msg:
                                status_text.value = "CỬA ĐANG MỞ (Mã PIN)"
                            else:
                                status_text.value = "CỬA ĐANG MỞ"
                                
                            page.update()
                            
                            await asyncio.sleep(5)
                            lock_icon.icon = ft.Icons.LOCK
                            lock_icon.color = ft.Colors.RED
                            status_text.value = "SẴN SÀNG"
                            page.update()

                async def send_messages():
                    while True:
                        msg_to_send = await outgoing_queue.get()
                        await ws.send(msg_to_send)

                await asyncio.gather(listen_messages(), send_messages())

        except Exception as e:
            status_text.value = f"Mất kết nối: Chờ tải lại..."
            status_text.color = ft.Colors.RED
            page.update()
            await asyncio.sleep(3)
            await connect_server() 

    # --- POPUP 3: XÁC THỰC ADMIN ---
    admin_pwd_input = ft.TextField(label="Mật khẩu Admin", password=True, can_reveal_password=True, width=250)
    admin_error = ft.Text("", color=ft.Colors.RED)

    async def verify_admin(e):
        if admin_pwd_input.value == "admin123":
            admin_pwd_input.value = ""
            admin_error.value = ""
            page.pop_dialog() # Đóng popup mật khẩu
            
            # Mở popup Quản lý
            enroll_status.value = "Điền Tên và ID để thao tác"
            enroll_status.color = ft.Colors.GREY
            name_input.value = ""
            id_input.value = ""
            await outgoing_queue.put("GET_USERS") # Yêu cầu tải danh sách
            page.show_dialog(mgmt_dialog)
        else:
            admin_error.value = "Mật khẩu không chính xác!"
            page.update()

    def close_auth_dialog(e):
        page.pop_dialog()

    auth_dialog = ft.AlertDialog(
        title=ft.Text("Xác thực Quyền Admin", weight=ft.FontWeight.BOLD),
        content=ft.Container(content=ft.Column([admin_pwd_input, admin_error], tight=True), width=300),
        actions=[
            ft.TextButton("Hủy", on_click=close_auth_dialog),
            ft.TextButton("Xác nhận", on_click=verify_admin)
        ],
    )

    # --- SỰ KIỆN NÚT BẤM ---
    async def unlock_clicked(e):
        await outgoing_queue.put("WEB_CMD:UNLOCK")
        status_text.value = "Đang truyền lệnh mở..."
        page.update()
        
    async def view_logs_clicked(e):
        await outgoing_queue.put("GET_LOGS")
        status_text.value = "Đang tải lịch sử..."
        page.update()
        
    async def manage_clicked(e):
        # Thay vì mở thẳng Mgmt Dialog, ta mở Auth Dialog trước
        admin_pwd_input.value = ""
        admin_error.value = ""
        page.show_dialog(auth_dialog)

    # --- XUẤT GIAO DIỆN CHÍNH ---
    unlock_btn = ft.Container(
        content=ft.Row([ft.Icon(icon=ft.Icons.KEY, color=ft.Colors.WHITE), ft.Text("MỞ KHÓA TỪ XA", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=250, height=60, bgcolor=ft.Colors.BLUE_700, border_radius=30, ink=True, on_click=unlock_clicked
    )
    logs_btn = ft.Container(
        content=ft.Row([ft.Icon(icon=ft.Icons.HISTORY, color=ft.Colors.WHITE), ft.Text("XEM LỊCH SỬ", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=250, height=50, bgcolor=ft.Colors.GREEN_700, border_radius=25, ink=True, on_click=view_logs_clicked
    )
    mgmt_btn = ft.Container(
        content=ft.Row([ft.Icon(icon=ft.Icons.PEOPLE, color=ft.Colors.WHITE), ft.Text("QUẢN LÝ VÂN TAY", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=250, height=50, bgcolor=ft.Colors.ORANGE_700, border_radius=25, ink=True, on_click=manage_clicked
    )

    page.add(
        title_text, ft.Container(height=30), lock_icon, ft.Container(height=10),
        status_text, ft.Container(height=30), unlock_btn, ft.Container(height=15),
        logs_btn, ft.Container(height=15), mgmt_btn
    )

    asyncio.create_task(connect_server())

if __name__ == "__main__":
    ft.run(main)
