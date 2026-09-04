#!/usr/bin/env python3
"""Throwaway analysis tool: render GSR_MAP_RECORD .bin room dumps as PNGs.

Layout follows golden_sun_map_record_write_binary (src/runner_main.cpp) and
the tile lookup arithmetic in golden_sun_field_tilemap_entry /
golden_sun_field_terrain_bg (src/widescreen_policy.h). Not part of the
product; not committed to the repo.
"""
import struct
import sys
import zlib
import os

GRID_W = 128
GRID_H = 128
CELL_PX = 16

DISPCNT_BG1 = 0x0200
DISPCNT_BG2 = 0x0400
DISPCNT_BG3 = 0x0800


def terrain_bg(dispcnt):
    if dispcnt & DISPCNT_BG1:
        return 1
    if dispcnt & DISPCNT_BG2:
        return 2
    if dispcnt & DISPCNT_BG3:
        return 3
    return 0


def parse_header(data):
    magic = data[0:8]
    assert magic == b"GSRMAP1\x00", magic
    version, sequence, room_ptr, min_x, min_y, ext_x, ext_y, snap_captured = \
        struct.unpack_from("<8I", data, 8)
    dispcnt, bg0cnt, bg1cnt, bg2cnt, bg3cnt, reserved = \
        struct.unpack_from("<6H", data, 8 + 8 * 4)
    (section_count,) = struct.unpack_from("<I", data, 8 + 8 * 4 + 6 * 2)
    assert section_count == 6, section_count
    table_off = 8 + 8 * 4 + 6 * 2 + 4
    assert table_off == 0x38, hex(table_off)
    sections = []
    off = table_off
    for i in range(6):
        soff, slen = struct.unpack_from("<II", data, off)
        sections.append((soff, slen))
        off += 8
    assert off == 0x68, hex(off)
    return {
        "sequence": sequence,
        "room_ptr": room_ptr,
        "min_x": min_x, "min_y": min_y, "ext_x": ext_x, "ext_y": ext_y,
        "dispcnt": dispcnt, "bg0cnt": bg0cnt, "bg1cnt": bg1cnt,
        "bg2cnt": bg2cnt, "bg3cnt": bg3cnt,
        "sections": sections,
    }


def load_room(path):
    with open(path, "rb") as f:
        data = f.read()
    h = parse_header(data)
    (ids_off, ids_len) = h["sections"][0]
    (arr_off, arr_len) = h["sections"][1]
    (fin_off, fin_len) = h["sections"][2]
    (raw_off, raw_len) = h["sections"][3]
    (vram_off, vram_len) = h["sections"][4]
    (pal_off, pal_len) = h["sections"][5]

    assert ids_len == GRID_W * GRID_H * 2
    assert raw_len == 4096 * 8
    assert vram_len == 0x10000
    assert pal_len == 512

    ids_merged = struct.unpack_from("<%dH" % (GRID_W * GRID_H), data, ids_off)
    raw_tile_table = data[raw_off:raw_off + raw_len]
    vram = data[vram_off:vram_off + vram_len]
    palraw = struct.unpack_from("<256H", data, pal_off)

    h["ids_merged"] = ids_merged
    h["raw_tile_table"] = raw_tile_table
    h["vram"] = vram
    h["palette"] = palraw
    return h


def bgr555_to_rgb888(v):
    r5 = v & 0x1F
    g5 = (v >> 5) & 0x1F
    b5 = (v >> 10) & 0x1F
    # 5-bit -> 8-bit via bit replication
    r = (r5 << 3) | (r5 >> 2)
    g = (g5 << 3) | (g5 >> 2)
    b = (b5 << 3) | (b5 >> 2)
    return r, g, b


