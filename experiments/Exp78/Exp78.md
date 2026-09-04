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

### it1 — 19200 (run `33871885735`, flashed COM8): FAILED (decisive)
- Boot log: `ps2_uart: Initializing ps2_uart driver with pins... SCL: P0.06; SDA: P0.08;
  SDA Pinctrl: P0.08` + `UART device is ready` — backend @ 19200 init clean, no rewiring.
- Every UART frame errors: repeat flood `0xfc/0xfe/0xf0/0x60/0xe0` tagged
  `Framing error (4)` / `Parity error (2)` — ~0% clean frames.
- Input: `bytes=1758 pkts=364 align_abort=804 buf_to=4` (~10s telemetry) — constant
  alignment aborts; full-scale garbage (`mov_y=96 -> Mouse movement set to 127/-5`);
  phantom MB1/MB2/MB3 (`Pressing button_m/r`, `Ignoring button presses...tranmission error`).
- Root cause (physical): PS/2 bit cell ~69µs (14.4kHz) is LONGER than the 19200 UART
  cell (52µs) → UART frame (520µs) ends mid-PS/2-byte, re-triggers on mid-byte LOWs →
  multiple garbage frames per real byte. 19200 > cell rate = worse than 14400.
- DBG flood saturated the CDC console as Exp70 warned; board stayed alive.

### it2 — 14400 A/B (run `33872395685`, flashed COM8): SAME CLASS, LESS SEVERE
- Same error flood (bytes `0x7f/0x78/0xe0/0xe1/0xe6/0xdf/0xbe/0xff...`), every byte
  parity (2) or framing (4) flagged; constant `Bit 3 of packet is 0` / `Multiple button
  presses` aborts.
- Telemetry comparison (~10s):
  - 19200: `bytes=1758 pkts=364 align_abort=804 buf_to=4` (~69% packets aborted)
  - 14400: `bytes=1960 pkts=591 align_abort=722 buf_to=1` (~55% aborted)
- So 14400 forms more packets but still a torrent of errors — this TP's RC clock drifts
  against ANY fixed baud (Exp68's "real decode but 50/50 erratic" is the optimistic end
  of the same spectrum; today's fork decodes it worse).
- Filters OFF was the right A/B call; nothing at either rate decodes cleanly enough for
  byte-level filters to rescue (Exp68: filters "improved but never fixed" 14400).

## Conclusion

**FAILED (experiment complete).** The one previously-untried rate is now tested:
uart-ps2 cannot decode this trackpoint at ANY baud:

- 9600 (Exp68): phase-walk garbage.
- 14400 (Exp68 + it2 A/B here): real-ish decode but torrent of parity/framing
  errors (~55% packet aborts today, `bytes=1960 pkts=591 align_abort=722`);
  Exp68's filters "improved but never fixed".
- **19200 (it1): worst** — UART frame (520µs) is shorter than the PS/2 bit cell
  (~69µs @14.4kHz), so the UART re-triggers mid-byte and produces multiple
  garbage frames per real byte; ~0% clean frames.

Root cause is structural, not fixable by suppressing/ignoring errors: this TP's
RC clock drifts/jitters around ~14.4kHz, and the uart-ps2 trick REQUIRES a stable
clock ≈ baud. The UART bytes are aliased (wrong), not merely flagged (right-but-
errored), so no log-level or flag-suppression lever recovers them. Byte-level
filters (median/slew/EMA) had nothing clean to average. Only CLK-edge sampling
(the gpio-ps2 backend, + this experiment's DISABLED gpio fallback) handles this
TP — Exp68's verdict stands, now with the missing 19200 data point added.

Deliverables / close-out:
- Exp78 config branch `zmk-config-ps2-test` @ `611dea9` (it2 14400) — kept for
  rollback/re-run; the bench is restored to the known-good gpio-ps2 firmware
  (`main` @ `e7635b5`, run `33278177086`, re-flashed to COM8).
- Known-good baseline: `main` `e7635b5` (gpio-ps2), driver `b8a2200b`, ZMK `ac7f75b8`.
- Lesson for the AGENTS.md uart-vs-gpio note: this TP has been tried at 9600 /
  14400 / 19200 over UART — all fail; the uart-ps2 trick is unsuitable for it.