param([switch]$VendorOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$header = Join-Path $projectRoot 'include/utils/json.hpp'
$expected = 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63'
if ((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash -ne $expected) {
    throw 'Vendor nlohmann/json v3.12.0 changed; restore the pinned original before continuing.'
}
if (-not $VendorOnly) {
    # VS Code's Windows PowerShell may have no ripgrep on PATH.
    $sourceRoots = @((Join-Path $projectRoot 'include'), (Join-Path $projectRoot 'src'))
    $sourceFiles = Get-ChildItem -LiteralPath $sourceRoots -Recurse -File |
        Where-Object { $_.Extension -in @('.h', '.hh', '.hpp', '.hxx', '.c', '.cc', '.cpp', '.cxx') -and $_.FullName -ne $header }
    $bad = @($sourceFiles |
        Select-String -SimpleMatch -Pattern '"map/MapGraph.hpp"utils/AtomicFile.hpp"' -List |
        Select-Object -ExpandProperty Path -Unique)
    if ($bad.Count) { throw "Corrupted includes: $($bad -join ', ')" }
    Write-Output 'PASS vendor v3.12.0 SHA256 and application include corruption scan'
} else {
    Write-Output 'PASS vendor v3.12.0 SHA256'
}
