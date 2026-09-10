#!/usr/bin/env python3
"""Offline analyser for GSR_OBJ_RECORD sprite-placement sessions.

Analysis tool, not part of the product; reads only host-written diagnostic
files under logs/objrec_<session>/ -- never touches ROM/BIOS bytes. Python
stdlib only.

The question it answers
-----------------------
NPCs and their shadows jump between the top and bottom of the expanded view.
The cause is not a bug in the placement arithmetic: OAM stores a sprite's row
in eight bits, and a 360x240 view spans 304 rows, so a row above the view and
a row in the bottom margin share a byte. Measured: slot 23 sits at logical
y=-101 and writes byte 155, and byte 155 also means row 155.

The only escape is to stop reading that byte and use the guest's own
pre-truncation coordinate, which the runner already captures. Before rewiring
the renderer to depend on that capture, this measures how much of it is
actually there -- per sprite, per actor record, and per emitting call site.

Modes
-----
  decode_obj.py <session_dir>
      Summarise one session: overall coverage, body vs shadow, exposure to
      the ambiguous byte band, and the call sites that emit sprites with no
      recoverable position (ranked by how many they lose).

  decode_obj.py <session_dir> --sites
      Full per-call-site table rather than the worst few.

  decode_obj.py <session_dir> --records
      Per-actor-record table. The runner admits any record-shaped pointer in
      the 0x03002000 array (stride 0x38) and lets the provenance gates
      authenticate it. If a town commits sprites this table
      cannot account for, an object buffer has to be keyed on something wider,
      and that shows up here as records seen versus sprites unaccounted for in
      the summary.

  decode_obj.py <session_dir> --frames
      Per-frame coverage, so a scene transition or a specific moment of
      jumping can be located in time.

  decode_obj.py <a> <b> [...] --compare
      One coverage row per session, for comparing runs (different towns,
      before/after a change).

See src/obj_recorder.cpp for how each CSV is produced and
src/widescreen_policy.h (GoldenSunObjPlacementOutcome) for what each outcome
means.
"""

import argparse
import csv
import os
import sys
from collections import defaultdict

# Must match golden_sun_obj_placement_outcome_name() in widescreen_policy.h.
# gsr_widescreen_policy_test pins those strings precisely so a rename cannot
# silently invalidate an already-recorded session.
EXACT_OUTCOMES = ("exact-placement", "paired-body-placement")
OUTCOMES = EXACT_OUTCOMES + (
    "source-unavailable",
    "placement-unavailable",
    "affine-or-double-size",
    "invalid-shape-size",
)


def read_csv(session, name):
    path = os.path.join(session, name)
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def as_int(row, key, default=0):
    value = row.get(key)
    if value is None or value == "":
        return default
    try:
        return int(value, 0)
    except ValueError:
        return default


def pct(n, d):
    return 0.0 if not d else 100.0 * n / d


def bar(fraction, width=28):
    filled = int(round(fraction * width))
    return "#" * filled + "." * (width - filled)


def load_totals(session):
    """Per-outcome totals, summed from coverage.csv.

    Deliberately recomputed from the per-frame rows rather than parsed out of
    summary.txt: summary.txt is prose for reading, coverage.csv is the
    measurement, and a disagreement between them means the session was
    truncated -- worth seeing, not worth hiding.
    """
    rows = read_csv(session, "coverage.csv")
    totals = defaultdict(int)
    committed = 0
    for row in rows:
        committed += as_int(row, "committed")
        for outcome in OUTCOMES:
            totals[outcome] += as_int(row, outcome)
    return committed, totals, len(rows)


def print_summary(session):
    committed, totals, frames = load_totals(session)
    if not committed:
        print(f"{session}: no committed sprites recorded.")
        print("  The recorder only runs in the expanded field view -- check "
              "that widescreen was active and the run reached a town.")
        return
    exact = sum(totals[o] for o in EXACT_OUTCOMES)

    print(f"session : {session}")
    print(f"frames  : {frames}")
    print(f"sprites : {committed}")
    print()
    print(f"  full-precision  {exact:>9}  {pct(exact, committed):6.2f}%  "
          f"{bar(exact / committed)}")
    print("  -- these are immune to the 8-bit vertical wrap at any view size")
    print()
    for outcome in OUTCOMES:
        count = totals[outcome]
        if not count:
            continue
        mark = " " if outcome in EXACT_OUTCOMES else "!"
        print(f"  {mark} {outcome:<22} {count:>9}  "
              f"{pct(count, committed):6.2f}%")

    # The summary the runner wrote carries the body/shadow split and the
    # wrap-band exposure, which coverage.csv does not break out per frame.
    summary_path = os.path.join(session, "summary.txt")
    if os.path.exists(summary_path):
        with open(summary_path, encoding="utf-8") as f:
            text = f.read()
        for heading in ("body vs shadow", "affine (rotation/scaling)",
                        "sprite tables observed", "paired shadow recovery",
                        "vertical-wrap exposure"):
            start = text.find(heading)
            if start < 0:
                continue
            block = text[start:].split("\n\n")[0]
            print()
            print("  " + block.replace("\n", "\n  "))

    print_worst_sites(session, limit=8)


def site_rows(session):
    rows = read_csv(session, "sites.csv")
    for row in rows:
        row["_total"] = as_int(row, "total")
        row["_exact"] = sum(as_int(row, o) for o in EXACT_OUTCOMES)
        row["_lost"] = row["_total"] - row["_exact"]
    return rows


