#!/usr/bin/env python3
"""Pack 12 unchanged, native-size destinations as directly mapped RGB565.

STLT header: magic, u32 version/size/count/payload CRC/width/height/reserved.
All words are little endian; 12 fixed-size pixel planes follow at byte 32.
"""
import argparse
import struct
import zlib
from pathlib import Path

from PIL import Image

from make_asset_pack import room_bytes

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/stelle-companion/device/trailblaze"
OUTPUT = ROOT / "build/assets/trailblaze.bin"


def build(source=SOURCE):
    expected = [source / f"{i:02}.png" for i in range(12)]
    missing = [p.name for p in expected if not p.is_file()]
    if missing:
        raise ValueError(f"Place 216x146 backgrounds in {source}; missing: {', '.join(missing)}")
    payload = bytearray()
    for path in expected:
        with Image.open(path) as image:
            if image.size != (216, 146):
                raise ValueError(f"{path.name}: expected 216x146, got {image.width}x{image.height}; no automatic cropping")
            if "A" in image.getbands() and image.getchannel("A").getextrema()[0] < 255:
                raise ValueError(f"{path.name}: background must be opaque")
            payload.extend(room_bytes(image))
    header = b"STLT" + struct.pack("<7I", 1, 32 + len(payload), 12, zlib.crc32(payload), 216, 146, 0)
    return header + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        blob = build()
    except ValueError as error:
        parser.exit(1, f"{error}\n")
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_bytes() != blob:
            parser.exit(1, "Trailblaze pack is missing or stale\n")
    else:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_bytes(blob)
    print(f"Trailblaze: 12 backgrounds, {len(blob)} bytes, {0xC0000 - len(blob)} bytes free")


if __name__ == "__main__":
    main()
