# Exp27: Dongle Topology — XIAO BLE Central + NiceNano Peripheral

## Architecture

```
PC ──USB HID──→ XIAO (dongle/central) ──BLE──→ NiceNano (peripheral) ──I2C──→ Pro Mini ──PS/2──→ TrackPoint
```

## Changes from Exp26

### zmk-trackpoint-shield (Exp27 branch, based on Exp26)

| File | Change |
|------|--------|
| `corne_trackpoint.dtsi` | Added `split_inputs` + `trackball_split` + `trackball_listener` (listener disabled by default, points to split) |
| `corne_trackpoint_right.overlay` | Dual-mode via `#if CONFIG_ZMK_SPLIT_ROLE_CENTRAL`: standalone (listener→trackball) vs peripheral (split→trackball, listener→split) |
| `trackpoint_dongle.overlay` | Clean dongle: mock kscan + enable listener (no input-processors) |
| `trackpoint_dongle.conf` | Added `CONFIG_ZMK_USB=y`, `CONFIG_ZMK_POINTING=y` |
| `corne_trackpoint_right.conf` | Added `CONFIG_LOG_PRINTK=y` |
| `build.yaml` | 5 targets: standalone, peripheral, dongle, 2× settings_reset |
| `config/west.yml` | Driver pinned to `041f096` (extensive logging) |

### promini-trackpoint

No changes (Exp26 firmware still running).

### zmk-pmw3610-driver

No changes (already has extensive logging from Exp26).

## Build targets

```yaml
- corne_trackpoint_right-nice_nano-standalone
- corne_trackpoint_right-nice_nano-peripheral    # BLE peripheral to dongle
- trackpoint_dongle-xiao_ble                     # BLE central, USB HID to PC
- settings_reset-nice_nano
- settings_reset-xiao_ble
```

## Key fixes

1. **`device` property** (not `input`): The `zmk,input-split` binding uses `device`, not `input`. The `#if CONFIG_ZMK_SPLIT_ROLE_CENTRAL` correctly adds `device = <&trackball>` only on the right half (not in .dtsi, so dongle doesn't reference undefined `trackball`).

2. **No input-processors**: Dongle overlay is minimal — no `input-processors` that could drop events.

3. **Pinned ZMK**: Using `fa33e35f` working SHA from Exp23.

## Success

- [x] All 5 builds pass
- [x] Peripheral flashed to NiceNano, dongle flashed to XIAO
- [x] Auto-pair between peripheral and dongle
- [x] Cursor moves via XIAO USB HID
- [x] I2C scan shows Pro Mini at 0x42 on peripheral side
