/**
 * @file      Shared_Data.h
 * @author    Do Dang Gia Huy
 * @brief     Single owner for the measured state of this slave.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Despite the historical file name this is NOT a public global store. The
 * measured state lives in Shared_Data.c as `static` variables that no other
 * file can name:
 *   - write it with the BMS_Data_Set*() functions, one owner each,
 *   - read it with BMS_Data_GetSnapshot().
 *
 * Why no mutex: the slave runs a COOPERATIVE scheduler (run to completion),
 * so two tasks can never overlap and task-versus-task races cannot exist. A
 * mutex would in fact deadlock here, since there is no context switch to let
 * the holder finish. The only real hazard is task-versus-ISR, which
 * BMS_Data_GetSnapshot() closes with a short PRIMASK section.
 */

#ifndef SHARED_DATA_H_
#define SHARED_DATA_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include <stdint.h>

/**********************
 *      DEFINES
 *********************/

/* Fault bitmask. These bit positions are part of the CAN payload documented
 * in the project report, so the names and values are fixed. */
#define ERROR_NONE          0x00U
#define ERROR_OVER_VOLT     0x01U
#define ERROR_UNDER_VOLT    0x02U
#define ERROR_OVER_TEMP     0x04U

/**********************
 *      TYPEDEFS
 **********************/

/** Consistent copy of the whole measured state.
 * Returned BY VALUE so the caller never holds a pointer into the real store,
 * and so every field comes from the same instant. */
typedef struct
{
    float   voltage_v;      /**< Battery terminal voltage, V            */
    float   current_a;      /**< Pack current, A, positive = discharge  */
    float   temp_c;         /**< Battery temperature, degrees Celsius   */
    uint8_t soc_pct;        /**< State of charge, 0..100 %              */
    uint8_t faults;         /**< OR of the fault bits of every producer */
} BMS_Snapshot_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Publish a voltage sample and its fault bits.
 * @param  voltage_v   Terminal voltage in volts.
 * @param  fault_bits  OR of the ERROR_* bits raised by the voltage checks.
 * @note   Sole caller: Task_Voltage. Also raises the freshness flag read by
 *         BMS_Data_TakeVoltageFresh().
 * @return None.
 */
void BMS_Data_SetVoltage(float voltage_v, uint8_t fault_bits);

/**
 * @brief  Publish a temperature sample and its fault bits.
 * @param  temp_c      Temperature in degrees Celsius.
 * @param  fault_bits  OR of the ERROR_* bits raised by the temperature check.
 * @note   Sole caller: Task_Temperature.
 * @return None.
 */
void BMS_Data_SetTemp(float temp_c, uint8_t fault_bits);

/**
 * @brief  Publish the pack current received from the master.
 * @param  current_a  Current in amperes, positive while discharging.
 * @note   Sole caller: the CAN RX ISR in BMS_CAN.c. The caller does the
 *         scaling; the store performs no arithmetic.
 * @return None.
 */
void BMS_Data_SetCurrent(float current_a);

/**
 * @brief  Publish the computed state of charge.
 * @param  soc_pct  State of charge, 0..100 %.
 * @note   Sole caller: Task_SOC.
 * @return None.
 */
void BMS_Data_SetSoc(uint8_t soc_pct);

/**
 * @brief  Copy the whole state out in one consistent read.
 * @param  p_out  Destination; ignored when NULL.
 * @return None.
 */
void BMS_Data_GetSnapshot(BMS_Snapshot_t *p_out);

/**
 * @brief  Read and clear the "new voltage sample" flag.
 * @note   The Kalman filter may only run its Update step on a FRESH voltage
 *         sample; correcting with a stale one was a real design defect, see
 *         section 2 of the project brief. Voltage arrives every 50 ms while
 *         Predict runs every 5 ms, so one Update follows ten Predicts.
 *         Sole caller: Task_SOC.
 * @return 1 when a new sample arrived since the previous call, 0 otherwise.
 */
uint8_t BMS_Data_TakeVoltageFresh(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SHARED_DATA_H_*/
