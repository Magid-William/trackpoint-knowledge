# Exp11: BLE Dongle with XIAO nRF52840 Central

## Hypothesis

Using a dedicated nRF52 central dongle (XIAO BLE) connected to the PC via USB HID, with the right half NiceNano as a BLE peripheral, will eliminate the cursor lag observed when the right half connects directly to the host PC via BLE. The nRF52-to-nRF52 BLE link negotiates a faster connection interval, and USB HID from the dongle delivers reports at 1ms polling.

## Architecture

```
PC ──USB HID──→ XIAO BLE (central/dongle) ──BLE split──→ NiceNano (peripheral) ──SPI──→ Pro Mini ──PS/2──→ TrackPoint
                    │                                          │
              Runs keymap                                Scans matrix
              Processes trackball events                 Reads trackball via SPI
              zip_xy_scaler applied                      Forwards events via BLE split
```

### Key concepts

- **`zmk,input-split`** (from ZMK pointing docs): A proxy device that bridges a physical sensor on a peripheral to the central. Defined in shared `.dtsi`, overridden by peripheral with `device = <&real_sensor>`.
- **Shared `corne_trackpoint.dtsi`**: Contains matrix transform, physical layout, split_inputs/trackball_split, and a disabled trackball_listener — included by both peripheral and dongle overlays.
- **Peripheral**: Right half NiceNano. Has kscan (matrix), SPI trackball hardware, and `&trackball_split { device = <&trackball>; }` to link the physical sensor to the split transport.
- **Central (dongle)**: XIAO BLE. Has keymap, enables `&trackball_listener`, and forwards HID to PC via USB. No kscan, no SPI.

## Changes by file

### New files

| File | Purpose |
|------|---------|
| `boards/shields/corne_trackpoint/corne_trackpoint.dtsi` | Shared devicetree: matrix transform, physical layout, split_inputs, disabled listener |
| `boards/shields/trackpoint_dongle/Kconfig.shield` | Shield identifier |
| `boards/shields/trackpoint_dongle/Kconfig.defconfig` | ZMK_KEYBOARD_NAME, ZMK_SPLIT_ROLE_CENTRAL, ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=1 |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.overlay` | Includes shared dtsi, enables trackball_listener |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.keymap` | Keymap copy from right half + `&bt BT_CLR` for bond clearing |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.conf` | ZMK_POINTING, SHELL, LOG |
| `boards/shields/trackpoint_dongle/trackpoint_dongle.zmk.yml` | Metadata |
| `experiments/Exp11/Exp11.md` | This file |

### Modified files

| File | Change |
|------|--------|
| `corne_trackpoint_right.overlay` | Now `#include "corne_trackpoint.dtsi"` + peripheral-specific: kscan, SPI, trackball_split override |
| `Kconfig.defconfig` | `ZMK_SPLIT_ROLE_CENTRAL` → `ZMK_SPLIT_ROLE_PERIPHERAL` |
| `corne_trackpoint_right.keymap` | Added `&bt BT_CLR` to lower layer |
| `build.yaml` | Added `trackpoint_dongle` (xiao_ble), `settings_reset` (both boards) |
| `Experiments.md` | Added Exp11 row |

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md |
| `PS2Trackpoint/` | Used as-is |
| `promini-trackpoint/` | Pro Mini firmware unchanged |
| `corne_trackpoint_right.conf` | Same SPI/PMW3610/POINTING config needed on peripheral |

## Build targets

```yaml
- board: nice_nano//zmk          # Right half (peripheral)
  shield: corne_trackpoint_right
- board: xiao_ble//zmk           # Dongle (central)
  shield: trackpoint_dongle
- board: nice_nano//zmk          # Settings reset
  shield: settings_reset
- board: xiao_ble//zmk           # Settings reset
  shield: settings_reset
```

## Flash & Pair Procedure

1. Flash **settings_reset** to NiceNano (right half) — clears all existing bonds
2. Flash **settings_reset** to XIAO BLE (dongle) — clears all existing bonds
3. Flash **corne_trackpoint_right** to NiceNano (peripheral firmware)
4. Flash **trackpoint_dongle** to XIAO BLE (central firmware)
5. Power both — they should auto-pair (peripheral advertises, central scans)
6. Connect XIAO BLE to PC via USB — cursor should move with nub

## Success criteria

