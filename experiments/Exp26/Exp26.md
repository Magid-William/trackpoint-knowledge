# Exp26: I2C Pin Rewiring — TrackPoint DAT D7, CLK D3

## Hypothesis

The physical wiring has been updated but the Pro Mini firmware still uses the old Exp20-alt-pins assignment (PS2_CLK=2, PS2_DAT=3). Correcting these to match the new wiring (DAT→D7, CLK→D3) restores PS/2 communication.

The I2C pins between Pro Mini and Nice Nano are already correct in the Exp22 overlay and need no changes.

## Changes

### promini-trackpoint (Exp26 branch)

| File | Change |
|------|--------|
| `trackpoint-i2c-slave.ino` | `PS2_CLK` 2→3, `PS2_DAT` 3→7 |

All other pins (MOT_PIN=14, NPN_PIN=4, PMOS_PIN=6) and I2C address (0x42) unchanged.

### zmk-trackpoint-shield (Exp26 branch)

Branch created from Exp22. No code changes — the overlay already has the correct I2C pins:
- P0.17 (SDA), P0.20 (SCL), P0.06 (MOT/IRQ), P0.08 (RST)

### zmk-pmw3610-driver

No changes. Driver unchanged per AGENTS.md.

## Wiring

| Trackpoint → Pro Mini | |
|---|---|
| DAT → D3 | CLK → D7 |
| GND → GND | VCC → ACC |

> Note: Initial assumption was DAT=D7, CLK=D3. Working configuration was the inverse: DAT=D3, CLK=D7.

| Pro Mini → Nice Nano (I2C) | |
|---|---|
| A4 (SDA) → P0.17 | A5 (SCL) → P0.20 |
| A0 (MOT) → P0.06 | RST (via 10kΩ pull-up) → P0.08 |
| GND → GND | |

| Pro Mini → CH340G (flashing) | |
|---|---|
| RXI → TXD | TXO → RXD |
| GND → GND | VCC → 3.3V |
| RST → DTR (via 100nF) | |

## Success criteria

- [x] Pro Mini reads TrackPoint data (CLK D7, DAT D3)
- [x] I2C burst returns valid X/Y at 0x42
- [x] NiceNano polls via I2C and cursor moves on screen
- [x] All builds pass GitHub Actions

## Revision

| File | Branch | Status |
|------|--------|--------|
| `promini-trackpoint` | `Exp26` | In progress |
| `zmk-trackpoint-shield` | `Exp26` | Created from Exp22 |
| `zmk-pmw3610-driver` | `debug-printk` (unchanged) | Reference only |
