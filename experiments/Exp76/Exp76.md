# Exp76: Verify deep sleep — both halves at 1-min idle (direct-PS/2 era)

> Renumbered from the provisional "Exp75" — the memory repo's `Exp75` branch is
> already claimed by the driver-PR refactor work (`pr-verify-dabase` era:
> Exp68-74 fork changes as optional features on badjeff main). This sleep test
> is Exp76.

## Hypothesis

Both keyboard halves use `CONFIG_ZMK_SLEEP=y` + `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT`.
The left half (keys-only) sleeps at 15 min and wakes normally; the right half
(direct-PS2 trackpoint) either **never sleeps** or **sleeps but never wakes**
(physical reset required).

Suspected cause for the right half: the `gpio-ps2` backend samples PS/2 bits via
**GPIO interrupts on the CLK line** (GPIOTE at priority 6 — highest — in
`dabase-v2-right-ps2.dtsi`). This trackpoint *streams on power-up* and keeps
clocking, so continuous CLK-edge interrupts keep the CPU active / reset ZMK's
idle timer → the half never settles into deep sleep. When it does occasionally
suspend, that same GPIO-driven activity can leave it with no clean wake source
→ physical reset.

To make sleep quick to observe and give a clean left↔right side-by-side, drop
`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` to **1 min (60000 ms)** on **both active
halves** (left + `dabase_v2_right-ps2-direct`). `standalone-usb` (fallback
build, not flashed) stays at 15 min.

## Plan

| Repo | Change |
|------|--------|
| zmk-config-dabaseV_0-2 (Exp76 branch) | `build.yaml`: `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=900000 → 60000` on the `dabase_v2_left` and `dabase_v2_right-ps2-direct` entries only |

Procedure:
1. Push Exp75 → GH Actions build (`gh`, wait 2.5 min then poll every 30 s).
2. Flash both halves: `dabase_v2_left.uf2` + `dabase_v2_right-ps2-direct.uf2`.
3. Verify **each** half sleeps ~1 min after idle and wakes on **key press**
   (TP touch is NOT a wake source — Exp49/50 keys-only wake for the right half).
4. Diagnosis branches:
   - Right never sleeps at 1 min → gpio-ps2 CLK stream keeps ZMK active →
     investigate `zmk,input-mouse-ps2` idle handling (pause reporting on ZMK
     idle, mirroring `zmk_mouse_ps2_activity_reporting_disable`).
   - Right sleeps but won't wake (needs physical reset) → wake-source /
     suspend-transition problem → compare the Exp49/50 wake mechanism (kscan0
     `wakeup-source` → physical_layouts `WS_ENABLED`) vs gpio-ps2 GPIO state.
   - Left fails too → generalizes to a non-trackpoint cause (both halves).

## Build

GH Actions config repo run `33336453526` (push, config branch `Exp75`) — see Findings.

## Findings

(to fill in after flash + on-device verification)

## Conclusion

(to fill in)

## Next steps

- Revert to 15 min (`=900000`) once a working timeout is chosen.