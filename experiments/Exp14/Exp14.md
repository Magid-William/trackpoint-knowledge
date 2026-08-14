# Exp14: Dual Rail Power Switching + PS/2 Pin Float

## Hypothesis

Cutting both VCC (AO3401 P-MOSFET on D6) and GND (NPN on D4) simultaneously, plus floating the PS/2 data lines (D3, D7) with no pull-ups during sleep, eliminates all parasitic power paths through ESD protection diodes on the PS/2 lines. The TrackPoint draws zero current when asleep. TTP223 touch wakes the Pro Mini, which restores power and re-inits PS/2 before resuming SPI traffic.

## Background

In Exp10, only the GND rail was switched (NPN on D4). Despite this, parasitic power leaked through the PS/2 data lines (D3=CLK, D7=DAT) — the AVR's internal pull-ups fed 3.3V back through TrackPoint ESD diodes to its VCC rail, keeping the IC partially alive.

## Wiring changes (from Exp10)

| Signal | Old Pin | New Pin | Component |
|--------|---------|---------|-----------|
| VCC switch (P-MOSFET Gate) | (none) | **D6** | AO3401 Gate via 1kΩ — HIGH=OFF, LOW=ON |
| GND switch (NPN Base) | D4 | D4 (unchanged) | NPN base via 1kΩ — HIGH=ON, LOW=OFF |
| PS/2 CLK | D3 | D3 (unchanged) | Floated (INPUT, no pull-up) during sleep |
| PS/2 DAT | D7 | D7 (unchanged) | Floated (INPUT, no pull-up) during sleep |

### AO3401 P-MOSFET wiring

| AO3401 Pin | Connected to |
|------------|-------------|
| Source | Pro Mini 3.3V |
| Drain | TrackPoint VCC (ACC) |
| Gate | D6 via 1kΩ |

### NPN wiring (unchanged)

| NPN Pin | Connected to |
|---------|-------------|
| Base | D4 via 1kΩ |
| Collector | TrackPoint GND |
| Emitter | System GND |

## Sleep/wake flow

```
Loop: read PS/2 → if data: reset idle timer → MOT pulse → serial debug
         │
         └── no PS/2 data for 2s? ──YES──→ enter_sleep():
                                               ├─ MOT = HIGH (stop pulses)
                                               ├─ NPN_PIN = LOW  (GND floating)
                                               ├─ VCC_PIN = HIGH (VCC disconnected)
                                               ├─ CLK+DAT = INPUT (no pull-ups)
                                               ├─ disable SPI, D13 LOW
                                               ├─ attachInterrupt(0, wakeUp, RISING)
                                               ├─ LowPower.powerDown(SLEEP_FOREVER)
                                               ├─ [TTP223 touch → INT0 RISING → wake]
                                               ├─ detachInterrupt(0)
                                               ├─ VCC_PIN = LOW  (VCC restored)
                                               ├─ NPN_PIN = HIGH (GND restored)
                                               ├─ delay(5)  — power stabilize
                                               ├─ ps2.begin() — re-init PS/2 pins
                                               ├─ delay(50) — TrackPoint power-on
                                               ├─ restore SPI, clear burst
                                               └─ Serial.println("Woke!")
```

## What's kept/stopped during sleep

| Subsystem | State | Reason |
|-----------|-------|--------|
| TrackPoint VCC (P-MOSFET) | OFF | D6 HIGH, AO3401 blocks 3.3V |
| TrackPoint GND (NPN) | OFF | D4 LOW, NPN blocks ground path |
| PS/2 CLK (D3) | INPUT, no pull-up | No source for parasitic power |
| PS/2 DAT (D7) | INPUT, no pull-up | No source for parasitic power |
| SPI | Disabled | CPU stopped anyway |
| Serial | Enabled | CPU stopped, no output |

## Key decisions

1. **Dual rail switching** — Both VCC and GND must be cut because a single switched rail still allows parasitic paths through signal lines. The PS/2 CLK/DAT lines connect directly to the TrackPoint IC and can back-feed through ESD protection diodes even when GND is floating.

2. **PS/2 pin float** — Setting CLK and DAT to `INPUT` (no pull-up) during sleep is critical. Even with both rails cut, if the AVR pulled these pins HIGH, the voltage could still leak into the TrackPoint. By setting them to high-impedance, there's no voltage source at all.

3. **50ms delay after wake** — The TrackPoint needs time to power up and stabilize before reading PS/2 packets. First 1-2 packets after power-on may have garbage values (Exp06 finding). A 50ms delay avoids desync.

