# Exp59: Investigate the trackpoint "31 hard cap" — ATtiny max_delta gate drops fast motion

## Hypothesis

Fast trackpoint flicks showed `x= 31 y= -11 timeouts=47083` in `live-trackpoint.ps1`
and the cursor stopped. The 31 "cap" is not the sensor and not ZMK — it is the
ATtiny85's `max_delta = 32` packet-discard sanity gate in `PS2Trackpoint.cpp`
(`if (ix > max_delta || ...) return false;`). Because `last_xraw`/`last_yraw` are
only stored **after** the gate passes, the debug buffer can never show |delta| ≥ 33:
it freezes at the largest passing value (31 = 32-1) and the big fast-motion packets
are dropped entirely → no burst → driver zero-count → cursor stops.

**Zero ZMK changes** — the debug readout (`live-trackpoint.ps1` reads the ATtiny
`0x03` debug buffer via `i2c read`) bypasses ZMK entirely, so ZMK is not a
candidate. ZMK config was also audited: overlay only has a `temp_layer`
input-processor (layer toggle, no scaling/clamp) and `trackpoint-i2c.c` passes
per-packet int8 straight through.

## Plan

1. Create `Exp59` branch in `attiny85-trackpoint`.
2. `PS2Trackpoint.h`: `max_delta = 32 -> 127` (gate now only catches genuinely
   absurd misaligned reads; misalignment is already caught by the Exp43
   `read_timeouts != tmo_before` check + Exp58 readTimeout tuning).
3. `pio run` the i2c-slave sketch, flash via Leonardo ISP (sig `0x1e930b`), verify.
4. Push branch. Apply the same one-line fix to `promini-trackpoint` too (no flash).
5. Live-test with `live-trackpoint.ps1` once the ATtiny is back on the NiceNano.
6. Update Exp59.md + Experiments.md with findings.

## Findings

- **Live hardware verification (after ATtiny back on NiceNano, COM8)**:
  `live-trackpoint.ps1 -Burst` shows raw deltas well past the old 31 cap once
  the gate is raised — `x=-48 y=-15`, `x=40 y=7`, `y=37`, `x=-32`, `x=29 y=15`,
  `x=19 y=-21` under fast flicks. These were previously **dropped** by the
  `max_delta = 32` gate. Sensor confirmed to emit large per-packet deltas; the
  gate (not the sensor) was the cap.
- **Root cause identified by code analysis**: `max_delta = 32` gate in
  `PS2Trackpoint.cpp:105` drops any packet with |delta| >= 33. `last_xraw` is
  stored only *after* the gate (line 108), so the debug buffer can never display
  |x| >= 33 — it freezes at `31 = 32 - 1`. Fast flicks produce large 9-bit signed
  PS/2 deltas (range ±255 per packet; sign math at lines 93-94) that hit the gate,
  get discarded → `trackpoint-i2c.c` sees 0,0 → 3× zero-count → accumulators
  cleared → cursor stops. The "cap at 31" and the "mouse stops" are the SAME bug.
- **ZMK ruled out**: the debug readout is raw bytes served by the ATtiny and never
  passes through ZMK's input/scaling. Overlay shows no scaler, driver passes int8
  through unchanged.
- **Sensor ruled out (by protocol)**: this IC streams standard PS/2 motion packets
  (randalea.de: fixed-function motion encoder, 9-bit signed deltas), so it can emit
  far more than ±31 per packet. `timeouts=47083` in the log is a separate,
  cumulative PS/2-read counter (Exp56/58) — not the cap.
- **Fix applied to both firmware repos** (single header line, same as Exp58's fix
  was ATtiny-side):
  - `attiny85-trackpoint` — `Exp59` branch, commit `1779327`. Built via
    `pio run` (2440 B flash / 89 B RAM), flashed + verified via Leonardo ISP
    (sig `0x1e930b`, no `-D`), pushed.
  - `promini-trackpoint` — `Exp59` branch, commit `3b5322e`. Also raised the
    sketch's own `MAX_DELTA 25 -> 127` in `trackpoint-i2c-slave.ino` (the sketch
    had its own stricter gate that would have still capped motion at 25
    regardless of the library fix). **Flashed + verified** via CH340G
    (stk500v1, sig `0x1e950f`, 3588 B written/verified).

## Conclusion

**SUCCESS (investigation + fix applied).** The "31 hard cap" was neither the
trackpoint sensor nor ZMK — it was the ATtiny85's `max_delta = 32` packet-discard
gate, which dropped exactly the large fast-motion packets, freezing the cursor and
making the debug buffer look capped at 31 (= 32−1). Raising the gate to 127 (and
relying on the Exp43 `read_timeouts`-mismatch check for misalignment rejection)
restores full-range deltas.

Deliverables:
- `attiny85-trackpoint` `Exp59` (`1779327`): `max_delta 32 -> 127`, built + flashed
  + verified (2440 B, sig `0x1e930b`).
- `promini-trackpoint` `Exp59` (`3b5322e`): library fix + sketch `MAX_DELTA 25 -> 127`
  (also raised; the sketch's own gate would have capped motion at 25 regardless),
  branched + pushed, **flashed + verified** (3588 B, sig `0x1e950f`).
- Zero ZMK/driver/shield changes.
- Live-verified on the NiceNano: `live-trackpoint.ps1 -Burst` on COM8 now streams
  deltas > 31 (`-48`, `40`, `37`, `-32`...) that the old gate discarded.

**Next experiment candidates**:
1. **Cursor feel at high velocity** — with full-range deltas flowing, confirm the
   host cursor keeps moving during fast flicks (no more freeze) and tune
   `zip_xy_scaler`/driver scaling if the top speed needs adjusting.
2. **BLE/dongle topology** — wire the trackpoint half into the dongle/split setup
   now that USB-only movement is smooth.