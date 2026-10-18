#!/usr/bin/env python3
"""Compose native preview screens into review sheets (nearest-neighbour only).

Applies the BSP's 30 px rounded-corner mask (pixels outside are black on the
device) so the review matches what the panel shows. Usage:
  review_sheet.py <screens-dir> <out.png> [scale] [index ...]
"""
import math
import sys
from pathlib import Path

from PIL import Image

RADIUS = 0  # matches BSP_LVGL_SCREEN_RADIUS set by the app (square frame)


def mask(img):
    w, h = img.size
    px = img.load()
    for y in range(h):
        for x in range(w):
            cx = RADIUS if x < RADIUS else w - 1 - RADIUS if x >= w - RADIUS else None
            cy = RADIUS if y < RADIUS else h - 1 - RADIUS if y >= h - RADIUS else None
            if cx is not None and cy is not None and math.hypot(x - cx, y - cy) > RADIUS:
                px[x, y] = (0, 0, 0)
    return img


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    files = sorted(src.glob("screen-*.png")) or sorted(src.glob("scene-*.ppm"))
    if len(sys.argv) > 4:
        files = [files[int(i)] for i in sys.argv[4:]]
    shots = [mask(Image.open(f).convert("RGB")) for f in files]
    cols = min(4, len(shots))
    rows = (len(shots) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (240 + 8), rows * (320 + 8)), "#9A9CA1")
    for i, s in enumerate(shots):
        sheet.paste(s, ((i % cols) * 248, (i // cols) * 328))
    sheet = sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST)
    sheet.save(out)


if __name__ == "__main__":
    main()
