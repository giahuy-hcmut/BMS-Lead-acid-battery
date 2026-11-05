/**
 * @file      BSP_Led.c
 * @author    Do Dang Gia Huy
 * @brief     Board-specific implementation of the status LED driver.
 * @date      2026-08-05
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Everything board-specific about the status LED lives here and nowhere
 * else. See BSP_Led.h for the rules.
 */

/**********************
 *      INCLUDES
 **********************/

#include "BSP_Led.h"

#include "stm32f1xx_hal.h"

/**********************
 *      DEFINES
 **********************/

/* Wiring of the status LED on this board.
 * On the STM32F103C8T6 module the LED sits between 3V3 and PC13, so the pin
 * must be driven LOW to light it -> LED_ON_STATE is GPIO_PIN_RESET. This is
 * exactly what Task_Sleep.c did before the refactor.
 * The GPIO itself is configured by MX_GPIO_Init() (CubeMX), so there is
 * deliberately no BSP_Led_Init() here. */
#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13
#define LED_ON_STATE    GPIO_PIN_RESET
#define LED_OFF_STATE   GPIO_PIN_SET

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
 * @brief  Light the status LED.
 * @return None.
 */
void BSP_Led_On(void)
{
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, LED_ON_STATE);
}

/**
 * @brief  Extinguish the status LED.
 * @return None.
 */
void BSP_Led_Off(void)
{
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, LED_OFF_STATE);
}

/**
 * @brief  Invert the current state of the status LED.
 * @return None.
 */
void BSP_Led_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
