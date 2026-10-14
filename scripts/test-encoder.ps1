param([switch]$SkipCoreBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$encoderOutput = Join-Path $projectRoot '.qa/encoder-build'
New-Item -ItemType Directory -Force -Path $encoderOutput | Out-Null
$compiler = 'C:/msys64/ucrt64/bin/g++.exe'
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
$firmwareRoot = Join-Path $projectRoot 'firmware/NCKHAGV/NCKHAGV'
$arduinoJson = Join-Path $firmwareRoot '.pio/libdeps/esp32dev_two_robot_bench_01/ArduinoJson/src'
foreach ($profile in @('01','02')) {
    $probe = Join-Path $encoderOutput "encoder_probe_$profile.exe"
    & $compiler '-std=c++17' '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$profile=1" `
        '-I' "$projectRoot/tests/encoder-host" '-I' "$firmwareRoot/src" '-I' "$projectRoot/include" '-I' $arduinoJson `
        "$projectRoot/tests/encoder-host/test_main.cpp" "$firmwareRoot/src/sensors/EncoderModule.cpp" `
        "$firmwareRoot/src/core/CoreProtocol.cpp" "$firmwareRoot/src/core/ControlLogic.cpp" '-o' $probe
    if ($LASTEXITCODE -ne 0) { throw "Build encoder probe $profile failed" }
    & $probe
    if ($LASTEXITCODE -ne 0) { throw "Encoder probe $profile failed" }
}
$coreProbe = Join-Path $encoderOutput 'encoder_core_tests.exe'
$clockProbe = Join-Path $encoderOutput 'network_clock_tests.exe'
& $compiler '-std=c++17' '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' '-DAGV_ROBOT_PROFILE_01=1' '-I' "$firmwareRoot/src" `
    "$projectRoot/tests/network-clock-host.cpp" '-o' $clockProbe
if ($LASTEXITCODE -ne 0) { throw 'Build network clock tests failed' }
& $clockProbe
if ($LASTEXITCODE -ne 0) { throw 'Network clock tests failed' }
& $compiler '-std=c++17' '-I' "$projectRoot/include" "$projectRoot/tests/encoder_core_tests.cpp" '-o' $coreProbe
if ($LASTEXITCODE -ne 0) { throw 'Build encoder Core tests failed' }
& $coreProbe
if ($LASTEXITCODE -ne 0) { throw 'Encoder Core tests failed' }
if (-not $SkipCoreBuild) { & "$PSScriptRoot/build-core.ps1" -Compiler $compiler -Tests -OutputDirectory '.qa/encoder-build' }
