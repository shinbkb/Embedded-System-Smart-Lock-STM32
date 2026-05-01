#include "app_freertos.h"
#include "cmsis_os.h"
#include "keypad.h"
#include "as608.h"
#include "servo.h"
#include "CLCD_I2C.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern I2C_HandleTypeDef hi2c1;         // Khai báo biến hi2c1
volatile bool isEnrolling = false;      // Biến kiểm tra chế độ đăng ký
extern UART_HandleTypeDef huart2;       // Khai báo biến huart2

// Các handle của FreeRTOS
osThreadId_t inputTaskHandle;       // Handle của Task Input
osThreadId_t authTaskHandle;        // Handle của Task Auth
osMessageQueueId_t inputQueueHandle;  // Handle của Queue Input
osTimerId_t autoLockTimerHandle;    // Handle của Timer AutoLock
osMutexId_t lcdMutexHandle;         // Handle của Mutex LCD
osSemaphoreId_t buzzerSemaphoreHandle;  // Handle của Semaphore Buzzer

CLCD_I2C_Name LCD1;                   // Khai báo biến LCD1
uint8_t error_count = 0;              // Biến đếm số lần nhập sai mã PIN
char correct_PIN[10] = "1234";      // Mảng chứa mã PIN đúng
char pending_new_pin[10];             // Mảng chứa mã PIN đang chờ thay đổi
char entered_PIN[10];                 // Mảng chứa mã PIN đã nhập
uint8_t pin_index = 0;                // Chỉ số của mã PIN

// Các định dạng nguồn phát lệnh
typedef struct {    // Định dạng nguồn phát lệnh
    uint8_t dataSource;   // Nguồn phát lệnh
    uint16_t value;       // Giá trị
} InputData_t;

// Các mã nguồn phát lệnh
#define SRC_KEYPAD        0           // Nguồn phát lệnh từ bàn phím
#define SRC_FINGER        1           // Nguồn phát lệnh từ vân tay
#define SRC_FINGER_FAIL   3           // Nguồn phát lệnh từ vân tay thất bại
#define SRC_REMOTE_UNLOCK 4           // Nguồn phát lệnh từ điều khiển mở cửa
#define SRC_REMOTE_ENROLL 5           // Nguồn phát lệnh từ điều khiển đăng ký vân tay
#define SRC_REMOTE_DELETE 6           // Nguồn phát lệnh từ điều khiển xóa vân tay
#define SRC_REMOTE_CHANGE_PIN 7       // Nguồn phát lệnh từ điều khiển thay đổi mã PIN
#define SRC_SYSTEM        99          // Nguồn phát lệnh từ hệ thống

// Các mã kiện từ Timer và Remote
#define EVT_AUTOLOCK      1           // Sự kiện AutoLock

// Khai báo các hàm
void Task_Input(void *argument);                                    // Hàm xử lý Task Input
void Task_Auth(void *argument);                                    // Hàm xử lý Task Auth
void AutoLock_Callback(void *argument);                            // Hàm xử lý AutoLock
void Task_Buzzer(void *argument);

// Các biến ngắt cho ESP32
uint8_t rx_byte;
char rx_buffer[32];
uint8_t rx_index = 0;

// LƯU TRỮ FLASH 
#define FLASH_USER_START_ADDR   0x0800FC00   // Địa chỉ Page 63 (Trang cuối của Flash 64KB)

void Save_PIN_To_Flash(const char* pin) {                // Hàm lưu mã PIN vào Flash
    HAL_FLASH_Unlock();                                 // Mở khóa Flash
    FLASH_EraseInitTypeDef EraseInitStruct;               // Khai báo cấu hình xóa Flash
    uint32_t PAGEError = 0;                               // Biến báo lỗi xóa Flash
    
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES; // Xóa theo trang
    EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;       // Địa chỉ bắt đầu xóa
    EraseInitStruct.NbPages     = 1;                       // Số trang cần xóa
    
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) == HAL_OK) { // Kiểm tra xóa thành công
        uint32_t Address = FLASH_USER_START_ADDR;                   // Địa chỉ bắt đầu ghi
        for(int i = 0; i < 10; i += 2) {                            // Lặp qua mã PIN
            uint16_t half_word = pin[i] | (pin[i+1] << 8);            // Gộp 2 ký tự thành 16-bit
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, half_word);  // Ghi mã PIN
            Address += 2;                                                       // Tăng địa chỉ
        }
    }
    HAL_FLASH_Lock();                                                       // Khóa Flash
}

