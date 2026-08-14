# Exp60: RawAccel-style Power curve baked into the Pro Mini (custom flat-top cap fix)

## Context

After switching to a custom flat-top cap, the trackpoint reports physically-larger
deltas for the same finger force — the cursor is "fast all the time" and fine
(sub-pixel-ish) movement is impossible. `speed-scale` was already dropped to 10
(~0.04×) as a blunt workaround.

## Hypothesis

A velocity-based gain curve, matching RawAccel's **Power** acceleration style, can
be baked into the Pro Mini firmware (between PS/2 read and I2C burst) so that:

- slow nudge → gain < 1 → fine/precise cursor crawl
- fast flick → gain > 1 → full (or accelerated) cursor speed

This fixes the flat-top cap exactly where the extra sensitivity lives (the raw
PS/2 deltas) and needs zero OS-side changes. ZMK input-processors can't do
nonlinear curves (`zip_xy_scaler` is a fixed multiplier), so the curve belongs in
the Pro Mini — which also honors the "all dev in the .ino" rule.

## The math (RawAccel Power, "whole" mode)

From the official RawAccel guide (`doc/Guide.md`):

```
v          = |(x,y)| = sqrt(x² + y²)          // L2 magnitude (whole mode)
f(v)       = start + (rate * v)^exponent      // Power sensitivity function
(out)      = (x * sens, y * sens) * f(v)      // applied to the whole vector
```

- `start` (output offset) = gain at v=0 → the precision knob. start 0.3 = slow
  deltas get 30% of sens.
- `rate` × `exponent` shape the rise; `f(v)` crosses 1 at the "comfortable" speed
  and accelerates past it.
- RawAccel defaults to **no input-speed EMA** (coalescion is optional/off) → we
  feed the raw per-sample magnitude, no smoothing.

## Implementation

All the curve logic lives in a new Arduino library:

```
promini-trackpoint/libraries/PowerCurve/
├── PowerCurve.h      # class: setSens / setParam / apply
└── PowerCurve.cpp    # LUT build (float pow at boot/param-write) + apply
```

- `gain_lut[64]` is a **Q8.8** fixed-point table `lut[v] = sens * f(v) * 256`,
  built once at boot / on every param write (float `pow()` only runs here, never
  in the 50Hz path).
- Per 50Hz sample: `v = isqrt(x²+y²)` → `g = lut[v]` → `out = x*g/256` with
  `rem_x/rem_y` fractional accumulation (kept from the old speed_scale path) →
  clamp to int8 (±127) for the existing 2-byte burst.
- The `.ino` keeps ~5 lines of glue; all existing logic (MOT, sleep, DEADBAND,
  MAX_DELTA) is untouched.

### Register protocol (extended, 2-byte LE — `wake_count` precedent)

| Reg | Param | Size | Notes |
|-----|-------|------|-------|
| 0x11 | sens (speed-scale) | 1 byte | existing; now folded into the LUT |
| 0x13 | rate         | 2 byte LE | Q8.8 (18 = 0.070) |
| 0x15 | exponent     | 2 byte LE | Q8.8 (256 = 1.0) |
| 0x17 | start        | 2 byte LE | Q8.8 (77 = 0.30) |

Driver writes all four at init (mirrors the existing `speed_scale` write); LUT is
rebuilt on every write (cheap, happens before any motion). Tuning changes only
need a **ZMK rebuild**, not a Pro Mini reflash.

## Changes by repo

| Repo | Change |
|------|--------|
| promini-trackpoint | new `libraries/PowerCurve/`; `trackpoint-i2c-slave.ino` uses it (register dispatch + `curve.apply`) |
| zmk-pmw3610-driver | `trackpoint-i2c.c`: DT props `curve-rate`, `curve-exponent`, `curve-start` → 2-byte writes at init (stated exception, Exp29/48 precedent); binding yaml props |
| zmk-config-dabaseV_0-2 | `dabase_v2_right.overlay`: 3 props on `trackball@42`; `speed-scale` 10 → 102 (curve provides low-speed precision so sens can return for high-speed) |

## Starting tuning

```
speed-scale = <102>     // 0.40 ×  — sens multiplier
curve-rate  = <18>      // 0.070   — f(v)=1 at v≈10 counts/20ms
curve-exponent = <256>  // 1.0     — power
curve-start = <77>      // 0.30    — slow gain = 0.30*102/256 ≈ 0.12
```

Slow (v=0): ~0.12× gain → fine crawl. v=10: 0.4× (normal). Fast (v=30): 0.07*30=2.1 → 0.4*2.1 = 0.84×… f(v)≈2.4 by v=32 → 0.96×. Aggressive starts can be pushed with higher rate/exp.

## Known offsets (curve-only scope)

