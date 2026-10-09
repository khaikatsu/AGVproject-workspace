param([int]$Port = 3100, [switch]$Once)
$ErrorActionPreference = 'Stop'
if ($Port -lt 1 -or $Port -gt 65535 -or $Port -in @(1883,3000,8080)) { throw 'Cổng mô phỏng không hợp lệ' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$launcherFile = Join-Path $projectRoot 'simulation/launcher.js'
$nodeFile = (Get-Command node -ErrorAction Stop).Source
$logDirectory = Join-Path $projectRoot 'simulation/autostart-logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$base = "http://127.0.0.1:$Port"
try {
    $status = Invoke-RestMethod -Uri "$base/launch/status" -TimeoutSec 2
    if ($null -ne $status.active) { Write-Output "Điểm mở mô phỏng đã sẵn sàng: $base/simulation"; return }
} catch {}
try {
    $state = Invoke-RestMethod -Uri "$base/sim-api/state" -TimeoutSec 2
    if ($state.simulation -eq $true) {
        Invoke-RestMethod -Method Post -Uri "$base/sim-api/stop" -ContentType 'application/json' -Body '{"confirm":true}' -TimeoutSec 5 | Out-Null
        for ($i=0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 500
            try { Invoke-RestMethod -Uri "$base/sim-api/state" -TimeoutSec 1 | Out-Null }
            catch { break }
        }
    }
} catch {}
$savedEnvironment = @{}
Get-ChildItem Env: | Where-Object { $_.Name -like 'AGV_*' -or $_.Name -eq 'SIM_WEB_PORT' } | ForEach-Object {
    $savedEnvironment[$_.Name] = $_.Value
    Remove-Item -LiteralPath ('Env:' + $_.Name)
}
if ($savedEnvironment.ContainsKey('AGV_MOSQUITTO_BIN')) { $env:AGV_MOSQUITTO_BIN = $savedEnvironment['AGV_MOSQUITTO_BIN'] }
$env:SIM_WEB_PORT = "$Port"
try {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    Start-Process -FilePath $nodeFile -ArgumentList ('"' + $launcherFile + '"') `
        -WorkingDirectory $projectRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $logDirectory "launcher-$Port-$stamp.out.log") `
        -RedirectStandardError (Join-Path $logDirectory "launcher-$Port-$stamp.err.log") | Out-Null
} finally {
    Remove-Item Env:SIM_WEB_PORT -ErrorAction SilentlyContinue
    Remove-Item Env:AGV_MOSQUITTO_BIN -ErrorAction SilentlyContinue
    foreach ($name in $savedEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $name) -Value $savedEnvironment[$name] }
}
for ($i=0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    try {
        $status = Invoke-RestMethod -Uri "$base/launch/status" -TimeoutSec 1
        if ($null -ne $status.active) { Write-Output "Điểm mở mô phỏng đã sẵn sàng: $base/simulation"; return }
    } catch {}
}
throw "Không mở được điểm mở mô phỏng; kiểm tra cổng $Port và log trong $logDirectory"
