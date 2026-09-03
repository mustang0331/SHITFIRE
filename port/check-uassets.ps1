# check-uassets.ps1 - binary-asset tripwire (UNREAL_RESEARCH.md 8).
#
# Fails (exit 1) listing every *.uasset / *.umap under port/unreal that does
# not match port/unreal/uasset-allowlist.txt. A GUI-tweaked or hand-made
# asset that slips into the repo unlisted becomes permanently invisible to
# the coding agents - this script is the pre-commit-discipline gate every
# P3/P4 agent runs before committing anything under port/unreal.
#
# Allowlist format: one entry per line, relative to port/unreal, forward
# slashes; entries are PowerShell -like patterns (so * wildcards work, e.g.
# Plugins/RealtimeMeshComponent/* for the vendored plugin's example content).
# Lines starting with # are comments.
#
# Usage:  powershell -ExecutionPolicy Bypass -File port\check-uassets.ps1
# Exit:   0 = clean, 1 = unlisted binary assets found, 2 = setup error.

$ErrorActionPreference = 'Stop'
$unrealDir = Join-Path $PSScriptRoot 'unreal'
$allowlistPath = Join-Path $unrealDir 'uasset-allowlist.txt'

if (-not (Test-Path $unrealDir)) { Write-Host "check-uassets: no port/unreal directory"; exit 2 }
if (-not (Test-Path $allowlistPath)) { Write-Host "check-uassets: missing $allowlistPath"; exit 2 }

$patterns = @(Get-Content $allowlistPath | ForEach-Object { $_.Trim() } |
    Where-Object { $_ -ne '' -and -not $_.StartsWith('#') })

# Build-byproduct dirs are git-ignored and full of transient copies
# (Saved/Autosaves etc.) - scan only what a commit could contain.
$excludeRe = '[\\/](Intermediate|Saved|DerivedDataCache|Binaries|Build)[\\/]'

$assets = @(Get-ChildItem -Path $unrealDir -Recurse -File |
    Where-Object { $_.Extension -in '.uasset', '.umap' } |
    Where-Object { $_.FullName -notmatch $excludeRe })

$offenders = @()
$prefixLen = $unrealDir.Length + 1
foreach ($f in $assets) {
    $rel = $f.FullName.Substring($prefixLen).Replace('\', '/')
    $ok = $false
    foreach ($p in $patterns) { if ($rel -like $p) { $ok = $true; break } }
    if (-not $ok) { $offenders += $rel }
}

# Drift check (non-fatal): allowlist entries that match nothing on disk.
foreach ($p in $patterns) {
    if ($p.Contains('*')) { continue }
    if (-not (Test-Path (Join-Path $unrealDir $p))) {
        Write-Host "check-uassets: NOTE allowlisted but not on disk: $p"
    }
}

if ($offenders.Count -gt 0) {
    Write-Host "check-uassets: FAIL - binary assets not in uasset-allowlist.txt:"
    $offenders | ForEach-Object { Write-Host "  $_" }
    Write-Host "Either regenerate via the checked-in tools (preferred) and allowlist"
    Write-Host "the output, or delete the asset. Never commit an unlisted .uasset."
    exit 1
}

Write-Host "check-uassets: OK ($($assets.Count) assets, all allowlisted)"
exit 0