void Load_PIN_From_Flash(void) {                                             // Hàm đọc mã PIN từ Flash
    char* flash_pin = (char*)FLASH_USER_START_ADDR;                          // Con trỏ đến mã PIN trong Flash
    if (flash_pin[0] != (char)0xFF) {                                         // Kiểm tra Flash không trống
        strncpy(correct_PIN, flash_pin, 9);                                  // Sao chép mã PIN
        correct_PIN[9] = '\0';                                             // Thêm ký tự kết thúc chuỗi
    }
}

void LCD_Print(uint8_t row, uint8_t col, const char* text) {                    // Hàm in ra LCD
    osMutexAcquire(lcdMutexHandle, osWaitForever);                               // Lấy Mutex LCD
    CLCD_I2C_SetCursor(&LCD1, row, col);                                         // Đặt con trỏ LCD
    CLCD_I2C_WriteString(&LCD1, (char*)text);                                    // In mã PIN
    osMutexRelease(lcdMutexHandle);                                              // Giải phóng Mutex LCD
}

void LCD_Clear(void) {                                                          // Hàm xóa LCD
    osMutexAcquire(lcdMutexHandle, osWaitForever);                               // Lấy Mutex LCD
    CLCD_I2C_Clear(&LCD1);                                                       // Xóa LCD
    osMutexRelease(lcdMutexHandle);                                              // Giải phóng Mutex LCD
}

// Hàm ngắt UART: Bắt lệnh từ ESP32
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {  // Kiểm tra UART2
        if (rx_byte == '\n' || rx_byte == '\r') {  // Kiểm tra ký tự kết thúc
            rx_buffer[rx_index] = '\0';  // Thêm ký tự kết thúc chuỗi
            if (rx_index > 0) {  // Kiểm tra buffer không trống
                if (strcmp(rx_buffer, "UNLOCK_DOOR") == 0) {  // So sánh "UNLOCK_DOOR"
                    InputData_t remoteData = {SRC_REMOTE_UNLOCK, 1};  // Tạo dữ liệu Remote Unlock
                    osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);  // Gửi dữ liệu vào Queue
                }
                else if (strncmp(rx_buffer, "ENROLL:", 7) == 0) {  // So sánh "ENROLL:"
                    int id = atoi(&rx_buffer[7]);  // Lấy ID vân tay
                    if (id > 0) {  // Kiểm tra ID > 0
                        InputData_t remoteData = {SRC_REMOTE_ENROLL, id};  // Tạo dữ liệu Remote Enroll
                        osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);  // Gửi dữ liệu vào Queue
                    }
                }
                else if (strncmp(rx_buffer, "DELETE:", 7) == 0) {  // So sánh "DELETE:"
                    int id = atoi(&rx_buffer[7]);  // Lấy ID vân tay
                    if (id > 0) {  // Kiểm tra ID > 0
                        InputData_t remoteData = {SRC_REMOTE_DELETE, id};  // Tạo dữ liệu Remote Delete
                        osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);  // Gửi dữ liệu vào Queue
                    }
                }
                else if (strncmp(rx_buffer, "CHANGE_PIN:", 11) == 0) {  // So sánh "CHANGE_PIN:"
                    strncpy(pending_new_pin, &rx_buffer[11], 9);  // Sao chép mã PIN mới
                    pending_new_pin[9] = '\0';  // Thêm ký tự kết thúc chuỗi
                    InputData_t remoteData = {SRC_REMOTE_CHANGE_PIN, 0};  // Tạo dữ liệu Remote Change PIN
                    osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);  // Gửi dữ liệu vào Queue
                }
            }
            rx_index = 0;  // Reset chỉ số buffer
        } else {
            if (rx_index < sizeof(rx_buffer) - 1) {  // Kiểm tra buffer không đầy
                rx_buffer[rx_index++] = rx_byte;  // Thêm ký tự vào buffer
            }
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);  // Nhận dữ liệu từ UART2
    }
}

