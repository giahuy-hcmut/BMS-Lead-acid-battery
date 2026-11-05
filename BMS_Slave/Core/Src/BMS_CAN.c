/**
 * @file      BMS_CAN.c
 * @author    Do Dang Gia Huy
 * @brief     CAN filter setup, RX interrupt handler and single-shot TX.
 * @date      2026-01-31
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "BMS_CAN.h"

#include "Board_Config.h"   /* CAN_MASTER_ID */
#include "Shared_Data.h"    /* BMS_Data_SetCurrent - the RX ISR pushes the pack
                             * current straight into the store, so that
                             * GetSnapshot() still hands out voltage+current
                             * from one consistent instant. */

#include <stddef.h>         /* NULL */

/**********************
 *      DEFINES
 **********************/

/* The STM32 filter registers hold a standard identifier left-aligned in the
 * upper 16 bits: bits [15:5] of FilterIdHigh. Hence the 5-bit shift. */
#define CAN_FILTER_ID_SHIFT     5U
#define CAN_STD_ID_MASK         0x7FFU

#define CAN_FILTER_BANK_MASTER  0U

/* Maximum payload of a classic CAN data frame. */
#define CAN_MAX_PAYLOAD         8U

/* Smallest payload carrying a usable pack current (bytes 0-1). */
#define CAN_CURRENT_MIN_DLC     2U

/* Pack current is sent as int16 two's complement in hundredths of an amp. */
#define CAN_CURRENT_LSB_A       0.01f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* TX header reused for every frame: only StdId and DLC change per call. */
static CAN_TxHeaderTypeDef s_tx_header;
static uint32_t            s_tx_mailbox;

/* The CAN handle, owned by this driver. main() hands it over once via
 * BMS_CAN_Init(); after that no other file needs to name a HAL type. */
static CAN_HandleTypeDef *s_hcan = NULL;

/* Timestamp of the last frame received from the master. Written by the RX
 * ISR, read by Task_Sleep through BMS_CAN_GetLastRxTick().
 * `volatile`: an ISR writes it while a task reads it.
 * A 32-bit aligned load/store is a single LDR/STR on Cortex-M3, so this
 * needs no critical section. */
static volatile uint32_t s_last_rx_tick = 0U;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Hand the CAN handle to the driver and preset the TX header.
 * @param  hcan  Handle configured by MX_CAN_Init().
 * @return None.
 */
void BMS_CAN_Init(CAN_HandleTypeDef *hcan)
{
    s_hcan = hcan;

    /* Constant for every frame this slave sends. */
    s_tx_header.IDE                = CAN_ID_STD;
    s_tx_header.RTR                = CAN_RTR_DATA;
    s_tx_header.TransmitGlobalTime = DISABLE;
}

/**
 * @brief  Install the acceptance filter and enable the RX FIFO 0 interrupt.
 * @return None.
 */
void BMS_CAN_InitRx(void)
{
    CAN_FilterTypeDef filter_config;

    if (s_hcan == NULL)
    {
        return;     /* BMS_CAN_Init() was never called */
    }

    filter_config.FilterBank  = CAN_FILTER_BANK_MASTER;
    filter_config.FilterMode  = CAN_FILTERMODE_IDMASK;
    filter_config.FilterScale = CAN_FILTERSCALE_32BIT;

    /* Accept the master frame and nothing else. The mask used to be 0x0000,
     * which accepts EVERY identifier, so frames from the sibling slaves also
     * raised an RX interrupt. That woke the WFI in Task_Sleep, and the slave
     * could never sleep even after the master stopped sending. */
    filter_config.FilterIdHigh     = (uint32_t)CAN_MASTER_ID << CAN_FILTER_ID_SHIFT;
    filter_config.FilterIdLow      = 0x0000U;
    filter_config.FilterMaskIdHigh = (uint32_t)CAN_STD_ID_MASK << CAN_FILTER_ID_SHIFT;
    filter_config.FilterMaskIdLow  = 0x0000U;

    filter_config.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter_config.FilterActivation     = ENABLE;

    HAL_CAN_ConfigFilter(s_hcan, &filter_config);
    HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/**
 * @brief  Queue one standard data frame for transmission.
 * @param  id    11-bit standard identifier.
 * @param  data  Payload, at least @p len bytes.
 * @param  len   Payload length, 0..8.
 * @return BMS_CAN_TX_OK on success, BMS_CAN_TX_BUSY otherwise.
 */
uint8_t BMS_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t len)
{
    if (s_hcan == NULL)
    {
        return BMS_CAN_TX_BUSY;     /* BMS_CAN_Init() was never called */
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) == 0U)
    {
        return BMS_CAN_TX_BUSY;     /* all mailboxes occupied */
    }

    s_tx_header.StdId = id;
    s_tx_header.DLC   = len;

    if (HAL_CAN_AddTxMessage(s_hcan, &s_tx_header, data, &s_tx_mailbox) != HAL_OK)
    {
        return BMS_CAN_TX_BUSY;
    }

    return BMS_CAN_TX_OK;
}

/**
 * @brief  HAL_GetTick() value of the most recent frame from the master.
 * @return Tick of the last accepted master frame, 0 if none arrived yet.
 */
uint32_t BMS_CAN_GetLastRxTick(void)
{
    return s_last_rx_tick;
}

/**
 * @brief  RX FIFO 0 interrupt callback, overrides the HAL weak symbol.
 * @param  hcan  Handle of the peripheral that raised the interrupt.
 * @note   Runs in interrupt context. It writes the pack current straight into
 *         the shared store so a later snapshot pairs it with the voltage
 *         sampled at the same instant.
 * @return None.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t             rx_data[CAN_MAX_PAYLOAD];
    int16_t             raw;

    /* Uses the handle HAL passed in, not s_hcan: the ISR must work off
     * whichever peripheral actually fired.
     * Check the return: on failure rx_header and rx_data are uninitialised
     * stack garbage that could match CAN_MASTER_ID by chance and feed a
     * random current into the filter. */
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
    {
        return;
    }

    if (rx_header.StdId == CAN_MASTER_ID)
    {
        s_last_rx_tick = HAL_GetTick();     /* the master frame is the heartbeat */

        /* Pack current: MSB first, int16 two's complement, A x100 - the same
         * convention as bytes 2-3 of this slave's own frame.
         * The DLC test is both a length check and a compatibility guard: it
         * keeps this build working against a master still sending the old
         * empty heartbeat, in which case the current holds its last value
         * until Task_SOC's staleness timeout zeroes it.
         * Multiplying by 0.01f rather than dividing by 100.0f: a softfloat
         * multiply is about 2.5x cheaper than a divide, which matters in
         * interrupt context. The ~1e-9 relative difference is irrelevant for
         * a current in amperes. */
        if (rx_header.DLC >= CAN_CURRENT_MIN_DLC)
        {
            raw = (int16_t)(((uint16_t)rx_data[0] << 8) | (uint16_t)rx_data[1]);
            BMS_Data_SetCurrent((float)raw * CAN_CURRENT_LSB_A);
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
