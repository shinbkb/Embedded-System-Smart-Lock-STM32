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

extern I2C_HandleTypeDef hi2c1;
volatile bool isEnrolling = false;
extern UART_HandleTypeDef huart2;

// Các handle của FreeRTOS
osThreadId_t inputTaskHandle;
osThreadId_t authTaskHandle;
osMessageQueueId_t inputQueueHandle;
osTimerId_t autoLockTimerHandle;
osMutexId_t lcdMutexHandle;
osSemaphoreId_t buzzerSemaphoreHandle;

CLCD_I2C_Name LCD1;
uint8_t error_count = 0;
const char correct_PIN[] = "1234";
char entered_PIN[10];
uint8_t pin_index = 0;

// Các định dạng nguồn phát lệnh
typedef struct {
    uint8_t dataSource;
    uint16_t value;
} InputData_t;

#define SRC_KEYPAD        0
#define SRC_FINGER        1
#define SRC_FINGER_FAIL   3
#define SRC_REMOTE_UNLOCK 4
#define SRC_REMOTE_ENROLL 5
#define SRC_REMOTE_DELETE 6
#define SRC_SYSTEM        99
#define EVT_AUTOLOCK      1

void Task_Input(void *argument);
void Task_Auth(void *argument);
void AutoLock_Callback(void *argument);
void Task_Buzzer(void *argument);

// --- CÁC BIẾN CHO NGẮT UART TỪ ESP32 ---
uint8_t rx_byte;
char rx_buffer[32];
uint8_t rx_index = 0;

void LCD_Print(uint8_t row, uint8_t col, const char* text) {
    osMutexAcquire(lcdMutexHandle, osWaitForever);
    CLCD_I2C_SetCursor(&LCD1, row, col);
    CLCD_I2C_WriteString(&LCD1, text);
    osMutexRelease(lcdMutexHandle);
}

void LCD_Clear(void) {
    osMutexAcquire(lcdMutexHandle, osWaitForever);
    CLCD_I2C_Clear(&LCD1);
    osMutexRelease(lcdMutexHandle);
}