void App_Init_FreeRTOS(void) {      // Khởi tạo FreeRTOS
    const osMutexAttr_t mutex_attr = { .name = "lcdMutex" }; // Tạo Mutex cho LCD
    lcdMutexHandle = osMutexNew(&mutex_attr);

    const osMessageQueueAttr_t q_attr = { .name = "inputQueue" }; // Tạo Queue cho Input
    inputQueueHandle = osMessageQueueNew(10, sizeof(InputData_t), &q_attr);

    const osTimerAttr_t t_attr = { .name = "autoLockTimer" }; // Tạo Timer cho AutoLock
    autoLockTimerHandle = osTimerNew(AutoLock_Callback, osTimerOnce, NULL, &t_attr);

    const osThreadAttr_t input_attr = { .name = "InputTask", .stack_size = 128 * 4, .priority = osPriorityNormal }; // Tạo Task cho Input
    inputTaskHandle = osThreadNew(Task_Input, NULL, &input_attr);

    const osThreadAttr_t auth_attr = { .name = "AuthTask", .stack_size = 256 * 4, .priority = osPriorityAboveNormal }; // Tạo Task cho Auth
    authTaskHandle = osThreadNew(Task_Auth, NULL, &auth_attr);
    
    // Tạo Semaphore và Task cho Buzzer
    const osSemaphoreAttr_t sem_attr = { .name = "buzzerSem" };    // Tạo Semaphore cho Buzzer    
    buzzerSemaphoreHandle = osSemaphoreNew(1, 0, &sem_attr);

    const osThreadAttr_t buzzer_attr = { .name = "BuzzerTask", .stack_size = 128 * 4, .priority = osPriorityNormal }; // Tạo Task cho Buzzer
    osThreadNew(Task_Buzzer, NULL, &buzzer_attr);
}

void Buzzer_Fail(void) {         // Hàm xử lý khi sai mã PIN
    if (error_count >= 3) {     // Nếu sai quá 3 lần
        // Chỉ in ra màn hình cảnh báo
        LCD_Clear();
        LCD_Print(1, 0, "!!! CANH BAO !!!");
        LCD_Print(2, 0, " KHOA BI KHOA ");
    }
    
    // Nhả Semaphore để đánh thức luồng Task_Buzzer thức dậy hú còi
    osSemaphoreRelease(buzzerSemaphoreHandle);
}

// TASK 1: QUÉT CẢM BIẾN
void Task_Input(void *argument) {  // Task xử lý quét cảm biến và lấy dữ liệu nhập
    InputData_t data;               // Tạo biến Data để chứa dữ liệu
    for(;;) {                       // Vòng lặp vô tận
        if (!isEnrolling) {         // Nếu không ở chế độ đăng ký
            int fingerID = AS608_CheckFingerprint();     // Quét dấu vân tay
            static int lastID = 0;                       // Biến lưu ID cuối cùng

            if ((fingerID > 0) && (fingerID != lastID)) { // Nếu có vân tay đúng
                data.dataSource = SRC_FINGER;               // Lưu dữ liệu
                data.value = (uint16_t)fingerID;            // Lưu ID
                osMessageQueuePut(inputQueueHandle, &data, 0, 100); // Gửi dữ liệu vào Queue
                lastID = fingerID;                          // Lưu ID
                osDelay(1000);                              // Chờ 1 giây
            }
            else if ((fingerID == -1) && (lastID != -1)) { // Nếu không có vân tay
                data.dataSource = SRC_FINGER_FAIL;          // Lưu dữ liệu
                data.value = 0;                             // Lưu ID
                osMessageQueuePut(inputQueueHandle, &data, 0, 100);  // Gửi dữ liệu vào Queue
                lastID = -1;                                // Cập nhật lại ID
            }
            else if (fingerID == 0) {                     // Nếu không có vân tay
            	lastID = 0;                                   // Cập nhật lại ID
            }
        }

        static char lastKey = 0;                          // Biến lưu phím cuối cùng
        char key = Keypad_GetKey();                       // Lấy phím từ bàn phím   
        if (key != 0 && key != lastKey) {                 // Nếu có phím bấm
            data.dataSource = SRC_KEYPAD;                   // Lưu dữ liệu
            data.value = key;                               // Lưu ID
            osMessageQueuePut(inputQueueHandle, &data, 0, 0);   // Gửi dữ liệu vào Queue
        }
        lastKey = key;                                    // Cập nhật lại lastKey
        osDelay(150);                                     // Chờ 150ms
    }
}

