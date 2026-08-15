# Runs your normal build and saves the full log to a file.
# Play a battle, use normal attacks and Psynergy, then close the window with the X.
$ErrorActionPreference = "Stop"

$root  = "C:\Users\Jimmy\Desktop\Golden Sun Recompiled"
$dir   = Join-Path $root "build\gs011"
$exe   = Join-Path $dir "GoldenSunRecomp.exe"
$rom   = "C:\Users\Jimmy\Documents\rom\Golden Sun.gba"
$bios  = "C:\Users\Jimmy\Documents\rom\gba_bios.bin"
$state = "C:\Users\Jimmy\Documents\rom\Golden Sun.state6"
$outO  = Join-Path $root "battle_log_out.txt"
$outE  = Join-Path $root "battle_log_err.txt"

if (-not (Test-Path $exe))   { throw "Build not found at $exe" }
if (-not (Test-Path $rom))   { throw "ROM not found at $rom" }
if (-not (Test-Path $bios))  { throw "BIOS not found at $bios" }
if (-not (Test-Path $state)) { throw "Savestate not found at $state" }

# Mutable-RAM native overlays are opt-in; the battle blit code lives there.
$env:GBARECOMP_SELFHEAL_RAM = "1"

# Quote each path: Start-Process splits an unquoted argument on spaces.
$argLine = '--rom "{0}" --bios "{1}" --load-state "{2}" --window' -f $rom, $bios, $state

Write-Host "Starting. Fight a battle, use attacks and Psynergy, then close with the X."

$p = Start-Process -FilePath $exe -WorkingDirectory $dir -ArgumentList $argLine `
        -RedirectStandardOutput $outO -RedirectStandardError $outE -PassThru -Wait

Write-Host ""
Write-Host ("Exited with code " + $p.ExitCode)
foreach ($f in @($outO, $outE)) {
    if (Test-Path $f) {
        Write-Host ("  {0}  ({1} bytes)" -f $f, (Get-Item $f).Length)
    }
}
