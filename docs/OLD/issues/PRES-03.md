# PRES-03 — Ivan/world-map shimmer

Status: open.

## Current evidence

World-map interpolation and supersampling fail endpoint gates. Soft filtering
and temporal blend remain opt-in mitigations.

## Next action

Identify the Ivan-specific source before widening either fallback.

## Closure condition

The source is measured and the selected fallback preserves endpoint correctness
with a documented visual result.
