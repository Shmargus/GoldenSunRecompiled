from __future__ import annotations

import random
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.explore_inputs import (
    KEY_A,
    KEY_B,
    KEY_START,
    NONE_KEYINPUT,
    Corpus,
    MacroOp,
    ReplayEvent,
    RunnerCrashed,
    RunnerConfig,
    SequenceRecord,
    attribute_sections,
    coverage_delta,
    decide_keep,
    generate_burst_ops,
    keyinput_with,
    macro_ops_total_frames,
    merge_coverage,
    mutate_ops,
    parse_coverage_text,
    parse_replay_file,
    replay_events_to_macro_ops,
    run_iteration,
)
from tools.measure_coverage import ROM_BASE, RomSection


class KeyinputTests(unittest.TestCase):
    def test_keyinput_with_clears_active_low_bits(self):
        self.assertEqual(keyinput_with(KEY_A), NONE_KEYINPUT & ~KEY_A)
        self.assertEqual(keyinput_with(KEY_A, KEY_B), NONE_KEYINPUT & ~KEY_A & ~KEY_B)

    def test_keyinput_with_no_bits_is_none(self):
        self.assertEqual(keyinput_with(), NONE_KEYINPUT)


class ReplayFileParsingTests(unittest.TestCase):
    def test_parses_frame_keyinput_rows_and_skips_comments(self):
        text = (
            "# gbarecomp-keyinput-v1\n"
            "# frame,keyinput_active_low\n"
            "0,0x03FF\n"
            "10,0x03DF\n"
            "40,0x03FF\n"
        )
        events = parse_replay_file(text)
        self.assertEqual(
            events,
            [
                ReplayEvent(0, 0x03FF),
                ReplayEvent(10, 0x03DF),
                ReplayEvent(40, 0x03FF),
            ],
        )

    def test_rejects_non_increasing_frame(self):
        text = "0,0x03FF\n5,0x03DF\n3,0x03FF\n"
        with self.assertRaises(ValueError):
            parse_replay_file(text)

    def test_rejects_out_of_range_keyinput(self):
        with self.assertRaises(ValueError):
            parse_replay_file("0,0xFFFF\n")

    def test_blank_lines_ignored(self):
        events = parse_replay_file("0,0x03FF\n\n10,0x03DF\n")
        self.assertEqual(len(events), 2)


class ReplayToMacroOpsTests(unittest.TestCase):
    def test_holds_value_until_next_event(self):
        events = [ReplayEvent(0, 0x03FF), ReplayEvent(10, 0x03DF), ReplayEvent(40, 0x03FF)]
        ops = replay_events_to_macro_ops(events, tail_frames=5)
        self.assertEqual(
            ops,
            [
                MacroOp(0x03FF, 10),
                MacroOp(0x03DF, 30),
                MacroOp(0x03FF, 5),
            ],
        )

    def test_zero_length_tail_dropped(self):
        events = [ReplayEvent(0, 0x03FF)]
        ops = replay_events_to_macro_ops(events, tail_frames=0)
        self.assertEqual(ops, [])

    def test_zero_length_middle_gap_dropped(self):
        # Two events on the same frame collapse to one hold, not a 0-frame op.
        events = [ReplayEvent(0, 0x03FF), ReplayEvent(0, 0x03DF), ReplayEvent(10, 0x03FF)]
        ops = replay_events_to_macro_ops(events, tail_frames=0)
        self.assertEqual(ops, [MacroOp(0x03DF, 10)])


class MacroOpsTotalFramesTests(unittest.TestCase):
    def test_sums_holds(self):
        ops = [MacroOp(1, 5), MacroOp(2, 7), MacroOp(3, 0)]
        self.assertEqual(macro_ops_total_frames(ops), 12)

    def test_empty(self):
        self.assertEqual(macro_ops_total_frames([]), 0)


