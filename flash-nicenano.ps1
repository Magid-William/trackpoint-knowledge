param(
    [Parameter(Mandatory=$true)]
    [string]$UF2Path,
    [string]$ComPort = "COM8",
    [string]$DriveName = "NICENANO"
)

Write-Host "=== nRF52 Headless Flasher ($DriveName) ==="

if (-not (Test-Path $UF2Path)) { Write-Host "ERROR: UF2 not found: $UF2Path"; exit 1 }
$size = (Get-Item $UF2Path).Length

Write-Host "[1/4] Entering bootloader (GPREGRET 0x57 + reboot)..."
try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort,115200,None,8,1
    $port.ReadTimeout = 1000
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.Open()
    Start-Sleep -Milliseconds 500
    $port.ReadExisting() | Out-Null
    $port.WriteLine("devmem 0x4000051C 32 0x00000057")
    Start-Sleep -Milliseconds 500
    $port.WriteLine("kernel reboot cold")
    Start-Sleep -Seconds 1
    try { $port.ReadExisting() | Out-Null } catch {}
    $port.Close()
} catch {
    Write-Host "WARN: serial not reachable ($($_.Exception.Message)) — continuing anyway"
}

Write-Host "[2/4] Waiting for $DriveName drive..."
$drive = $null
for ($i = 0; $i -lt 15; $i++) {
    Start-Sleep -Seconds 2
    $drive = Get-CimInstance Win32_LogicalDisk | Where-Object { $_.VolumeName -eq $DriveName }
    if ($drive) { break }
    Write-Host "   ... waiting ($($i+1)/15)"
}

if (-not $drive) { Write-Host "ERROR: $DriveName drive not found"; exit 1 }
Write-Host "[3/4] Found $($drive.DeviceID) - copying UF2 ($size bytes)..."

$copied = $false
for ($j = 0; $j -lt 15 -and -not $copied; $j++) {
    $drive = Get-CimInstance Win32_LogicalDisk | Where-Object { $_.DeviceID -eq $drive.DeviceID }
    if (-not $drive) {
        Write-Host "   drive vanished - re-waiting..."
        for ($i = 0; $i -lt 10 -and -not $drive; $i++) {
            Start-Sleep 2
            $drive = Get-CimInstance Win32_LogicalDisk | Where-Object { $_.VolumeName -eq $DriveName }
        }
        if (-not $drive) { Write-Host "ERROR: drive gone mid-copy"; exit 1 }
    }
    try {
        Copy-Item $UF2Path "$($drive.DeviceID)\" -ErrorAction Stop
        Start-Sleep 1
        # The bootloader may already be flashing (drive disconnects on accept).
        # If the drive is gone right after a successful copy, that IS the flash starting.
        $drive = Get-CimInstance Win32_LogicalDisk | Where-Object { $_.DeviceID -eq $drive.DeviceID }
        if (-not $drive) {
            Write-Host "   drive disconnected right after copy - bootloader accepted UF2, flashing..."
            $copied = $true
        } else {
            $dest = Get-Item "$($drive.DeviceID)\$([IO.Path]::GetFileName($UF2Path))" -ErrorAction SilentlyContinue
            if ($dest -and $dest.Length -eq $size) {
                $copied = $true
            } else {
                Write-Host "   size mismatch (got $($dest.Length)/$size) - retry $($j+1)"
            }
        }
    } catch {
        Write-Host "   copy failed - retry $($j+1): $($_.Exception.Message)"
    }
    Start-Sleep 1
}

if (-not $copied) { Write-Host "ERROR: copy failed after retries"; exit 1 }
Write-Host "[4/4] Done: $([IO.Path]::GetFileName($UF2Path)) ($size bytes). Waiting for reboot..."

for ($i = 0; $i -lt 10; $i++) {
    Start-Sleep -Seconds 2
    $com = Get-CimInstance Win32_SerialPort | Where-Object { $_.DeviceID -eq $ComPort }
    if ($com) { Write-Host "Device back online on $ComPort"; exit 0 }
}

Write-Host "Device rebooted (drive disconnected)"
