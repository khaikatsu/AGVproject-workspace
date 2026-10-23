param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $projectRoot 'firmware/NCKHAGV/src'
$outputRoot = Join-Path $projectRoot '.qa/rounded-map-v4'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$previousPath = $env:PATH
$env:PATH = 'C:/msys64/ucrt64/bin;' + $previousPath
try {
    Push-Location $projectRoot
    foreach ($profile in @('01','02')) {
        $probe = Join-Path $outputRoot "rounded-map-$profile.exe"
        & 'C:/msys64/ucrt64/bin/g++.exe' '-std=c++17' '-I' "$projectRoot/include" '-I' $firmwareRoot `
            '-DAGV_TWO_ROBOT_BENCH_TRIAL=1' "-DAGV_ROBOT_PROFILE_$profile=1" `
            "$projectRoot/tests/rounded-map.cpp" "$firmwareRoot/core/CoreNavigator.cpp" `
            "$firmwareRoot/core/ControlLogic.cpp" '-o' $probe
        if ($LASTEXITCODE -ne 0) {throw "Build rounded map failed: $profile"}
        & $probe
        if ($LASTEXITCODE -ne 0) {throw "Rounded map failed: $profile"}
    }
} finally {Pop-Location; $env:PATH = $previousPath}
