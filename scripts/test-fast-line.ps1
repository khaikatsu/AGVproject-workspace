param()
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskFw = Join-Path $taskRoot 'firmware/NCKHAGV/NCKHAGV'
$taskOut = Join-Path $taskRoot '.qa/fast-line'
New-Item -ItemType Directory -Force -Path $taskOut | Out-Null
$taskPreviousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $taskPreviousPath
try {
    foreach ($taskProfile in @('01','02')) {
        $taskBinary = Join-Path $taskOut "fast-line-$taskProfile.exe"
        & 'C:/msys64/ucrt64/bin/g++.exe' '-std=c++17' '-I' "$taskRoot/tests/motor-host" '-I' "$taskFw/src" `
            '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$taskProfile=1" `
            "$taskRoot/tests/motor-host/fast_line.cpp" "$taskFw/src/core/CoreNavigator.cpp" `
            "$taskFw/src/core/ControlLogic.cpp" "$taskFw/src/motion/MovementModule.cpp" `
            "$taskFw/src/motion/LineFollowModule.cpp" "$taskFw/src/motion/MotorModule.cpp" '-o' $taskBinary
        if ($LASTEXITCODE -ne 0) {throw "Build probe $taskProfile failed"}
        & $taskBinary
        if ($LASTEXITCODE -ne 0) {throw "Probe $taskProfile failed"}
    }
} finally { $env:PATH = $taskPreviousPath }
