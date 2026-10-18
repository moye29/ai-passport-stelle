#!/usr/bin/env python3
"""Side-by-side review: reference image (box-scaled to 240 x 320, for viewing
only) next to a native preview screen with the BSP corner mask applied.
Usage: compare_reference.py <reference> <screen.png|ppm> <out.png> [scale]
"""
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_sheet import mask  # noqa: E402

ref = Image.open(sys.argv[1]).convert("RGB").resize((240, 320), Image.BOX)
shot = mask(Image.open(sys.argv[2]).convert("RGB"))
scale = int(sys.argv[4]) if len(sys.argv) > 4 else 2
sheet = Image.new("RGB", (240 * 2 + 8, 320), "#9A9CA1")
sheet.paste(mask(ref), (0, 0))
sheet.paste(shot, (248, 0))
sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST).save(sys.argv[3])
