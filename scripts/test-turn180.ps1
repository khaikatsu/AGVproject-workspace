param()
$ErrorActionPreference = 'Stop'
$turnRoot = Split-Path -Parent $PSScriptRoot
$turnFw = Join-Path $turnRoot 'firmware/NCKHAGV/src'
$turnOut = Join-Path $turnRoot '.qa/turn180'
New-Item -ItemType Directory -Force -Path $turnOut | Out-Null
$turnPreviousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $turnPreviousPath
try {
    foreach ($turnProfile in @('01','02')) {
        $turnBinary = Join-Path $turnOut "turn180-$turnProfile.exe"
        & 'C:/msys64/ucrt64/bin/g++.exe' '-std=c++17' '-I' "$turnRoot/tests/motor-host" '-I' $turnFw `
            '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$turnProfile=1" `
            "$turnRoot/tests/motor-host/turn180.cpp" "$turnFw/core/CoreNavigator.cpp" `
            "$turnFw/core/ControlLogic.cpp" "$turnFw/motion/MovementModule.cpp" `
            "$turnFw/motion/LineFollowModule.cpp" "$turnFw/motion/MotorModule.cpp" '-o' $turnBinary
        if ($LASTEXITCODE -ne 0) { throw "Build turn180 profile $turnProfile failed" }
        & $turnBinary
        if ($LASTEXITCODE -ne 0) { throw "Turn180 profile $turnProfile failed" }
    }
} finally { $env:PATH = $turnPreviousPath }
