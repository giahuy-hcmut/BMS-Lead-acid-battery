/**
 * @file      Scheduler.c
 * @author    Do Dang Gia Huy
 * @brief     Delta-list bookkeeping for the cooperative scheduler.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Scheduler.h"

#include <stddef.h>         /* NULL */

/**********************
 *      DEFINES
 **********************/

/** Return values of SCH_Delete_Task(). */
#define SCH_DELETE_OK       1U
#define SCH_DELETE_FAIL     0U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* `volatile`: SCH_Update() runs in the SysTick ISR and writes delay/run_me
 * while SCH_Dispatch() reads them from the main loop. Without volatile the
 * compiler may keep run_me in a register across the check, and Dispatch then
 * never sees the ISR's update - the scheduler simply stops. At -O0 this is
 * masked; a release build (-Os) is free to break it.
 * `static`: nothing outside this file touches the table. */
static volatile st_task_t s_sch_tasks[SCH_MAX_TASKS];

/** Index of the first entry of the delta list. */
static volatile uint8_t s_head_index = NO_TASK_ID;

/* Number of times a task was already pending again when it got dispatched,
 * i.e. a deadline was missed. The surplus run_me cannot be recovered (the
 * re-armed instance starts at run_me = 0), so at least count the event.
 * Watch SCH_GetOverrunCount() in Live Expressions: it must stay 0. */
static volatile uint16_t s_overrun_count = 0U;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Empty the task table and the ready list.
 * @return None.
 */
void SCH_Init(void)
{
    uint8_t i;

    for (i = 0U; i < SCH_MAX_TASKS; i++)
    {
        (void)SCH_Delete_Task(i);
    }

    s_head_index = NO_TASK_ID;
}

/**
 * @brief  Advance the delta list by one tick.
 * @note   Runs in the SysTick interrupt, so it stays short.
 * @return None.
 */
void SCH_Update(void)
{
    uint8_t idx;

    if (s_head_index == NO_TASK_ID)
    {
        return;
    }

    /* Only the head is decremented: every other delay is relative to it. */
    if (s_sch_tasks[s_head_index].delay > 0U)
    {
        s_sch_tasks[s_head_index].delay--;
    }

    /* Head reached zero -> release the WHOLE group sharing that deadline.
     * In a delta list a following entry with delay == 0 falls due at the same
     * instant as its predecessor, so walk the chain and raise run_me for all
     * of them. Releasing only one task per tick was what made the lower
     * priority tasks run about 11% late under load. Stop at the first entry
     * with delay > 0.
     *
     * A one-shot task (delay == 0, period == 0) simply stays at the head
     * until SCH_Dispatch() picks it up. */
    if (s_sch_tasks[s_head_index].delay == 0U)
    {
        idx = s_head_index;
        while ((idx != NO_TASK_ID) && (s_sch_tasks[idx].delay == 0U))
        {
            s_sch_tasks[idx].run_me += 1U;
            idx = s_sch_tasks[idx].next_index;
        }
    }
}

/**
 * @brief  Insert a task into the delta list.
 * @param  p_function  Task entry point.
 * @param  delay_ms    Delay before the first release, in milliseconds.
 * @param  period_ms   Repeat period in milliseconds, 0 for a one-shot task.
 * @param  priority    Tie-break on an EQUAL deadline, higher runs first.
 * @return Slot index, or NO_TASK_ID on refusal.
 */
