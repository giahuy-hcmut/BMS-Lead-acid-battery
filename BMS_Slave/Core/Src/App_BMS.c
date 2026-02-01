/*
 * App_BMS.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */


#include "App_BMS.h"

// Biến instance (đối tượng) BMS
 App_BMS_t myBMS;

void App_BMS_Init(void) {
    myBMS.voltage = 0.0f;
    myBMS.status = 0;
    myBMS.lastTick = 0;
}

void App_BMS_Process(ADC_HandleTypeDef *hadc, CAN_HandleTypeDef *hcan) {
    // 1. Đọc cảm biến (Luôn cập nhật giá trị mới nhất)
    myBMS.voltage = BMS_ADC_GetVoltage(hadc);

    // 2. Kiểm tra thời gian (Non-blocking delay): Gửi mỗi 100ms
    if (HAL_GetTick() - myBMS.lastTick >= 100) {

        // --- LOGIC ĐÓNG GÓI DỮ LIỆU ---
        uint8_t txData[2];

        // Nhân 100 để gửi số nguyên (48.55V -> 4855)
        uint16_t sendVal = (uint16_t)(myBMS.voltage * 100);

        txData[0] = (sendVal >> 8) & 0xFF; // Byte cao
        txData[1] = sendVal & 0xFF;        // Byte thấp

        // Gọi Driver CAN để gửi
        BMS_CAN_Transmit(hcan, CAN_SLAVE_ID, txData, 2);

        // Cập nhật thời gian
        myBMS.lastTick = HAL_GetTick();

        // (Optional) Nháy LED báo hiệu
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}
