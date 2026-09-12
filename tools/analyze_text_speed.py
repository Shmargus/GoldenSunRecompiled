#!/usr/bin/env python3
"""Find the per-frame letter budget behind Golden Sun's Message speed option.

Reads ``text_speed.csv`` from a function-tracer session directory (written by
the GSR_TEXT_RECORD probe in ``src/function_tracer.cpp``) and answers two
questions:

1. How fast do letters actually appear at each Message-speed setting?
2. Which field of the text-context struct carries that setting?

Question 2 is the point. FACTS.md (2026-09-10) measured that Message speed
changes how many letters the text processor emits per frame -- 1 on Normal,
about 3 on Fast -- so the control is a per-frame budget somewhere in that
struct, and the probe dumps 64 bytes at every pointer argument of every
processor call. This
diffs those dumps across the two settings rather than assuming the offset:
a byte that holds one value throughout Normal dialogue and a different value
throughout Fast dialogue is a candidate; a byte that also varies inside a
single setting is not, and is reported separately as noise.

Nothing here reads the ROM, and the dumps it consumes live only under
``logs/`` (gitignored).

Usage:
    python tools/analyze_text_speed.py logs/trace_<stamp>
"""

import argparse
import collections
import csv
import os
import sys

# Letter frames closer together than this belong to the same line being
# revealed; a wider gap is a line break, a box opening, or the player
# pressing A. Chosen to sit well above the 1-2 frame spacing measured at both
# settings and well below the 65+ frame gaps between lines in the same
# sessions -- it separates runs, it is not a timing threshold.
RUN_GAP_FRAMES = 12

# A run shorter than this is a fragment (a name, a prompt) rather than a line
# worth quoting a rate from.
MIN_RUN_LETTERS = 5


def load(session_dir):
    path = os.path.join(session_dir, 'text_speed.csv')
    if not os.path.exists(path):
        sys.exit('no text_speed.csv in ' + session_dir +
                 ' -- was "Record text progression" ticked?')
    with open(path, newline='') as handle:
        rows = list(csv.DictReader(handle))
    for row in rows:
        row['frame'] = int(row['frame'])
    if rows and 'ctx' not in rows[0]:
        print('note: this capture predates the context dump; '
              'offset analysis unavailable\n')
    return rows


def letter_rate(rows):
    """Letters per frame at each setting, over continuous runs."""
    print('=== letter rate by Message-speed byte (0x0200044C) ===')
    speeds = sorted({r['speed'] for r in rows})
    for speed in speeds:
        counts = collections.Counter(
            r['frame'] for r in rows
            if r['kind'] == 'glyph' and r['speed'] == speed)
        frames = sorted(counts)
        if not frames:
            print('  speed=%s: no letters drawn' % speed)
            continue

        runs, current = [], [frames[0]]
        for previous, nxt in zip(frames, frames[1:]):
            if nxt - previous <= RUN_GAP_FRAMES:
                current.append(nxt)
            else:
                runs.append(current)
                current = [nxt]
        runs.append(current)

        print('  speed=%s: %d letters over %d frames' %
              (speed, sum(counts.values()), len(frames)))
        for run in runs:
            letters = sum(counts[f] for f in run)
            if letters < MIN_RUN_LETTERS:
                continue
            span = run[-1] - run[0] + 1
            print('     frames %d..%d  %d letters in %d frames'
                  '  = %.2f letters/frame  (%s per frame)'
                  % (run[0], run[-1], letters, span,
                     letters / float(span),
                     ','.join(str(counts[f]) for f in run[:12])))
    print()


def context_offsets(rows):
    """Bytes of the text context that track the setting, and only it."""
    dumps = [r for r in rows if r['kind'] == 'proc']
    columns = [c for c in ('ctx0', 'ctx1', 'ctx2', 'ctx3') if dumps
               and c in dumps[0]]
    if not columns:
        return

    print('=== text-context bytes that follow the setting ===')
    found_any = False
    for column in columns:
        register = 'r' + column[-1]
        # Group by the pointer itself: two addresses are two different structs,
        # and comparing a byte across them means nothing.
        by_address = collections.defaultdict(list)
        for row in dumps:
            if row.get(column):
                by_address[row[register]].append(row)

        for address, group in sorted(by_address.items()):
            per_speed = collections.defaultdict(
                lambda: collections.defaultdict(set))
            for row in group:
                blob = row[column]
                for offset in range(len(blob) // 2):
                    per_speed[row['speed']][offset].add(
                        blob[offset * 2:offset * 2 + 2])

            speeds = sorted(per_speed)
            print('  %s = 0x%s  (%d samples, settings seen: %s)'
                  % (register, address, len(group), ','.join(speeds)))
            if len(speeds) < 2:
                print('     only one setting captured here -- needs a Normal '
                      'and a Fast dialogue through the SAME context')
                continue

            steady, noisy = [], []
            size = min(len(per_speed[s]) for s in speeds)
            for offset in range(size):
                values = [per_speed[s][offset] for s in speeds]
                if any(len(v) != 1 for v in values):
                    if len({frozenset(v) for v in values}) > 1:
                        noisy.append(offset)
                    continue
                singles = [next(iter(v)) for v in values]
                if len(set(singles)) > 1:
                    steady.append((offset, singles))

            for offset, singles in steady:
                found_any = True
                print('     +0x%02X  %s   <-- constant per setting, differs '
                      'between them'
                      % (offset, '  '.join('%s=%s' % (s, v)
                                           for s, v in zip(speeds, singles))))
            if not steady:
                print('     no byte is constant within each setting and '
                      'different between them')
            if noisy:
                print('     varying during play (not a plain setting copy): %s'
                      % ', '.join('+0x%02X' % o for o in noisy))

    if not found_any:
        print()
        print('  No candidate field. Either the budget is not in the first '
              '64 bytes of any argument pointer, or the capture lacks a '
              'Normal and a Fast dialogue through the same text context.')
    print()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('session_dir',
                        help='logs/trace_<stamp> directory to analyse')
    args = parser.parse_args()

    rows = load(args.session_dir)
    print('%d rows: %s\n'
          % (len(rows),
             ', '.join('%s=%d' % kv for kv in
                       sorted(collections.Counter(
                           r['kind'] for r in rows).items()))))
    letter_rate(rows)
    context_offsets(rows)


if __name__ == '__main__':
    main()
