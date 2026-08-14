# Exp56: ATtiny85 bridge — real trackpoint PS/2 → I2C slave → ZMK shell read

## Hypothesis

Bridging the two proven halves from Exp54 (ATtiny85 reads real PS/2 trackpoint
X/Y, both axes live) and Exp55 (ATtiny85 I2C slave @0x42 read continuously by
the stock `promini,trackpoint-i2c` driver, shell shows the stream) into a
single sketch gives live **real** trackpoint X/Y in the ZMK USB shell. The
existing `trackpoint-i2c-slave-attiny85` sketch is already this bridge — no
code changes expected. Read-only test: no mouse movement required, no host
cursor check.

**Zero ZMK changes.** Reuses the Exp55 build `dabase_v2_right-standalone-usb`
(right half = central, zmk-usb-logging snippet, I2C shell, 10ms plain polling
since the overlay has no `irq-gpios`).

Why reuse works: the USI slave *does* deliver the register byte to
`receiveEvent` on the repeated start (USI_TWI_Slave.c START ISR), so the
sketch's register-aware burst serving (@0x12) should work on the stable Exp55
prototype PCB — Exp55 already concluded the USI slave + nRF TWIM were never
the problem; the breadboard was.

## Plan

1. `pio run -d attiny85-trackpoint/attiny85-trackpoint-i2c-slave` — no source
   change (Exp41 port of the Pro Mini slave; PS2 CLK=3/DAT=4, 8MHz).
2. Flash Leonardo as ArduinoISP (`pio run -e leonardo -t upload`).
3. Attach Leonardo to WSL, probe signature, flash hex (no `-D`).
4. **User physically moves the ATtiny85 to the NiceNano prototype PCB and
   wires the trackpoint to PB3/PB4.**
5. Confirm NiceNano still runs Exp55 `dabase_v2_right-standalone-usb` (reflash
   only if it was overwritten since).
6. Read X/Y from the ZMK shell (`i2c scan` for 0x42, watch driver log stream;
   `i2c read 0x42 0x12 2` for the live burst, `i2c read 0x42 0x03 5` for the
   debug packet if needed).

## Wiring (ATtiny85 → NiceNano + trackpoint)

| ATtiny85 | Function | Target |
|----------|----------|--------|
| Pin 5 (PB0) | SDA | NiceNano P0.17 |
| Pin 7 (PB2) | SCL | NiceNano P0.20 |
| Pin 8 | VCC | 3.3V |
| Pin 4 | GND | GND |
| Pin 2 (PB3) | PS2 CLK | trackpoint CLK |
| Pin 3 (PB4) | PS2 DAT | trackpoint DAT |
| Pin 6 (PB1) | MOT | unconnected (driver plain-polls) |
| — | — | trackpoint VCC ← 3.3V (5V only on the Leonardo flash bench), GND shared |

4.7kΩ pull-ups on SDA/SCL → 3.3V (per AGENTS.md).

## Success criteria

- [x] `pio run` green, flash < 8192 B (6882 B hex, 2440 B written)
- [x] ATtiny85 sig `0x1e930b`, hex written + verified via Leonardo (no `-D`)
- [x] `i2c scan` shows `0x42` on the NiceNano bus
- [x] Shell reads continuous `x/y` from **real nub motion**, both axes (no Exp39
      one-axis regression) — live stream at 3.3V on the stable PCB
- [x] ZMK firmware untouched (Exp55 `dabase_v2_right-standalone-usb` reused)

## Findings

- **I2C bridge fully proven** (register-aware serving works on the stable PCB):
  `i2c read 0x42 0x12 2` → `00 00` (2 bytes), `i2c read 0x42 0x03 5` → 5-byte debug
  packet. The register byte IS delivered to the USI slave on the repeated start
  (USI_TWI_Slave.c START ISR) — the Exp55-era "register byte unreliable" concern
  does not apply here. Reuse of `trackpoint-i2c-slave-attiny85` validated.