def decode_room(room, bg):
    """Decode the merged metatile grid to pixels using BGx (x=bg)'s control
    register for depth/char-base. No fallback logic here -- see
    resolve_terrain_layer for how `bg` gets picked."""
    bgcnt = {0: room["bg0cnt"], 1: room["bg1cnt"], 2: room["bg2cnt"],
              3: room["bg3cnt"]}[bg]
    depth8 = bool(bgcnt & 0x0080)
    char_base_block = (bgcnt >> 2) & 0x3
    char_base = char_base_block * 0x4000

    vram = room["vram"]
    palette = room["palette"]
    raw_tile_table = room["raw_tile_table"]
    ids = room["ids_merged"]

    pal_rgb = [bgr555_to_rgb888(p) for p in palette]

    tile_bytes = 64 if depth8 else 32

    W = GRID_W * CELL_PX
    H = GRID_H * CELL_PX
    img = bytearray(W * H * 3)  # RGB

    def set_px(x, y, rgb):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            img[o] = rgb[0]
            img[o + 1] = rgb[1]
            img[o + 2] = rgb[2]

    def decode_tile_pixel(tile_number, palette_bank, hflip, vflip, px, py):
        # px, py in [0,8)
        sx = 7 - px if hflip else px
        sy = 7 - py if vflip else py
        base = char_base + tile_number * tile_bytes
        if depth8:
            off = base + sy * 8 + sx
            if off >= len(vram):
                return None
            idx = vram[off]
            if idx == 0:
                return (0, 0, 0)
            if idx >= len(pal_rgb):
                return (0, 0, 0)
            return pal_rgb[idx]
        else:
            byte_off = base + sy * 4 + (sx >> 1)
            if byte_off >= len(vram):
                return None
            byte = vram[byte_off]
            nibble = (byte >> 4) if (sx & 1) else (byte & 0xF)
            if nibble == 0:
                return (0, 0, 0)
            full_idx = palette_bank * 16 + nibble
            if full_idx >= len(pal_rgb):
                return (0, 0, 0)
            return pal_rgb[full_idx]

    non_black_inside = 0
    total_inside = 0
    non_black_outside = 0
    total_outside = 0

    min_cell_x = (room["min_x"] >> 16) // CELL_PX
    min_cell_y = (room["min_y"] >> 16) // CELL_PX
    max_cell_x = min_cell_x + (room["ext_x"] >> 16) // CELL_PX
    max_cell_y = min_cell_y + (room["ext_y"] >> 16) // CELL_PX

    for map_y in range(GRID_H):
        for map_x in range(GRID_W):
            cell = map_y * GRID_W + map_x
            metatile_id = ids[cell] & 0x0FFF
            entry_off = metatile_id * 8
            inside_rect = (min_cell_x <= map_x < max_cell_x and
                           min_cell_y <= map_y < max_cell_y)
            # sub-tile positions: (tile_x&1, tile_y&1) -> raw_off offset
            # (ty&1)*4 + (tx&1)*2, matching golden_sun_field_tilemap_entry.
            for sub_ty in (0, 1):
                for sub_tx in (0, 1):
                    sub_off = entry_off + sub_ty * 4 + sub_tx * 2
                    if sub_off + 2 > len(raw_tile_table):
                        continue
                    raw_entry = raw_tile_table[sub_off] | \
                        (raw_tile_table[sub_off + 1] << 8)
                    tile_number = raw_entry & 0x03FF
                    hflip = bool(raw_entry & 0x0400)
                    vflip = bool(raw_entry & 0x0800)
                    palette_bank = (raw_entry >> 12) & 0xF
                    base_px = map_x * CELL_PX + sub_tx * 8
                    base_py = map_y * CELL_PX + sub_ty * 8
                    for py in range(8):
                        for px in range(8):
                            rgb = decode_tile_pixel(
                                tile_number, palette_bank, hflip, vflip,
                                px, py)
                            if rgb is None:
                                continue
                            set_px(base_px + px, base_py + py, rgb)
                            is_black = (rgb == (0, 0, 0))
                            if inside_rect:
                                total_inside += 1
                                if not is_black:
                                    non_black_inside += 1
                            else:
                                total_outside += 1
                                if not is_black:
                                    non_black_outside += 1

    stats = {
        "non_black_inside": non_black_inside,
        "total_inside": total_inside,
        "non_black_outside": non_black_outside,
        "total_outside": total_outside,
        "bg": bg, "depth8": depth8, "char_base_block": char_base_block,
        "char_base": char_base,
        "rect": (min_cell_x, min_cell_y, max_cell_x, max_cell_y),
    }
    return img, W, H, stats


