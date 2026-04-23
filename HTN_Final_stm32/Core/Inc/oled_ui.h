#ifndef INC_OLED_UI_H_
#define INC_OLED_UI_H_

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

// Khởi tạo OLED và Mutex bảo vệ
void OLED_UI_Init(void);

// Hiển thị thông báo ở giữa màn hình (đã được bảo vệ bằng Mutex)
void OLED_ShowMessage(char* title, char* content);

#endif /* INC_OLED_UI_H_ */
