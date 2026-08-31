<#
.SYNOPSIS
  Launch a scripted replay through GoldenSunLauncher.exe, on the secondary
  monitor, without stealing focus from whatever the user is doing on the
  primary one.

.DESCRIPTION
  Agents drive replays through the launcher's developer-replay seam
  (GBARECOMP_AUTO_LAUNCH and friends — see src/launcher_replay_policy.h),
  never by invoking build/gs011_opt/GoldenSunRecomp.exe directly, because the
  launcher is what turns on RAM self-heal.

  This wrapper does not touch that seam or any game code. It only:
    1. sets the inherited replay env vars for the child process,
    2. starts GoldenSunLauncher.exe,
    3. waits for the game window ("Golden Sun Recompiled") to appear,
    4. moves it onto the non-primary monitor and shows it with
       SWP_NOACTIVATE, so it never takes keyboard/mouse focus,
    5. restores whatever window had focus before step 2, best-effort.

  On a single-monitor machine it launches normally and leaves the window
  wherever Windows/SDL puts it — it does not guess a placement.

.EXAMPLE
  .\scripts\gs-replay.ps1 -LoadState state1.sav -ReplayFrames 600 -Mute
#>
[CmdletBinding()]
param(
    # Passed through to GBARECOMP_LOAD_STATE.
    [string]$LoadState,

    # Passed through to GBARECOMP_REPLAY_FRAMES.
    [string]$ReplayFrames,

    # Passed through to GBARECOMP_INPUT_REPLAY.
    [string]$InputReplay,

    # Passed through to GBARECOMP_VRAM_MAP_TRACE.
    [string]$VramMapTrace,

    # Sets SDL_AUDIODRIVER=dummy for the child process.
    [switch]$Mute,

    # Seconds to wait for the game window to appear before giving up on
    # placement (the process still launches either way).
    [int]$WindowTimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Launcher = Join-Path $RepoRoot 'GoldenSunLauncher.exe'
if (-not (Test-Path $Launcher)) {
    throw "GoldenSunLauncher.exe not found at $Launcher. Build it first."
}

$WindowTitle = 'Golden Sun Recompiled'

Add-Type -Namespace GsReplay -Name Win32 -MemberDefinition @'
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll", CharSet = CharSet.Auto)]
public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
    int X, int Y, int cx, int cy, uint uFlags);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
[DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
'@

Add-Type -AssemblyName System.Windows.Forms

# --- env vars for the replay seam ----------------------------------------

if ($LoadState)    { $env:GBARECOMP_LOAD_STATE = $LoadState }    else { Remove-Item Env:\GBARECOMP_LOAD_STATE -ErrorAction SilentlyContinue }
if ($ReplayFrames) { $env:GBARECOMP_REPLAY_FRAMES = $ReplayFrames } else { Remove-Item Env:\GBARECOMP_REPLAY_FRAMES -ErrorAction SilentlyContinue }
if ($InputReplay)  { $env:GBARECOMP_INPUT_REPLAY = $InputReplay }  else { Remove-Item Env:\GBARECOMP_INPUT_REPLAY -ErrorAction SilentlyContinue }
if ($VramMapTrace) { $env:GBARECOMP_VRAM_MAP_TRACE = $VramMapTrace } else { Remove-Item Env:\GBARECOMP_VRAM_MAP_TRACE -ErrorAction SilentlyContinue }
$env:GBARECOMP_AUTO_LAUNCH = '1'
if ($Mute) { $env:SDL_AUDIODRIVER = 'dummy' } else { Remove-Item Env:\SDL_AUDIODRIVER -ErrorAction SilentlyContinue }

# --- figure out the secondary monitor before launch -----------------------

$screens = [System.Windows.Forms.Screen]::AllScreens
$secondary = $screens | Where-Object { -not $_.Primary } | Select-Object -First 1
if (-not $secondary) {
    Write-Host "Only one monitor detected; launching without repositioning the window."
}

$previousForeground = [GsReplay.Win32]::GetForegroundWindow()

Write-Host "Launching: $Launcher"
$process = Start-Process -FilePath $Launcher -PassThru

if ($secondary) {
    $bounds = $secondary.Bounds
    $deadline = (Get-Date).AddSeconds($WindowTimeoutSeconds)
    $target = $null
    while ((Get-Date) -lt $deadline) {
        $target = Get-Process -Name 'GoldenSunRecomp' -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowTitle -eq $WindowTitle -and $_.MainWindowHandle -ne 0 } |
            Select-Object -First 1
        if ($target) { break }
        Start-Sleep -Milliseconds 200
    }

    if ($target) {
        $hwnd = $target.MainWindowHandle
        # SWP_NOACTIVATE (0x0010) | SWP_NOSIZE (0x0001) | SWP_NOZORDER (0x0004):
        # move only, keep the window's own size, never activate it.
        [void][GsReplay.Win32]::SetWindowPos($hwnd, [IntPtr]::Zero, $bounds.X, $bounds.Y, 0, 0, 0x0015)
        Write-Host "Moved '$WindowTitle' to secondary monitor at $($bounds.X),$($bounds.Y)."

        if ($previousForeground -ne [IntPtr]::Zero -and [GsReplay.Win32]::IsWindow($previousForeground)) {
            [void][GsReplay.Win32]::SetForegroundWindow($previousForeground)
        }
    } else {
        Write-Host "Timed out waiting for the '$WindowTitle' window; leaving placement to Windows/SDL."
    }
}

return $process
