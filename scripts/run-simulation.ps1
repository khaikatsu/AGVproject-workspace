param([int]$Port = 3100, [switch]$Build)
$ErrorActionPreference = 'Stop'
$simulationRoot = Split-Path -Parent $PSScriptRoot
if ($Port -in @(1883,3000,8080) -or $Port -lt 1 -or $Port -gt 65535) { throw 'Chọn cổng riêng cho mô phỏng, mặc định 3100' }
try { $simulationExisting = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/sim-api/state" -TimeoutSec 1 } catch { $simulationExisting = $null }
try { $launcherExisting = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/launch/status" -TimeoutSec 1 } catch { $launcherExisting = $null }
if ($null -ne $launcherExisting.active) {
    Write-Host "Mở http://127.0.0.1:$Port/simulation để bắt đầu mô phỏng."
    return
}
if ($simulationExisting.simulation -eq $true) {
    Write-Host "Bản mô phỏng đang chạy: http://127.0.0.1:$Port/simulation"
    Write-Host 'Dừng phiên cũ bằng scripts/stop-simulation.ps1 trước nếu muốn một phiên mới.'
    return
}
if ($Build -or !(Test-Path -LiteralPath (Join-Path $simulationRoot 'build/agv_core.exe'))) {
    & (Join-Path $PSScriptRoot 'build-core.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build Core thất bại' }
}
# Env is changed only for this command, then restored for the user's shell.
$simulationSaved = @{}
Get-ChildItem Env: | Where-Object { $_.Name -like 'AGV_*' -or $_.Name -eq 'SIM_WEB_PORT' } | ForEach-Object {
    $simulationSaved[$_.Name] = $_.Value
    Remove-Item -LiteralPath ('Env:' + $_.Name)
}
try {
    if ($simulationSaved.ContainsKey('AGV_MOSQUITTO_BIN')) { $env:AGV_MOSQUITTO_BIN = $simulationSaved['AGV_MOSQUITTO_BIN'] }
    $env:SIM_WEB_PORT = "$Port"
    & node (Join-Path $simulationRoot 'simulation/launcher.js')
} finally {
    Remove-Item Env:SIM_WEB_PORT -ErrorAction SilentlyContinue
    Remove-Item Env:AGV_MOSQUITTO_BIN -ErrorAction SilentlyContinue
    foreach ($simulationName in $simulationSaved.Keys) { Set-Item -LiteralPath ('Env:' + $simulationName) -Value $simulationSaved[$simulationName] }
}
