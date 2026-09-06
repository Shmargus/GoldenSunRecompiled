#!/usr/bin/env python3
"""Offline decoder for GSRSNAP1 scene snapshots (src/map_recorder.cpp).

Analysis tool, not part of the product; reads only host-written diagnostic
files under logs/maprec_<session>/ -- never touches ROM/BIOS bytes. Python
stdlib only: PIL is not installed on the target machine, so PNGs are written
with the same hand-rolled zlib-based writer tools/render_maprec.py used.

Three modes:

  decode_snap.py <snap.bin> [--out DIR]
      Decode one snapshot: print every IO register the recorder logs (by
      name), render each enabled BG layer (regular or affine) to PNG, render
      the Mode 0 128x128 metatile grid, and hexdump the EWRAM region holding
      the (currently unidentified) room-bounds struct.

  decode_snap.py --compare SESSION_DIR
      Read every snap_*.bin in SESSION_DIR plus its frames.csv, and print one
      row per snapshot: label, BG mode, DISPCNT, BG0CNT-BG3CNT (decoded),
      and the set of memory pages that changed while that scene was active.
      Rows are grouped by identical (mode, DISPCNT, BG0-3 CNT) signature, so
      whether towns/dungeons/indoor rooms (or any other pair of manually-
      labelled scenes) share one mechanism or fork into different ones is a
      property of the table, not something eyeballed from raw dumps.

  decode_snap.py --roomload-intersect PATH [PATH ...]
      Read one or more roomload_*.bin room-load EWRAM write logs (each PATH
      may be a file, a glob, or a directory to search for roomload_*.bin in),
      OR each file's per-frame line bitmaps into "lines touched by that
      load", then intersect across files. What survives in every observed
      load is where the room-bounds struct must live -- see
      map_recorder_on_ewram_write in src/map_recorder.cpp for how the log is
      produced and why a single snapshot diff cannot answer this (thousands
      of EWRAM addresses vary per room; only "written during THIS burst" is
      a stable discriminator).

See src/map_recorder.cpp for the exact on-disk format and the IO register
offsets (standard GBA memory map, cross-checked against
gbarecomp/src/gba/gba_ppu.cpp -- see that file's own comments for citations).
"""
import argparse
import glob
import os
import re
import struct
import sys
import zlib

# ---- GSRSNAP1 header (see map_recorder.cpp's write_snapshot_bin) ---------
MAGIC = b"GSRSNAP1"
TAG_BYTES = 64
SECTION_NAME_BYTES = 8


