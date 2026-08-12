# Architecture Decision Log

Record durable decisions here. Add an ADR in `docs/adr/` if a decision needs deeper analysis.

## D-001 — Windows x86-64 is the first host target

Status: accepted.

Reason: it matches the initial user's environment and proves ARM/THUMB → host-native translation clearly. Architecture must remain portable enough for later Linux/macOS builds.

## D-002 — Faithful 240×160 baseline precedes enhancements

Status: accepted.

Reason: expanded view, timing changes, and replacement presentation make oracle comparison harder and can conceal baseline faults.

## D-003 — `gbarecomp` is the platform core

Status: provisional until commit and license are pinned.

Reason: it already provides the ARMv4T and GBA runtime layers. Rebuilding those from scratch would multiply scope.

## D-004 — `gsret/goldensun` is consumed through local metadata import

Status: accepted.

Reason: it is valuable layout evidence but has no visible root license. The project should not vendor its assembly.

## D-005 — Overlay identity is part of dispatch correctness

Status: accepted.

Reason: multiple Golden Sun map overlays can use shared runtime addresses. Runtime PC alone may select stale translated code.

## D-006 — Generated code is not reviewed source

Status: accepted.

Reason: correcting generated output by hand hides defects in discovery/config/codegen and breaks reproducibility.
