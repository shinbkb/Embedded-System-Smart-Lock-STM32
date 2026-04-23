#include "app_freertos.h"
#include "cmsis_os.h"
#include "keypad.h"
#include "as608.h"
#include "servo.h"
#include "CLCD_I2C.h"
#include <stdbool.h>
#include <string.h>

// --- THÊM DÒNG NÀY ĐỂ LẤY BIẾN I2C TỪ MAIN ---
extern I2C_HandleTypeDef hi2c1;
volatile bool isEnrolling = false;

// Các handle của FreeRTOS
osThreadId_t inputTaskHandle;
osThreadId_t authTaskHandle;
osMessageQueueId_t inputQueueHandle;
osTimerId_t autoLockTimerHandle;
osMutexId_t lcdMutexHandle;      // ← THÊM MUTEX CHO LCD

// Khai báo biến LCD toàn cục
CLCD_I2C_Name LCD1;

uint8_t error_count = 0;
const char correct_PIN[] = "1234";
char entered_PIN[10];
uint8_t pin_index = 0;

// ← THÊM MẢNG LƯU TRỮ ID ĐÃ ĐĂNG KÝ (CÓ THỂ LƯU VÀO FLASH THẬT TỪ CẢM BIẾN)
#define MAX_ENROLLED_IDS 10
uint16_t enrolledIDs[MAX_ENROLLED_IDS];
uint8_t enrolledIDCount = 0;

typedef struct {
    uint8_t dataSource;
    uint16_t value;
} InputData_t;
#define SRC_KEYPAD       0
#define SRC_FINGER       1
#define SRC_FINGER_FAIL  3
#define SRC_SYSTEM       99   //

#define EVT_AUTOLOCK     1  //


void Task_Input(void *argument);
void Task_Auth(void *argument);
void AutoLock_Callback(void *argument);

// ← HÀM HỖ TRỢ: IN LÊN LCD VỚI MUTEX
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

// ← HÀM HỖ TRỢ: KIỂM TRA ID CÓ TRONG DANH SÁCH KHÔNG
bool IsIDEnrolled(uint16_t id) {
    for (int i = 0; i < enrolledIDCount; i++) {
        if (enrolledIDs[i] == id) {
            return true;
        }
    }
    return false;
}

// ← HÀM HỖ TRỢ: THÊM ID VÀO DANH SÁCH
bool AddID(uint16_t id) {
    if (enrolledIDCount >= MAX_ENROLLED_IDS) return false;
    enrolledIDs[enrolledIDCount++] = id;
    return true;
}

// ← HÀM HỖ TRỢ: XÓA ID KHỎI DANH SÁCH
bool RemoveID(uint16_t id) {
    for (int i = 0; i < enrolledIDCount; i++) {
        if (enrolledIDs[i] == id) {
            for (int j = i; j < enrolledIDCount - 1; j++) {
                enrolledIDs[j] = enrolledIDs[j + 1];
            }
            enrolledIDCount--;
            return true;
        }
    }
    return false;
}

void App_Init_FreeRTOS(void) {
    // ← THÊM MUTEX
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
}

