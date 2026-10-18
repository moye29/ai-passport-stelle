[简体中文](README.zh_CN.md) · [English](#english)

<a id="chinese"></a>

# AI Passport · 口袋搭档 · 星

为 **FoloToy AI Passport** 制作的非商业 同人角色陪伴应用，以《崩坏：星穹铁道》 开拓者「星」为原型。

## 界面预览

## 功能

1. **日常陪伴：** 默认眨眼、夜间睡眠待机动画，以及开机／唤醒屏幕时自动打招呼。
2. **Focus 专注：** 固定 25 分钟倒计时或正计时，支持暂停和保存专注记录。
3. **Days 日历：** 展示每日累计专注时长的日历，以及《崩坏：星穹铁道》开服纪念页，包含发车动画和芯片风格音乐。
4. **Chat 对话：** 通过小智AI进行非连续语音对话，本地保存最近 15 条文字聊天记录。需要连接 Wi-Fi 后在 [xiaozhi.me](https://xiaozhi.me/console/agents) 绑定设备。
5. **Play 互动：** 打招呼、加油、跳跃和挥棒动画（部分含角色语音）。
6. **Battle 战斗：** 类似解压木鱼，按 OK 键挥棒打击，怪物自动轮换，并保存累计打击次数。
7. **Settings 设置：** 亮度、音量、自动息屏时长、Wi-Fi，以及切换壁纸，支持 2 种按时间变化的固定场景动态壁纸或每日随机探索的星铁场景。

## 操作与首次使用

首次启动按照屏幕提示，用手机扫码连接热点配网；连接热点后如未自动弹出配网页，手动访问 [http://192.168.4.1](http://192.168.4.1) 可选择 Wi-Fi。联网校时和 Chat 需要网络。

主页面通过 UP／DOWN 可在 **Focus → Days → Chat → Play → Battle → Settings** 功能间切换，OK 进入。长按 OK 通常用于返回或停止当前互动，具体以屏幕提示为准。息屏后的第一次按键只负责唤醒。

首次开启 Chat 时，请用浏览器登录[小智控制台](https://xiaozhi.me/console/agents)，点击右上角 **添加设备**，输入设备屏幕中角色上方文字气泡显示的 **6 位数字**，完成设备绑定。

Chat 为非连续语音对话，使用外部小智服务，需要用户手动开启，按 OK 结束录音或取消播放回复。Chat 未开启或出错时，长按 OK 可查看本机最近 15 条聊天记录。

## 构建

适配 ESP32-C3、8 MB Flash、无 PSRAM、240 × 320 屏幕和三个实体按键。

使用 **ESP-IDF 5.5.3**，在激活的环境中准备 Python 和 Pillow。
完整主机检查另外需要 cryptography。
仓库包含处理后的素材和生成的 C 资源；只有重新处理音频时才需要 FFmpeg、NumPy 等工具。

```sh
python tools/make_asset_pack.py
python tools/prepare_trailblaze.py --check
python tools/make_trailblaze_pack.py
./tools/validate.sh --static
./tools/validate.sh --firmware
```

完整检查为 `./tools/validate.sh`。验证后的合并固件为
`build/FoloToy-AI-Passport-full.bin`，从 `0x0` 烧录。完整镜像烧录可能重置设置和记录。
本应用采用自定义分区布局，更新已有设备前请阅读
[固件布局与数据策略](docs/development/engineering/firmware-layout.zh_CN.md)。
预留 Recovery 分区不代表已附带恢复固件或支持自动回滚。

本项目仍在持续完善，构建、主机测试与实机验收需分别判断。
[应用说明](docs/stelle-companion.zh_CN.md)和[聊天说明](docs/stelle-chat.zh_CN.md)
记录了当前实现细节与资源边界。

## 致谢与素材说明

开拓者星是《崩坏：星穹铁道》中的角色，版权归米哈游所有。本项目为非官方同人作品，
与米哈游／HoYoverse 无隶属、赞助或官方背书关系。

- **人物、怪物与随机壁纸：** 基于游戏实机截图，经 AI 加工制作，并由作者手工补绘、修正细节。
- **固定场景壁纸：** 纯 AI 生成。
- **音频：** 包含采用开放许可的音效，以及作者在游戏内亲自录制的官方角色语音。
  Battle 命中音效来自 [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds)（CC0）。
  纪念页音乐为 HOYO-MiX《Star Rail》的芯片风格改编，参考
  [Tokiku](https://tokiku-music.github.io/music/star_rail)与
  [frostwing98](https://frostwing98.com/scores/)的编配。

AI 加工、手工修改和个人录制仅说明素材制作方式，不改变原游戏内容的权利归属。
© 米哈游版权所有。《崩坏：星穹铁道》素材的权利归米哈游所有，
其他内容的相关权利、利益归各自权利人所有。

项目源代码采用 [MIT 许可](LICENSE)，并保留第三方声明；该许可**不涵盖**
游戏角色、图片、动画、音乐与语音等素材。包含这些素材的版本仅供个人非商业同人交流，
请勿售卖、捆绑付费产品或作为独立素材包再分发。使用时须遵守相关权利人的条款与
同人创作指引，本仓库不授予第三方素材的额外使用权。

字体与其他依赖仍遵循各自许可，详见[素材说明](assets/README.zh_CN.md)、
[字体许可](assets/fonts/stelle-chat/LICENSE.txt)及
[小智版权声明](main/xiaozhi-reference.LICENSE)。
如权利人对收录内容有异议，请通过 GitHub Issues 联系维护者核实处理。
欢迎分享有趣的玩法与改作，分享时请保留许可、致谢和素材说明。

开发 Skill 参考：[宁芙_nymph（小红书）](https://www.xiaohongshu.com/user/profile/58e726796a6a69071fc77a76)。

项目使用 [FoloToy AI Passport](https://github.com/FoloToy/ai-passport) 的基础框架与硬件驱动，相关代码保留原有 MIT 许可与版权声明。

---

<a id="english"></a>

[简体中文](#chinese) · **English**

# AI Passport · Pocket Companion · Stelle

A non-commercial fan-made character companion featuring Stelle, the Trailblazer
from *Honkai: Star Rail*, made for **FoloToy AI Passport**.

## UI preview

## What it does

1. **Everyday companionship:** blinking, nighttime sleeping idle animation, and automatic greetings on startup or screen wake.
2. **Focus:** a fixed 25-minute countdown or a count-up timer, with pausing and saved focus records.
3. **Days:** a calendar showing each day's accumulated focus time, plus a *Honkai: Star Rail* launch anniversary page with a departing-train animation and chip-style music.
4. **Chat:** non-continuous voice exchanges through XiaoZhi AI, saving the latest 15 text chat entries locally. Connect to Wi-Fi and bind the device at [xiaozhi.me](https://xiaozhi.me/console/agents).
5. **Play:** greeting, encouragement, jump and bat-swing animations, some with character voice clips.
6. **Battle:** a relaxing tap-to-hit interaction: press OK to swing the bat, cycle through enemies automatically and save the total hit count.
7. **Settings:** brightness, volume, screen-off timeout, Wi-Fi and wallpaper selection, with two fixed scenes that change with the time of day or a random *Honkai: Star Rail* exploration scene each day.

## Controls and first use

On first boot, follow the screen prompts and scan the QR code with your phone to join the setup hotspot. If the configuration page does not open automatically, visit [http://192.168.4.1](http://192.168.4.1) after joining the hotspot to select a Wi-Fi network. Time synchronization and Chat require a network connection.

On the home screen, UP / DOWN select **Focus → Days → Chat → Play → Battle → Settings**, and OK opens the selected feature. Holding OK generally returns or stops the current interaction; follow the on-screen hints. The first press after screen-off only wakes the display.

When you first open Chat, sign in to the [XiaoZhi console](https://xiaozhi.me/console/agents) in a browser. Click **Add Device** in the upper-right corner, then enter the **6-digit code** shown in the text bubble above the character on the device screen to complete binding.

Chat uses the external XiaoZhi service for non-continuous voice exchanges and must be started manually. Press OK to finish recording or cancel reply playback. When Chat is inactive or in an error state, hold OK to view the latest 15 locally saved text entries.

## Build

Built for the ESP32-C3, 8 MB Flash, no PSRAM, a 240 × 320 display and three buttons.

Use **ESP-IDF 5.5.3**, with Python and Pillow available in the activated environment.
The complete host checks also require cryptography.
The repository includes prepared assets and generated C resources; FFmpeg and
NumPy are needed only for the optional audio-preparation tools.

```sh
python tools/make_asset_pack.py
python tools/prepare_trailblaze.py --check
python tools/make_trailblaze_pack.py
./tools/validate.sh --static
./tools/validate.sh --firmware
```

The complete check is `./tools/validate.sh`. The verified merged image is
`build/FoloToy-AI-Passport-full.bin`, flashed at `0x0`. A full-image flash can
reset settings and records. This application has a custom partition layout;
use the [firmware layout and data policy](docs/development/engineering/firmware-layout.md)
when updating an existing device. A reserved Recovery partition is not a promise
of a bundled recovery image or automatic rollback.

This is a community project under development. Build, host-test and physical
device results must be assessed separately; see the
[application notes](docs/stelle-companion.md) and [Chat notes](docs/stelle-chat.md)
for current implementation details and resource limits.

## Credits and material use

Stelle, the Trailblazer, is a character from *Honkai: Star Rail*, with copyright
belonging to miHoYo. This is an unofficial fan project, with no affiliation,
sponsorship or endorsement from miHoYo / HoYoverse.

- **Characters, enemies and random wallpapers:** based on in-game screenshots
  processed with AI,
  then manually redrawn and corrected by the author.
- **Fixed-scene wallpapers:** entirely AI-generated.
- **Audio:** includes open-license effects and official character voice clips
  personally recorded in-game by the author. Battle's impact sound is from
  [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) (CC0).
  The anniversary music is a chip-style adaptation of HOYO-MiX's *Star Rail*,
  referencing arrangements by [Tokiku](https://tokiku-music.github.io/music/star_rail)
  and [frostwing98](https://frostwing98.com/scores/).

AI processing, manual edits and personal recording describe how the materials
were made; they do not transfer rights in the underlying game content.
© miHoYo. Rights in *Honkai: Star Rail* materials belong to miHoYo;
rights in other materials belong to their respective owners.

The source code is provided under [MIT](LICENSE), subject to retained third-party
notices. This does **not** license game characters, artwork, music or voice assets
under MIT. Versions containing these materials are shared for personal,
non-commercial fan use; do not sell them, bundle them with paid products or
redistribute the materials as a standalone asset pack. Follow the applicable
rights holders' terms and fan-creation guidelines. No additional rights to
third-party materials are granted by this repository.

Font and other dependency licenses remain applicable; see
[asset credits](assets/README.md), [font license](assets/fonts/stelle-chat/LICENSE.txt)
and [XiaoZhi notice](main/xiaozhi-reference.LICENSE).
If you are a rights holder with a concern, please contact the maintainer through
GitHub Issues. Contributions and interesting remixes are welcome; retain the
license, credits and material-use notices when sharing.

Development skill references: [nymph (Xiaohongshu)](https://www.xiaohongshu.com/user/profile/58e726796a6a69071fc77a76).

The project uses the base framework and board drivers from
[FoloToy AI Passport](https://github.com/FoloToy/ai-passport), retaining their
original MIT license and copyright notices.
