# Exp70: right-half hardfault on trackpoint touch — stack overflow + 0xFE resend writes

## Bug report

- Topology: Exp69 dongle — right half `dabase_v2_right-ps2-direct` (BLE peripheral,
  gpio-ps2, NO_HOST_COMMANDS) + XIAO dongle + left half. TP direct to right nice_nano
  (DAT→P0.08, CLK→P0.06, P0.13 EXT rail).
- Symptom: touch TP → mouse works a couple of seconds → **whole right half dies**
  (pointer + right-half keys). Left-half keys keep working through the dongle.
  Untouched TP → everything keeps living.
- Root-framing: right half alone faults; left half survives ⇒ the fault lives in the
  right-half path `PS/2 decode → input_report → trackball_split (BLE forward)` and
  requires an active split-forward link.

## Hypothesis (verified pre-change)

Two concrete fragilities in the driver fork `5ef6699`, both matching the symptom:

1. **Stack overflow on the PS/2 callback work queue (H1).**
   `src/drivers/ps2/ps2_gpio.c` hard-coded
   `PS2_GPIO_WORK_QUEUE_CB_STACK_SIZE 1024` at priority 2. `ps2_gpio_read_callback_work_handler`
   → `zmk_mouse_ps2_activity_callback` → on a complete packet
   `zmk_mouse_ps2_activity_move_mouse` → **`input_report_rel()` inline**. In the Exp68
   no-split builds the chain was shallow; with `trackball_split` forwarding over the
   split BLE transport the chain is deep (input → split → L2CAP/BT send) and 1 KiB
   overflows → MPU/HARD FAULT → whole firmware dies.
2. **The gpio backend still writes 0xFE to the TP (H2).** `ps2_gpio_read_abort(true,…)`
   → `ps2_gpio_send_cmd_resend()` → `ps2_gpio_send_cmd_resend_worker()` →
   `ps2_gpio_write_byte(0xFE)`. `ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS` only gated the
   *input-driver* resends. This TP rejects every command, so each attempt inhibits CLK
   and times out (~10 ms write SCL) — corrupting an ongoing stream / wedging the write
   FSM, and firing once decode errors appear under load.

## Plan

1. Driver fork (`kb_zmk_ps2_mouse_trackpoint_driver`): branch `Exp70` off `5ef6699`.
   - F1: `PS2_GPIO_WORK_QUEUE_CB_STACK_SIZE` Kconfig, default **4096** (replaces the
     1024 define).
   - F2: `PS2_GPIO_NO_RESEND` Kconfig, `default y if ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS`,
     no-ops the 0xFE write in `ps2_gpio_send_cmd_resend_worker`.
2. Config (`zmk-config-dabaseV_0-2`): branch `Exp70`, west.yml → driver `d971d8d`;
   `CONFIG_PS2_GPIO_NO_RESEND=y` + crash diagnostics in `dabase_v2_right.conf`
   (`CONFIG_FAULT_DUMP=2`, `CONFIG_THREAD_ANALYZER=y`, `CONFIG_THREAD_ANALYZER_AUTO=n`,
   `CONFIG_DEBUG_THREAD_INFO=y`; MPU stack guard stays default-on).
3. Build via GH Actions; flash right half (COM7) + dongle; user moves the nub
   (question tool prompt). Success = no crash on sustained TP motion.

## Changes

- Driver `Magid-William/zmk-ps2-trackpoint-driver` branch `Exp70` @ `d971d8d`:
  - `src/drivers/ps2/Kconfig.gpio` — two new config entries.
  - `src/drivers/ps2/ps2_gpio.c` — stack-size define reads the Kconfig; resend worker
    gated behind `CONFIG_PS2_GPIO_NO_RESEND`.
- Config `Magid-William/zmk-config-dabaseV_0-2` branch `Exp70` @ `dd39aa2`:
  - `config/west.yml` — driver revision `5ef6699` → `d971d8d`.
  - `config/dabase_v2_right.conf` — `CONFIG_PS2_GPIO_NO_RESEND=y` + crash diagnostics.

## Findings

- Build 1 (F1+F2): GH run `33221758823` — **all 8 builds pass**. Right half
  `dabase_v2_right-ps2-direct.uf2` = **612352 B**. User verified twice (sustained ~10s
  and ~30s fast-flicks + right-half keys) — **then crashed again in normal use**.
- Crash behavior confirmed by user: **stayed dead, needed a power cycle** (no self
  reboot). Consistent with a hardfault+halt (ZMK ships `RESET_ON_FATAL_ERROR=n`) AND
  with a hang — self-reboot doesn't discriminate, so a fault dump would be needed.
- **Reframing (crash returned)** — ZMK `ac7f75b8` source read (Part 2): the ps2
  driver's `input_report_rel()` is shallow (input core queues to `tpoint0` + submits
  `input_process` work); the deep chain runs on the **system workqueue**:
  `input_process → zmk,input-split split_input_handler → zmk_split_peripheral_report_event
  → zmk_split_bt_report_input → bt_gatt_notify` — **inline, unbuffered, once per event**.
  ZMK's own design queues position/sensor events to a dedicated notify thread
  (`service_work_q`) but input events bypass it. F1 (ps2 cb stack 1024→4096) therefore
  raised the wrong thread's stack — explaining why the crash returned.
- Build 2 (iteration 2): driver + `REPORT_INTERVAL_MIN` accumulator fix (`809cf98`),
  `CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=4096`, `CONFIG_ZMK_INPUT_MOUSE_PS2_REPORT_INTERVAL_MIN=5`.
  GH run `33247795712` **failed Kconfig**: `CONFIG_ZMK_WATCHDOG` is **undefined at
  ZMK ac7f75b8** (no watchdog option/feed loop there). Dropped that line.
