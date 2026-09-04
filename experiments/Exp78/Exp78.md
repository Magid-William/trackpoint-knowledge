# Exp78: uart-ps2 backend @ 19200 baud on the ps2test bench

## Context

The direct-PS/2 stack (badjeff driver fork, `zmk-ps2-trackpoint-driver`)
currently runs the **gpio-ps2** backend (CLK-edge sampling — the AVR
mechanism). Exp68 tried the **uart-ps2** trick on THIS trackpoint and
abandoned it:

- `9600` — phase-walk garbage ({8C, CC, C4, 84, 80, BE...}).
- `14400` (= measured cell rate) — real 4-directional decode but ~50/50
  erratic: the TP's cheap RC clock jitters, so fixed-rate UART sampling
  straddles bit boundaries; filters (median-3, EMA-8, median-5+slew)
  improved but never fixed it.
- **19200 was NEVER tried.**

User instruction: ignore the prior UART conclusions — 19200 is an untried
rate — and run a clean test. This TP rejects ALL host commands (Exp06), so
decode adjustments must happen **driver-side**; the gpio-ps2 backend is
**not** a fallback in this experiment.

## Hypothesis

At 19200 (52 µs/bit) vs the ~69 µs/bit cell measured at 14400, the UART
frame "compresses" the PS/2 byte: sample points alternate mid-bit /
near-bit-boundary. Could be same-class erratic, or could track differently
if the TP's clock actually runs faster than 14400 during motion — the only
way to know is a clean build with every UART-era filter OFF.

## Plan

1. `zmk-config-ps2-test` branch `Exp78` (off `main` e7635b5):
   - `ps2test_trackpoint.dtsi`: mirror the proven justinmklam dabao uart-ps2
     structure — `&pinctrl` default/off states (UART RX = DAT **P0.08**,
     TX parked on unexposed **P0.27**), `&uart0` `current-speed = <19200>`
     + child `uart_ps2` node, `tpoint0.ps2-device = <&uart_ps2>`.
     Wiring unchanged (DAT→P0.08 = uart0 RX physically; Exp68 proved "only
     RX-on-P0.08 ever received frames").
   - `ps2test_right.conf`: `PS2_UART=y` / `PS2_GPIO=n`; drop the four
     `PS2_GPIO_*` options; add `CONFIG_LOG_CMDS=y` +
     `CONFIG_PS2_LOG_LEVEL=4` (shell `log enable dbg ps2_uart` works).
   - Keep: `NO_HOST_COMMANDS` (TP rejects everything), PowerCurve,
     telemetry printk (1s counters), divisor 1, filters OFF, EXT_POWER.
2. GH Actions build → `ps2test_right-nice_nano.uf2` (run `33871618416`).
3. Flash COM8 (baseline now: working gpio-ps2 firmware — user sanity check).
4. Shell on COM8: probe → `uart:~$` → `log enable dbg ps2_uart` → nub
   motion → watch per-byte decode + parity/framing flags + 1s telemetry.
5. User A/B (question tool): 4 directions? smooth low-speed? fast flicks?
   feel vs the working gpio baseline.
6. If erratic: driver-side iterations only (median/slew/EMA are driver
   Kconfigs; then 14400 A/B on the current fork). Never switch to gpio.

## Known-good / baseline

- Config `main` @ `e7635b5` (gpio-ps2, working on COM8 today).
- Driver pinned `b8a2200b` (pr/optional-features, same family as dabase
  `75d4d71`); ZMK pinned `ac7f75b8`.

## Files changed (config branch Exp78 @ 95155d5)

| File | Change |
|---|---|
| `boards/shields/ps2test/ps2test_trackpoint.dtsi` | gpio_ps2 → disabled; `&pinctrl` uart0 default/off; `&uart0` 19200 + `uart_ps2` child; `tpoint0` → `&uart_ps2` |
| `boards/shields/ps2test/ps2test_right.conf` | backend flip; gpio-only options removed; `LOG_CMDS` + `PS2_LOG_LEVEL=4` |

## Findings

(pending flash + test)

## Conclusion

(pending)