def decode_layer_full(room, bg):
    """Decode BGx (x=bg, bg in 0..3) fully against the SAME merged metatile
    grid + raw tile-entry table used everywhere else, but keep an explicit
    per-pixel opacity mask (index 0 == transparent, per GBA tile hardware,
    not "black") instead of collapsing index 0 to black like decode_room
    does. Needed for back-to-front compositing and for an honest per-layer
    "is this layer actually drawing something here" fraction.

    Also tallies a simple blue-dominant heuristic per pixel (b5 clearly
    bigger than both r5 and g5) as a proxy for GBA water tiles, which in
    Golden Sun are conventionally saturated blue. This is a heuristic over
    decoded palette colour only -- it is not tile-content/asset knowledge,
    just colour-channel comparison on this snapshot's own palette.
    """
    bgcnt = {0: room["bg0cnt"], 1: room["bg1cnt"], 2: room["bg2cnt"],
              3: room["bg3cnt"]}[bg]
    depth8 = bool(bgcnt & 0x0080)
    char_base_block = (bgcnt >> 2) & 0x3
    char_base = char_base_block * 0x4000

    vram = room["vram"]
    palette = room["palette"]
    raw_tile_table = room["raw_tile_table"]
    ids = room["ids_merged"]

    pal_rgb = [bgr555_to_rgb888(p) for p in palette]

    def is_blueish(p):
        r5 = p & 0x1F
        g5 = (p >> 5) & 0x1F
        b5 = (p >> 10) & 0x1F
        return b5 > r5 + 4 and b5 > g5 + 4

    pal_blue = [is_blueish(p) for p in palette]

    tile_bytes = 64 if depth8 else 32

    W = GRID_W * CELL_PX
    H = GRID_H * CELL_PX
    img = bytearray(W * H * 3)     # RGB, transparent pixels left at (0,0,0)
    alpha = bytearray(W * H)       # 1 = opaque (palette index != 0)
    blue = bytearray(W * H)        # 1 = opaque AND blue-dominant palette entry

    def decode_tile_pixel(tile_number, palette_bank, hflip, vflip, px, py):
        sx = 7 - px if hflip else px
        sy = 7 - py if vflip else py
        base = char_base + tile_number * tile_bytes
        if depth8:
            off = base + sy * 8 + sx
            if off >= len(vram):
                return None
            idx = vram[off]
            if idx == 0 or idx >= len(pal_rgb):
                return None
            return pal_rgb[idx], pal_blue[idx]
        else:
            byte_off = base + sy * 4 + (sx >> 1)
            if byte_off >= len(vram):
                return None
            byte = vram[byte_off]
            nibble = (byte >> 4) if (sx & 1) else (byte & 0xF)
            if nibble == 0:
                return None
            full_idx = palette_bank * 16 + nibble
            if full_idx >= len(pal_rgb):
                return None
            return pal_rgb[full_idx], pal_blue[full_idx]

    min_cell_x = (room["min_x"] >> 16) // CELL_PX
    min_cell_y = (room["min_y"] >> 16) // CELL_PX
    max_cell_x = min_cell_x + (room["ext_x"] >> 16) // CELL_PX
    max_cell_y = min_cell_y + (room["ext_y"] >> 16) // CELL_PX

    opaque_inside = opaque_outside = 0
    total_inside = total_outside = 0
    blue_inside = blue_outside = 0

    for map_y in range(GRID_H):
        for map_x in range(GRID_W):
            cell = map_y * GRID_W + map_x
            metatile_id = ids[cell] & 0x0FFF
            entry_off = metatile_id * 8
            inside_rect = (min_cell_x <= map_x < max_cell_x and
                           min_cell_y <= map_y < max_cell_y)
            for sub_ty in (0, 1):
                for sub_tx in (0, 1):
                    sub_off = entry_off + sub_ty * 4 + sub_tx * 2
                    if sub_off + 2 > len(raw_tile_table):
                        continue
                    raw_entry = raw_tile_table[sub_off] | \
                        (raw_tile_table[sub_off + 1] << 8)
                    tile_number = raw_entry & 0x03FF
                    hflip = bool(raw_entry & 0x0400)
                    vflip = bool(raw_entry & 0x0800)
                    palette_bank = (raw_entry >> 12) & 0xF
                    base_px = map_x * CELL_PX + sub_tx * 8
                    base_py = map_y * CELL_PX + sub_ty * 8
                    for py in range(8):
                        row_o = ((base_py + py) * W + base_px) * 3
                        row_i = (base_py + py) * W + base_px
                        for px in range(8):
                            r = decode_tile_pixel(
                                tile_number, palette_bank, hflip, vflip,
                                px, py)
                            if inside_rect:
                                total_inside += 1
                            else:
                                total_outside += 1
                            if r is None:
                                continue
                            rgb, is_blue = r
                            o = row_o + px * 3
                            img[o] = rgb[0]
                            img[o + 1] = rgb[1]
                            img[o + 2] = rgb[2]
                            alpha[row_i + px] = 1
                            if inside_rect:
                                opaque_inside += 1
                            else:
                                opaque_outside += 1
                            if is_blue:
                                blue[row_i + px] = 1
                                if inside_rect:
                                    blue_inside += 1
                                else:
                                    blue_outside += 1

    stats = {
        "bg": bg, "depth8": depth8, "char_base_block": char_base_block,
        "char_base": char_base, "bgcnt": bgcnt,
        "opaque_inside": opaque_inside, "total_inside": total_inside,
        "opaque_outside": opaque_outside, "total_outside": total_outside,
        "blue_inside": blue_inside, "blue_outside": blue_outside,
        "rect": (min_cell_x, min_cell_y, max_cell_x, max_cell_y),
    }
    return img, alpha, blue, W, H, stats


