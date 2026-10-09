$ErrorActionPreference = "Stop"

$projectRoot = "c:\Users\DELL\OneDrive\noobcoder\AGVproject"
cd $projectRoot

# Define folders
$includeDirs = @("include\core", "include\map", "include\mqtt", "include\utils", "include\commands")
$srcDirs = @("src\core", "src\map", "src\mqtt", "src\utils", "src\test")

# Create directories
foreach ($dir in ($includeDirs + $srcDirs)) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
}

# Move mapping
$fileMoves = @(
    @{ src = "src\AGVserver.cpp"; dest = "src\core\AGVserver.cpp"; module = "core" },
    @{ src = "src\FLeetmanager.hpp"; dest = "include\core\FLeetmanager.hpp"; module = "core" },
    @{ src = "src\ShiftController.hpp"; dest = "include\core\ShiftController.hpp"; module = "core" },
    @{ src = "src\RuntimeStore.hpp"; dest = "include\core\RuntimeStore.hpp"; module = "core" },
    
    @{ src = "src\AGVmap.cpp"; dest = "src\map\AGVmap.cpp"; module = "map" },
    @{ src = "src\MapGraph.hpp"; dest = "include\map\MapGraph.hpp"; module = "map" },
    
    @{ src = "src\AGVMQTT.cpp"; dest = "src\mqtt\AGVMQTT.cpp"; module = "mqtt" },
    
    @{ src = "src\WebCommands.hpp"; dest = "include\commands\WebCommands.hpp"; module = "commands" },
    @{ src = "src\CommandIds.hpp"; dest = "include\commands\CommandIds.hpp"; module = "commands" },
    
    @{ src = "src\AtomicFile.hpp"; dest = "include\utils\AtomicFile.hpp"; module = "utils" },
    @{ src = "src\Mutexarbiter.hpp"; dest = "include\utils\Mutexarbiter.hpp"; module = "utils" },
    @{ src = "src\Identifiers.hpp"; dest = "include\utils\Identifiers.hpp"; module = "utils" },
    @{ src = "src\json.hpp"; dest = "include\utils\json.hpp"; module = "utils" },
    
    @{ src = "src\AGVtest.cpp"; dest = "src\test\AGVtest.cpp"; module = "test" }
)

# Move files
foreach ($move in $fileMoves) {
    if (Test-Path $move.src) {
        Move-Item -Path $move.src -Destination $move.dest -Force
    }
}

# Move data
if (Test-Path "src\map_layout.json") { Move-Item "src\map_layout.json" "data\map_layout.json" -Force }
if (Test-Path "src\data\*") { Move-Item "src\data\*" "data\" -Force }

# Move old root files to backup
if (-not (Test-Path "backup")) { New-Item -ItemType Directory -Force -Path "backup" | Out-Null }
if (Test-Path "FLeetmanager.hpp") { Move-Item "FLeetmanager.hpp" "backup\FLeetmanager.hpp" -Force }
if (Test-Path "MapGraph.hpp") { Move-Item "MapGraph.hpp" "backup\MapGraph.hpp" -Force }

# Fix includes in all .cpp and .hpp files
$includeMap = @{
    "FLeetmanager.hpp" = "core/FLeetmanager.hpp"
    "ShiftController.hpp" = "core/ShiftController.hpp"
    "RuntimeStore.hpp" = "core/RuntimeStore.hpp"
    "MapGraph.hpp" = "map/MapGraph.hpp"
    "WebCommands.hpp" = "commands/WebCommands.hpp"
    "CommandIds.hpp" = "commands/CommandIds.hpp"
    "AtomicFile.hpp" = "utils/AtomicFile.hpp"
    "Mutexarbiter.hpp" = "utils/Mutexarbiter.hpp"
    "Identifiers.hpp" = "utils/Identifiers.hpp"
    "json.hpp" = "utils/json.hpp"
}

$filesToUpdate = Get-ChildItem -Path "src", "include" -Include *.cpp, *.hpp -Recurse

foreach ($file in $filesToUpdate) {
    $content = Get-Content -Path $file.FullName -Raw
    $changed = $false
    
    foreach ($key in $includeMap.Keys) {
        # regex for `#include "FileName.hpp"` or `#include <FileName.hpp>`
        $pattern = "#include\s+[`"<$key>`"]"
        $replacement = "#include `"" + $includeMap[$key] + "`""
        if ($content -match $pattern) {
            $content = $content -replace $pattern, $replacement
            $changed = $true
        }
    }
    
    if ($changed) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
    }
}

Write-Output "Refactoring complete."