- [ ] All 4 build targets pass via GitHub Actions
- [ ] NiceNano flashes with peripheral firmware
- [ ] XIAO BLE flashes with dongle firmware
- [ ] Auto-pair between peripheral and dongle (no manual intervention)
- [ ] Cursor moves with nub via dongle USB HID
- [ ] No perceivable lag (or significantly less than direct BLE)
- [ ] BT_CLR clears bonds and allows re-pair

## Expected findings

The nRF52-to-nRF52 BLE connection should negotiate at the fastest possible interval (7.5ms). Combined with USB HID at 1ms from the dongle, this should be noticeably faster than direct BLE to a PC host (which typically negotiates 15-30ms intervals). The ZMK split transport adds ~3.75ms average latency overhead.

Total expected latency: ~7.5ms (BLE interval) + ~3.75ms (split overhead) + ~1ms (USB HID) ≈ 12ms. Compare to direct BLE at 15-30ms — a 2-3× improvement.

## Bluetooth Profile Clearing

The lower layer keymap includes `&bt BT_CLR` on the right half's first key (lower layer, row 2, col 0 equiv). Press Lower + that key to clear the active BT profile on the dongle.

To reset split pairing:
1. Flash settings_reset to both boards
2. Re-flash normal firmware
3. They auto-pair on next power-up

## Findings

### Serial diagnostics (dongle, central)

The dongle's USB CDC ACM shell revealed:

1. **Keyboard and mouse events both arrive** over BLE split transport
2. **Keyboard uses queued notifications** (`position_state_msgq` → `service_work_q`), mouse uses **direct `bt_gatt_notify()`** on the calling thread (sysworkq)
3. After ~1-2 seconds of mouse motion, both keyboard and mouse stop — no BLE disconnect, no crash logs
4. Dongle stays alive 35+ seconds (shell responsive) but receives no more events after the initial ~1-2s burst
5. Heavy log message dropping during mouse activity (`--- 83 messages dropped ---`)
6. `bt` shell command reported `command not found` even though `bt_hci_controller` is READY

### Attempted fixes

| # | Fix | Result |
|---|-----|--------|
| 1 | Increase `ZMK_SPLIT_BLE_CENTRAL_POSITION_QUEUE_SIZE` (5→20), `ZMK_SPLIT_BLE_CENTRAL_SPLIT_RUN_STACK_SIZE` (512→1024), `ZMK_BLE_MOUSE_REPORT_QUEUE_SIZE` (20→40) | Dongle lifetime ~10s → ~35s |
| 2 | Remove `zip_xy_scaler` (raw motion, no scaling) | No change — mouse still stops |
| 3 | Fork ZMK, queue input-split notifications via `input_event_msgq` + `service_work_q` | No change — issue persists |

### Root cause analysis (inconclusive)

The fork fix (attempt #3) targeted the **peripheral** — queuing `bt_gatt_notify()` so it wouldn't block sysworkq. It didn't fix the issue, suggesting the root cause is **not** a blocked work queue on the peripheral. Three remaining theories:

- **Dongle-side resource exhaustion**: Something on the XIAO BLE central accumulates over time (memory, BT buffers, or input processor state) and stops accepting split events after ~1-2s
- **Peripheral silent hang**: The NiceNano peripheral stops processing but doesn't crash (no BLE disconnect logged). JTAG/SWD needed to inspect
- **Zephyr BT stack bug**: A latent issue in the BLE split transport protocol when handling high-rate sensor data

### What would be needed to properly debug

Without JTAG/SWD access to either board, or physical UART pins on the peripheral, we can't get crash dumps, stack traces, or fault handlers. A debugger is essential for:
- Catching the exact instruction where the peripheral hangs
- Inspecting work queue and thread states at the moment of failure
- Memory/heap exhaustion tracking

## Conclusion

**Status: Failed** — the BLE dongle approach (split keyboard with sensor forwarding) cannot be made to work reliably without proper hardware debugging tools. The USB-direct pipeline (Exp10) remains the stable, proven configuration.

The `zmk,input-split` mechanism and ZMK's BLE split transport for sensor data need JTAG-level debugging to resolve the root cause. This experiment explored the queuing theory thoroughly (stack sizes, queue sizes, forking ZMK) but none of the software-only fixes resolved the issue.

## Rollback

Revert all repos to Exp10 state. Go back to right half as central with direct USB HID.

## Next steps

Once JTAG/SWD debugging capability is available (or a USB-UART adapter for the peripheral's physical UART pins), re-open this investigation.