def compare_metatile_regions(room, rx0, ry0, rx1, ry1):
    """Compare the merged 12-bit metatile-ID grid inside the room rect
    against the region immediately to its right (same rows, columns
    [rx1, GRID_W)), looking for an exact or x-shifted duplicate. Returns a
    dict describing what was found. This operates purely on
    ids_merged (metatile IDs), not on decoded pixels, so it is not an
    "eyeball" comparison.
    """
    ids = room["ids_merged"]
    rect_w = rx1 - rx0
    rect_h = ry1 - ry0

    def cellid(x, y):
        return ids[y * GRID_W + x] & 0x0FFF

    right_w = GRID_W - rx1
    result = {"rect_w": rect_w, "rect_h": rect_h, "right_w": right_w}

    # 1) Direct alignment: does region-to-the-right at [rx1, rx1+rect_w)
    #    equal the rect region cell-for-cell (same rows)?
    if right_w > 0:
        cmp_w = min(rect_w, right_w)
        match = 0
        total = cmp_w * rect_h
        for y in range(ry0, ry1):
            for x in range(cmp_w):
                if cellid(rx0 + x, y) == cellid(rx1 + x, y):
                    match += 1
        result["direct_alignment_match_fraction"] = match / total if total else 0.0
        result["direct_alignment_compare_w"] = cmp_w

    # 2) Best x-shift search: slide the rect-width window across every
    #    possible starting column in [rx1, GRID_W - rect_w] (same rows) and
    #    find the shift with the highest exact cell-ID match fraction
    #    against the rect. This detects a duplicate placed at any offset,
    #    not just immediately adjacent.
    best = None
    if rect_w > 0 and GRID_W - rect_w >= rx1:
        rect_cells = [[cellid(rx0 + x, y) for x in range(rect_w)]
                      for y in range(ry0, ry1)]
        for start_x in range(rx1, GRID_W - rect_w + 1):
            match = 0
            for yi, y in enumerate(range(ry0, ry1)):
                row = rect_cells[yi]
                for x in range(rect_w):
                    if row[x] == cellid(start_x + x, y):
                        match += 1
            total = rect_w * rect_h
            frac = match / total if total else 0.0
            if best is None or frac > best[1]:
                best = (start_x, frac)
    if best:
        result["best_shift_start_x"] = best[0]
        result["best_shift_offset"] = best[0] - rx0
        result["best_shift_match_fraction"] = best[1]

    # 3) For reference: fraction of cells to the right that are literally
    #    id 0 (never written this room) vs nonzero, to distinguish "empty"
    #    from "structured but different".
    if right_w > 0:
        nonzero = 0
        total = right_w * rect_h
        for y in range(ry0, ry1):
            for x in range(rx1, GRID_W):
                if cellid(x, y) != 0:
                    nonzero += 1
        result["right_region_nonzero_fraction"] = nonzero / total if total else 0.0

    return result


