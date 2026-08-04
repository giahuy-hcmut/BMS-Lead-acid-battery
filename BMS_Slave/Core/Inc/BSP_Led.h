/*
 * BSP_Led.h
 *
 *  Created on: Aug 5, 2026
 *      Author: User
 *
 *  Board Support Package - status LED of THIS board.
 *
 *  The interface says WHAT (on / off / toggle), never WHICH PIN. Port, pin
 *  number and the active-low polarity all stay inside BSP_Led.c, so the
 *  application layer needs no HAL type and cannot get the polarity wrong.
 */

#ifndef BSP_LED_H
#define BSP_LED_H

void BSP_Led_On(void);
void BSP_Led_Off(void);
void BSP_Led_Toggle(void);

#endif /* BSP_LED_H */
