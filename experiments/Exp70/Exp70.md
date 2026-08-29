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

## Final root cause (it6 — the kill mechanism)

**The crash was Zephyr's dedicated `input` thread defaulting to a 1024-byte stack.**
`INPUT_MODE_THREAD` is the Zephyr default: every input event is processed by ONE global
`input` thread, and the registered callbacks execute inside it. Our callback is the
pointer pipeline: `input_process → (zmk,input-split) split_input_handler →
zmk_split_peripheral_report_event → zmk_split_bt_report_input → bt_gatt_notify`
(or listener → HID on non-split). Zephyr's own help text: the stack "must have enough
space for executing the registered callbacks". A 1KB stack with that deep BT/HID chain
**overflowed on the first pointer event** — silent, whole-half death, needs power cycle.
This is why it happened in EVERY build and topology since Exp68 (even the plain
USB-HID ps2-test standalone) and why it was 100% correlated with touching the nub
(11+ min idle = fine; very first byte 0x8 arrives → death).

Secondary/contributing defects fixed along the way:
- `ps2_gpio_read_scl_timout` (150µs deadline) was scheduled on the **system workqueue**;
  any load fired it late → mid-byte abort storm (`scl timeout at pos=4/5`). Moved to the
  driver's own queue (`4d8fe3c`).
- This TP's clock **pauses mid-byte** for hundreds of µs–ms (the AVR decoder needed a
  ~7.5ms read-byte timeout, Exp58). Widened the cycle budget to 8ms (`5fbc21f`) → abort
  storm gone.
- The backend still wrote 0xFE resends to a command-rejecting TP — suppressed
  (`PS2_GPIO_NO_RESEND`, F2).
- ISR-context `printk` (console spinlock deadlock) — gated out.
- `kernel thread stacks` (idle) showed `HID Over GATT Send Work` and `Low Priority Work
  Queue` both **100% / 768**, `usbd_workq` 93% — output-side stacks raised.

## Conclusion

**Exp70: SUCCESS (it6).** The right-half whole-system crash on trackpoint touch is fixed
and **user-confirmed on the it6 ps2-direct build: "it's solid now, it didn't crash."**
The killer was the 1KB Zephyr `input` thread overflowing inside the pointer-forward
callback chain — now 4096 bytes (plus queue 16→64). Combined driver/config hardening:
read watchdog off the system workqueue, 8ms clock tolerance, no-resend, ISR-safe logging,
and the logged exhausted output-thread stacks (768s → 2048).

## Changes (final)

- Driver `Magid-William/zmk-ps2-trackpoint-driver` branch `Exp70` @ `5fbc21f`:
  - `d971d8d` F1/F2: cb-workqueue stack Kconfig (4096) + `PS2_GPIO_NO_RESEND`.
  - `809cf98` report-interval accumulator per-axis reset.
  - `ec806ae`→`ea1836e` it3 printk telemetry (counters, per-packet lines).
  - `4d8fe3c` it4: read timeout off the system workqueue; ISR-safe abort logging.
  - `5fbc21f` it4c: `PS2_GPIO_TIMING_SCL_CYCLE_MAX` → 8ms (this TP pauses mid-byte).
- Config `Magid-William/zmk-config-dabaseV_0-2` branch `Exp70` @ `7e9d97b`:
  - `config/west.yml` → driver `5fbc21f`.
  - `config/dabase_v2_right.conf`:
    - **`CONFIG_INPUT_THREAD_STACK_SIZE=4096` (THE fix) + `CONFIG_INPUT_QUEUE_MAX_MSGS=64`**
    - `ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=2048`, `ZMK_BLE_THREAD_STACK_SIZE=2048`
    - `LOG_BACKEND_UART=y`, no `LOG_PRINTK`, deferred logging (no immediate-mode flood)
    - `PS2_GPIO_NO_RESEND=y`, `FAULT_DUMP=2`, `THREAD_ANALYZER`, `DEBUG_THREAD_INFO`,
      `SYSTEM_WORKQUEUE_STACK_SIZE=4096`, `REPORT_INTERVAL_MIN=5`
  - `build.yaml`: ps2-direct cmake-args includes `ZMK_SPLIT_BLE_PERIPHERAL_STACK_SIZE=2048`.

### Known-good (final)

- ZMK `ac7f75b8`. Driver `5fbc21f`. Config `7e9d97b`. GH run `33256681738` (8/8).
- Flashed: `dabase_v2_right-ps2-direct.uf2` (612864 B) — **user-confirmed solid**.

## Next experiment

- **Full dongle topology soak** — ps2-direct right half + XIAO dongle + left half, pointer
  + keys, extended session → the production scenario (it6 standalone/ps2-direct held, but
  the dongle adds the split-forward + central side).
- **Cleanup for production**: strip the per-packet `P`/`GA` printk telemetry (keep the 1s
  counter line + LOG_* path), then re-introduce temp_layer/scroll/volume on the proven stack.
- **AGENTS.md lessons to record**: (1) Zephyr input events run through the 1KB `input`
  thread — a driver whose callbacks do BT/HID must raise `CONFIG_INPUT_THREAD_STACK_SIZE`;
  (2) never schedule a µs-scale watchdog on the system workqueue from a driver that feeds
  that same queue; (3) `LOG_PRINTK` reroutes printk into a dead log framework — the reason
  "nothing ever logged" across the project; `LOG_BACKEND_UART` is required; (4) this TP's
  clock pauses mid-byte — the decoder must tolerate ~8ms (AVR knew this, Exp58).