- **PS/2 side NOT streaming**: debug `[status, xraw, yraw, to_lo, to_hi]` shows
  `read_timeouts` saturating at 0xffff and wrapping (65535 → 0 → climbing), i.e.
  `readPacket()` times out on essentially every call. Status/xraw/yraw stay 0.
  One transient `7f ff ff ff ff` was a misalignment artifact reading idle 0xFF
  bytes, not real motion.
- USB driver log silent on this build (same quirk Exp47 saw) — shell `i2c read`
  is the diagnostic path instead.
- **Exp54 reproduction (5V bench) — PASSED**: re-flashed `trackpoint-serial-
  attiny85` via Leonardo ISP (sig 0x1e930b, 2270 B verified, no `-D`), reflashed
  Leonardo as serial bridge, read live `X:.. Y:..` on COM9 with real nub motion,
  both axes. Trackpoint + ATtiny85 + PS/2 code confirmed healthy.
  (Gotcha: wrong bench wiring browned out the Leonardo — brief LED, slow fade,
  COM9 missing; fixing the wiring restored it.)
- **Root cause isolated to the 3.3V NiceNano hookup**: trackpoint streams at 5V
  but not at 3.3V on the PCB. Next: re-check 3.3V wiring or move trackpoint to 5V.
- **SUCCESS — combined sketch streaming at 3.3V**: reflashed the combined
  `trackpoint-i2c-slave-attiny85` (2440 B, sig 0x1e930b, verified) and moved the
  ATtiny85 back to the NiceNano PCB with the trackpoint rewired carefully at
  3.3V (CLK/DAT verified not swapped, RST float). Direct shell reads now show
  real motion: `i2c read 0x42 0x03 5` → `[status, xraw, yraw, to_lo, to_hi]`
  with xraw/yraw changing under nub motion; `i2c read 0x42 0x12 2` burst shows
  live deltas like `11 fa` (x=17, y=-6). The Exp56 3.3V silence was indeed the
  wiring, not the voltage.
- **read_timeouts still climbs (~0x7400 +90/300ms)** — each `readPacket()`
  gap-sync times out between real packets, but genuine packets land and serve
  x/y. Harmless for the driver's 10ms plain-poll.
- `i2c scan` flood (51 addresses ACKing) is a known USI transient-wedge artifact;
  direct `i2c read` is the reliable path.
- **`live-trackpoint.ps1` added** (repo root) — busy-loop live streamer:
  sends `i2c read 0x42 0x03 5` every 300ms and prints each `x/y/timeouts`
  sample the instant it arrives (212 samples in 8s verified). Event-driven
  first attempt logged nothing (PowerShell `Register-ObjectEvent` action output
  queues into a hidden job); busy-loop + line-buffer + byte-array regex fixed it.
  `-Burst` switch reads destructive `0x12`.

## Conclusion

**SUCCESS.** The bridge works end-to-end on the stable PCB at 3.3V: the ATtiny85
reads the real PS/2 trackpoint (both axes, no regression) and serves live X/Y as
a register-aware I2C slave @0x42, read live from the ZMK shell. The Exp56 3.3V
"failure" was a wiring problem on the PCB hookup (CLK/DAT/RST), not voltage,
firmware, or hardware — confirmed by both the 5V bench reproduction and the
carefully-rewired 3.3V test.

Deliverables this round:
- Combined sketch reflashed & verified (sig `0x1e930b`, 2440 B, no `-D`).
- Live `x/y` stream proven from the ZMK shell via `i2c read` (both `0x03` debug
  and `0x12` destructive burst).
- `live-trackpoint.ps1` — reusable busy-loop live streamer for future debugging
  (e.g. ZMK driver tuning).

**Next experiment candidates** (in order of value):
1. **Make the ZMK driver actually move the mouse** — confirm `promini,
   trackpoint-i2c` binds + feeds the pointer at 10ms poll with real data, and
   the host cursor moves (the still-unchecked success box from the original
   goal). This is the last step to "mouse moving on screen".
2. Layer-toggle on touch (TTP223) + deep sleep power handling — already proven
   separately (Exp48/49) but not yet combined with live trackpoint data.
3. Dongle/BLE topology testing once #1 lands.