// TASK 2: XỬ LÝ CHÍNH
void Task_Auth(void *argument) {                                // Khởi tạo task xử lý chính
    Load_PIN_From_Flash(); // Đọc mã PIN từ bộ nhớ Flash chống mất điện
    CLCD_I2C_Init(&LCD1, &hi2c1, 0x4E, 16, 2);                   // Khởi tạo LCD
    LCD_Clear();                                                  // Xóa màn hình
    LCD_Print(2, 0, "SMART LOCK");                              // In dòng "SMART LOCK"
    LCD_Print(3, 1, "SAN SANG...");                             // In dòng "SAN SANG..."

    // KÍCH HOẠT LẮNG NGHE LỆNH TỪ APP NGAY KHI KHỞI ĐỘNG
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    InputData_t rxData;                                // Tạo biến Data để chứa dữ liệu
    uint8_t systemMode = 0;                             // Tạo biến systemMode để chứa chế độ hệ thống

    for(;;) {                                            // Vòng lặp vô tận
        if (osMessageQueueGet(inputQueueHandle, &rxData, NULL, osWaitForever) == osOK) { // Nếu nhận được dữ liệu
            bool unlock = false;                            // Tạo biến unlock để chứa trạng thái khóa
            char msg_success[40] = "";                      // Tạo biến msg_success để chứa thông báo

            if (rxData.dataSource == SRC_SYSTEM) {          // Nếu dữ liệu từ system
                if (rxData.value == EVT_AUTOLOCK) {         // Nếu dữ liệu là EVT_AUTOLOCK
                    Lock_Close();                               // Đóng khóa
                    LCD_Clear();                                // Xóa màn hình
                    LCD_Print(2,0,"DA TU KHOA");                // In dòng "DA TU KHOA"
                    osDelay(800);                               // Chờ 800ms
                    LCD_Clear();                                // Xóa màn hình
                    LCD_Print(2,0,"SMART LOCK");              // In dòng "SMART LOCK"
                    LCD_Print(3,1,"SAN SANG...");             // In dòng "SAN SANG..."
                }
                continue;                                       // Tiếp tục vòng lặp
            }

            // LỆNH TỪ APP: YÊU CẦU ĐĂNG KÝ VÂN TAY TỪ XA
            if (rxData.dataSource == SRC_REMOTE_ENROLL) {   // Nếu dữ liệu từ remote
                uint16_t enroll_id = rxData.value;          // Lấy ID từ data
                LCD_Clear();                                // Xóa màn hình
                char buf[32];                               // Tạo buffer để chứa chuỗi
                sprintf(buf, "ID %d: DAT TAY", enroll_id);    // Tạo chuỗi thông báo
                LCD_Print(0, 0, buf);                       // In chuỗi thông báo

                isEnrolling = true;                         // Set cờ
                int res = AS608_Enroll(enroll_id);          // Đăng ký vân tay
                isEnrolling = false;                        // Reset cờ

                if (res == 1) {                             // Nếu đăng ký thành công
                    char msg[32];                          // Tạo buffer để chứa chuỗi
                    sprintf(msg, "ENROLL_SUCCESS:%d\n", enroll_id);  // Tạo chuỗi thông báo
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);  // Gửi chuỗi thông báo
                }

                LCD_Clear();                                // Xóa màn hình
                LCD_Print(2, 0, "SMART LOCK");              // In dòng "SMART LOCK"
                LCD_Print(3, 1, "SAN SANG...");             // In dòng "SAN SANG..."
                systemMode = 0;                             // Reset systemMode
                continue;                                   // Tiếp tục vòng lặp
            }

            // LỆNH TỪ APP: YÊU CẦU XÓA VÂN TAY TỪ XA
            if (rxData.dataSource == SRC_REMOTE_DELETE) {   // Nếu dữ liệu từ remote
                uint16_t del_id = rxData.value;          // Lấy ID từ data
                AS608_DeleteChar(del_id);                  // Xóa khỏi bộ nhớ Flash

                LCD_Clear();                                // Xóa màn hình
                LCD_Print(1, 0, "XOA ID OK!");              // In dòng "XOA ID OK!"
                osDelay(1500);                              // Chờ 1500ms

                LCD_Clear();                                // Xóa màn hình
                LCD_Print(2, 0, "SMART LOCK");              // In dòng "SMART LOCK"
                LCD_Print(3, 1, "SAN SANG...");             // In dòng "SAN SANG..."
                systemMode = 0;                             // Reset systemMode
                continue;                                   // Tiếp tục vòng lặp
            }

            // LỆNH TỪ APP: YÊU CẦU ĐỔI PIN
            if (rxData.dataSource == SRC_REMOTE_CHANGE_PIN) {   // Nếu dữ liệu từ remote
                strcpy(correct_PIN, pending_new_pin);       // Copy PIN mới vào correct_PIN
                Save_PIN_To_Flash(correct_PIN);             // Lưu vĩnh viễn vào Flash
                
                LCD_Clear();                                // Xóa màn hình
                LCD_Print(1, 0, "DOI PIN OK!");             // In dòng "DOI PIN OK!"
                osDelay(1500);                              // Chờ 1500ms

                LCD_Clear();                                // Xóa màn hình
                LCD_Print(2, 0, "SMART LOCK");              // In dòng "SMART LOCK"
                LCD_Print(3, 1, "SAN SANG...");             // In dòng "SAN SANG..."
                systemMode = 0;                             // Reset systemMode
                continue;                                   // Tiếp tục vòng lặp
            }

            // LỆNH TỪ APP: MỞ KHÓA
            if (rxData.dataSource == SRC_REMOTE_UNLOCK) {   // Nếu dữ liệu từ remote
                unlock = true;                              // Mở khóa
                // Nếu mở từ App thì không gửi báo cáo lại cho App nữa (tránh vòng lặp)
                strcpy(msg_success, "");                  // Xóa thông báo
            }

            // CHỌN CHẾ ĐỘ
            else if (systemMode == 0) {               // Nếu systemMode là 0
                if (rxData.dataSource == SRC_KEYPAD) {  // Nếu dữ liệu từ bàn phím
                    char key = (char)rxData.value;      // Lấy ký tự từ data
                    if (key == 'A') {                   // Nếu là 'A'
                        systemMode = 1;                 // Chuyển sang chế độ vân tay
                        LCD_Clear();                    // Xóa màn hình
                        LCD_Print(1, 0, "CHE DO: VAN TAY");
                        LCD_Print(3, 1, "SAN SANG...");
                        osDelay(1500);                  // Chờ 1500ms
                        pin_index = 0;                  // Reset pin_index
                    }
                    else if (key == 'B') {              // Nếu là 'B'
                        systemMode = 2;                 // Chuyển sang chế độ PIN
                        LCD_Clear();                    // Xóa màn hình
                        LCD_Print(0, 0, "NHAP PIN:");   // In dòng "NHAP PIN:"
                        pin_index = 0;                  // Reset pin_index
                    }
                }
            }
            // CHẾ ĐỘ VÂN TAY
            else if (systemMode == 1) {  // Nếu systemMode là 1
                if (rxData.dataSource == SRC_FINGER && rxData.value > 0) {  // Nếu dữ liệu từ vân tay
                    unlock = true;  // Mở khóa
                    // Báo cáo lên App: Truyền số ID vân tay
                    sprintf(msg_success, "DOOR_OPENED:FINGER:%d\n", rxData.value);
                }
                else if (rxData.dataSource == SRC_FINGER_FAIL) {  // Nếu dữ liệu từ vân tay thất bại
                	error_count++;  // Tăng số lần thất bại
                	Buzzer_Fail();  // Báo động
                    LCD_Clear();    // Xóa màn hình
                    LCD_Print(1, 0, "VANTAY KHONG HOP");
                    osDelay(2000);  // Chờ 2000ms
                    LCD_Clear();    // Xóa màn hình
                    LCD_Print(1, 0, "CHE DO: VAN TAY");  // In dòng "CHE DO: VAN TAY"
                    LCD_Print(3, 1, "SAN SANG...");      // In dòng "SAN SANG..."
                }
                else if (rxData.dataSource == SRC_KEYPAD) {  // Nếu dữ liệu từ bàn phím
                    if ((char)rxData.value == '*') {  // Nếu là '*'
                        LCD_Clear();  // Xóa màn hình
                        LCD_Print(2, 0, "SMART LOCK");  // In dòng "SMART LOCK"
                        LCD_Print(3, 1, "SAN SANG...");  // In dòng "SAN SANG..."
                        systemMode = 0;  // Reset systemMode
                    }
                }
            }
            // CHẾ ĐỘ BÀN PHÍM
            else if (systemMode == 2) {  // Nếu systemMode là 2
                if (rxData.dataSource == SRC_KEYPAD) {  // Nếu dữ liệu từ bàn phím
                    char key = (char)rxData.value;  // Lấy ký tự từ data
                    if (key == '*') {  // Nếu là '*'
                        systemMode = 0;  // Reset systemMode
                        pin_index = 0;  // Reset pin_index
                        LCD_Clear();  // Xóa màn hình
                        LCD_Print(2, 0, "SMART LOCK");  // In dòng "SMART LOCK"
                        LCD_Print(3, 1, "SAN SANG...");  // In dòng "SAN SANG..."
                        osDelay(300);  // Chờ 300ms
                    }
                    else if (key == 'D') {  // Nếu là 'D'
                        entered_PIN[pin_index] = '\0';  // Kết thúc chuỗi
                        if (strcmp(entered_PIN, correct_PIN) == 0) {  // Nếu mã PIN đúng
                            unlock = true;  // Mở khóa
                            sprintf(msg_success, "DOOR_OPENED:PIN\n");  // Gửi thông báo
                        } else {  // Nếu mã PIN sai
                        	error_count++;  // Tăng số lần thất bại
                        	Buzzer_Fail();  // Báo động
                            LCD_Clear();  // Xóa màn hình
                            LCD_Print(2, 0, "SAI MA PIN!");  // In dòng "SAI MA PIN!"
                            osDelay(2000);  // Chờ 2000ms
                            LCD_Clear();  // Xóa màn hình
                            LCD_Print(0, 0, "NHAP LAI:");  // In dòng "NHAP LAI:"
                        }
                        pin_index = 0;  // Reset pin_index
                    }
                    else if (key == 'C') {  // Nếu là 'C'
                        pin_index = 0;  // Reset pin_index
                        LCD_Clear();  // Xóa màn hình
                        LCD_Print(0, 0, "NHAP LAI:");  // In dòng "NHAP LAI:"
                    }
                    else if (pin_index < 9) {  // Nếu pin_index nhỏ hơn 9
                        entered_PIN[pin_index++] = key;  // Thêm ký tự vào mã PIN
                        LCD_Print(pin_index, 1, "*");  // In ký tự '*' vào màn hình
                    }
                }
            }

            // MỞ KHÓA THỰC TẾ
            if (unlock) {   
            	error_count = 0;  // Reset số lần thất bại
                LCD_Clear();  // Xóa màn hình
                LCD_Print(4, 0, "MO CUA");  // In dòng "MO CUA"
                Lock_Open();  // Mở khóa

                // Báo cáo lên App nếu chuỗi phản hồi không rỗng
                if (strlen(msg_success) > 0) {  // Nếu chuỗi msg_success không rỗng
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg_success, strlen(msg_success), 100);  // Gửi dữ liệu lên App
                }

                osTimerStop(autoLockTimerHandle);  // Dừng timer tự động khóa
                osTimerStart(autoLockTimerHandle, 3000);  // Bắt đầu timer tự động khóa
                systemMode = 0;  // Reset systemMode
            }
        }
    }
}

