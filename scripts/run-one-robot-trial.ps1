param(
    [string]$DataRoot = 'data',
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dataPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $DataRoot))
$mapPath = Join-Path $dataPath 'map_layout.json'
if (-not (Test-Path -LiteralPath $mapPath)) {
    throw "Missing trial map: $mapPath"
}
$names = @('AGV_ONE_ROBOT_TRIAL','AGV_HOME_STATIONS','AGV_MAP_PATH',
    'AGV_COMMAND_ID_PATH','AGV_TASK_ID_PATH','AGV_RUNTIME_STATE_PATH','AGV_SHIFT_STATE_PATH')
$previous = @{}
foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $env:AGV_ONE_ROBOT_TRIAL = '1'
    $env:AGV_HOME_STATIONS = '{"01":"S1"}'
    $env:AGV_MAP_PATH = $mapPath
    $env:AGV_COMMAND_ID_PATH = Join-Path $dataPath 'command_sequence.json'
    $env:AGV_TASK_ID_PATH = Join-Path $dataPath 'task_sequence.json'
    $env:AGV_RUNTIME_STATE_PATH = Join-Path $dataPath 'runtime_state.json'
    $env:AGV_SHIFT_STATE_PATH = Join-Path $dataPath 'shift_state.json'
    & (Join-Path $PSScriptRoot 'run-core.ps1') -Build:$Build -OutputDirectory 'build/trial'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$previous[$name],'Process') }
}
