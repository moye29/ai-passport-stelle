[简体中文](README.zh_CN.md) · **English**

# Active assets and conversion tools

This directory contains the inputs used by the current firmware, their prepared
outputs and license notices. Reference-only artwork, discarded sprite pipelines,
audio auditions and image-generation prompts are not part of the published source.

## Material rights

Characters, enemies and random wallpapers use in-game screenshots processed with
AI and manual drawing/detail corrections. Fixed-scene wallpapers are AI-generated.
Official character voice clips were recorded in-game. Processing and recording do
not transfer underlying rights. See the [project material notice](../README.md#credits-and-material-use).

The hit effect uses [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds)
(CC0); keep `music/stelle-battle-candidates/source/License.txt` with it.
The anniversary music adapts HOYO-MiX's *Star Rail*, referencing arrangements by
[Tokiku](https://tokiku-music.github.io/music/star_rail) and
[frostwing98](https://frostwing98.com/scores/). Source URLs and credits are recorded
in `music/stelle-anniversary/scores/sources.json`. The game-derived material is
outside the code's MIT license. Fonts retain their own [license](fonts/stelle-chat/LICENSE.txt).

## Active input map

| Inputs | Use and tool |
| --- | --- |
| `images/stelle-companion/device/sprites.json`, `device/frames/`, `device/prepared/` | Active character frames and timings; `tools/make_asset_pack.py` |
| `images/stelle-companion/source/room-backgrounds.webp`, `train-backgrounds.webp` | Four time-of-day panels for each fixed scene; `tools/ui_assets/rooms.py` |
| `images/stelle-companion/source/astral-express-pixel.png` | Static anniversary image; `tools/ui_assets/pixelate.py` |
| `images/stelle-companion/source/trailblaze-preview.png` | Scene-picker artwork; `tools/make_asset_pack.py` |
| `images/stelle-companion/source/trailblaze/`, `device/trailblaze/` | Twelve random scenes, source order and native copies; `tools/prepare_trailblaze.py`, `tools/make_trailblaze_pack.py` |
| `images/stelle-companion/source/battle/`, `device/battle/` | Selected enemy/hit frames and cue; `tools/prepare_battle.py`, `tools/prepare_enemy_b.py` |
| `images/stelle-companion/device/anniversary/` | Eight animation frames; `tools/prepare_anniv_frames.py` |
| `music/stelle-play-sounds/` | Three voice clips and their ordered playback pack; `tools/prepare_play_sounds.py` |
| `music/stelle-anniversary/selected/` | Current music pack, note/format manifest and decoding/fade regression inputs |
| `fonts/stelle-chat/` | Font source, generated subset, coverage and license; `tools/make_chat_font.py` |
| `certificates/esp-idf-5.5.3/` | Public trust roots used by `tools/make_chat_ca.py` |

The active sprite manifest is authoritative. Animation source sizes may differ;
prepared cells are 160 × 160. The large character frames in `device/frames/bat_swing/`
are conversion inputs, not spare artwork. Retain both inputs and prepared frames
when rebuilding or running the geometry regressions.

## Rebuilding resources

For an ordinary firmware build, use the prepared inputs already in the repository:

```sh
python tools/make_asset_pack.py
python tools/prepare_trailblaze.py --check
python tools/make_trailblaze_pack.py
```

After changing source art, run the corresponding preparation tool and its `--check`
mode before rebuilding the resource images. The selected hit cue can be reused
without encoding with `tools/prepare_battle.py --hit-pack
assets/music/stelle-battle-candidates/A-punch.sbgm`. Voice/audio conversion requires
an explicit FFmpeg path. Music synthesis additionally uses NumPy.

Room/font/icon previews generated under `images/stelle-companion/qa/` are local
review outputs and are not committed. Upstream product illustrations at the root
of `images/` support the retained hardware documentation and keep their original notices.
