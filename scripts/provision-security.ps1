param(
    [Parameter(Mandatory=$true)][string]$LanAddress,
    [string]$OpenSsl = 'openssl',
    [string]$MosquittoPasswd = 'mosquitto_passwd'
)
$ErrorActionPreference = 'Stop'
$parsedAddress = $null
if (-not [System.Net.IPAddress]::TryParse($LanAddress,[ref]$parsedAddress) -or $parsedAddress.AddressFamily -ne 'InterNetwork') {
    throw 'LanAddress phải là IPv4 của máy chạy gateway/broker.'
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$deployRoot = Join-Path $projectRoot 'deploy'
$tlsRoot = Join-Path $deployRoot 'tls'
$secretRoot = Join-Path $deployRoot 'secrets'
foreach ($target in @($tlsRoot,$secretRoot,(Join-Path $deployRoot '.env'))) {
    if (Test-Path -LiteralPath $target) {throw "Đã có $target; giữ nguyên credential hiện có. Dùng quy trình gia hạn/đổi mật khẩu riêng."}
}
New-Item -ItemType Directory -Path $tlsRoot,$secretRoot | Out-Null
function InvokeChecked([string]$Program,[string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {throw "$Program thất bại; giữ file để kiểm tra, không tiếp tục provisioning."}
}
InvokeChecked $OpenSsl @('req','-x509','-newkey','rsa:3072','-nodes','-days','3650','-subj','/CN=AGV LAN CA','-keyout',(Join-Path $tlsRoot 'ca.key'),'-out',(Join-Path $tlsRoot 'ca.crt'))
InvokeChecked $OpenSsl @('req','-newkey','rsa:2048','-nodes','-subj','/CN=AGV LAN','-keyout',(Join-Path $tlsRoot 'server.key'),'-out',(Join-Path $tlsRoot 'server.csr'))
$extensionPath = Join-Path $tlsRoot 'extensions.cnf'
Set-Content -LiteralPath $extensionPath -Encoding ASCII -Value "subjectAltName=DNS:localhost,DNS:broker,IP:127.0.0.1,IP:$LanAddress"
InvokeChecked $OpenSsl @('x509','-req','-in',(Join-Path $tlsRoot 'server.csr'),'-CA',(Join-Path $tlsRoot 'ca.crt'),'-CAkey',(Join-Path $tlsRoot 'ca.key'),'-CAcreateserial','-days','365','-extfile',$extensionPath,'-out',(Join-Path $tlsRoot 'server.crt'))
Write-Output 'Nhập ba mật khẩu riêng trong công cụ Mosquitto: core, 01, 02. Mật khẩu không được in hoặc ghi vào mã nguồn.'
$passwordFile = Join-Path $secretRoot 'passwords'
InvokeChecked $MosquittoPasswd @('-c',$passwordFile,'core')
InvokeChecked $MosquittoPasswd @($passwordFile,'01')
InvokeChecked $MosquittoPasswd @($passwordFile,'02')
function NewToken {
    $bytes = New-Object byte[] 32
    $generator = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    try {$generator.GetBytes($bytes)} finally {$generator.Dispose()}
    return [BitConverter]::ToString($bytes).Replace('-','').ToLowerInvariant()
}
$envPath = Join-Path $deployRoot '.env'
@("AGV_CORE_TOKEN=$(NewToken)","AGV_AUTH_TOKEN=$(NewToken)",'AGV_MQTT_PASSWORD=REPLACE_WITH_CORE_PASSWORD',"AGV_ALLOWED_ORIGINS=https://${LanAddress}:3000,https://localhost:3000") | Set-Content -LiteralPath $envPath -Encoding ASCII
$configPath = Join-Path $deployRoot 'mosquitto.windows.conf'
$configuration = Get-Content -LiteralPath (Join-Path $deployRoot 'mosquitto.conf') -Raw
$configuration = $configuration.Replace('/mosquitto/config/',$deployRoot.Replace('\','/')+'/').Replace('persistence_location /mosquitto/data/', 'persistence_location '+$secretRoot.Replace('\','/')+'/')
Set-Content -LiteralPath $configPath -Value $configuration -Encoding ASCII
Write-Output "Đã tạo CA/cert/ACL/password file và token trong $deployRoot. Điền mật khẩu core trong .env trước khi chạy. Không tự thay broker hiện hành hoặc flash xe. Xem docs/18-hardening.md."
