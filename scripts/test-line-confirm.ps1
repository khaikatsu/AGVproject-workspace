param()
$ErrorActionPreference = 'Stop'
$confirmRoot = Split-Path -Parent $PSScriptRoot
$confirmFw = Join-Path $confirmRoot 'firmware/NCKHAGV/NCKHAGV/src'
$confirmOut = Join-Path $confirmRoot '.qa/line-confirm'
New-Item -ItemType Directory -Force -Path $confirmOut | Out-Null
$confirmPreviousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $confirmPreviousPath
try {
    foreach ($confirmProfile in @('01','02')) {
        $confirmBinary = Join-Path $confirmOut "line-confirm-$confirmProfile.exe"
        & 'C:/msys64/ucrt64/bin/g++.exe' '-std=c++17' '-I' "$confirmRoot/tests/motor-host" '-I' $confirmFw `
            '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$confirmProfile=1" `
            "$confirmRoot/tests/motor-host/line_confirm.cpp" "$confirmFw/core/CoreNavigator.cpp" `
            "$confirmFw/core/ControlLogic.cpp" "$confirmFw/motion/MovementModule.cpp" `
            "$confirmFw/motion/LineFollowModule.cpp" "$confirmFw/motion/MotorModule.cpp" '-o' $confirmBinary
        if ($LASTEXITCODE -ne 0) { throw "Build line-confirm profile $confirmProfile failed" }
        & $confirmBinary
        if ($LASTEXITCODE -ne 0) { throw "Line-confirm profile $confirmProfile failed" }
    }
} finally { $env:PATH = $confirmPreviousPath }
