param([string]$ArduinoJsonInclude = '', [string]$UnitySource = '')
$ErrorActionPreference = 'Stop'
$protocolRoot = Split-Path -Parent $PSScriptRoot
$protocolFirmware = Join-Path $protocolRoot 'firmware/NCKHAGV'
$protocolOutput = Join-Path $protocolRoot '.qa/core-protocol'
$protocolCompiler = if (Get-Command 'g++.exe' -ErrorAction SilentlyContinue) { (Get-Command 'g++.exe').Source } else { 'C:/msys64/ucrt64/bin/g++.exe' }
foreach ($protocolDeps in @("$protocolFirmware/.pio/libdeps/esp32dev",
    "$protocolFirmware/.pio/libdeps/esp32dev_two_robot_bench_01",
    'D:/AGV-build-cache/AGVproject-workspace-main/libdeps/esp32dev_two_robot_bench_01')) {
    if (-not $ArduinoJsonInclude -and (Test-Path "$protocolDeps/ArduinoJson/src/ArduinoJson.h")) {
        $ArduinoJsonInclude = "$protocolDeps/ArduinoJson/src"
    }
    if (-not $UnitySource -and (Test-Path "$protocolDeps/Unity/src/unity.c")) {
        $UnitySource = "$protocolDeps/Unity/src/unity.c"
    }
}
if (-not (Test-Path "$ArduinoJsonInclude/ArduinoJson.h") -or -not (Test-Path $UnitySource)) {
    throw 'Cần ArduinoJson và Unity đã cài; truyền -ArduinoJsonInclude và -UnitySource nếu cache ở nơi khác.'
}
$protocolUnityInclude = Split-Path -Parent $UnitySource
New-Item -ItemType Directory -Force -Path $protocolOutput | Out-Null
foreach ($protocolProfile in @('01','02')) {
    $protocolBinary = Join-Path $protocolOutput "core-protocol-$protocolProfile.exe"
    & $protocolCompiler '-std=c++17' '-I' "$protocolFirmware/src" '-I' $ArduinoJsonInclude '-I' $protocolUnityInclude `
        '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$protocolProfile=1" `
        "$protocolFirmware/test/test_core_protocol/test_main.cpp" "$protocolFirmware/src/core/CoreNavigator.cpp" `
        "$protocolFirmware/src/core/CoreProtocol.cpp" "$protocolFirmware/src/core/ControlLogic.cpp" `
        "$protocolFirmware/src/core/CommandCodec.cpp" $UnitySource '-o' $protocolBinary
    if ($LASTEXITCODE -ne 0) { throw "Build core protocol profile $protocolProfile failed" }
    & $protocolBinary
    if ($LASTEXITCODE -ne 0) { throw "Core protocol profile $protocolProfile failed" }
}
