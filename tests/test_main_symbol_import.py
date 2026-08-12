from __future__ import annotations

import struct
import unittest

from tools.import_main_symbols import (
    ElfFormatError,
    ElfImage,
    Symbol,
    import_symbols,
    parse_elf32_arm,
)


def _string_table(names: list[str]) -> tuple[bytes, dict[str, int]]:
    data = bytearray(b"\0")
    offsets: dict[str, int] = {"": 0}
    for name in names:
        offsets[name] = len(data)
        data.extend(name.encode("ascii") + b"\0")
    return bytes(data), offsets


def build_synthetic_elf(
    *,
    thumb_state_bit: bool = True,
    thumb_size: int = 4,
    physical_address: int = 0x08000100,
) -> bytes:
    symbol_names = ["$a", "$t", "$d", "synthetic_arm", "synthetic_thumb"]
    strings, string_offsets = _string_table(symbol_names)
    section_names, section_offsets = _string_table(
        [".text", ".symtab", ".strtab", ".shstrtab"]
    )

    symbols = [struct.pack("<IIIBBH", 0, 0, 0, 0, 0, 0)]
    for name, address in (("$a", 0x03000000), ("$t", 0x03000004), ("$d", 0x03000008)):
        symbols.append(
            struct.pack("<IIIBBH", string_offsets[name], address, 0, 0, 0, 1)
        )
    symbols.append(
        struct.pack(
            "<IIIBBH", string_offsets["synthetic_arm"], 0x03000000, 4, 0x12, 0, 1
        )
    )
    thumb_value = 0x03000005 if thumb_state_bit else 0x03000004
    symbols.append(
        struct.pack(
            "<IIIBBH",
            string_offsets["synthetic_thumb"],
            thumb_value,
            thumb_size,
            0x12,
            0,
            1,
        )
    )
    symtab = b"".join(symbols)

    text_offset = 0x100
    strings_offset = 0x120
    symtab_offset = 0x180
    section_names_offset = 0x200
    section_headers_offset = 0x240
    section_count = 5
    total_size = section_headers_offset + section_count * 40
    data = bytearray(total_size)

    ident = ELF_MAGIC = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        ident,
        2,
        40,
        1,
        0,
        52,
        section_headers_offset,
        0x05000200,
        52,
        32,
        1,
        40,
        section_count,
        4,
    )
    data[: len(header)] = header
    data[52:84] = struct.pack(
        "<IIIIIIII",
        1,
        text_offset,
        0x03000000,
        physical_address,
        12,
        12,
        5,
        4,
    )
    data[text_offset : text_offset + 12] = bytes(12)
    data[strings_offset : strings_offset + len(strings)] = strings
    data[symtab_offset : symtab_offset + len(symtab)] = symtab
    data[section_names_offset : section_names_offset + len(section_names)] = section_names

    sections = [bytes(40)]
    sections.append(
        struct.pack(
            "<IIIIIIIIII",
            section_offsets[".text"],
            1,
            0x6,
            0x03000000,
            text_offset,
            12,
            0,
            0,
            4,
            0,
        )
    )
    sections.append(
        struct.pack(
            "<IIIIIIIIII",
            section_offsets[".symtab"],
            2,
            0,
            0,
            symtab_offset,
            len(symtab),
            3,
            4,
            4,
            16,
        )
    )
    sections.append(
        struct.pack(
            "<IIIIIIIIII",
            section_offsets[".strtab"],
            3,
            0,
            0,
            strings_offset,
            len(strings),
            0,
            0,
            1,
            0,
        )
    )
    sections.append(
        struct.pack(
            "<IIIIIIIIII",
            section_offsets[".shstrtab"],
            3,
            0,
            0,
            section_names_offset,
            len(section_names),
            0,
            0,
            1,
            0,
        )
    )
    data[section_headers_offset:] = b"".join(sections)
    return bytes(data)


def import_fixture(data: bytes):
    return import_symbols(
        parse_elf32_arm(data),
        binary_id="synthetic_main",
        evidence_source="synthetic.elf:.symtab+mapping-symbols+PT_LOAD",
        evidence_revision="0" * 40,
    )


