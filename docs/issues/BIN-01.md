# BIN-01 — RelWithDebInfo binary size

Status: open; non-blocking.

## Current evidence

`build/gs011_opt/GoldenSunRecomp.exe` is about 882 MB, mostly debug info.
Crash symbols currently come from the generated symbol map. LTO remains off.

## Next action

If distribution size matters, measure a stripped/Release artifact against speed
and diagnostic usefulness.

## Closure condition

A measured artifact choice documents size, performance, and crash-diagnostic
tradeoffs, or the issue is explicitly accepted as non-blocking.
