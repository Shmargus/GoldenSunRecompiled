#!/usr/bin/env python3
"""Turn a CPU-headroom capture into the step points for an automatic overclock.

Reads ``logs/headroom_<stamp>.csv`` (written by the GBARECOMP_HEADROOM_PROBE
probe in ``gbarecomp/src/runtime/runtime.cpp``), one row per emulated frame:

    frame, idle_cycles, frame_cycles, overclock, overclock_q8

``overclock`` is the whole multiple in force; ``overclock_q8`` is the
same value in 8.8 fixed point (256 = 1.0x), which is what automatic
mode actually moves, in steps far smaller than 1x. Captures written
before 2026-09-11 have no ``overclock_q8`` column and are read as
whole multiples.

``idle_cycles`` is how much of that frame the guest spent halted, waiting for
the next one. A frame with plenty of idle had cycles to spare; a frame with
none never finished its work, which is exactly what makes Golden Sun stutter.

The automatic mode has to answer two questions with numbers, and this reports
what the capture actually says about each rather than proposing a value:

  * step UP    -- how saturated, for how long, before more cycles are due?
  * step DOWN  -- how much spare, for how long, before they are given back?

Report sections:

  distribution   where frames actually sit, so "plenty" and "none" get defined
                 from this game rather than from intuition
  saturated runs consecutive frames with no idle at all -- the stutter events
                 an automatic mode exists to catch, and how long they last
  spare runs     the opposite, bounding how quickly it is safe to step down
                 without flapping straight back up

Usage:
    python tools/analyze_headroom.py logs/headroom_<stamp>.csv
"""

import argparse
import collections
import csv
import sys

# A frame is called "saturated" when the guest never reached its idle wait at
# all. That is a measured property of the frame, not a tuned threshold: zero
# idle cycles means the work did not fit.
SATURATED_IDLE_CYCLES = 0

# Buckets for the distribution, as a percentage of the frame spent idle.
BUCKETS = (0, 1, 5, 10, 25, 50, 75, 90, 99, 100)


def load(path):
    with open(path, newline='') as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        sys.exit('empty capture: ' + path)
    for row in rows:
        row['frame'] = int(row['frame'])
        row['idle_cycles'] = int(row['idle_cycles'])
        row['frame_cycles'] = int(row['frame_cycles'])
        row['overclock'] = int(row['overclock'])
        row['overclock_q8'] = int(row.get('overclock_q8') or
                                  row['overclock'] * 256)
        row['idle_pct'] = (100.0 * row['idle_cycles'] /
                           row['frame_cycles']) if row['frame_cycles'] else 0.0
    return rows


def runs_of(rows, predicate):
    """Lengths and start frames of consecutive runs matching predicate."""
    out, start, length = [], None, 0
    for row in rows:
        if predicate(row):
            if start is None:
                start = row['frame']
            length += 1
        elif start is not None:
            out.append((start, length))
            start, length = None, 0
    if start is not None:
        out.append((start, length))
    return out


def report(rows):
    total = len(rows)
    factors = collections.Counter('%.3fx' % (r['overclock_q8'] / 256.0)
                                  for r in rows)
    print('%d frames, overclock in force: %s'
          % (total, ', '.join('%s on %d frames' % kv
                              for kv in sorted(factors.items()))))
    print()

    print('=== how much of each frame the guest spent asleep ===')
    counts = collections.Counter()
    for row in rows:
        for edge in reversed(BUCKETS):
            if row['idle_pct'] >= edge:
                counts[edge] += 1
                break
    for edge in BUCKETS:
        n = counts.get(edge, 0)
        if not n:
            continue
        bar = '#' * max(1, int(round(60.0 * n / total)))
        print('  >=%3d%% idle  %6d frames (%5.1f%%)  %s'
              % (edge, n, 100.0 * n / total, bar))
    print()

    saturated = runs_of(rows, lambda r: r['idle_cycles'] <= SATURATED_IDLE_CYCLES)
    frames_saturated = sum(length for _, length in saturated)
    print('=== frames that never reached the idle wait ===')
    print('  %d frames (%.2f%% of the capture) in %d runs'
          % (frames_saturated, 100.0 * frames_saturated / total,
             len(saturated)))
    if saturated:
        lengths = sorted(length for _, length in saturated)
        print('  run length: shortest %d, median %d, longest %d frames'
              % (lengths[0], lengths[len(lengths) // 2], lengths[-1]))
        print('  longest runs (start frame, frames):')
        for start, length in sorted(saturated, key=lambda p: -p[1])[:8]:
            print('     frame %-10d %d frames (%.2fs)'
                  % (start, length, length / 59.7275))
        print()
        print('  Step-up evidence: a run shorter than the step-up delay is a'
              ' stutter Auto would never react to.')
    else:
        print('  None. Nothing in this capture needed more cycles, so it'
              ' cannot justify any step-up point -- capture a summon or a'
              ' large battle effect.')
    print()

    print('=== frames with cycles to spare ===')
    for spare in (50, 75, 90):
        spare_runs = runs_of(rows, lambda r, s=spare: r['idle_pct'] >= s)
        held = sum(length for _, length in spare_runs)
        if not spare_runs:
            print('  >=%d%% idle: never' % spare)
            continue
        lengths = sorted(length for _, length in spare_runs)
        print('  >=%d%% idle: %d frames (%.1f%%) in %d runs, median run %d,'
              ' longest %d'
              % (spare, held, 100.0 * held / total, len(spare_runs),
                 lengths[len(lengths) // 2], lengths[-1]))
    print()
    print('  Step-down evidence: a step-down delay shorter than the gaps'
          ' between saturated runs will hand cycles back mid-effect and take'
          ' them again immediately.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', help='logs/headroom_<stamp>.csv')
    args = parser.parse_args()
    report(load(args.capture))


if __name__ == '__main__':
    main()