class GenerateBurstOpsTests(unittest.TestCase):
    def test_deterministic_given_same_seed(self):
        a = generate_burst_ops(random.Random(42), target_frames=300)
        b = generate_burst_ops(random.Random(42), target_frames=300)
        self.assertEqual(a, b)

    def test_different_seeds_usually_differ(self):
        a = generate_burst_ops(random.Random(1), target_frames=300)
        b = generate_burst_ops(random.Random(2), target_frames=300)
        self.assertNotEqual(a, b)

    def test_reaches_at_least_target_frames(self):
        ops = generate_burst_ops(random.Random(7), target_frames=200)
        self.assertGreaterEqual(macro_ops_total_frames(ops), 200)

    def test_never_emits_zero_frame_holds(self):
        ops = generate_burst_ops(random.Random(3), target_frames=500)
        self.assertTrue(all(op.hold > 0 for op in ops))

    def test_only_uses_multi_frame_holds_never_single_frame_noise(self):
        ops = generate_burst_ops(random.Random(5), target_frames=500)
        # Every action unit in the generator holds for several frames, per
        # the "no 1-frame presses" mutation-strategy requirement.
        self.assertTrue(all(op.hold >= 6 for op in ops))

    def test_respects_max_ops_bound(self):
        # The loop checks the bound before adding one action unit (<= 2 ops),
        # so it can overshoot by at most one unit - never runs away unbounded.
        ops = generate_burst_ops(random.Random(9), target_frames=10 ** 9, max_ops=10)
        self.assertLessEqual(len(ops), 12)


class MutateOpsTests(unittest.TestCase):
    def test_deterministic_given_same_seed(self):
        base = generate_burst_ops(random.Random(1), target_frames=200)
        a = mutate_ops(random.Random(99), list(base))
        b = mutate_ops(random.Random(99), list(base))
        self.assertEqual(a, b)

    def test_full_rate_replaces_every_op(self):
        base = generate_burst_ops(random.Random(1), target_frames=200)
        mutated = mutate_ops(random.Random(123), list(base), rate=1.0)
        self.assertNotEqual(base, mutated)

    def test_zero_rate_can_still_append_or_shrink_but_preserves_untouched_ops(self):
        base = [MacroOp(NONE_KEYINPUT, 10), MacroOp(keyinput_with(KEY_A), 8)]
        mutated = mutate_ops(random.Random(0), list(base), rate=0.0)
        # rate=0 never replaces an existing op in place.
        self.assertEqual(mutated[: len(base)], base)


class CoverageSetArithmeticTests(unittest.TestCase):
    def test_parse_coverage_text(self):
        text = "0x08000100 thumb\n0x08000200 arm\n"
        parsed = parse_coverage_text(text)
        self.assertEqual(parsed, frozenset({(0x08000100, "thumb"), (0x08000200, "arm")}))

    def test_parse_coverage_text_ignores_blank_lines(self):
        parsed = parse_coverage_text("0x08000100 thumb\n\n")
        self.assertEqual(parsed, frozenset({(0x08000100, "thumb")}))

    def test_coverage_delta_is_new_minus_known(self):
        cumulative = frozenset({(0x1, "arm"), (0x2, "thumb")})
        observed = frozenset({(0x2, "thumb"), (0x3, "arm")})
        self.assertEqual(coverage_delta(cumulative, observed), frozenset({(0x3, "arm")}))

    def test_coverage_delta_empty_when_nothing_new(self):
        cumulative = frozenset({(0x1, "arm")})
        observed = frozenset({(0x1, "arm")})
        self.assertEqual(coverage_delta(cumulative, observed), frozenset())

    def test_merge_coverage_is_union(self):
        cumulative = frozenset({(0x1, "arm")})
        observed = frozenset({(0x2, "thumb")})
        self.assertEqual(merge_coverage(cumulative, observed), frozenset({(0x1, "arm"), (0x2, "thumb")}))

    def test_decide_keep_true_when_new_functions_present(self):
        self.assertTrue(decide_keep(frozenset({(0x1, "arm")})))

    def test_decide_keep_false_when_empty(self):
        self.assertFalse(decide_keep(frozenset()))


