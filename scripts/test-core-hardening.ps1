param([switch]$SkipCoreBuild,[switch]$Integration,[switch]$Browser)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $projectRoot '.qa/core-hardening-20261025'
$compiler = 'C:/msys64/ucrt64/bin/g++.exe'
$previousPath = $env:PATH
$previousCore = $env:AGV_CORE_BIN
$env:PATH = 'C:/msys64/ucrt64/bin;' + $previousPath
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
Push-Location $projectRoot
try {
    & "$PSScriptRoot/check-vendor.ps1"
    if (-not $SkipCoreBuild) { & "$PSScriptRoot/build-core.ps1" -Tests -OutputDirectory $outputRoot }
    foreach ($name in @('core_hardening','runtime-map-migration','encoder_core_tests','stopped_occupancy')) {
        $binary = Join-Path $outputRoot "$name.exe"
        & $compiler '-std=c++17' '-I' "$projectRoot/include" "$projectRoot/tests/$name.cpp" '-o' $binary
        if ($LASTEXITCODE -ne 0) { throw "Compile $name failed" }
        & $binary
        if ($LASTEXITCODE -ne 0) { throw "Test $name failed" }
    }
    $env:AGV_CORE_BIN = Join-Path $outputRoot 'agv_core.exe'
    & node tests/startup-errors.integration.js
    if ($LASTEXITCODE -ne 0) { throw 'Startup regression failed' }
    if ($Integration) {
        & node tests/stopped-occupancy.integration.js
        if ($LASTEXITCODE -ne 0) { throw 'MQTT stopped occupancy regression failed' }
    }
    if ($Browser) {
        & node tests/browser.integration.js
        if ($LASTEXITCODE -ne 0) { throw 'Dashboard regression failed' }
    }
} finally {
    Pop-Location
    $env:PATH = $previousPath
    $env:AGV_CORE_BIN = $previousCore
}
