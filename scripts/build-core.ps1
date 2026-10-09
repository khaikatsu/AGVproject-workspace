param([string]$Compiler = 'g++', [switch]$Tests, [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = if ($OutputDirectory) { [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory)) } else { Join-Path $projectRoot 'build' }
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$source = Join-Path $projectRoot 'src/core/AGVserver.cpp'
$binary = Join-Path $outputRoot 'agv_core.exe'
& $Compiler '-I' "$projectRoot/include" '-std=c++17' $source '-lmosquitto' '-lcrypto' '-lssl' '-lws2_32' '-o' $binary
if ($LASTEXITCODE -ne 0) {throw 'Build core thất bại'}
Write-Output "Core: $binary"
if ($Tests) {
    $testSource = Join-Path $projectRoot 'tests/core_tests.cpp'
    $testBinary = Join-Path $outputRoot 'core_tests.exe'
    & $Compiler '-I' "$projectRoot/include" '-std=c++17' $testSource '-o' $testBinary
    if ($LASTEXITCODE -ne 0) {throw 'Build kiểm thử thất bại'}
    & $testBinary
    if ($LASTEXITCODE -ne 0) {throw 'Kiểm thử core thất bại'}
}
