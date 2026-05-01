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

    outgoing_queue = asyncio.Queue()  # Hàng đợi gửi lệnh đến STM32

    # POPUP 1: LỊCH SỬ MỞ CỬA
    logs_table = ft.DataTable(  # Bảng lịch sử
        columns=[   # Cột lịch sử
            ft.DataColumn(ft.Text("Thời gian")),
            ft.DataColumn(ft.Text("Người mở")),
            ft.DataColumn(ft.Text("Cách mở")),
        ],
        rows=[],
    )
    def close_logs_dialog(e):  # Đóng dialog lịch sử
        page.pop_dialog()

    logs_dialog = ft.AlertDialog(  # Dialog lịch sử
        title=ft.Text("Lịch Sử Mở Cửa", weight=ft.FontWeight.BOLD),
        content=ft.Container(content=ft.Column([logs_table], scroll=ft.ScrollMode.AUTO), height=400, width=320),
        actions=[ft.TextButton("Đóng", on_click=close_logs_dialog)],
    )

    # POPUP 2: QUẢN LÝ VÂN TAY
    name_input = ft.TextField(label="Tên (VD: Bố)", expand=True)  #Ô nhập tên
    id_input = ft.TextField(label="ID Vân tay", width=110, keyboard_type=ft.KeyboardType.NUMBER)  #Ô nhập ID
    enroll_status = ft.Text("Điền Tên và ID để thao tác", color=ft.Colors.GREY)  #Hiển thị trạng thái
    
    users_table = ft.DataTable(  # Bảng quản lý người dùng
        columns=[
            ft.DataColumn(ft.Text("ID")),
            ft.DataColumn(ft.Text("Tên Người Dùng")),
        ],
        rows=[],
    )

    async def btn_enroll_clicked(e):  # Nhấn nút Thêm
        if not name_input.value or not id_input.value:  # Kiểm tra đã nhập tên và ID chưa
            enroll_status.value = "Vui lòng nhập đủ Tên và ID!"
            enroll_status.color = ft.Colors.RED
            page.update()  # Cập nhật giao diện
            return
        
        enroll_status.value = "Đang chờ đặt tay lên cửa..."
        enroll_status.color = ft.Colors.YELLOW
        page.update()
        
        # Ra lệnh Đăng ký
        await outgoing_queue.put(f"WEB_CMD:ENROLL_NAME:{id_input.value}:{name_input.value}")

    async def btn_delete_clicked(e):    # Nhấn nút Xóa
        if not id_input.value:  # Kiểm tra đã nhập ID chưa
            enroll_status.value = "Vui lòng nhập ID cần xóa!"  # Hiện thông báo lỗi
            enroll_status.color = ft.Colors.RED  # Đổi màu sang đỏ
            page.update()  # Cập nhật giao diện
            return
            
        # Ra lệnh Xóa
        await outgoing_queue.put(f"WEB_CMD:DELETE_ID:{id_input.value}")
        enroll_status.value = f"Đã gửi lệnh xóa ID {id_input.value}!"
        enroll_status.color = ft.Colors.GREEN
        page.update()

    def close_mgmt_dialog(e):  # Đóng dialog quản lý người dùng
        page.pop_dialog()

    mgmt_dialog = ft.AlertDialog(
        title=ft.Text("Quản lý Người Dùng", weight=ft.FontWeight.BOLD),  # Tiêu đề dialog
        content=ft.Container(
            content=ft.Column([
                ft.Row([name_input, id_input]),
                ft.Container(height=10),
                ft.Row([
                    ft.Button("Thêm", icon=ft.Icons.FINGERPRINT, bgcolor=ft.Colors.GREEN_700, color=ft.Colors.WHITE, on_click=btn_enroll_clicked),
                    ft.Button("Xóa", icon=ft.Icons.DELETE, bgcolor=ft.Colors.RED_700, color=ft.Colors.WHITE, on_click=btn_delete_clicked),
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

    # POPUP 4: ĐỔI MÃ PIN 
    new_pin_input = ft.TextField(label="Mã PIN mới (Chỉ số)", keyboard_type=ft.KeyboardType.NUMBER, width=250)  # Ô nhập mã PIN mới

    async def change_pin_confirm(e):  # Xác nhận đổi mã PIN
        if not new_pin_input.value.isdigit() or len(new_pin_input.value) > 9:  # Kiểm tra mã PIN mới đã nhập chưa
            status_text.value = "Lỗi: Mã PIN phải là số và < 10 ký tự!"  # Hiện thông báo lỗi
            status_text.color = ft.Colors.RED
            page.pop_dialog()  # Đóng dialog
            page.update()  # Cập nhật giao diện
            return
        pin = new_pin_input.value  # Lấy mã PIN mới
        await outgoing_queue.put(f"WEB_CMD:CHANGE_PIN:{pin}")  # Gửi lệnh đổi mã PIN
        new_pin_input.value = ""  # Xóa mã PIN mới
        page.pop_dialog()  # Đóng dialog
        status_text.value = f"Đã gửi lệnh đổi mã PIN mới!"  # Hiện thông báo đổi mã PIN mới
        status_text.color = ft.Colors.GREEN  # Đổi màu sang xanh
        page.update()  # Cập nhật giao diện

    def close_change_pin_dialog(e):  # Đóng dialog đổi mã PIN
        page.pop_dialog()  # Đóng dialog

    change_pin_dialog = ft.AlertDialog(
        title=ft.Text("Đổi Mã PIN Cửa", weight=ft.FontWeight.BOLD),  # Tiêu đề dialog
        content=ft.Container(content=ft.Column([new_pin_input], tight=True), width=300),  # Nội dung dialog
        actions=[
            ft.TextButton("Hủy", on_click=close_change_pin_dialog),  # Nút hủy
            ft.TextButton("Xác nhận", on_click=change_pin_confirm)  # Nút xác nhận
        ],
    )

    # KẾT NỐI WEBSOCKET 
    async def connect_server():  # Kết nối WebSocket
        uri = "ws://192.168.137.1:3030" 
        try:
            async with websockets.connect(uri) as ws:  # Kết nối WebSocket
                status_text.value = "Đã kết nối với Khóa!"  # Hiện thông báo đã kết nối
                status_text.color = ft.Colors.GREEN  # Đổi màu sang xanh
                page.update()
                
                async def listen_messages():  # Lắng nghe tin nhắn từ WebSocket
                    async for msg in ws:  # Lắng nghe tin nhắn từ WebSocket
                        # 0. Kết quả đăng nhập
                        if msg == "LOGIN_SUCCESS":  # Kiểm tra kết quả đăng nhập
                            print("=== NHẬN LOGIN SUCCESS ===")  # In thông báo nhận đăng nhập thành công
                            try:
                                login_view.visible = False  # Ẩn giao diện đăng nhập
                                main_view.visible = True  # Hiện giao diện chính
                                page.update()  # Cập nhật giao diện
                                print("=== CẬP NHẬT GIAO DIỆN CHÍNH THÀNH CÔNG ===")  # In thông báo cập nhật giao diện chính thành công
                            except Exception as e:
                                print(f"=== LỖI CẬP NHẬT GIAO DIỆN: {e} ===")  # In thông báo lỗi cập nhật giao diện
                            continue
                        elif msg == "LOGIN_FAIL":  # Kiểm tra kết quả đăng nhập
                            login_error_text.value = "Tài khoản hoặc mật khẩu sai!"  # Hiện thông báo lỗi đăng nhập
                            login_error_text.color = ft.Colors.RED  # Đổi màu sang đỏ
                            page.update()  # Cập nhật giao diện
                            continue
                        elif msg == "REGISTER_SUCCESS":  # Kiểm tra k   ết quả đăng ký
                            page.pop_dialog()  # Đóng dialog
                            login_error_text.value = "Đăng ký thành công! Hãy bấm Đăng Nhập."  # Hiện thông báo đăng ký thành công
                            login_error_text.color = ft.Colors.GREEN
                            page.update()
                            continue
                        elif msg == "REGISTER_FAIL":  # Kiểm tra kết quả đăng ký
                            reg_error_text.value = "Tài khoản đã tồn tại!"  # Hiện thông báo tài khoản đã tồn tại
                            reg_error_text.color = ft.Colors.RED  # Đổi màu sang đỏ
                            page.update()  # Cập nhật giao diện
                            continue

                        # Nhận dữ liệu Lịch sử
                        if "logs_data" in msg:  # Kiểm tra dữ liệu lịch sử  
                            parsed_msg = json.loads(msg)  # Phân tích cú pháp JSON
                            logs_table.rows.clear()  # Xóa bảng lịch sử
                            for row in parsed_msg["data"]:  # Lặp qua dữ liệu lịch sử
                                logs_table.rows.append(
                                    ft.DataRow(cells=[
                                        ft.DataCell(ft.Text(row["opened_at"])),  # Thời gian mở
                                        ft.DataCell(ft.Text(row["full_name"], color=ft.Colors.GREEN_400)),  # Tên người mở
                                        ft.DataCell(ft.Text(row["method"])),  # Phương thức mở
                                    ])
                                )
                            page.show_dialog(logs_dialog)  # Hiện dialog lịch sử
                            status_text.value = "Tải lịch sử thành công!"
                            page.update()  # Cập nhật giao diện
                            
                        # Nhận danh sách người dùng
                        elif "users_data" in msg:  # Kiểm tra dữ liệu người dùng
                            parsed_msg = json.loads(msg)  # Phân tích cú pháp JSON
                            users_table.rows.clear()  # Xóa bảng người dùng
                            for row in parsed_msg["data"]:  # Lặp qua dữ liệu người dùng
                                users_table.rows.append(  # Thêm dữ liệu người dùng vào bảng
                                    ft.DataRow(cells=[  
                                        ft.DataCell(ft.Text(str(row["fingerprint_id"]))),  # ID người dùng
                                        ft.DataCell(ft.Text(row["full_name"], color=ft.Colors.BLUE_400)),  # Tên người dùng
                                    ])
                                )
                            page.update()
                            
                        # STM32 báo Đăng ký thành công
                        elif "ENROLL_SUCCESS" in msg:  # Kiểm tra đăng ký thành công
                            f_id = msg.split(":")[1].strip()  # Lấy ID người dùng
                            enroll_status.value = f"Tuyệt vời! Đã đăng ký thành công ID {f_id}!"  # Hiện thông báo đăng ký thành công
                            enroll_status.color = ft.Colors.GREEN  # Đổi màu sang xanh
                            page.update()  # Cập nhật giao diện
                            
                        # STM32 báo Mở cửa thành công
                        elif "DOOR_OPENED" in msg:  # Kiểm tra mở cửa thành công
                            lock_icon.icon = ft.Icons.LOCK_OPEN  # Đổi icon sang mở cửa
                            lock_icon.color = ft.Colors.GREEN  # Đổi màu sang xanh
                            
                            if "FINGER" in msg:
                                f_id = msg.split(":")[2].strip()
                                status_text.value = f"CỬA ĐANG MỞ (Vân tay số {f_id})"  # Hiện thông báo cửa đang mở
                            elif "PIN" in msg:
                                status_text.value = "CỬA ĐANG MỞ (Mã PIN)"  # Hiện thông báo cửa đang mở
                            else:
                                status_text.value = "CỬA ĐANG MỞ"  # Hiện thông báo cửa đang mở
                                
                            page.update()   
                            
                            await asyncio.sleep(5)  # Chờ 5 giây
                            lock_icon.icon = ft.Icons.LOCK  # Đổi icon sang khóa
                            lock_icon.color = ft.Colors.RED  # Đổi màu sang đỏ
                            status_text.value = "SẴN SÀNG"  # Hiện thông báo sẵn sàng
                            page.update()   # Cập nhật giao diện

                async def send_messages():  # Gửi tin nhắn đến STM32
                    while True:
                        msg_to_send = await outgoing_queue.get()  # Lấy tin nhắn từ hàng đợi
                        await ws.send(msg_to_send)  # Gửi tin nhắn đến STM32

                await asyncio.gather(listen_messages(), send_messages())

        except Exception as e:  # Lỗi kết nối
            import traceback    
            traceback.print_exc()           # In lỗi        
            try:
                status_text.value = f"Mất kết nối: Chờ tải lại..."  # Hiện thông báo mất kết nối
                status_text.color = ft.Colors.RED  # Đổi màu sang đỏ
                page.update()  # Cập nhật giao diện
            except:
                pass
            await asyncio.sleep(3)  # Chờ 3 giây
            await connect_server()   # Kết nối lại

    # HÀM TẠO GLASSMORPHISM         
    def GlassContainer(content, width=None, height=None, padding=15, on_click=None, border_radius=25, bgcolor=None):   # Tạo giao diện trong suốt
        if bgcolor is None:
            bgcolor = ft.Colors.with_opacity(0.1, ft.Colors.WHITE)   # Tạo màu nền trong suốt
        return ft.Container(
            content=content, width=width, height=height, padding=padding, border_radius=border_radius,   # Tạo khung chứa nội dung
            bgcolor=bgcolor, border=ft.Border.all(1, ft.Colors.with_opacity(0.2, ft.Colors.WHITE)),  # Tạo viền khung
            blur=15, on_click=on_click, ink=True if on_click else False, alignment=ft.Alignment(0, 0)
        )

    # VIEWS (MÀN HÌNH) 
    login_view = ft.Column(visible=True, horizontal_alignment=ft.CrossAxisAlignment.CENTER)   # Màn hình đăng nhập
    main_view = ft.Column(visible=False, horizontal_alignment=ft.CrossAxisAlignment.CENTER)  # Màn hình chính

    # MÀN HÌNH ĐĂNG NHẬP 
    login_user_input = ft.TextField(    # Ô nhập tên đăng nhập
        label="Tên đăng nhập", width=250,  # Nhãn và chiều rộng
        border_color=ft.Colors.WHITE70, color=ft.Colors.WHITE, label_style=ft.TextStyle(color=ft.Colors.WHITE70)   # Màu sắc viền, chữ và nhãn
    )
    login_pwd_input = ft.TextField(     # Ô nhập mật khẩu
        label="Mật khẩu", password=True, can_reveal_password=True, width=250,  # Nhãn, mật khẩu, hiện mật khẩu và chiều rộng
        border_color=ft.Colors.WHITE70, color=ft.Colors.WHITE, label_style=ft.TextStyle(color=ft.Colors.WHITE70)
    )
    login_error_text = ft.Text("", color=ft.Colors.RED)  # Text lỗi đăng nhập

    async def btn_login_clicked(e):   # Hàm xử lý đăng nhập
        try:
            u = login_user_input.value  # Lấy tên đăng nhập
            p = login_pwd_input.value   # Lấy mật khẩu
            if u and p:
                await outgoing_queue.put(f"WEB_CMD:LOGIN:{u}:{p}")  # Gửi yêu cầu đăng nhập đến STM32
                login_error_text.value = "Đang kiểm tra..."  # Hiện thông báo đang kiểm tra
                login_error_text.color = ft.Colors.YELLOW  # Đổi màu sang vàng
                page.update()  # Cập nhật giao diện
        except Exception as e:
            import traceback
            traceback.print_exc()
            login_error_text.value = f"Lỗi: {e}"
            page.update()

    # DIALOG ĐĂNG KÝ 
    reg_user_input = ft.TextField(          # Ô nhập tên đăng nhập mới
        label="Tên đăng nhập mới", width=250   # Nhãn và chiều rộng
    )
    reg_pwd_input = ft.TextField(label="Mật khẩu", password=True, can_reveal_password=True, width=250)   # Ô nhập mật khẩu
    reg_pwd2_input = ft.TextField(label="Nhập lại mật khẩu", password=True, can_reveal_password=True, width=250)   # Ô nhập lại mật khẩu
    reg_error_text = ft.Text("", color=ft.Colors.RED)  # Text lỗi đăng ký

    async def confirm_register(e):   # Hàm xử lý đăng ký
        u = reg_user_input.value    # Lấy tên đăng nhập mới
        p1 = reg_pwd_input.value    # Lấy mật khẩu
        p2 = reg_pwd2_input.value   # Lấy lại mật khẩu
        if not u or not p1:   # Kiểm tra thông tin đăng ký
            reg_error_text.value = "Vui lòng nhập đủ thông tin!"  # Hiện thông báo thiếu thông tin
            page.update()   # Cập nhật giao diện
            return
        if p1 != p2:    # Kiểm tra mật khẩu có khớp nhau không
            reg_error_text.value = "Mật khẩu không khớp!"  # Hiện thông báo mật khẩu không khớp
            page.update()   # Cập nhật giao diện
            return
            
        await outgoing_queue.put(f"WEB_CMD:REGISTER:{u}:{p1}")  # Gửi yêu cầu đăng ký đến STM32
        reg_error_text.value = "Đang gửi yêu cầu..."  # Hiện thông báo đang gửi yêu cầu
        reg_error_text.color = ft.Colors.YELLOW  # Đổi màu sang vàng
        page.update()  # Cập nhật giao diện

    def close_reg_dialog(e):  # Hàm đóng dialog đăng ký
        page.pop_dialog()  # Đóng dialog đăng ký

    register_dialog = ft.AlertDialog(    # Dialog đăng ký
        title=ft.Text("Đăng Ký Tài Khoản Mới", weight=ft.FontWeight.BOLD),  # Tiêu đề dialog
        content=ft.Container(content=ft.Column([reg_user_input, reg_pwd_input, reg_pwd2_input, reg_error_text], tight=True), width=300),  # Nội dung dialog
        actions=[
            ft.TextButton("Hủy", on_click=close_reg_dialog),  # Nút hủy đăng ký
            ft.TextButton("Đăng Ký", on_click=confirm_register)  # Nút đăng ký
        ],
    )

    def btn_register_clicked(e):  # Hàm xử lý nút đăng ký
        reg_user_input.value = ""  # Xóa ô nhập tên đăng nhập
        reg_pwd_input.value = ""  # Xóa ô nhập mật khẩu
        reg_pwd2_input.value = ""  # Xóa ô nhập lại mật khẩu
        reg_error_text.value = ""  # Xóa text lỗi đăng ký
        page.show_dialog(register_dialog)  # Hiển thị dialog đăng ký

    login_view.controls = [    # Màn hình đăng nhập
        GlassContainer(
            content=ft.Column([
                ft.Text("ĐĂNG NHẬP ADMIN", size=25, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE),  # Tiêu đề đăng nhập
                ft.Container(height=10),  # Khoảng cách
                ft.Icon(ft.Icons.SECURITY, size=80, color=ft.Colors.WHITE70),  # Icon bảo mật
                ft.Container(height=10),  # Khoảng cách
                login_user_input,  # Ô nhập tên đăng nhập
                login_pwd_input,  # Ô nhập mật khẩu
                login_error_text,  # Text lỗi đăng nhập
                ft.Container(height=10),  # Khoảng cách
                GlassContainer(  # Glass container cho nút đăng nhập    
                    content=ft.Text("Đăng Nhập", size=16, weight=ft.FontWeight.BOLD, color=ft.Colors.WHITE),  # Nút đăng nhập
                    width=250, height=50, on_click=btn_login_clicked, padding=10,  # Chiều rộng, chiều cao, hàm xử lý nút, padding và màu sắc nền
                    bgcolor=ft.Colors.with_opacity(0.3, ft.Colors.WHITE)  # Màu nền
                ),
                ft.Container(height=5),  # Khoảng cách
                ft.TextButton("Đăng Ký Tài Khoản", on_click=btn_register_clicked, style=ft.ButtonStyle(color=ft.Colors.WHITE70))  # Nút đăng ký
            ], horizontal_alignment=ft.CrossAxisAlignment.CENTER, tight=True),  # Căn chỉnh nội dung
            width=350, padding=30  # Chiều rộng, padding
        )
    ]

    # SỰ KIỆN NÚT BẤM (MAIN VIEW)
    async def unlock_clicked(e):  # Hàm xử lý nút mở cửa
        await outgoing_queue.put("WEB_CMD:UNLOCK")  # Gửi yêu cầu mở cửa đến STM32
        status_text.value = "Đang truyền lệnh mở..."  # Hiện thông báo đang mở cửa
        page.update()  # Cập nhật giao diện
        
    async def view_logs_clicked(e):  # Hàm xử lý nút xem lịch sử
        await outgoing_queue.put("GET_LOGS")  # Gửi yêu cầu xem lịch sử đến STM32
        status_text.value = "Đang tải lịch sử..."  # Hiện thông báo đang tải lịch sử
        page.update()  # Cập nhật giao diện
        
    async def manage_clicked(e):  # Hàm xử lý nút quản lý
        enroll_status.value = "Điền Tên và ID để thao tác"  # Hiện thông báo điền tên và ID để thao tác
        enroll_status.color = ft.Colors.GREY  # Đổi màu sang xám
        name_input.value = ""  # Xóa ô nhập tên
        id_input.value = ""  # Xóa ô nhập ID
        await outgoing_queue.put("GET_USERS")  # Gửi yêu cầu xem danh sách người dùng đến STM32
        page.show_dialog(mgmt_dialog)  # Hiển thị dialog quản lý

    async def change_pin_clicked(e):  # Hàm xử lý nút thay đổi mã PIN
        page.show_dialog(change_pin_dialog)  # Hiển thị dialog thay đổi mã PIN

    # GIAO DIỆN CHÍNH (MAIN VIEW)
    unlock_btn = GlassContainer(  # Nút mở cửa
        content=ft.Row([ft.Icon(icon=ft.Icons.LOCK_OPEN, color=ft.Colors.WHITE, size=30), ft.Text("MỞ KHÓA TỪ XA", weight=ft.FontWeight.BOLD, size=18, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),  # Nội dung nút
        width=300, height=80, on_click=unlock_clicked, bgcolor=ft.Colors.with_opacity(0.2, ft.Colors.BLUE_ACCENT), padding=15  # Chiều rộng, chiều cao, hàm xử lý nút, màu nền, padding
    )
    logs_btn = GlassContainer(  # Nút xem lịch sử
        content=ft.Row([ft.Icon(icon=ft.Icons.HISTORY, color=ft.Colors.WHITE), ft.Text("XEM LỊCH SỬ", weight=ft.FontWeight.BOLD, size=16, color=ft.Colors.WHITE)], alignment=ft.MainAxisAlignment.CENTER),  # Nội dung nút
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

    page.add(main_bg)   # Thêm main_bg vào page

    asyncio.create_task(connect_server())   # Tạo task để kết nối server

if __name__ == "__main__":  # Điều kiện để chạy app
    ft.app(main, view=ft.AppView.WEB_BROWSER, port=8080)   # Chạy app   

