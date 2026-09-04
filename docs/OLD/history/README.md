# Closed records

Nothing in this directory is current. These files are kept because they hold
the evidence behind decisions already made — not because they describe how the
project works today.

**Do not act on anything here.** For current state read `docs/STATUS.md`; for
open work read `docs/ACTIVE_ISSUES.md`.

## Completed milestone records (GS-001 … GS-011)

| File | Recorded |
|---|---|
| `TASKS.md` | The original GS-000…GS-011 backlog. All of it is done or superseded. |
| `GBARECOMP_SCAN_BASELINE.md` | GS-006 exact-ROM discovery baseline. |
| `MAIN_SYMBOL_IMPORT.md` | GS-004 main symbol import evidence. |
| `OVERLAY_IMPORT_BASELINE.md` | GS-005 overlay symbol import evidence. |
| `MAIN_TOML_BASELINE.md` | GS-007 main TOML proposal. Its counts were already stale when written. |
| `OVERLAY_RUNTIME_SPIKE.md` | GS-008 same-PC overlay dispatch spike. |
| `RUNNER_BOOTSTRAP.md` | GS-009 first cartridge instruction through the native runner. |
| `ORACLE_HANDOFF_BASELINE.md` | GS-010 BIOS-handoff sync with the mGBA oracle, and the first divergence. |
| `CHECKPOINT_2026-08-06.md` | The first FULLY_STATIC 5,400-frame checkpoint. |

## Superseded handoffs and session logs

| File | Why it is here |
|---|---|
| `TECHNICAL_HANDOFF.md` | The 79 KB handoff. Large parts are correct history, but its "NEXT STEP" section was proven wrong and it was still being cited as current. Replaced by `docs/STATUS.md`. |
| `CODEX_HANDOFF.md` | Working instructions for a Codex "Sol/Luna" team that no longer runs this repo. |
| `SESSION_2026-08-13.md` | Measurement session that killed the halt/idle-pump lead. |
| `SESSION_2026-08-13B.md` | Battle-lag fix and the guest CPU overclock, both since shipped. |
| `HANDOFF_CRASH_SCRIPTED_FIGHT.md` | The first scripted fight crash. Fixed. |
| `VISUAL_ENHANCEMENTS_2026-08-13.md` | Visual work since shipped; its widescreen scope is superseded by WIDE-01 in `docs/ACTIVE_ISSUES.md`. |
| `MP2K_NATIVE_AUDIO_HANDOFF.md` | Native MP2K audio. Paused deliberately — an architecture review found no benefit even on success. Kept so the reasoning is not lost and the work is not restarted by accident. |

## Archived dated handoffs

| File | Why it is here |
|---|---|
| `HANDOFF_2026-08-19.md` | Session handoff covering the unimplemented walk-speed/text-speed work and parked Mercury Lighthouse crash investigation. |
| `HANDOFF_2026-08-20.md` | Battle/menu stutter investigation; the recursion-guard regression is fixed, with remaining hitch work tracked in current issue docs. |

## Obsolete setup guides

| File | Why it is here |
|---|---|
| `BOOTSTRAP.md` | Told you to `git init` and make the first commit. |
| `FIRST_AGENT_TASK.md` | Phase 0 / GS-001 onboarding task, long complete. |