def print_worst_sites(session, limit=None):
    rows = [r for r in site_rows(session) if r["_lost"]]
    if not rows:
        print()
        print("  Every emitting call site produced a full-precision position.")
        return
    rows.sort(key=lambda r: r["_lost"], reverse=True)
    shown = rows if limit is None else rows[:limit]
    print()
    print("  call sites losing position "
          f"({len(rows)} of {len(site_rows(session))} total)")
    print(f"  {'return_pc':<12}{'writer_pc':<12}{'depth':>6}"
          f"{'lost':>10}{'of':>10}  worst outcome")
    for row in shown:
        worst = max(
            ((o, as_int(row, o)) for o in OUTCOMES if o not in EXACT_OUTCOMES),
            key=lambda kv: kv[1],
        )
        print(f"  {row['return_pc']:<12}{row['writer_pc']:<12}"
              f"{row['depth']:>6}{row['_lost']:>10}{row['_total']:>10}  "
              f"{worst[0]}")
    if limit is not None and len(rows) > limit:
        print(f"  ... {len(rows) - limit} more (--sites for the full table)")


def print_sites(session):
    rows = site_rows(session)
    if not rows:
        print(f"{session}: no sites.csv")
        return
    rows.sort(key=lambda r: r["_total"], reverse=True)
    header = ["return_pc", "writer_pc", "depth", "total", "exact%"]
    header += [o for o in OUTCOMES]
    print("\t".join(header))
    for row in rows:
        cells = [row["return_pc"], row["writer_pc"], row["depth"],
                 str(row["_total"]),
                 f"{pct(row['_exact'], row['_total']):.1f}"]
        cells += [row.get(o, "0") for o in OUTCOMES]
        print("\t".join(str(c) for c in cells))


def print_records(session):
    rows = read_csv(session, "records.csv")
    if not rows:
        print(f"{session}: no records.csv -- no sprite was traced to an "
              "actor record at all.")
        return
    print(f"{'record':<12}{'body':>8}{'body ok':>9}{'shadow':>9}"
          f"{'shad ok':>9}")
    body_total = body_ok = shadow_total = shadow_ok = 0
    for row in sorted(rows, key=lambda r: as_int(r, "record_base")):
        b, bo = as_int(row, "body_committed"), as_int(row, "body_exact")
        s, so = as_int(row, "shadow_committed"), as_int(row, "shadow_exact")
        body_total += b
        body_ok += bo
        shadow_total += s
        shadow_ok += so
        print(f"{row['record_base']:<12}{b:>8}{bo:>9}{s:>9}{so:>9}")
    print(f"{'total':<12}{body_total:>8}{body_ok:>9}{shadow_total:>9}"
          f"{shadow_ok:>9}")
    print()
    print(f"{len(rows)} distinct actor records seen. The runner recognises "
          "13 (0x03002000, stride 0x38).")

    # A sprite that never reached a record is invisible in this table, so
    # name the gap explicitly rather than leaving the totals to imply it.
    committed, totals, _ = load_totals(session)
    accounted = body_total + shadow_total
    if committed > accounted:
        print(f"{committed - accounted} of {committed} committed sprites were "
              "never traced to any record --")
        print("that population is what an object buffer would have to grow to "
              "cover.")


def print_frames(session):
    rows = read_csv(session, "coverage.csv")
    if not rows:
        print(f"{session}: no coverage.csv")
        return
    print("\t".join(["frame", "committed", "exact%"] + list(OUTCOMES)))
    for row in rows:
        committed = as_int(row, "committed")
        exact = sum(as_int(row, o) for o in EXACT_OUTCOMES)
        cells = [row["frame"], str(committed),
                 f"{pct(exact, committed):.1f}"]
        cells += [row.get(o, "0") for o in OUTCOMES]
        print("\t".join(cells))


def print_compare(sessions):
    print(f"{'session':<34}{'sprites':>10}{'exact':>10}{'exact%':>9}"
          f"{'unsourced':>11}")
    for session in sessions:
        committed, totals, _ = load_totals(session)
        exact = sum(totals[o] for o in EXACT_OUTCOMES)
        name = os.path.basename(os.path.normpath(session))
        print(f"{name:<34}{committed:>10}{exact:>10}"
              f"{pct(exact, committed):>8.2f}%"
              f"{totals['source-unavailable']:>11}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sessions", nargs="+", metavar="SESSION_DIR",
                    help="logs/objrec_<stamp> directory")
    ap.add_argument("--sites", action="store_true",
                    help="full per-call-site table")
    ap.add_argument("--records", action="store_true",
                    help="per-actor-record table")
    ap.add_argument("--frames", action="store_true",
                    help="per-frame coverage")
    ap.add_argument("--compare", action="store_true",
                    help="one coverage row per session")
    args = ap.parse_args()

    for session in args.sessions:
        if not os.path.isdir(session):
            print(f"not a directory: {session}", file=sys.stderr)
            return 2

    if args.compare:
        print_compare(args.sessions)
        return 0
    for i, session in enumerate(args.sessions):
        if i:
            print()
        if args.sites:
            print_sites(session)
        elif args.records:
            print_records(session)
        elif args.frames:
            print_frames(session)
        else:
            print_summary(session)
    return 0


if __name__ == "__main__":
    sys.exit(main())
