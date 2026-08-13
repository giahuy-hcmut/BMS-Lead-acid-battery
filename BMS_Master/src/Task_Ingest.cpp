#include "Task_Ingest.h"
#include "System_Data.h"   // canQueue + System_Update_Pack + BMS_Message_t

/* Event-driven: blocks on the CAN queue and copies each frame into the store.
 * Pure data plumbing - holds no state, makes no decisions, and sleeps at 0%
 * CPU when no frame is waiting. Split out of Task_Logic so the safety monitor
 * runs on its own fixed cadence instead of being paced by frame arrival. */
void Task_Ingest_Run(void *pvParameters) {
    BMS_Message_t msg;

    for (;;) {
        if (xQueueReceive(canQueue, &msg, portMAX_DELAY) == pdTRUE) {
            System_Update_Pack(msg.can_id, msg.voltage, msg.temperature,
                               msg.status, msg.soc);
        }
    }
}
