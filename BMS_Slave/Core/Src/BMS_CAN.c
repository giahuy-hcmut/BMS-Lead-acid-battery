/*
 * BMS_CAN.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */


#include "BMS_CAN.h"
#include "Shared_Data.h"


static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;

void BMS_CAN_Init(CAN_HandleTypeDef *hcan) {
    // Cấu hình Header mặc định
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.TransmitGlobalTime = DISABLE;
}

void BMS_CAN_InitRx(CAN_HandleTypeDef *hcan) {
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank           = 0;
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale          = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh         = 0x0000;
    sFilterConfig.FilterIdLow          = 0x0000;
    sFilterConfig.FilterMaskIdHigh     = 0x0000;
    sFilterConfig.FilterMaskIdLow      = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation     = ENABLE;
    HAL_CAN_ConfigFilter(hcan, &sFilterConfig);
    HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData);
    if (RxHeader.StdId == 0x100) {
        lastHeartbeatTick = HAL_GetTick();
    }
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
