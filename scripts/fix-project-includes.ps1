[CmdletBinding(SupportsShouldProcess)]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& "$PSScriptRoot/check-vendor.ps1" -VendorOnly
# Explicit application directories; never traverse vendor/dependencies/builds.
$roots = @('src/core','include/core','include/commands','include/map')
$pattern = '(?m)^([ \t]*)#include\s+"map/MapGraph\.hpp"utils/AtomicFile\.hpp"[^\r\n]*?utils/Identifiers\.hpp"([^">\r\n]+)([">])'
foreach ($root in $roots) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot $root) -Recurse -File | Where-Object { $_.Extension -in '.hpp','.cpp' }) {
        $source = [IO.File]::ReadAllText($file.FullName)
        if ($source -notmatch $pattern) { continue }
        $updated = [regex]::Replace($source,$pattern,{param($match)
            $opening = if ($match.Groups[3].Value -eq '>') { '<' } else { '"' }
            $match.Groups[1].Value + '#include ' + $opening + $match.Groups[2].Value + $match.Groups[3].Value
        })
        if ($PSCmdlet.ShouldProcess($file.FullName,'Repair application include directive')) {
            [IO.File]::WriteAllText($file.FullName,$updated,[Text.UTF8Encoding]::new($false))
        }
    }
}
& "$PSScriptRoot/check-vendor.ps1"