class SectionAttributionTests(unittest.TestCase):
    def test_buckets_addresses_by_containing_section(self):
        sections = [
            RomSection(index=1, name="rom_c0", address=ROM_BASE, size=0x1000, rom_offset=0),
            RomSection(index=2, name="rom_9000", address=ROM_BASE + 0x9000, size=0x1000, rom_offset=0x9000),
        ]
        addrs = [ROM_BASE + 0x10, ROM_BASE + 0x9010, ROM_BASE + 0x9020]
        counts = attribute_sections(addrs, sections, [])
        self.assertEqual(counts, {"rom_c0": 1, "rom_9000": 2})

    def test_unmapped_address_buckets_separately(self):
        counts = attribute_sections([0x02000000], [], [])
        self.assertEqual(counts, {"(unmapped/transient)": 1})


class RunIterationHangToleranceTests(unittest.TestCase):
    """A wedged candidate must be discarded, not crash the whole exploration run."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.corpus = Corpus(Path(self._tmp.name))
        root = SequenceRecord(
            id="0000_root", parent_id=None, ops=[MacroOp(NONE_KEYINPUT, 10)],
            checkpoint="checkpoints/root.state", new_function_count=5,
            total_functions_at_keep=5, frames=10,
        )
        self.corpus.add_sequence(root)
        self.corpus.cumulative = frozenset({(0x08000000, "arm")})
        self.corpus.save_cumulative()
        (self.corpus.checkpoints_dir / "root.state").write_bytes(b"synthetic-not-a-real-savestate")
        self.config = RunnerConfig(runner_exe=Path("runner.exe"), bios=Path("bios.bin"), rom=Path("rom.gba"))

    def test_runner_crashed_is_caught_and_recorded_as_discarded(self):
        with mock.patch(
            "tools.explore_inputs.run_macro_session",
            side_effect=RunnerCrashed("no response within 300.0s (runner likely wedged)"),
        ):
            entry = run_iteration(self.config, self.corpus, random.Random(1), 1, burst_frames=120)
        self.assertFalse(entry["kept"])
        self.assertEqual(entry["new_functions"], 0)
        self.assertIn("wedged", entry["error"])
        # The corpus's cumulative set must be untouched by a discarded attempt.
        self.assertEqual(self.corpus.cumulative, frozenset({(0x08000000, "arm")}))
        # No sequence record was added for the crashed attempt.
        self.assertNotIn("0001_extend", self.corpus.sequences)
        self.assertNotIn("0001_mutate", self.corpus.sequences)

    def test_root_is_never_mutated_only_extended(self):
        # Root's ops are the whole bootstrap replay; mutating them is slow
        # and hang-prone (see run_iteration's comment). Force the "mutate"
        # coin flip to always come up true and confirm root is still
        # extended, never mutated, regardless.
        with mock.patch("tools.explore_inputs.run_macro_session") as mocked:
            mocked.return_value = (frozenset({(0x1, "arm")}), "deadbeef", "")
            with mock.patch("random.Random.random", return_value=0.99):
                entry = run_iteration(self.config, self.corpus, random.Random(1), 1, burst_frames=120)
        self.assertEqual(entry["kind"], "extend")

    def test_no_leftover_checkpoint_or_coverage_file_after_a_crash(self):
        with mock.patch(
            "tools.explore_inputs.run_macro_session",
            side_effect=RunnerCrashed("wedged"),
        ):
            entry = run_iteration(self.config, self.corpus, random.Random(1), 1, burst_frames=120)
        candidate_checkpoint = self.corpus.checkpoints_dir / f"{entry['id']}.state"
        coverage_path = self.corpus.root / "attempts" / f"{entry['id']}.cov"
        self.assertFalse(candidate_checkpoint.exists())
        self.assertFalse(coverage_path.exists())


if __name__ == "__main__":
    unittest.main()
