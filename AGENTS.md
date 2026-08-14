# Trackpoint ZMK project

A PS/2 trackpoint exposed to ZMK over I2C:

```
NiceNano (ZMK) <-I2C @0x42-> Pro Mini / ATtiny85 (I2C slave) <-PS/2-> Trackpoint
```

ZMK runs the `promini,trackpoint-i2c` driver (repo `zmk-trackpoint-driver`) as
I2C master. The Pro Mini / ATtiny85 is the slave at address `0x42`, reads the
PS/2 trackpoint, and streams X/Y to the driver.

**This repo is the project memory** (the experiment system): `AGENTS.md`
(rules + hard-won lessons), `Experiments.md` (status index), `experiments/`
(details per experiment), plus the two flash/read tools and `opencode.json`.
No firmware or ZMK code lives here — it lives in the sub-repos below.

# Repos

| Repo | What it is |
|---|---|
| `trackpoint-knowledge` | **This repo** — memory system + scripts + opencode config |
| `zmk-trackpoint-driver` | ZMK I2C driver `promini,trackpoint-i2c` (no PMW3610 remains) |
| `zmk-trackpoint-shield` | corne_trackpoint shield (right half = trackpoint), GH Actions |
| `zmk-config-dabaseV_0-2` | **ACTIVE** dabase_v2 shield config (right half = trackpoint) |
| `promini-trackpoint` | Pro Mini firmware (PS2Trackpoint lib, i2c slave, diag sketches) |
| `attiny85-trackpoint` | ATtiny85 sketches + self-contained libraries (ISP/serial guides in its README) |

# CRITICAL: Every build must have USB logging

These configs must be on every build:

```
CONFIG_I2C_SHELL=y
CONFIG_SHELL=y
CONFIG_KERNEL_SHELL=y
CONFIG_LOG=y
snippet: zmk-usb-logging
```

Including `settings_reset`. Without a shell, headless bootloader entry via
serial is impossible.

# CRITICAL: AVR TWI needs combined I2C transactions

The driver must use combined `i2c_write_read_dt()` (repeated START). Split
write/read transactions break AVR TWI on the Pro Mini. Also: the 8 MHz Pro Mini
has ~8.5% baud error at 115200 — debug serial must be 9600.

# Rules

- Everything you need is in `D:\DIY\trackpoint\promini-zmk\`
- Never visit any other folder unless it's `G:\` to copy uf2 to.
- Code changes happen only in the sub-repos; this repo only tracks memory/docs.

# Background

The trackpoint is unusual ([why it needs this decoder](https://randalea.de/~db7/crunch-ps2-decoder.html))
and was verified working at 3.3V (no level shifting). PS/2 is not friendly to
the nRF52840, so a small AVR sits in between and translates it. The link used
to be SPI (Exp01-Exp16, PMW3610 emulation) but that path was abandoned — I2C
master/slave (`promini,trackpoint-i2c`) is the transport since Exp17.

# Goals wishlist

Open:

- [ ] It works perfectly on USB [buggy]
- [ ] Works in BLE
- [ ] Works over dongle topology
- [ ] If battery still drains fast, consider a third peripheral with its own battery

Done (see `Experiments.md` for details):

- [x] Layer-toggle when touching the trackpoint (Exp15/20/29 — `temp_layer` input processor)
- [x] Sleep on inactivity; auto-cut trackpoint power on inactivity (Exp49 — ZMK deep sleep)
- [x] MOT data-ready level instead of polling (Exp64)
- [x] Driver rename + cleanup: PMW3610 gone, I2C-only (Exp65)

# Development — experiments are the memory system

Every experiment is a branch named `ExpNN` (one per repo that has changes), so
any past state can be rolled back to and re-run. The memory system:

- `Experiments.md` — one table row per experiment: number, status, hypothesis,
  result. Minimal on purpose: it tells you where you are and what was tried.
- `experiments/ExpNN/ExpNN.md` — the detail file. Start it with the hypothesis
  and the plan, update it with findings as you go, and finish with a conclusion
  + a suggestion for the next experiment.

Rule: a new experiment = a new branch + a new `ExpNN.md` + an `Experiments.md`
row. Start numbering where the last one ended.

## Phase 1 — the ZMK side

### Building

Active shield config: `zmk-config-dabaseV_0-2` — right half is
`dabase_v2_right`. Secondary: `zmk-trackpoint-shield` (`corne_trackpoint_right`).

Build via GitHub Actions in the respective repo. Using `gh` we're logged in, so
use it to get the pipeline status and read error details. Always wait exactly
2.5 minutes, then check; if still running, keep waiting 30s and re-check until
done.

### Flashing

Enable the serial shell:

```ini
CONFIG_SHELL=y
```

For USB logging use `snippet: zmk-usb-logging` in `build.yaml` (the old
`CONFIG_ZMK_USB_LOGGING=y` no longer works in ZMK main / Zephyr 4.1).

#### Via flash-nicenano.ps1 (recommended)

```powershell
# Find the correct COM port first. The interface hosting the shell is NOT
# reliable across builds — probe all COMs until one answers uart:~$
function probeShell($portName) {
  $p = New-Object System.IO.Ports.SerialPort $portName,115200,None,8,1
  $p.ReadTimeout=800; $p.DtrEnable=$true; $p.RtsEnable=$true; $p.Open(); Start-Sleep 1
  $p.Write("`n"); Start-Sleep 1; $r=$p.ReadExisting(); $p.Close(); return $r
}
probeShell COM<X>   # should show "uart:~$"

