$ErrorActionPreference = "Stop"

$files = Get-ChildItem -Path "src", "include" -Include *.cpp, *.hpp -Recurse

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    
    # The bad prefix starts with "#include `"map/MapGraph.hpp`"utils/AtomicFile.hpp`"" and ends with "`"utils/Identifiers.hpp`""
    # Let's match: #include\s+"map/MapGraph\.hpp"utils/AtomicFile\.hpp".*?utils/Identifiers\.hpp"
    # Actually, we can just match any long chain of "..."..."..." 
    # Let's match #include\s+"map/MapGraph\.hpp".*?"([^"]+)>
    # But some end in "
    
    # A safer regex: match lines starting with optionally spaces, #include, then a corrupted string up to the actual filename.
    # Since we know the corruption always starts with "map/MapGraph.hpp"utils/AtomicFile.hpp"
    $pattern_bracket = '(?m)^[ \t]*#include\s+"map/MapGraph\.hpp"utils/AtomicFile\.hpp".*?utils/Identifiers\.hpp"(.*?)>'
    $pattern_quote = '(?m)^[ \t]*#include\s+"map/MapGraph\.hpp"utils/AtomicFile\.hpp".*?utils/Identifiers\.hpp"(.*?)"'
    
    if ($content -match $pattern_bracket -or $content -match $pattern_quote) {
        $content = [regex]::Replace($content, $pattern_bracket, '#include <$1>')
        $content = [regex]::Replace($content, $pattern_quote, '#include "$1"')
        Set-Content -Path $file.FullName -Value $content -NoNewline
    }
}

Write-Output "Fix complete."
