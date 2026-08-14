# Exp46: Prove the serial monitor path (CH340G, 9600) + raw PS/2 byte capture

## Hypothesis

The serial monitor reading is a **hardware-path** problem, not a firmware problem. The Arduino Nano (used as USB passthrough) was burned — it overheated from a wire fault and produced garbage serial reads while still allowing flashing. The dedicated CH340G STC programmer at **9600** baud should read clean, and raw PS/2 bytes from the TrackPoint should be capturable over that same serial path.

## Plan

1. Bare `serial-test` sketch — nothing but `Serial.println(millis())` every 500ms at 9600 → prove the CH340G read path end-to-end.
2. `trackpoint-ps2-bytes` sketch — raw bit-banged PS/2 byte reader (start bit, 8 data bits LSB-first, parity/stop ignored) printing each byte as 8 binary digits at 9600 → prove TrackPoint data flows on the wired pins (CLK=D7, DAT=D3).
3. Both sketches committed on branch `Exp46` → GH Actions `compile.yml` builds → artifacts `pro-mini-serial-test`, `pro-mini-ps2-bytes`.
4. Flash via CH340G + WSL Alpine avrdude, read via the proven `stty ... 9600 ... -hupcl; cat /dev/ttyUSB0` command.

## Result

- **Serial path proven**: bare `serial-test` outputs clean, continuous millis timestamps (`0, 499, 999, 1499, 2000...`) at 9600 — zero corruption. Run `31026867560`, 1802 B written + verified.
- **Raw PS/2 stream proven**: `trackpoint-ps2-bytes` captures the TrackPoint byte stream live — mostly `11111111` (0xFF idle) interleaved with status/data bytes (`11111101`, `10111111`, `11101111`, `11111110`). Run `31027675525`, 2038 B written + verified. The TrackPoint streams on power-up, no init needed.
- Flash sizes verified each time. `-carduino` always hung (DTR toggling ioctl via usbipd); `-c stk500v1` flashed successfully **hands-free** — the CH340G's auto-reset circuit pulses on port open, so no reset button was ever pressed.

## Findings

- **9600 is the proven baud** on this 8MHz Pro Mini. 115200 has an ~8.5% baud error (real rate ≈111111 with U2X) — the cause of all earlier garbage reads.
- **`-c stk500v1` is hands-free.** `-carduino` hangs because it actively toggles DTR (`ioctl("TIOCMSET") Device timeout`) which usbipd chokes on; `stk500v1` never touches DTR after open, and the CH340G's DTR→100nF→RST circuit pulses reset on port open into the bootloader window — no button needed.
- **The user ("Me") burned the Arduino Nano.** It overheated from a wire fault (since fixed); a thermally degraded CH340 produces garbage on continuous streams while short flash frames still pass — exactly the "flashes fine but serial is garbage" signature. Flashing success never proved the Nano's serial path was healthy because flashing uses a *different* adapter (CH340G).
- The dedicated CH340G STC programmer is the **only** read+flash path going forward (it's bidirectional; RXD←TXO is the read line, already wired for the bootloader ACK).
- Library `readPacket` printed no X/Y over serial even though raw bytes flow — points to a library-level sync/parse issue, not hardware or wiring.

## Conclusion

**Success.** Serial monitor reading works cleanly at 9600 via the dedicated CH340G, and the raw PS/2 byte stream is captured live. The earlier garbage was the burned Nano + wrong baud, not the firmware. Serial link, flashing path, and TrackPoint data flow are all proven.

## Next steps

1. Debug the library `readPacket` X/Y path: raw bytes flow but no parsed packets print — check the CLK-high sync / packet-boundary logic (Exp43 `_syncIdleCount`) against the observed stream.
2. If needed, feed the raw captured bytes (Exp46) into a parser to identify packet framing (status/X/Y/parity) and validate `readPacket`'s assumptions.
3. Remove the burned Nano from the bench; the CH340G covers all flashing + serial needs.
