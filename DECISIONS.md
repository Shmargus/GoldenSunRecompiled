# Architecture Decision Log

Record durable decisions here. Add an ADR in `docs/adr/` if a decision needs deeper analysis.

## D-001 — Windows x86-64 is the first host target

Status: accepted.

Reason: it matches the initial user's environment and proves ARM/THUMB → host-native translation clearly. Architecture must remain portable enough for later Linux/macOS builds.

## D-002 — Faithful 240×160 baseline precedes enhancements

Status: accepted.

Reason: expanded view, timing changes, and replacement presentation make oracle comparison harder and can conceal baseline faults.

## D-003 — `gbarecomp` is the platform core

Status: accepted. Commit and license pinned 2026-07-18 (see `UPSTREAM.md` and `docs/GBRECOMP_BASELINE.md`).

Reason: it already provides the ARMv4T and GBA runtime layers. Rebuilding those from scratch would multiply scope.

Caveat: the local checkout carries generic fixes not yet upstreamed, so the public pin must not be advanced and gates depending on those fixes are not yet reproducible from the pin alone. Still open as of 2026-09-04; the `gbarecomp` submodule carries uncommitted work from several sessions.

## D-004 — `gsret/goldensun` is consumed through local metadata import

Status: accepted.

Reason: it is valuable layout evidence but has no visible root license. The project should not vendor its assembly.

## D-005 — Overlay identity is part of dispatch correctness

Status: accepted.

Reason: multiple Golden Sun map overlays can use shared runtime addresses. Runtime PC alone may select stale translated code.

## D-006 — Generated code is not reviewed source

Status: accepted.

Reason: correcting generated output by hand hides defects in discovery/config/codegen and breaks reproducibility.

## D-007 — The goal is native execution plus modifiability, not readable source

Status: accepted 2026-09-04.

Reason: stated directly by the user. Run the game on native C++ with the ROM as
assets only, and be able to change how it works — widescreen, turbo, walk
speeds, replacement item and Psynergy tables. Readable or idiomatic generated
code is explicitly not wanted; a faithful but ugly native implementation counts
as success.

Consequence: the code half of this is already largely met by the existing
recompilation. The remaining work is in the hardware layer and in the ability
to modify behaviour.

## D-008 — Draw the field from map data, not per pixel

Status: accepted 2026-09-04. Supersedes the per-pixel margin approach.

Reason: the previous widescreen implementation hooked the PPU's per-texel tile
lookup and invented margin content, then culled wrong answers. That single
design caused both the cost — a branching host callback per margin pixel, not
the pixel count — and the visual defects. Three culls failed against it on
2026-09-03.

Decision: reconstruct a room into a buffer when it is entered and render from
that, so a margin pixel is an array lookup and nothing is invented. The
widescreen implementation was removed entirely to give this a clean slate.

A GPU/OpenGL renderer is **not** required for this and is not planned. Earlier
scoping framed it that way; that was more than is needed.

## D-009 — Full decompilation is out of scope

Status: accepted 2026-09-04.

Reason: roughly 6,000 real functions and 1.3 MB of code, comparable to
decompilation projects that took communities years. It is also unnecessary —
D-007 does not require readable source. Targeted finds of specific data and
functions serve the goal; wholesale translation does not.

## D-010 — Native audio work is shelved, not abandoned

Status: shelved 2026-09-04.

Reason: focus is on map data and native presentation. The native MP2K path was
experimental and fail-closed, with the canonical hardware path as the oracle;
its last probation evidence failed on correlation and ratio, and producer
ownership, fidelity and clock completion were all still open.

State is preserved in `docs/features/MP2K.md` and `docs/OLD/issues/AUD-*.md`.
Do not resume without saying so first.
