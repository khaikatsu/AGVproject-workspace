$ErrorActionPreference = "Stop"

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
        # Using [regex]::Escape($key) makes sure the literal string is matched
        # Match #include "FileName.hpp" or #include <FileName.hpp>
        # Let's match: #include\s+(["<])FileName.hpp([">])
        $escKey = [regex]::Escape($key)
        $pattern = '#include\s+(["<])' + $escKey + '([">])'
        
        # Replacement should keep the quotes/brackets
        # $1 captures the start quote/bracket, $2 captures the end
        $replacement = '#include $1' + $includeMap[$key] + '$2'
        
        if ($content -match $pattern) {
            $content = [regex]::Replace($content, $pattern, $replacement)
            $changed = $true
        }
    }
    
    if ($changed) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
    }
}

Write-Output "Safe refactor complete."