.\flash-nicenano.ps1 -UF2Path "build\firmware\dabase_v2_right-promini-i2c.uf2" -ComPort COM<X>

# XIAO dongle — drive label is XIAO-SENSE (NOT XIAO-BOOT!)
.\flash-nicenano.ps1 -UF2Path "build\firmware\dabase_v2_dongle-with_studio.uf2" -ComPort COM<X> -DriveName XIAO-SENSE
```

> XIAO drive label is `XIAO-SENSE` (verified 2026 — bootloader enumerates as
> Seeed VID_2886/PID_0045 with a 32MB volume). The script asserts DTR/RTS for
> the XIAO shell; harmless for the NiceNano. Port can change after reboot.

#### Manual (headless GPREGRET)

```powershell
$port = New-Object System.IO.Ports.SerialPort COM<X>,115200,None,8,1
$port.ReadTimeout = 1000; $port.Open()
Start-Sleep 1
$port.WriteLine("devmem 0x4000051C 32 0x00000057")
Start-Sleep 1
$port.WriteLine("kernel reboot cold")
$port.Close()
# Wait for NICENANO/XIAO-SENSE drive (~4s), then copy UF2 to it
Copy-Item "path\to\firmware.uf2" "G:\"
```

> `kernel reboot bootloader` was removed in Zephyr 4.1. Writing `0x57` to
> GPREGRET (0x4000051C) sets the magic value the Adafruit bootloader checks.

### Wiring (I2C — no power gates)

The Pro Mini is powered directly from the nice_nano VCC rail (P0.13 switch).
There is **no** power-gating circuitry. When ZMK enters deep sleep
(`CONFIG_ZMK_SLEEP=y`), the rail drops and the Pro Mini draws nothing (verified
with a multimeter). This is why the driver stays in plain polling mode when no
`irq-gpios` is set — MOT from an unpowered slave is meaningless.

| NiceNano | Signal | Pro Mini |
|---|---|---|
| P0.17 | SDA | D18 (A4) |
| P0.20 | SCL | D19 (A5) |
| GND | GND | GND |

- Both SDA and SCL: 4.7kΩ pull-ups to 3.3V.
- Trackpoint PS/2 (see `PS2Trackpoint.md`): Pro Mini GND→GND, ACC→VCC,
  D3→SDA (DAT), D7→SCL (CLK), RST must stay float.

## Phase 2 — the AVR side

`promini-trackpoint` / `attiny85-trackpoint` hold the firmware. The
`PS2Trackpoint` library handles PS/2 X/Y reads; the I2C slave sketch exposes
X/Y at `0x42` with the driver's burst protocol (`0x12` destructive read, `0x03`
debug read, `0x11/0x13/0x15/0x17` speed/curve regs). Driver changes require an
experiment note — the driver must stay protocol-compatible.

### Building & flashing the Pro Mini

The Pro Mini is flashed via a standalone CH340G USB TTL programmer (3.3V):
TXD→RXI, RXD→TXO, GND→GND, 3.3V→VCC, DTR→RST via 100nF (auto-reset).

- Build `.ino` sketches via GitHub Actions (download the `.hex` artifact), or
  compile simple C directly with Alpine's avr-gcc in WSL.
- Attach the CH340G to WSL: `usbipd attach --wsl --busid 1-3`.
- Flash: `avrdude -C /etc/avrdude.conf -patmega328p -c stk500v1 -P /dev/ttyUSB0 -b 57600 -D -U flash:w:<hex>:i`
  > **PROVEN (Exp46): use `-c stk500v1`** — hands-free. The DTR→100nF auto-reset
  > pulses on serial open; avrdude syncs inside the bootloader window. `-carduino`
  > hangs (actively toggles DTR, usbipd chokes on it).
- Read debug serial at **9600 baud** with `-hupcl`:
  `stty -F /dev/ttyUSB0 raw 9600 cs8 -cstopb -parenb -hupcl; timeout 8 cat /dev/ttyUSB0`

# Roll back

Every experiment is a branch, so any experiment can be re-run at will. Each repo
keeps its own experiment branches.

# Success

The handshake between the Pro Mini/ATtiny85 and the NiceNano is working, data is
flowing, and the cursor moves on screen when the trackpoint nub is touched.

# Resources

zmk.dev is blocked, so use `https://web.archive.org/web/20260626150914`:

- https://web.archive.org/web/20260626150914/https://zmk.dev/docs/development/local-toolchain/setup
- https://web.archive.org/web/20260626150914/https://zmk.dev/blog/2025/12/09/zephyr-4-1
- https://web.archive.org/web/20260626150914/https://zmk.dev/docs/hardware-integration/new-shield
- https://web.archive.org/web/20260626150914/https://zmk.dev/docs/development/module-creation
- https://web.archive.org/web/20260626150914/https://zmk.dev/docs/features/pointing
- https://web.archive.org/web/20260626150914/https://zmk.dev/docs/development/usb-logging
- https://randalea.de/~db7/crunch-ps2-decoder.html
