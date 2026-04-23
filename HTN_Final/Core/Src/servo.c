#include "servo.h"

// Gọi ra biến Timer 2 mà CubeMX đã tạo sẵn
extern TIM_HandleTypeDef htim2;

void Servo_Init(void) {
    // Bắt đầu phát xung PWM trên Channel 1 (Chân PA0)
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    Lock_Close(); // Khởi động lên là khóa cửa luôn
}

void Lock_Open(void) {
    // Xoay 90 độ (Mở chốt) - Cấp giá trị 1500 (1.5ms) vào thanh ghi CCR1
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1500);
}

void Lock_Close(void) {
    // Xoay về 0 độ (Đóng chốt) - Cấp giá trị 1000 (1.0ms) vào thanh ghi CCR1
    // (Lưu ý: Tùy cơ cấu cơ khí của bạn, có thể chỉnh số này từ 500 - 1000 cho vừa vặn)
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1000);
}
