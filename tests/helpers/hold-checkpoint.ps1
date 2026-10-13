param([string]$Path,[string]$Held,[string]$Release)
$ErrorActionPreference='Stop'
$handle=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    [IO.File]::WriteAllText($Held,'locked')
    $deadline=[DateTime]::UtcNow.AddSeconds(25)
    while (-not [IO.File]::Exists($Release) -and [DateTime]::UtcNow -lt $deadline) {Start-Sleep -Milliseconds 25}
} finally {$handle.Dispose()}
