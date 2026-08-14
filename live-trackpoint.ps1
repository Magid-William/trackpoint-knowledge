param(
    [string]$Port = "COM8",
    [int]$IntervalMs = 300,
    [switch]$Burst
)

# Live trackpoint streamer — busy-loop, prints each sample the instant it
# arrives (no event jobs, no accumulate-then-print).
#
#   Default: `i2c read 0x42 0x03 5` (debug, NON-destructive). Bytes are
#   [status, xraw, yraw, to_lo, to_hi], updated by every trackpoint packet.
#   -Burst: `i2c read 0x42 0x12 2` (destructive [x,y]); prints non-zero only.
#
# Example:  .\live-trackpoint.ps1 -Port COM8
# Stop with Ctrl+C.

$read = if ($Burst) { "i2c read i2c@40003000 0x42 0x12 2" } else { "i2c read i2c@40003000 0x42 0x03 5" }
$regex = [regex]'00000000: ([0-9a-f]{2}(?: [0-9a-f]{2})*)'

$p = New-Object System.IO.Ports.SerialPort $Port,115200,None,8,1
$p.ReadTimeout = 50
$p.DtrEnable = $true
$p.RtsEnable = $true
$p.Open()
Start-Sleep 1
try { $p.ReadExisting() } catch { }

$send = [System.Diagnostics.Stopwatch]::StartNew()
$buf = ""
"Streaming $read every ${IntervalMs}ms on $Port -- Ctrl+C to stop"

try {
    while ($true) {
        if ($send.Elapsed.TotalMilliseconds -ge $IntervalMs) {
            $p.Write("$read`n")
            $send.Restart()
        }
        if ($p.BytesToRead -gt 0) {
            $buf += $p.ReadExisting()
            $lines = $buf -split "`n"
            $buf = $lines[-1]
            foreach ($ln in $lines[0..($lines.Length - 2)]) {
                $m = $regex.Match($ln)
                if ($m.Success) {
                    $bytes = @($m.Groups[1].Value -split ' ' | Where-Object { $_ })
                    $n = $bytes.Count
                    if ($Burst) {
                        if ($n -ge 2) {
                            $x = [Convert]::ToSByte($bytes[0], 16)
                            $y = [Convert]::ToSByte($bytes[1], 16)
                            if ($x -ne 0 -or $y -ne 0) {
                                "{0:HH:mm:ss.fff}  x={1,4}  y={2,4}" -f (Get-Date), $x, $y
                            }
                        }
                    }
                    else {
                        if ($n -ge 5) {
                            $x = [Convert]::ToSByte($bytes[1], 16)
                            $y = [Convert]::ToSByte($bytes[2], 16)
                            $toLo = [Convert]::ToInt32($bytes[3], 16)
                            $toHi = [Convert]::ToInt32($bytes[4], 16)
                            $to = ($toHi -shl 8) -bor $toLo
                            "{0:HH:mm:ss.fff}  x={1,4}  y={2,4}  timeouts={3}" -f (Get-Date), $x, $y, $to
                        }
                    }
                }
            }
        }
    }
}
finally {
    $p.Close()
}
