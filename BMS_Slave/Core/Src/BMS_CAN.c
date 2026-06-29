/*
 * BMS_CAN.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */


#include "BMS_CAN.h"


static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;

void BMS_CAN_Init(CAN_HandleTypeDef *hcan) {
    // Cấu hình Header mặc định
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.TransmitGlobalTime = DISABLE;
}

uint8_t BMS_CAN_Transmit(CAN_HandleTypeDef *hcan, uint32_t id, uint8_t *data, uint8_t len) {
    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {
        return 0; // Bus bận, không gửi được
    }

    TxHeader.StdId = id;
    TxHeader.DLC = len;

    if (HAL_CAN_AddTxMessage(hcan, &TxHeader, data, &TxMailbox) != HAL_OK) {
        return 0; // Lỗi HAL
    }
    return 1; // Gửi thành công
}