// --- HÀM NGẮT UART: BẮT LỆNH TỪ APP GỬI XUỐNG ---
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (rx_byte == '\n' || rx_byte == '\r') {
            rx_buffer[rx_index] = '\0';
            if (rx_index > 0) {
                if (strcmp(rx_buffer, "UNLOCK_DOOR") == 0) {
                    InputData_t remoteData = {SRC_REMOTE_UNLOCK, 1};
                    osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);
                }
                else if (strncmp(rx_buffer, "ENROLL:", 7) == 0) {
                    int id = atoi(&rx_buffer[7]);
                    if (id > 0) {
                        InputData_t remoteData = {SRC_REMOTE_ENROLL, id};
                        osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);
                    }
                }
                else if (strncmp(rx_buffer, "DELETE:", 7) == 0) {
                    int id = atoi(&rx_buffer[7]);
                    if (id > 0) {
                        InputData_t remoteData = {SRC_REMOTE_DELETE, id};
                        osMessageQueuePut(inputQueueHandle, &remoteData, 0, 0);
                    }
                }
            }
            rx_index = 0;
        } else {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

void App_Init_FreeRTOS(void) {
    const osMutexAttr_t mutex_attr = { .name = "lcdMutex" };
    lcdMutexHandle = osMutexNew(&mutex_attr);

    const osMessageQueueAttr_t q_attr = { .name = "inputQueue" };
    inputQueueHandle = osMessageQueueNew(10, sizeof(InputData_t), &q_attr);

    const osTimerAttr_t t_attr = { .name = "autoLockTimer" };
    autoLockTimerHandle = osTimerNew(AutoLock_Callback, osTimerOnce, NULL, &t_attr);

    const osThreadAttr_t input_attr = { .name = "InputTask", .stack_size = 128 * 4, .priority = osPriorityNormal };
    inputTaskHandle = osThreadNew(Task_Input, NULL, &input_attr);

    const osThreadAttr_t auth_attr = { .name = "AuthTask", .stack_size = 256 * 4, .priority = osPriorityAboveNormal };
    authTaskHandle = osThreadNew(Task_Auth, NULL, &auth_attr);

    // --- KHỞI TẠO SEMAPHORE VÀ TASK CHO BUZZER ---
    const osSemaphoreAttr_t sem_attr = { .name = "buzzerSem" };
    buzzerSemaphoreHandle = osSemaphoreNew(1, 0, &sem_attr);

    const osThreadAttr_t buzzer_attr = { .name = "BuzzerTask", .stack_size = 128 * 4, .priority = osPriorityNormal };
    osThreadNew(Task_Buzzer, NULL, &buzzer_attr);
}
void Buzzer_Fail(void) {
    if (error_count >= 3) {
        // Chỉ in ra màn hình cảnh báo
        LCD_Clear();
        LCD_Print(1, 0, "!!! CANH BAO !!!");
        LCD_Print(2, 0, " KHOA BI KHOA ");
    }
    
    // Nhả Semaphore để đánh thức luồng Task_Buzzer thức dậy hú còi
    osSemaphoreRelease(buzzerSemaphoreHandle);
}
// --- TASK 1: QUÉT CẢM BIẾN ---
void Task_Input(void *argument) {
    InputData_t data;
    for(;;) {
        if (!isEnrolling) {
            int fingerID = AS608_CheckFingerprint();
            static int lastID = 0;

            if ((fingerID > 0) && (fingerID != lastID)) {
                data.dataSource = SRC_FINGER;
                data.value = (uint16_t)fingerID;
                osMessageQueuePut(inputQueueHandle, &data, 0, 100);
                lastID = fingerID;
                osDelay(1000);
            }
            else if ((fingerID == -1) && (lastID != -1)) {
                data.dataSource = SRC_FINGER_FAIL;
                data.value = 0;
                osMessageQueuePut(inputQueueHandle, &data, 0, 100);
                lastID = -1;
            }
            else if (fingerID == 0) {
            	lastID = 0;
            }
        }

        static char lastKey = 0;
        char key = Keypad_GetKey();
        if (key != 0 && key != lastKey) {
            data.dataSource = SRC_KEYPAD;
            data.value = key;
            osMessageQueuePut(inputQueueHandle, &data, 0, 0);
        }
        lastKey = key;
        osDelay(150);
    }
}

// --- TASK 2: XỬ LÝ CHÍNH ---
void Task_Auth(void *argument) {
    CLCD_I2C_Init(&LCD1, &hi2c1, 0x4E, 16, 2);
    LCD_Clear();
    LCD_Print(2, 0, "SMART LOCK");
    LCD_Print(3, 1, "SAN SANG...");

    // KÍCH HOẠT LẮNG NGHE LỆNH TỪ APP NGAY KHI KHỞI ĐỘNG
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    InputData_t rxData;
    uint8_t systemMode = 0; // 0: Menu, 1: Vân tay, 2: Bàn phím

    for(;;) {
        if (osMessageQueueGet(inputQueueHandle, &rxData, NULL, osWaitForever) == osOK) {
            bool unlock = false;
            char msg_success[40] = "";

            if (rxData.dataSource == SRC_SYSTEM) {
                if (rxData.value == EVT_AUTOLOCK) {
                    Lock_Close();
                    LCD_Clear();
                    LCD_Print(2,0,"DA TU KHOA");
                    osDelay(800);
                    LCD_Clear();
                    LCD_Print(2,0,"SMART LOCK");
                    LCD_Print(3,1,"SAN SANG...");
                }
                continue;
            }

            // LỆNH TỪ APP: YÊU CẦU ĐĂNG KÝ VÂN TAY TỪ XA
            if (rxData.dataSource == SRC_REMOTE_ENROLL) {
                uint16_t enroll_id = rxData.value;
                LCD_Clear();
                char buf[16];
                sprintf(buf, "ID %d: DAT TAY", enroll_id);
                LCD_Print(0, 0, buf);

                isEnrolling = true;
                int res = AS608_Enroll(enroll_id);
                isEnrolling = false;

                if (res == 1) {
                    // Đăng ký thành công, báo cho App
                    char msg[32];
                    sprintf(msg, "ENROLL_SUCCESS:%d\n", enroll_id);
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
                }

                LCD_Clear();
                LCD_Print(2, 0, "SMART LOCK");
                LCD_Print(3, 1, "SAN SANG...");
                systemMode = 0;
                continue;
            }

            // LỆNH TỪ APP: YÊU CẦU XÓA VÂN TAY TỪ XA
            if (rxData.dataSource == SRC_REMOTE_DELETE) {
                uint16_t del_id = rxData.value;
                AS608_DeleteChar(del_id); // Xóa khỏi bộ nhớ Flash

                LCD_Clear();
                LCD_Print(1, 0, "XOA ID OK!");
                osDelay(1500);

                LCD_Clear();
                LCD_Print(2, 0, "SMART LOCK");
                LCD_Print(3, 1, "SAN SANG...");
                systemMode = 0;
                continue;
            }

            // LỆNH TỪ APP: MỞ KHÓA
            if (rxData.dataSource == SRC_REMOTE_UNLOCK) {
                unlock = true;
                // Nếu mở từ App thì không gửi báo cáo lại cho App nữa (tránh vòng lặp)
                strcpy(msg_success, "");
            }

            // CHỌN CHẾ ĐỘ
            else if (systemMode == 0) {
                if (rxData.dataSource == SRC_KEYPAD) {
                    char key = (char)rxData.value;
                    if (key == 'A') {
                        systemMode = 1;
                        LCD_Clear();
                        LCD_Print(1, 0, "CHE DO: VAN TAY");
                        LCD_Print(3, 1, "SAN SANG...");
                        osDelay(1500);
                        pin_index = 0;
                    }
                    else if (key == 'B') {
                        systemMode = 2;
                        LCD_Clear();
                        LCD_Print(0, 0, "NHAP PIN:");
                        pin_index = 0;
                    }
                    // BỎ NÚT C QUẢN LÝ Ở ĐÂY, ĐÃ DỊCH CHUYỂN LÊN APP
                }
            }
            // CHẾ ĐỘ VÂN TAY
            else if (systemMode == 1) {
                if (rxData.dataSource == SRC_FINGER && rxData.value > 0) {
                    unlock = true;
                    // Báo cáo lên App: Truyền số ID vân tay
                    sprintf(msg_success, "DOOR_OPENED:FINGER:%d\n", rxData.value);
                }
                else if (rxData.dataSource == SRC_FINGER_FAIL) {
                	error_count++;
                	Buzzer_Fail();
                    LCD_Clear();
                    LCD_Print(1, 0, "VANTAY KHONG HOP");
                    osDelay(2000);
                    LCD_Clear();
                    LCD_Print(1, 0, "CHE DO: VAN TAY");
                    LCD_Print(3, 1, "SAN SANG...");
                }
                else if (rxData.dataSource == SRC_KEYPAD) {
                    if ((char)rxData.value == '*') {
                        LCD_Clear();
                        LCD_Print(2, 0, "SMART LOCK");
                        LCD_Print(3, 1, "SAN SANG...");
                        systemMode = 0;
                    }
                }
            }
            // CHẾ ĐỘ BÀN PHÍM
            else if (systemMode == 2) {
                if (rxData.dataSource == SRC_KEYPAD) {
                    char key = (char)rxData.value;
                    if (key == '*') {
                        systemMode = 0;
                        pin_index = 0;
                        LCD_Clear();
                        LCD_Print(2, 0, "SMART LOCK");
                        LCD_Print(3, 1, "SAN SANG...");
                        osDelay(300);
                    }
                    else if (key == 'D') {
                        entered_PIN[pin_index] = '\0';
                        if (strcmp(entered_PIN, correct_PIN) == 0) {
                            unlock = true;
                            sprintf(msg_success, "DOOR_OPENED:PIN\n");
                        } else {
                        	error_count++;
                        	Buzzer_Fail();
                            LCD_Clear();
                            LCD_Print(2, 0, "SAI MA PIN!");
                            osDelay(2000);
                            LCD_Clear();
                            LCD_Print(0, 0, "NHAP LAI:");
                        }
                        pin_index = 0;
                    }
                    else if (key == 'C') {
                        pin_index = 0;
                        LCD_Clear();
                        LCD_Print(0, 0, "NHAP LAI:");
                    }
                    else if (pin_index < 9) {
                        entered_PIN[pin_index++] = key;
                        LCD_Print(pin_index, 1, "*");
                    }
                }
            }

            // MỞ KHÓA THỰC TẾ
            if (unlock) {
            	error_count = 0;
                LCD_Clear();
                LCD_Print(4, 0, "MO CUA");
                Lock_Open();

                // Báo cáo lên App nếu chuỗi phản hồi không rỗng
                if (strlen(msg_success) > 0) {
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg_success, strlen(msg_success), 100);
                }

                osTimerStop(autoLockTimerHandle);
                osTimerStart(autoLockTimerHandle, 3000);
                systemMode = 0;
            }
        }
    }
}

void AutoLock_Callback(void *argument) {
    InputData_t ev;
    ev.dataSource = SRC_SYSTEM;
    ev.value = EVT_AUTOLOCK;
    osMessageQueuePut(inputQueueHandle, &ev, 0, 0);
}

// --- TASK 3: LUỒNG RIÊNG CHO CÒI BUZZER ---
void Task_Buzzer(void *argument) {
    for(;;) {
        // Task sẽ đứng đợi ở đây mãi mãi (osWaitForever) cho đến khi có ai "nhả" Semaphore
        if (osSemaphoreAcquire(buzzerSemaphoreHandle, osWaitForever) == osOK) {
            if (error_count >= 3) {
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
