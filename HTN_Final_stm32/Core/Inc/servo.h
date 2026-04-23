#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include "stm32f1xx_hal.h"

// Khởi tạo và bật PWM
void Servo_Init(void);

// Điều khiển đóng/mở chốt
void Lock_Open(void);
void Lock_Close(void);

#endif /* INC_SERVO_H_ */
