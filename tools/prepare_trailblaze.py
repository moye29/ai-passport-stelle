#!/usr/bin/env python3
"""Prepare centre-cropped scene inputs without modifying source artwork."""
import argparse
import hashlib
import io
import json
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/stelle-companion/source/trailblaze"
DEVICE = ROOT / "assets/images/stelle-companion/device/trailblaze"


def outputs(source=SOURCE):
    inventory = source / "scenes.json"
    if inventory.is_file():
        names = json.loads(inventory.read_text(encoding="utf-8"))
        if not isinstance(names, list) or len(names) != 12 or any(
                not isinstance(name, str) or Path(name).name != name or Path(name).suffix.lower() != ".png"
                for name in names) or len(set(names)) != 12:
            raise ValueError("scenes.json must list twelve unique PNG filenames in device-index order")
        files = [source / name for name in names]
        missing = [path.name for path in files if not path.is_file()]
        if missing:
            raise ValueError(f"Missing listed backgrounds: {', '.join(missing)}")
    else:
        files = sorted(source.glob("*.png"), key=lambda path: path.name.casefold())
    if len(files) != 12:
        raise ValueError(f"Expected 12 source PNGs in {source}, found {len(files)}")
    images, records = {}, []
    for index, path in enumerate(files):
        original = path.read_bytes()
        with Image.open(io.BytesIO(original)) as image:
            # Backgrounds are opaque RGB565; retain source RGB colour at any
            # partly transparent edge pixel rather than introducing black edges.
            fitted = ImageOps.fit(image.convert("RGB"), (216, 146),
                                  method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
            name = f"{index:02}.png"
            buffer = io.BytesIO()
            fitted.save(buffer, format="PNG")
            images[name] = buffer.getvalue()
            records.append({"index": index, "source": path.name, "source_size": list(image.size),
                            "source_sha256": hashlib.sha256(original).hexdigest(), "device": name,
                            "device_sha256": hashlib.sha256(images[name]).hexdigest()})
    manifest = {"size": [216, 146], "conversion": "RGB, LANCZOS, centred aspect-preserving crop",
                "images": records}
    images["manifest.json"] = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    return images


def check(source=SOURCE, device=DEVICE):
    expected = outputs(source)
    manifest = json.loads(expected["manifest.json"])
    stale = []
    for entry in manifest["images"]:
        name = entry["device"]
        path = device / name
        try:
            actual = path.read_bytes()
            with Image.open(io.BytesIO(actual)) as image, Image.open(io.BytesIO(expected[name])) as reference:
                if image.mode != "RGB" or image.size != reference.size or image.tobytes() != reference.tobytes():
                    stale.append(name)
            # PNG compression may differ across platforms. The manifest must
            # still bind the exact committed file, while pixels match the source.
            entry["device_sha256"] = hashlib.sha256(actual).hexdigest()
        except (OSError, ValueError):
            stale.append(name)
    try:
        if json.loads((device / "manifest.json").read_text(encoding="utf-8")) != manifest:
            stale.append("manifest.json")
    except (OSError, ValueError):
        stale.append("manifest.json")
    if stale:
        raise ValueError(f"Stale Trailblaze device art: {', '.join(stale)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        if args.check:
            check()
        else:
            expected = outputs()
            DEVICE.mkdir(parents=True, exist_ok=True)
            for name, data in expected.items():
                (DEVICE / name).write_bytes(data)
    except ValueError as error:
        parser.exit(1, f"{error}\n")
    print("Trailblaze: 12 centred 216x146 device images; original source files preserved")


if __name__ == "__main__":
    main()