def parse_snapshot(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[0:8] != MAGIC:
        raise ValueError("%s: bad magic %r" % (path, data[0:8]))
    version, section_count = struct.unpack_from("<II", data, 8)
    (frame,) = struct.unpack_from("<Q", data, 16)
    tag_raw = data[24:24 + TAG_BYTES]
    tag = tag_raw.split(b"\x00", 1)[0].decode("ascii", "replace")
    off = 24 + TAG_BYTES
    sections = {}
    for _ in range(section_count):
        name_raw = data[off:off + SECTION_NAME_BYTES]
        name = name_raw.split(b"\x00", 1)[0].decode("ascii", "replace")
        s_off, s_len = struct.unpack_from("<QQ", data, off + SECTION_NAME_BYTES)
        sections[name] = (s_off, s_len)
        off += SECTION_NAME_BYTES + 16
    return {
        "path": path,
        "version": version,
        "frame": frame,
        "tag": tag,
        "sections": {
            name: data[o:o + n] for name, (o, n) in sections.items()
        },
    }


# ---- IO register decode (offsets match map_recorder.cpp's append_io_fields,
# which cites its GBA hardware / gba_ppu.cpp evidence) ---------------------
def u16(io, off):
    return io[off] | (io[off + 1] << 8)


def s16(io, off):
    v = u16(io, off)
    return v - 0x10000 if v & 0x8000 else v


def s28(io, off):
    v = io[off] | (io[off + 1] << 8) | (io[off + 2] << 16) | (io[off + 3] << 24)
    v &= 0x0FFFFFFF
    if v & 0x08000000:
        v -= 0x10000000
    return v


IO_REGISTERS = [
    ("DISPCNT", 0x00, "u16"),
    ("BG0CNT", 0x08, "u16"),
    ("BG1CNT", 0x0A, "u16"),
    ("BG2CNT", 0x0C, "u16"),
    ("BG3CNT", 0x0E, "u16"),
    ("BG0HOFS", 0x10, "u16"),
    ("BG0VOFS", 0x12, "u16"),
    ("BG1HOFS", 0x14, "u16"),
    ("BG1VOFS", 0x16, "u16"),
    ("BG2HOFS", 0x18, "u16"),
    ("BG2VOFS", 0x1A, "u16"),
    ("BG3HOFS", 0x1C, "u16"),
    ("BG3VOFS", 0x1E, "u16"),
    ("BG2PA", 0x20, "s16"),
    ("BG2PB", 0x22, "s16"),
    ("BG2PC", 0x24, "s16"),
    ("BG2PD", 0x26, "s16"),
    ("BG2X", 0x28, "s28"),
    ("BG2Y", 0x2C, "s28"),
    ("BG3PA", 0x30, "s16"),
    ("BG3PB", 0x32, "s16"),
    ("BG3PC", 0x34, "s16"),
    ("BG3PD", 0x36, "s16"),
    ("BG3X", 0x38, "s28"),
    ("BG3Y", 0x3C, "s28"),
    ("WIN0H", 0x40, "u16"),
    ("WIN1H", 0x42, "u16"),
    ("WIN0V", 0x44, "u16"),
    ("WIN1V", 0x46, "u16"),
    ("WININ", 0x48, "u16"),
    ("WINOUT", 0x4A, "u16"),
    ("MOSAIC", 0x4C, "u16"),
    ("BLDCNT", 0x50, "u16"),
    ("BLDALPHA", 0x52, "u16"),
    ("BLDY", 0x54, "u16"),
]


def decode_io_registers(io):
    out = {}
    for name, off, kind in IO_REGISTERS:
        if kind == "u16":
            out[name] = u16(io, off)
        elif kind == "s16":
            out[name] = s16(io, off)
        else:
            out[name] = s28(io, off)
    return out


def print_io_registers(regs):
    print("IO registers:")
    for name, _off, _kind in IO_REGISTERS:
        v = regs[name]
        if v < 0:
            print("  %-8s = %d (0x%08X)" % (name, v, v & 0xFFFFFFFF))
        else:
            print("  %-8s = %d (0x%04X)" % (name, v, v))


def bg_mode(dispcnt):
    return dispcnt & 0x7


def bgcnt_fields(bgcnt):
    """Decode one BGxCNT per GBATEK; matches gba_ppu.cpp's own decode."""
    return {
        "priority": bgcnt & 0x3,
        "char_base": ((bgcnt >> 2) & 0x3) * 0x4000,
        "mosaic": bool(bgcnt & 0x0040),
        "color256": bool(bgcnt & 0x0080),
        "screen_base": ((bgcnt >> 8) & 0x1F) * 0x800,
        "wrap": bool(bgcnt & 0x2000),
        "size_code": (bgcnt >> 14) & 0x3,
    }


def regular_bg_size(size_code):
    width_tiles = 64 if (size_code & 1) else 32
    height_tiles = 64 if (size_code & 2) else 32
    return width_tiles * 8, height_tiles * 8


def affine_bg_size(size_code):
    px = 128 << size_code
    return px, px


# ---- PNG writer (stdlib zlib, no PIL -- same approach as
# tools/render_maprec.py's write_png) --------------------------------------
def write_png(path, img, w, h):
    def chunk(tag, payload):
        c = tag + payload
        return (struct.pack(">I", len(payload)) + c +
                struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF))

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)
        raw.extend(img[y * stride:(y + 1) * stride])
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def bgr555_to_rgb888(v):
    r5, g5, b5 = v & 0x1F, (v >> 5) & 0x1F, (v >> 10) & 0x1F
    return ((r5 << 3) | (r5 >> 2), (g5 << 3) | (g5 >> 2), (b5 << 3) | (b5 >> 2))


