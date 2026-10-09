param([int]$Port = 3100)
$ErrorActionPreference = 'Stop'
if ($Port -in @(1883,3000,8080) -or $Port -lt 1 -or $Port -gt 65535) { throw 'Cổng này không thuộc web mô phỏng' }
$launcher = $null
try { $launcher = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/launch/status" -TimeoutSec 2 } catch {}
if ($launcher -and $launcher.url) {
    $Port = ([uri]$launcher.url).Port
} elseif ($launcher -and -not $launcher.active) {
    Write-Host 'Mô phỏng đã dừng; điểm mở trang vẫn sẵn sàng.'
    return
}
$simulationState = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/sim-api/state" -TimeoutSec 3
if ($simulationState.simulation -ne $true) { throw 'Không tìm thấy server mô phỏng tại cổng này' }
Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$Port/sim-api/stop" -ContentType 'application/json' -Body '{"confirm":true}' | Out-Null
Write-Host 'Đang lưu report.json và dừng riêng gateway, Core, broker mô phỏng.'
