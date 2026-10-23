param([switch]$ServoOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $projectRoot 'firmware/NCKHAGV'
$parityOutput = Join-Path $projectRoot '.qa/robot02-parity-build'
New-Item -ItemType Directory -Force -Path $parityOutput | Out-Null
$compiler = 'C:/msys64/ucrt64/bin/g++.exe'
$previousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $previousPath
$jsonInclude = Join-Path $firmwareRoot '.pio/libdeps/esp32dev_two_robot_bench_01/ArduinoJson/src'
function Build-Probe($name, $adapter, $sources, $profile) {
    $probePath = Join-Path $parityOutput ($name + '.exe')
    $arguments = @('-std=c++17', '-I', "$projectRoot/include", '-I', "$firmwareRoot/src", '-I', $jsonInclude)
    if ($adapter) { $arguments += @('-I', "$projectRoot/tests/$adapter") }
    if ($profile) { $arguments += @('-DAGV_TWO_ROBOT_BENCH_TRIAL=1', "-DAGV_ROBOT_PROFILE_$profile=1") }
    foreach ($source in $sources) { $arguments += Join-Path $projectRoot $source }
    & $compiler @arguments '-o' $probePath
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $name" }
    & $probePath
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
}
try {
    $fw = 'firmware/NCKHAGV/src'
    foreach ($profile in @('01','02')) {
        if (-not $ServoOnly) {
            Build-Probe "motor_$profile" 'motor-host' @('tests/motor-host/reverse_speed.cpp', "$fw/motion/MovementModule.cpp",
                "$fw/motion/MotorModule.cpp", "$fw/motion/LineFollowModule.cpp", "$fw/core/ControlLogic.cpp") $profile
        }
        Build-Probe "servo_$profile" 'servo-host' @('tests/servo-host/test_main.cpp', "$fw/actuators/ServoModule.cpp",
            "$fw/core/CoreProtocol.cpp", "$fw/core/ControlLogic.cpp") $profile
        if (-not $ServoOnly) {
            Build-Probe "encoder_$profile" 'encoder-host' @('tests/encoder-host/test_main.cpp', "$fw/sensors/EncoderModule.cpp",
                "$fw/core/CoreProtocol.cpp", "$fw/core/ControlLogic.cpp") $profile
            Build-Probe "sonar_$profile" 'sonar-host' @('tests/sonar-host/directional_test.cpp', "$fw/sensors/SensorModule.cpp",
                "$fw/core/CoreNavigator.cpp", "$fw/core/ControlLogic.cpp") $profile
        }
    }
    if ($ServoOnly) {
        Build-Probe 'servo_disabled' 'servo-host' @('tests/servo-host/test_main.cpp', "$fw/actuators/ServoModule.cpp",
            "$fw/core/CoreProtocol.cpp", "$fw/core/ControlLogic.cpp") ''
    } else {
        Build-Probe 'encoder_core' '' @('tests/encoder_core_tests.cpp') ''
    }
    Build-Probe 'servo_core' '' @('tests/servo_core.cpp') ''
} finally { $env:PATH = $previousPath }