def composite_layers(room, layer_imgs):
    """Composite decoded per-layer (img, alpha) pairs back-to-front in GBA
    priority order BG3 (furthest back) -> BG2 -> BG1 -> BG0 (nearest front),
    treating palette index 0 (alpha==0) as transparent so a lower layer
    shows through. layer_imgs is a dict bg -> (img, alpha, W, H)."""
    W = H = None
    for img, alpha, w, h in layer_imgs.values():
        W, H = w, h
        break
    out = bytearray(W * H * 3)
    for bg in (3, 2, 1, 0):
        if bg not in layer_imgs:
            continue
        img, alpha, w, h = layer_imgs[bg]
        for i in range(W * H):
            if alpha[i]:
                o = i * 3
                out[o] = img[o]
                out[o + 1] = img[o + 1]
                out[o + 2] = img[o + 2]
    return out, W, H


def process_all_layers(bin_path, out_dir):
    room = load_room(bin_path)
    seq = room["sequence"]
    rx0, ry0, rx1, ry1 = None, None, None, None

    print("=== per-layer breakdown: sequence %d (%s) ===" %
          (seq, os.path.basename(bin_path)))
    print("  dispcnt=0x%04x bg0cnt=0x%04x bg1cnt=0x%04x bg2cnt=0x%04x "
          "bg3cnt=0x%04x" %
          (room["dispcnt"], room["bg0cnt"], room["bg1cnt"], room["bg2cnt"],
           room["bg3cnt"]))
    dispcnt = room["dispcnt"]
    bg_enabled = {
        0: bool(dispcnt & 0x0100),
        1: bool(dispcnt & 0x0200),
        2: bool(dispcnt & 0x0400),
        3: bool(dispcnt & 0x0800),
    }

    layer_imgs = {}
    layer_stats = {}
    for bg in (0, 1, 2, 3):
        img, alpha, blue, W, H, stats = decode_layer_full(room, bg)
        layer_imgs[bg] = (img, alpha, W, H)
        layer_stats[bg] = stats
        rx0, ry0, rx1, ry1 = stats["rect"]
        px0, py0, px1, py1 = (rx0 * CELL_PX, ry0 * CELL_PX,
                               rx1 * CELL_PX, ry1 * CELL_PX)

        out_img = bytearray(img)  # copy so the rect outline doesn't affect
                                   # the alpha-driven composite below
        draw_rect_outline(out_img, W, H, px0, py0, px1, py1)
        full_path = os.path.join(out_dir, "maprec_%04d_layer%d.png" % (seq, bg))
        write_png(full_path, out_img, W, H)
        small_img, W2, H2 = downscale_half(out_img, W, H)
        small_path = os.path.join(out_dir,
                                   "maprec_%04d_layer%d_small.png" % (seq, bg))
        write_png(small_path, small_img, W2, H2)

        frac_in = (stats["opaque_inside"] / stats["total_inside"]
                   if stats["total_inside"] else 0.0)
        frac_out = (stats["opaque_outside"] / stats["total_outside"]
                    if stats["total_outside"] else 0.0)
        blue_frac_in = (stats["blue_inside"] / stats["opaque_inside"]
                        if stats["opaque_inside"] else 0.0)
        blue_frac_out = (stats["blue_outside"] / stats["opaque_outside"]
                          if stats["opaque_outside"] else 0.0)
        print("  BG%d: bgcnt=0x%04x depth=%dbpp char_base_block=%d "
              "(0x%05x) dispcnt_enabled=%s" %
              (bg, stats["bgcnt"], 8 if stats["depth8"] else 4,
               stats["char_base_block"], stats["char_base"],
               bg_enabled[bg]))
        print("    non-black(opaque) fraction inside rect:  %.4f (%d/%d)" %
              (frac_in, stats["opaque_inside"], stats["total_inside"]))
        print("    non-black(opaque) fraction outside rect: %.4f (%d/%d)" %
              (frac_out, stats["opaque_outside"], stats["total_outside"]))
        print("    blue-dominant-palette fraction of opaque pixels: "
              "inside=%.4f outside=%.4f" % (blue_frac_in, blue_frac_out))
        print("    wrote: %s" % full_path)
        print("    wrote: %s" % small_path)

    # Metatile-ID region comparison (rect vs. area to its right), independent
    # of any single layer's colour decode.
    cmp = compare_metatile_regions(room, rx0, ry0, rx1, ry1)
    print("  --- metatile-ID region comparison (rect vs. area to its right) ---")
    print("  rect is %d x %d cells; %d columns lie to the right of it" %
          (cmp["rect_w"], cmp["rect_h"], cmp["right_w"]))
    if "direct_alignment_match_fraction" in cmp:
        print("  immediate-right window (same rows, cols [%d,%d)) exact "
              "cell-ID match vs rect: %.4f" %
              (rx1, rx1 + cmp["direct_alignment_compare_w"],
               cmp["direct_alignment_match_fraction"]))
    if "best_shift_match_fraction" in cmp:
        print("  best x-shifted window starts at col %d (offset +%d from "
              "rect origin), exact cell-ID match vs rect: %.4f" %
              (cmp["best_shift_start_x"], cmp["best_shift_offset"],
               cmp["best_shift_match_fraction"]))
    if "right_region_nonzero_fraction" in cmp:
        print("  fraction of cells to the right that are nonzero "
              "(i.e. \"structured\", not never-written): %.4f" %
              cmp["right_region_nonzero_fraction"])

    # Composite, back (BG3) to front (BG0), index-0 transparent.
    comp_img, W, H = composite_layers(room, layer_imgs)
    draw_rect_outline(comp_img, W, H, rx0 * CELL_PX, ry0 * CELL_PX,
                       rx1 * CELL_PX, ry1 * CELL_PX)
    comp_path = os.path.join(out_dir, "maprec_%04d_merged.png" % seq)
    write_png(comp_path, comp_img, W, H)
    small_comp, W2, H2 = downscale_half(comp_img, W, H)
    comp_small_path = os.path.join(out_dir, "maprec_%04d_merged_small.png" % seq)
    write_png(comp_small_path, small_comp, W2, H2)
    print("  wrote: %s" % comp_path)
    print("  wrote: %s" % comp_small_path)
    print()


