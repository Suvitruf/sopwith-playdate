# Read the Playdate USB console through the Windows host (also usable from WSL).
param(
    [Parameter(Mandatory = $true)]
    [string]$PortName,
    [ValidateRange(1, 60)]
    [int]$Seconds = 30,
    [ValidateSet('version', 'help')]
    [string]$Query
)

$ErrorActionPreference = 'Stop'
$device = Get-CimInstance Win32_SerialPort |
    Where-Object { $_.DeviceID -eq $PortName -and $_.PNPDeviceID -like '*VID_1331&PID_5740*' }
if (-not $device) {
    throw "No Playdate USB serial device found at $PortName. Unlock the console and check its COM port."
}

$port = New-Object System.IO.Ports.SerialPort $PortName, 115200, None, 8, One
$port.DtrEnable = $true
$port.ReadTimeout = 500
try {
    $port.Open()
    Write-Output "Listening to Playdate on $PortName for $Seconds seconds."
    if ($Query) {
        $port.WriteLine($Query)
    }
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $port.ReadExisting()
        if ($chunk) {
            [Console]::Write($chunk)
        }
        Start-Sleep -Milliseconds 100
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
    $port.Dispose()
}