void AutoLock_Callback(void *argument) {    // Hàm callback cho timer tự động khóa
    InputData_t ev;     // Tạo một biến InputData_t
    ev.dataSource = SRC_SYSTEM;  // Set data source là SRC_SYSTEM
    ev.value = EVT_AUTOLOCK;  // Set value là EVT_AUTOLOCK
    osMessageQueuePut(inputQueueHandle, &ev, 0, 0);  // Gửi dữ liệu vào input queue
}

// TASK 3: LUỒNG RIÊNG CHO CÒI BUZZER
void Task_Buzzer(void *argument) {  // Hàm callback cho timer tự động khóa      
    for(;;) {  // Vòng lặp vô hạn
        // Task sẽ đứng đợi ở đây mãi mãi (osWaitForever) cho đến khi có ai "nhả"  Semaphore
        if (osSemaphoreAcquire(buzzerSemaphoreHandle, osWaitForever) == osOK) {  // Kiểm tra semaphore
            if (error_count >= 3) {  // Nếu số lần thất bại lớn hơn hoặc bằng 3
                // Sai 3 lần: Hú dài 5 giây (Buzzer Active-High: SET là BẬT, RESET là TẮT)
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); // Bật còi
                osDelay(5000); 
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);   // Tắt còi
                
                error_count = 0; // Tự reset lại bộ đếm sau khi hú xong

            } else {
                // Sai 1 hoặc 2 lần: Kêu 'Bíp' ngắn 200ms
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); // Bật còi
                osDelay(200);
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);   // Tắt còi
            }
        }
    }
}
