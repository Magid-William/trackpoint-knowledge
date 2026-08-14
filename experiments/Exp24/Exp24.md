# Exp24: BLE Dongle with XIAO nRF52840 (I2C split transport)

## Hypothesis

Exp23 proved the ZMK main regression was the only thing blocking cursor movement in standalone mode. Now that ZMK is pinned, the peripheral and dongle topology should work. The one build failure from Exp23 (`trackball_split` missing `input` property) is fixed by restructuring the devicetree files.

## Architecture

```
PC ──USB HID──→ XIAO BLE (central/dongle) ──BLE split──→ NiceNano (peripheral) ──I2C──→ Pro Mini ──PS/2──→ TrackPoint
```

## Changes from Exp23

| File | Change |
|------|--------|
| `corne_trackpoint.dtsi` | Removed `split_inputs` and `trackball_listener` — now only matrix, layout, chosen |
| `corne_trackpoint_right.overlay` | Added `split_inputs` with `trackball_split` (with `input = <&trackball>` for peripheral), `trackball_listener` with `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` for standalone vs peripheral |
| `trackpoint_dongle.overlay` | Added `split_inputs` with `trackball_split` (central role, no input), `trackball_listener` pointing to `&trackball_split` |

## Build targets

```yaml
- standalone:  corne_trackpoint_right (no cmake-args) → listener device=<&trackball>
- peripheral:  corne_trackpoint_right (-DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n) → listener device=<&trackball_split>, split input=<&trackball>
- dongle:      trackpoint_dongle (central by default) → listener device=<&trackball_split>
```

## Test procedure

1. Flash `settings_reset` to both NiceNano and XIAO
2. Flash peripheral build to NiceNano
3. Flash dongle build to XIAO
4. Power both — they auto-pair
5. Connect XIAO to PC via USB
6. Move trackpoint nub — cursor should move through dongle

## Risks

- **XIAO USB enumeration** — known issue from Exp22. If phantom devices, try different cable/port or debug via XIAO shell
- **Dual `trackball_listener` label** — the label `trackball_listener` is defined in both overlays. Since they're built for different boards/shields, no conflict.

## Results

(To be filled after testing)
