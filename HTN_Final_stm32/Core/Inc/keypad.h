#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include "stm32f1xx_hal.h"

// Trả về ký tự được nhấn, hoặc trả về 0 nếu không có phím nào nhấn
char Keypad_GetKey(void);

#endif /* INC_KEYPAD_H_ */
