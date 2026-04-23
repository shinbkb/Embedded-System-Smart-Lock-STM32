/*
 * oled_ui.c
 *
 *  Created on: Apr 8, 2026
 *      Author: PC
 */


#include "oled_ui.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>

// Khai báo Mutex dùng riêng cho OLED
extern osMutexId_t oledMutexHandle;

void OLED_UI_Init(void) {
    // Khởi tạo màn hình
    ssd1306_Init();

    // Màn hình chào mừng
    ssd1306_Fill(Black);
    ssd1306_SetCursor(10, 25);
    ssd1306_WriteString("SMART LOCK", Font_11x18, White);
    ssd1306_UpdateScreen();
}

void OLED_ShowMessage(char* title, char* content) {
    // 1. Yêu cầu chiếm quyền điều khiển I2C (Chờ tối đa osWaitForever)
    if (osMutexAcquire(oledMutexHandle, osWaitForever) == osOK) {

        // 2. Xóa màn hình
        ssd1306_Fill(Black);

        // 3. In tiêu đề (Ví dụ: "TRANG THAI:") ở dòng trên cùng
        ssd1306_SetCursor(5, 5);
        ssd1306_WriteString(title, Font_7x10, White);

        // 4. In nội dung (Ví dụ: "MO CUA", "SAI MA PIN") ở giữa màn hình
        // Căn giữa tương đối dựa trên độ dài chuỗi
        int x_pos = (128 - (strlen(content) * 11)) / 2;
        if(x_pos < 0) x_pos = 0;

        ssd1306_SetCursor(x_pos, 30);
        ssd1306_WriteString(content, Font_11x18, White);

        // 5. Cập nhật dữ liệu từ Buffer ra màn hình
        ssd1306_UpdateScreen();

        // 6. Nhả quyền điều khiển để Task khác có thể dùng
        osMutexRelease(oledMutexHandle);
    }
}
