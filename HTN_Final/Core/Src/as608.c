#include "as608.h"
#include "CLCD_I2C.h"
#include "cmsis_os.h"
extern CLCD_I2C_Name LCD1;
// Tham chiếu đến cấu hình UART1 sinh ra bởi CubeMX
extern UART_HandleTypeDef huart1;

// Header và Address mặc định của cảm biến AS608
const uint8_t AS608_Header[6] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};

// --- HÀM GỬI LỆNH XUỐNG CẢM BIẾN ---
static void AS608_SendCommand(uint8_t *cmd, uint8_t len) {
	uint8_t dummy;
	    // Đọc bỏ liên tục cho đến khi Timeout (bộ đệm rỗng hoàn toàn)
	while (HAL_UART_Receive(&huart1, &dummy, 1, 2) == HAL_OK) {}
    // Gửi Header và Address trước
    HAL_UART_Transmit(&huart1, (uint8_t*)AS608_Header, 6, 100);
    // Gửi gói lệnh (Instruction) và Checksum
    HAL_UART_Transmit(&huart1, cmd, len, 100);
}

// --- HÀM NHẬN PHẢN HỒI TỪ CẢM BIẾN ---
static uint8_t AS608_ReceiveResponse(uint8_t *buffer, uint8_t len) {
	if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET) {
	        __HAL_UART_CLEAR_OREFLAG(&huart1);
	    }
    // Chờ nhận dữ liệu, Timeout 500ms
    if (HAL_UART_Receive(&huart1, buffer, len, 500) == HAL_OK) {
        // Byte thứ 9 trong frame phản hồi của AS608 chính là mã xác nhận (Confirmation Code)
        return buffer[9];
    }
    return 0xFF; // Lỗi mất kết nối / Timeout
}

// 1. Chụp ảnh vân tay
uint8_t AS608_GetImage(void) {
    // Cấu trúc: PID(01) + Len(00 03) + Lệnh GetImage(01) + Sum(00 05)
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    uint8_t rxBuffer[12];

    AS608_SendCommand(cmd, sizeof(cmd));
    return AS608_ReceiveResponse(rxBuffer, 12);
}

// 2. Chuyển đổi ảnh thành đặc trưng (CharBuffer)
uint8_t AS608_GenChar(uint8_t bufferID) {
    // Cấu trúc: PID(01) + Len(00 04) + Lệnh GenChar(02) + BufferID + Sum
    uint8_t cmd[] = {0x01, 0x00, 0x04, 0x02, bufferID, 0x00, (uint8_t)(0x07 + bufferID)};
    uint8_t rxBuffer[12];

    AS608_SendCommand(cmd, sizeof(cmd));
    return AS608_ReceiveResponse(rxBuffer, 12);
}

// 3. Tìm kiếm vân tay trong bộ nhớ
uint8_t AS608_Search(uint16_t *fingerID, uint16_t *matchScore) {
    // Tìm kiếm trong CharBuffer1, bắt đầu từ ID 0, độ dài 200 (0x00 0xC8) ID.
    // Mã Checksum = 01+00+08+04+01+00+00+00+C8 = D6
    uint8_t cmd[] = {0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xC8, 0x00, 0xD6};
    uint8_t rxBuffer[16];

    AS608_SendCommand(cmd, sizeof(cmd));

    // Hàm Search trả về mảng 16 byte nếu thành công
    if (HAL_UART_Receive(&huart1, rxBuffer, 16, 500) == HAL_OK) {
        if (rxBuffer[9] == AS608_OK) {
            *fingerID = (rxBuffer[10] << 8) | rxBuffer[11]; // Ghép 2 byte ID
            *matchScore = (rxBuffer[12] << 8) | rxBuffer[13]; // Ghép 2 byte điểm số
            return AS608_OK;
        }
        return rxBuffer[9];
    }
    return 0xFF;
}

// --- HÀM TỔNG HỢP: QUÉT VÀ KIỂM TRA ---
int AS608_CheckFingerprint(void) {
    uint16_t fingerID = 0, score = 0;

    // Bước 1: Quét ngón tay
    if (AS608_GetImage() != AS608_OK) {
        return 0; // Không có ngón tay đặt lên
    }

    // Bước 2: Tạo mẫu vân tay lưu vào Buffer 1
    if (AS608_GenChar(1) != AS608_OK) {
        return -1; // Đọc ảnh lỗi (vân tay mờ, xước...)
    }

    // Bước 3: Tìm kiếm trong Flash của cảm biến
    if (AS608_Search(&fingerID, &score) == AS608_OK) {
        return fingerID; // Trả về số ID hợp lệ
    }

    return -1; // Vân tay không có trong hệ thống
}
// Trộn mẫu
uint8_t AS608_RegModel(void) {
    uint8_t cmd[] = {0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    uint8_t rxBuffer[12];
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];
    return 0xFF;
}

// Lưu vào Flash
uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID) {
    uint8_t sum = 0x01 + 0x00 + 0x06 + 0x06 + bufferID + (pageID >> 8) + (pageID & 0xFF);
    uint8_t cmd[] = {0x01, 0x00, 0x06, 0x06, bufferID, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF), 0x00, sum};
    uint8_t rxBuffer[12];
    HAL_UART_Transmit(&huart1, (uint8_t*)"\xEF\x01\xFF\xFF\xFF\xFF", 6, 100);
    HAL_UART_Transmit(&huart1, cmd, sizeof(cmd), 100);
    if (HAL_UART_Receive(&huart1, rxBuffer, 12, 500) == HAL_OK) return rxBuffer[9];
    return 0xFF;
}

// Kịch bản đăng ký
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
