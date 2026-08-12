# Workspace Layout

Recommended Windows layout:

```text
C:\Dev\GoldenSunWorkspace\
├─ GoldenSunRecomp\
├─ gbarecomp\
├─ goldensun-disasm\
└─ private\
   ├─ goldensun.gba
   ├─ gba_bios.bin
   ├─ saves\
   └─ traces\
```

Set optional environment variables:

```powershell
$env:GSR_ROM = "C:\Dev\GoldenSunWorkspace\private\goldensun.gba"
$env:GSR_BIOS = "C:\Dev\GoldenSunWorkspace\private\gba_bios.bin"
$env:GBARECOMP_ROOT = "C:\Dev\GoldenSunWorkspace\gbarecomp"
$env:GSR_DISASM_ROOT = "C:\Dev\GoldenSunWorkspace\goldensun-disasm"
```

Never write scripts that default to copying private inputs into the repository.
