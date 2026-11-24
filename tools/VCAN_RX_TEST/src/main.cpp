/*
 * VCAN_RX_TEST - stand-in "vehicle" node that receives the BMS frames.
 *
 *  Purpose: prove the BMS master actually transmits 0x200 / 0x201 on the
 *  vehicle CAN bus, and provide the ACK the master's MCP2515 needs (so it
 *  stops printing "[VCAN] TX tac"). This node also demonstrates how to use the
 *  hand-off decoder BMS_VehicleCAN.h.
 *
 *  Hardware: this ESP32 + an MCP2551 transceiver.
 *      GPIO5 (TX) -> MCP2551 TXD (pin 1)
 *      GPIO4 (RX) <- MCP2551 RXD (pin 4)
 *      MCP2551: VCC=5V, GND=GND, Rs(pin8)->GND (high-speed mode)
 *      MCP2551 CANH/CANL -> vehicle bus (master's MCP2515 CANH/CANL)
 *      120 ohm across CANH-CANL at each end of the bus (2 total)
 *      COMMON GROUND with the master.
 */
#include <Arduino.h>
#include "driver/twai.h"
#include "BMS_VehicleCAN.h"

#define TX_PIN GPIO_NUM_5    /* -> MCP2551 TXD */
#define RX_PIN GPIO_NUM_4    /* <- MCP2551 RXD */

static void printFlags(uint8_t f) {
    if (f == 0) { Serial.print("[none]"); return; }
    if (f & BMS_FLAG_OVER_VOLT)    Serial.print("OV ");
    if (f & BMS_FLAG_UNDER_VOLT)   Serial.print("UV ");
    if (f & BMS_FLAG_OVER_TEMP)    Serial.print("OT ");
    if (f & BMS_FLAG_OVER_CURRENT) Serial.print("OC ");
    if (f & BMS_FLAG_SLAVE_LOST)   Serial.print("SLAVE_LOST ");
    if (f & BMS_FLAG_RELAY_OPEN)   Serial.print("RELAY_OPEN ");
    if (f & BMS_FLAG_MANUAL_OFF)   Serial.print("MANUAL_OFF ");
    if (f & BMS_FLAG_BMS_INTERNAL) Serial.print("BMS_INTERNAL ");
}

void setup() {
    Serial.begin(115200);
    delay(300);

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g, &t, &f) != ESP_OK) {
        Serial.println("[RX] TWAI install FAIL");
        while (1) { delay(1000); }
    }
    if (twai_start() != ESP_OK) {
        Serial.println("[RX] TWAI start FAIL");
        while (1) { delay(1000); }
    }
    Serial.println("[RX] TWAI ready @500 kbps - waiting for BMS frames...");
}

void loop() {
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(2000)) != ESP_OK) {
        Serial.println("[RX] ... no frame (check CANH/CANL, 120 ohm x2, common GND)");
        return;
    }

    /* raw dump - always useful */
    Serial.printf("ID 0x%03X DLC %u:", (unsigned)msg.identifier, msg.data_length_code);
    for (int k = 0; k < msg.data_length_code; k++) Serial.printf(" %02X", msg.data[k]);
    Serial.println();

    /* decode via the hand-off header - exactly how the teammate uses it */
    BmsStatus st;
    if (BmsVcan_DecodeStatus(msg.identifier, msg.data, msg.data_length_code, &st)) {
        Serial.printf("  STATUS  Vtong=%.2fV  I=%s%.2fA  SOC=%s  state=%s  cnt=%u  flags=",
            st.total_voltage_v,
            st.current_valid ? (st.current_a >= 0 ? "+" : "") : "",
            st.current_valid ? st.current_a : 0.0f,
            (st.sys_soc < 0) ? "??" : String(st.sys_soc).c_str(),
            BmsVcan_StateName(st.state),
            st.alive_counter);
        printFlags(st.flags);
        if (!st.current_valid) Serial.print("  (CURRENT SENSOR LOST)");
        Serial.println();
    }

    BmsPack pk;
    if (BmsVcan_DecodePack(msg.identifier, msg.data, msg.data_length_code, &pk)) {
        Serial.printf("  PACK[%u] V=%.2fV  SOC=%s  T=%s  online=%u  (n=%u)  flags=",
            pk.index, pk.voltage_v,
            (pk.soc  < 0)     ? "??" : String(pk.soc).c_str(),
            (pk.temp_c < -200) ? "??" : String(pk.temp_c).c_str(),
            pk.online, pk.pack_count);
        printFlags(pk.flags);
        Serial.println();
    }
}
