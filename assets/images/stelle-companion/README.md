[简体中文](README.zh_CN.md) · **English**

# Character and scene assets

The current firmware uses `device/sprites.json` as the frame/timing index.
`device/frames/` holds active source frames; `device/prepared/` holds the bat and
sleep conversion outputs. Keep these inputs when regenerating packs.

`source/` contains only the selected background, anniversary, scene-picker,
random-scene and enemy inputs. `device/anniversary/` is the native eight-frame
animation; `device/battle/` and `device/trailblaze/` contain prepared resources.
See the [asset guide](../../README.md) for commands and the
[material notice](../../../README.md#credits-and-material-use) for rights.
