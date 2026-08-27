# CRASH-07 — Town re-entry RAM dispatch recursion

Status: open pending manual retest.

## Current evidence

Session `110405` showed `EXCEPTION_STACK_OVERFLOW` from
`verified_ram_dispatch -> runtime_dispatch_miss -> overlay_try_dispatch ->
runtime_dispatch`. Active/depth rejection now calls
`runtime_bridge_interpret` directly. Session `111651` measured 11,036,278
interpreted instructions after five valid same-depth RAM loop redispatches were
misclassified; session `112549` measured another 1,630,223 after a valid
`0x03000828` guest loop was misclassified. Validated PCs now remain native
tail transfers and guest call-stack state preserves recursion semantics.

## Next action

Repeatedly enter and leave a town through the root launcher and compare
interpreted instruction counts with the pre-change native path.

## Closure condition

No stack overflow or unsafe retry occurs, and valid same-depth RAM loops do not
regress into large bridge interpretation during the manual transition route.
