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

    title_text = ft.Text("SMART LOCK", size=25, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE)
    status_text = ft.Text("Đang kết nối Server...", size=16, color=ft.Colors.YELLOW)
    lock_icon = ft.Icon(icon=ft.Icons.LOCK, size=100, color=ft.Colors.RED)

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

    # --- POPUP 4: ĐỔI MÃ PIN ---
    new_pin_input = ft.TextField(label="Mã PIN mới (Chỉ số)", keyboard_type=ft.KeyboardType.NUMBER, width=250)

    async def change_pin_confirm(e):
        if not new_pin_input.value.isdigit() or len(new_pin_input.value) > 9:
            status_text.value = "Lỗi: Mã PIN phải là số và < 10 ký tự!"
            status_text.color = ft.Colors.RED
            page.pop_dialog()
            page.update()
            return
        pin = new_pin_input.value
        await outgoing_queue.put(f"WEB_CMD:CHANGE_PIN:{pin}")
        new_pin_input.value = ""
        page.pop_dialog()
        status_text.value = f"Đã gửi lệnh đổi mã PIN mới!"
        status_text.color = ft.Colors.GREEN
        page.update()

    def close_change_pin_dialog(e):
        page.pop_dialog()

    change_pin_dialog = ft.AlertDialog(
        title=ft.Text("Đổi Mã PIN Cửa", weight=ft.FontWeight.BOLD),
        content=ft.Container(content=ft.Column([new_pin_input], tight=True), width=300),
        actions=[
            ft.TextButton("Hủy", on_click=close_change_pin_dialog),
            ft.TextButton("Xác nhận", on_click=change_pin_confirm)
        ],
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
                        # 0. Kết quả đăng nhập
                        if msg == "LOGIN_SUCCESS":
                            print("=== NHẬN LOGIN SUCCESS ===")
                            try:
                                login_view.visible = False
                                main_view.visible = True
                                page.update()
                                print("=== CẬP NHẬT GIAO DIỆN CHÍNH THÀNH CÔNG ===")
                            except Exception as e:
                                print(f"=== LỖI CẬP NHẬT GIAO DIỆN: {e} ===")
                            continue
                        elif msg == "LOGIN_FAIL":
                            login_error_text.value = "Tài khoản hoặc mật khẩu sai!"
                            login_error_text.color = ft.Colors.RED
                            page.update()
                            continue
                        elif msg == "REGISTER_SUCCESS":
                            page.pop_dialog()
                            login_error_text.value = "Đăng ký thành công! Hãy bấm Đăng Nhập."
                            login_error_text.color = ft.Colors.GREEN
                            page.update()
                            continue
                        elif msg == "REGISTER_FAIL":
                            reg_error_text.value = "Tài khoản đã tồn tại!"
                            reg_error_text.color = ft.Colors.RED
                            page.update()
                            continue

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
            import traceback
            traceback.print_exc()
            try:
                status_text.value = f"Mất kết nối: Chờ tải lại..."
                status_text.color = ft.Colors.RED
                page.update()
            except:
                pass
            await asyncio.sleep(3)
            await connect_server() 

    # --- GLASSMORPHISM HELPER ---
    def GlassContainer(content, width=None, height=None, padding=15, on_click=None, border_radius=25, bgcolor=None):
        if bgcolor is None:
            bgcolor = ft.Colors.with_opacity(0.1, ft.Colors.WHITE)
        return ft.Container(
            content=content, width=width, height=height, padding=padding, border_radius=border_radius,
            bgcolor=bgcolor, border=ft.border.all(1, ft.Colors.with_opacity(0.2, ft.Colors.WHITE)),
            blur=15, on_click=on_click, ink=True if on_click else False, alignment=ft.Alignment(0, 0)
        )

    # --- VIEWS (MÀN HÌNH) ---
    login_view = ft.Column(visible=True, horizontal_alignment=ft.CrossAxisAlignment.CENTER)
    main_view = ft.Column(visible=False, horizontal_alignment=ft.CrossAxisAlignment.CENTER)

    # --- 1. MÀN HÌNH ĐĂNG NHẬP ---
    login_user_input = ft.TextField(
        label="Tên đăng nhập", width=250,
        border_color=ft.Colors.WHITE70, color=ft.Colors.WHITE, label_style=ft.TextStyle(color=ft.Colors.WHITE70)
    )
    login_pwd_input = ft.TextField(
        label="Mật khẩu", password=True, can_reveal_password=True, width=250,
        border_color=ft.Colors.WHITE70, color=ft.Colors.WHITE, label_style=ft.TextStyle(color=ft.Colors.WHITE70)
    )
    login_error_text = ft.Text("", color=ft.Colors.RED)

    async def btn_login_clicked(e):
        try:
            u = login_user_input.value
            p = login_pwd_input.value
            if u and p:
                await outgoing_queue.put(f"WEB_CMD:LOGIN:{u}:{p}")
                login_error_text.value = "Đang kiểm tra..."
                login_error_text.color = ft.Colors.YELLOW
                page.update()
        except Exception as e:
            import traceback
            traceback.print_exc()
            login_error_text.value = f"Lỗi: {e}"
            page.update()

    # --- DIALOG ĐĂNG KÝ ---
    reg_user_input = ft.TextField(label="Tên đăng nhập mới", width=250)
    reg_pwd_input = ft.TextField(label="Mật khẩu", password=True, can_reveal_password=True, width=250)
    reg_pwd2_input = ft.TextField(label="Nhập lại mật khẩu", password=True, can_reveal_password=True, width=250)
    reg_error_text = ft.Text("", color=ft.Colors.RED)

    async def confirm_register(e):
        u = reg_user_input.value
        p1 = reg_pwd_input.value
        p2 = reg_pwd2_input.value
        if not u or not p1:
            reg_error_text.value = "Vui lòng nhập đủ thông tin!"
            page.update()
            return
        if p1 != p2:
            reg_error_text.value = "Mật khẩu không khớp!"
            page.update()
            return
            
        await outgoing_queue.put(f"WEB_CMD:REGISTER:{u}:{p1}")
        reg_error_text.value = "Đang gửi yêu cầu..."
        reg_error_text.color = ft.Colors.YELLOW
        page.update()

    def close_reg_dialog(e):
        page.pop_dialog()

    register_dialog = ft.AlertDialog(
        title=ft.Text("Đăng Ký Tài Khoản Mới", weight=ft.FontWeight.BOLD),
        content=ft.Container(content=ft.Column([reg_user_input, reg_pwd_input, reg_pwd2_input, reg_error_text], tight=True), width=300),
        actions=[
            ft.TextButton("Hủy", on_click=close_reg_dialog),
            ft.TextButton("Đăng Ký", on_click=confirm_register)
        ],
    )

    def btn_register_clicked(e):
        reg_user_input.value = ""
        reg_pwd_input.value = ""
        reg_pwd2_input.value = ""
        reg_error_text.value = ""
        page.show_dialog(register_dialog)

    login_view.controls = [
        GlassContainer(
            content=ft.Column([
                ft.Text("ĐĂNG NHẬP ADMIN", size=25, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE),
                ft.Container(height=10),
                ft.Icon(ft.Icons.SECURITY, size=80, color=ft.Colors.WHITE70),
                ft.Container(height=10),
                login_user_input,
                login_pwd_input,
                login_error_text,
                ft.Container(height=10),
                GlassContainer(
                    content=ft.Text("Đăng Nhập", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE),
                    width=250, height=50, on_click=btn_login_clicked, padding=10,
                    bgcolor=ft.Colors.with_opacity(0.3, ft.Colors.WHITE)
                ),
                ft.Container(height=5),
                ft.TextButton("Đăng Ký Tài Khoản", on_click=btn_register_clicked, style=ft.ButtonStyle(color=ft.Colors.WHITE70))
            ], horizontal_alignment=ft.CrossAxisAlignment.CENTER, tight=True),
            width=350, padding=30
        )
    ]

    # --- SỰ KIỆN NÚT BẤM (MAIN VIEW) ---
    async def unlock_clicked(e):
        await outgoing_queue.put("WEB_CMD:UNLOCK")
        status_text.value = "Đang truyền lệnh mở..."
        page.update()
        
    async def view_logs_clicked(e):
        await outgoing_queue.put("GET_LOGS")
        status_text.value = "Đang tải lịch sử..."
        page.update()
        
    async def manage_clicked(e):
        enroll_status.value = "Điền Tên và ID để thao tác"
        enroll_status.color = ft.Colors.GREY
        name_input.value = ""
        id_input.value = ""
        await outgoing_queue.put("GET_USERS")
        page.show_dialog(mgmt_dialog)

    async def change_pin_clicked(e):
        page.show_dialog(change_pin_dialog)

    # --- GIAO DIỆN CHÍNH (MAIN VIEW) ---
    unlock_btn = GlassContainer(
        content=ft.Row([ft.Icon(icon=ft.Icons.LOCK_OPEN, color=ft.Colors.WHITE, size=30), ft.Text("MỞ KHÓA TỪ XA", weight=ft.FontWeight.BOLD, size=18, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=300, height=80, on_click=unlock_clicked, bgcolor=ft.Colors.with_opacity(0.2, ft.Colors.BLUE_ACCENT), padding=15
    )
    logs_btn = GlassContainer(
        content=ft.Row([ft.Icon(icon=ft.Icons.HISTORY, color=ft.Colors.WHITE), ft.Text("XEM LỊCH SỬ", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=300, height=60, on_click=view_logs_clicked, padding=10
    )
    mgmt_btn = GlassContainer(
        content=ft.Row([ft.Icon(icon=ft.Icons.PEOPLE, color=ft.Colors.WHITE), ft.Text("QUẢN LÝ VÂN TAY", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=300, height=60, on_click=manage_clicked, padding=10
    )
    change_pin_btn = GlassContainer(
        content=ft.Row([ft.Icon(icon=ft.Icons.PASSWORD, color=ft.Colors.WHITE), ft.Text("ĐỔI MÃ PIN", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),
        width=300, height=60, on_click=change_pin_clicked, padding=10
    )

    main_view.controls = [
        GlassContainer(
            content=ft.Column([
                title_text, ft.Container(height=5), lock_icon, ft.Container(height=5), status_text
            ], horizontal_alignment=ft.CrossAxisAlignment.CENTER, tight=True),
            width=300, padding=20
        ),
        ft.Container(height=30),
        unlock_btn, ft.Container(height=15),
        logs_btn, ft.Container(height=15), mgmt_btn, ft.Container(height=15), change_pin_btn
    ]

    page.padding = 0
    main_bg = ft.Container(
        expand=True,
        gradient=ft.LinearGradient(
            begin=ft.Alignment(-1, -1),
            end=ft.Alignment(1, 1),
            colors=["#2b1055", "#7597de"]
        ),
        alignment=ft.Alignment(0, 0),
        content=ft.Column([
            login_view, main_view
        ], horizontal_alignment=ft.CrossAxisAlignment.CENTER, alignment=ft.MainAxisAlignment.CENTER, scroll=ft.ScrollMode.AUTO)
    )

    page.add(main_bg)

    asyncio.create_task(connect_server())

if __name__ == "__main__":
    ft.app(target=main)

