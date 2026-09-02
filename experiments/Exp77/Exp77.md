# Exp77: Single-packet magnitude jump filter (random-jump suppression)

## Hypothesis

The direct-PS/2 dabase right half (gpio-ps2 decode) occasionally emits a
**random cursor jump** — a single spurious packet with an absurd per-packet
delta that slips past the bit-3 alignment / parity / stop-bit checks. All the
UART-era anti-glitch filters (MEDIAN/SLEW/EMA) are OFF on the right half
(`dabase_v2_right.conf`) because gpio-ps2 decodes cleanly, so a rare slip
passes straight through to the cursor.

Detect it simply: keep the last **accepted** per-axis delta; if a packet's
`|delta|` exceeds a threshold, treat it as a jump and **re-emit the last
accepted value** instead of the spike (cursor holds, no leap). Idle replays 0.

Known trade-off (accepted): a plain magnitude threshold also clips genuine
single fast flicks — unlike the existing `SLEW_MAX` gate, which exempts
*sustained* big deltas (real flicks). If flick-clipping shows up, fall back to
`SLEW_MAX` or combine.

## Plan

| Repo | Change |
|------|--------|
| zmk-ps2-trackpoint-driver (`Exp77` branch = `pr/optional-features` base) | New `CONFIG_ZMK_INPUT_MOUSE_PS2_DELTA_MAX` (int, 0=off, 0..255). Adds `jump_last_x/y` state + a magnitude-reject/replay filter at the **TOP** of `zmk_mouse_ps2_activity_move_mouse()` (before median/curve/divisor). |
| zmk-config-dabaseV_0-2 (`Exp77` branch) | `config/west.yml` → driver `75d4d71`; `config/dabase_v2_right.conf` → `CONFIG_ZMK_INPUT_MOUSE_PS2_DELTA_MAX=100`. |
| trackpoint-knowledge (this repo) | This file + Experiments.md row. |

Procedure: push driver branch → pin + conf in config → GH Actions build
(`gh`, wait 2.5 min then poll 30 s) → flash `dabase_v2_right-ps2-direct.uf2`
→ user verifies jumps are gone and flicks still work.

## Build

1. First attempt pinned driver `49065fb` — built on the **stale fork Exp74**
   (`3aadda2`), which lacks the `pr/optional-features` symbols the conf
   expects (`PS2_GPIO_INTERNAL_PULLUP`, `PS2_GPIO_TIMING_SCL_CYCLE_MAX`,
   `ZMK_INPUT_MOUSE_PS2_POWER_CURVE`, `ZMK_INPUT_MOUSE_PS2_TELEMETRY`) →
   Kconfig "undefined symbol" warnings → **abort**, both right-half builds.
   Lesson: the active dabase driver is the `pr/optional-features` lineage
   (local clone `kb_zmk_ps2_mouse_trackpoint_driver_pr`), pinned `b8a2200b`.
2. Rebuilt Exp77 on `b8a2200b` → clean commit `75d4d71` (3 files,
   +44). Config re-pinned to `75d4d71` (`e104fa3`). Also gitignored the stray
   `.git-rewrite/` artifact in the driver clone.
3. GH Actions run `33621827224` — **8/8 green**, artifacts uploaded incl.
   `dabase_v2_right-ps2-direct`.

Known-good: driver `75d4d71`, config Exp77 `e104fa3`.

## Findings

- Flashed `dabase_v2_right-ps2-direct-exp77.uf2` (628736 B) headless via
  COM22 (`flash-nicenano.ps1`, GPREGRET bootloader entry). Drive accepted the
  copy, device back online.
- User: jumps gone? genuine flicks still smooth? (threshold 100; tune
  `DELTA_MAX` in `dabase_v2_right.conf` — lower = more aggressive rejection,
  also more flick clipping).

## Conclusion

(pending flash + user confirmation)

## Next steps

- If flicks feel clipped: lower the threshold or switch to `SLEW_MAX`'s
  sustained-exemption logic; note the trade-off in the Kconfig help.