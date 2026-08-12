from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.import_main_symbols import ElfImage, ProgramHeader, Section, Symbol
from tools.measure_coverage import (
    ROM_BASE,
    ROM_SIZE,
    STATE_CODE,
    STATE_DATA,
    STATE_UNKNOWN,
    ShadowWindow,
    build_rom_sections,
    classify_code_data,
    containing_symbol,
    build_symbol_index,
    find_runs,
    map_translated_intervals,
    read_generated_intervals,
)

STT_NOTYPE = 0
STT_FUNC = 2


def _mapping_symbol(index: int, name: str, address: int, section_index: int) -> Symbol:
    return Symbol(
        index=index,
        name=name,
        value=address,
        size=0,
        symbol_type=STT_NOTYPE,
        binding=0,
        section_index=section_index,
    )


def build_synthetic_image(
    *,
    extra_symbols: tuple[Symbol, ...] = (),
    shadow_segment: bool = False,
) -> ElfImage:
    """A tiny synthetic ELF image: two ROM-resident sections with mapping symbols.

    section "text_a": runtime 0x08000000..0x08000100 (256 bytes)
        $t 0x08000000 (code), $d 0x08000040 (data) -> code=[0,0x40) data=[0x40,0x100)
    section "text_b": runtime 0x08001000..0x08001040 (64 bytes), no mapping symbols
        -> entirely "unknown" (no preceding mapping symbol at all)
    """

    sections = [
        Section(
            index=0, name="", section_type=0, flags=0, address=0, offset=0, size=0,
            link=0, entry_size=0,
        ),
        Section(
            index=1,
            name="text_a",
            section_type=1,
            flags=0x6,  # ALLOC | EXECINSTR
            address=0x08000000,
            offset=0x1000,
            size=0x100,
            link=0,
            entry_size=0,
        ),
        Section(
            index=2,
            name="text_b",
            section_type=1,
            flags=0x6,
            address=0x08001000,
            offset=0x2000,  # segment.offset (0x1000) + (address - segment.vaddr) (0x1000)
            size=0x40,
            link=0,
            entry_size=0,
        ),
    ]
    program_headers = [
        ProgramHeader(
            header_type=1,
            offset=0x1000,
            virtual_address=0x08000000,
            physical_address=0x08000000,
            file_size=0x1040,
            memory_size=0x1040,
            flags=0x5,
        ),
    ]
    if shadow_segment:
        # An IWRAM shadow window: runtime 0x03000000, physically resident at
        # ROM offset (0x08000200 - 0x08000000) = 0x200, 0x20 bytes.
        sections.append(
            Section(
                index=3,
                name="text_iwram",
                section_type=1,
                flags=0x6,
                address=0x03000000,
                offset=0x3000,
                size=0x20,
                link=0,
                entry_size=0,
            )
        )
        program_headers.append(
            ProgramHeader(
                header_type=1,
                offset=0x3000,
                virtual_address=0x03000000,
                physical_address=0x08000200,
                file_size=0x20,
                memory_size=0x20,
                flags=0x5,
            )
        )

    symbols: list[Symbol] = [
        Symbol(index=0, name="", value=0, size=0, symbol_type=0, binding=0, section_index=0),
        _mapping_symbol(1, "$t", 0x08000000, 1),
        _mapping_symbol(2, "$d", 0x08000040, 1),
    ]
    symbols.extend(extra_symbols)

    return ElfImage(
        sections=tuple(sections),
        program_headers=tuple(program_headers),
        symbols=tuple(symbols),
    )


class RomSectionMappingTests(unittest.TestCase):
    def test_sections_map_to_rom_offsets_via_paddr(self) -> None:
        image = build_synthetic_image()
        sections, shadow_windows = build_rom_sections(image)
        by_name = {s.name: s for s in sections}
        self.assertEqual(by_name["text_a"].rom_offset, 0x0)
        self.assertEqual(by_name["text_b"].rom_offset, 0x1000)
        self.assertEqual(shadow_windows, [])

    def test_shadow_segment_produces_a_shadow_window(self) -> None:
        image = build_synthetic_image(shadow_segment=True)
        sections, shadow_windows = build_rom_sections(image)
        self.assertEqual(len(shadow_windows), 1)
        window = shadow_windows[0]
        self.assertEqual(window.vaddr_start, 0x03000000)
        self.assertEqual(window.length, 0x20)
        self.assertEqual(window.rom_offset, 0x200)


