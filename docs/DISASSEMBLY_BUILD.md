# Golden Sun Disassembly Build Record

Status: reproduced locally on Windows on 2026-07-18.

## Inputs

```text
ROM SHA-1: 5c4695205413df7db52b9a184815a07783999971
disassembly commit: 0fa7b312199c10b96544e825be86cfc476493eb7
host OS/environment: Windows 11 10.0.26200, MSYS2 MSYS + UCRT64
GNU make version: 4.4.1
arm-none-eabi binutils version: 2.46.1
host compiler version: MSYS2 GCC 15.3.0 (/usr/bin/gcc)
```

## Commands

```powershell
# In the separate local goldensun-disasm checkout. baserom.gba is an
# ignored NTFS hard link to the hash-verified private ROM.
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"
C:\msys64\usr\bin\bash.exe -lc "make -j4 CC=/usr/bin/gcc"

# Git for Windows checked goldensun.sha1 out with CRLF, so the aggregate
# compare-rom recipe treated the carriage return as part of the filename.
# Preserve the upstream checkout and perform the equivalent exact checks:
C:\msys64\usr\bin\bash.exe -lc `
  "make -j4 compare-overlays CC=/usr/bin/gcc && tr -d '\r' < goldensun.sha1 | sha1sum -c -"
```

## Outputs

```text
main ELF/map: goldensun.elf, goldensun.map
overlay count: 96
overlay output pattern: overlays/rom_*/overlay.{elf,map,bin,lz}
all comparisons pass: yes; all 96 uncompressed overlays match and the rebuilt
main ROM SHA-1 is 5c4695205413df7db52b9a184815a07783999971
```

## README/status reconciliation

The checked-out revision does rebuild compressed overlays and links them into the
final ROM. The README statement that overlays are not built into the output ROM
is stale relative to commit `0fa7b312` (`Compress overlays during build`). The
Makefile's `OVERLAY_LZS` dependency and the matching final ROM prove the current
behavior.

The linker reports RWX load-segment warnings for the main ELF and overlay ELFs;
these are recorded warnings, not comparison failures.
