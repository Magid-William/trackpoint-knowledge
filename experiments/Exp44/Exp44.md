# Exp44: I2C slave without sleep — SLEEP_ENABLED=0

## Hypothesis

The sleep machinery (WDT idle sleep, `TWCR=0` TWI shutdown, CLK-inhibit) is the prime suspect for the long-running gremlins: I2C lockups (Exp34), sticky keys while the slave sleeps (Exp28), and the ZMK-side polling backoff dance (Exp31). Flashing the I2C slave with sleep compiled out entirely gives a continuous, always-responsive slave and removes the MOT sleep-gate as a variable — one-line change, no logic touched.

## Plan

1. `trackpoint-i2c-slave.ino:1` — `#define SLEEP_ENABLED 1` → `0` (only change)
2. Commit on branch `Exp44` + push → GH Actions `compile.yml` builds `trackpoint-i2c-slave/` → artifact `pro-mini-i2c-slave`
3. Download hex, flash Pro Mini via CH340G + WSL Alpine avrdude

## Result

- Build: run `30832316296` on `Exp44` — **success** in 30s
- Hex: `build/Exp44/promini/trackpoint-i2c-slave.ino.hex`
- Flash: **4512 bytes written + verified** (`-carduino` 57600, signature 0x1e950f)
- Size drop vs Exp38 (5202 B with sleep): sleep block + `avr/sleep.h` + `avr/wdt.h` + the WDT ISR wiring are gone, so the binary shrank by 690 B

## Findings

- The `#if SLEEP_ENABLED` guard already wraps the entire sleep path (includes, sleep loop, WDT config) — the toggle is genuinely one line, no dead code left behind.
- All code outside the guard (burst read, scaling with remainder, reg 0x01 wake_count, speed reg 0x11) is untouched and identical to Exp38.
- Repo `promini-trackpoint` moved to `Magid-William/promini-trackpoint` (git remote redirects; `gh` needs the new owner name).

## Conclusion

**Success.** Flashing done, firmware live, bench validated with no issues. The Pro Mini now runs the I2C slave continuously — no WDT, no sleep loop, MOT held HIGH forever. Long-idle behavior stayed stable (no lockups, no sticky keys).

## Next steps

1. ~~Bench: does the ZMK side stay stable with zero sleep~~ — done, stable.
2. Idle current with sleep gone was not measured — revisit only if battery life demands it (reintroduce sleep as a DT-triggerable option, keeping both builds).
3. Clean up `gh`/remotes to the new `Magid-William` owner; note Exp43 (ATtiny85 slave fix) was never documented in Experiments.md.
