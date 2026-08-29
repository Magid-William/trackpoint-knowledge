# Exp74: On-device Power curve on the nice_nano (direct-PS/2) — no coprocessor

## Context

The coprocessor-era Power curve (Exp60/61: RawAccel "Power" in the
`PowerCurve` library on the Pro Mini / ATtiny85) is gone since the direct-PS/2
stack (Exp68+). The dongle overlay even noted "direct-PS2 raw deltas are larger
than the old PowerCurve'd I2C stream" — the fast-all-the-time feel the curve
fixed is back. Question: bake the curve into the nicenano, or do it at the OS
level (RawAccel on Windows)?

## Hypothesis

Porting the AVR `PowerCurve` 1:1 into `zmk,input-mouse-ps2` (the driver that
reads the direct-PS/2 trackpoint) gives the same curve feel with zero
coprocessor, is topology-independent (runs on the right half in every role —
split peripheral, standalone central, dongle), and travels with the keyboard
instead of being a per-machine OS tool (the Exp67 lesson: compensation belongs
in the keyboard, host stays neutral).

## The math (identical to the AVR library)

```
v     = |(x,y)| = sqrt(x*x + y*y)              (whole mode, L2 magnitude)
f(v)  = start + (rate * v)^exponent            (Power sensitivity function)
out   = (x * sens, y * sens) * f(v)            (applied to the whole vector)
```

Same numerics as the AVR: 64-entry Q8.8 gain LUT `lut[v] = sens*f(v)*256`
built with float `pow()` on param change, integer `isqrt16` for the magnitude,
fractional-remainder accumulation per axis, int8 clamp (±127) so HID reports
stay int8.

Params are Q8.8 DT props on `zmk,input-mouse-ps2`; `curve-rate != 0` enables
the curve and `curve-rate = 0` (default) keeps the raw stream — safe identity.

## Implementation

| Repo | Change |
|------|--------|
| zmk-ps2-trackpoint-driver (`kb_zmk_ps2_mouse_trackpoint_driver`) | new `src/drivers/input/power_curve.c/.h` (1:1 AVR port); binding YAML +4 props (`curve-sens`, `curve-rate`, `curve-exponent`, `curve-start`); `input_mouse_ps2.c` applies the curve in `zmk_mouse_ps2_activity_move_mouse()` after the Exp68 filters and before the divisor/report stage (the AVR's "between PS/2 read and I2C burst" slot); curve init from DT props at device init; README subsection 3.4.4.1 |
| zmk-config-dabaseV_0-2 | `dabase_v2_right.overlay`: `&tpoint0 { curve-sens 128; curve-rate 18; curve-exponent 256; curve-start 77; }` (Exp61 known-good); `config/west.yml` pinned to driver Exp74 `3aadda2`; dongle overlay scroll comment updated |

### Driver detail

- `power_curve.c` is a faithful port: Q8.8 LUT, integer `isqrt16` (no libm in
  the sample path — `pow()` only runs at init/param-write), fixed-point
  multiply with remainder accumulation, `OUT_MAX 127` clamp, `CLAMP_MAX` gain
  8.0, identity defaults (`rate 0`, `exp 256`, `start 256`).
- Guard added for wide int16 inputs: `mag2 > UINT16_MAX` saturates to the top
  gain bin (real PS/2 deltas are int8 so this never triggers on data).
- `have_x/have_y` are recomputed after the curve so a delta that shrinks to 0
  reports nothing (sparse reports).
- Runs in the input driver → applies in every topology, and scroll/volume/
  temp_layer on the dongle consume the shaped deltas exactly like the
  coprocessor era.

## Known-good tuning (starting point, from Exp60/61)

```
curve-sens = 128   // 0.5x  — loudness
curve-rate = 18    // 0.070 — curve shape
curve-exponent = 256  // 1.0
curve-start = 77   // 0.30  — slow-speed precision
```

## Build

GH Actions config repo run `33266133984` (Exp74 branch) — see Findings.

## Verification plan

1. Flash `dabase_v2_right-promini-i2c.uf2`-equivalent direct-PS2 build to the
   right half (COM7).
2. Slow touch → precise crawl; fast flick → accelerates; no runaway.
3. Hold J scroll / hold K volume / touch-toggle still work (inherit shaped
   deltas).
4. Compare feel to the coprocessor-era curve (~Exp61 memory).

## Findings

- Build: GH run `33266133984`, **all 8 jobs green** (only Node.js-20 deprecation
  annotations). Artifact `dabase_v2_right-ps2-direct.uf2` **629248 B**.
- Flashed to the right half on **COM7** (user note: right side = COM7, dongle
  = COM21/22) — `flash-nicenano.ps1` headless, bootloader accepted UF2, device
  back online on COM7.
- Verified live on the shell: boot log shows
  `PowerCurve: sens=128 rate=18 exp=256 start=77 (Q8.8)` (curve ACTIVE);
  `ps2_mouse @15851ms: bytes=936 pkts=312 align_abort=0 buf_to=0` (clean
  decode, zero aborts); live packet stream `x=16 y=3`, `x=-10 y=-13` etc.
- **User feel test: "Great — matches coprocessor era"** — slow nudges crawl,
  flicks accelerate, no regressions reported. The Exp60/61 tuning transfers
  1:1 to the on-device curve (as hoped — same formula, same units v=|(x,y)|).
- Script gotcha: `flash-nicenano.ps1` uses `($size bytes)` string interpolation
  that **Windows PowerShell 5.1** rejects at parse time. Run it under **pwsh 7**
  (`& script.ps1`), not `powershell -File`.

## Conclusion

**Success.** Baking the RawAccel-style Power curve into the nice_nano (ZMK
driver) is viable and matches the coprocessor-era feel with zero coprocessor
and zero OS-level tooling. The `zmk,input-mouse-ps2` driver now carries the
curve as Q8.8 DT props (`curve-sens/-rate/-exponent/-start`), applied
per-sample before the divisor/report stage — topology-independent (peripheral,
standalone central, and dongle all get it) and portable across hosts. The OS
stays neutral (Windows speed 10), honoring the Exp67 lesson.

Known-good:
- zmk-ps2-trackpoint-driver fork `3aadda2` (Exp74 branch)
- zmk-config-dabaseV_0-2 `5ef1d9b` (Exp74 branch; driver pinned `3aadda2`)
- GH run `33266133984`, right-half uf2 `dabase_v2_right-ps2-direct.uf2`

## Next steps / decisions

- If curve feel is right at `128/18/256/77`, tune from there (sens = loudness,
  start = precision, rate/exp = shape).
- Revisit `zip_scroll_scaler 1 2` now that deltas are halved again (Exp73
  tuning comment).
- OS-level (RawAccel) remains the tuning harness option if instant A/B is
  wanted; numbers transfer directly (same formula).

Known-good: driver fork `3aadda2` (Exp74), config `5ef1d9b` (Exp74).