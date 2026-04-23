#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// 1. Chọn dòng vi điều khiển (Nhóm bạn đang dùng STM32F103)
#define STM32F1

// 2. KÍCH HOẠT CÁC FONT CHỮ SẼ DÙNG
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18

// 3. Định nghĩa cấu hình I2C
#define SSD1306_USE_I2C
#define SSD1306_I2C_PORT        hi2c1
//#define SSD1306_I2C_ADDR        0x7A   // <--- HÃY THÊM DÒNG NÀY VÀO
#define SSD1306_I2C_ADDR 0x78
// 4. Khai báo thư viện chuẩn của STM32 HAL
#include "stm32f1xx_hal.h"

#endif /* __SSD1306_CONF_H__ */
