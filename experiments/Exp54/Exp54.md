# Exp54: ATtiny85 PS/2 serial reader — trackpoint X/Y via Leonardo bridge

## Hypothesis

The Exp53 read path (ATtiny85 soft-serial TX on PB0 → Leonardo SoftwareSerial
bridge → USB CDC) works with a real PS/2 trackpoint sketch, not just a
`millis()` heartbeat. The ATtiny85 reads the trackpoint with the proven
PS2Trackpoint library (Exp41–44 pins CLK=3/DAT=4, 8MHz prescale) and prints
X/Y @9600 on PB0. This validates PS/2 parsing standalone on the ATtiny85,
isolated from the I2C slave — Exp39's one-axis failure must NOT recur.

## Plan

1. New sketch `trackpoint-serial-attiny85.ino` (Exp45 promini reader +
   Exp42 OLED pin map). X/Y only, no buttons.
2. New PlatformIO project `attiny85-trackpoint-serial/` (attiny85 @ 8MHz,
   `stk500v1`, `lib_extra_dirs = ../libraries`).
3. Build → flash via Leonardo ISP (Exp53 path) → reflash Leonardo as bridge
   → read COM9.

## Wiring (Leonardo 5V bench — Exp53 verbatim, trackpoint at 5V per user choice)

| Leonardo | ATtiny85 Pin | Function |
|----------|--------------|----------|
| ICSP Pin 2 (5V) | Pin 8 | VCC |
| ICSP Pin 6 (GND) | Pin 4 | GND |
| Digital Pin 10 | Pin 1 | RESET (PB5) |
| ICSP Pin 4 (MOSI) | Pin 5 | PB0 (soft-serial TX → Leonardo D16) |
| ICSP Pin 1 (MISO) | Pin 6 | PB1 |
| ICSP Pin 3 (SCK) | Pin 7 | PB2 |
| — | Pin 2 (PB3) | TrackPoint CLK |
| — | Pin 3 (PB4) | TrackPoint DAT |
| — | — | TrackPoint VCC ← 5V rail, GND shared |

## Commands (all verified in this experiment)

### Build

```powershell
pio run -d attiny85-trackpoint/attiny85-trackpoint-serial
# -> 2270 B flash / 163 B RAM, no warnings
```

### Flash the Leonardo as programmer (ArduinoISP)

```powershell
pio run -e leonardo -t upload          # in attiny85-trackpoint/arduino-isp/, avr109 over native USB
```

### Attach the Leonardo to WSL + flash the ATtiny85

```powershell
usbipd attach --wsl --busid 3-1        # busid 3-1 = Leonardo COM9
```

```bash
# in Alpine:
modprobe cdc_acm; ls /dev/ttyACM0
timeout 2 cat /dev/ttyACM0 >/dev/null  # drain port, first probe may sync-fail
avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200              # probe: 0x1e930b
avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200 \
  -U flash:w:"/mnt/d/DIY/trackpoint/promini-zmk/attiny85-trackpoint/attiny85-trackpoint-serial/.pio/build/program_via_ArduinoISP/firmware.hex":i
```

> Note: no `-D` flag — avrdude does the full chip erase first (Exp42 gotcha).

### Detach from WSL, reflash the Leonardo as serial bridge

```powershell
usbipd detach --busid 3-1
pio run -t upload                      # in attiny85-trackpoint/leonardo-serial-reader/
```

### Read the ATtiny85 live stream on Windows (COM9, USB CDC — baud ignored)

```powershell
$p = New-Object System.IO.Ports.SerialPort COM9,115200,None,8,1
$p.DtrEnable = $true; $p.RtsEnable = $true; $p.Open()
while ($true) {
    if ($p.BytesToRead -gt 0) { Write-Host -NoNewline $p.ReadExisting() }
    Start-Sleep -Milliseconds 20
}
```

Ctrl+C to stop. Expect the `--- Exp54 ... ---` banner then `X:.. Y:..` lines
as the nub moves. First-connect garbage = mid-stream banner capture + PS/2
cold-start sync; packets lock in right after. If COM9 changed, check with
`[System.IO.Ports.SerialPort]::getportnames()`.

## Success criteria

- [x] Green `pio run`, flash < 8192 B, no warnings
- [x] Signature 0x1e930b, hex written + verified (no `-D`)
- [x] Clean X:/Y: stream on COM9 through the Leonardo bridge
- [x] **Both axes** move (Exp39 one-axis regression check)

## Findings

- Build: **2270 / 8192 bytes** (27.7%) flash, 163 / 512 B RAM, no warnings.
- Flash via Leonardo ISP: sig 0x1e930b, 2270 B written + verified.
- Serial through Leonardo bridge: **clean `X:.. Y:..` stream @9600** with real
  nub motion. X ranged −19…+21, Y ranged −29…+11 over a 20s read — **both
  axes move**, no Exp39 one-axis regression.
- Live stream (the PowerShell loop above) is the recommended way to watch the
  output — no recording, prints as packets arrive.
- Trackpoint at **5V** (user choice; spec is 3.3V) worked without issue on
  this bench.
- Leading garbage on the first read = mid-stream capture of the boot banner +
  PS/2 cold-start misalignment; self-corrects once packet sync locks (Exp43
  sync logic). Not a defect.
- No `reset()`/`enableStreaming()` needed — the IC streams on power-up
  (confirmed again).

## Conclusion

**Success.** The Exp53 Leonardo serial-bridge path carries a real PS/2
trackpoint sketch cleanly: ATtiny85 reads X/Y and streams it to Windows over
the Leonardo's USB, standalone from any I2C/ZMK stack. PS/2 parsing on the
ATtiny85 at 8MHz is validated, both axes live. The live-stream command above
is the go-to for future ATtiny85 bench reads.
