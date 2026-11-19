"""DAW Tools icon build - derives app.ico from the 1024 master app.png.

The artwork is the 2026-11-11 fader design published in the software
portfolio (icons/daw.png); app.png here is that master, unchanged. Every ICO
entry is resampled from the master, so 16-32px stay legible.

Outputs (next to this file):
  app.ico        multi-size Windows icon: 16/24/32/48/64/128/256
  app-sizes.png  size sheet, small entries magnified for legibility checks

Run: python assets/icons/generate_icon.py
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
ZOOM = {16: 4, 24: 4, 32: 4, 48: 2, 64: 2, 128: 1, 256: 1}
SHEET_BG = (24, 30, 38)
LABEL = (200, 210, 220)


def build_ico(master: Image.Image) -> None:
    master.save(OUT / "app.ico", format="ICO", sizes=[(s, s) for s in ICO_SIZES])


def build_size_sheet(master: Image.Image) -> None:
    pad, top = 10, 26
    items = []
    for size in ICO_SIZES:
        frame = master.resize((size, size), Image.LANCZOS)
        shown = frame.resize((size * ZOOM[size], size * ZOOM[size]), Image.NEAREST)
        items.append((size, shown))
    width = sum(shown.width + pad for _, shown in items) + pad
    height = top + max(shown.height for _, shown in items) + pad
    sheet = Image.new("RGB", (width, height), SHEET_BG)
    draw = ImageDraw.Draw(sheet)
    x = pad
    for size, shown in items:
        draw.text((x, 8), f"{size}px", fill=LABEL)
        sheet.paste(shown, (x, height - pad - shown.height), shown)
        x += shown.width + pad
    sheet.save(OUT / "app-sizes.png")


def main() -> None:
    master = Image.open(OUT / "app.png").convert("RGBA")
    if master.width != master.height or master.width < 256:
        raise SystemExit(f"app.png must be a square master of at least 256px, got {master.size}")
    build_ico(master)
    build_size_sheet(master)
    print(f"app.ico: {len(ICO_SIZES)} entries from {master.width}px master")
    print("app-sizes.png written")


if __name__ == "__main__":
    main()
