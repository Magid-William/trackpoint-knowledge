# Exp68: badjeff's PS/2 driver, never-send-commands mode, trackpoint DIRECT to nice_nano

## Hypothesis

The weird generic USB trackpoint streams standard 3-byte PS/2 packets on power-up and
rejects all host commands (Exp06). badjeff's `kb_zmk_ps2_mouse_trackpoint_driver`
(UART-trick `ps2_uart` @ 9600 on nRF52) is proven working on a nice_nano via justinmklam's
dabao keyboard. If we strip **every** host→device command from that driver, the trackpoint
still streams, the nRF decodes it directly, and the cursor moves with **no AVR coprocessor**.

## Plan

1. Fork driver `badjeff/kb_zmk_ps2_mouse_trackpoint_driver` @ `2df4d6d`
   (the exact SHA justinmklam's verified combo pins; diff to current main is only an
   unrelated `rst-gpios` rename) → `Magid-William/zmk-ps2-trackpoint-driver`, branch `Exp68`.
2. Add `ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS` (default n):
   - `input_mouse_ps2.c`: init thread skips self-test/reset/detect/config handshake —
     waits 1500 ms for the power-up stream, then enables the data callback **without**
     the 0xF4 write; misalignment "resend" (0xFE) becomes packet-buffer reset only;
     enable/disable reporting never write.
   - `ps2_uart.c`: `ps2_write()` refuses all writes (-ENOTSUP) — belt and braces.
3. New config repo `Magid-William/zmk-config-ps2-test` (unified template layout), branch
   `Exp68`:
   - ZMK pinned `ac7f75b8` (justinmklam's verified revision)
   - shield `ps2test_right` (bench 4x6 matrix = corne_trackpoint right wiring):
     - `ps2test_trackpoint.dtsi` — justinmklam's dabao_trackpoint.dtsi verbatim
       (pins/uart0 pinctrl/priorities/9600) + `bias-pull-up` on the UART RX pin
       (nRF UART RX has no internal pull; Exp06 proved a weak pull suffices — no
       external resistors)
     - no tp-* / sampling-rate / scroll-mode props, no rst-gpios, buttons enabled
     - standard `zmk,input-listener` → `&tpoint0` (no input processors for bring-up;
       temp_layer/axis transforms later)
     - `.conf`: PS2 + UART_INTERRUPT_DRIVEN + ZMK_POINTING/MOUSE + NEWLIB (their
       picolibc fix) + shell/logging rule + DBG levels + `NO_HOST_COMMANDS=y`
     - `snippet: zmk-usb-logging` + settings_reset build (AGENTS.md rule)
4. Hardware (bench rig): TP DAT→P0.06, CLK→P0.08, RST float, VCC 3.3 V rail.
5. Build via GH Actions; flash bench nice_nano (COM8); verify via shell/DBG logs.

## Wiring

| TrackPoint | NiceNano v2 (pro_micro map @ ac7f75b8) |
|---|---|
| DAT | P0.06 (pro_micro 1) — UART RX, internal pull-up |
| CLK | P0.08 (pro_micro 0) |
| RST | float |
| VCC/GND | 3.3 V rail / GND |

## Status

- [x] Driver fork + `NO_HOST_COMMANDS` mode (commit `4f23ac0`, branch `Exp68`)
- [x] Config repo scaffolded + pushed (commit `998e84d`, branch `Exp68`) — build run
      `33190992394`
- [ ] Build green
- [ ] Flashed on bench nice_nano (COM8)
- [ ] DBG logs: packet decodes present, zero command sends
- [ ] Cursor moves when touching the nub (no coprocessor)
- [ ] Left/Middle/Right buttons click
- [ ] No 0xFE resend spam, no drift

## Known-good revisions (fill in)

- ZMK: `ac7f75b8`
- Driver fork (Exp68): `4f23ac0`
- Config (Exp68): `998e84d`

## Conclusion

TBD — fill in after bench verification.