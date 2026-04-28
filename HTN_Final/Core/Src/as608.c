#include "as608.h"
#include "CLCD_I2C.h"
#include "cmsis_os.h"
extern CLCD_I2C_Name LCD1;
extern UART_HandleTypeDef huart1;

const uint8_t AS608_Header[6] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};

static void AS608_SendCommand(uint8_t *cmd, uint8_t len) {
	uint8_t dummy;
	while (HAL_UART_Receive(&huart1, &dummy, 1, 2) == HAL_OK) {}
    HAL_UART_Transmit(&huart1, (uint8_t*)AS608_Header, 6, 100);
    HAL_UART_Transmit(&huart1, cmd, len, 100);
}

static uint8_t AS608_ReceiveResponse(uint8_t *buffer, uint8_t len) {
	if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET) {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }
    if (HAL_UART_Receive(&huart1, buffer, len, 500) == HAL_OK) {
        return buffer[9];
    }
    return 0xFF;
}

uint8_t AS608_GetImage(void) {
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    uint8_t rxBuffer[12];
    AS608_SendCommand(cmd, sizeof(cmd));
    return AS608_ReceiveResponse(rxBuffer, 12);
}

uint8_t AS608_GenChar(uint8_t bufferID) {
    uint8_t cmd[] = {0x01, 0x00, 0x04, 0x02, bufferID, 0x00, (uint8_t)(0x07 + bufferID)};
    uint8_t rxBuffer[12];
    AS608_SendCommand(cmd, sizeof(cmd));
    return AS608_ReceiveResponse(rxBuffer, 12);
}

uint8_t AS608_Search(uint16_t *fingerID, uint16_t *matchScore) {
    uint8_t cmd[] = {0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xC8, 0x00, 0xD6};
    uint8_t rxBuffer[16];
    AS608_SendCommand(cmd, sizeof(cmd));
    if (HAL_UART_Receive(&huart1, rxBuffer, 16, 500) == HAL_OK) {
        if (rxBuffer[9] == AS608_OK) {
            *fingerID = (rxBuffer[10] << 8) | rxBuffer[11];
            *matchScore = (rxBuffer[12] << 8) | rxBuffer[13];
            return AS608_OK;
        }
        return rxBuffer[9];
    }
    return 0xFF;
}

int AS608_CheckFingerprint(void) {
    uint16_t fingerID = 0, score = 0;
    if (AS608_GetImage() != AS608_OK) return 0;
    if (AS608_GenChar(1) != AS608_OK) return -1;
    if (AS608_Search(&fingerID, &score) == AS608_OK) return fingerID;
    return -1;
}

uint8_t AS608_RegModel(void) {
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    uint8_t rxBuffer[12];
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];
    return 0xFF;
}

uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID) {
    uint8_t sum = 0x01 + 0x00 + 0x06 + 0x06 + bufferID + (pageID >> 8) + (pageID & 0xFF);
    uint8_t cmd[] = {0x01, 0x00, 0x06, 0x06, bufferID, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF), 0x00, sum};
    uint8_t rxBuffer[12];
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];
    return 0xFF;
}

int AS608_Enroll(uint16_t id) {
    CLCD_I2C_Clear(&LCD1);
    CLCD_I2C_WriteString(&LCD1, "DAT TAY LAN 1...");
    while (AS608_GetImage() != AS608_OK) { osDelay(100); }
    if (AS608_GenChar(1) != AS608_OK) return 0;

    CLCD_I2C_Clear(&LCD1);
    CLCD_I2C_WriteString(&LCD1, "NHAC TAY RA...");
    while (AS608_GetImage() == AS608_OK) { osDelay(100); }

    CLCD_I2C_Clear(&LCD1);
    CLCD_I2C_WriteString(&LCD1, "DAT LAI LAN 2...");
    while (AS608_GetImage() != AS608_OK) { osDelay(100); }
    if (AS608_GenChar(2) != AS608_OK) return 0;

    if (AS608_RegModel() != AS608_OK || AS608_StoreChar(1, id) != AS608_OK) {
        CLCD_I2C_Clear(&LCD1);
        CLCD_I2C_WriteString(&LCD1, "LOI LUU TRU!");
        osDelay(2000);
        return 0;
    }
    CLCD_I2C_Clear(&LCD1);
    CLCD_I2C_WriteString(&LCD1, "DANG KY OK!");
    osDelay(2000);
    return 1;
}

// --- HÀM MỚI: XÓA VÂN TAY TRONG FLASH CỦA CẢM BIẾN ---
uint8_t AS608_DeleteChar(uint16_t pageID) {
    uint8_t sum = 0x01 + 0x00 + 0x07 + 0x0C + (pageID >> 8) + (pageID & 0xFF) + 0x00 + 0x01;
    uint8_t cmd[] = {0x01, 0x00, 0x07, 0x0C, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF), 0x00, 0x01, sum};
    uint8_t rxBuffer[12];
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];
    return 0xFF;
}
