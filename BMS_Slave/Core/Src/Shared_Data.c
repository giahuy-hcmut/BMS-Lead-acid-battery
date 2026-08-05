/*
 * Shared_Data.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#include "Shared_Data.h"
#include "stm32f1xx_hal.h"   // CMSIS: __get_PRIMASK / __disable_irq / __set_PRIMASK
#include <stddef.h>          // NULL

// ==========================================
// KHO DU LIEU - biến riêng của file này
// ==========================================
// static   -> internal linkage -> khong file nao khac goi ten duoc.
//             Ai viet `extern` o file khac se LOI LUC LINK.
// volatile -> ISR CAN ghi vao s_current_a, va cac lenh doc ben duoi
//             khong duoc phep bi compiler cache vao register.
static volatile float   s_voltage_v = 0.0f;
static volatile float   s_current_a = 0.0f;
static volatile float   s_temp_c    = 0.0f;
static volatile uint8_t s_soc_pct   = 0;

// Co loi giu RIENG theo tung chu, chi OR lai luc doc.
// Dung MOT byte chung thi phai read-modify-write (LDRB / ORR / STRB);
// mot ISR chen vao giua 3 lenh do se lam mat bit ma chu kia vua set.
static volatile uint8_t s_faults_volt = 0;
static volatile uint8_t s_faults_temp = 0;

void BMS_Data_SetVoltage(float voltage_v, uint8_t fault_bits)
{
    s_voltage_v   = voltage_v;
    s_faults_volt = fault_bits;
}

void BMS_Data_SetTemp(float temp_c, uint8_t fault_bits)
{
    s_temp_c      = temp_c;
    s_faults_temp = fault_bits;
}

void BMS_Data_SetCurrent(float current_a)
{
    /* Called from the CAN RX ISR. A single aligned 32-bit store (STR), so it
     * is atomic against a task read and needs no critical section. The store
     * itself does no arithmetic - the caller hands over amperes already. */
    s_current_a = current_a;
}

void BMS_Data_GetSnapshot(BMS_Snapshot_t *out)
{
    uint32_t primask;

    if (out == NULL)
    {
        return;
    }

    // Save and restore PRIMASK instead of calling __enable_irq()
    // unconditionally: if this ever runs with interrupts already masked,
    // restoring is correct while re-enabling would silently break the
    // caller's critical section.
    primask = __get_PRIMASK();
    __disable_irq();

    out->voltage_v = s_voltage_v;
    out->current_a = s_current_a;
    out->temp_c    = s_temp_c;
    out->soc_pct   = s_soc_pct;
    out->faults    = (uint8_t)(s_faults_volt | s_faults_temp);

    __set_PRIMASK(primask);
}