uint8_t SCH_Add_Task(void (*p_function)(void), uint32_t delay_ms, uint32_t period_ms, uint8_t priority)
{
    uint8_t  new_index = 0U;
    uint8_t  current;
    uint8_t  prev;
    uint32_t delay_ticks;
    uint32_t primask;

    /* Both arguments are converted by integer division below, so anything
     * that is not a whole number of ticks gets silently truncated:
     * period_ms = 5 with SCH_TICK_MS = 10 yields period = 0, which Dispatch
     * treats as a one-shot and deletes. Refuse it loudly instead. */
    if (((period_ms % SCH_TICK_MS) != 0U) || ((delay_ms % SCH_TICK_MS) != 0U))
    {
        return NO_TASK_ID;
    }

    /* 1. Find a free slot.
     * Bound check FIRST: the old order dereferenced s_sch_tasks[new_index]
     * before testing new_index < SCH_MAX_TASKS, so a full table read one
     * element past the end of the array. */
    while ((new_index < SCH_MAX_TASKS) && (s_sch_tasks[new_index].p_task != NULL))
    {
        new_index++;
    }

    if (new_index == SCH_MAX_TASKS)
    {
        return NO_TASK_ID;      /* table full */
    }

    /* 2. Convert to ticks. */
    delay_ticks = delay_ms / SCH_TICK_MS;

    s_sch_tasks[new_index].p_task   = p_function;
    s_sch_tasks[new_index].period   = period_ms / SCH_TICK_MS;
    s_sch_tasks[new_index].run_me   = 0U;
    s_sch_tasks[new_index].priority = priority;

    /* 3. Insertion, CRITICAL SECTION.
     * Interrupts must be masked so SCH_Update() cannot decrement a delay
     * halfway through the walk and leave the chain inconsistent.
     * Save/restore PRIMASK instead of __enable_irq(): this function is called
     * from SCH_Dispatch(), so re-enabling unconditionally would break any
     * critical section the caller might be holding. */
    primask = __get_PRIMASK();
    __disable_irq();

    if (s_head_index == NO_TASK_ID)
    {
        /* Case 1: the list is empty. */
        s_head_index                      = new_index;
        s_sch_tasks[new_index].delay      = delay_ticks;
        s_sch_tasks[new_index].next_index = NO_TASK_ID;
    }
    else if ((delay_ticks < s_sch_tasks[s_head_index].delay) ||
             ((delay_ticks == s_sch_tasks[s_head_index].delay) &&
              (priority > s_sch_tasks[s_head_index].priority)))
    {
        /* Case 2: insert at the HEAD - either due earlier than the current
         * head, or due at the same instant but with a higher priority. */
        s_sch_tasks[s_head_index].delay -= delay_ticks;     /* keep the delta */

        s_sch_tasks[new_index].next_index = s_head_index;
        s_sch_tasks[new_index].delay      = delay_ticks;
        s_head_index                      = new_index;
    }
    else
    {
        /* Case 3: insert in the MIDDLE or at the TAIL.
         * The head comparison above already failed, so the first loop pass
         * always advances and prev can never stay NO_TASK_ID. */
        current = s_head_index;
        prev    = NO_TASK_ID;

        while (current != NO_TASK_ID)
        {
            if (delay_ticks < s_sch_tasks[current].delay)
            {
                break;      /* due before 'current' */
            }

            if ((delay_ticks == s_sch_tasks[current].delay) &&
                (priority > s_sch_tasks[current].priority))
            {
                break;      /* same instant, higher priority -> go first */
            }

            delay_ticks -= s_sch_tasks[current].delay;
            prev    = current;
            current = s_sch_tasks[current].next_index;
        }

        /* Link in after 'prev'. */
        s_sch_tasks[new_index].delay      = delay_ticks;
        s_sch_tasks[new_index].next_index = current;
        s_sch_tasks[prev].next_index      = new_index;

        /* Unless this is the new tail, the successor's delta shrinks by the
         * amount now consumed by the new entry. */
        if (current != NO_TASK_ID)
        {
            s_sch_tasks[current].delay -= delay_ticks;
        }
    }

    __set_PRIMASK(primask);

    return new_index;
}

/**
 * @brief  Release a task slot.
 * @param  task_index  Slot to release.
 * @warning Does NOT unlink the slot from the delta list; see Scheduler.h.
 * @return SCH_DELETE_OK when the slot was in use, SCH_DELETE_FAIL otherwise.
 */
uint8_t SCH_Delete_Task(uint8_t task_index)
{
    uint32_t primask;

    if ((task_index >= SCH_MAX_TASKS) || (s_sch_tasks[task_index].p_task == NULL))
    {
        return SCH_DELETE_FAIL;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    s_sch_tasks[task_index].p_task     = NULL;
    s_sch_tasks[task_index].delay      = 0U;
    s_sch_tasks[task_index].period     = 0U;
    s_sch_tasks[task_index].run_me     = 0U;
    s_sch_tasks[task_index].priority   = 0U;
    s_sch_tasks[task_index].next_index = NO_TASK_ID;    /* leave no stale link */

    __set_PRIMASK(primask);

    return SCH_DELETE_OK;
}

/**
 * @brief  Run at most one ready task, then re-arm it.
 * @return None.
 */
void SCH_Dispatch(void)
{
    void (*p_run_task)(void);
    uint32_t period;
    uint32_t primask;
    uint8_t  priority;
    uint8_t  current_id;

    /* Only the head needs checking: the list is ordered, so whatever is due
     * is at the front. */
    if (s_head_index == NO_TASK_ID)
    {
        return;
    }

    if (s_sch_tasks[s_head_index].run_me == 0U)
    {
        return;
    }

    /* 1. Copy out what is needed before the slot can be reused. */
    p_run_task = s_sch_tasks[s_head_index].p_task;
    period     = s_sch_tasks[s_head_index].period;
    priority   = s_sch_tasks[s_head_index].priority;
    current_id = s_head_index;

    /* 2. Decrement run_me AND unlink from the head in ONE critical section.
     *    `run_me -= 1` is a read-modify-write (LDRB / SUB / STRB); if
     *    SCH_Update() lands between those instructions its own `run_me += 1`
     *    is overwritten and a deadline vanishes. */
    primask = __get_PRIMASK();
    __disable_irq();

    if (s_sch_tasks[current_id].run_me > 1U)
    {
        s_overrun_count++;      /* deadline missed - the surplus is lost */
    }

    s_sch_tasks[current_id].run_me -= 1U;

    /* The successor becomes the head; its delta is already correct. */
    s_head_index = s_sch_tasks[current_id].next_index;

    __set_PRIMASK(primask);

    /* 3. Re-arm BEFORE running the body.
     *    SCH_Add_Task() used to run after p_run_task(), so the period was
     *    measured from when the task FINISHED. A task overrunning by one tick
     *    slipped, and the slip became the new baseline, so the error
     *    accumulated in SCH_TICK_MS steps.
     *    Delete first so Add_Task may reuse this very slot; p_run_task was
     *    already copied out above, so clearing p_task is safe. */
    (void)SCH_Delete_Task(current_id);

    if (period > 0U)
    {
        (void)SCH_Add_Task(p_run_task, period * SCH_TICK_MS, period * SCH_TICK_MS, priority);
    }

    /* 4. Run the task. */
    if (p_run_task != NULL)
    {
        p_run_task();
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
