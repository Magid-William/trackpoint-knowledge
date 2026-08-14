# Exp17: I2C Handshake Sanity — Pro Mini I2C Slave + ZMK I2C Master

## Hypothesis

Switching from SPI to I2C between the Pro Mini and NiceNano V2 will establish a reliable handshake on the first attempt. Unlike SPI (Exp02), I2C has built-in clock stretching, multi-byte framing, and ACK/NACK — the AVR's single-buffered USART/TWI hardware handles slave mode properly without the byte-gap race that plagued SPDR.

## Plan

Minimal I2C sanity check: Pro Mini as an I2C slave device with a few readable registers; ZMK as I2C master that reads them and logs the values. If the bus handshake works, we'll see expected bytes in the logs. No motion data, no PS/2 integration yet.

### Wiring

| Pro Mini | I2C | NiceNano V2 | Note |
|----------|-----|-------------|------|
| **D18 (A4)** | SDA | **P0.17** | 4.7kΩ pull-up to 3.3V |
| **D19 (A5)** | SCL | **P0.20** | 4.7kΩ pull-up to 3.3V |
| **D14 (A0)** | MOT | **P0.06** | Keep IRQ |
| GND | GND | GND | |

Freed: D10/D11/D12/D13 (Pro Mini old SPI), P0.08/P0.10 (NiceNano old SPI).

### Pro Mini Side: I2C Slave Firmware

- I2C slave address `0x42`
- Register file: `reg[0x00]=0x3E` (mock Product ID), `reg[0x02-0x04]` mock motion bytes
- `requestEvent` sends `reg[addr]`, `receiveEvent` captures register address writes
- MOT pulses at 20ms interval

### ZMK Side: New I2C Driver

- New binding `promini,trackpoint-i2c` (includes i2c-device.yaml)
- Minimal driver: 100ms polling timer reads register 0x00 over I2C, prints via printk
- Overlay: replace `&spi0` with `&i2c0`, i2c0 pinctrl on P0.17/P0.20
- .conf: `CONFIG_I2C=y` instead of `CONFIG_SPI=y`

### Protocol

- Master writes 1 byte (register address) → slave stores it
- Master reads N bytes → slave sends `reg[addr]` through `reg[addr+N-1]`

## Files

| File | Action |
|------|--------|
| `zmk-pmw3610-driver/dts/bindings/promini,trackpoint-i2c.yml` | Create |
| `zmk-pmw3610-driver/src/trackpoint-i2c.c` | Create |
| `zmk-pmw3610-driver/CMakeLists.txt` | Edit |
| `zmk-pmw3610-driver/Kconfig` | Edit |
| `corne_trackpoint_right.overlay` | SPI → I2C |
| `corne_trackpoint_right.conf` | I2C Kconfig |
| `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino` | Create |

## Success Criteria

- [x] Builds pass (both repos)
- [x] I2C read returns 0x3E from reg 0x00
- [x] No NACK errors or bus lockups

## Findings

1. **I2C works, SPI was the wrong choice:** The AVR TWI slave handles combined transactions (write addr + repeated START + read data) correctly, unlike the SPI SPDR single-buffer race that plagued Exp02–Exp03.

2. **Initial wiring issues:** The SDA/SCL lines had a faulty wire (loose VCC, incorrect physical pin routing). After fixing, the Pro Mini appeared at 0x42 on the first scan.

3. **MOT interrupt not firing on nRF:** `GPIO_INT_LEVEL_ACTIVE` with `GPIO_ACTIVE_LOW` on P0.10 didn't trigger the GPIO callback. Switched to a 100ms polling timer which works reliably. Root cause: likely nRF SENSE/ PORT event configuration mismatch — worth investigating for Exp18.

4. **Polling is stable:** 100ms timer produces clean I2C reads with zero errors. The driver reads `reg[0x00]=0x3E` every poll cycle.

5. **Firmware size:** ZMK UF2 grew from 630KB (Exp16, no I2C) to 636KB with I2C driver + shell support.

## Conclusion

Exp17 succeeded. I2C is a viable replacement for SPI between the Pro Mini and NiceNano. The bus handshake is reliable, clock stretching works automatically, and the `i2c_write_read_dt` combined transaction pattern is supported by the AVR TWI hardware.

The MOT interrupt issue on nRF needs further investigation but is not blocking — polling is acceptable for this experiment.

## Wiring (Final)

| Pro Mini | I2C | NiceNano V2 |
|----------|-----|-------------|
| D18 (A4) | SDA | P0.17 |
| D19 (A5) | SCL | P0.20 |
| D14 (A0) | MOT | P0.06 |
| GND | GND | GND |

Both SDA and SCL: 4.7kΩ pull-ups to 3.3V.

## Next Steps

Exp18: Integrate PS/2 trackpoint reading on the Pro Mini, expose real X/Y motion data over I2C registers, and add motion reporting (input_report) to the ZMK driver.
