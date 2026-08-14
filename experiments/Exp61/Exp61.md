# Exp61: Port the Exp60 Power curve (PowerCurve library) to the ATtiny85 I2C slave

## Context

Exp60 baked a RawAccel-style **Power** velocity curve into the Pro Mini's I2C
slave (custom flat-top cap fix), with curve params written by the ZMK driver at
init + every link-restore. The ATtiny85 runs the same PMW3610 emulator protocol
at `0x42` (`trackpoint-i2c-slave-attiny85.ino`) and is a drop-in replacement for
the Pro Mini — so the same curve, driver, and overlay should work with zero
changes on the ZMK side.

## Hypothesis

Copying `PowerCurve` into `attiny85-trackpoint/libraries/` and wiring it into the
ATtiny85 slave (`receiveEvent` dispatch for 0x11/0x13/0x15/0x17, `curve.begin()`
in setup, `curve.apply()` in loop) gives the ATtiny85 the exact same curve feel
as the Exp60 Pro Mini. The existing ZMK driver + `dabase_v2_right.overlay`
params (`speed-scale 128`, `curve-rate 18`, `curve-exponent 256`,
`curve-start 77`) are already re-applied on the first successful poll and on
every link-restore, so the ATtiny85 picks them up automatically.

## Implementation

- Copied `promini-trackpoint/libraries/PowerCurve/` → `attiny85-trackpoint/libraries/PowerCurve/`
  (already on `lib_extra_dirs = ../libraries`; code is AVR-generic — 32-bit
  `double` on tinyX5 like the 328P).
- `.ino` changes in `trackpoint-i2c-slave-attiny85.ino`:
  - `#include <PowerCurve.h>` + `PowerCurve curve;`
  - `receiveEvent`: read `b1`/`b2` (Wire buffers all received bytes) and dispatch
    `0x11 → setSens(b1)`, `0x13/0x15/0x17 → setParam(reg, b1|(b2<<8))` — mirrors
    the Pro Mini's handler.
  - `setup`: `curve.begin();`
  - `loop`: `curve.apply()` into locals (burst globals are `volatile` for the
    ISR), then `curve.update()` to rebuild the LUT when a param landed.
- Kept the Exp43 destructive-read burst (serves once then zeroes — better than
  the Pro Mini's non-destructive burst), the debug register 0x03, and
  `SLEEP_ENABLED 0`. No sleep/power-gating on the ATtiny85 (deliberate — it's
  the standalone 4-wire config).

## Build

```
pio run   # in attiny85-trackpoint/attiny85-trackpoint-i2c-slave/
```

- RAM: 229/512 B (44.7%) — was 89 B; PowerCurve LUT is 128 B + members.
- Flash: 5576/8192 B (68.1%) — was 2440 B.
- Both well within the tinyX5 budget.

## Flash (Leonardo ISP, per attiny85-trackpoint/README.md)

- Leonardo in programmer role (`arduino-isp`), `usbipd attach --wsl --busid 3-1`.
- Probe: `avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200` → sig `0x1e930b`.
- Write + verify: 5576 bytes flash written + verified.

## Known-good

- attiny85-trackpoint `01d8d24` (Exp59 branch, pushed)

## Status

- [x] Port + build + flash
- [ ] Bench verify: swap the ATtiny85 onto the dabase_v2 right half I2C bus in
  place of the Pro Mini (SDA/SCL/GND, 4.7k pull-ups). Expect `i2c scan` to show
  `0x42`, cursor moving, and the curve feel to match Exp60 exactly — params land
  automatically on first successful poll + every link-restore.

## Notes / deferred

- Power-gating (Exp48-50 NPN/PNP gates) and ZMK deep-sleep auto-cut do **not**
  apply to the ATtiny85 in this config — it stays powered like the Exp47 4-wire
  setup. The user explicitly said to ignore power-gating for this experiment.
- No ZMK driver or config changes were needed; `trackpoint-i2c.c` params handling
  (Exp60 `767a5dd`) works unchanged against the ATtiny85.
