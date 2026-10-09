param(
    [string]$ArduinoJsonInclude = $env:AGV_ARDUINOJSON_INCLUDE,
    [string]$Compiler = 'g++'
)
$ErrorActionPreference = 'Stop'
if (-not $ArduinoJsonInclude -or -not (Test-Path -LiteralPath (Join-Path $ArduinoJsonInclude 'ArduinoJson.h'))) {
    throw 'Truyền -ArduinoJsonInclude tới thư mục src của thư viện ArduinoJson 7.x đã cài.'
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $projectRoot '.qa/firmware-host-build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$probe = Join-Path $outputRoot 'sketch_probe.exe'
& $Compiler '-std=c++17' '-DAGV_FIRMWARE_HOST' '-Wall' '-Wextra' (Join-Path $projectRoot 'tests/firmware-host/probe.cpp') (Join-Path $projectRoot 'firmware/AGV_Connection_Test/driver.cpp') '-I' (Join-Path $projectRoot 'tests/firmware-host') '-I' $ArduinoJsonInclude '-o' $probe
if ($LASTEXITCODE -ne 0) {throw 'Build sketch host probe thất bại'}
& $probe '--self-test' (Join-Path $projectRoot 'data/map_layout.json') (Join-Path $projectRoot 'contracts/examples/current-mqtt.json') (Join-Path $projectRoot 'tests/fixtures/rfid-tags.json')
if ($LASTEXITCODE -ne 0) {throw 'Sketch host tests thất bại'}
$driverBench = Join-Path $outputRoot 'driver_bench.exe'
& $Compiler '-std=c++17' '-DAGV_DRIVER_BENCH' '-Wall' '-Wextra' (Join-Path $projectRoot 'tests/firmware-host/driver_bench.cpp') '-I' (Join-Path $projectRoot 'tests/firmware-host') '-o' $driverBench
if ($LASTEXITCODE -ne 0) {throw 'Build motor bench shim thất bại'}
& $driverBench
if ($LASTEXITCODE -ne 0) {throw 'Motor bench shim tests thất bại'}
