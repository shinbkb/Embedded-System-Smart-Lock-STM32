#include "keypad.h"

// Định nghĩa ma trận phím
const char keyMap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// Mảng chứa các chân Cột (PA8, PA11, PA12, PA15)
GPIO_TypeDef* colPorts[4] = {GPIOA, GPIOA, GPIOA, GPIOA};
uint16_t colPins[4] = {GPIO_PIN_8, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_15};

// Mảng chứa các chân Hàng (PB12, PB13, PB14, PB15)
GPIO_TypeDef* rowPorts[4] = {GPIOB, GPIOB, GPIOB, GPIOB};
uint16_t rowPins[4] = {GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};

char Keypad_GetKey(void) {
    char pressedKey = 0; // Mặc định không có phím

    for (int col = 0; col < 4; col++) {
        // Bước 1: Đặt tất cả các Cột lên mức CAO (HIGH)
        for (int i = 0; i < 4; i++) {
            HAL_GPIO_WritePin(colPorts[i], colPins[i], GPIO_PIN_SET);
        }

        // Bước 2: Kéo Cột hiện tại xuống mức THẤP (LOW)
        HAL_GPIO_WritePin(colPorts[col], colPins[col], GPIO_PIN_RESET);

        // Tạo một khoảng trễ siêu nhỏ để tín hiệu điện ổn định
        HAL_Delay(2);

        // Bước 3: Đọc trạng thái 4 Hàng
        for (int row = 0; row < 4; row++) {
            if (HAL_GPIO_ReadPin(rowPorts[row], rowPins[row]) == GPIO_PIN_RESET) {
                // Đợi thả phím (Chống dội - Debounce)
                while (HAL_GPIO_ReadPin(rowPorts[row], rowPins[row]) == GPIO_PIN_RESET) {
                    HAL_Delay(10);
                }
                pressedKey = keyMap[row][col]; // Lấy ký tự từ ma trận
                break;
            }
        }

        if (pressedKey != 0) break; // Thoát nếu đã tìm thấy phím
    }

    // Đưa tất cả Cột về mức CAO trước khi thoát
    for (int i = 0; i < 4; i++) {
        HAL_GPIO_WritePin(colPorts[i], colPins[i], GPIO_PIN_SET);
    }

    return pressedKey;
}
