**简体中文** · [English](README.md)

# 当前素材与转换工具

此目录保留当前固件的有效输入、处理结果和许可声明。
仅供参考的图片、已废弃的人物帧流程、音频试听和图像生成提示词不属于公开源码。

## 素材权利

人物、怪物与随机壁纸以游戏实机截图为基础，结合 AI 加工、手工补绘与细节修正。
固定场景壁纸为纯 AI 生成，官方角色语音由游戏内录制取得。
加工与录制不改变原内容权利，详见[项目素材声明](../README.zh_CN.md#致谢与素材说明)。

打击音效使用 [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds)（CC0），
保留 `music/stelle-battle-candidates/source/License.txt`。
纪念页音乐改编自 HOYO-MiX《Star Rail》，参考
[Tokiku](https://tokiku-music.github.io/music/star_rail)和
[frostwing98](https://frostwing98.com/scores/)的编配，
来源与致谢保存在 `music/stelle-anniversary/scores/sources.json`。
游戏衍生素材不属于代码 MIT 许可；字体保留其[独立许可](fonts/stelle-chat/LICENSE.txt)。

## 有效输入

| 输入 | 用途与工具 |
| --- | --- |
| `images/stelle-companion/device/sprites.json`、`device/frames/`、`device/prepared/` | 当前人物帧与时长；`tools/make_asset_pack.py` |
| `images/stelle-companion/source/room-backgrounds.webp`、`train-backgrounds.webp` | 两个固定场景各四个时段的背景；`tools/ui_assets/rooms.py` |
| `images/stelle-companion/source/astral-express-pixel.png` | 纪念页静态图；`tools/ui_assets/pixelate.py` |
| `images/stelle-companion/source/trailblaze-preview.png` | 场景选择插画；`tools/make_asset_pack.py` |
| `images/stelle-companion/source/trailblaze/`、`device/trailblaze/` | 十二张随机场景、顺序与设备图；`tools/prepare_trailblaze.py`、`tools/make_trailblaze_pack.py` |
| `images/stelle-companion/source/battle/`、`device/battle/` | 选用的怪物／命中图片与音效；`tools/prepare_battle.py`、`tools/prepare_enemy_b.py` |
| `images/stelle-companion/device/anniversary/` | 八帧纪念动画；`tools/prepare_anniv_frames.py` |
| `music/stelle-play-sounds/` | 三段语音与顺序播放包；`tools/prepare_play_sounds.py` |
| `music/stelle-anniversary/selected/` | 当前音乐包、音符／格式清单及解码／淡出回归输入 |
| `fonts/stelle-chat/` | 字体源文件、生成子集、覆盖表与许可；`tools/make_chat_font.py` |
| `certificates/esp-idf-5.5.3/` | `tools/make_chat_ca.py` 使用的公开信任根证书 |

以当前精灵清单为准。动作源图尺寸可能不同，处理后的单元格为 160 × 160。
`device/frames/bat_swing/` 中的大图是转换输入，不是备用参考图；
重建素材或执行几何回归时，应同时保留输入和处理后的帧。

## 重建资源

普通构建使用仓库中已处理的输入：

```sh
python tools/make_asset_pack.py
python tools/prepare_trailblaze.py --check
python tools/make_trailblaze_pack.py
```

修改原图后，先执行对应准备工具及其 `--check` 模式，再重建资源镜像。
选定打击音效可通过 `tools/prepare_battle.py --hit-pack
assets/music/stelle-battle-candidates/A-punch.sbgm` 直接复用，不重新编码。
语音／音频转换需要显式 FFmpeg 路径，音乐合成另外需要 NumPy。

`images/stelle-companion/qa/` 下的房间、字体与图标预览为本地检查产物，不提交。
`images/` 根目录的上游产品插图用于保留的硬件文档，维持原有声明。
