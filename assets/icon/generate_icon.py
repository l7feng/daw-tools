"""DAW Tools icon generator - standard library only, reproducible.

Design: rounded slate tile, film-frame corner marks, a symmetric audio
waveform with one amber centre bar (playhead / frame), plus two timeline rules.
Every size is redrawn from the same vector-style parameters instead of being
scaled down, so small sizes stay legible.

Outputs (next to this file):
  daw-tools-512.png   preview
  daw-tools.ico       multi-size Windows icon

Run: python assets/icon/generate_icon.py
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
PREVIEW = 512

BG_A = (26, 43, 54)
BG_B = (12, 22, 30)
EDGE = (53, 80, 95)
FRAME = (143, 199, 214)
WAVE = (93, 202, 165)
ACCENT = (250, 199, 117)

VALS = (0.25, 0.45, 0.68, 0.92, 0.55, 1.0, 0.55, 0.92, 0.68, 0.45, 0.25)
SMALL_VALS = (0.30, 0.62, 0.95, 1.0, 0.95, 0.62, 0.30)


def round_rect(x, y, x0, y0, x1, y1, r):
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def bars(n):
    if n <= 32:
        vals, step, half, start = SMALL_VALS, 0.09, 0.030, 0.23
    else:
        vals, step, half, start = VALS, 0.056, 0.015, 0.22
    out = []
    for i, v in enumerate(vals):
        cx = start + i * step
        h = 0.08 + 0.46 * v
        out.append((cx, half, 0.5 - h / 2, 0.5 + h / 2, i == len(vals) // 2))
    return out, start, step


def shade(x, y, n, geometry):
    if not round_rect(x, y, 0.0, 0.0, 1.0, 1.0, 0.22):
        return (0.0, 0.0, 0.0, 0.0)
    t = min(1.0, max(0.0, (x + y) / 2))
    c = tuple((BG_A[i] + (BG_B[i] - BG_A[i]) * t) / 255.0 for i in range(3))

    if n >= 32:
        outer = round_rect(x, y, 0.030, 0.030, 0.970, 0.970, 0.195)
        inner = round_rect(x, y, 0.044, 0.044, 0.956, 0.956, 0.185)
        if outer and not inner:
            c = tuple(EDGE[i] / 255.0 for i in range(3))

    u, v = min(x, 1 - x), min(y, 1 - y)
    m, length, width = (0.07, 0.24, 0.065) if n <= 32 else (0.10, 0.20, 0.036)
    if (m <= u <= m + length and m <= v <= m + width) or \
       (m <= u <= m + width and m <= v <= m + length):
        c = tuple(FRAME[i] / 255.0 for i in range(3))

    if n >= 32 and 0.22 <= x <= 0.78 and (0.175 <= y <= 0.186 or 0.814 <= y <= 0.825):
        c = tuple(0.6 * c[i] + 0.4 * (FRAME[i] / 255.0) for i in range(3))

    if 0.18 <= x <= 0.82 and 0.18 <= y <= 0.82:
        items, start, step = geometry
        centre = int(round((x - start) / step))
        for idx in (centre - 1, centre, centre + 1):
            if not 0 <= idx < len(items):
                continue
            cx, half, y0, y1, is_accent = items[idx]
            if round_rect(x, y, cx - half, y0, cx + half, y1, half):
                base = ACCENT if is_accent else WAVE
                c = tuple(base[i] / 255.0 for i in range(3))
                break
    return (c[0], c[1], c[2], 1.0)


def render(n, ss=2):
    geometry = bars(n)
    buf = bytearray(n * n * 4)
    for py in range(n):
        for px in range(n):
            ar = ag = ab = aa = 0.0
            for sy in range(ss):
                yy = (py + (sy + 0.5) / ss) / n
                for sx in range(ss):
                    xx = (px + (sx + 0.5) / ss) / n
                    r, g, b, a = shade(xx, yy, n, geometry)
                    ar += r * a
                    ag += g * a
                    ab += b * a
                    aa += a
            if aa > 0:
                r, g, b, a = ar / aa, ag / aa, ab / aa, aa / (ss * ss)
            else:
                r = g = b = a = 0.0
            pos = (py * n + px) * 4
            buf[pos] = int(min(1.0, r) * 255 + 0.5)
            buf[pos + 1] = int(min(1.0, g) * 255 + 0.5)
            buf[pos + 2] = int(min(1.0, b) * 255 + 0.5)
            buf[pos + 3] = int(min(1.0, a) * 255 + 0.5)
    return bytes(buf)


def png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def png_bytes(rgba, n, height=None):
    h = height or n
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * n * 4:(y + 1) * n * 4]
    ihdr = struct.pack(">IIBBBBB", n, h, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
            + png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + png_chunk(b"IEND", b""))


def contact_sheet(rendered, sizes, cell=112, gap=8, bg=(16, 24, 32)):
    """Nearest-neighbour enlargement of every size, for legibility review."""
    width = len(sizes) * cell + gap * (len(sizes) + 1)
    height = cell + gap * 2
    buf = bytearray(width * height * 4)
    for p in range(0, len(buf), 4):
        buf[p:p + 4] = bytes((bg[0], bg[1], bg[2], 255))
    for index, n in enumerate(sizes):
        src = rendered[n]
        ox = gap + index * (cell + gap)
        oy = gap
        for dy in range(cell):
            sy = min(n - 1, dy * n // cell)
            for dx in range(cell):
                sx = min(n - 1, dx * n // cell)
                s = (sy * n + sx) * 4
                d = ((oy + dy) * width + (ox + dx)) * 4
                a = src[s + 3] / 255.0
                for c in range(3):
                    buf[d + c] = int(src[s + c] * a + bg[c] * (1 - a) + 0.5)
                buf[d + 3] = 255
    return png_bytes(bytes(buf), width, height)


def dib_bytes(rgba, n):
    """32bpp bottom-up BGRA + AND mask, for classic ICO entries."""
    header = struct.pack("<IiiHHIIiiII", 40, n, 2 * n, 1, 32, 0, 0, 0, 0, 0, 0)
    xor = bytearray()
    for y in range(n - 1, -1, -1):
        for x in range(n):
            p = (y * n + x) * 4
            xor += bytes((rgba[p + 2], rgba[p + 1], rgba[p], rgba[p + 3]))
    row_bytes = ((n + 31) // 32) * 4
    mask = bytes(row_bytes * n)
    return header + bytes(xor) + mask


def ico_bytes(items):
    header = struct.pack("<HHH", 0, 1, len(items))
    entries = bytearray()
    body = bytearray()
    offset = 6 + 16 * len(items)
    for n, blob in items:
        w = 0 if n >= 256 else n
        h = 0 if n >= 256 else n
        entries += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(blob), offset)
        body += blob
        offset += len(blob)
    return header + bytes(entries) + bytes(body)


def verify_ico(data, expected):
    reserved, kind, count = struct.unpack("<HHH", data[:6])
    assert reserved == 0 and kind == 1 and count == len(expected)
    for i, (n, _) in enumerate(expected):
        w, h, colors, res, planes, bits, size, offset = struct.unpack(
            "<BBBBHHII", data[6 + 16 * i:22 + 16 * i])
        assert (w == 0 or w == n) and (h == 0 or h == n) and planes == 1 and bits == 32
        blob = data[offset:offset + size]
        assert len(blob) == size
        if n <= 48:
            bi_size, bi_w, bi_h, _, bi_bits = struct.unpack("<IiiHH", blob[:16])
            assert bi_size == 40 and bi_w == n and bi_h == 2 * n and bi_bits == 32
        else:
            assert blob[:8] == b"\x89PNG\r\n\x1a\n"
    return count


def main():
    rendered = {}
    for n in ICO_SIZES:
        rendered[n] = render(n, 3 if n <= 48 else 2)
        print("rendered", n)
    big = render(PREVIEW, 2)
    print("rendered", PREVIEW)

    preview = OUT / "daw-tools-512.png"
    preview.write_bytes(png_bytes(big, PREVIEW))
    print("preview", preview.name, preview.stat().st_size)
    sheet = OUT / "daw-tools-sizes.png"
    sheet.write_bytes(contact_sheet(rendered, ICO_SIZES))
    print("sheet", sheet.name, sheet.stat().st_size)

    items = []
    for n in ICO_SIZES:
        blob = dib_bytes(rendered[n], n) if n <= 48 else png_bytes(rendered[n], n)
        items.append((n, blob))
    ico = ico_bytes(items)
    target = OUT / "daw-tools.ico"
    target.write_bytes(ico)
    print("icon", target.name, len(ico), "bytes", verify_ico(ico, items), "entries")
    print("OK")


if __name__ == "__main__":
    main()
