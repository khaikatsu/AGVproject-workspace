param([string]$Compiler = 'g++', [switch]$Tests, [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& "$PSScriptRoot/check-vendor.ps1"
$outputRoot = if (-not $OutputDirectory) {
    Join-Path $projectRoot 'build'
} elseif ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$source = Join-Path $projectRoot 'src/core/AGVserver.cpp'
$binary = Join-Path $outputRoot 'agv_core.exe'
# Windows không cho linker ghi đè executable đang chạy; không tự dừng Core.
if ($env:OS -eq 'Windows_NT') {
    $runningCore = Get-CimInstance Win32_Process -Filter "Name = 'agv_core.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -and [IO.Path]::GetFullPath($_.ExecutablePath) -eq $binary }
    if ($runningCore) {
        throw "Core tại '$binary' đang chạy (PID $($runningCore.ProcessId -join ', ')). Dừng đúng phiên Core trước khi build/F5, hoặc dùng -OutputDirectory .qa/core-build để build riêng."
    }
}
$compilerCommand = Get-Command $Compiler -CommandType Application -ErrorAction SilentlyContinue
if (-not $compilerCommand -and $Compiler -eq 'g++') {
    $compilerCommand = Get-Command 'C:/msys64/ucrt64/bin/g++.exe' -CommandType Application -ErrorAction SilentlyContinue
}
if (-not $compilerCommand) { throw "Không tìm thấy compiler '$Compiler'. Dùng -Compiler với đường dẫn g++.exe." }
$compilerPath = $compilerCommand.Source
$previousPath = $env:PATH
# Compiler và test đều cần DLL UCRT64/MinGW/Mosquitto kể cả khi VS Code chưa nhận PATH mới.
$env:PATH = (Split-Path -Parent $compilerPath) + ';' + $previousPath
$extraIncludes = @('-I', "$projectRoot/include")
if (Test-Path 'C:/Program Files/CodeBlocks/MinGW/opt/include') { $extraIncludes += @('-I', 'C:/Program Files/CodeBlocks/MinGW/opt/include') }
if (Test-Path 'C:/Program Files/Mosquitto/devel') { $extraIncludes += @('-I', 'C:/Program Files/Mosquitto/devel') }
$extraLibs = @()
if (Test-Path 'C:/Program Files/Mosquitto/devel') { $extraLibs += @('-L', 'C:/Program Files/Mosquitto/devel') }
if (Test-Path 'C:/Program Files/CodeBlocks/MinGW/opt/lib') { $extraLibs += @('-L', 'C:/Program Files/CodeBlocks/MinGW/opt/lib') }
# MinGW linker can reject an absolute output path containing Vietnamese characters.
# Compile there with ASCII output basenames; run tests from the caller's directory.
try {
    Push-Location -LiteralPath $outputRoot
    try {
        & $compilerPath @extraIncludes '-std=c++17' '-D_WIN32_WINNT=0x0600' '-g' $source @extraLibs '-lmosquitto' '-lcrypto' '-lssl' '-lws2_32' '-o' 'agv_core.exe'
        if ($LASTEXITCODE -ne 0) {throw "Build core thất bại (compiler exit $LASTEXITCODE). Xem lỗi GCC/linker phía trên; đầu ra: $binary"}
        Write-Output "Core: $binary"
        if ($Tests) {
            $testSource = Join-Path $projectRoot 'tests/core_tests.cpp'
            $testBinary = Join-Path $outputRoot 'core_tests.exe'
            & $compilerPath @extraIncludes '-std=c++17' '-D_WIN32_WINNT=0x0600' '-g' $testSource @extraLibs '-lmosquitto' '-lcrypto' '-lssl' '-lws2_32' '-o' 'core_tests.exe'
            if ($LASTEXITCODE -ne 0) {throw 'Build kiểm thử thất bại'}
        }
    } finally {
        Pop-Location
    }
    if ($Tests) {
        $testEnvPath = $env:PATH
        $env:PATH = 'C:\Program Files\Mosquitto;C:\Program Files\CodeBlocks\MinGW\opt\bin;C:\Program Files\CodeBlocks\MinGW\bin;' + $env:PATH
        try {
            & $testBinary
            if ($LASTEXITCODE -ne 0) {throw 'Kiểm thử core thất bại'}
        } finally {
            $env:PATH = $testEnvPath
        }
    }
} finally {
    $env:PATH = $previousPath
}
