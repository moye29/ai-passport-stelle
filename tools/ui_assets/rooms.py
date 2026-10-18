"""Room backgrounds (one per time of day) from the user-supplied sheets.

Two scenes, each a 2 x 2 sheet (day, dusk / night, late night) separated by
thin dark dividers:
  room   assets/images/stelle-companion/source/room-backgrounds.webp
  train  assets/images/stelle-companion/source/train-backgrounds.webp
Each panel is centre-cropped to the room's aspect ratio and scaled once, with
nearest-neighbour sampling, to the native room size. No other processing.

RUG gives the centre of each scene's rug in room pixels (measured from the
gold rug ring by tools/make_ui_assets.py --measure); Stelle's ground shadow is
placed there.
"""
from pathlib import Path

from PIL import Image

W, H = 216, 146
PHASES = ("day", "dusk", "night", "late")
SCENES = ("room", "train")

_SRC = Path(__file__).resolve().parents[2] / "assets/images/stelle-companion/source"
SOURCES = {
    "room": _SRC / "room-backgrounds.webp",
    "train": _SRC / "train-backgrounds.webp",
}
# Panel rectangles in each 1536 x 1024 sheet, inside the dividers.
PANELS = {
    "room": {
        "day": (0, 0, 764, 503), "dusk": (772, 0, 1536, 503),
        "night": (0, 513, 764, 1024), "late": (772, 513, 1536, 1024),
    },
    "train": {
        "day": (0, 0, 762, 508), "dusk": (772, 0, 1536, 508),
        "night": (0, 519, 762, 1024), "late": (772, 519, 1536, 1024),
    },
}
# Rug centre (x, y) in room pixels, per scene (same in all four phases),
# read from the rendered panels; measure_rug() is a helper for that check.
RUG = {"room": (114, 132), "train": (109, 133)}

# Backwards-compatible alias for the first scene.
SOURCE = SOURCES["room"]


def _crop_to_room(panel):
    x0, y0, x1, y1 = panel
    pw, ph = x1 - x0, y1 - y0
    cw = min(pw, round(ph * W / H))
    ch = min(ph, round(cw * H / W))
    left = x0 + (pw - cw) // 2
    top = y0 + (ph - ch) // 2
    return left, top, left + cw, top + ch


def paint(phase, scene="room"):
    with Image.open(SOURCES[scene]) as sheet:
        sheet = sheet.convert("RGB")
        if sheet.size != (1536, 1024):
            raise ValueError(f"unexpected {scene} sheet size {sheet.size}")
        return sheet.crop(_crop_to_room(PANELS[scene][phase])).resize((W, H), Image.NEAREST)


def measure_rug(img):
    """Centre of the gold rug ring: extremes of warm-gold pixels low in the room."""
    px = img.load()
    xs, ys = [], []
    for y in range(int(H * 0.72), H):
        for x in range(10, 196):
            r, g, b = px[x, y]
            if r > 170 and g > 120 and b < 120 and r - b > 80:
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return (min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2, (min(xs), min(ys), max(xs), max(ys))