class MainSymbolImportTests(unittest.TestCase):
    def test_imports_mapping_proven_modes_and_pt_load_sources(self) -> None:
        corpus, report = import_fixture(build_synthetic_elf())
        self.assertEqual(report["unresolved"], [])
        self.assertEqual(len(corpus["symbols"]), 2)
        arm, thumb = corpus["symbols"]
        self.assertEqual(arm["mode"], "arm")
        self.assertEqual(arm["runtime_address"], "0x03000000")
        self.assertEqual(arm["source_address"], "0x08000100")
        self.assertEqual(thumb["mode"], "thumb")
        self.assertEqual(thumb["runtime_address"], "0x03000004")
        self.assertEqual(thumb["source_address"], "0x08000104")

    def test_mapping_and_thumb_state_bit_must_agree(self) -> None:
        corpus, report = import_fixture(build_synthetic_elf(thumb_state_bit=False))
        self.assertEqual(len(corpus["symbols"]), 1)
        reasons = report["unresolved"][0]["reasons"]
        self.assertIn(
            "function symbol state bit disagrees with mapping-symbol mode", reasons
        )

    def test_function_crossing_data_mapping_is_retained_but_not_proven(self) -> None:
        corpus, report = import_fixture(build_synthetic_elf(thumb_size=8))
        self.assertEqual(len(corpus["symbols"]), 2)
        self.assertEqual(corpus["symbols"][1]["confidence"], "unresolved")
        self.assertEqual(report["unresolved"], [])
        observation = report["internal_mapping_transitions"][0]
        self.assertEqual(observation["name"], "synthetic_thumb")
        self.assertEqual(
            observation["transitions"],
            [{"runtime_address": "0x03000008", "mapping_kind": "data"}],
        )

    def test_overlapping_stt_func_ranges_are_all_excluded(self) -> None:
        image = parse_elf32_arm(build_synthetic_elf())
        overlapping = Symbol(
            index=len(image.symbols),
            name="synthetic_thumb_tail_entry",
            value=0x03000007,
            size=2,
            symbol_type=2,
            binding=1,
            section_index=1,
        )
        image = ElfImage(
            sections=image.sections,
            program_headers=image.program_headers,
            symbols=image.symbols + (overlapping,),
        )
        corpus, report = import_symbols(
            image,
            binary_id="synthetic_main",
            evidence_source="synthetic.elf:.symtab+mapping-symbols+PT_LOAD",
            evidence_revision="0" * 40,
        )
        self.assertEqual([entry["name"] for entry in corpus["symbols"]], ["synthetic_arm"])
        unresolved_names = {entry["name"] for entry in report["unresolved"]}
        self.assertEqual(
            unresolved_names,
            {"synthetic_thumb", "synthetic_thumb_tail_entry"},
        )
        for entry in report["unresolved"]:
            self.assertTrue(any("range overlaps STT_FUNC" in reason for reason in entry["reasons"]))

    def test_absolute_function_import_is_not_owned_code(self) -> None:
        image = parse_elf32_arm(build_synthetic_elf())
        external = Symbol(
            index=len(image.symbols),
            name="synthetic_external",
            value=0x08001001,
            size=8,
            symbol_type=2,
            binding=1,
            section_index=0xFFF1,
        )
        image = ElfImage(
            sections=image.sections,
            program_headers=image.program_headers,
            symbols=image.symbols + (external,),
        )
        corpus, report = import_symbols(
            image,
            binary_id="synthetic_main",
            evidence_source="synthetic.elf:.symtab+mapping-symbols+PT_LOAD",
            evidence_revision="0" * 40,
        )
        self.assertEqual(len(corpus["symbols"]), 2)
        self.assertEqual(report["unresolved"], [])
        self.assertEqual(
            report["external_function_symbols"],
            {"undefined": 0, "absolute": 1},
        )

    def test_rejects_non_elf_input(self) -> None:
        with self.assertRaisesRegex(ElfFormatError, "not an ELF"):
            parse_elf32_arm(b"synthetic text")

if __name__ == "__main__":
    unittest.main()
