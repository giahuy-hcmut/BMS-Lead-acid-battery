Tools
=====

Firmware that is not the product. Each directory is a self-contained PlatformIO
project targeting a spare `esp32doit-devkit-v1`, built and flashed on its own,
and none of it ships on the vehicle. They are kept because two of them are
evidence and the third is a record.

| Directory | What it is | Why it stays |
|---|---|---|
| [`BENCH_INA219/`](BENCH_INA219) | bench rig that characterised the INA219 across the 100 A / 75 mV shunt, 2026-08-12 | every measured current-sensing number in the firmware is cited back to it |
| [`VCAN_RX_TEST/`](VCAN_RX_TEST) | stand-in vehicle node, ESP32 and MCP2551, that receives and decodes frames `0x200` and `0x201` | it is what proved the vehicle bus works, and `include/BMS_VehicleCAN.h` was handed to the motor-controller team as the reference decoder |
| [`Esp-now/`](Esp-now) | firmware for the handheld remote reached over ESP-NOW | legacy, superseded by the web interface, kept as the only record of that link |

`BENCH_INA219/` is the one to be careful with. The offset of −17.5 µV, the 27.2 µV
single-sample noise, the ±427 A range and the gain check all come from that rig,
and the firmware says so in
[`Config.h`](../BMS_Master/include/Config.h) and
[`Task_Current.h`](../BMS_Master/include/Task_Current.h). Delete the directory and
those become numbers with no source, which is the one thing they must never be.