def render_regular_bg(vram, palette_rgb, bgcnt):
    f = bgcnt_fields(bgcnt)
    width_px, height_px = regular_bg_size(f["size_code"])
    width_tiles, height_tiles = width_px // 8, height_px // 8
    block_cols = width_tiles // 32
    img = bytearray(width_px * height_px * 3)
    tile_bytes = 64 if f["color256"] else 32

    for tile_y in range(height_tiles):
        for tile_x in range(width_tiles):
            block = (tile_x >> 5) + (tile_y >> 5) * block_cols
            map_off = (f["screen_base"] + block * 0x800 +
                       ((tile_y & 31) * 32 + (tile_x & 31)) * 2)
            if map_off + 1 >= len(vram):
                continue
            entry = vram[map_off] | (vram[map_off + 1] << 8)
            tile_num = entry & 0x03FF
            hflip = bool(entry & 0x0400)
            vflip = bool(entry & 0x0800)
            pal_bank = (entry >> 12) & 0xF
            base = f["char_base"] + tile_num * tile_bytes
            for py in range(8):
                sy = 7 - py if vflip else py
                for px in range(8):
                    sx = 7 - px if hflip else px
                    if f["color256"]:
                        off = base + sy * 8 + sx
                        if off >= len(vram):
                            continue
                        idx = vram[off]
                        if idx == 0:
                            continue
                        rgb = palette_rgb[idx] if idx < len(palette_rgb) else (0, 0, 0)
                    else:
                        byte_off = base + sy * 4 + (sx >> 1)
                        if byte_off >= len(vram):
                            continue
                        byte = vram[byte_off]
                        nib = (byte >> 4) if (sx & 1) else (byte & 0xF)
                        if nib == 0:
                            continue
                        full_idx = pal_bank * 16 + nib
                        rgb = palette_rgb[full_idx] if full_idx < len(palette_rgb) else (0, 0, 0)
                    o = ((tile_y * 8 + py) * width_px + tile_x * 8 + px) * 3
                    img[o], img[o + 1], img[o + 2] = rgb
    return img, width_px, height_px


def render_affine_bg(vram, palette_rgb, bgcnt):
    """Affine BGs use 1-byte tile indices, no flip bits, 256-colour only
    (FACTS.md 'Scene classes: Overworld, Mode 2', measured 2026-09-04)."""
    f = bgcnt_fields(bgcnt)
    width_px, height_px = affine_bg_size(f["size_code"])
    tiles = width_px // 8
    img = bytearray(width_px * height_px * 3)

    for tile_y in range(tiles):
        for tile_x in range(tiles):
            map_off = f["screen_base"] + tile_y * tiles + tile_x
            if map_off >= len(vram):
                continue
            tile_num = vram[map_off]
            base = f["char_base"] + tile_num * 64
            for py in range(8):
                for px in range(8):
                    off = base + py * 8 + px
                    if off >= len(vram):
                        continue
                    idx = vram[off]
                    if idx == 0:
                        continue
                    rgb = palette_rgb[idx] if idx < len(palette_rgb) else (0, 0, 0)
                    o = ((tile_y * 8 + py) * width_px + tile_x * 8 + px) * 3
                    img[o], img[o + 1], img[o + 2] = rgb
    return img, width_px, height_px


# ---- Mode 0 metatile grid (FACTS.md "Field map table layout") ------------
EWRAM_BASE = 0x02000000
METATILE_GRID_ADDR = 0x02010000
TILE_ATLAS_ADDR = 0x02020000
GRID_W = GRID_H = 128
CELL_PX = 16

ROOM_BOUNDS_ADDR = 0x02030CC0
ROOM_BOUNDS_END = 0x02030D20


