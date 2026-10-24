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
USB attached ──► rail ON and stays on (VBUS overrides the idle cut)
USB removed ──► fall back to the activity state (immediate if already idle)
1 h with no activity ──► System OFF (rail already low, pins already isolated)
```

USB is detected with `zmk_usb_is_powered()` / `zmk_usb_conn_state_changed`
(VBUS, not HID-ready — the peripheral build has no HID endpoint). ZMK's
`activity.c` already refuses deep sleep on USB for the same reason, so the
policy mirrors existing behaviour: unlimited power, no idle budget.

Consequence by design: once the rail is off the trackpoint is blind, so the
**next key** brings it back, not the nub. The first nudge after a long pause is
lost.

## Implementation

| Repo | Change |
|---|---|
| `zmk-ps2-trackpoint-driver` (Exp81 `797205a`) | New `src/power/tp_power_policy.c`: `ZMK_LISTENER` on `zmk_activity_state_changed` + `zmk_usb_conn_state_changed`, drives the `EXT_POWER` `control-gpios` directly, disconnects/restores CLK/DAT, arms/disarms the PS/2 callback, boot mute via `k_work_delayable`. New `zmk_mouse_ps2_power_suspend/resume()` clear the 3-byte assembler around a rail cycle. Kconfig `ZMK_PS2_TP_POWER_POLICY` + `ZMK_PS2_TP_POWER_BOOT_MUTE_MS`. |
| `zmk-config-dabaseV_0-2` (Exp81 `6a7271d`) | `dabase_v2_right.conf`: policy on, mute 800 ms. `build.yaml` right-half: `CONFIG_ZMK_IDLE_TIMEOUT=60000`, `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000` (1 h). Left half untouched at 1.5 h — control. `west.yml` pins the driver. |

Two deliberate avoidances:

- **`ext_power_disable()` is not used.** It schedules `settings_save_one()` on
  every call, so a 60 s idle cycle would write the settings partition twice a
  minute and can persist `ext_power = off` across a reboot. The policy drives
  the GPIO directly; the ext_power driver keeps its status at "on", so
  `&ext_power` must not be used on this build.
- **The policy needs `NO_HOST_COMMANDS`.** In command mode
  `reporting_disable()` would write `0xF5`, which this trackpoint rejects.

## Build

| Run | What | Result |
|---|---|---|
| `40004581101` | base policy (idle cut + pin isolation) | 8/8 green, 638,976 B, flashed, **user-confirmed** |
| `40089304349` | + USB override, guard on `CONFIG_ZMK_USB` | 8/8 green but **byte-identical** UF2 (guard compiled out) |
| `40089700537` | + USB override, guard on `CONFIG_USB_DEVICE_STACK` | 8/8 green, 638,976 B, different hash, **flashed** |

Verified in run 1's Kconfig:

```
CONFIG_ZMK_IDLE_TIMEOUT=60000
CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000
CONFIG_ZMK_PS2_TP_POWER_POLICY=y
CONFIG_ZMK_PS2_TP_POWER_BOOT_MUTE_MS=800
CONFIG_USB_DEVICE_STACK=y
# CONFIG_ZMK_USB is not set        <- split peripheral
```

The USB-override build was flashed at 24 Oct; the board was unplugged before
the >60 s USB-idle check could be read back, so the override itself is
**pending on-device confirmation** (expected: rail stays high while VBUS is
present; unplug while idle -> rail off immediately).

The `dabase_v2_right-ps2-direct` build keeps the `zmk-usb-logging` snippet for
bring-up (state transitions are `LOG_INF`); strip it in a follow-up commit
before the battery measurement.

## Bench checks

1. After 60 s idle: `EXT_VCC` (TP VCC pad) = 0 V, CLK/DAT idle (disconnected).
2. Key press → rail back, pointer works within ~1 s, no jump/drift, no phantom
   clicks (watch the Exp80 decode logs for `drops`/resync).
3. Deep sleep at 1 h: half unreachable, key wakes, pointer resumes.
4. USB override (second build): with VBUS present, > 60 s idle must **not** cut
   the rail (`devmem 0x50000504` bit 13 stays set, no `TP power: off` line);
   unplug while idle → rail off immediately.
5. Long-run feel: is losing the first nudge after a pause acceptable at 60 s?

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

- **Rail cut/restore works on hardware** (user, one day of use): DC voltage on
  the rail drops at idle and returns on any key, pointer resumes, no jumps
  reported.
- **This trackpoint does not stream at idle.** With the rail on and the board
  up for 80 s untouched, `ps2_gpio: int=0` / `bytes=0`, then a single edge
  (`int=1 abort=1`). The earlier "CLK-edge ISR stream costs power" hypothesis is
  dead: the awake cost is the powered module itself, not the interrupt rate.
- **`CONFIG_ZMK_USB` is OFF on a split peripheral** (`depends on !ZMK_SPLIT ||
  ZMK_SPLIT_ROLE_CENTRAL`). The first USB-override build was **byte-identical**
  to the pre-USB build because the guard compiled the path out; caught by
  comparing UF2 hashes. The VBUS state lives in `app/src/usb.c`, built under
  `CONFIG_USB_DEVICE_STACK` (on, via the CDC console), so the guard must be
  `CONFIG_USB_DEVICE_STACK`, not `CONFIG_ZMK_USB`.
- **USB override confirmed** (user, with and without the cable): rail stays on
  while VBUS is present, unplug falls back to the activity state, replug brings
  it back.
- Battery numbers not measured yet (logging snippet still on this build).

## Conclusion

**Success — on both power paths, user-confirmed.**

- **Battery:** the rail cuts after 60 s without a key or a nudge (verified with
  a DC voltmeter at the trackpoint rail) and any right-half key brings it back
  with a usable pointer. No jumps or drift reported in a day of use.
- **USB:** with VBUS present the rail stays on regardless of the idle timer;
  unplugging falls back to the activity state. Tested with and without the
  cable.
- **Sleep:** the pin-isolation half of the policy (rail low, CLK/DAT
  `GPIO_DISCONNECTED`) runs on both the idle and the sleep transition, and that
  pin state survives into System OFF — so the ~4 %/night pull-up leak the
  battery history exposed is addressed by the same routine.

What the experiment cost: one small module (`tp_power_policy.c`), two hooks in
the input driver, no changes to the decode path, and one retracted hypothesis
(the trackpoint does not stream at idle, so the CLK-interrupt rate was never a
cost).

What it does **not** prove: the battery gain. The logging snippet is still on
this build and nobody has measured a night or a workday since the policy went
in. The behavioural evidence is solid; the energy evidence is still pending.

Known, accepted limits: the wake source is the right half's own keys (left-half
typing does not re-arm it), and the first nudge after a pause is lost by
design.

## Next

- **Measure it:** strip `zmk-usb-logging` from `dabase_v2_right-ps2-direct`,
  reflash, then 3 nights + 2 workdays of battery-center history. Targets: night
  ~4.4 % → ~1 %, day ~12.8 % → ≤5 %/day.
- Cross-half wake via the dongle (split run-behavior) if the right-half-only
  wake source proves annoying in daily use.
- If the day number disappoints: pointer-only idle (Y) instead of any-activity,
  accepting flap, or a presence sensor on the stick.
