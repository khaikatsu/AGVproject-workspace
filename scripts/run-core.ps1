param(
    [switch]$Build,
    [string]$OutputDirectory = 'build',
    [string]$Mode = '',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ExtraArgs
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
}
$binary = Join-Path $outputRoot 'agv_core.exe'
if ($Build) { & (Join-Path $PSScriptRoot 'build-core.ps1') -OutputDirectory $outputRoot }
if (-not (Test-Path -LiteralPath $binary)) {
    throw "Chưa có core mới tại $binary. Chạy scripts/run-core.ps1 -Build trước."
}
# Giữ counter của các lần chạy từ src; không tạo một chuỗi cmd_id mới về 1.
$normalCounter = Join-Path $projectRoot 'data/command_sequence.json'
$existingCounter = Join-Path $projectRoot 'src/data/command_sequence.json'
$previousCounterOverride = $env:AGV_COMMAND_ID_PATH
$previousPath = $env:PATH
if (-not $env:AGV_COMMAND_ID_PATH -and -not $env:AGV_MAP_PATH -and -not (Test-Path -LiteralPath $normalCounter) -and
    (Test-Path -LiteralPath $existingCounter)) {
    $env:AGV_COMMAND_ID_PATH = $existingCounter
    Write-Output "Giữ counter đang có: $existingCounter"
}
Push-Location -LiteralPath $projectRoot
try {
    $dllPaths = @(
        'C:\Program Files\Mosquitto',
        'C:\Program Files\CodeBlocks\MinGW\opt\bin',
        'C:\Program Files\CodeBlocks\MinGW\bin',
        'C:\msys64\ucrt64\bin',
        'D:\devc\Dev-Cpp\TDM-GCC-64\bin'
    )
    $validDllPaths = $dllPaths | Where-Object { Test-Path -LiteralPath $_ }
    if ($validDllPaths) {
        $env:PATH = ($validDllPaths -join ';') + ';' + $previousPath
    }
    $runArgs = @()
    if ($Mode) {
        $runArgs += @('--mode', $Mode)
    }
    if ($ExtraArgs) {
        $runArgs += $ExtraArgs
    }
    Write-Output "Chạy core: $binary $($runArgs -join ' ')"
    Write-Output "Working directory: $projectRoot"
    & $binary @runArgs
    if ($LASTEXITCODE -ne 0) { throw "Core dừng với mã lỗi $LASTEXITCODE. Xem log phía trên." }
} finally {
    Pop-Location
    $env:AGV_COMMAND_ID_PATH = $previousCounterOverride
    $env:PATH = $previousPath
}
