/**
 * @file      BSP_Led.h
 * @author    Do Dang Gia Huy
 * @brief     Board support package for the status LED of this board.
 * @date      2026-08-05
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * The interface says WHAT (on / off / toggle), never WHICH PIN. Port, pin
 * number and the active-low polarity all stay inside BSP_Led.c, so the
 * application layer needs no HAL type and cannot get the polarity wrong.
 */

#ifndef BSP_LED_H_
#define BSP_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Light the status LED.
 * @note   The GPIO is configured by MX_GPIO_Init(), so no init call is needed.
 * @return None.
 */
void BSP_Led_On(void);

/**
 * @brief  Extinguish the status LED.
 * @return None.
 */
void BSP_Led_Off(void);

/**
 * @brief  Invert the current state of the status LED.
 * @return None.
 */
void BSP_Led_Toggle(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BSP_LED_H_*/
