# Exp19: Real PS/2 TrackPoint over I2C

## Hypothesis

Real TrackPoint motion data from the PS/2 interface, read by the Pro Mini and exposed via I2C burst, will drive cursor movement on screen through the existing ZMK `trackpoint-i2c` driver — with proper MOT pin signaling and 50Hz read gating.

## Plan

### Wiring

Same as Exp17/Exp18:

| Pro Mini | I2C | NiceNano V2 |
|----------|-----|-------------|
| D18 (A4) | SDA | P0.17 (4.7kΩ pull-up) |
| D19 (A5) | SCL | P0.20 (4.7kΩ pull-up) |
| D14 (A0) | MOT | P0.06 |
| GND | GND | GND |

TrackPoint PS/2 (unchanged from previous experiments):

| Pro Mini 3.3V | TrackPoint |
|---------------|------------|
| D7 | SDA |
| D3 | SCL |
| D4 | NPN GND switch |
| D6 | P-MOSFET VCC switch |

### Pro Mini Changes

**File:** `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino`

1. **50Hz read gate** — PS/2 reads are throttled to once per 20ms via `READ_INTERVAL_MS`, preventing back-to-back reads from saturating the loop
2. **MOT pin pulse** — After each valid PS/2 packet with non-zero motion, D14 pulses LOW for 100µs to signal the ZMK side
3. **No button handling** — TTP223 touch sensor gates motion (as before), no physical buttons exist on the TrackPoint

No changes to the I2C burst protocol: still 2-byte `[X, Y]` at BURST_ADDR 0x12.

### ZMK Driver

**No changes.** The `trackpoint-i2c.c` driver already:
- Polls at 10ms via `k_work_delayable`
- Reads 2-byte burst at 0x12
- Accumulates deltas and calls `input_report()` with `INPUT_EV_REL` / `INPUT_REL_X` / `INPUT_REL_Y`
- Has MOT GPIO configured (callback no-op, polling used instead)

### No Changes To

- `pmw3610.c` — untouched (SPI driver, not in use)
- `corne_trackpoint_right.overlay` — already has I2C, MOT pin, input listener
- `corne_trackpoint_right.conf` — already has `CONFIG_TRACKPOINT_I2C`, `CONFIG_I2C`, etc.
- `zmk-pmw3610-driver/Kconfig` — `TRACKPOINT_I2C` menu exists
- `zmk-pmw3610-driver/CMakeLists.txt` — already compiles `trackpoint-i2c.c`
- `PS2Trackpoint` library — no changes needed

### I2C Protocol

- Master writes `0x12` (burst address)
- Master reads 2 bytes: `[X, Y]` (int8, raw motion deltas)
- Same as Exp18 — no protocol change

### Success Criteria

- [ ] Pro Mini reads real PS/2 TrackPoint motion when nub is touched
- [ ] I2C burst delivers X/Y deltas at 50Hz
- [ ] MOT pin pulses LOW for 100µs on each non-zero motion read
- [ ] ZMK driver polls and reports motion to HID
- [ ] Cursor moves on screen when touching and moving the nub
- [ ] Sleep/wake works via TTP223 touch

## Findings (Round 1)

Tested with serial log capture:

1. **Wake drift confirmed** — After sleep/wake, the baseline (EMA from the previous session) is stale. The user's finger is already pressing during wake, so the raw PS/2 values are biased. The baseline slowly tracks the new bias, but on finger release the offset creates apparent "gravity" drifting for ~32+ samples.

2. **Erratic burst spikes** — Single packets like `108,18` pass through the existing `abs(x)>=127` clamp. The 20ms gate and 5-packet wake discard don't catch these.

3. **Persistent residual drift** — Even well past wake transients (~45+ samples into the session), the baseline continues to drift while raw output is pinned at `-4`. The EMA time constant causes the signal to lag real TrackPoint internal calibration changes.

## Fixes (Round 2)

| Fix | Detail |
|-----|--------|
| **Baseline reset on wake** | `base_x`, `base_y` moved from `loop()`-local scope to file scope; zeroed in `enter_sleep()` after wake |
| **Time-based wake discard** | `wake_discard = 5` (packet count) replaced with `wake_deadline = millis() + 800` (800ms time window) — more robust since PS/2 packet rate varies during power-on settling |
| **MAX_DELTA 60 spike gate** | `#define MAX_DELTA 60` — discards packets where `abs(x) > 60 \|\| abs(y) > 60` as a new `else if` before normal processing |
| **Raw + baseline debug logging** | Every 10th non-zero motion packet prints `r<raw_x>,<raw_y> b<baseline_x>,<baseline_y> o<out_x>,<out_y>` — allows direct diagnosis of late-session drift in the next log |

## ZMK Driver

Still **no changes.** The `trackpoint-i2c.c` driver remains stable.

## Success Criteria

- [x] Pro Mini reads real PS/2 TrackPoint motion when nub is touched
- [x] I2C burst delivers X/Y deltas at 50Hz
- [x] MOT pin pulses LOW for 100µs on each non-zero motion read
- [x] ZMK driver polls and reports motion to HID
- [x] Cursor moves on screen when touching and moving the nub
- [x] Sleep/wake works via TTP223 touch

## Key Findings

### TrackPoint power cycling causes drift

When VCC/GND are cut during sleep (NPN/PMOS toggling), the TrackPoint re-calibrates internally on power-up. Since finger pressure is already present at that moment (TTP223 triggered wake), the TrackPoint's internal calibration is biased. The one-shot software calibration (`calib_x/calib_y`) can't fully compensate because the TrackPoint's internal baseline continues to shift over 1-2 seconds as it settles.

With the TrackPoint **continuously powered** (MCU-only sleep), no drift occurs: the TrackPoint calibrates once at initial power-on and stays stable.

**Conclusion:** NPN/PMOS power gates should not be toggled during sleep. The Pro Mini's power-down sleep + TTP223 wake provides adequate power savings while leaving the TrackPoint powered.

### One-shot calibration beats EMA baseline

The initial approach used a continuously-adapting EMA baseline (`base_x += raw - base_x/32`) that always lagged behind the TrackPoint's internal settling. Replacing it with a **one-shot calibration** captured during the first 400ms after wake eliminates the "gravity" drift entirely.

### Self-centering lag

Even with power always on, the TrackPoint's raw output can take seconds to settle back to zero after the finger lifts (its internal spring returns slowly). The 100ms cleanup timeout (`last_ps2_ms`) zeros `burst_x/burst_y` when PS/2 packets slow down, but if the TrackPoint keeps sending non-zero values, the cursor continues drifting. The fix was to keep the TrackPoint powered (no re-calibration artifacts) — the one-shot calibration subtracts any persistent bias.

## Changes Made

| File | Action |
|------|--------|
| `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino` | 50Hz read gate, MOT pin pulse, one-shot calibration (400ms), MAX_DELTA 25 spike filter, burst clear on discard, INVERT_Y via ZMK driver, MCU-only sleep |
| `zmk-pmw3610-driver/src/trackpoint-i2c.c` | INVERT_Y 0→1 |
| `zmk-trackpoint-shield/config/west.yml` | Driver ref Exp18→Exp19 |
| `experiments/Exp19/Exp19.md` | This file |
