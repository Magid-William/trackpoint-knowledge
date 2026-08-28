# Exp69: dongle topology — direct PS/2 trackpoint on dabase right (no coprocessor)

## Hypothesis

Exp68 proved the direct-PS/2 setup (badjeff driver fork, `gpio-ps2` backend,
`ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS`, DAT→P0.08 / CLK→P0.06) moves the cursor on
a bench nice_nano. Porting the same device nodes + Kconfig into the production
**dabase_v2 right half** should work identically, and the existing split topology
should carry the input to the XIAO dongle central over BLE **without the
ATtiny85 / Pro Mini / I2C bridge**. Layer-toggle (temp_layer), scroll layer, and
the Exp51 volume layer are stripped — this experiment is cursor-only.

**Success = mouse confirmed to move.** Flash standalone first, then dongle.

> Note: an earlier "Exp69" attempt existed and is deliberately IGNORED — the user
> said it was bad because it wrongly pointed at a different repo to port. This is
> the fresh rebuild from `origin/main`.

## Plan

- **Repo:** `zmk-config-dabaseV_0-2`, branch `Exp69` (fresh, from `origin/main` 32c30ec).
- **west.yml:** ZMK pinned `ac7f75b8` (Exp68 known-good); `zmk-trackpoint-driver`
  (promini-i2c) → `zmk-ps2-trackpoint-driver` `5ef6699`; drop
  `zmk-input-processor-{xyz,keybind,report-rate-limit}` (only served scroll/volume
  /layer-toggle).
- **New `dabase-v2-right-ps2.dtsi`:** verbatim copy of `ps2test_trackpoint.dtsi`.
- **Right overlay:** remove i2c0/trackball@42; split/listener on `&tpoint0`
  (central → local listener, peripheral → split); EXT_POWER P0.13 powers TP.
- **Dongle overlay:** plain listener on `trackball_split`, no input-processors.
- **Keymap:** strip `tp_layer`/`scroll_layer`/`volume_layer`; K/J/Y plain.
- **build.yaml:** `dabase_v2_right-ps2-direct` (peripheral) +
  `dabase_v2_right-standalone-usb` (central/USB) + dongle + left + settings_reset.
  Plain board targets `nice_nano`/`xiao_ble` (the `//zmk` qualifier does not exist
  at ac7f75b8 — Exp68 lesson); workflow pinned `@v0.3` (Exp68 era).

## Builds / commits

- `b38e1ea` — main Exp69 port (7 files, 165+/194−).
- `7d76b6f` — build.yaml plain board targets (fix run-1 `Board qualifiers /nrf52840/zmk not found`).
- `96d7d01` — workflow `@main` → `@v0.3` (fix run-2 "Missing ZMK Compat" check).
- GH run `33219195923` — **all 8 builds pass**; artifacts in `build/Exp69/firmware/`.

## Hardware bring-up (COM7 right half)

Order flashed & result:

1. `dabase_v2_right-standalone-usb.uf2` (role central) → **NO cursor** (user: no mouse movement).
2. Exp68 known-good `ps2test_right-nice_nano.uf2` → **cursor moves** (user: ready, cursor moved).
   → **wiring is NOT the problem**; dtsi is byte-identical.
3. `dabase_v2_right-ps2-direct.uf2` (role **peripheral**) → **cursor moves** ✓
   → `trackball_split { device = <&tpoint0> }` path works; the PS/2 layer is correct.

**GPIO probe** (`devmem 0x50000510`, both failing & working dabase builds): `0x40140` —
CLK/DAT (P0.06/0.08) idle high, P0.13 LOW on both. P0.13/EXT rail was a red herring,
not the discriminator.

## Diagnosis (active)

The only broken variant is the **standalone-central** path — `trackball_listener`
`status="okay"` on `&tpoint0` when `CONFIG_ZMK_SPLIT_ROLE_CENTRAL` defaults y. The
peripheral split→forward path (what the dongle uses) provably works. Root-cause
hypotheses for the central-listener path:

- The `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` branch may not be enabling the listener
  the way the promini-era (proven) config did — compare with the old
  `dabase_v2_right.overlay` (Exp67) which had `scroll_override`s and pointed
  listener at `&trackball`, and note promini-era **standalone** still relied on a
  central USB build.
- Possible missing `CONFIG_ZMK_USB`/HID config, or the `zmk,input-listener` under a
  role-central split build needs the input-split to be the listener source (as the
  old config did: listener device = `<&trackball_split>` by default, overridden to
  `<&trackball>` under central).

## Next steps

1. Fix the standalone-central variant so it moves too (nice-to-have; see diagnosis).
   OR treat dongle as the primary success path (peripheral build already proven).
2. **Dongle topology:** flash `dabase_v2_right-ps2-direct` (right, already proven),
   `dabase_v2_left` (left), `dabase_v2_dongle-usb-log` (XIAO, COM21 shell,
   `XIAO-SENSE` drive). Pair; keys first, then nub-move via question tool
   (ready/not ready).
3. Log known-good SHAs + run IDs in the conclusion.

## COM map

- COM7 = right nice_nano (flash target + shell).
- COM21 / COM22 = XIAO dongle (COM21 answers `uart:~$`).
- COM3/COM4 = BT SPP (hang on open — ignore).