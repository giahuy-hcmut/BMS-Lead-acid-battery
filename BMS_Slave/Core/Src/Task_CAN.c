/*
 * Task_CAN.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#include "Task_CAN.h"
#include "Shared_Data.h"
#include "BMS_CAN.h"
#include "main.h"

extern CAN_HandleTypeDef hcan;

void Task_CAN_Run(void) {
    uint8_t txData[8] = {0};

    // --- 1. ĐÓNG GÓI ĐIỆN ÁP (Byte 0-1) ---
    uint16_t v_send = (uint16_t)(myBMS.voltage * 100);
    txData[0] = (v_send >> 8) & 0xFF;
    txData[1] = v_send & 0xFF;

    // --- 2. ĐÓNG GÓI DÒNG ĐIỆN (Byte 2-3) ---
    int16_t i_send = (int16_t)(myBMS.current * 100);
    txData[2] = (i_send >> 8) & 0xFF;
    txData[3] = i_send & 0xFF;

    // --- 3. CÁC THÔNG SỐ KHÁC ---
    txData[4] = myBMS.soc;
    txData[5] = (uint8_t)(myBMS.temp + 40); // Offset nhiệt độ
    txData[6] = myBMS.status;

    // --- 4. GỬI ĐI ---
    BMS_CAN_Transmit(&hcan, CAN_SLAVE_ID, txData, 8);

    // Nháy đèn báo hiệu đã gửi
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}
