"""Checks for the Stelle companion's art, fonts and palette.

- device frames: size, inside the cell, distinct, jump keeps its air gap
- sprite timing matches companion_model.c so clips end on their last frame
- generated fonts/rooms are up to date with tools/make_ui_assets.py
- every character the firmware can print exists in the pixel font
- palette: UI tokens are cool blue-violet greys like the reference, no saturated
  purple; gold stays the only accent. Rooms come from the user-supplied sheet.
"""
import colorsys
import hashlib
import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEVICE = ROOT / "assets/images/stelle-companion/device"
sys.path.insert(0, str(ROOT / "tools"))
from ui_assets.glyphs import GLYPHS  # noqa: E402
from ui_assets import rooms  # noqa: E402

# Frame counts come from the user-curated manifest, not from this test.
MANIFEST = json.loads((DEVICE / "sprites.json").read_text())
STATES = {s["name"]: len(s["frames"]) for s in MANIFEST["states"]}
GROUNDED = ("idle", "greeting", "focus", "bat_swing")
FEET_X, FEET_BOTTOM = 80, 153
SHADOW_RGB = (16, 17, 24)


def is_saturated_purple(rgb):
    """A purple accent (button, bar, glow) rather than a cool grey tint."""
    r, g, b = (c / 255 for c in rgb[:3])
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    return s > 0.35 and v > 0.25 and 0.70 <= h <= 0.94  # ~250..340 degrees


def is_cool(rgb):
    r, g, b = rgb[:3]
    return b > r and b > g


