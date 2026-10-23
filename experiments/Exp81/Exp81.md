# Exp81: trackpoint rail power policy — idle cut + sleep pin isolation

## Why (measured, not guessed)

`zmk-battery-center` history (15 Sep – 23 Oct 2026, 458 k rows) on the
`dabase_v2` dongle stack:

| Observation | Right (Peripheral 1) | Left (Peripheral 0) |
|---|---|---|
| Clean 24 h block, 17–18 Oct | **12.8 %/day** | 2.6 %/day |
| Daytime, in use | 0.6–1.4 %/h | ~0.1–0.2 %/h |
| **Asleep** overnight (~12 h, half unreachable, dongle alive) | **~4.4 %** | ~0.9 % |

Both halves reach deep sleep (both report 0 / unreachable overnight while the
Central keeps reporting), so Exp76's "right never sleeps" does not reproduce on
the current stack. The right half still costs ~5× the left, split roughly:

- **~6 points/day: the trackpoint is powered the whole time the half is awake.**
  `EXT_VCC` comes up at boot and stays up for 1.5 h after the last activity.
- **~4 points/day: leak while asleep.** With the rail off, the internal PS/2
  pull-ups on P0.06/P0.08 push current through the module's ESD diodes into its
  unpowered VCC rail (~0.25 mA, ~40× the nRF System OFF current).
- ~2.5 points/day: normal nRF/BLE baseline (the left half).

Logging/shell was ruled out: the left half's rate is flat across the 14 Oct
logging removal, and the right's 20 Oct re-enable shows no step.

## Hypothesis

Binding the switchable rail to ZMK's own activity state removes both the awake
surplus and the sleep leak, without touching the driver's decode path:

- rail on only while the keyboard is "active" (any key/encoder/pointer event)
- rail off at the idle state (`CONFIG_ZMK_IDLE_TIMEOUT`, 60 s here)
- CLK/DAT disconnected while the rail is off — the pin state survives into
  System OFF, so deep sleep inherits the fix
- next activity edge brings the rail back, with an 800 ms mute covering the
  module's 600 ms ±20 % power-on and its 1–2 garbage first packets (Exp06)

## Policy

```
boot / key press ──► rail ON, 800 ms mute, PS/2 callback armed
key / pointer / encoder ──► activity timer reset
60 s with none of those ──► rail OFF, callback disarmed, assembler cleared,
                            CLK/DAT = GPIO_DISCONNECTED
key press while off ──► rail ON, 800 ms mute, timer reset
1 h with no activity ──► System OFF (rail already low, pins already isolated)
```

Consequence by design: once the rail is off the trackpoint is blind, so the
**next key** brings it back, not the nub. The first nudge after a long pause is
lost.

## Implementation

| Repo | Change |
|---|---|
| `zmk-ps2-trackpoint-driver` (Exp81 `ddbe5bb`) | New `src/power/tp_power_policy.c`: `ZMK_LISTENER` on `zmk_activity_state_changed`, drives the `EXT_POWER` `control-gpios` directly, disconnects/restores CLK/DAT, arms/disarms the PS/2 callback, boot mute via `k_work_delayable`. New `zmk_mouse_ps2_power_suspend/resume()` clear the 3-byte assembler around a rail cycle. Kconfig `ZMK_PS2_TP_POWER_POLICY` + `ZMK_PS2_TP_POWER_BOOT_MUTE_MS`. |
| `zmk-config-dabaseV_0-2` (Exp81 `ffa27e3`) | `dabase_v2_right.conf`: policy on, mute 800 ms. `build.yaml` right-half: `CONFIG_ZMK_IDLE_TIMEOUT=60000`, `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000` (1 h). Left half untouched at 1.5 h — control. `west.yml` pins the driver to `ddbe5bb`. |

Two deliberate avoidances:

- **`ext_power_disable()` is not used.** It schedules `settings_save_one()` on
  every call, so a 60 s idle cycle would write the settings partition twice a
  minute and can persist `ext_power = off` across a reboot. The policy drives
  the GPIO directly; the ext_power driver keeps its status at "on", so
  `&ext_power` must not be used on this build.
- **The policy needs `NO_HOST_COMMANDS`.** In command mode
  `reporting_disable()` would write `0xF5`, which this trackpoint rejects.

## Build

Config repo run: see Findings. `dabase_v2_right-ps2-direct` keeps the
`zmk-usb-logging` snippet for bring-up (state transitions are `LOG_INF`); strip
it in a follow-up commit before the battery measurement.

## Bench checks

1. After 60 s idle: `EXT_VCC` (TP VCC pad) = 0 V, CLK/DAT idle (disconnected).
2. Key press → rail back, pointer works within ~1 s, no jump/drift, no phantom
   clicks (watch the Exp80 decode logs for `drops`/resync).
3. Deep sleep at 1 h: half unreachable, key wakes, pointer resumes.
4. Long-run feel: is losing the first nudge after a pause acceptable at 60 s?

## Measurement

- One build, both effects: nights should fall from ~4.4 % to ~1 % (leak gone),
  days from ~12.8 %/day toward the left's ~2.6 %/day (rail cut).
- Read the app history medians over ≥3 nights and ≥2 workdays. Ignore the top
  15–20 % after a charge (LiPo surface charge) and 0 samples (asleep).
- The app cannot resolve 1 %/day changes; use a µA meter for that.

## Risks / open points

- **Wake source is the right half only.** ZMK's per-half activity state means a
  key on the *left* half does not re-arm the right half's rail. If that bites,
  the follow-up is a split run-behavior from the dongle on any key.
- Power-cycling the module risks the drift seen in Exp19 (AVR era); the boot
  mute + Exp80 resync should absorb the stream restart, but watch for creep.
- `ZMK_IDLE_TIMEOUT` is shared with anything else listening to idle (nothing in
  this keymap).
- Rail cut also happens on USB builds if the policy is enabled there; only the
  ps2-direct battery build has it for now.

## Findings

(to fill after flash + on-device verification)

## Conclusion

(to fill)

## Next

- If the day number disappoints: pointer-only idle (Y) instead of any-activity,
  accepting flap, or a presence sensor on the stick.
- Cross-half wake via the dongle (split run-behavior) if the right-half-only
  wake source proves annoying.