def resolve_terrain_layer(room):
    """Pick which BGxCNT register to use for depth/char-base.

    Primary rule (as specified / mirrors golden_sun_field_terrain_bg): the
    lowest-indexed of BG1/BG2/BG3 that DISPCNT reports enabled.

    Both captured rooms have DISPCNT registers that do NOT enable any of
    BG1/BG2/BG3 at the single instant the snapshot was taken (room 1:
    DISPCNT=0x1002, only OBJ enabled; room 3: DISPCNT=0x4140, only BG0
    enabled) -- likely a transition/menu/lighting frame caught by the
    "brightest palette" capture heuristic, not the field-visible frame. This
    is a genuine gap in the strict rule as specified, not something I can
    silently paper over, so this function falls back to an evidence-based
    heuristic and reports exactly what it did:

      1. Restrict candidates to BG1/BG2/BG3 whose BGxCNT is non-zero (a
         zero register is the hardware power-on/never-configured value, not
         a real "4bpp/charbase0" choice).
      2. If exactly one candidate is non-zero, use it.
      3. If multiple are non-zero (room 3: all three are), decode each
         fully against this snapshot's actual VRAM/palette bytes and take
         the one with the highest overall non-black fraction, on the
         reasoning that a wrong char-base/depth pairing for this specific
         VRAM buffer will mostly hit unloaded/blank tiles.

    Returns (bg, notes) where notes is a list of strings describing what
    was tried, for the report.
    """
    notes = []
    dispcnt = room["dispcnt"]
    strict_bg = terrain_bg(dispcnt)
    if strict_bg != 0:
        notes.append("DISPCNT=0x%04x reports BG%d enabled (strict rule)." %
                      (dispcnt, strict_bg))
        return strict_bg, notes

    bgcnt_map = {1: room["bg1cnt"], 2: room["bg2cnt"], 3: room["bg3cnt"]}
    notes.append(
        "DISPCNT=0x%04x enables none of BG1/BG2/BG3 (bit9/10/11 clear) -- "
        "strict rule yields no terrain layer for this snapshot frame." %
        dispcnt)
    candidates = [b for b, v in bgcnt_map.items() if v != 0]
    if not candidates:
        raise RuntimeError(
            "No BG1/BG2/BG3 register has ever been configured (all "
            "BGxCNT == 0) -- TODO-EVIDENCE, cannot pick a terrain layer.")
    if len(candidates) == 1:
        bg = candidates[0]
        notes.append(
            "Only BG%d has a non-zero BGCNT (0x%04x); the others are "
            "0x0000 (never configured). Using BG%d." %
            (bg, bgcnt_map[bg], bg))
        return bg, notes

    notes.append(
        "Multiple BG1-3 registers are non-zero (%s); disambiguating by "
        "decoding each against this snapshot's VRAM and comparing overall "
        "non-black fraction." %
        ", ".join("BG%d=0x%04x" % (b, bgcnt_map[b]) for b in candidates))
    scored = []
    for b in candidates:
        _, _, _, s = decode_room(room, b)
        total = s["total_inside"] + s["total_outside"]
        nonblack = s["non_black_inside"] + s["non_black_outside"]
        frac = nonblack / total if total else 0.0
        scored.append((frac, b, s))
        notes.append(
            "  BG%d: depth=%dbpp char_base_block=%d -> overall non-black "
            "fraction %.4f" % (b, 8 if s["depth8"] else 4,
                                s["char_base_block"], frac))
    scored.sort(key=lambda t: t[0], reverse=True)
    best_frac, best_bg, _ = scored[0]
    notes.append("Chose BG%d (highest non-black fraction %.4f)." %
                  (best_bg, best_frac))
    return best_bg, notes