class FramesTest(unittest.TestCase):
    def frame(self, state, index):
        entry = next(s for s in MANIFEST["states"] if s["name"] == state)
        return Image.open(DEVICE / entry["frames"][index]).convert("RGBA")

    def shadow_bbox(self, state, index):
        frame = self.frame(state, index)
        mask = Image.new("L", frame.size, 0)
        mask.putdata([255 if ((state == "sleep" and i//160 >= 150 and max(pixel[:3]) < 80 and pixel[3] >= 40)
                            or (state != "sleep" and pixel[:3] == SHADOW_RGB and 40 <= pixel[3] <= 220)) else 0
                      for i,pixel in enumerate(frame.get_flattened_data())])
        return mask.getbbox()

    def character_bbox(self, frame):
        """Opaque pixels that are not the flat-coloured ground shadow."""
        mask = Image.new("L", frame.size, 0)
        mask.putdata([255 if pixel[3] >= 128 and pixel[:3] != SHADOW_RGB else 0
                      for pixel in frame.get_flattened_data()])
        return mask.getbbox()

    def feet(self, frame):
        """Centre and bottom of the soles (lowest four character rows)."""
        mask = [(i % 160, i // 160) for i, p in enumerate(frame.get_flattened_data())
                if p[3] >= 128 and p[:3] != SHADOW_RGB]
        bottom = max(y for _, y in mask)
        row = [x for x, y in mask if y >= bottom - 3]
        return (min(row) + max(row)) / 2, bottom

    def test_frames_inside_cells(self):
        for state, count in STATES.items():
            for i in range(count):
                f = self.frame(state, i)
                self.assertEqual(f.size, (160, 160), f"{state}/{i:02d}")
                left, top, right, bottom = f.getchannel("A").getbbox()
                self.assertTrue(left > 0 and top > 0 and right < 160 and bottom < 160, f"{state}/{i:02d}")

    def test_grounded_actions_keep_feet_in_place(self):
        # Every pose except the jump stands on the same spot: feet centred on the
        # shadow and on the same baseline, so weight does not drift between frames.
        for state in GROUNDED:
            for i in range(STATES[state]):
                x, bottom = self.feet(self.frame(state, i))
                self.assertLessEqual(abs(x - FEET_X), 1, f"{state}/{i:02d} feet x {x}")
                self.assertEqual(bottom, FEET_BOTTOM, f"{state}/{i:02d} feet bottom {bottom}")

    def test_bat_swing_uses_eight_current_sources_with_geometry_only(self):
        self.assertEqual(STATES["bat_swing"], 8)
        result = subprocess.run([sys.executable, str(ROOT / "tools/prepare_bat_frames.py"), "--check"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads((DEVICE / "prepared/bat_swing/conversion.json").read_text())
        self.assertEqual(report["operation"], "geometry only; no pose replacement or AI edits")
        for row in report["frames"]:
            source = DEVICE / "frames/bat_swing" / row["frame"]
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), row["source_sha256"])
            if row["source_size"] == [160, 160]:
                original = Image.open(source).convert("RGBA")
                expected = Image.new("RGBA", (160, 160))
                expected.paste(original, tuple(row["translation"]))
                self.assertEqual(self.frame("bat_swing", int(source.stem)).tobytes(), expected.tobytes())

    def test_jump_shadow_stays_put_and_shrinks_in_the_air(self):
        shadows = [self.shadow_bbox("celebration", i) for i in range(STATES["celebration"])]
        self.assertTrue(all(shadows))
        centres = {(s[0] + s[2]) / 2 for s in shadows}
        self.assertLessEqual(max(centres) - min(centres), 1)  # the shadow does not move
        bodies = [self.character_bbox(self.frame("celebration", i)) for i in range(STATES["celebration"])]
        apex = min(range(len(bodies)), key=lambda i: bodies[i][3])
        self.assertGreaterEqual(bodies[0][3] - bodies[apex][3], 16)  # visible air gap
        self.assertLess(shadows[apex][2] - shadows[apex][0], shadows[0][2] - shadows[0][0])

    def test_nothing_is_clipped_by_the_room(self):
        # The view places the shadow centre (UI_SPRITE_ANCHOR_*) on each scene's rug.
        theme = (ROOT / "main/ui_theme.h").read_text()
        anchor_y = int(re.search(r"UI_SPRITE_ANCHOR_Y (\d+)", theme).group(1))
        for scene, (_rug_x, rug_y) in rooms.RUG.items():
            for state, count in STATES.items():
                for index in range(count):
                    top = self.frame(state, index).getchannel("A").getbbox()[1]
                    self.assertGreaterEqual(rug_y - anchor_y + top, 0, f"{scene}: {state}/{index:02d} clipped")

    def test_every_frame_has_a_dark_grounding_shadow(self):
        for state, count in STATES.items():
            for index in range(count):
                shadow = self.shadow_bbox(state, index)
                self.assertIsNotNone(shadow, f"{state}/{index:02d} has no contact shadow")
                if state == "sleep":  # the box keeps its art's shadow along the base, not an ellipse
                    self.assertGreaterEqual(shadow[3], 150)
                    continue
                self.assertGreaterEqual(shadow[1], 144)
                self.assertLess(shadow[3], 160)
    def test_current_counts_and_requested_frame_durations(self):
        expected = {"idle": [2500]+[100]*4, "sleep": [3000]+[200]*7,
                    "greeting": [200]*5, "focus": [200]*9,
                    "celebration": [200]*5, "bat_swing": [100]*8}
        for state in MANIFEST["states"]:
            self.assertEqual(state["durations_ms"], expected[state["name"]])
            self.assertEqual(len(state["frames"]), len(expected[state["name"]]))
        idle = next(s for s in MANIFEST["states"] if s["name"] == "idle")
        self.assertEqual([Path(p).name for p in idle["frames"]], ["00.png","01.png","02.png","04.png","05.png"])

    def test_sleep_preparation_preserves_all_eight_current_sources(self):
        result = subprocess.run([sys.executable,str(ROOT / "tools/prepare_sleep_frames.py"),"--check"], capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        for i in range(8):
            self.assertEqual(self.frame("sleep",i).tobytes(), Image.open(DEVICE / "frames/sleep" / f"{i:02d}.png").convert("RGBA").tobytes())

    def test_sprite_cycles_match_companion_model(self):
        manifest = json.loads((DEVICE / "sprites.json").read_text())
        cycles = {s["name"]: sum(s["durations_ms"]) for s in manifest["states"]}
        source = (ROOT / "main/companion_model.c").read_text()
        for motion, state in (("GREETING", "greeting"), ("FOCUS", "focus"),
                              ("CELEBRATION", "celebration"), ("BAT_SWING", "bat_swing")):
            ms = int(re.search(rf"COMPANION_MOTION_{motion}\] = (\d+)", source).group(1))
            self.assertEqual(ms, cycles[state], motion)
        generated = (ROOT / "main/generated/pack/stelle_pack.c").read_text()
        for state, total in cycles.items():
            self.assertRegex(generated, rf"\{{\d+, {STATES[state]}, {total}U, {state}_ms\}}")


class PaletteAndFontTest(unittest.TestCase):
    def test_theme_tokens_are_cool_greys_plus_gold(self):
        theme = (ROOT / "main/ui_theme.h").read_text()
        tokens = dict(re.findall(r"#define (UI_\w+)\s+0x([0-9A-Fa-f]{6})", theme))
        self.assertGreaterEqual(len(tokens), 8)
        self.assertEqual(tokens["UI_GOLD"].upper(), "F5C15D")
        for name, hexv in tokens.items():
            rgb = tuple(int(hexv[i:i + 2], 16) for i in (0, 2, 4))
            self.assertFalse(is_saturated_purple(rgb), name)
            if name != "UI_GOLD":
                self.assertTrue(is_cool(rgb), f"{name} should lean blue-violet like the reference")

    def test_rooms_come_from_the_supplied_sheet(self):
        for phase in rooms.PHASES:
            img = rooms.paint(phase)
            self.assertEqual(img.size, (rooms.W, rooms.H))
        self.assertTrue(rooms.SOURCE.exists())

    def test_generated_assets_are_current(self):
        result = subprocess.run([sys.executable, str(ROOT / "tools/make_ui_assets.py"), "--check"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_every_printed_character_has_a_glyph(self):
        missing = set()
        for name in ("ui_copy.c", "pet_view.c", "app_model.c", "companion_model.c"):
            text = (ROOT / "main" / name).read_text(encoding="utf-8")
            for literal in re.findall(r'"((?:[^"\\\n]|\\.)*)"', text):
                if literal.endswith(".h") or literal.startswith("UI_ICON"):
                    continue
                raw = literal.encode("latin-1", "backslashreplace").decode("unicode_escape").encode("latin-1")
                try:
                    s = raw.decode("utf-8")
                except UnicodeDecodeError:
                    continue
                for ch in s:
                    if ch in "%\n\r" or ord(ch) >= 0xE000:
                        continue
                    if ch not in GLYPHS:
                        missing.add(ch)
        self.assertEqual(missing, set())


if __name__ == "__main__":
    unittest.main()
