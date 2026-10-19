param()
$ErrorActionPreference = 'Stop'
$pdRoot = Split-Path -Parent $PSScriptRoot
$pdFirmware = Join-Path $pdRoot 'firmware/NCKHAGV/NCKHAGV/src'
$pdOutput = Join-Path $pdRoot '.qa/forward-pd'
New-Item -ItemType Directory -Force -Path $pdOutput | Out-Null
$pdPreviousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $pdPreviousPath
try {
    foreach ($pdProfile in @('01','02')) {
        foreach ($pdEnabled in @(1,0)) {
          $pdAssistModes = if ($pdProfile -eq '01' -and $pdEnabled -eq 1) { @(1,0) } else { @(0) }
          foreach ($pdAssist in $pdAssistModes) {
            $pdBinary = Join-Path $pdOutput "forward-pd-$pdProfile-$pdEnabled-assist$pdAssist.exe"
            & 'C:/msys64/ucrt64/bin/g++.exe' '-std=c++17' '-I' "$pdRoot/tests/motor-host" '-I' $pdFirmware `
                '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$pdProfile=1" "-DAGV_FORWARD_PD_ENABLED=$pdEnabled" `
                "-DAGV_FORWARD_LINE_ASSIST_ENABLED=$pdAssist" `
                "$pdRoot/tests/motor-host/forward_pd.cpp" "$pdFirmware/core/CoreNavigator.cpp" `
                "$pdFirmware/core/ControlLogic.cpp" "$pdFirmware/motion/MovementModule.cpp" `
                "$pdFirmware/motion/LineFollowModule.cpp" "$pdFirmware/motion/MotorModule.cpp" '-o' $pdBinary
            if ($LASTEXITCODE -ne 0) {throw "Build PD probe failed: $pdProfile/$pdEnabled/assist$pdAssist"}
            & $pdBinary
            if ($LASTEXITCODE -ne 0) {throw "PD probe failed: $pdProfile/$pdEnabled/assist$pdAssist"}
          }
        }
    }
} finally { $env:PATH = $pdPreviousPath }
