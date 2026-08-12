[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [Parameter(Mandatory = $true)]
    [string]$GbarecompPath,

    [Parameter(Mandatory = $true)]
    [string]$DisasmPath
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

Write-Host "GoldenSunRecomp bootstrap checks" -ForegroundColor Cyan
Write-Host "Repository: $Root"

py "$Root\tools\inspect_environment.py"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

py "$Root\tools\verify_rom.py" $RomPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$GbarecompCmake = Join-Path $GbarecompPath "CMakeLists.txt"
if (-not (Test-Path $GbarecompCmake -PathType Leaf)) {
    throw "gbarecomp checkout not found at: $GbarecompPath"
}

$DisasmMakefile = Join-Path $DisasmPath "Makefile"
if (-not (Test-Path $DisasmMakefile -PathType Leaf)) {
    throw "Golden Sun disassembly checkout not found at: $DisasmPath"
}

Write-Host ""
Write-Host "OK: paths and exact ROM are valid." -ForegroundColor Green
Write-Host "Next: pin upstream commits in UPSTREAM.md and complete GS-001/GS-002."
Write-Host "This script intentionally does not copy the ROM or generate guessed config."
