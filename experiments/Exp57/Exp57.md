# Exp57: Full pipeline — ATtiny85 PS/2 bridge → I2C → stock driver → host cursor moves

## Hypothesis

Exp56 proved the ATtiny85 reads the real PS/2 trackpoint (both axes) and serves
live X/Y as an I2C slave @0x42, read from the ZMK shell. The last unchecked box
from the original goal is making the ZMK driver actually feed the pointer so the
**host cursor moves** with the nub. The mouse-moving config already exists in
`zmk-trackpoint-shield` (corne_trackpoint, `Exp49` branch — known-good "mouse
moves" build) and the stock `promini,trackpoint-i2c` driver polls `0x12`
@10ms and emits REL_X/REL_Y. Flashing that build onto the NiceNano should give
live cursor movement from the real trackpoint — **zero code changes on any
side** (ATtiny85 unchanged, driver unchanged, no overlay edits expected).

**Zero ATtiny changes.** The Exp56 bridge firmware stays. **Zero driver
changes.** Reuse the verified `zmk-trackpoint-shield` corne_trackpoint config
(central right half, zmk-usb-logging, i2c shell, plain 10ms polling, no
irq-gpios).

## Plan

1. Create `Exp57` branch in `zmk-trackpoint-shield` from `Exp49` (clean).
2. Push → GH Actions builds `corne_trackpoint_right-nice_nano-poweron` (central,
   `-DCONFIG_ZMK_SLEEP=y -DCONFIG_ZMK_IDLE_SLEEP_TIMEOUT=120000`).
3. Download the `.uf2` when the run passes.
4. Flash NiceNano headless via `flash-nicenano.ps1 -ComPort COM8`.
5. Verify: bus scan for 0x42, live-stream the burst, and have the user touch the
   nub — the host cursor must move.

## Success criteria

- [x] Exp57 branch created + pushed, GH Actions run green (~2.5 min)
- [x] `corne_trackpoint_right-nice_nano-poweron.uf2` downloaded
- [x] Flashed headless (GPREGRET + UF2 copy, "Device back online on COM8")
- [x] Live `x/y` stream from the real nub via `live-trackpoint.ps1 -Burst`
- [x] **Host cursor moves with the nub**
- [x] ATtiny85 firmware untouched; driver untouched

## Findings

- **Everything just worked once the right firmware was on.** The previous
  no-movement build was Exp56's read-only test by design; the mouse-moving
  config lives in `zmk-trackpoint-shield` (corne_trackpoint). No source changes
  were needed anywhere.
- **GH Actions**: run `31527783428` succeeded in 2m25s; uf2 was 666,624 bytes.
- **Flash hiccup**: after the first flash, the shell was unreachable — every
  serial probe (pwsh + python) hung in `Open()` on COM8 even after killing
  orphaned processes. Root cause was a stale/ghost USB CDC state post-flash.
  **Unplug + replug the NiceNano** cleared it. The board had finished booting,
  but the port was wedged until re-enumeration.
- **Live stream (Exp56 bridge → ZMK shell, no cursor check needed for data)**:
  `live-trackpoint.ps1 -Port COM8 -Burst` shows real motion in both axes, e.g.
  `x= 2 y= -4`, `x= 10 y= -15`, `x=-13 y=-13`, `x=-21 y=-15`, `x= 4 y= 15`,
  `x= 0 y= 4`. Deltas change exactly when the user touches/moves the nub.
- **Host cursor moves.** User confirmed the mouse moves and logs flow after the
  replug. This is the full-pipeline success box from the original goal.
- Reused known-good pieces: `zmk-pmw3610-driver` pinned `6f84b62` (polls `0x12`
  @10ms, emits REL_X/REL_Y), corne_trackpoint central enables the
  `trackball_listener` → `&trackball` wiring that moves the pointer.

## Conclusion

**SUCCESS.** The complete chain — real PS/2 trackpoint → ATtiny85 (Exp56 bridge,
2440 B, unchanged) → I2C slave @0x42 → stock `promini,trackpoint-i2c` driver →
ZMK pointer → host cursor — works end-to-end with **zero code changes** on any
side. The only friction was a wedged USB CDC after flashing; unplugging and
replugging the NiceNano restored the shell, and then the mouse moved.

Deliverables this round:
- `Exp57` branch in `zmk-trackpoint-shield` (from `Exp49`, only uf2 artifact
  cached locally at `build/Exp57-poweron.uf2`).
- Flashed firmware verified moving the host cursor from the real nub.

**Next experiment candidates** (in order of value):
1. **Trim power / sleep with live data** — prove the Exp49 `CONFIG_ZMK_SLEEP=y`
   auto-cut still behaves with the real ATtiny bridge (it was verified with the
   Pro Mini gates; confirm the ATtiny85 power path is equally cut + wake works).
2. **BLE topology** — wire the trackpoint half into the dongle/split setup
   (Exp27/28 era) now that USB-only movement is proven.
3. **Smoothing/accel tuning** — `zip_xy_scaler`/driver scaling with the ATtiny's
   raw deltas if cursor feel needs adjustment.
