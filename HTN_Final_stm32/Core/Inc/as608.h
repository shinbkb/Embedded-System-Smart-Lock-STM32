#ifndef INC_AS608_H_
#define INC_AS608_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

// Các mã trạng thái trả về của AS608
#define AS608_OK              0x00
#define AS608_NO_FINGER       0x02
#define AS608_IMAGE_FAIL      0x03
#define AS608_MATCH_FAIL      0x09
#define AS608_NOT_FOUND       0x0A

// Hàm giao tiếp chính (Dùng trong Task RTOS)
// Trả về ID vân tay nếu đúng, trả về -1 nếu không có/không khớp
int AS608_CheckFingerprint(void);
int AS608_Enroll(uint16_t id);
// Các hàm xử lý cấp thấp
uint8_t AS608_GetImage(void);
uint8_t AS608_GenChar(uint8_t bufferID);
uint8_t AS608_Search(uint16_t *fingerID, uint16_t *matchScore);

#endif /* INC_AS608_H_ */
