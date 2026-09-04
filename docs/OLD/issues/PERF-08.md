# PERF-08 — Residual stutter and dynamic-RAM churn

Status: open; route remains **NOT_STATIC**.

## Current evidence

Severe first-use stalls are gone. Clean-run p99 was 14.70 ms and worst was
44.51 ms; the measured variant-cache cap is 10. Unregistered RAM PCs and
zero-bridge frames remain. The validated Bilibin replays are clean, but the
widened pool-LDM probe is diagnostic-only.

## Next action

Keep full CRC/end validation and repeat the manual transition route while
recording bounded self-heal and dispatch evidence.

## Closure condition

Measured remaining stalls have a proven cause or are within the accepted
performance boundary, and the route has no unresolved dynamic-RAM coverage.
