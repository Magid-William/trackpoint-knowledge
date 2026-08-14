# Exp01: ZMK Corne Trackpoint Shield (Phase 1)

## Hypothesis

A ZMK shield with the PMW3610 driver wired to the correct SPI pins on the NiceNano V2 will build successfully via GitHub Actions. Even though no physical PMW3610 is connected (the Pro Mini emulator isn't ready yet), the firmware will flash, serial shell will respond, and we'll see the expected PMW3610 init failure in logs — confirming the build system, pin configuration, and driver integration are correct.

## Plan

Create a full ZMK split shield (`corne_trackpoint`) with:

1. **Matrix:** 4 rows (3 main + 1 thumb) x 6 cols per half (48 keys total), right half only built
2. **SPI + PMW3610 driver:** Custom pinctrl to route SPI to non-standard pins:
   - SCK: P0.08, MOSI: P0.17, MISO: P0.06, CS: P0.20, IRQ: P0.10
3. **USB logging + serial shell** for debugging
4. **2-layer keymap:** QWERTY + LOWER

## Files Created

| File | Purpose |
|------|---------|
| `zephyr/module.yml` | Registers shield directory with Zephyr build system |
| `config/west.yml` | West manifest pulling ZMK + Maged-William/zmk-pmw3610-driver |
| `build.yaml` | CI target: nice_nano_v2 + corne_trackpoint_right |
| `boards/shields/corne_trackpoint/Kconfig.shield` | Defines left/right shield identifiers |
| `boards/shields/corne_trackpoint/Kconfig.defconfig` | KB name, split config, central=right |
| `boards/shields/corne_trackpoint/corne_trackpoint_right.overlay` | Kscan + SPI pinctrl + PMW3610 + matrix transform |
| `boards/shields/corne_trackpoint/corne_trackpoint_right.conf` | Enables SPI, INPUT, POINTING, PMW3610_ALT, SHELL, USB_LOGGING |
| `boards/shields/corne_trackpoint/corne_trackpoint_right.keymap` | QWERTY (layer 0) + LOWER (layer 1) |
| `boards/shields/corne_trackpoint/corne_trackpoint.zmk.yml` | Metadata for ZMK studio / tooling |

## Wiring Reference (for verification, not changed in this exp)

### NiceNano → Pro Mini (SPI)

| NiceNano | Pro Mini | Function |
|----------|----------|----------|
| P0.06 | D12 | MISO |
| P0.08 | D13 | SCK |
| P0.17 | D11 | MOSI |
| P0.20 | D10 | CS |
| P0.10 | D2 | IRQ/MOT |

### Matrix (NiceNano → Keyboard switches) — FINAL

| Role | nRF52 GPIO |
|------|------------|
| Col 0 | P0.02 |
| Col 1 | P0.29 |
| Col 2 | P0.09 |
| Col 3 | P1.00 |
| Col 4 | P1.04 |
| Col 5 | P1.11 |
| Row 0 | P0.31 |
| Row 1 | P1.15 |
| Row 2 | P0.24 |
| Row 3 | P1.06 |

## Build & Test

1. Push to `zmk-trackpoint-shield` branch `Exp01`
2. GitHub Actions builds via `build-user-config.yml`
3. Download UF2 artifact
4. Serial monitor → write GPREGRET + cold reboot
5. Copy UF2 to NICENANO drive
6. Verify serial shell responds

## Success Criteria

- [x] GitHub Actions build passes
- [x] UF2 artifact produced (650KB)
- [x] UF2 flashes to NiceNano (via headless GPREGRET + cold reboot)
- [x] Serial shell responds (uart:~$ prompt)
- [x] All devices READY: kscan0, spi0, trackball@0, bt_hci_controller, HID_0
- [ ] PMW3610 init failure logged (expected — no sensor yet, init may fail silently)

## Findings

1. **Board name change (ZMK main / Zephyr 4.1):** `nice_nano_v2` no longer works. Must use `nice_nano//zmk` (shorthand for `nice_nano@2.0.0//zmk` with zmk board variant).

2. **USB logging migration:** The legacy `CONFIG_ZMK_USB_LOGGING=y` Kconfig symbol is deprecated in ZMK main. Must use `snippet: zmk-usb-logging` in `build.yaml` instead. Using the old symbol caused Kconfig dependency errors (USB_CDC_ACM, UART_CONSOLE, UART_INTERRUPT_DRIVEN deps unmet).

3. **Build succeeded cleanly** on the 3rd attempt after fixing the above two issues.

4. **Bootloader entry broken in Zephyr 4.1:** `kernel reboot bootloader` command was removed. Instead, manually write `0x57` to GPREGRET at `0x4000051C` (`devmem 0x4000051C 32 0x00000057`), then `kernel reboot cold`. This sets the magic value the Adafruit nRF52 bootloader checks on reset to enter DFU mode.

5. **Headless flashing pipeline proven:** A PowerShell script (`flash-nicenano.ps1`) automates: serial bootloader entry → poll NICENANO drive → copy UF2 → verify reboot. NICENANO drive appears ~4s after cold reboot.

6. **Firmware flashed successfully:** New UF2 copied to NiceNano, device rebooted, shell confirmed alive at `uart:~$` with fresh uptime. `device list` shows all devices READY including kscan0 (4-row matrix), spi0, and trackball@0.

7. **Matrix updated mid-experiment:** Changed from 3-row `&pro_micro` pins to 4-row raw `&gpio` pins per user's actual hardware wiring.

## Conclusion

Exp01 succeeded. The ZMK shield provides:
- Working serial shell for debugging
- 4-row × 6-col keyboard matrix via raw GPIO pins (ready for key testing)
- SPI0 configured on custom pins (P0.06/P0.08/P0.17/P0.20)
- PMW3610 driver integrated and device registered (init will fail without Pro Mini emulator)
- USB logging via zmk-usb-logging snippet
- Headless flashing pipeline via GPREGRET trick + Get-CimInstance drive detection

## Assistant Notes

- AGENTS.md updated with corrected bootloader entry and USB logging methods.
- `flash-nicenano.ps1` saved locally for headless flashing.
- ZMK driver Product ID check at register 0x00 will need bypassing in Exp02 for Pro Mini emulator to pass init.

## Next Experiment

Exp02 will build the Pro Mini SPI slave firmware (PMW3610 emulator) and modify the PMW3610 driver to skip the Product ID check.
