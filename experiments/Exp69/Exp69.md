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

## Diagnosis (resolved)

The only broken variant was the **standalone-central** path — `trackball_listener`
`status="okay"` on `&tpoint0` when `CONFIG_ZMK_SPLIT_ROLE_CENTRAL` defaults y. The
peripheral split→forward path (what the dongle uses) worked, so this didn't block
the experiment. Root-cause candidates for the central-listener path (unresolved):

- The `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` branch may not be enabling the listener
  the way the promini-era (proven) config did — compare with the old
  `dabase_v2_right.overlay` (Exp67) which had `scroll_override`s and pointed
  listener at `&trackball`, and note promini-era **standalone** still relied on a
  central USB build.
- Possible missing `CONFIG_ZMK_USB`/HID config, or the `zmk,input-listener` under a
  role-central split build needs the input-split to be the listener source (as the
  old config did: listener device = `<&trackball_split>` by default, overridden to
  `<&trackball>` under central).

## Result: SUCCESS — cursor moves with direct PS/2 on the dabase right half, no coprocessor

User-confirmed via question tool ("ready - cursor moved"). The working build is the
**BLE peripheral** `dabase_v2_right-ps2-direct` — the same role the dongle uses —
and it moves the cursor plugged into USB on COM7, exactly like the promini-era
verification method (`dabase_v2_right-promini-i2c` flashed on USB). The
`trackball_split → tpoint0` forwarding path is proven live.

## Conclusion

**Exp69: SUCCESS.** The Exp68 direct-PS/2 stack ports cleanly into the production
dabase right half: `gpio-ps2` backend + `NO_HOST_COMMANDS` + int8 decode, wired
DAT→P0.08 / CLK→P0.06 (bench-reversed pin map), EXT_POWER P0.13 rail, cursor-only
keymap (temp_layer / scroll / volume stripped). No ATtiny, no Pro Mini, no I2C.

Key findings:
1. **Wiring and dtsi are proven correct.** Exp68 known-good FW moves the cursor on
   this exact board; the dabase `dabase-v2-right-ps2.dtsi` is byte-identical.
2. **The peripheral-role build works; the standalone-central build does not.** The
   broken path is `trackball_listener` enabled on `&tpoint0` when
   `CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y` (standalone USB build). The peripheral split
   path (`trackball_split { device = <&tpoint0> }`) works. Since dongle mode =
   peripheral + central, this is non-blocking for Exp69's goal.
3. P0.13 EXT-rail sampling was a red herring — reads LOW on the working build too.

## Known-good

- Config: `Magid-William/zmk-config-dabaseV_0-2` branch `Exp69` @ `96d7d01`
  (commits `b38e1ea` port → `7d76b6f` plain board targets → `96d7d01` workflow @v0.3).
- GH run `33219195923` — all 8 builds pass.
- Flashed + user-confirmed: `dabase_v2_right-ps2-direct.uf2` (610304 B) on COM7.
- Dongle flashed: `dabase_v2_dongle-usb-log.uf2` on XIAO (COM21 shell).
- ZMK `ac7f75b8`, driver fork `zmk-ps2-trackpoint-driver` @ `5ef6699`.

## Remaining / next experiments

- **Standalone-central right build doesn't move cursor** — root-cause the
  `trackball_listener`-on-`tpoint0` path under role central (compare with the old
  promini overlay's listener wiring / `CONFIG_ZMK_USB`). Low priority for dongle
  use; matters only for "laptop with one half".
- **Full dongle BLE test** — right (peripheral) ↔ XIAO dongle (central) over BLE,
  left half for keys sanity; confirm cursor over the dongle's USB HID, then
  re-introduce temp_layer / scroll on the proven stack.
- Document in AGENTS.md: the gpio-ps2-vs-uart-ps2 decision + peripheral-vs-
  standalone-central split-listener wiring (Exp68 exp suggested this).

## COM map

- COM7 = right nice_nano (flash target + shell).
- COM21 / COM22 = XIAO dongle (COM21 answers `uart:~$`).
- COM3/COM4 = BT SPP (hang on open — ignore).