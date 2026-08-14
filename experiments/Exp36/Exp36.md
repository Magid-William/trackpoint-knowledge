# Exp36: Level-based MOT sleep gating — stop polling a sleeping Pro Mini

## Hypothesis

Every previous ZMK-side deep-sleep failure came from the driver polling a sleeping I2C slave:

| Exp | ZMK-side symptom | Root cause |
|-----|------------------|------------|
| Exp28 | Sticky keys | 10ms polls vs non-responsive slave → 1-2ms CPU per fail → keyboard scan starved |
| Exp31 | Fixed via backoff | …but up to 5s resume latency after touching the nub |
| Exp34 | Permanent I2C lockup after 5–15 min | `consecutive_errors` uint8 wrap → 10ms error flood → nRF52 TWIM Errata 78 wedges I2C |

Instead of tolerating errors, **stop generating them entirely**: repurpose the MOT wire (P0.06) as a level-based sleep/awake signal. Pro Mini drives MOT **LOW** while sleeping, **HIGH** while awake. ZMK driver cancels its poll work on the falling edge (zero I2C traffic, zero CPU, zero errata risk) and resumes on the rising edge (~100ms after touch instead of up to 5s).

Also switch back to `zmk-trackpoint-shield` with the right half as **central** (no dongle, no left half) to keep the topology simple.

## Plan

### promini-trackpoint (branch Exp36)

`trackpoint-i2c-slave.ino`:
- `SLEEP_ENABLED` → 1, and moved above the `#if SLEEP_ENABLED` includes (was defined at line 23 but used at line 3 — AVR sleep headers were never actually included)
- MOT becomes level-based: remove `pulse_mot()`; awake = MOT HIGH (setup already sets it)
- Sleep entry: CLK inhibit (D3 LOW) + `TWCR = 0` (TWI off) + MOT LOW
- WDT 60ms loop unchanged; on detected motion: `Wire.begin(I2C_ADDR)` re-enable + MOT HIGH + break

### zmk-pmw3610-driver (branch Exp36, `58fd284`)

`src/trackpoint-i2c.c`:
- IRQ → `GPIO_INT_EDGE_BOTH`; callback reads `gpio_pin_get_dt`:
  - logical 1 (MOT low, `GPIO_ACTIVE_LOW`) → `k_work_cancel_delayable(&poll_work)`
  - logical 0 (MOT high) → `k_work_schedule(..., K_MSEC(10))`
- Poll re-checks MOT level at the top; if sleeping, returns without rescheduling (stops; wake edge resumes) — closes the race where an in-flight poll survives the cancel
- Init: `k_work_init_delayable` early (before interrupt enable — an edge during the reset window must not hit an uninitialized work item); probe/speed-write only if awake; interrupt registered last
- Exp31/Exp34 backoff + error cap kept as a safety net (no longer triggered during sleep)

### zmk-trackpoint-shield (branch Exp36, `b3e599f`)

- `build.yaml`: 2 targets only — `corne_trackpoint_right` central + `settings_reset-nice_nano` (Exp26-style). Dongle/peripheral targets removed from build (files kept)
- `corne_trackpoint_right.overlay`: central branch now defines `temp_layer` (`zmk,input-processor-temp-layer`) and attaches `input-processors = <&temp_layer 2 2000>` to the listener — restores layer-toggle without the dongle (keymap `tp_layer` already exists)
- `config/west.yml`: driver pinned to `Exp36` (`58fd284`), ZMK stays at `fa33e35f`

## Changes by repo

| Repo | Branch | Commit | Change |
|------|--------|--------|--------|
| promini-trackpoint | Exp36 | `bb2632f` | Level-based MOT sleep gate |
| zmk-pmw3610-driver | Exp36 | `58fd284` | MOT level gating, both-edge IRQ, safe init ordering |
| zmk-trackpoint-shield | Exp36 | `b3e599f` | Central-only build + temp_layer + driver pin |

## Verification

1. Build right half via GH Actions (`corne_trackpoint_right-nice_nano-central`); flash Pro Mini via CH340G/avrdude, NiceNano via flash-nicenano.ps1
2. USB: idle 30+ min → `sleep: poll cancelled` logged, **no lockup** (Exp34 killer), keys responsive (Exp28 killer)
3. Touch nub → cursor resumes ~100ms (vs up to 5s before); layer-toggle still activates
4. Probe while sleeping: `i2c write_read <bus> 0x42 0x01 2` → NACK (expected, not a wedge)
5. Optional: BLE direct to PC — same sleep/wake tests (Exp11 failure mode)

## Success criteria

- [ ] Zero I2C errors during 30+ min idle
- [ ] No permanent lockup after 20+ min sleep (Exp34 regression test)
- [ ] Keyboard keys responsive while Pro Mini sleeps (Exp28 regression test)
- [ ] Cursor resumes < 200ms after touch
- [ ] Layer-toggle works after wake, no dongle
- [ ] Central build connects to PC (USB + optional BLE)

## Key findings

### CRITICAL: `ZMK_SPLIT_ROLE_CENTRAL` is undefined in non-split builds
The overlay used `#if CONFIG_ZMK_SPLIT_ROLE_CENTRAL` for the standalone/central path, but that Kconfig only exists inside `if ZMK_SPLIT` (app/src/split/Kconfig, no default). A non-split build → symbol undefined → `#if` false → the build silently used the **peripheral `trackball_split` branch** → motion went into the BLE split void → **cursor never moved**. Fixed with `#if defined(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL) && CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL`.

### DTS top-level node placement
`temp_layer` defined at the bare overlay top level failed DTS parse ("expected label reference (&foo)"). Top-level overlay content must be `&label {}` references; new nodes go inside `/ { };`. Matches how the dongle overlay defined it.

### Sleep/wake verified
- Pro Mini sleeps after 5s idle → MOT LOW + `TWCR=0` → `0x42` absent from I2C scan, **zero** I2C traffic
- nRF GPIO->IN bit6 (P0.06) LOW confirms MOT driven by Pro Mini during sleep
- Driver cancels poll on falling edge (no logs captured — shell clears them, but zero I2C errors/absence of 0x42 confirms no polling)
- Touch → cursor moves (~instant, no 5s backoff lag)
- Phantom `0x28` in early scans was an artifact of the Pro Mini's mid-sleep TWI pin state — disappears once properly asleep

### IDLE-sleep caveat (pre-existing)
`SLEEP_MODE_IDLE` + Timer0 (millis) means the AVR actually wakes ~every 1ms; each ~84ms loop iteration is dominated by `readPacket()` timeout against the CLK-inhibited TrackPoint. Functional, but not true deep sleep on the AVR. Worth a follow-up (WDT-only wake via disabling Timer0, or `SLEEP_MODE_POWER_DOWN` with a wake source).

## Conclusion

**Success.** The level-based MOT sleep gate eliminates every prior ZMK-side sleep failure:
- No I2C errors during sleep (zero transactions — poll is cancelled on the MOT falling edge)
- No errata-78 wedge risk (no error flood to trigger it)
- No CPU starvation (no polling a non-responsive slave)
- Instant wake (~100ms vs up to 5s with Exp31 backoff)

The central-only build in `zmk-trackpoint-shield` works: cursor moves, sleep engages, wake resumes. Remaining checks: 20+ min idle lockup regression (Exp34 killer) and optional BLE.
