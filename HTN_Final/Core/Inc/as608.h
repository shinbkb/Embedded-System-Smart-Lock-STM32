#ifndef AS608_H
#define AS608_H

#include "stm32f1xx_hal.h"

#define AS608_OK 0x00

uint8_t AS608_GetImage(void);
uint8_t AS608_GenChar(uint8_t bufferID);
uint8_t AS608_Search(uint16_t *fingerID, uint16_t *matchScore);
int AS608_CheckFingerprint(void);

uint8_t AS608_RegModel(void);
uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID);
int AS608_Enroll(uint16_t id);
uint8_t AS608_DeleteChar(uint16_t pageID);

#endif
