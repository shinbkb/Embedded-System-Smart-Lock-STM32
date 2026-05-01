#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H

#include "main.h"
#include "servo.h"
#include <stdbool.h>

void LCD_Print(uint8_t row, uint8_t col, const char* text);
void LCD_Clear(void);
bool IsIDEnrolled(uint16_t id);
bool AddID(uint16_t id);
bool RemoveID(uint16_t id);
void App_Init_FreeRTOS(void);

#endif