def resolve_terrain_layer(regs):
    """Pick which BGxCNT register supplies char_base/colour-depth for the
    shared metatile atlas. The atlas entry format (10-bit tile number + 2
    flip bits + 4-bit palette bank) is the standard GBA 4bpp regular-BG
    screen-entry layout, but Golden Sun has been observed loading BG1-3
    with either depth, so this cannot be hardcoded -- ported from
    tools/render_maprec.py's resolve_terrain_layer (2026-09-04 measurement:
    both depths seen across real captures), not a fresh guess.

    Rule: the lowest-indexed of BG1/BG2/BG3 that DISPCNT reports enabled
    (matches golden_sun_field_terrain_bg). If DISPCNT enables none of them
    (a transition/menu frame), fall back to whichever of BG1-3 has a
    non-zero BGxCNT (a zero register is the power-on/never-configured
    value); if more than one is non-zero the caller disambiguates by
    decoding both and comparing non-black fraction (see
    decode_metatile_grid's `candidates` handling below).
    """
    dispcnt = regs["DISPCNT"]
    if dispcnt & 0x0200:
        return 1, "DISPCNT enables BG1 (strict rule)"
    if dispcnt & 0x0400:
        return 2, "DISPCNT enables BG2 (strict rule)"
    if dispcnt & 0x0800:
        return 3, "DISPCNT enables BG3 (strict rule)"

    bgcnt_map = {1: regs["BG1CNT"], 2: regs["BG2CNT"], 3: regs["BG3CNT"]}
    candidates = [b for b, v in bgcnt_map.items() if v != 0]
    if not candidates:
        return None, "DISPCNT enables none of BG1-3 and all BGxCNT are 0x0000 -- TODO-EVIDENCE, no terrain layer for this snapshot"
    if len(candidates) == 1:
        b = candidates[0]
        return b, "DISPCNT enables none of BG1-3; only BG%d has a non-zero BGCNT" % b
    return candidates, ("DISPCNT enables none of BG1-3 and multiple BGxCNT are "
                        "non-zero (%s); disambiguating by non-black fraction" %
                        candidates)


def decode_metatile_grid_pixels(ewram, palette_rgb, vram, char_base, color256):
    grid_off = METATILE_GRID_ADDR - EWRAM_BASE
    atlas_off = TILE_ATLAS_ADDR - EWRAM_BASE
    grid = struct.unpack_from("<%dI" % (GRID_W * GRID_H), ewram, grid_off)
    atlas = ewram[atlas_off:atlas_off + 4096 * 8]

    w, h = GRID_W * CELL_PX, GRID_H * CELL_PX
    img = bytearray(w * h * 3)
    opaque = 0
    total = 0

    def decode_tile_pixel(tile_number, palette_bank, hflip, vflip, px, py):
        sx = 7 - px if hflip else px
        sy = 7 - py if vflip else py
        if color256:
            off = char_base + tile_number * 64 + sy * 8 + sx
            if off >= len(vram):
                return None
            idx = vram[off]
            if idx == 0:
                return None
            return palette_rgb[idx] if idx < len(palette_rgb) else (0, 0, 0)
        off = char_base + tile_number * 32 + sy * 4 + (sx >> 1)
        if off >= len(vram):
            return None
        byte = vram[off]
        nib = (byte >> 4) if (sx & 1) else (byte & 0xF)
        if nib == 0:
            return None
        idx = palette_bank * 16 + nib
        return palette_rgb[idx] if idx < len(palette_rgb) else (0, 0, 0)

    for cell_y in range(GRID_H):
        for cell_x in range(GRID_W):
            metatile_id = grid[cell_y * GRID_W + cell_x] & 0xFFF
            entry_off = metatile_id * 8
            if entry_off + 8 > len(atlas):
                continue
            for sub_ty in (0, 1):
                for sub_tx in (0, 1):
                    o2 = entry_off + sub_ty * 4 + sub_tx * 2
                    raw_entry = atlas[o2] | (atlas[o2 + 1] << 8)
                    tile_number = raw_entry & 0x03FF
                    hflip = bool(raw_entry & 0x0400)
                    vflip = bool(raw_entry & 0x0800)
                    pal_bank = (raw_entry >> 12) & 0xF
                    base_px = cell_x * CELL_PX + sub_tx * 8
                    base_py = cell_y * CELL_PX + sub_ty * 8
                    for py in range(8):
                        for px in range(8):
                            total += 1
                            rgb = decode_tile_pixel(tile_number, pal_bank,
                                                    hflip, vflip, px, py)
                            if rgb is None:
                                continue
                            opaque += 1
                            o = ((base_py + py) * w + base_px + px) * 3
                            img[o], img[o + 1], img[o + 2] = rgb
    return img, w, h, (opaque / total if total else 0.0)


