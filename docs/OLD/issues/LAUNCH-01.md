# LAUNCH-01 — Launcher freezes after ROM selection

Status: **open, investigation paused**. Opened 2026-08-31.

## Symptom (user-reported)

Launcher window opens normally. After picking the ROM, the launcher window
freezes and stays frozen on screen; no game window ever appears. The user
must kill it.

## What was already found and fixed (does not fix the freeze)

The playable build `build/gs011_opt` had been configured without SDL2
(`SDL2_INCLUDE_DIR-NOTFOUND` / `SDL2_LIBRARY-NOTFOUND` in its CMakeCache), so
`HostWindow::is_available()` compiled to the stub `return false;`
(`gbarecomp/src/runtime/host_window.cpp`). The runner therefore took the
headless fallback (`args.frames = 1`), loaded ROM + BIOS, ran one frame and
exited. Log `logs/session_20260831_181701.log` shows exactly that:
`frames_presented=0`, `run_state.txt` = `clean`.

Actions taken 2026-08-31:

1. Installed `mingw-w64-x86_64-SDL2` 2.32.10 into MSYS2 (it was genuinely
   absent: no headers, no `SDL2.dll`).
2. Reconfigured `build/gs011_opt` with explicit native-path overrides
   `-DSDL2_INCLUDE_DIR=C:/msys64/mingw64/include/SDL2` and
   `-DSDL2_LIBRARY=C:/msys64/mingw64/lib/libSDL2.dll.a` — the default
   Unix-form `GBARECOMP_MINGW_PREFIX_UNIX` does not resolve under the native
   Kitware cmake in use. This mirrors the existing workaround at
   `scripts/gs.ps1:234-235`.
3. Rebuilt (`-j 14`), exit 0, new exe 18:39 (883,151,994 bytes). `objdump -p`
   confirms `SDL2.dll` in the import table, so the real windowed backend is
   compiled in, not the stub.
4. Copied `C:/msys64/mingw64/bin/SDL2.dll` next to the exe, matching the
   existing manual placement of `libgcc_s_seh-1.dll`, `libstdc++-6.dll`,
   `libwinpthread-1.dll` in `build/gs011_opt/`. There is no CMake post-build
   copy step for these.

Build flavour was not changed (`RelWithDebInfo`, `GBARECOMP_STATIC_RELEASE=OFF`,
non-LTO). No game/runtime source was modified. The WIDE-01 work was not touched.

The freeze persists after this rebuild.

## Evidence from the post-rebuild attempt

- `logs/session_20260831_184203.log` contains only the launcher's own
  `input_record=ENABLED` line — no runner output at all.
- `run_state.txt` read `live pid=16172` after the attempt, with neither
  process still running (user killed it), so that entry is stale.
- The empty log is **not** evidence the child died: the child's stdout is a
  pipe and therefore fully buffered, so nothing is flushed until it exits.
  An empty log is consistent with a child that started and kept running.
- Launcher pipe handling was reviewed and looks correct
  (`src/launcher_main.cpp:912-982`): only the write ends are inherited, the
  parent's read ends are marked non-inheritable, and the parent closes its
  write-end copies before starting the pump threads.
- `SDL2.dll` imports only standard Windows DLLs — no missing-dependency risk.

## Leading hypothesis (UNVERIFIED)

`src/launcher_main.cpp:868` calls `DestroyWindow(window)` immediately after
`CreateProcessW` succeeds, and the same thread then blocks in
`out_thread.join()` / `err_thread.join()` (`:981-982`) until the child exits.
`DestroyWindow` needs the message loop to keep running to actually tear the
window down and repaint the desktop behind it. With the thread blocked in the
joins, the launcher window can remain painted on screen and unresponsive —
which matches the user's description of a frozen launcher exactly.

If that is what is happening, the frozen launcher is a *display* artifact and
the real question is why the child never exits and never shows its own window.
That would make this two separate problems, not one.

Note the tension to resolve first: the user reports "nothing ever launches",
but `run_state.txt` recorded a live game pid. Establish whether the 18:42 run
corresponds to the user's frozen attempt before trusting either signal.

## Next steps

1. Confirm whether the game child process actually exists while the launcher
   looks frozen (Task Manager, or check `run_state.txt` during the freeze).
2. Reproduce without the GUI: find the ROM/BIOS paths the launcher persists,
   then run `build/gs011_opt/GoldenSunRecomp.exe` directly with the same
   arguments. This separates "the game hangs / never opens a window" from
   "the launcher's window teardown looks like a freeze". This is the decisive
   experiment and it was not yet run.
3. If the direct run hangs, take one stack with `C:/msys64/mingw64/bin/gdb.exe`
   against the hung pid (the build is RelWithDebInfo).
4. Only then decide where the fix belongs. No fix has been chosen or attempted.

## Not causes (ruled out)

- The `runner_main.cpp` WIDE-01 / OBJ-census diff: gated behind
  `golden_sun_wide_diagnostics_enabled()` / `golden_sun_experimental_fixes_enabled()`,
  both default-off and explicitly zeroed by the launcher unless the checkboxes
  are ticked.
- `roms/` being empty apart from `README.md`: the launcher takes a
  user-chosen path via `GetOpenFileNameW` and does not read from `roms/`.
- `gs.ps1` / `main.toml` / `build_main_toml.py` changes: build-time
  recompiler config, not loaded at runtime.
- `hang_dump.log` / `hang_trace.csv`: stale, from 2026-08-30 13:43.
