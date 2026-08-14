# Exp20: PS/2 CLK-inhibit WDT Sleep (Ditch TTP223)

## Hypothesis

Removing the TTP223 touch sensor and instead using the TrackPoint's CLK line (D3, INT1) with **clock inhibit** during sleep will provide reliable wake-on-touch without needing a separate sensor. Since the TrackPoint streams continuously at ~100Hz, CLK toggles constantly — so a simple FALLING interrupt would re-wake instantly. Instead, we **hold CLK LOW** during sleep (PS/2 protocol inhibit), forcing the TrackPoint to pause transmission. A WDT wake every 60ms releases CLK, checks for non-zero motion, and either stays awake or re-inhibits and re-sleeps.

This eliminates the TTP223 entirely while preserving:
- No TrackPoint power cycling → no calibration drift
- MCU-only sleep with low power
- Motion-detected wake with ~60ms latency

## Plan

### Wiring Changes

| Remove | Reason |
|--------|--------|
| TTP223 module (D2, VCC, GND, sensitive pad) | Replaced by CLK-inhibit + WDT wake |

No new wiring needed — PS2_CLK is already on D3, PS2_DAT on D7.

### Pro Mini Changes

**File:** `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino`

1. **Remove TTP223** — delete `TOUCH_PIN`, all `digitalRead(TOUCH_PIN)` references, TTP223 gate on PS/2 reads
2. **Always read PS/2** — no gating, continuous reading at 20ms interval
3. **Track motion** — `last_motion_ms` only updates on non-zero `burst_x/burst_y` (not zero-value stream packets)
4. **CLK inhibit sleep** — after `IDLE_TIMEOUT_MS` (1s) of no non-zero motion:
   - Set D3 (CLK) as OUTPUT LOW → TrackPoint stops transmitting
   - Disable TWI (`TWCR = 0`)
   - `LowPower.powerDown(SLEEP_60MS, ...)` — WDT wakes every 60ms
   - On wake: re-enable I2C, release CLK, read one packet
   - If non-zero motion → break sleep loop, stay awake
   - If zero/no packet → re-inhibit CLK, re-sleep
5. **Calibration** — unchanged, one-shot 400ms at boot. Not reset on wake (TrackPoint stays powered).

### No Changes To

- ZMK driver (`trackpoint-i2c.c`) — untouched
- ZMK shield (`.overlay`, `.conf`) — untouched
- PS2Trackpoint library — untouched

### Sleep/Wake Flow

```
Normal loop:
  readPacket() every 20ms
  if non-zero motion → update last_motion_ms, pulse MOT
  if no non-zero motion for 1000ms → enter sleep

Sleep:
  pinMode(CLK, OUTPUT) + digitalWrite(LOW)  ← inhibit TrackPoint
  TWCR = 0                                   ← disable TWI
  LowPower.powerDown(SLEEP_60MS, ...)

WDT wake:
  Wire.begin()                               ← re-enable I2C
  pinMode(CLK, INPUT_PULLUP)                 ← release CLK
  readPacket()
  if non-zero motion → stay awake (break sleep loop)
  if no motion → pinMode(CLK, OUTPUT, LOW) + TWCR = 0 → re-sleep
```

### Success Criteria

- [x] Pro Mini sleeps after 1s of no non-zero TrackPoint motion
- [x] CLK held LOW during sleep, TrackPoint stops transmitting
- [x] WDT wakes every 60ms, releases CLK, checks motion
- [x] Touching the nub produces non-zero motion → MCU stays awake
- [x] I2C responds correctly after wake
- [x] Cursor moves on screen
- [x] No false wakes, no calibration drift
- [x] Layer-toggle works (ported to I2C driver)

## Key Findings

### CLK inhibit works for PS/2 sleep
Holding CLK (D3) as OUTPUT LOW during sleep successfully inhibits TrackPoint transmission per PS/2 protocol. When CLK is released (INPUT_PULLUP) on WDT wake, the TrackPoint resumes streaming within <1ms. No power cycling needed.

### 60ms WDT polling is acceptable
The 60ms polling interval adds imperceptible latency to first-motion detection. The Layer-toggle timeout (2000ms) is orders of magnitude larger.

### Layer-toggle was never ported to I2C driver
The original `trackpoint-i2c.c` driver had zero layer-toggle code — no `zmk_keymap_layer_activate()`, no deactivation work, no DT properties. All of this was ported from `pmw3610.c` (SPI driver) with identical logic.

### CURRENT.UF2 stale file quirk
When entering the NRF bootloader, the stale `CURRENT.UF2` from a previous flash is auto-flashed before the user can copy a new UF2. Fix: delete `CURRENT.UF2` from the G: drive first, then copy fresh UF2.

## Changes Made

| File | Action |
|------|--------|
| `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino` | Remove TTP223, CLK-inhibit + WDT 60ms sleep, motion-based idle timeout |
| `zmk-pmw3610-driver/src/trackpoint-i2c.c` | Port layer-toggle: `#include <zmk/keymap.h>`, config/data fields, activation in poll, deactivation callback, init, DT_PROP in define macro |
| `zmk-pmw3610-driver/Kconfig` | Add `CONFIG_TRACKPOINT_I2C_LAYER_TOGGLE` |
| `zmk-pmw3610-driver/dts/bindings/promini,trackpoint-i2c.yml` | Add `layer-toggle` (int, default -1) and `layer-toggle-timeout-ms` (int, default 2000) |
| `zmk-trackpoint-shield/boards/shields/corne_trackpoint/corne_trackpoint_right.overlay` | Add `layer-toggle = <2>; layer-toggle-timeout-ms = <2000>;` to trackball node |
| `zmk-trackpoint-shield/boards/shields/corne_trackpoint/corne_trackpoint_right.conf` | Add `CONFIG_TRACKPOINT_I2C_LAYER_TOGGLE=y` |
| `zmk-trackpoint-shield/config/west.yml` | Driver revision Exp19→Exp20 |
| `experiments/Exp20/Exp20.md` | This file |