def render_room(room):
    bg, notes = resolve_terrain_layer(room)
    img, W, H, stats = decode_room(room, bg)
    stats["resolution_notes"] = notes
    return img, W, H, stats


def draw_rect_outline(img, W, H, x0, y0, x1, y1, color=(255, 0, 255),
                       thickness=2):
    def set_px(x, y):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            img[o] = color[0]
            img[o + 1] = color[1]
            img[o + 2] = color[2]

    for t in range(thickness):
        for x in range(x0, x1):
            set_px(x, y0 + t)
            set_px(x, y1 - 1 - t)
        for y in range(y0, y1):
            set_px(x0 + t, y)
            set_px(x1 - 1 - t, y)


def downscale_half(img, W, H):
    W2, H2 = W // 2, H // 2
    out = bytearray(W2 * H2 * 3)
    for y in range(H2):
        for x in range(W2):
            sx = x * 2
            sy = y * 2
            acc = [0, 0, 0]
            for dy in (0, 1):
                for dx in (0, 1):
                    o = ((sy + dy) * W + (sx + dx)) * 3
                    acc[0] += img[o]
                    acc[1] += img[o + 1]
                    acc[2] += img[o + 2]
            oo = (y * W2 + x) * 3
            out[oo] = acc[0] // 4
            out[oo + 1] = acc[1] // 4
            out[oo + 2] = acc[2] // 4
    return out, W2, H2


