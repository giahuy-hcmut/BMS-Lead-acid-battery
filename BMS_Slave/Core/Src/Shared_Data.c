/**
 * @file      Shared_Data.c
 * @author    Do Dang Gia Huy
 * @brief     File-local measured state with one writer per field.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Shared_Data.h"

#include "stm32f1xx_hal.h"  /* CMSIS __get_PRIMASK / __disable_irq / __set_PRIMASK */

#include <stddef.h>         /* NULL */

/**********************
 *      DEFINES
 **********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* The store itself.
 * `static`   -> internal linkage, no other file can even name these. An
 *               `extern` declaration elsewhere fails at link time.
 * `volatile` -> the CAN RX ISR writes s_current_a, so the compiler must not
 *               cache any of these reads in a register. */
static volatile float   s_voltage_v = 0.0f;
static volatile float   s_current_a = 0.0f;
static volatile float   s_temp_c    = 0.0f;
static volatile uint8_t s_soc_pct   = 0U;

/* Fault bits are kept PER PRODUCER and only OR-ed together on read.
 * A single shared byte would need a read-modify-write (LDRB / ORR / STRB),
 * and an ISR landing between those three instructions would drop a bit the
 * other producer had just set. */
static volatile uint8_t s_faults_volt = 0U;
static volatile uint8_t s_faults_temp = 0U;

/* "A voltage sample nobody has consumed yet". Raised by BMS_Data_SetVoltage(),
 * cleared by BMS_Data_TakeVoltageFresh(). It lets Task_SOC, running every
 * 5 ms, tell when the 50 ms voltage sample is fresh, so the Kalman filter
 * never corrects twice with the same measurement. */
static volatile uint8_t s_voltage_fresh = 0U;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Publish a voltage sample and its fault bits.
 * @param  voltage_v   Terminal voltage in volts.
 * @param  fault_bits  OR of the ERROR_* bits raised by the voltage checks.
 * @return None.
 */
void BMS_Data_SetVoltage(float voltage_v, uint8_t fault_bits)
{
    s_voltage_v     = voltage_v;
    s_faults_volt   = fault_bits;
    s_voltage_fresh = 1U;       /* a new sample is available */
}

/**
 * @brief  Publish a temperature sample and its fault bits.
 * @param  temp_c      Temperature in degrees Celsius.
 * @param  fault_bits  OR of the ERROR_* bits raised by the temperature check.
 * @return None.
 */
void BMS_Data_SetTemp(float temp_c, uint8_t fault_bits)
{
    s_temp_c      = temp_c;
    s_faults_temp = fault_bits;
}

/**
 * @brief  Publish the pack current received from the master.
 * @param  current_a  Current in amperes, positive while discharging.
 * @note   Called from the CAN RX ISR. A single aligned 32-bit store (STR), so
 *         it is atomic against a task read and needs no critical section. The
 *         store does no arithmetic: the caller hands over amperes already.
 * @return None.
 */
void BMS_Data_SetCurrent(float current_a)
{
    s_current_a = current_a;
}

/**
 * @brief  Publish the computed state of charge.
 * @param  soc_pct  State of charge, 0..100 %.
 * @return None.
 */
void BMS_Data_SetSoc(uint8_t soc_pct)
{
    s_soc_pct = soc_pct;
}

/**
 * @brief  Copy the whole state out in one consistent read.
 * @param  p_out  Destination; ignored when NULL.
 * @return None.
 */
void BMS_Data_GetSnapshot(BMS_Snapshot_t *p_out)
{
    uint32_t primask;

    if (p_out == NULL)
    {
        return;
    }

    /* Save and restore PRIMASK instead of calling __enable_irq()
     * unconditionally: if this ever runs with interrupts already masked,
     * restoring is correct while re-enabling would silently break the
     * caller's critical section. */
    primask = __get_PRIMASK();
    __disable_irq();

    p_out->voltage_v = s_voltage_v;
    p_out->current_a = s_current_a;
    p_out->temp_c    = s_temp_c;
    p_out->soc_pct   = s_soc_pct;
    p_out->faults    = (uint8_t)(s_faults_volt | s_faults_temp);

    __set_PRIMASK(primask);
}

/**
 * @brief  Read and clear the "new voltage sample" flag.
 * @note   Read-and-clear. The producer (Task_Voltage) and the consumer
 *         (Task_SOC) are both tasks, and the cooperative scheduler runs each
 *         to completion, so they cannot interleave and no critical section is
 *         needed. This would have to change if an ISR ever set the flag.
 * @return 1 when a new sample arrived since the previous call, 0 otherwise.
 */
uint8_t BMS_Data_TakeVoltageFresh(void)
{
    uint8_t fresh = s_voltage_fresh;

    s_voltage_fresh = 0U;

    return fresh;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
