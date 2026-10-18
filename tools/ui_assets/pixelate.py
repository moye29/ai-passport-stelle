"""Turn the user's Astral Express picture into a crisp pixel-art panel.

Source: assets/images/stelle-companion/source/astral-express.png (a photo of
a monitor). Steps: crop to the screen area (no bezel, build text or
watermark), centre-crop to the panel aspect, area-downscale once to the
native panel size, lift contrast/colour slightly, then reduce to a 48-colour
octree palette without dithering so every pixel is a flat, hard-edged colour.
"""
from pathlib import Path

from PIL import Image, ImageEnhance

SRC = Path(__file__).resolve().parents[2] / "assets/images/stelle-companion/source/astral-express.png"
# Screen area of the 800 x 600 photo: below the monitor's top edge, left of
# its right bezel, above the build text, bezel and watermark at the bottom.
CROP = (0, 26, 786, 526)
W, H = 216, 136     # device pixels: fills a 220 px wide card inside its 2 px frame
COLORS = 48


# Preferred source: the user's own pixel-art version of the same picture. It
# is already pixel art, so it is only fitted and scaled once (no colour boost).
PIXEL_SRC = SRC.with_name("astral-express-pixel.png")


def pixelate(src=None, crop=CROP, size=(W, H), colors=COLORS):
    if src is None and PIXEL_SRC.exists():
        with Image.open(PIXEL_SRC) as im:
            im = im.convert("RGB")
        ar = size[0] / size[1]
        w, h = im.size
        if w / h > ar:
            nw = round(h * ar)
            im = im.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
        else:
            nh = round(w / ar)
            im = im.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
        small = im.resize(size, Image.BOX)
        return small.quantize(colors=64, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).convert("RGB")
    with Image.open(src or SRC) as im:
        im = im.convert("RGB").crop(crop)
    ar = size[0] / size[1]
    w, h = im.size
    if w / h > ar:
        nw = round(h * ar)
        im = im.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
    else:
        nh = round(w / ar)
        im = im.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
    small = im.resize(size, Image.BOX)
    small = ImageEnhance.Contrast(small).enhance(1.2)
    small = ImageEnhance.Color(small).enhance(1.15)
    return small.quantize(colors=colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).convert("RGB")
