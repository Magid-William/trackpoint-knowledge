# Exp10: Deep Sleep with TTP223 INT0 Wakeup

## Hypothesis

Replacing the D19 pin-change interrupt sleep (Exp09) with D2 INT0 (`attachInterrupt`) gives a simpler, more reliable deep-sleep/wake flow. Using PS/2 idle (not TTP223 state) as the sleep trigger avoids false sleeps when the touch sensor is glitchy, and keeping SPI/serial/TrackPoint power enabled during sleep eliminates the re-init overhead that caused the 15s boot grace to be needed.

## Pin changes (from Exp09)

| Signal | Old Pin | New Pin | Reason |
|--------|---------|---------|--------|
| TTP223 OUT | D19 (A5) | **D2** | INT0 supports `attachInterrupt()` for clean Low-Power wake |
| PS/2 DAT | D2 | **D7** | Freed D2 for TTP223 |

CLK=D3, MOT=D14, NPN=D4 unchanged.

## Sleep/wake flow

```
Loop: read PS/2 → if data: reset idle timer → MOT pulse → serial debug
         │
         └── no PS/2 data for 2s? ──YES──→ enter_sleep():
                                              ├─ MOT = HIGH (stop pulses)
                                              ├─ attachInterrupt(0, wakeUp, RISING)
                                              ├─ LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF)
                                              ├─ [TTP223 touch → INT0 RISING → wake]
                                              ├─ detachInterrupt(0)
                                              └─ clear burst, reset state
```

## What's kept active during sleep

| Subsystem | State | Reason |
|-----------|-------|--------|
| TrackPoint (NPN) | HIGH (powered) | Instant PS/2 data on wake |
| SPI (SPE) | Enabled | NiceNano only reads on MOT — no pulses = no SPI |
| Serial (UCSR0B) | Enabled | CPU stopped anyway, no output during sleep |
| MOT pin | HIGH (idle) | No accidental ZMK trigger |

## Changes by file

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

- `TOUCH_PIN` 19 → 2
- `PS2_DAT` 2 → 7
- Added `NPN_PIN 4` — set HIGH in `setup()`
- Removed `ISR(PCINT1_vect)` (no longer needed)
- Added `wakeUp()` — empty ISR for INT0
- `enter_sleep()` simplified: no UCSR0B/SPCR/PCINT manipulation, just `attachInterrupt` + `LowPower.powerDown` + `detachInterrupt`
- Sleep trigger changed from TTP223-based to PS/2-idle-based (2s of no packets)
- 15s boot grace retained to let NiceNano complete PMW3610 init
- Updated header comment and boot Serial message

### `promini-trackpoint/trackpoint-diag/trackpoint-diag.ino`

- `PS2_CLK` 7 → 3, `PS2_DAT` 3 → 7

### `promini-trackpoint/trackpoint-serial-reader/trackpoint-serial-reader.ino`

- `CLK_PIN` 7 → 3, `DAT_PIN` 3 → 7
- Updated header comment

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md |
| `zmk-trackpoint-shield/` | No config changes needed |
| `PS2Trackpoint/` | Used as-is; pins passed at construction |
| `libraries/LowPower/` | Already vendored from Exp09 |

## Success criteria

- [x] Pro Mini builds via GH Actions (all 3 sketches)
- [x] Pro Mini flashes via WSL avrdude
- [x] Serial boot message: `MOT=D14  PS2 CLK=D3  DAT=D7  NPN=D4  TTP223=D2`
- [x] `Sleeping...` logged after 2s of no PS/2 data (post-15s boot grace)
- [x] TTP223 touch → `Woke!` logged → SPI/cursor resumes
- [x] SPI handshake works after wake (NiceNano reads burst)
- [x] Cursor moves on screen (full pipeline working)
- [x] Repeated sleep/wake cycles stable
- [x] D13 LED off during sleep

## Findings

1. **Sleep/wake cycle verified.** Serial output confirmed: boot → idle 2s → `Sleeping...` → TTP223 touch → `Woke!` → SPI resumes → `Sleeping...` again. Repeated stable.

2. **PS/2-idle based trigger works better than TTP223-state.** Using PS/2 data absence as the idle signal avoids false sleeps when the TTP223 is noisy or briefly disconnects. The 2s buffer provides a clean hysteresis.

3. **15s boot grace still needed.** Even with SPI kept enabled during sleep, the boot grace prevents the Pro Mini from sleeping during the NiceNano's PMW3610 async init sequence (power-up reset, clear observation, configure registers). Without it, the NiceNano would attempt SPI writes while the Pro Mini CPU is stopped, corrupting the init.

4. **D13 LED fix required SPI toggle.** The Pro Mini's built-in LED is tied to D13 (SCK). In SPI mode 3 (CPOL=1), SCK idles HIGH, keeping the LED on. Brief `SPCR &= ~_BV(SPE)` + `pinMode(13, OUTPUT)` + `digitalWrite(13, LOW)` before `powerDown()` kills the LED during sleep. SPI is fully restored on wake. No impact on NiceNano communication.

5. **Counter wrap is cosmetic.** The `spi_byte_count` (uint8_t) wraps at 256, producing log lines like `SPI bytes: 31 (+-210)`. Harmless — the +N delta is only correct under ~255 bytes/interval.

6. **Nano LEDs are hardware-wired.** The Arduino Nano used for avrdude passthrough has a power LED (to VCC) and TX/RX LEDs (driven by CH340). These cannot be software-controlled from the Pro Mini.

## Conclusion

**Exp10: Success!** Deep sleep with TTP223 INT0 wakeup is proven:

```
TrackPoint ──PS/2 (D3=CLK, D7=DAT)──→ Pro Mini ──SPI──→ NiceNano ──USB HID──→ cursor
                                           │
                                    NPN (D4) keeps TrackPoint powered
                                    TTP223 (D2) = INT0 wake source
                                    MOT (D14) → NiceNano IRQ

Loop: PS/2 data? → MOT pulses → SPI traffic → ...
      2s idle? → disable SPE, D13 LOW, powerDown
      TTP223 touch → INT0 RISING → wake → restore SPI → resume
```

Key improvements over Exp09:
- `attachInterrupt` instead of manual PCINT — cleaner, more portable
- PS/2-idle trigger instead of TTP223-state — more robust
- D13 LED off during sleep — lower power, visually confirms sleep state
- SPI and serial kept enabled — zero re-init overhead on wake

The Pro Mini's sleep current is dominated by the TTP223 module's quiescent draw. The ATmega328P itself in POWER_DOWN with BOD disabled draws sub-µA. Total sleep current with TTP223 powered is ~3-5µA from the TTP223 datasheet.
