/**
 * @file      Task_CAN.c
 * @author    Do Dang Gia Huy
 * @brief     Encoding of the slave status frame and its transmission.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Payload layout of CAN_SLAVE_ID, DLC 8:
 *   byte 0-1  voltage, uint16 big endian, V x100
 *   byte 2-3  current, int16 big endian two's complement, A x100
 *   byte 4    state of charge, 0..100 %
 *   byte 5    temperature, uint8, degrees Celsius + 40
 *   byte 6    fault bitmask, see the ERROR_* bits in Shared_Data.h
 *   byte 7    RESERVED, transmitted as 0x00
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_CAN.h"

#include "BMS_CAN.h"
#include "BSP_Led.h"
#include "Board_Config.h"   /* CAN_SLAVE_ID */
#include "Debug_Pins.h"
#include "Shared_Data.h"

/**********************
 *      DEFINES
 **********************/

/** Fixed payload size of the slave frame; byte 7 is reserved. */
#define CAN_SLAVE_FRAME_DLC     8U

/** Voltage and current are both transmitted in hundredths of their unit. */
#define CAN_SCALE_X100          100.0f

/** Temperature is offset so that -40 C maps to 0 and fits in a uint8_t. */
#define CAN_TEMP_OFFSET_C       40.0f

/** Payload byte indices. */
#define CAN_IDX_VOLT_HI         0U
#define CAN_IDX_VOLT_LO         1U
#define CAN_IDX_CURR_HI         2U
#define CAN_IDX_CURR_LO         3U
#define CAN_IDX_SOC             4U
#define CAN_IDX_TEMP            5U
#define CAN_IDX_FAULTS          6U

#define CAN_BYTE_MASK           0xFFU

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Pack the current snapshot into CAN_SLAVE_ID and transmit it.
 * @return None.
 */
void Task_CAN_Run(void)
{
    uint8_t        tx_data[CAN_SLAVE_FRAME_DLC] = {0};
    BMS_Snapshot_t snap;
    uint16_t       v_send;
    int16_t        i_send;

    DBG_SET(DBG_CAN);

    /* One consistent copy: all five fields come from the same instant, so
     * bytes 0-1 cannot hold the voltage of t1 next to the current of t2. */
    BMS_Data_GetSnapshot(&snap);

    v_send = (uint16_t)(snap.voltage_v * CAN_SCALE_X100);
    tx_data[CAN_IDX_VOLT_HI] = (uint8_t)((v_send >> 8) & CAN_BYTE_MASK);
    tx_data[CAN_IDX_VOLT_LO] = (uint8_t)(v_send & CAN_BYTE_MASK);

    i_send = (int16_t)(snap.current_a * CAN_SCALE_X100);
    tx_data[CAN_IDX_CURR_HI] = (uint8_t)(((uint16_t)i_send >> 8) & CAN_BYTE_MASK);
    tx_data[CAN_IDX_CURR_LO] = (uint8_t)((uint16_t)i_send & CAN_BYTE_MASK);

    tx_data[CAN_IDX_SOC]    = snap.soc_pct;
    tx_data[CAN_IDX_TEMP]   = (uint8_t)(snap.temp_c + CAN_TEMP_OFFSET_C);
    tx_data[CAN_IDX_FAULTS] = snap.faults;
    /* byte 7 stays 0x00 from the initialiser above */

    (void)BMS_CAN_Transmit(CAN_SLAVE_ID, tx_data, CAN_SLAVE_FRAME_DLC);

    /* Blink to show a frame went out. */
    BSP_Led_Toggle();

    DBG_CLR(DBG_CAN);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