def render_metatile_grid(ewram, palette_rgb, vram, regs):
    bg, note = resolve_terrain_layer(regs)
    print("metatile grid terrain layer: %s" % note)
    if bg is None:
        return None
    bgcnt_by_layer = {1: regs["BG1CNT"], 2: regs["BG2CNT"], 3: regs["BG3CNT"]}
    if isinstance(bg, list):
        scored = []
        for b in bg:
            f = bgcnt_fields(bgcnt_by_layer[b])
            img, w, h, frac = decode_metatile_grid_pixels(
                ewram, palette_rgb, vram, f["char_base"], f["color256"])
            print("  BG%d: char_base=0x%04X color256=%d -> opaque fraction %.4f"
                  % (b, f["char_base"], f["color256"], frac))
            scored.append((frac, img, w, h))
        scored.sort(key=lambda t: t[0], reverse=True)
        _, img, w, h = scored[0]
        return img, w, h
    f = bgcnt_fields(bgcnt_by_layer[bg])
    img, w, h, _frac = decode_metatile_grid_pixels(
        ewram, palette_rgb, vram, f["char_base"], f["color256"])
    return img, w, h


def hexdump(data, base_addr):
    lines = []
    for i in range(0, len(data), 16):
        row = data[i:i + 16]
        hexpart = " ".join("%02X" % b for b in row)
        asciipart = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append("  %08X  %-47s  %s" % (base_addr + i, hexpart, asciipart))
    return "\n".join(lines)


