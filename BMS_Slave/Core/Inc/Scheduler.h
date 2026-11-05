/**
 * @file      Scheduler.h
 * @author    Do Dang Gia Huy
 * @brief     Cooperative delta-list scheduler driven by the SysTick tick.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Tasks run to completion, one after another, out of the main loop. The ready
 * list is a delta list: each entry stores the delay RELATIVE to its
 * predecessor, so the tick handler only has to decrement the head, which
 * keeps it O(1). The cost is moved to insertion, which is O(n) but happens
 * outside interrupt context.
 *
 * Public names keep the SCH_ prefix and the Add_Task / Delete_Task spelling
 * used in the project report, instead of the module_object_action form of the
 * coding standard, so that code and report stay readable against each other.
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include "main.h"           /* HAL, stdint and the CMSIS PRIMASK intrinsics */

/**********************
 *      DEFINES
 *********************/

/** Size of the task table; SCH_Add_Task() fails once all slots are taken. */
#define SCH_MAX_TASKS   10U

/** Returned by SCH_Add_Task() on failure, and the end-of-list terminator. */
#define NO_TASK_ID      0xFFU

/** Period of the SCH_Update() call, in milliseconds.
 * Lowered from 10 to 5 so Task_SOC can run at 5 ms, matching the rate at
 * which the master publishes the pack current to the Kalman predict step.
 * Every DELAY_MS and PERIOD_MS passed to SCH_Add_Task() must be a whole
 * multiple of this value or the task is rejected with NO_TASK_ID. */
#define SCH_TICK_MS     5U

/**********************
 *      TYPEDEFS
 **********************/

/** One entry of the delta list. */
typedef struct
{
    void (*p_task)(void);       /**< Task entry point                        */
    uint32_t delay;             /**< Ticks still to wait, RELATIVE to the
                                 *   preceding entry in the list             */
    uint32_t period;            /**< Repeat period in ticks, 0 for one-shot  */
    uint8_t  run_me;            /**< Pending activations, raised by the tick */
    uint8_t  priority;          /**< Tie-break on an EQUAL deadline; a higher
                                 *   number runs first                       */
    uint8_t  next_index;        /**< Index of the next entry, or NO_TASK_ID  */
} st_task_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Empty the task table and the ready list.
 * @note   Call once before adding any task.
 * @return None.
 */
void SCH_Init(void);

/**
 * @brief  Advance the delta list by one tick.
 * @note   Call from the SysTick interrupt. O(1) in the common case: only the
 *         head is decremented. When the head reaches zero the whole group of
 *         entries sharing that deadline is marked ready in one pass.
 * @return None.
 */
void SCH_Update(void);

/**
 * @brief  Run at most one ready task, then re-arm it.
 * @note   Call from the main loop. The task is re-armed BEFORE its body runs,
 *         so the period is measured from the release instant and an overrun
 *         does not shift the following deadlines.
 * @return None.
 */
void SCH_Dispatch(void);

/**
 * @brief  Insert a task into the delta list.
 * @param  p_function  Task entry point.
 * @param  delay_ms    Delay before the first release, in milliseconds.
 * @param  period_ms   Repeat period in milliseconds, 0 for a one-shot task.
 * @param  priority    Tie-break on an EQUAL deadline; the higher number runs
 *                     first. No effect between different deadlines.
 * @note   Both @p delay_ms and @p period_ms must be whole multiples of
 *         SCH_TICK_MS; otherwise integer division would silently truncate
 *         them, so the task is refused instead.
 * @return Index of the slot taken, or NO_TASK_ID when the arguments are not
 *         tick multiples or the table is full.
 */
uint8_t SCH_Add_Task(void (*p_function)(void), uint32_t delay_ms, uint32_t period_ms, uint8_t priority);

/**
 * @brief  Release a task slot.
 * @param  task_index  Slot to release.
 * @warning Does NOT unlink the slot from the delta list: the preceding entry
 *          keeps pointing at it. Only call it on an entry already popped off
 *          the head, which is what SCH_Dispatch() does. Calling it on an
 *          entry in the middle of the list corrupts the chain.
 * @return 1 when the slot was in use and is now free, 0 otherwise.
 */
uint8_t SCH_Delete_Task(uint8_t task_index);

/**
 * @brief  Number of missed deadlines observed so far.
 * @note   Counts the times a task was already pending again when it got
 *         dispatched. Watch it in Live Expressions: a healthy schedule keeps
 *         it at 0.
 * @return Missed-deadline count since reset.
 */
uint16_t SCH_GetOverrunCount(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SCHEDULER_H_*/
