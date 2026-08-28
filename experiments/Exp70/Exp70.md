# Exp70: dongle topology — direct PS/2 trackpoint on dabase right (no coprocessor)

## Hypothesis

Exp68's verified direct-PS/2 config (`zmk-config-ps2-test`, user said "Perfect")
was ported into the production **dabase_v2 right half** in Exp69, but bring-up
there ran only in standalone-USB mode and never tested the **dongle topology**:
right-half peripheral → BLE split → XIAO dongle central → USB HID.

This experiment builds and flashes the FULL dongle set (right peripheral +
XIAO dongle central + left peripheral) and confirms the cursor moves with the
nub over BLE through the dongle. No ATtiny/Pro Mini — the trackpoint is wired
**directly** to the nice_nano (gpio-ps2, `NO_HOST_COMMANDS`).

**Success = mouse confirmed to move.**

## Plan

- **Repo:** `zmk-config-dabaseV_0-2`, branch `Exp70` (off Exp69 — the PS/2
  migration already landed there; this experiment is the dongle test of the
  same config).
- **Keep from Exp69:** `west.yml` (zmk `fa33e35f` + `zmk-ps2-trackpoint-driver`
  `5ef6699e6a...`), `dabase-v2-right-ps2.dtsi` (gpio_ps2 + tpoint0, P0.06/P0.08,
  IRQ priorities), right overlay split/listener 2-way `#ifdef`, right conf
  (gpio-ps2, NO_HOST_COMMANDS, divisor 1, filters off), strip of
  layer-toggle/scroll/volume from the keymap, dongle overlay listener on
  `trackball_split`.
- **Not part of this experiment:** layer-toggle (temp_layer), scroll layer,
  volume layer — already stripped; cursor-only.
- **build.yaml:** already emits `dabase_v2_dongle` (usb-log + studio builds),
  `dabase_v2_left` (peripheral + sleep), `dabase_v2_right-ps2-direct`
  (peripheral, split role n), `dabase_v2_right-standalone-usb` (local central
  for bench checks).
- **Memory:** `experiments/Exp70/Exp70.md` + `Experiments.md` row.

## Build / verification log

- Committed `6d6f286`, pushed `Exp70` → GH Actions run `33216004137`.