# Exp69: dongle topology — direct PS/2 trackpoint on dabase_v2 right (no coprocessor)

## Hypothesis

Exp68 proved the direct-PS/2 setup (badjeff driver fork, `gpio-ps2` backend,
`ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS`, DAT→P0.08 / CLK→P0.06) moves the cursor on
a bench nice_nano. Porting the same device nodes + Kconfig into the production
**dabase_v2 right half** should work identically, and the existing split topology
should carry the input to the XIAO dongle central over BLE **without the
ATtiny85 / Pro Mini / I2C bridge**. Layer-toggle (temp_layer), scroll layer, and
the Exp51 volume layer are stripped — this experiment is cursor-only.

**Success = mouse confirmed to move.**

## Plan

- **Repo:** `zmk-config-dabaseV_0-2`, branch `Exp69` (off Exp67 `1dd3c95`).
- **west.yml:** `zmk-trackpoint-driver` → `zmk-ps2-trackpoint-driver`
  (`5ef6699e6a...`); drop `zmk-input-processor-{xyz,keybind,report-rate-limit}`
  (only served the removed scroll/volume/layer-toggle features).
- **New `dabase-v2-right-ps2.dtsi`:** `gpio_ps2` (scl P0.06, sda P0.08) + `tpoint0`
  (`zmk,input-mouse-ps2`, `disable-clicking`, no host-command props) + the PS/2
  IRQ-priority overrides (port of `ps2test_trackpoint.dtsi`).
- **Right overlay:** remove I2C pinctrl + `trackball@42`; repoint `trackball_split`
  → `&tpoint0` (peripheral), local listener for standalone USB builds (new 3-way
  `#ifdef`); EXT_POWER P0.13 now powers the trackpoint directly.
- **Dongle overlay:** listener → `&trackball_split` stays; strip temp_layer/scroll/volume.
- **Right conf:** `CONFIG_PS2=y/CONFIG_PS2_GPIO=y/PS2_UART=n` + NO_HOST_COMMANDS +
  SPEED_DIVISOR=1 + filters off + newlib libc; drop I2C driver config.
- **Keymap:** `&lt 5 J` → `&kp J`, `&lt 5 Y` → `&kp Y`, delete
  `scroll_layer`/`tp_layer`/`volume_layer`, `SOFT_OFF_KEY` → `&kp K`.
- **build.yaml:** right peripheral → `dabase_v2_right-ps2-direct` (no sleep for
  bring-up), standalone-usb keeps its local-listener role.
- **ZMK revision:** keep dabase's `fa33e35f` first; fall back to Exp68-verified
  `ac7f75b8` only if the driver fork fails against it.
- **Memory:** `experiments/Exp69/Exp69.md` + `Experiments.md` row.

## Hardware wiring (user-side)

Trackpoint on the right half: **DAT → P0.08** (freed), **CLK → P0.06** (freed from
MOT/irq), **VCC → EXT rail P0.13** (was ATtiny power), **GND**, **RST float**.
Internal pull-ups come from the Exp68 fork patch (`ps2_gpio.c`).

## Build / verification log

- Committed `6caa3db`, pushed `Exp69` → GH Actions run `33209473730`.
- **Run 1 FAILED (peripheral build only):** ZMK static assert in `input_split.c`
  — *"Peripheral input splits need an `input` property set"*. Root cause: the
  dabase shield `Kconfig.defconfig` defaults `ZMK_SPLIT_ROLE_CENTRAL=y` for the
  right shield, and my 3-way `#ifdef/#elif defined(CONFIG_ZMK_SPLIT)/#else`
  rewrite fell through to the wrong branch for the peripheral build
  (`SPLIT_ROLE_CENTRAL=n` via cmake-arg), so the peripheral's `zmk,input-split`
  node ended up enabled WITHOUT a `device` property.
  **Standalone USB right build succeeded** (role defaults centrally) and all
  other builds passed.
- **Fix (`08313df`):** restored the proven 2-way structure —
  `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` → local `trackball_listener` on
  `&tpoint0` (covers standalone USB right, role defaults y);
  `#else` → `trackball_split { device = <&tpoint0>; }` (peripheral, role forced n).
  Rebuild: run `33210214015`.
- Lesson: **do not re-architect working `#ifdef` split/role branches without
  reading the shield `Kconfig.defconfig` role defaults.** The right shield is
  CENTRAL by default; only the explicit `-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`
  cmake-arg makes it a peripheral.

## Conclusion

<empty — fill after hardware test>