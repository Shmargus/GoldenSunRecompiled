# Parked work

Deferred work with explicit resume conditions. Do not treat these as active
issues.

## CORE-01 — Tail-dispatch refactor

Status: parked. The current tail-dispatch change is not required for the
playable baseline and may affect dynamic-RAM behavior.

Resume when: CRASH-03/05/06/07 evidence is closed, dynamic RAM has validated
writer identity plus full CRC/end validation, and a focused test can compare
the refactor against the current dispatch path.

## BIOS-01 — BIOS-free opt-in mode

Status: parked at the user's request (2026-08-15). The real BIOS remains
required and is the verification oracle.

Resume when: a real-play SWI log covers battles, menus, and map transitions;
Halt/IRQ replacement is implemented in this repository; and a BIOS-free run
matches the real-BIOS run at measured handoff and scene boundaries. Keep this
opt-in; it must never replace the faithful verification path.

## Adaptive/resizable widescreen

Status: parked; fixed Native, 288x160, and 360x240 modes remain the supported
experiment.

Resume when: fixed-mode manual acceptance is complete and a measured scene/
layer policy exists for resize behavior.

## Native MP2K defaults

Status: parked behind the fail-closed experimental path.

Resume when: `features/MP2K.md` acceptance gates pass, including complete
producer ownership, fidelity, independent audio timing, fallback, and user
tests at 1x/2x/4x.