class ClassifyCodeDataTests(unittest.TestCase):
    def test_mapping_symbols_classify_code_and_data(self) -> None:
        image = build_synthetic_image()
        sections, _ = build_rom_sections(image)
        states, out_of_range = classify_code_data(image, sections)
        self.assertEqual(len(states), ROM_SIZE)
        self.assertEqual(out_of_range, [])
        # [0, 0x40) is code (from $t at 0x08000000)
        self.assertTrue(all(b == STATE_CODE for b in states[0:0x40]))
        # [0x40, 0x100) is data (from $d at 0x08000040)
        self.assertTrue(all(b == STATE_DATA for b in states[0x40:0x100]))

    def test_section_without_mapping_symbols_is_entirely_unknown(self) -> None:
        image = build_synthetic_image()
        sections, _ = build_rom_sections(image)
        states, _ = classify_code_data(image, sections)
        by_name = {s.name: s for s in sections}
        start = by_name["text_b"].rom_offset
        self.assertTrue(all(b == STATE_UNKNOWN for b in states[start : start + 0x40]))

    def test_gap_between_sections_is_unknown_not_folded_into_code_or_data(self) -> None:
        image = build_synthetic_image()
        sections, _ = build_rom_sections(image)
        states, _ = classify_code_data(image, sections)
        # ROM offset 0x100..0x1000 is not covered by any section at all.
        self.assertTrue(all(b == STATE_UNKNOWN for b in states[0x100:0x1000]))
        code_total = states.count(STATE_CODE)
        data_total = states.count(STATE_DATA)
        unknown_total = ROM_SIZE - code_total - data_total
        self.assertEqual(code_total, 0x40)
        self.assertEqual(data_total, 0xC0)
        self.assertGreater(unknown_total, 0)

    def test_ambiguous_mapping_symbol_kinds_at_same_address_are_unknown(self) -> None:
        extra = (
            _mapping_symbol(3, "$a", 0x08000080, 1),  # collides with existing $d run start? no
            _mapping_symbol(4, "$d", 0x08000080, 1),  # same address, different kind -> ambiguous
        )
        image = build_synthetic_image(extra_symbols=extra)
        sections, _ = build_rom_sections(image)
        states, _ = classify_code_data(image, sections)
        # 0x80 is ambiguous ($a and $d collide) -> unknown, not guessed either way.
        self.assertEqual(states[0x80], STATE_UNKNOWN)

    def test_mapping_symbol_outside_its_own_section_range_is_excluded_not_guessed(self) -> None:
        # A $d symbol indexed under section 1 (text_a, [0x08000000,0x08000100))
        # but whose value falls outside that range -- observed in the real ELF.
        extra = (_mapping_symbol(3, "$d", 0x10000000, 1),)
        image = build_synthetic_image(extra_symbols=extra)
        sections, _ = build_rom_sections(image)
        states, out_of_range = classify_code_data(image, sections)
        self.assertEqual(len(states), ROM_SIZE)  # no corruption / no bitmap growth
        self.assertEqual(out_of_range, [("text_a", 0x10000000)])
        # Classification of text_a is unaffected by the bogus symbol.
        self.assertTrue(all(b == STATE_CODE for b in states[0:0x40]))
        self.assertTrue(all(b == STATE_DATA for b in states[0x40:0x100]))


