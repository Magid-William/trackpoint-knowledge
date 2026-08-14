# Exp22: BLE Dongle with XIAO nRF52840 (I2C-based)

## Hypothesis

The I2C-based trackpoint pipeline (Exp17-21) is proven reliable over USB and direct BLE. Adding a XIAO nRF52840 as a BLE central dongle should work without the stall issues seen in Exp11, because:

- Exp11's stall was caused by the SPI pipeline's interrupt-driven `input_report()` rate overwhelming the split transport
- The I2C pipeline uses `k_work_delayable` polling at 100Hz with explicit `input_report()` calls — no interrupt floods, no byte-shift issues
- nRF52-to-nRF52 BLE should negotiate a fast connection interval (~7.5ms)
- USB HID from the dongle delivers at 1ms polling

## Architecture

```
PC ──USB HID──→ XIAO BLE (central/dongle) ──BLE split──→ NiceNano (peripheral) ──I2C──→ Pro Mini ──PS/2──→ TrackPoint
```

The right half NiceNano becomes a BLE peripheral. The XIAO dongle acts as central, receiving both key positions and pointing events over the split transport, and forwarding them to the PC as USB HID.

## Key features

### Dual-mode right half

The right half `corne_trackpoint_right` shield works in two modes, selected at build time:

| Mode | `ZMK_SPLIT_ROLE_CENTRAL` | Behavior |
|------|--------------------------|----------|
| Standalone (Exp21 compat) | `y` (default) | `trackball_listener` connects directly to `&trackball`, USB/BLE direct to PC |
| Peripheral | `n` (via cmake-args) | `trackball_split { device = <&trackball> }` forwards events to dongle via BLE |

Conditional logic in `corne_trackpoint_right.overlay` uses `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` to switch between the two.

### Shared corne_trackpoint.dtsi

The matrix transform, physical layout, chosen node, `split_inputs` (trackball_split), and a disabled `trackball_listener` (pointing to the split) are shared via `corne_trackpoint.dtsi`. Both the right half overlay and the dongle overlay include this file.

### Dongle shield (trackpoint_dongle)

A simplified shield with no physical keys (mock kscan). Includes the shared .dtsi, enables `trackball_listener`, and runs the same keymap as the right half. Uses `seeed_xiao` board.

## Changes by file

### zmk-trackpoint-shield (Exp22 branch)

| File | Change |
|------|--------|
| `boards/shields/corne_trackpoint/corne_trackpoint.dtsi` | **New.** Shared devicetree: matrix transform, physical layout, chosen node, split_inputs (trackball_split), disabled trackball_listener |
| `boards/shields/corne_trackpoint/corne_trackpoint_right.overlay` | **Modified.** Include .dtsi. Add `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` for standalone vs peripheral mode. Keep I2C pins, trackpoint-i2c node, kscan, col-offset. |
| `boards/shields/trackpoint_dongle/Kconfig.shield` | **New.** Shield identifier |
| `boards/shields/trackpoint_dongle/Kconfig.defconfig` | **New.** Dongle config: name, ZMK_SPLIT_ROLE_CENTRAL, ZMK_SPLIT, ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=1 |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.overlay` | **New.** Include .dtsi, mock kscan, enable trackball_listener |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.keymap` | **New.** Same keymap as right half |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.conf` | **New.** ZMK_POINTING, shell, logging |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.zmk.yml` | **New.** Metadata |
| `Kconfig.defconfig` | **No change.** `ZMK_SPLIT_ROLE_CENTRAL` default y stays; cmake-args overrides for peripheral build |
| `build.yaml` | **Modified.** 5 targets: standalone right, peripheral right, dongle, 2× settings_reset |
| `Experiments.md` | **Modified.** Added Exp22 row |

### zmk-pmw3610-driver

No changes. Driver unchanged per AGENTS.md.

### promini-trackpoint

No changes. Pro Mini firmware untouched.

## Build targets

```yaml
include:
  - board: nice_nano//zmk
    shield: corne_trackpoint_right
    snippet: zmk-usb-logging
  - board: nice_nano//zmk
    shield: corne_trackpoint_right
    cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
  - board: xiao_ble//zmk
    shield: trackpoint_dongle
    snippet: zmk-usb-logging
  - board: nice_nano//zmk
    shield: settings_reset
  - board: xiao_ble//zmk
    shield: settings_reset
```

## Flash & Pair Procedure

1. Flash **settings_reset** to NiceNano (clears bonds)
2. Flash **settings_reset** to XIAO (clears bonds)
3. Flash **trackpoint_dongle** to XIAO (dongle firmware)
4. Flash **corne_trackpoint_right** (peripheral build) to NiceNano
5. Power both — they auto-pair (peripheral advertises, central scans)
6. Connect XIAO to PC via USB — cursor should move with nub

> Both XIAO boards have identical hardware. The experiment XIAO is the one that shows up as a COM port (personal XIAO has no USB debug). Only flash the one with visible COM.

## Standalone recovery

To revert the right half to standalone mode, flash the **standalone** build of `corne_trackpoint_right` (first build.yaml target). No settings_reset needed — the split role is purely a firmware compile-time setting.

## Success criteria

