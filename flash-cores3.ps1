# Flash M5Stack CoreS3 - run from repo root or firmware/
#
# If upload says "No serial data received", hold LEFT button, tap RESET, release both.
# If upload says "port is busy", this script tries to close serial monitors first.

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Stop-SerialPortHolders {
    param([string]$Port)
    $portNeedle = $Port.ToUpperInvariant()
    $procs = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        if ($_.ProcessId -eq $PID) { return $false }
        $cmd = ($_.CommandLine | Out-String)
        if (-not $cmd) { return $false }
        $cmd -match 'platformio\s+device\s+monitor' -or
        $cmd -match 'ghost_forge'
    }
    if (-not $procs) { return 0 }
    $killed = 0
    foreach ($p in $procs) {
        Write-Host "  Closing PID $($p.ProcessId): $($p.Name)" -ForegroundColor DarkYellow
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
        $killed++
    }
    if ($killed -gt 0) {
        Start-Sleep -Seconds 1.5
    }
    return $killed
}

function Get-CoreS3Port {
    $preferred = @()
    $all = @()
    try {
        $lines = & py -m serial.tools.list_ports 2>$null
        foreach ($line in $lines) {
            if ($line -match '^(COM\d+)\s+') {
                $dev = $Matches[1]
                $all += $dev
                if ($line -match '303A|8119|ESP32|M5') {
                    $preferred += $dev
                }
            }
        }
    } catch { }
    if ($preferred.Count -gt 0) { return $preferred[0] }
    if ($all.Count -gt 0) { return $all[0] }
    return "COM4"
}

function Test-PortOpen {
    param([string]$Port)
    try {
        $ser = New-Object System.IO.Ports.SerialPort $Port, 115200
        $ser.ReadTimeout = 200
        $ser.WriteTimeout = 200
        $ser.Open()
        $ser.Close()
        $ser.Dispose()
        return $true
    } catch {
        return $false
    }
}

Write-Host ""
Write-Host "=== M5Stack CoreS3 flash ===" -ForegroundColor Cyan

$uploadPort = $env:M5_UPLOAD_PORT
if (-not $uploadPort) {
    $uploadPort = Get-CoreS3Port
}

Write-Host "Upload port: $uploadPort (set M5_UPLOAD_PORT to override)" -ForegroundColor Yellow
Write-Host "Releasing serial port holders..." -ForegroundColor Yellow
$closed = Stop-SerialPortHolders -Port $uploadPort
if ($closed -gt 0) {
    Write-Host "Closed $closed process(es) that were using the port." -ForegroundColor Yellow
} else {
    Write-Host "No known monitor processes found. Close Ghost Forge manually if upload fails." -ForegroundColor DarkGray
}

if (-not (Test-PortOpen -Port $uploadPort)) {
    Write-Host ""
    Write-Host "COM port $uploadPort is still locked or missing." -ForegroundColor Red
    Write-Host "  - Close Ghost Forge, Cursor serial terminals, and Arduino IDE"
    Write-Host "  - Unplug/replug USB, then re-run flash-cores3.ps1"
    Write-Host "  - Check Device Manager for the correct COM port (VID 303A)"
    exit 1
}

Write-Host "Port $uploadPort is free. Uploading..." -ForegroundColor Green
Write-Host ""

py -m platformio run -e m5stack-cores3 -t upload --upload-port $uploadPort
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Upload failed. Try:" -ForegroundColor Red
    Write-Host "  - LEFT button held + tap RESET, then re-run this script within 10s"
    Write-Host '  - $env:M5_UPLOAD_PORT="COMx"; .\flash-cores3.ps1'
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Flash OK. Boot check (Ctrl+C to stop):" -ForegroundColor Green
$monitorCmd = "py -m platformio device monitor -e m5stack-cores3 --port $uploadPort"
Write-Host "  $monitorCmd"
Write-Host ""
