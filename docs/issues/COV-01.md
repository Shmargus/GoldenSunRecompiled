# COV-01 — New progression coverage misses

Status: open; no confirmed regression.

## Current evidence

The post-bandit route completed but found new ROM, overlay, and RAM misses.
Current evidence classifies this as coverage work, not a confirmed gameplay
regression.

## Next action

Prove immutable entries against the pinned ELF data, regenerate, and rerun from
the state9 checkpoint.

## Closure condition

Every reported miss is either evidenced and covered or documented as a real
dynamic-RAM boundary; the state9 route completes without unexplained misses.