- [ ] All 5 build targets pass via GitHub Actions
- [ ] NiceNano flashes with peripheral firmware and pairs to dongle
- [ ] XIAO flashes with dongle firmware
- [ ] Auto-pair between peripheral and dongle (no manual intervention)
- [ ] Cursor moves with nub via dongle USB HID
- [ ] Keyboard keystrokes work via dongle USB HID
- [ ] Standalone build still works as before (no regression)
- [ ] No stall or freeze during extended use

## Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Split transport stall (Exp11) | Unlikely — I2C pipe is polled, not interrupt-driven. No byte-shift or SPDR overwrite bugs. |
| XIAO board confusion | Personal XIAO has no COM port. Always flash the one visible in device manager. |
| Dual-mode overlay complexity | `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` is simple and well-understood. Both paths share the same I2C and kscan configuration. |
| Dongle needs USB logging for debug | `snippet: zmk-usb-logging` enabled in build.yaml. |

## Expected latency

nRF52-to-nRF52 BLE connection: ~7.5ms interval. ZMK split transport overhead: ~3.75ms. USB HID from dongle: ~1ms. Total: ~12ms expected.

Compare to direct BLE to PC host: 15-30ms typical. Dongle should be 2-3× faster.

## Replication failure & fixes

When re-testing Exp22 to confirm stability, **standalone mode had regressed** — cursor did not move. Debugging revealed three compounding issues:

### 1. Wrong I2C transaction type (`feature/no-irq` vs `Exp21`)

The `zmk-pmw3610-driver`'s `feature/no-irq` branch split combined I2C transactions (`i2c_write_read_dt`) into separate write+read (`i2c_write_dt` + `i2c_read_dt`). The Pro Mini's AVR TWI **cannot handle separate transactions** (STOP between them confuses the slave). The combined `i2c_write_read_dt` with repeated START is required.

**Fix:** `west.yml` must point to `Exp21` driver (combined transactions), not `feature/no-irq`.

### 2. `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` not evaluating correctly

The `#ifdef` in `corne_trackpoint_right.overlay` intended to switch between standalone (listener) and peripheral (split) mode. Due to Kconfig dependency ordering (`ZMK_SPLIT_ROLE_CENTRAL` depends on `ZMK_SPLIT`, which is selected by `ZMK_SPLIT_BLE`), the macro was sometimes undefined even when it should have been `y`. This caused the `#else` branch to execute, leaving the listener disabled in standalone mode.

**Fix:** Removed the `#ifdef`; always enable listener + split together. In standalone mode the split is unused; in peripheral mode the listener is unused but harmless.

### 3. `zip_ble_report_rate_limit` input processor blocking events

The input-processor may have been dropping motion events in certain ZMK main branch builds.

**Fix:** Removed `input-processors` from the listener.

### 4. Settings_reset builds had no shell

Flashing `settings_reset` to clear bonds would leave the device in a boot loop with no shell, making headless bootloader entry impossible. The user had to manually double-tap reset.

**Fix:** Added `snippet: zmk-usb-logging` to all builds including settings_reset.

### Permanent rule

These config options must be present on **every** build, always:
```
CONFIG_I2C_SHELL=y
CONFIG_SHELL=y
CONFIG_KERNEL_SHELL=y
CONFIG_LOG=y
snippet: zmk-usb-logging
```

## Conclusion

**Standalone mode: Success** — cursor moves with nub over USB HID.

**Dongle mode: Blocked** — the XIAO dongle firmware builds and flashes but the XIAO's USB is not reliably visible to Windows (phantom devices only). Needs separate debugging, possibly hardware (cable, port, or bootloader corruption).

### Key findings

1. **AVR TWI requires combined I2C transactions** with repeated START. Split write+read with STOP between them fails.
2. **Always enable both listener and split** in the overlay — don't rely on `#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL` which has Kconfig ordering issues.
3. **Input processors can silently drop events** — remove them during debugging.
4. **Every build must have USB logging** for headless bootloader entry, including settings_reset.
5. **Check driver branch compatibility** — the `feature/no-irq` split I2C change broke AVR TWI compatibility.

### Dual-mode right half

The right half firmware is built in two variants:
- **Standalone** (`corne_trackpoint_right-standalone`) — direct USB/BLE — **verified working**
- **Peripheral** (`corne_trackpoint_right-peripheral`) — BLE peripheral to dongle — **builds but untested** (XIAO connectivity issues)

Both share the same shield files; the mode is selected at build time via `cmake-args`.

### Build artifacts

All 7 build targets pass via GitHub Actions:
- `corne_trackpoint_right-nice_nano-standalone.uf2`
- `corne_trackpoint_right-nice_nano-peripheral.uf2`
- `trackpoint_dongle-xiao_ble__zmk-zmk.uf2`
- `settings_reset-nice_nano__zmk-zmk.uf2` (with USB logging)
- `settings_reset-xiao_ble__zmk-zmk.uf2` (with USB logging)
- `settings_reset-xiao_ble__zmk-zmk.uf2` (with USB logging)
- `corne_trackpoint_right-nice_nano-standalone.uf2` (standalone, with USB logging)

### Next experiment suggestion (Exp24)

Start the dongle debugging from first principles: build a minimal XIAO BLE firmware that just blinks an LED and outputs to USB serial. Verify the XIAO's USB connection is stable. Then add ZMK split/pointing features one at a time.