4. **`ps2.begin()` on wake** — The PS2Trackpoint library's `begin()` sets pins to `INPUT + PULLUP`. We can't just restore DDR/PORT manually — calling `begin()` is the cleanest re-init path.

5. **No boot grace needed after wake** — The NiceNano never restarted during sleep, so its PMW3610 init sequence was completed long ago. `boot_grace` stays `false` after the first wake.

## Timing budget

| Operation | Duration |
|-----------|----------|
| Sleep entry (power cut + pin float) | ~15ms |
| TrackPoint restart (power restore + begin + delay) | ~60ms |
| Time to first PS/2 packet after wake | ~70ms (60ms delay + ~10ms for TrackPoint to stream) |
| Time to first MOT pulse after wake | ~80ms |

## Changes by file

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

- Add `#define VCC_PIN 6`
- `setup()`: `pinMode(VCC_PIN, OUTPUT); digitalWrite(VCC_PIN, LOW);`
- `enter_sleep()`: cut both rails, float PS/2 pins, kill SPI
- Post-sleep wake: restore rails, `ps2.begin()`, restore SPI, clear burst
- Boot serial message: `MOT=D14  PS2 CLK=D3  DAT=D7  NPN=D4  VCC=D6  TTP223=D2`

### `zmk-trackpoint-shield/`

| File | Change |
|------|--------|
| `build.yaml` | Single target — `nice_nano//zmk` + `corne_trackpoint_right`, no dongle |
| `Kconfig.defconfig` | Add `ZMK_SPLIT_ROLE_CENTRAL` (right half is central) |
| `corne_trackpoint_right.conf` | Remove Exp12 thread-monitoring/BT_L2CAP Kconfigs |
| `corne_trackpoint_right.overlay` | Inline dtsi content (no split_inputs/listener) |
| `corne_trackpoint.dtsi` | Deleted — no longer needed |
| `trackpoint_dongle/` | Deleted — dongle approach abandoned |

### `AGENTS.md`

Add P-MOSFET wiring row to TrackPoint power section.

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md |
| `PS2Trackpoint/` | Used as-is |
| `flash-nicenano.ps1` | Already correct |
| `corne_trackpoint_right.keymap` | No changes needed |

## Success criteria

- [ ] Pro Mini builds via GH Actions (spi-slave artifact)
- [ ] Shield builds via GH Actions (single UF2)
- [ ] LED on TrackPoint VCC turns **OFF** during sleep
- [ ] TTP223 touch → LED turns ON → cursor moves on screen
- [ ] No parasitic power — TrackPoint VCC rail measures 0V when asleep
- [ ] PS/2 data resumes cleanly after wake (no garbage packets)
- [ ] SPI handshake intact after every wake cycle
- [ ] Repeated sleep/wake cycles stable (10+ cycles)
- [ ] D13 LED off during sleep

## Conclusion

**Status: Partial Success** — firmware runs and power-switching logic is correct, but hardware power delivery to TrackPoint could not be verified due to CH340G stability issues.

### What worked

- Exp14 firmware builds and flashes to Pro Mini (4964 bytes, verified)
- D13 blinks at boot confirm `setup()` executes fully on both builds
- NiceNano flashed with Exp14 shield UF2 via `flash-nicenano.ps1`

### What didn't work

- **TrackPoint LED stays off** — `setup()` sets D4 HIGH (NPN ON) and D6 LOW (P-MOSFET ON), but the TrackPoint doesn't receive power. Likely causes:
  - AO3401 P-MOSFET wiring or orientation issue (Source→3.3V, Drain→TP VCC, Gate→D6)
  - NPN transistor or base resistor issue
  - CH340G's 3.3V regulator unable to supply enough current after usbipd cycles
- **CH340G serial unreliable** — After WSL usbipd attach/detach cycles, the CH340G's USB endpoint stalls (`urb->status -104`). The Windows driver becomes unresponsive ("device not functioning") until a physical USB replug. This made serial diagnostics nearly impossible.
- **`-c stk500v1` avrdude** requires manual reset button press; `-c arduino` (DTR auto-reset) worked once but DTR via usbipd is unreliable per AGENTS.md.

### Hardware to check

| Component | Check |
|-----------|-------|
| AO3401 | Source to 3.3V? Drain to TP VCC? Gate to D6 via 1kΩ? Not swapped S/D? |
| NPN | Base to D4 via 1kΩ? Collector to TP GND? Emitter to GND? |
| TP VCC LED | Connected between switched VCC (AO3401 Drain) and GND with 1kΩ? |

### Next steps

- Verify D4/D6 voltage with multimeter while Pro Mini runs Exp14
- Test with external power instead of CH340G's 3.3V regulator
- Consider Exp15: hardware debugging of TrackPoint power rail
