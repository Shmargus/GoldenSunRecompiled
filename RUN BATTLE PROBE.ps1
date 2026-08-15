# Runs the instrumented probe build and captures the dynamic-RAM table.
# Play a battle, use attacks/Psynergy, then close the window with the X.
$ErrorActionPreference = "Stop"

$root  = "C:\Users\Jimmy\Desktop\Golden Sun Recompiled"
$exe   = Join-Path $root "build\gs_probe\GoldenSunRecomp.exe"
$rom   = "C:\Users\Jimmy\Documents\rom\Golden Sun.gba"
$bios  = "C:\Users\Jimmy\Documents\rom\gba_bios.bin"
$state = "C:\Users\Jimmy\Documents\rom\Golden Sun.state7"
$outO  = Join-Path $root "battle_probe_out.txt"
$outE  = Join-Path $root "battle_probe_err.txt"

if (-not (Test-Path $exe))   { throw "Probe build not found at $exe" }
if (-not (Test-Path $rom))   { throw "ROM not found at $rom" }
if (-not (Test-Path $bios))  { throw "BIOS not found at $bios" }
if (-not (Test-Path $state)) { throw "Savestate not found at $state" }

$env:GSR_DYNAMIC_RAM_PROBE = "1"
$env:GSR_DYNAMIC_RAM_PROBE_WINDOW = "64"

# Match src/launcher_main.cpp's run_game() launch environment exactly (the
# working GoldenSunLauncher.exe -> build/gs011 path), or self-heal RAM stays
# off and the on-disk overlay cache resolves to an empty relative directory:
#   - GBARECOMP_SELFHEAL_RAM=1: without this, mutable-RAM native overlays are
#     interpreter-only ("self_heal_ram=DISABLED"), which is the whole tier
#     under investigation.
#   - GBARECOMP_HEAL_CACHE=<absolute recomp_cache path>: overlay_loader_init()
#     always passes the literal relative string "recomp_cache" as its default
#     (gbarecomp/src/runtime/runtime.cpp:1391); GBARECOMP_HEAL_CACHE is the
#     only override. Without it, "recomp_cache" resolves against whatever the
#     process's CWD happens to be, which produced the empty
#     rom_bios=0 ram=0 warm-load the first time this script ran.
#   - GBARECOMP_PRESENT_IN_PLACE=0 / GBARECOMP_AUDIO_NATIVE=0 /
#     GBARECOMP_AUDIO_STEREO=1: same behavior parity settings the launcher
#     sets, so this probe run matches the user's normal play session.
$env:GBARECOMP_SELFHEAL_RAM = "1"
$env:GBARECOMP_HEAL_CACHE = Join-Path $root "recomp_cache"
$env:GBARECOMP_PRESENT_IN_PLACE = "0"
$env:GBARECOMP_AUDIO_NATIVE = "0"
$env:GBARECOMP_AUDIO_STEREO = "1"

Write-Host "Starting probe build. Fight a battle, then close the window with the X."
Write-Host "The [dynamic-ram-probe] table prints to stderr ($outE) when the window closes."

# Windows PowerShell 5.1's Start-Process -ArgumentList joins an array with
# spaces and does NOT quote elements — every one of these paths ("Golden
# Sun.gba", "Golden Sun.state6") has a space in it, so an array here truncates
# each --rom/--bios/--load-state value at its first space. Build one
# pre-quoted argument string instead.
$argLine = '--rom "{0}" --bios "{1}" --load-state "{2}" --window' -f $rom, $bios, $state

$p = Start-Process -FilePath $exe -ArgumentList $argLine `
     -WorkingDirectory $root `
     -RedirectStandardOutput $outO -RedirectStandardError $outE -PassThru -Wait

Write-Host ""
Write-Host ("Exited with code " + $p.ExitCode)
foreach ($f in @($outO, $outE)) {
    if (Test-Path $f) {
        $len = (Get-Item $f).Length
        Write-Host ("  {0}  ({1} bytes)" -f $f, $len)
    }
}
