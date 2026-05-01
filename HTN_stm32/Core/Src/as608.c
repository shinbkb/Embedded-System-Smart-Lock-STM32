#include "as608.h"
#include "CLCD_I2C.h"
#include "cmsis_os.h"
extern CLCD_I2C_Name LCD1;
extern UART_HandleTypeDef huart1;

const uint8_t AS608_Header[6] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};       // Header

static void AS608_SendCommand(uint8_t *cmd, uint8_t len) {  // Hàm gửi lệnh đến AS608
	uint8_t dummy;    // Khai báo biến dummy
	while (HAL_UART_Receive(&huart1, &dummy, 1, 2) == HAL_OK) {}  // Đọc dữ liệu từ UART1 để xóa buffer rác
    HAL_UART_Transmit(&huart1, (uint8_t*)AS608_Header, 6, 100);  // Gửi header
    HAL_UART_Transmit(&huart1, cmd, len, 100);  // Gửi lệnh
}

static uint8_t AS608_ReceiveResponse(uint8_t *buffer, uint8_t len) {  // Hàm nhận phản hồi từ AS608
	if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET) {  // Kiểm tra lỗi
        __HAL_UART_CLEAR_OREFLAG(&huart1);  // Xóa lỗi
    }
    if (HAL_UART_Receive(&huart1, buffer, len, 500) == HAL_OK) {  // Nhận dữ liệu từ UART1
        return buffer[9];
    }
    return 0xFF;  // Trả về lỗi
}

uint8_t AS608_GetImage(void) {     // Hàm lấy hình ảnh từ AS608         
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x01, 0x00, 0x05};    // Lệnh lấy hình ảnh
    uint8_t rxBuffer[12];   // Buffer chứa phản hồi
    AS608_SendCommand(cmd, sizeof(cmd));  // Gửi lệnh
    return AS608_ReceiveResponse(rxBuffer, 12);  // Nhận phản hồi
}

uint8_t AS608_GenChar(uint8_t bufferID) {  // Hàm tạo đặc trưng vân tay 
    uint8_t cmd[] = {0x01, 0x00, 0x04, 0x02, bufferID, 0x00, (uint8_t)(0x07 + bufferID)};  // Lệnh tạo đặc trưng
    uint8_t rxBuffer[12];  // Buffer chứa phản hồi
    AS608_SendCommand(cmd, sizeof(cmd));  // Gửi lệnh
    return AS608_ReceiveResponse(rxBuffer, 12);  // Nhận phản hồi
}

uint8_t AS608_Search(uint16_t *fingerID, uint16_t *matchScore) {  // Hàm tìm kiếm vân tay
    uint8_t cmd[] = {0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xC8, 0x00, 0xD6};  // Lệnh tìm kiếm
    uint8_t rxBuffer[16];  // Buffer chứa phản hồi
    AS608_SendCommand(cmd, sizeof(cmd));  // Gửi lệnh
    if (HAL_UART_Receive(&huart1, rxBuffer, 16, 500) == HAL_OK) {  // Nhận phản hồi
        if (rxBuffer[9] == AS608_OK) {  // Kiểm tra phản hồi
            *fingerID = (rxBuffer[10] << 8) | rxBuffer[11];  // Lấy ID vân tay
            *matchScore = (rxBuffer[12] << 8) | rxBuffer[13];  // Lấy điểm trùng khớp
            return AS608_OK;  // Trả về thành công
        }
        return rxBuffer[9];  // Trả về lỗi
    }
    return 0xFF;  // Trả về lỗi
}

int AS608_CheckFingerprint(void) {  // Hàm kiểm tra vân tay
    uint16_t fingerID = 0, score = 0;
    if (AS608_GetImage() != AS608_OK) return 0;  // Lấy hình ảnh
    if (AS608_GenChar(1) != AS608_OK) return -1;  // Tạo đặc trưng
    if (AS608_Search(&fingerID, &score) == AS608_OK) return fingerID;  // Tìm kiếm
    return -1;  // Trả về lỗi
}

uint8_t AS608_RegModel(void) {  // Hàm đăng ký vân tay
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x05, 0x00, 0x09};  // Lệnh đăng ký
    uint8_t rxBuffer[12];  // Buffer chứa phản hồi
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);  // Gửi header
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);  // Gửi lệnh
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];  // Nhận phản hồi
    return 0xFF;  // Trả về lỗi
}

uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID) {  // Hàm lưu đặc trưng vân tay
    uint8_t sum = 0x01 + 0x00 + 0x06 + 0x06 + bufferID + (pageID >> 8) + (pageID & 0xFF);
    uint8_t cmd[] = {0x01, 0x00, 0x06, 0x06, bufferID, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF), 0x00, sum};  // Lệnh lưu đặc trưng
    uint8_t rxBuffer[12];  // Buffer chứa phản hồi
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);  // Gửi header
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);  // Gửi lệnh
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];  // Nhận phản hồi
    return 0xFF;  // Trả về lỗi
}

int AS608_Enroll(uint16_t id) {  // Hàm đăng ký vân tay
    CLCD_I2C_Clear(&LCD1);  // Xóa màn hình
    CLCD_I2C_WriteString(&LCD1, "DAT TAY LAN 1...");  // Hiển thị thông báo
    while (AS608_GetImage() != AS608_OK) { osDelay(100); }  // Lấy hình ảnh
    if (AS608_GenChar(1) != AS608_OK) return 0;  // Tạo đặc trưng

    CLCD_I2C_Clear(&LCD1);  // Xóa màn hình
    CLCD_I2C_WriteString(&LCD1, "NHAC TAY RA...");  // Hiển thị thông báo
    while (AS608_GetImage() == AS608_OK) { osDelay(100); }  // Lấy hình ảnh

    CLCD_I2C_Clear(&LCD1);  // Xóa màn hình
    CLCD_I2C_WriteString(&LCD1, "DAT LAI LAN 2...");  // Hiển thị thông báo
    while (AS608_GetImage() != AS608_OK) { osDelay(100); }  // Lấy hình ảnh
    if (AS608_GenChar(2) != AS608_OK) return 0;  // Tạo đặc trưng

    if (AS608_RegModel() != AS608_OK || AS608_StoreChar(1, id) != AS608_OK) {  // Đăng ký và lưu đặc trưng
        CLCD_I2C_Clear(&LCD1);  // Xóa màn hình
        CLCD_I2C_WriteString(&LCD1, "LOI LUU TRU!");  // Hiển thị thông báo
        osDelay(2000);
        return 0;
    }
    CLCD_I2C_Clear(&LCD1);  // Xóa màn hình
    CLCD_I2C_WriteString(&LCD1, "DANG KY OK!");   // Hiển thị thông báo    
    osDelay(2000); 
    return 1;  // Trả về thành công
}


uint8_t AS608_DeleteChar(uint16_t pageID) {  // Hàm xóa vân tay
    uint8_t sum = 0x01 + 0x00 + 0x07 + 0x0C + (pageID >> 8) + (pageID & 0xFF) + 0x00 + 0x01;  // Tính tổng kiểm tra
    uint8_t cmd[] = {0x01, 0x00, 0x07, 0x0C, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF), 0x00, 0x01, sum};  // Lệnh xóa vân tay
    uint8_t rxBuffer[12];  // Buffer chứa phản hồi
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);  // Gửi header
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);  // Gửi lệnh
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];  // Nhận phản hồi
    return 0xFF;  // Trả về lỗi
}