class TranslatedIntervalDedupTests(unittest.TestCase):
    def test_overlapping_entry_points_count_each_byte_once(self) -> None:
        # Two overlapping guest ranges (as if a resume alias / re-emitted
        # function pointed into the same bytes as another entry).
        intervals = [
            (0x08000000, 0x08000010, "arm"),
            (0x08000008, 0x08000020, "arm"),  # overlaps [0x08000008, 0x08000010)
        ]
        translated, transient, anomalies, count = map_translated_intervals(intervals, [])
        self.assertEqual(anomalies, [])
        self.assertEqual(transient, [])
        self.assertEqual(count, 2)
        union_length = 0x20 - 0x0  # [0,0x10) union [0x8,0x20) = [0,0x20)
        self.assertEqual(translated.count(1), union_length)

    def test_non_rom_resident_addresses_are_reported_as_transient(self) -> None:
        intervals = [(0x03003478, 0x030034a0, "thumb")]  # not in ROM space, no shadow window
        translated, transient, anomalies, count = map_translated_intervals(intervals, [])
        self.assertEqual(count, 0)
        self.assertEqual(translated.count(1), 0)
        self.assertEqual(transient, [(0x03003478, 0x030034a0, "thumb")])
        self.assertEqual(anomalies, [])

    def test_shadow_window_addresses_map_back_to_rom_bytes(self) -> None:
        window = ShadowWindow(vaddr_start=0x03000000, length=0x20, rom_offset=0x200)
        intervals = [(0x03000004, 0x03000008, "arm")]
        translated, transient, anomalies, count = map_translated_intervals(intervals, [window])
        self.assertEqual(transient, [])
        self.assertEqual(anomalies, [])
        self.assertEqual(count, 1)
        self.assertEqual(translated[0x204:0x208], bytes([1, 1, 1, 1]))
        self.assertEqual(translated.count(1), 4)


class GeneratedIntervalParsingTests(unittest.TestCase):
    def test_parses_header_comment_and_ignores_alias_lines(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "recompiled_000.cpp").write_text(
                "\n".join(
                    [
                        "/* 0x080000C0  mode=thumb  end=0x080000C4  branches=1  indirect */",
                        "void gf__Func_30f8(void) { }",
                        "/* alias 0x080000C2 -> gf__Func_30f8 */",
                        "void gf__Func_30f8__alias_080000C2(void) { }",
                        "/* 0x08000100  mode=arm  end=0x08000110  branches=0 */",
                        "void gf__Func_100(void) { }",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            result = read_generated_intervals(directory)
            self.assertEqual(result.duplicate_keys, 0)
            self.assertEqual(
                sorted(result.intervals),
                [
                    (0x080000C0, 0x080000C4, "thumb"),
                    (0x08000100, 0x08000110, "arm"),
                ],
            )

    def test_duplicate_entry_point_key_is_counted_and_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "recompiled_000.cpp").write_text(
                "\n".join(
                    [
                        "/* 0x08000000  mode=arm  end=0x08000010  branches=0 */",
                        "/* 0x08000000  mode=arm  end=0x08000020  branches=0 */",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            result = read_generated_intervals(directory)
            self.assertEqual(result.duplicate_keys, 1)
            self.assertEqual(result.intervals, [(0x08000000, 0x08000010, "arm")])


class RunFindingAndSymbolLookupTests(unittest.TestCase):
    def test_find_runs_locates_contiguous_regions(self) -> None:
        flags = bytes([0, 1, 1, 1, 0, 0, 1, 0])
        self.assertEqual(find_runs(flags, 1), [(1, 4), (6, 7)])

    def test_containing_symbol_lookup(self) -> None:
        image = ElfImage(
            sections=(),
            program_headers=(),
            symbols=(
                Symbol(index=0, name="f_a", value=0x08000000, size=0x10, symbol_type=STT_FUNC, binding=1, section_index=1),
                Symbol(index=1, name="f_b", value=0x08000020, size=0x8, symbol_type=STT_FUNC, binding=1, section_index=1),
            ),
        )
        starts, rest = build_symbol_index(image)
        self.assertEqual(containing_symbol(0x08000004, starts, rest), "f_a")
        self.assertEqual(containing_symbol(0x08000022, starts, rest), "f_b")
        self.assertIsNone(containing_symbol(0x08000018, starts, rest))  # gap between funcs


if __name__ == "__main__":
    unittest.main()
