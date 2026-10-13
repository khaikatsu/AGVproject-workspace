param([int]$Port = 3110, [switch]$Stop)
$ErrorActionPreference = 'Stop'
$shareRoot = Split-Path -Parent $PSScriptRoot
$shareDirectory = Join-Path $shareRoot '.qa/public-simulation'
$shareStatePath = Join-Path $shareDirectory 'share.json'
$shareCloudflared = Join-Path $shareRoot '.qa/public-simulation-tools/cloudflared.exe'
if (-not $Stop -and -not (Test-Path (Join-Path $shareRoot 'build/simulation/agv_core.exe'))) { throw 'Thiếu Core mô phỏng; chạy scripts/run-simulation.ps1 -Build trước' }
if ($Port -lt 1 -or $Port -gt 65535 -or $Port -in @(1883,3000,3100,8080)) { throw 'Dùng cổng chia sẻ riêng, mặc định 3110' }
if ($Stop) {
    if (-not (Test-Path $shareStatePath)) { Write-Host 'Chưa có phiên chia sẻ.'; return }
    $shareState = Get-Content -LiteralPath $shareStatePath -Raw | ConvertFrom-Json
    $shareTunnel = Get-CimInstance Win32_Process -Filter ('ProcessId=' + $shareState.tunnelPid)
    if ($shareTunnel.ExecutablePath -eq $shareCloudflared) { Stop-Process -Id $shareTunnel.ProcessId }
    Write-Host 'Đã tắt đường hầm Internet. Mô phỏng cục bộ vẫn hoạt động.'
    return
}
if (Test-Path $shareStatePath) {
    $shareState = Get-Content -LiteralPath $shareStatePath -Raw | ConvertFrom-Json
    $shareTunnel = Get-CimInstance Win32_Process -Filter ('ProcessId=' + $shareState.tunnelPid)
    if ($shareTunnel.ExecutablePath -eq $shareCloudflared) { Write-Host $shareState.url; return }
}
New-Item -ItemType Directory -Force -Path $shareDirectory | Out-Null
if (-not (Test-Path $shareCloudflared)) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $shareCloudflared) | Out-Null
    $shareRelease = Invoke-RestMethod 'https://api.github.com/repos/cloudflare/cloudflared/releases/latest'
    $shareAsset = $shareRelease.assets | Where-Object name -eq 'cloudflared-windows-amd64.exe'
    Invoke-WebRequest -UseBasicParsing -Uri $shareAsset.browser_download_url -OutFile $shareCloudflared
    if (-not $shareAsset.digest -or ('sha256:' + (Get-FileHash $shareCloudflared).Hash.ToLowerInvariant()) -ne $shareAsset.digest) { throw 'Không xác minh được checksum cloudflared' }
}
try { $shareExisting = Invoke-RestMethod "http://127.0.0.1:$Port/launch/status" -TimeoutSec 2 } catch { $shareExisting = $null }
if ($shareExisting -and $shareExisting.public_proxy -ne $true) { throw 'Cổng đang có launcher không hỗ trợ chia sẻ; chọn cổng khác' }
$shareLauncherPid = $null
if (-not $shareExisting) {
    $shareSavedEnvironment = @{}
    Get-ChildItem Env: | Where-Object { $_.Name -like 'AGV_*' -or $_.Name -eq 'SIM_WEB_PORT' } | ForEach-Object {
        $shareSavedEnvironment[$_.Name] = $_.Value
        Remove-Item -LiteralPath ('Env:' + $_.Name)
    }
    try {
        if ($shareSavedEnvironment.ContainsKey('AGV_MOSQUITTO_BIN')) { $env:AGV_MOSQUITTO_BIN = $shareSavedEnvironment['AGV_MOSQUITTO_BIN'] }
        $env:SIM_WEB_PORT = "$Port"
        $shareNode = (Get-Command node.exe).Source
        $shareLauncher = Start-Process -FilePath $shareNode -ArgumentList @('simulation/launcher.js','--public-proxy') -WorkingDirectory $shareRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $shareDirectory 'launcher.out.log') -RedirectStandardError (Join-Path $shareDirectory 'launcher.err.log') -PassThru
        $shareLauncherPid = $shareLauncher.Id
    } finally {
        Remove-Item Env:SIM_WEB_PORT -ErrorAction SilentlyContinue
        Remove-Item Env:AGV_MOSQUITTO_BIN -ErrorAction SilentlyContinue
        foreach ($shareEnvironmentName in $shareSavedEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $shareEnvironmentName) -Value $shareSavedEnvironment[$shareEnvironmentName] }
    }
    $shareDeadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 200
        try { $shareExisting = Invoke-RestMethod "http://127.0.0.1:$Port/launch/status" -TimeoutSec 1 } catch {}
    } while (-not $shareExisting -and (Get-Date) -lt $shareDeadline)
    if (-not $shareExisting.public_proxy) { throw 'Điểm chia sẻ chưa khởi động; xem launcher.err.log' }
}
$shareTunnelLog = Join-Path $shareDirectory 'tunnel.err.log'
$shareTunnel = Start-Process -FilePath $shareCloudflared -ArgumentList @('tunnel','--no-autoupdate','--protocol','http2','--url',"http://127.0.0.1:$Port") -WorkingDirectory $shareRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $shareDirectory 'tunnel.out.log') -RedirectStandardError $shareTunnelLog -PassThru
$shareDeadline = (Get-Date).AddSeconds(45)
$shareUrl = $null
do {
    Start-Sleep -Milliseconds 300
    if (Test-Path $shareTunnelLog) {
        $shareMatch = [regex]::Match((Get-Content -LiteralPath $shareTunnelLog -Raw),'https://[a-z0-9-]+\.trycloudflare\.com')
        if ($shareMatch.Success) { $shareUrl = $shareMatch.Value }
    }
    $shareTunnel.Refresh()
} while (-not $shareUrl -and -not $shareTunnel.HasExited -and (Get-Date) -lt $shareDeadline)
if (-not $shareUrl) { if (-not $shareTunnel.HasExited) { Stop-Process -Id $shareTunnel.Id }; throw 'Chưa tạo được link Internet; xem .qa/public-simulation/tunnel.err.log' }
$shareInfo = [ordered]@{url=($shareUrl+'/simulation');port=$Port;tunnelPid=$shareTunnel.Id;launcherPid=$shareLauncherPid;createdAt=(Get-Date).ToString('o')}
$shareInfo | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $shareStatePath
Write-Host $shareInfo.url
Write-Host 'Máy tính và đường hầm cần tiếp tục chạy. Người có link có thể điều khiển phiên mô phỏng chung.'