// --- TASK 1: QUÉT CẢM BIẾN ---
void Task_Input(void *argument) {
    InputData_t data;
    for(;;) {
        if (!isEnrolling) {
            int fingerID = AS608_CheckFingerprint();
            static int lastID = -1;

            if ((fingerID > 0) && (fingerID != lastID)) {
                data.dataSource = SRC_FINGER;
                data.value = (uint16_t)fingerID;
                osMessageQueuePut(inputQueueHandle, &data, 0, 100);
                osDelay(1000);
            }
            else if (fingerID == -1) {
                data.dataSource = SRC_FINGER_FAIL;
                data.value = 0;
                osMessageQueuePut(inputQueueHandle, &data, 0, 100);
                osDelay(1000);
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
        osDelay(50);
    }
}
// --- TASK 2: XỬ LÝ CHÍNH ---
void Task_Auth(void *argument) {
    // 1. Khởi tạo LCD
    CLCD_I2C_Init(&LCD1, &hi2c1, 0x4E, 16, 2);
    LCD_Clear();
    LCD_Print(2, 0, "SMART LOCK");
    LCD_Print(3, 1, "SAN SANG...");

    InputData_t rxData;
    uint8_t systemMode = 0; // 0: Chờ chế độ, 1: Chế độ vân tay, 2: Chế độ bàn phím

    for(;;) {
        if (osMessageQueueGet(inputQueueHandle, &rxData, NULL, osWaitForever) == osOK) {
            bool unlock = false;
            if (rxData.dataSource == SRC_SYSTEM) {
                switch(rxData.value) {
                    case EVT_AUTOLOCK:
                        Lock_Close();
                        LCD_Clear();
                        LCD_Print(2,0,"DA TU KHOA");
                        osDelay(800);
                        LCD_Clear();
                        LCD_Print(2,0,"SMART LOCK");
                        LCD_Print(3,1,"SAN SANG...");
                        break;
                }
                continue;
            }
            // ← TRẠNG THÁI: CHỌN CHẾ ĐỘ (systemMode == 0)
            else if (systemMode == 0) {
                if (rxData.dataSource == 0) {
                    char key = (char)rxData.value;
                    if (key == 'A') {
                        // Chế độ vân tay
                        systemMode = 1;
                        LCD_Clear();
                        LCD_Print(1, 0, "CHE DO: VAN TAY");
                        LCD_Print(3, 1, "SAN SANG...");
                        osDelay(1500);
                        pin_index = 0;
                    }
                    else if (key == 'B') {
                        // Chế độ bàn phím
                        systemMode = 2;
                        LCD_Clear();
                        LCD_Print(0, 0, "NHAP PIN:");
                        pin_index = 0;
                    }
                    else if (key == 'C') {
                        // Chế độ quản lý vân tay
                        // ← NHẬP CHẾ ĐỘ QUẢN LÝ ID
                        LCD_Clear();
                        LCD_Print(0, 0, "QUAN LY ID:");
                        LCD_Print(1, 0, "A: Them ID");
                        LCD_Print(3, 0, "B: Xoa ID");
                        osDelay(1000);

                        char mgmtKey = 0;
                        while (mgmtKey == 0) {
                            InputData_t mgmtData;
                            if (osMessageQueueGet(inputQueueHandle, &mgmtData, NULL, 500) == osOK) {
                                if (mgmtData.dataSource == 0) {
                                    mgmtKey = (char)mgmtData.value;
                                }
                            }
                        }
                        if (mgmtKey == '*')   // ← EXIT
                                            {
                                                systemMode = 0;
                                                pin_index = 0;

                                                LCD_Clear();
                                                LCD_Print(2, 0, "SMART LOCK");
                                                LCD_Print(3, 1, "SAN SANG...");
                                                osDelay(300);
                                            }
                        else if (mgmtKey == 'A') {
                            // Thêm ID (gọi chế độ đăng ký)
                            isEnrolling = true;
                            LCD_Clear();
                            LCD_Print(0, 0, "NHAP ID(1-300):");
                            char id_str[4] = "";
                            uint8_t id_idx = 0;
                            bool cancel = false;

                            while (1) {
                                InputData_t enrollData;
                                if (osMessageQueueGet(inputQueueHandle, &enrollData, NULL, osWaitForever) == osOK) {
                                    if (enrollData.dataSource == 0) {
                                        char k = (char)enrollData.value;

                                        if (k >= '0' && k <= '9' && id_idx < 3) {
                                            id_str[id_idx++] = k;
                                            id_str[id_idx] = '\0';
                                            LCD_Print(6, 1, id_str);
                                        }
                                        else if (k == 'C') {
                                            id_idx = 0;
                                            id_str[0] = '\0';
                                            LCD_Print(6, 1, "   ");
                                        }
                                        else if (k == 'A') {
                                            cancel = true;
                                            break;
                                        }
                                        else if (k == 'D') {
                                            break;
                                        }
                                    }
                                }
                            }

                            if (!cancel && id_idx > 0) {
                                int enroll_id = 0;
                                for(int i = 0; i < id_idx; i++) {
                                    enroll_id = enroll_id * 10 + (id_str[i] - '0');
                                }

                                if (enroll_id >= 1 && enroll_id <= 300) {
                                    AS608_Enroll(enroll_id);
                                    AddID(enroll_id); // ← THÊM VÀO DANH SÁCH
                                } else {
                                    LCD_Clear();
                                    LCD_Print(0, 0, "ID KHONG HOP LE!");
                                    osDelay(2000);
                                }
                            }

                            isEnrolling = false;
                            LCD_Clear();
                            LCD_Print(2, 0, "SMART LOCK");
                            LCD_Print(3, 1, "SAN SANG...");
                            systemMode = 0;
                        }
                        else if (mgmtKey == 'B') {
                            // Xóa ID
                            LCD_Clear();
                            LCD_Print(0, 0, "NHAP ID XOA:");
                            char id_str[4] = "";
                            uint8_t id_idx = 0;

                            while (1) {
                                InputData_t enrollData;
                                if (osMessageQueueGet(inputQueueHandle, &enrollData, NULL, osWaitForever) == osOK) {
                                    if (enrollData.dataSource == 0) {
                                        char k = (char)enrollData.value;

                                        if (k >= '0' && k <= '9' && id_idx < 3) {
                                            id_str[id_idx++] = k;
                                            id_str[id_idx] = '\0';
                                            LCD_Print(6, 1, id_str);
                                        }
                                        else if (k == 'C') {
                                            id_idx = 0;
                                            id_str[0] = '\0';
                                            LCD_Print(6, 1, "   ");
                                        }
                                        else if (k == 'D') {
                                            break;
                                        }
                                    }
                                }
                            }

                            if (id_idx > 0) {
                                int delete_id = 0;
                                for(int i = 0; i < id_idx; i++) {
                                    delete_id = delete_id * 10 + (id_str[i] - '0');
                                }

                                if (RemoveID(delete_id)) {
                                    LCD_Clear();
                                    LCD_Print(1, 0, "XOA ID OK!");
                                    osDelay(1500);
                                } else {
                                    LCD_Clear();
                                    LCD_Print(0, 0, "ID KHONG TON TAI!");
                                    osDelay(1500);
                                }
                            }

                            LCD_Clear();
                            LCD_Print(2, 0, "SMART LOCK");
                            LCD_Print(3, 1, "SAN SANG...");
                            systemMode = 0;
                        }
                    }
                }
            }
            // ← TRẠNG THÁI: CHẾ ĐỘ VÂN TAY (systemMode == 1)
            else if (systemMode == 1) {
                if (rxData.dataSource == 1 && rxData.value > 0) {
                    unlock = true;
                }
                else if (rxData.dataSource == 3) {
                    LCD_Clear();
                    LCD_Print(1, 0, "VANTAY KHONG HOP");
                    osDelay(2000);
                    LCD_Clear();
                    LCD_Print(1, 0, "CHE DO: VAN TAY");
                    LCD_Print(3, 1, "SAN SANG...");
                }
                else if (rxData.dataSource == 0) {
                    char key = (char)rxData.value;
                    if (key == '*') {
                        // Quay về menu chính
                        LCD_Clear();
                        LCD_Print(2, 0, "SMART LOCK");
                        LCD_Print(3, 1, "SAN SANG...");
                        systemMode = 0;
                    }
                }

            }
            // ← TRẠNG THÁI: CHẾ ĐỘ BÀN PHÍM (systemMode == 2)
            else if (systemMode == 2) {
                if (rxData.dataSource == 0) {
                    char key = (char)rxData.value;
                    if (key == '*')   // ← EXIT
                    {
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
                        } else {
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
                        LCD_Print(pin_index, 1, "*"); // Hiển thị * thay vì mật khẩu thật
                    }

                }
            }

            if (unlock) {
                LCD_Clear();
                LCD_Print(4, 0, "MO CUA");
                Lock_Open();
                osTimerStop(autoLockTimerHandle);
                if (osTimerStart(autoLockTimerHandle, 3000) != osOK) {
                    // debug ở đây
                }                systemMode = 0; // ← QUAY VỀ MENU CHÍNH
            }
        }
    }
}


// --- CALLBACK TIMER: TỰ ĐỘNG KHÓA ---
void AutoLock_Callback(void *argument)
{
    InputData_t ev;
    ev.dataSource = SRC_SYSTEM;
    ev.value = EVT_AUTOLOCK;

    if (osMessageQueuePut(inputQueueHandle, &ev, 0, 0) != osOK) {
        // queue full → mất event
    }
}
