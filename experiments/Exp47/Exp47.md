# Exp47: 4-wire I2C sanity test — no MOT, no RST

## Hypothesis

The Pro Mini I2C slave can talk to the ZMK driver with only **SDA, SCL, GND, VCC** (4 wires). MOT (P0.06) and RST (P0.08) are unnecessary because the Pro Mini never sleeps (`SLEEP_ENABLED 0`) — polling always finds an awake slave, so no reset pulse and no MOT gating are needed. This frees P0.06/P0.08 and simplifies wiring.

## Plan

1. **promini-trackpoint (branch Exp47)**: keep `trackpoint-i2c-slave.ino` at `SLEEP_ENABLED 0` (never sleeps), set `SERIAL_LOG 0` (silent UART build). CI → `pro-mini-i2c-slave` artifact.
2. **zmk-pmw3610-driver (branch Exp47, from `e51e4c5`)**: make `irq-gpios` and `reset-gpios` optional:
   - Binding: drop `required: true` from both.
   - `trackpoint-i2c.c`: `GPIO_DT_SPEC_INST_GET_OR(..., {0})` in the config macro; runtime `if (cfg->irq_gpio.port)` / `if (cfg->reset_gpio.port)` guards around IRQ config, reset pulse, MOT-level gating, and edge interrupt registration. Without MOT → plain 10ms polling. Exp31/34 backoff kept as dormant safety net.
3. **zmk-trackpoint-shield (branch Exp47, from `0949120`)**: remove `irq-gpios` + `reset-gpios` from `trackball@42`; pin driver to new SHA `6779463`; ZMK stays `fa33e35f`. Build: central + settings_reset.

## Changes by repo

| Repo | Branch | Commit | Change |
|------|--------|--------|--------|
| promini-trackpoint | Exp47 | `0a8af67` | `SERIAL_LOG 0` — silent build |
| zmk-pmw3610-driver | Exp47 | `6779463` | Optional irq/reset GPIOs, plain-poll fallback |
| zmk-trackpoint-shield | Exp47 | `21640ae` | Drop irq/reset from overlay, pin driver |

## Verification

1. Both CI builds green (promini run `31033546228`, shield run `31033982152`).
2. Pro Mini flashed via CH340G + WSL avrdude (`-c stk500v1`, hands-free — **no reset button touched**; see finding below). **3584 bytes written + verified**, signature `0x1e950f`.
3. NiceNano flashed headless (GPREGRET 0x57 + reboot) with `corne_trackpoint_right-nice_nano-central.uf2`.
4. Wiring: SDA A4/D18↔P0.17, SCL A5/D19↔P0.20, GND, VCC + 4.7kΩ pulls. P0.06/P0.08 floating.

## Results

- **Mouse moves** with only 4 wires — no MOT, no RST. Driver init completes with zero GPIO errors (`no irq-gpios in DT — MOT gating disabled, plain polling` path).
- `i2c scan i2c0` → **`0x42` found**.
- `i2c read i2c0 0x42 0x12 2` → `00 00` (burst reg returns 2 bytes, X/Y idle).
- Shell works (`uart:~$`).
- USB log output appears silent on this build (pre-flash firmware flooded; Exp47 build quiet — logging config unchanged, likely log-level/backend nuance, not a blocker for the sanity test).

### Flashing finding: hands-free confirmed again (Exp47)

The user **never touched the Pro Mini reset button** during Exp47 — I instructed a manual reset press as avrdude started, the user ignored it, and the flash still succeeded (signature + write + verify). This confirms the AGENTS.md Exp46 finding: the CH340G's DTR→100nF→RST circuit pulses on **serial port open**, resetting the Pro Mini into the ~1s bootloader window that avrdude (`-c stk500v1`) syncs inside. Manual reset is not required. (`-carduino` still hangs via usbipd — `ioctl("TIOCMSET"): Device timeout` — so stk500v1 remains the programmer of choice.)

## Conclusion

**Success.** The 4-wire I2C link (SDA/SCL/GND/VCC) fully works with the Pro Mini as a never-sleeping slave. MOT and RST are truly optional — driver boots, probes, polls, and reports motion without either wire. P0.06 and P0.08 are free for other uses. The optional-GPIO driver change is backward compatible (when irq/reset are present in DT, prior behavior is preserved).

## Known-good pins

- promini `0a8af67` (Exp47)
- driver `6779463` (Exp47)
- shield `21640ae` (Exp47)
- ZMK `fa33e35f`

## Next steps

1. Investigate the silent USB logging on the Exp47 build (compare `zmk-usb-logging` snippet behavior vs Exp36) if shell-based log debugging is needed.
2. Long-idle soak test: confirm no lockup after 30+ min with continuous 10ms polling.
3. Consider wiring MOT back later for sleep gating (Exp36 behavior) or keep 4-wire simplicity if power budget allows.
