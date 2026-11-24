Hardware
========

Altium Designer source, fabrication data and schematic PDFs for the two printed
circuit boards of this project. The firmware for each board sits in the
repository root directory of the same name, so `hardware/BMS_Master` and
`BMS_Master` are the board and its code.

| Board | Role | Copper layers | Holes | Fab data exported |
|---|---|---|---|---|
| [`BMS_Master/`](BMS_Master) | central controller — ESP32, INA219, MCP2515, contactor drive | 2 | 97 | 2026-09-03 |
| [`BMS_Slave/`](BMS_Slave) | battery monitor, five identical boards — STM32F103, ADuM1201 | 2 | 90 | 2026-08-15 |

Part choices and the reasoning behind them are in the main README,
[§3.1](../README.md#31-battery-monitor-5) for the monitor and
[§3.2](../README.md#32-central-controller) for the controller. This directory
carries the design data only, not a second copy of that argument.

### Directory convention

Each board holds the same three directories, and the split between the first two
is the whole point of the layout:

```
altium/              the editable design - the source of truth
fabrication/rev-a/   frozen copy of exactly what went to the fab house
doc/                 schematic as PDF, readable without Altium
```

`altium/` moves. `fabrication/rev-<letter>/` does not: once a board has been
manufactured from a set of Gerbers, that set is the record of a physical object,
and it is never edited in place. A later layout change gets a `rev-b` directory,
not a rewrite of `rev-a`. Without that rule there is no way to answer which
Gerbers produced the board in a given photograph, which is the question that
actually gets asked when a board misbehaves.

Inside a revision:

```
gerber/   RS-274X layer files, plus the aperture list
drill/    ASCII Excellon, round holes and slots separately, and the drill report
report/   design rule check, Gerber generation report
```

A bill of materials is missing from both boards and has to be exported from
Altium (Reports, Bill of Materials) into `fabrication/rev-a/bom/bom.csv`. Until
then neither fabrication package is complete enough for someone else to build
the board.

### Tooling

Altium Designer 24.1.2, recorded in the `TF.GenerationSoftware` attribute of
every Gerber file. Gerbers are RS-274X, absolute coordinates, 4.4 format,
millimetres (`%FSLAX44Y44%`, `%MOMM%`). Drill files are ASCII Excellon.

### Libraries are deliberately absent

Both projects take their symbols and footprints from Altium's own installed
library and from the CERN Altium Library 2013. Neither belongs in this
repository: the first is licensed Altium content and this repository is public,
and the two together are several hundred megabytes.

Cloning therefore does not give a project that opens with every footprint
resolved - Altium will report missing components. That is expected, and it costs
little, because the schematic PDFs and the Gerbers are complete and
self-contained: the design can be read, reviewed and manufactured without ever
opening Altium.

### What is not here

The design history is not: Altium's `History/` directories hold about 300 MB of
automatic save archives across these two projects, and Git already does that job.
The same goes for `Project Logs for *` (engineering change order logs, rewritten
whenever the project is opened), `__Previews/` (thumbnail cache) and
`*.PrjPcbStructure` (local user interface state).

The first revision of the design - a single board carrying both functions, built
between June and July 2026 and superseded by the split into a controller and five
monitors - is not here either.