def decode_one(path, out_dir):
    snap = parse_snapshot(path)
    io = snap["sections"]["IO"]
    ewram = snap["sections"]["EWRAM"]
    vram = snap["sections"]["VRAM"]
    pal_raw = snap["sections"]["PAL"]
    palette = struct.unpack_from("<%dH" % (len(pal_raw) // 2), pal_raw, 0)
    palette_rgb = [bgr555_to_rgb888(p) for p in palette]

    print("=== %s ===" % path)
    print("version=%d frame=%d tag=%r" % (snap["version"], snap["frame"], snap["tag"]))
    regs = decode_io_registers(io)
    print_io_registers(regs)
    mode = bg_mode(regs["DISPCNT"])
    print("BG mode: %d" % mode)

    base = os.path.splitext(os.path.basename(path))[0]
    if out_dir is None:
        out_dir = os.path.dirname(path) or "."
    os.makedirs(out_dir, exist_ok=True)

    bgcnts = {0: regs["BG0CNT"], 1: regs["BG1CNT"], 2: regs["BG2CNT"], 3: regs["BG3CNT"]}
    affine_layers = set()
    if mode == 1:
        affine_layers = {2}
    elif mode == 2:
        affine_layers = {2, 3}
    for layer in range(4):
        if not (regs["DISPCNT"] & (0x0100 << layer)):
            continue
        bgcnt = bgcnts[layer]
        if layer in affine_layers:
            img, w, h = render_affine_bg(vram, palette_rgb, bgcnt)
        else:
            img, w, h = render_regular_bg(vram, palette_rgb, bgcnt)
        png_path = os.path.join(out_dir, "%s_bg%d.png" % (base, layer))
        write_png(png_path, img, w, h)
        print("wrote %s (%dx%d)" % (png_path, w, h))

    if mode == 0:
        result = render_metatile_grid(ewram, palette_rgb, vram, regs)
        if result is None:
            print("skipped metatile grid: no terrain layer resolvable (see note above)")
        else:
            img, w, h = result
            png_path = os.path.join(out_dir, "%s_metatiles.png" % base)
            write_png(png_path, img, w, h)
            print("wrote %s (%dx%d)" % (png_path, w, h))

    dump_off = ROOM_BOUNDS_ADDR - EWRAM_BASE
    dump_len = ROOM_BOUNDS_END - ROOM_BOUNDS_ADDR
    region = ewram[dump_off:dump_off + dump_len]
    print("EWRAM 0x%08X-0x%08X (room-bounds struct region, offsets TODO-EVIDENCE):"
          % (ROOM_BOUNDS_ADDR, ROOM_BOUNDS_END))
    print(hexdump(region, ROOM_BOUNDS_ADDR))


# ---- compare mode ----------------------------------------------------------
SNAP_NAME_RE = re.compile(r"^snap_(\d+)_(.+)$")


def load_frames_csv(session_dir):
    """Returns a list of (frame:int, set-of-changed-page-indices)."""
    path = os.path.join(session_dir, "frames.csv")
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, "r", newline="") as f:
        header = f.readline()
        if not header:
            return rows
        cols = header.strip().split(",")
        frame_idx = cols.index("frame")
        pages_idx = cols.index("changed_pages")
        for line in f:
            parts = line.rstrip("\n").split(",")
            if len(parts) <= max(frame_idx, pages_idx):
                continue
            frame = int(parts[frame_idx])
            pages_field = parts[pages_idx]
            pages = set()
            if pages_field:
                for cell in pages_field.split("|"):
                    idx_str = cell.split(":", 1)[0]
                    if idx_str:
                        pages.add(int(idx_str))
            rows.append((frame, pages))
    return rows


def pages_changed_in_span(frame_rows, start_frame, end_frame):
    """end_frame is exclusive, or None for "to the end of the log". Returns
    None if start_frame > end_frame (the frame counter moved backward across
    this span -- see map_recorder.cpp's note on savestate loads jumping
    runtime_current_frame() non-monotonically). Never guesses across a
    rewind; callers must report that case explicitly rather than silently
    computing a wrong set."""
    if end_frame is not None and end_frame < start_frame:
        return None
    changed = set()
    for frame, pages in frame_rows:
        if frame < start_frame:
            continue
        if end_frame is not None and frame >= end_frame:
            continue
        changed |= pages
    return changed


def compare_mode(session_dir):
    paths = sorted(glob.glob(os.path.join(session_dir, "snap_*.bin")))
    if not paths:
        print("no snap_*.bin files in %s" % session_dir)
        return
    frame_rows = load_frames_csv(session_dir)

    entries = []
    for p in paths:
        m = SNAP_NAME_RE.match(os.path.splitext(os.path.basename(p))[0])
        seq = int(m.group(1)) if m else -1
        snap = parse_snapshot(p)
        regs = decode_io_registers(snap["sections"]["IO"])
        entries.append({
            "seq": seq, "path": p, "frame": snap["frame"], "tag": snap["tag"],
            "regs": regs,
        })
    entries.sort(key=lambda e: e["seq"])

    for i, e in enumerate(entries):
        start = e["frame"]
        end = entries[i + 1]["frame"] if i + 1 < len(entries) else None
        pages = pages_changed_in_span(frame_rows, start, end)
        e["changed_pages"] = pages

    def signature(e):
        r = e["regs"]
        return (bg_mode(r["DISPCNT"]), r["DISPCNT"], r["BG0CNT"], r["BG1CNT"],
                r["BG2CNT"], r["BG3CNT"])

    signatures = sorted(set(signature(e) for e in entries))
    sig_group = {sig: i for i, sig in enumerate(signatures)}

    for e in entries:
        e["group"] = sig_group[signature(e)]

    ordered = sorted(entries, key=lambda e: (e["group"], e["seq"]))

    print("%-4s %-8s %-24s %-4s %-6s %-6s %-24s %-24s %-24s %-24s %s"
          % ("grp", "seq", "label", "mode", "frame", "dispcnt",
             "bg0cnt", "bg1cnt", "bg2cnt", "bg3cnt", "changed_pages"))
    last_group = None
    for e in ordered:
        if last_group is not None and e["group"] != last_group:
            print("-" * 40 + " (signature changes here) " + "-" * 40)
        last_group = e["group"]
        r = e["regs"]

        def bgcnt_str(bgcnt):
            f = bgcnt_fields(bgcnt)
            return ("cb=0x%04X sb=0x%04X sz=%d wrap=%d pri=%d c256=%d" %
                    (f["char_base"], f["screen_base"], f["size_code"],
                     f["wrap"], f["priority"], f["color256"]))

        if e["changed_pages"] is None:
            pages_str = "N/A (frame counter moved backward in this span -- savestate load)"
        else:
            pages_str = ",".join(str(i) for i in sorted(e["changed_pages"]))
            pages_str = "%d pages: %s" % (len(e["changed_pages"]), pages_str)

        print("%-4d %-8d %-24s %-4d %-6d 0x%04X %-24s %-24s %-24s %-24s %s"
              % (e["group"], e["seq"], e["tag"][:24], bg_mode(r["DISPCNT"]),
                 e["frame"], r["DISPCNT"], bgcnt_str(r["BG0CNT"]),
                 bgcnt_str(r["BG1CNT"]), bgcnt_str(r["BG2CNT"]),
                 bgcnt_str(r["BG3CNT"]), pages_str))


# ---- roomload_<frame>.bin (see map_recorder.cpp's write_room_load_capture) --
ROOMLOAD_MAGIC = b"GSRRLOD1"
# magic(8s) version(I) trigger_frame(Q) frame_count(I) bitmap_bytes(I)
# ewram_base(I) line_bytes(I) line_count(I) -- 40 bytes, matches the C++
# writer field-for-field.
ROOMLOAD_HEADER_FMT = "<8sIQIIIII"
ROOMLOAD_HEADER_BYTES = struct.calcsize(ROOMLOAD_HEADER_FMT)


def parse_roomload(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[0:8] != ROOMLOAD_MAGIC:
        raise ValueError("%s: bad magic %r" % (path, data[0:8]))
    (magic, version, trigger_frame, frame_count, bitmap_bytes, ewram_base,
     line_bytes, line_count) = struct.unpack_from(ROOMLOAD_HEADER_FMT, data, 0)
    off = ROOMLOAD_HEADER_BYTES
    frames = []
    for _ in range(frame_count):
        (frame_no,) = struct.unpack_from("<Q", data, off)
        off += 8
        bitmap = data[off:off + bitmap_bytes]
        off += bitmap_bytes
        frames.append((frame_no, bitmap))
    return {
        "version": version, "trigger_frame": trigger_frame,
        "frame_count": frame_count, "bitmap_bytes": bitmap_bytes,
        "ewram_base": ewram_base, "line_bytes": line_bytes,
        "line_count": line_count, "frames": frames,
    }


def bitmap_lines_set(bitmap):
    lines = set()
    for byte_i, b in enumerate(bitmap):
        if not b:
            continue
        for bit in range(8):
            if b & (1 << bit):
                lines.add(byte_i * 8 + bit)
    return lines


def resolve_roomload_paths(inputs):
    paths = []
    for p in inputs:
        if os.path.isdir(p):
            paths.extend(sorted(glob.glob(os.path.join(p, "roomload_*.bin"))))
        else:
            matches = sorted(glob.glob(p))
            paths.extend(matches if matches else [p])
    return paths


def roomload_intersect_mode(inputs):
    paths = resolve_roomload_paths(inputs)
    if not paths:
        print("no roomload_*.bin files found for %r" % (inputs,))
        return

    geometry = None
    per_file = []
    intersection = None
    for path in paths:
        rl = parse_roomload(path)
        this_geometry = (rl["ewram_base"], rl["line_bytes"], rl["line_count"])
        if geometry is None:
            geometry = this_geometry
        elif this_geometry != geometry:
            print("%s: geometry %r does not match %r, skipping" %
                 (path, this_geometry, geometry))
            continue
        touched = set()
        for _frame_no, bitmap in rl["frames"]:
            touched |= bitmap_lines_set(bitmap)
        per_file.append((path, rl["trigger_frame"], rl["frame_count"], touched))
        intersection = touched if intersection is None else (intersection & touched)

    if geometry is None:
        print("no usable roomload_*.bin files")
        return
    ewram_base, line_bytes, line_count = geometry

    print("Per-file lines touched during the load's captured window:")
    for path, trigger_frame, frame_count, touched in per_file:
        print("  %-40s trigger_frame=%-10d frames=%-3d lines=%d/%d" %
             (os.path.basename(path), trigger_frame, frame_count,
              len(touched), line_count))

    if intersection is None:
        intersection = set()
    print("\nIntersection across %d file(s): %d of %d lines" %
         (len(per_file), len(intersection), line_count))
    for line in sorted(intersection):
        addr = ewram_base + line * line_bytes
        print("  line %5d  0x%08X..0x%08X" %
             (line, addr, addr + line_bytes - 1))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", nargs="?", help="snapshot .bin file to decode")
    ap.add_argument("--out", help="output directory for PNGs (decode mode)")
    ap.add_argument("--compare", metavar="SESSION_DIR",
                    help="print a comparison table over every snapshot in SESSION_DIR")
    ap.add_argument("--roomload-intersect", nargs="+", metavar="PATH",
                    help="intersect EWRAM lines touched across one or more "
                         "roomload_*.bin files (each PATH may be a file, a "
                         "glob, or a directory)")
    args = ap.parse_args()

    if args.roomload_intersect:
        roomload_intersect_mode(args.roomload_intersect)
        return
    if args.compare:
        compare_mode(args.compare)
        return
    if not args.path:
        ap.error("a snapshot path is required unless --compare or "
                 "--roomload-intersect is used")
    decode_one(args.path, args.out)


if __name__ == "__main__":
    main()
