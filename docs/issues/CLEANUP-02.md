# CLEANUP-02 — Cost-probe IRQ instrumentation

Status: open; defer until current playtest work is stable.

## Current evidence

`runtime_irq()` still references diagnostic counters and isolated codegen tests
need stubs.

## Next action

After current playtest work, remove the instrumentation and stubs or document
the intentional runtime cost. Do not change IRQ behavior mid-test.

## Closure condition

Production IRQ code and isolated tests have an explicit, measured instrumentation
decision with no behavior change.
