# Exp49: ZMK deep sleep auto-cuts Pro Mini power (P0.06/P0.08), keys-only wake

## Hypothesis

`CONFIG_ZMK_SLEEP=y` puts the NiceNano into deep sleep (`sys_poweroff`) after an idle timeout. Zephyr PM suspend runs `ext_power_generic_pm_action(PM_DEVICE_ACTION_SUSPEND)` which calls `ext_power_generic_disable()` → clears **all** control pins → P0.08 (PNP high-side) + P0.06 (NPN low-side) both inactive → **Pro Mini + trackpoint fully unpowered during ZMK sleep**. This automates the Exp48 manual DOT-cut without any Pro Mini or driver changes.

Wake is **keys-only**: `kscan0` already has `wakeup-source` (`corne_trackpoint_right.overlay:70`), so any keyboard key pulls the nRF out of System OFF. The trackpoint cannot wake the board — it has no irq-gpios in the 4-wire wiring, no PM device, and is unpowered during sleep. On wake the NiceNano reboots, ext_power re-enables the pins, the Pro Mini boots (~1–2s), and the driver's I2C backoff recovers — the pointer resumes.

## Changes by repo

| Repo | Branch | Change |
|------|--------|--------|
| promini-trackpoint | — | **NO changes** (`0a8af67` stays) |
| zmk-pmw3610-driver | — | **NO changes** (`6f84b62` stays) |
| zmk-trackpoint-shield | Exp49 | Poweron build gets `CONFIG_ZMK_SLEEP=y -DCONFIG_ZMK_IDLE_SLEEP_TIMEOUT=120000` via cmake-args in `build.yaml` |

Built-in chain, verified in pinned ZMK `fa33e35f` / Zephyr v4.1+zmk-fixes:
- `CONFIG_ZMK_SLEEP` selects `POWEROFF` and `ZMK_PM_DEVICE_SUSPEND_RESUME`
- `PM_DEVICE` defaults `y` (ZMK forces it under `ZMK_SLEEP`)
- `PM_DEVICE_SYSTEM_MANAGED` defaults `y` when `!PM_DEVICE_RUNTIME` → `zmk_pm_suspend_devices()` iterates devices and calls `PM_DEVICE_ACTION_SUSPEND` on all non-wakeup, non-busy devices
- `ext_power_generic.c` implements the PM action → `disable()` during suspend
- `activity.c` `activity_work_handler`: inactive > `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` AND no USB power → suspend devices → `sys_poweroff()`

## Key constraints

1. **USB power blocks sleep**: `is_usb_power_present()` in `activity.c` gates deep sleep. With USB (or `zmk-usb-logging`) connected the board never deep-sleeps — **test on battery**.
2. Safe to couple with the `zmk-usb-logging` snippet build mandatory; while plugged, behavior identical to Exp48.
3. ext_power settings: suspend→`disable()` calls `ext_power_save_state()`, but that work is debounced 60s (`CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`) and `sys_poweroff()` follows immediately — the save never fires, so the persisted state stays "on" and the Pro Mini powers back up after wake.

## Verification

1. Build via GH Actions (`Exp49` branch), download hex, flash with `flash-nicenano.ps1`. — run 31046370395, all jobs green.
2. Confirm pointer works and `i2c scan i2c0` → `0x42`. — **Confirmed**: 0x42 found, cursor moves.
3. **Unplug USB** → idle for the ~2min timeout → expect: Pro Mini dead (LED off, `i2c scan` → 0 devices, draw ≈ near-zero). — **Confirmed**: Pro Mini dies on ZMK deep sleep.
4. Touch the trackpoint → **must NOT wake** (board stays asleep). — **Confirmed**: trackpoint is not a wake source.
5. Press any key → wakes: ext_power pins re-asserted → Pro Mini boots ~1–2s → trackpoint responsive and pointer moves. — **Confirmed**: key wakes the board; Pro Mini back up in ~1s, pointer works.
6. Cycle sleep/wake several times; watch for freezing (Exp34/Exp36 concern) and confirm settings still report ext-power on. — **No lockups** across cycles.

## Conclusion

**Success.** `CONFIG_ZMK_SLEEP=y` (120s timeout on the poweron build) drives the full Exp48 power-gate mechanism automatically: ZMK deep sleep → PM suspend → ext_power disable → P0.06/P0.08 inactive → Pro Mini + trackpoint fully unpowered. Wake is keys-only via kscan0 `wakeup-source`; touching the trackpoint does not wake the board. On key wake, ext_power re-asserts the pins and the Pro Mini is operational in ~1s (measured), then the pointer resumes. No lockups. The wishlist item "auto-cut on inactivity" is now done — zero Pro Mini or driver changes, two cmake-args on the poweron build.

## Known-good pins

- shield `Exp49` (this branch)
- driver `6f84b62`
- promini `0a8af67`
- ZMK `fa33e35f`

## Next steps

1. Bump `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` to the final value (default 900000ms) for production use; the 120s test value is intentional for this experiment.
2. Dongle topology: repeat with the dongle + peripheral pair — wake key originates on the other half (keyboard), so confirm the trackpoint half still sleeps/wakes correctly.