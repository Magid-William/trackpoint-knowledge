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

(WIP — fill in after hardware test.)

## Conclusion

(WIP.)

## Next experiment

(Crash persists: decode the HARD FAULT dump → thread/stack pinpoints H1 vs H3
(zmk,input-split/BT). Crash gone: re-introduce temp_layer/scroll on the proven
stack + document gpio-ps2-vs-uart-ps2 in AGENTS.md.)