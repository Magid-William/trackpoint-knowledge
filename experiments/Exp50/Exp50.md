# Exp50: Port Exp49 ZMK sleep/wake to the dabase_v2 right shield (dongle topology) + match left half

## Hypothesis

The Exp49 mechanism (`CONFIG_ZMK_SLEEP=y` → PM suspend → `ext_power` disable → P0.06/P0.08 gates cut Pro Mini power; keys-only wake via kscan0 `wakeup-source`) is board-agnostic. The dabase_v2 right half uses the same `nice_nano//zmk` board and the same pinned ZMK (`fa33e35f`) as the corne Exp49 build, so replicating the Exp48 overlay changes + Exp49 cmake-args should give the dabase the same sleep/wake behavior — no `&ext_power EP_TOG`, just automatic sleep and wake.

This is also the **first sleep test in split-peripheral topology** (dongle = central, right half = peripheral) — the untested next step from Exp49.

## Changes by repo

| Repo | Branch | Change |
|------|--------|--------|
| promini-trackpoint | — | **NO changes** (`0a8af67`) |
| zmk-pmw3610-driver | — | **NO changes** (`6f84b62`) |
| zmk-config-dabaseV_0-2 | Exp50 | 1) `west.yml`: driver pin `58fd284` → `6f84b62` (Exp48 — irq/reset optional; `58fd284` requires them) 2) `dabase_v2_right.overlay`: drop `irq-gpios`/`reset-gpios` from trackball (frees P0.06/P0.08), add root `EXT_POWER` override (P0.08 PNP ACTIVE_LOW, P0.06 NPN ACTIVE_HIGH, P0.13 rail ACTIVE_HIGH, init-delay 500ms) 3) `build.yaml`: promini-i2c build gets sleep cmake-args, left build matched 4) **no keymap changes** (no EP_TOG) |

Driver pin bump is required: `58fd284` (Exp36) declares `irq-gpios`/`reset-gpios` as `required: true`, so the overlay wouldn't compile without them.

### Branch history

| Commit | Change |
|--------|--------|
| `d4ec751` | LOG_DEFAULT_LEVEL=4 + LOG_CMDS (diag build, later dropped) |
| `28a71d4` | idle sleep timeout 120s → 30s (fast battery testing) |
| `7832dee` + `fef0c75` | `zmk_soft_off_wakeup_sources` node attempt (soft-off detour — **dropped**) |
| `7c182f8` | **Success commit**: timeout 30s → 15min (900s), drop softoff diag build + wakeup-sources node |
| `093a723` → `dfe76e2` → `442b701` | left half matched: sleep args → 30s test build → verified → 15min final |

## Key constraints

1. **USB blocks sleep**: `is_usb_power_present()` gates deep sleep — test on battery only. USB-attached tests can never validate deep sleep.
2. **Soft-off ≠ deep sleep**: `zmk_pm_soft_off()` disables ALL wakeups first (bag-safe by design) — its "no key wake" is expected, not a bug. Testing soft-off as a deep-sleep proxy was the main methodological error of this experiment.
3. No MOT/RST — driver logs "no irq-gpios — MOT gating disabled, plain polling", "skipping reset pulse" (Exp47 path).

## Verification (battery, dongle untouched, no USB)

Right half (`dabase_v2_right-promini-i2c`, 30s test timeout):

1. Awake baseline: cursor moves when touching trackpoint. ✅
2. Idle ~40s (30s timeout + activity tick granularity) → **deep sleep engages**: Pro Mini power cut (ext_power suspend → P0.06/P0.08), BLE drops. ✅ — this was the original "never engages" mystery; it works.
3. Touch trackpoint during sleep → **does NOT wake** (keys-only). ✅
4. **Any key wakes it**; ext_power re-asserts, Pro Mini boots in ~1s, pointer resumes via dongle. ✅
5. Sleep/wake cycles stable, no lockups. ✅

Left half (`dabase_v2_left`, 30s test timeout):

1. Idle ~40s → sleeps. ✅
2. Any key wakes. ✅
3. Encoder rotation does NOT wake (keys-only). ✅

Final value: `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=900000` (15 min) — user-confirmed realistic idle, flashed on both halves.

## Wake mechanism (verified in ZMK `fa33e35f` source)

`kscan0` has `wakeup-source` → WS_CAPABLE → `zmk_physical_layouts_init` (`app/src/physical_layouts.c`) calls `pm_device_wakeup_enable(kscan, true)` at boot → deep sleep's `zmk_pm_suspend_devices()` (`app/src/pm.c`) **skips** the wakeup-enabled kscan → matrix pins stay configured → key press = GPIO DETECT = wake from System OFF. Same mechanism on both halves via the shared `dabase_v2.dtsi` kscan0.

## Conclusion

**Success.** Deep sleep with keys-only wake works on both halves in the dongle topology:

- Right half: 40s idle → sleep (Pro Mini fully unpowered), trackpoint touch does NOT wake, any key wakes, pointer resumes in ~1s.
- Left half: identical, encoder excluded from wake.
- Both halves finalized at a 15-minute idle timeout, matching the user's real usage.
- Exp49's open item (dongle topology) is closed.

Notable lessons:

- **Battery-only testing is mandatory** for sleep work — USB blocks deep sleep by design.
- **Soft-off was a wrong proxy**: it intentionally kills matrix wakeup; the `zmk_soft_off_wakeup_sources` mechanism is only for soft-off, irrelevant to deep sleep.
- The artifact-verification rabbit hole (byte-identical uf2s across config-differing builds) remains unresolved but became moot once the correct target (deep sleep, not soft-off) was tested directly on battery.

## Known-good

- shield/config repo branch `Exp50` @ `442b701`
- driver `6f84b62`, promini `0a8af67`, ZMK `fa33e35f`, Zephyr `9df4b12`
- firmware: `dabase_v2_right-promini-i2c.uf2` (543744 B, run 31105051891) + `dabase_v2_left.uf2` (518144 B, run 31107816450) → `build/Exp50c/firmware/`

## Next steps

1. Full-day battery drain validation with the 15 min timeout (confirms the wishlist battery goal).
2. Restore `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` default or tune after drain data (current 900s = 15 min).
