#ifndef INC_I2C_LCD_H_
#define INC_I2C_LCD_H_

#include "stm32f1xx_hal.h"

// Khởi tạo LCD
void lcd_init (void);
// Gửi chuỗi ký tự
void lcd_send_string (char *str);
// Đặt con trỏ (Hàng 0-1, Cột 0-15)
void lcd_put_cur(int row, int col);
// Xóa màn hình
void lcd_clear (void);

#endif /* INC_I2C_LCD_H_ */
