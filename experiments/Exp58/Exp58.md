# Exp58: Smoothing the cursor — ATtiny PS/2 readTimeout kills choppiness

## Hypothesis

Exp57 delivered a moving host cursor, but movement felt choppy. The wire
(trackpoint → ATtiny85 → I2C → ZMK driver) delivers ~100Hz PS/2 packets but the
data reaches the pointer irregularly. Two prime suspects:

1. **ZMK side**: `trackpoint-i2c.c` logs `LOG_INF("raw sign-extended...")` on
   every 10ms poll plus two more `LOG_INF` lines per SEND. With
   `zmk-usb-logging` + `CONFIG_TRACKPOINT_I2C_LOG_LEVEL_INF`, that is ~100–200
   USB CDC log lines/sec while the poll runs on the **system workqueue** — each
   blocking-ish serial write stretches the 10ms poll cadence → jitter.
2. **ATtiny side**: `PS2Trackpoint::readByte()` uses a busy-loop timeout counter.
   `setReadTimeout(2000)` ≈ 1.5ms at 8MHz, but the inter-packet PS/2 gap is
   ~7ms. The read starts waiting inside the gap and **times out before the next
   packet's clock edges arrive** (Exp56 already showed `read_timeouts` climbing
   ~90/300ms) → misaligned/missed reads → jittery X/Y.

## Plan

1. ZMK first (user preference): demote `CONFIG_TRACKPOINT_I2C_LOG_LEVEL_INF` →
   `WRN` on the `Exp58` branch of `zmk-trackpoint-shield`, GH-build, flash, test.
2. If still choppy, isolate the ATtiny side: bump `setReadTimeout(2000 → 10000)`
   (≈7.5ms, covers the whole gap) and reflash via Leonardo ISP.

## Findings

- **ZMK log-level change made no difference.** Exp58 branch GH run
  `31530996047` built `corne_trackpoint_right-nice_nano-poweron.uf2`
  (663,552 B), flashed headless on COM8, WRN log flood gone — cursor still
  choppy. Log flood ruled out as primary cause; **reverted** the conf back to
  `INF` (`34a9c73`) since the artifact is functionally identical (no reflash).
- **ATtiny `readTimeout` bump fixed it.** Single-line change:
  `ps2.setReadTimeout(2000 → 10000)` in
  `attiny85-trackpoint/trackpoint-i2c-slave-attiny85.ino:66`. Build still
  2440 B flash (89 B RAM), flashed + verified via Leonardo ISP (sig
  `0x1e930b`, no `-D`). User confirmed the cursor is now smooth.
- **What `10000` means**: a busy-loop timeout counter, not ms. Each `while`
  iteration in `readByte()` polls the CLK pin (~0.75µs at 8MHz), so 10000 ≈
  7.5ms of waiting for a clock edge before giving up. That spans the ~7ms
  inter-packet gap, so reads lock onto real packet boundaries and deltas stop
  being dropped/misaligned.
- Safe because I2C serving is USI-ISR-driven (`requestEvent`), not main-loop
  driven — a longer main-loop block in `readByte` does not starve the I2C bus.

## Conclusion

**SUCCESS.** Choppiness was on the **ATtiny side**, not ZMK: a too-short PS/2
`readByte` timeout was timing out inside the inter-packet gap, dropping and
misaligning X/Y samples. Raising the busy-loop timeout to cover the gap
(`2000 → 10000`) made the cursor smooth. The ZMK log-level experiment was
negative and was reverted. No ATtiny accumulation/smoothing was needed.

Deliverables:
- ATtiny fix: `trackpoint-i2c-slave-attiny85.ino` `setReadTimeout(10000)` —
  flashed + verified (2440 B, sig `0x1e930b`).
- `Exp58` branch in `zmk-trackpoint-shield` (`a70c534` WRN attempt,
  `34a9c73` revert to INF) — no net code change on the ZMK side.

**Next experiment candidates**:
1. **BLE/dongle topology** — wire the trackpoint half into the dongle/split
   setup now that USB-only movement is smooth.
2. **Accel/sensitivity tuning** — `zip_xy_scaler`/driver scaling if cursor feel
   (speed/distance) needs adjustment now that it is smooth.
