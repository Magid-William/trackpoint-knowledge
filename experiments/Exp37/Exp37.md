# Exp37: Verify I2C connection restoration on Pro Mini wake

## Hypothesis

The Pro Mini's I2C slave connection is fully restored when it wakes from sleep. The wake path orders TWI re-init before signaling the ZMK driver (`Wire.begin(I2C_ADDR)` at line 181, then `digitalWrite(MOT_PIN, HIGH)` at line 182), so the driver's first post-wake poll should succeed with no errors. Sleep entry disables TWI (`TWCR = 0`) and drives MOT LOW; wake re-arms TWI and drives MOT HIGH.

## Scope

Verify-only per the plan — no code changes unless a real gap is found. Focus: Pro Mini I2C connection restored when awake, across repeated sleep/wake cycles.

## Verification results

### 1. Wake → first I2C read succeeds (no error)

Polled the motion burst register (`i2c read i2c0 0x42 0x12 2`) from the NiceNano shell while touching the nub. Every AWAKE read returned data with **zero I2C errors** — the very first read after wake succeeds, confirming `Wire.begin()` fully re-armed the TWI slave before MOT HIGH signaled the driver.

Example reads (burst_x burst_y as bytes):
```
03 ff   fe fe   00 04   01 ff   02 02   00 ff   ff ff
```
Motion flows correctly. Sub-deadband values (`fe`=-2, `fc`=-4) appear in the burst while awake but correctly do not wake the MCU (DEADBAND=3 gate in the wake path).

### 2. Repeated sleep/wake cycles — 0x42 responds every time

Multiple transitions observed across two polling sessions:
```
asleep (NACK) → touch → AWAKE (burst data) → idle 5s → asleep (NACK) → touch → AWAKE ...
```
The slave responds on every wake, no TWAR drift, no gradual degradation, no phantom addresses when properly asleep. The mid-sleep phantom addresses (`0x28`/`0x20`) from Exp36 were confirmed harmless artifacts of the TWI-disabled pin state — they disappear when the MCU is truly asleep.

### 3. Speed-scale persistence

`speed_scale` (reg 0x11) is a `static` in the Pro Mini — written once by the driver at init, never touched by the sleep loop or wake path, and survives across sleep (static memory isn't reset). Cursor sensitivity was consistent after wake. Register 0x11 is write-only so it can't be read back, but persistence holds by construction.

### 4. Driver first-poll handshake

The driver resumes on the MOT rising edge (`wake: MOT inactive (high), resuming poll work`) and its first poll reads the burst successfully — the manual I2C reads above prove the slave responds, and the cursor moving after wake (confirmed in Exp36 and this session) proves the driver's poll→`input_report`→cursor path reconnects with no handshake gap.

### 5. Long-idle wake restoration

After ~18 minutes of continuous Pro Mini sleep (Exp36's lockup regression test), touching the nub woke it and the cursor resumed — no re-init, no reset needed. The driver's error-backoff is never engaged on the happy path because it never polls a sleeping slave.

## Conclusion

**Success — zero code changes.**

The current I2C-restoration behavior is verified-good:

- `Wire.begin()` on wake fully restores the TWI slave **before** MOT HIGH signals the ZMK driver (ordering at trackpoint-i2c-slave.ino:181-182)
- First post-wake read succeeds with no I2C error, every cycle
- Repeated sleep/wake cycles stable, speed_scale preserved, long-idle wake clean

## Notes for next experiments

- True deep sleep (`SLEEP_MODE_POWER_DOWN`) is the next Pro Mini goal — the current IDLE-mode sleep leaves the AVR mostly running (Timer0 wakes every 1ms, PS/2 read busy-waits ~84ms). See Exp37 analysis in discussion.
- Trackpoint must stay powered (it's the wake source), so the P-mosfet power-cut idea is a separate future experiment.