def write_png(path, img, W, H):
    def chunk(tag, data):
        c = tag + data
        return (struct.pack(">I", len(data)) + c +
                struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF))

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)  # 8-bit, RGB
    raw = bytearray()
    stride = W * 3
    for y in range(H):
        raw.append(0)  # filter type 0 (none)
        raw.extend(img[y * stride:(y + 1) * stride])
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def process(bin_path, out_dir):
    room = load_room(bin_path)
    seq = room["sequence"]
    img, W, H, stats = render_room(room)
    rx0, ry0, rx1, ry1 = stats["rect"]
    px0, py0, px1, py1 = rx0 * CELL_PX, ry0 * CELL_PX, rx1 * CELL_PX, ry1 * CELL_PX
    draw_rect_outline(img, W, H, px0, py0, px1, py1)

    full_path = os.path.join(out_dir, "maprec_%04d_render.png" % seq)
    write_png(full_path, img, W, H)

    small_img, W2, H2 = downscale_half(img, W, H)
    small_path = os.path.join(out_dir, "maprec_%04d_render_small.png" % seq)
    write_png(small_path, small_img, W2, H2)

    frac_in = (stats["non_black_inside"] / stats["total_inside"]
               if stats["total_inside"] else 0.0)
    frac_out = (stats["non_black_outside"] / stats["total_outside"]
                if stats["total_outside"] else 0.0)

    print("=== sequence %d (%s) ===" % (seq, os.path.basename(bin_path)))
    for n in stats["resolution_notes"]:
        print("  [layer resolution] %s" % n)
    print("  terrain BG = BG%d, depth = %dbpp, char_base_block = %d (0x%05x)"
          % (stats["bg"], 8 if stats["depth8"] else 4,
             stats["char_base_block"], stats["char_base"]))
    print("  dispcnt=0x%04x bg0cnt=0x%04x bg1cnt=0x%04x bg2cnt=0x%04x "
          "bg3cnt=0x%04x" %
          (room["dispcnt"], room["bg0cnt"], room["bg1cnt"], room["bg2cnt"],
           room["bg3cnt"]))
    print("  room rect (cells): x=[%d,%d) y=[%d,%d)" % (rx0, rx1, ry0, ry1))
    print("  non-black fraction inside rect:  %.4f (%d/%d)" %
          (frac_in, stats["non_black_inside"], stats["total_inside"]))
    print("  non-black fraction outside rect: %.4f (%d/%d)" %
          (frac_out, stats["non_black_outside"], stats["total_outside"]))
    print("  wrote: %s" % full_path)
    print("  wrote: %s" % small_path)


if __name__ == "__main__":
    logs_dir = sys.argv[1] if len(sys.argv) > 1 else "logs"
    targets = [
        os.path.join(logs_dir, "maprec_0001_02030ccc_0x0_512x256.bin"),
        os.path.join(logs_dir, "maprec_0003_02030ccc_0x0_800x512.bin"),
    ]
    for t in targets:
        process(t, logs_dir)
    for t in targets:
        process_all_layers(t, logs_dir)
