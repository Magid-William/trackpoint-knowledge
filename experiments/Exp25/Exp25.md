# Exp25: Snapshot of working dongle topology

## Hypothesis

Exp22 and Exp24 established a working BLE dongle topology (XIAO central, NiceNano peripheral, Pro Mini I2C slave). Exp25 is a snapshot branch with no code changes — purely to tag the verified working configuration.

## Architecture

```
PC ──USB HID──→ XIAO BLE (central/dongle) ──BLE split──→ NiceNano (peripheral) ──I2C──→ Pro Mini ──PS/2──→ TrackPoint
```

## Pin configuration (verified working 2026-07-28)

### TrackPoint → Pro Mini (PS/2)

| Pro Mini | TrackPoint |
|----------|------------|
| D2 | CLK |
| D3 | DAT |
| VCC (3.3V) | VCC |
| GND | GND |

> RST must stay float per Crunch PS/2 decoder design.

### Pro Mini → NiceNano (I2C)

| Pro Mini | NiceNano | Function |
|----------|----------|----------|
| A4 | P0.17 | I2C SDA |
| A5 | P0.20 | I2C SCL |
| A0 (D14) | P0.06 | MOT (IRQ) |
| RST | P0.08 | Reset via 10kΩ |
| GND | GND | Ground |

Both SDA and SCL: 4.7kΩ pull-ups to 3.3V.

## Firmware versions

| Device | Repo | Branch | Build |
|--------|------|--------|-------|
| Pro Mini (3.3V 8MHz) | `promini-trackpoint` | `Exp25` | `trackpoint-i2c-slave` (SERIAL_LOG=1, IDLE_TIMEOUT=30s, D2=CLK, D3=DAT, DEADBAND=3) |
| NiceNano (peripheral) | `zmk-trackpoint-shield` | `Exp25` | `corne_trackpoint_right` with `-DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n` |
| XIAO nRF52840 (dongle) | `zmk-trackpoint-shield` | `Exp25` | `trackpoint_dongle` |
| Driver | `zmk-pmw3610-driver` | `debug-printk` | Combined I2C transactions (Exp21-compatible) |

## ZMK revision pinnings

- ZMK: `fa33e35f11d2b15311973cda9fb89dcd2376888c` (July 26 working SHA, pinned in Exp23)

## Test procedure

1. Flash `settings_reset` to both NiceNano and XIAO (clear bonds)
2. Flash `trackpoint_dongle` to XIAO
3. Flash `corne_trackpoint_right` (peripheral) to NiceNano
4. Power both — auto-pair (peripheral advertises, central scans)
5. Connect XIAO to PC via USB
6. Move trackpoint nub

## Results

| Criterion | Status |
|-----------|--------|
| I2C initialization (`trackpoint-i2c INIT OK`) | ✅ |
| TrackPoint motion detected (`POLL: rawx=... rawy=...`) | ✅ |
| Motion reported (`REPORT: dx=... dy=...`) | ✅ |
| BLE auto-pair (peripheral ↔ dongle) | ✅ |
| XIAO USB HID enumeration | ✅ |
| Cursor moves with nub via dongle | ✅ |
| No stall or freeze during extended use | ✅ |

## Key observations

1. **I2C is reliable** — occasional `POLL FAIL: -5` (EIO) occurs when Pro Mini is in sleep/CLK-inhibit state, but recovers on next poll cycle.
2. **XIAO USB now works** — unlike Exp22 where XIAO showed only phantom devices, the Exp24 devicetree restructuring resolved USB enumeration.
3. **Auto-pair works** — no manual intervention needed; the dongle scans and connects to the peripheral automatically.
4. **Reset pin via 10kΩ** — the Pro Mini RST is driven by NiceNano P0.08 through a 10kΩ resistor, giving a clean boot state.

## Comparison to Exp22

| Aspect | Exp22 | Exp25 |
|--------|-------|-------|
| Dongle mode | Blocked (XIAO USB phantom) | Working |
| Standalone mode | Working | Not retested |
| ZMK revision | `main` | `fa33e35f` (pinned) |
| Devicetree structure | `split_inputs`/`listener` in `.dtsi` | Moved to `.overlay` |

## Next steps

- Exp25 serves as a stable baseline for any future experiments
- If regressions appear, `Exp25` branches can be used to verify the working state
