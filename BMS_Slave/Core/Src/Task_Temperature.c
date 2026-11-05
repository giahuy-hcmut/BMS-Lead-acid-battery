/**
 * @file      Task_Temperature.c
 * @author    Do Dang Gia Huy
 * @brief     Split-phase DS18B20 read: publish, then start the next convert.
 * @date      2026-03-03
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Temperature.h"

#include "Board_Config.h"   /* THRESHOLD_OVER_TEMP */
#include "Debug_Pins.h"
#include "Shared_Data.h"
#include "ds18b20.h"

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

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Read the previous conversion, publish it, and start the next one.
 * @return None.
 */
void Task_Temperature_Run(void)
{
    float   temp_c;
    uint8_t faults = 0U;

    DBG_SET(DBG_TEMP);

    /* Collect the result of the conversion started one period ago. */
    temp_c = DS18B20_Read_Temperature();

    /* On a read error, do NOT publish: the previous value and its fault bit
     * are left untouched, which is the behaviour this task always had. */
    if (temp_c != DS18B20_TEMP_ERROR_C)
    {
        if (temp_c > THRESHOLD_OVER_TEMP)
        {
            faults |= ERROR_OVER_TEMP;
        }

        BMS_Data_SetTemp(temp_c, faults);
    }

    /* Start the next conversion and leave. The sensor needs up to 750 ms,
     * which the CPU spends running the other tasks instead of waiting. */
    DS18B20_Start_Conversion();

    DBG_CLR(DBG_TEMP);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