- The 2× double-read (non-destructive I2C burst + 10ms driver poll) stays — it's a
  constant multiplier `speed_scale` already compensates for; curve shape unaffected.
- 50Hz sampling stays (RawAccel's units are counts/20ms here).
- Scroll/volume layers (5/7) inherit the curve since they consume the transformed
  deltas — consistent behavior.
- int8 burst saturates at very aggressive gain → clamped (driver accumulates).

## Verification plan

1. Build promini hex (GH Actions, `pro-mini-i2c-slave` artifact) + ZMK uf2
   (driver+shield). Flash via CH340G `stk500v1` + `flash-nicenano.ps1`.
2. Slow touch → cursor crawls (~1px at a time), light taps don't jump.
3. Fast flick → accelerates with full speed; no runaway at cap.
4. Tune DT props → rebuild ZMK only → reflash NiceNano → retest.
5. Confirm scroll (J) / volume (K) still work off the curve.

## Conclusion

**Success.** The RawAccel-style Power curve is live on the Pro Mini and the
NiceNano right half, and the user confirmed it finally changes speed.

- Pro Mini flashed + verified via CH340G `stk500v1` (6500 B — up from 4512 B,
  the `pow()` LUT cost; 8KB headroom is fine on 32KB).
- NiceNano flashed via `flash-nicenano.ps1` (COM7); `i2c scan i2c@40003000`
  finds 0x42 → Pro Mini powered + link good.
- **Root cause found & fixed** (see below): the params were written once at init,
  but the power-gated Pro Mini boots after init and is power-cycled on every
  deep-sleep wake — so `speed-scale`/curve writes were being lost. The fix makes
  the driver re-apply params on the first successful poll AND on every
  link-restore. This also explains why all earlier `speed-scale` tuning
  (Exp53/54/55: 204→102→51→10) appeared to do nothing.
- **Final tuning** (user-chosen): `speed-scale 128` = "normal speed"
  (~0.5× at v=10, crawl at rest, ~1.2× on fast flicks).
- Curve params (0x11 sens, 0x13 rate, 0x15 exp, 0x17 start) are write-only from
  the shell (requestEvent only serves burst 0x12 + wake_count 0x01) — verified
  behaviorally (speed change confirmed by the user).

### Root cause: params written once at init, lost on Pro Mini reboot

The trackpoint driver wrote speed/curve params exactly once, at init. But the
Pro Mini is power-gated by `EXT_POWER` (P0.06/P0.08) with `init-delay-ms=500`
(Exp48/49/50): at NiceNano boot the Pro Mini isn't on the bus yet (init writes
NACK), and on every deep-sleep wake it's power-cycled and reboots fresh
(`PowerCurve::begin()` → sens=255 identity). So the cursor always ran at the
default 1:1 regardless of DT values.

Fix (`trackpoint-i2c.c`): re-apply params when the link is (re)established —
`if (!params_written || was_lost)` in the poll success path, using a
`was_lost = consecutive_errors > 0` snapshot taken before reset. Kept the init
write as a fast path.

### The 2× double-read note

The non-destructive I2C burst + 10ms poll means the driver accumulates each 50Hz
sample ~2×. This is a constant multiplier `speed_scale` already compensates for;
curve shape unaffected. Deferred (curve-only scope).

Implementation notes:
- All curve logic lives in the new `PowerCurve` library; the `.ino` keeps ~5
  lines of glue (`curve.apply`, `curve.update`, register dispatch).
- Params mark the LUT dirty from the ISR; the 64-entry float `pow()` rebuild
  runs in `loop()` (`curve.update()`) — never in the I2C ISR.
- Library compiled clean with avr-g++ 12.2 `-Wall -Wextra`, linked, no
  undefined symbols before flashing.

Known-good:
- promini-trackpoint `8e345cb`
- zmk-pmw3610-driver `767a5dd284db0e5a8c7a9b8a0d01f6a9a2af0b24` (pinned in west.yml)
- zmk-config-dabaseV_0-2 `4b70d66` (speed-scale 128)
- CI: promini `31587702644` success; config `31594902525` (param re-apply) + `31595865991` (speed 128) success
- artifacts: `build/Exp60/promini/pro-mini-i2c-slave/` + `build/Exp60/zmk/firmware/dabase_v2_right-promini-i2c.uf2`

## Next steps

- Speed is a single knob: `speed-scale` only (ZMK rebuild + NiceNano reflash).
- If slow-speed precision still needs work, tune `curve-start` (lower = more
  attenuation at rest); fast-speed feel is `curve-rate`/`curve-exponent`.
- Follow-ups: the Exp43 destructive-read fix + 100Hz PS/2 sampling were
  deliberately deferred (curve-only scope) and would give the curve a cleaner
  tuning space (and kill the 2× factor) if needed later.