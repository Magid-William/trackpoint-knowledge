# Exp75: PR-worthy refactor — Exp68–74 fork changes as optional features on fresh badjeff main

## Context / hypothesis

Badjeff `kb_zmk_ps2_mouse_trackpoint_driver` is the upstream the direct-PS/2
stack (Exp68–74) was forked from. Fork `Magid-William/zmk-ps2-trackpoint-driver`
carried every experiment change as hardcoded/always-on or default-off-but-not-
always-gated code. Hypothesis: a fresh branch on current badjeff `origin/main`,
where **every** fork change is (a) a pure fix, or (b) opt-in via Kconfig/DT
props with defaults equal to stock, is PR-worthy — a third party with the
command-rejecting trackpoint module can follow upstream wiring + a config
snippet and reach the Exp74-equivalent state without touching upstream code.

## Result: SUCCESS (build-verified; hardware feel-test optional)

The refactor is complete and both configurations compile clean:

- **Features ON** — `zmk-config-ps2-test` `pr-verify` run `33272161053`: all green
  (`ps2test_right-nice_nano` built in 3m18s). Enables every new option +
  PowerCurve tuning `128/18/256/77` on `tpoint0`.
- **Stock OFF** — `zmk-config-justinmklam` (fork) `pr-verify-stock` run `33272166618`:
  all green (dabao built in 3m6s). Zero new options — proves the stock handshake path
  compiles against the refactor with unchanged behavior.
- First stock run (`33272028706`) **caught a real latent bug**: the fork's
  `reporting_enable`/`disable` only ever compiled with `NO_HOST_COMMANDS=y`, so the
  `int err` declarations collided when the feature was OFF (`redefinition of 'err'`).
  Fixed by hoisting `int err = 0;` to function scope. This is exactly the kind of
  regression the "stock build" check exists for, and it was not visible in the fork
  because the fork never built that configuration.

## Decisions (user-confirmed)

| Topic | Decision |
|---|---|
| Base | badjeff `origin/main` (`7ab7846`) — fresh clone, includes upstream `rst-gpios` fix |
| PowerCurve gate | `ZMK_INPUT_MOUSE_PS2_POWER_CURVE` bool **default n** |
| Internal pull-ups | `PS2_GPIO_INTERNAL_PULLUP` + `PS2_UART_INTERNAL_PULLUP` **default n** |
| ps2-gpio cb stack | Kconfig **default 1024** (strict stock); config sets 4096 |
| Config deliverable | README docs only |
| PR submit | **No** — branch pushed to fork, ready to open |

## Feature ↔ Kconfig map (the PR's option surface)

| Option | Default | What it restores from the fork |
|---|---|---|
| `ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS` | n | handshake skip, int8 decode, no 0xF4/F5/FE (Exp68) |
| `PS2_GPIO_NO_RESEND` | n (y if NO_HOST_COMMANDS) | suppress backend 0xFE writes (Exp70) |
| `PS2_GPIO_TIMING_SCL_CYCLE_MAX` | 100 | jittery-clock tolerance (8000 for TP) (Exp70) |
| `PS2_GPIO_INTERNAL_PULLUP` / `PS2_UART_INTERNAL_PULLUP` | n | open-drain pull-ups (Exp68) |
| `PS2_GPIO_WORK_QUEUE_CB_STACK_SIZE` | 1024 | deep split-callback chain stack (Exp70) |
| `ZMK_INPUT_MOUSE_PS2_SPEED_DIVISOR` | 1 | remainder-accumulated speed scaling (Exp68) |
| `ZMK_INPUT_MOUSE_PS2_MEDIAN_WINDOW` / `SLEW_MAX` / `MOVEMENT_EMA_N` | 1 / 0 / 1 | anti-glitch filters (Exp68) |
| `ZMK_INPUT_MOUSE_PS2_TELEMETRY` | n | 1s decode counters (Exp70 it3, LOG_* instead of printk) |
| `ZMK_INPUT_MOUSE_PS2_POWER_CURVE` | n | on-device RawAccel Power curve (Exp74) + `curve-*` DT props |

Pure fixes shipped unconditional (no behavior change):

- read watchdog moved off the system workqueue → the driver work queue (Exp70 it4)
- `REPORT_INTERVAL_MIN` accumulators cleared per-axis (Exp70 it2)
- `rst-gpios` prop-name regression from the fork NOT carried (upstream `e493238` already fixed it)

## Commit series (on `fork:pr/optional-features`, base badjeff `7ab7846`)

1. `fix(ps2-gpio): run read watchdog on the driver work queue`
2. `fix(input-mouse-ps2): clear report-interval accumulators per axis`
3. `feat(ps2-gpio): make the callback work-queue stack size a Kconfig`
4. `feat(ps2-gpio): make the SCL cycle timeout configurable, add opt-in pull-ups`
5. `feat(ps2-uart): add opt-in internal pull-up on the CLK line while reading`
6. `feat(input-mouse-ps2): add NO_HOST_COMMANDS mode`
7. `feat(input-mouse-ps2): add speed divisor and anti-glitch filters`
8. `feat(ps2): add opt-in decode telemetry`
9. `feat(input-mouse-ps2): add on-device PowerCurve (RawAccel "Power")`
10. `docs: direct-wired no-command trackpoint guide + option reference`

Branch head `0d38078` pushed to `Magid-William/zmk-ps2-trackpoint-driver` `pr/optional-features`.

## Verification (GH Actions)

- **Features ON** — `zmk-config-ps2-test` `pr-verify` (west.yml → `b186d02`): run
  `33272161053` all green. Enables NO_HOST_COMMANDS, PS2_GPIO_NO_RESEND,
  INTERNAL_PULLUP, SCL_CYCLE_MAX=8000, CB_STACK=4096, POWER_CURVE, TELEMETRY + curve
  tuning props (128/18/256/77).
- **Stock OFF** — `zmk-config-justinmklam` (fork) `pr-verify-stock` (west.yml → `b186d02`):
  run `33272166618` all green. Zero new options — regression check on the normal
  handshake path (rst-gpios, sampling-rate, scroll-mode).
- The first stock attempt (`33272028706`, SHA `0d38078`) failed with `redefinition of
  'err'` in `reporting_enable`/`disable` — see Result above; fixed at `b186d02`.

## Files changed (driver PR)

`README.md`, `dts/bindings/input/zmk,input-mouse-ps2.yaml`, `src/drivers/input/CMakeLists.txt`,
`src/drivers/input/Kconfig`, `input_mouse_ps2.c`, `power_curve.c/.h`,
`src/drivers/ps2/Kconfig.gpio`, `Kconfig.uart`, `ps2_gpio.c`, `ps2_uart.c`.

## Next / follow-ups

- Optional hardware feel-test: flash `zmk-config-ps2-test` pr-verify `ps2test_right-nice_nano`
  artifact to the COM7 nice_nano, confirm cursor + PowerCurve feel on the bench.
- When ready, open the PR from `Magid-William/zmk-ps2-trackpoint-driver`
  `pr/optional-features` (`b186d02`) → `badjeff/kb_zmk_ps2_mouse_trackpoint_driver` main.
- Optional: template config repo if the README recipe proves insufficient.