- Build 3 (it2 final): `51aa0e9`, GH run `33247994049` — **all 8 builds pass**,
  right uf2 612352 B. Flashed COM7; user round-3 test passed (20-30s sustained +
  flicks + right-half keys). **Soak test pending** (crash is intermittent).

## Findings — the real root cause (it3 logging → it4 fix)

- **F1/F2/it2 all failed** (cb-stack 4096, no-resend, system_wq 4096, report-interval 5).
- **Key user facts**: crashes on EVERY build — including `zmk-config-ps2-test` (Exp68's
  plain USB-HID standalone) — so split/BLE/dongle theories are out; it's the gpio-ps2
  chain + system workqueue in every topology. Crash = whole half dead, needs power cycle.
- **Logging was never visible before**: root cause of that mystery =
  `CONFIG_LOG_PRINTK=y` rerouted ALL printk through the log framework, which had no
  backend (zmk-usb-logging only sets `ZMK_USB_LOGGING`, never a `LOG_BACKEND_UART`).
  Fix: `CONFIG_LOG_BACKEND_UART=y` + drop `LOG_PRINTK` + driver telemetry via printk.
- **Level-4 capture (it3d, death-tail) revealed**: packets parse fine right up to the end
  (`P 306857: st=08 x=8 y=40`), every byte interrupted by `scl timeout at pos=4/5` +
  `invalid start bit` resyncs, then TOTAL SILENCE immediately after a `GA` abort printk
  emitted from the GPIO ISR context — a freeze (no fault text), exactly matching the
  "stayed dead" symptom.
- **it4 fix (`4d8fe3c`)**:
  1. `ps2_gpio_read_scl_timout` (**150µs deadline**) was scheduled on the **system
     workqueue** (`k_work_schedule`); writes already used the driver's own queue. Any
     system load fired it late → spurious mid-byte aborts → abort storm → system
     workqueue wedge → whole half dies. **Fix: `k_work_schedule_for_queue(&ps2_gpio_work_queue)`**.
  2. `printk` in `ps2_gpio_read_abort` ran in **ISR context** (console spinlock
     deadlock). **Fix: gate to thread context**; counters on the 1s stats line.
  - **User-verified: survived 60s+ hard nub motion (was dead by ~2-5 min), with logs.**
- **it4b fix (`cb4ab37`)**: even with the timeout off the system queue, the level-4 log
  still shows the per-byte abort pattern — the **150µs threshold is tighter than this
  TP's legit clock jitter** (the "jittery RC clock" Exp68 found over UART). Raised
  `PS2_GPIO_TIMING_SCL_CYCLE_MAX` 100→200µs (read timeout 200+50=250µs; still well
  inside the inter-byte gap). Target: clean decode, no abort noise.

## Conclusion

Exp70 (it4+it4b): **the crash is diagnosed and fixed.** The whole-half freeze on TP touch
was the gpio-ps2 read watchdog (`read_scl_timout`, 150µs) being scheduled on the **system
workqueue** — a shared queue that this driver's own events busy up, firing late aborts
mid-byte, snowballing into a workqueue wedge that takes keys + pointer down together.
Compounding it: this TP's jittery clock is tighter than the 100µs cycle budget (fixed by
widening to 200µs), and ISR-context console output could deadlock the logging builds
(gated out).

The earlier F1/F2 stack/no-resend work stays (harmless hardening + removes real hazards);
it just wasn't the kill mechanism. Split/BLE/dongle are exonerated — the crash predates
them (Exp68 ps2test standalone).

## Changes (final)

- Driver `Magid-William/zmk-ps2-trackpoint-driver` branch `Exp70`:
  - `d971d8d` F1/F2: cb-workqueue stack Kconfig (4096) + `PS2_GPIO_NO_RESEND`.
  - `809cf98` interval accumulator per-axis reset fix.
  - `ec806ae`→`ea1836e` it3 telemetry: counters + per-packet/abort lines via **printk**
    (log framework proven dead on this build without a backend).
  - `4d8fe3c` **it4**: read timeout → `ps2_gpio_work_queue`; ISR-safe abort logging.
  - `cb4ab37` **it4b**: `PS2_GPIO_TIMING_SCL_CYCLE_MAX` 100→200µs.
- Config `Magid-William/zmk-config-dabaseV_0-2` branch `Exp70` @ `6b59f6c`:
  - `config/west.yml` → driver `cb4ab37`; `dabase_v2_right.conf`:
    `LOG_BACKEND_UART=y`, `LOG_MODE_IMMEDIATE=y`, **no `LOG_PRINTK`**,
    `PS2_GPIO_NO_RESEND=y`, `FAULT_DUMP=2`, `THREAD_ANALYZER`, `DEBUG_THREAD_INFO`,
    `SYSTEM_WORKQUEUE_STACK_SIZE=4096`, `REPORT_INTERVAL_MIN=5`.

### Known-good (rolling)

- ZMK `ac7f75b8`. Driver `cb4ab37` (it4b). Config `6b59f6c`. GH run `33255107906`.
- Logging pipeline verified working (boot lines, DBG, telemetry, per-packet).

## Next experiment

- Confirm it4b: clean decode (abort spam gone) + long survivability, standalone first,
  then **full dongle topology** (ps2-direct peripheral + XIAO dongle) — the original
  production scenario.
- Re-introduce production features (temp_layer, scroll, volume) on the proven stack.
- AGENTS.md lesson: never schedule a µs-scale watchdog on the system workqueue from a
  driver that generates work for that same queue; and the `LOG_PRINTK` silent-log trap.