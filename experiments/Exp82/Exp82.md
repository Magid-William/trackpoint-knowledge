# Exp82 — ps_gaming layer + trackpoint as right stick (layer 5 only)

## Hypothesis

A `ps_gaming` keymap layer (index 5) with PlayStation-style bindings can be toggled by a `PSgaming` combo (`&tog 5`, key-positions 40 43), and the trackpoint can act as a right joystick stick **only while layer 5 is active** (normal mouse cursor otherwise).

## Plan

1. Add `ps_gaming` layer (idx 5) to `config/dabase_v2.keymap` with PlayStation bindings (PS, L1/L2/L3, R1/R2/R3, △◯✕□, Select, Start, dpad ↑←↓→).
2. Add `PSgaming` combo: `bindings = <&tog 5>; key-positions = <40 43>;`
3. Add `badjeff/zmk-hid-joystick` to `config/west.yml` (per `badjeff/zmk-hid-io` research; joystick spin-off gives gamepad HID axes).
4. Enable `CONFIG_ZMK_HID_IO_JOYSTICK=y`, `CONFIG_USB_HID_DEVICE_COUNT=2`, `CONFIG_USB_DEVICE_HID=y`, `CONFIG_ZMK_HID_IO=y` in `config/dabase_v2.conf` (+ Kconfig.defconfig if needed).
5. Device tree: joystick input-listener forwarding trackpoint X/Y to joystick usage page via `input-processor-fwd-to-hid-io`.
6. **Core challenge:** gate the trackpoint→stick forwarding so it's active only when layer 5 is on (input listeners aren't natively layer-gated; may need a custom input processor or listener enable/disable tied to layer state).
7. Build all 4 shields via GH Actions, flash right half + dongle, verify stick works only on layer 5.

## References

- https://github.com/badjeff/zmk-hid-io (HID usage page module)
- https://github.com/badjeff/zmk-hid-joystick (joystick/gamepad HID spin-off)
- Existing layer-toggle pattern: Exp29/Exp72 `temp_layer` input processor on the dongle listener.

## Status

In progress — layer + combo + joystick foundation implemented; layer-gated stick forwarding pending.

## Next

Build & flash, then solve the layer-gating for trackpoint→right-stick.
