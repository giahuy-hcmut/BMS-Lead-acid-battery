/*
 * BMS_CAN.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */


#include "BMS_CAN.h"
#include "Board_Config.h"   // CAN_MASTER_ID
#include "Shared_Data.h"    // BMS_Data_SetCurrent - the RX ISR pushes the pack
                            // current straight into the store, so that
                            // GetSnapshot() still hands out voltage+current
                            // from one consistent instant.
#include <stddef.h>         // NULL


static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;

/* The CAN handle, owned by this driver. main() hands it over once via
 * BMS_CAN_Init(); after that no other file needs to name a HAL type. */
static CAN_HandleTypeDef *s_hcan = NULL;

/* Timestamp of the last frame received from the master. Written by the RX
 * ISR, read by Task_Sleep through BMS_CAN_GetLastRxTick().
 * `volatile`: an ISR writes it while a task reads it.
 * A 32-bit aligned load/store is a single LDR/STR on Cortex-M3, so this
 * needs no critical section. */
static volatile uint32_t s_last_rx_tick = 0;

void BMS_CAN_Init(CAN_HandleTypeDef *hcan) {
    s_hcan = hcan;

    // Cấu hình Header mặc định
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.TransmitGlobalTime = DISABLE;
}

void BMS_CAN_InitRx(void) {
    if (s_hcan == NULL) {
        return;                     // BMS_CAN_Init chua duoc goi
    }

    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank           = 0;
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale          = CAN_FILTERSCALE_32BIT;
    /* Chi nhan frame 0x100 tu master. Mask mo (0x0000) truoc day nhan MOI ID,
     * nen frame cua cac slave anh em cung sinh ngat RX -> danh thuc WFI cua
     * Task_Sleep -> slave khong bao gio ngu duoc du master da ngung 0x100. */
    sFilterConfig.FilterIdHigh         = (CAN_MASTER_ID << 5);  // 0x100<<5 = 0x2000
    sFilterConfig.FilterIdLow          = 0x0000;
    sFilterConfig.FilterMaskIdHigh     = (0x7FF << 5);          // 0xFFE0 - khop du 11 bit ID
    sFilterConfig.FilterMaskIdLow      = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation     = ENABLE;
    HAL_CAN_ConfigFilter(s_hcan, &sFilterConfig);
    HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];

    /* Uses the handle HAL passed in, not s_hcan: the ISR must work off
     * whichever peripheral actually fired.
     * Check the return: on failure RxHeader and RxData are uninitialised
     * stack garbage that could match CAN_MASTER_ID by chance and feed a
     * random current into the filter. */
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
        return;
    }

    if (RxHeader.StdId == CAN_MASTER_ID) {
        s_last_rx_tick = HAL_GetTick();     /* frame 0x100 doubles as heartbeat */

        /* Pack current: MSB first, int16 two's complement, A x100 - the same
         * convention as bytes 2-3 of this slave's own frame.
         * The DLC test keeps this build working against a master that still
         * sends the old empty heartbeat (current then stays at its last value
         * until Task_SOC's staleness timeout zeroes it).
         * `* 0.01f` not `/ 100.0f`: a softfloat multiply is ~2.5x cheaper than
         * a divide, which matters in interrupt context. The ~1e-9 relative
         * difference is irrelevant for a current in amperes. */
        if (RxHeader.DLC >= 2U) {
            int16_t raw = (int16_t)(((uint16_t)RxData[0] << 8) | (uint16_t)RxData[1]);
            BMS_Data_SetCurrent((float)raw * 0.01f);
        }
    }
}

uint32_t BMS_CAN_GetLastRxTick(void) {
    return s_last_rx_tick;
}

uint8_t BMS_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t len) {
    if (s_hcan == NULL) {
        return 0; // BMS_CAN_Init chua duoc goi
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) == 0) {
        return 0; // Bus bận, không gửi được
    }

    TxHeader.StdId = id;
    TxHeader.DLC = len;

    if (HAL_CAN_AddTxMessage(s_hcan, &TxHeader, data, &TxMailbox) != HAL_OK) {
        return 0; // Lỗi HAL
    }
    return 1; // Gửi thành công
}
