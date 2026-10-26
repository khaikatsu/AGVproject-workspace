param()
$ErrorActionPreference = 'Stop'
$dockingRoot = Split-Path -Parent $PSScriptRoot
$dockingFirmware = Join-Path $dockingRoot 'firmware/NCKHAGV/src'
$dockingOutput = Join-Path $dockingRoot '.qa/docking'
New-Item -ItemType Directory -Force -Path $dockingOutput | Out-Null
$dockingCompiler = if (Get-Command 'g++.exe' -ErrorAction SilentlyContinue) { (Get-Command 'g++.exe').Source } else { 'C:/msys64/ucrt64/bin/g++.exe' }
foreach ($dockingProfile in @('01','02')) {
    $dockingBinary = Join-Path $dockingOutput "docking-$dockingProfile.exe"
    & $dockingCompiler '-std=c++17' '-I' "$dockingRoot/tests/motor-host" '-I' $dockingFirmware `
        '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$dockingProfile=1" `
        "$dockingRoot/tests/motor-host/docking.cpp" "$dockingFirmware/core/CoreNavigator.cpp" `
        "$dockingFirmware/core/ControlLogic.cpp" "$dockingFirmware/motion/MovementModule.cpp" `
        "$dockingFirmware/motion/LineFollowModule.cpp" "$dockingFirmware/motion/MotorModule.cpp" '-o' $dockingBinary
    if ($LASTEXITCODE -ne 0) { throw "Build docking profile $dockingProfile failed" }
    & $dockingBinary
    if ($LASTEXITCODE -ne 0) { throw "Docking profile $dockingProfile failed" }
    $dockingSonarBinary = Join-Path $dockingOutput "rear-handoff-$dockingProfile.exe"
    & $dockingCompiler '-std=c++17' '-I' "$dockingRoot/tests/sonar-host" '-I' $dockingFirmware `
        '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$dockingProfile=1" `
        "$dockingRoot/tests/sonar-host/reverse_home.cpp" "$dockingFirmware/sensors/SensorModule.cpp" `
        "$dockingFirmware/core/CoreNavigator.cpp" "$dockingFirmware/core/ControlLogic.cpp" '-o' $dockingSonarBinary
    if ($LASTEXITCODE -ne 0) { throw "Build rear sonar handoff profile $dockingProfile failed" }
    & $dockingSonarBinary
    if ($LASTEXITCODE -ne 0) { throw "Rear sonar handoff profile $dockingProfile failed" }
}
