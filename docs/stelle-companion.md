[简体中文](stelle-companion.zh_CN.md) · **English**

# Companion application

A three-button character companion for FoloToy AI Passport. No knowledge of the
source game's story or mechanics is needed to use or develop the application.
The project overview and material notices are in the [README](../README.md#english).

## Screens and controls

The home navigation is **Focus / Days / Chat / Play / Battle / Settings**.
UP and DOWN select a feature; OK opens it. A hold is 500 ms. The first key after
screen-off only wakes the display. Settings and history are separate detail views.

| Feature | Behavior |
| --- | --- |
| Focus | Fixed 25-minute countdown or count-up, pause/resume, stop/reset confirmation |
| Days | Daily accumulated focus time, completion count and launch-anniversary view |
| Chat | One manually started voice exchange; local text history; see [Chat](stelle-chat.md) |
| Play | Greeting, encouragement, jump and bat-swing animations; selected actions have audio |
| Battle | Press OK to hit, rotate enemies and save the total hit count |
| Settings | Brightness, volume, timeout, Wi-Fi and scene selection |

**Path Energy** is the UI label for the cumulative hit counter, not a separate
game system. **Trailblaze** is the daily random-scene mode. Room and Train are
fixed scenes with day/dusk/night/late variants. Random scenes change once per
local day after time synchronization; applying that scene mode selects a scene.

The companion idles normally and uses a sleeping idle before 08:00 when local
time is known. Play, Chat and Battle keep the character standing. Startup and
screen wake show a one-second greeting on character-display pages; deliberate
input takes priority. A completed focus session plays its celebration animation.

Battle starts in an encounter-ready state. OK reveals an enemy, and the next OK
records one hit. Hit and recovery each last 250 ms; their input is discarded.
After recovery the enemy rotates automatically. UP resets the count outside the
hit stage; holding OK exits. Storage failure rolls back the count update and
allows retry. The count saturates at the maximum unsigned 32-bit value.

## Saved data and time

Preferences, daily focus records and the hit counter use NVS. The focus log is a
366-slot daily ring. Session events and periodic checkpoints save focused time;
only saved data is guaranteed to survive a power loss. An interrupted timer is
not restored as a running session. Storage errors do not trigger automatic erase.

The hardware has no verified battery-backed real-time clock. Boot and periodic
network synchronization establish local time; unknown time displays `--:--`.
Wi-Fi setup uses the device-displayed access point and a local web form. Saved
network credentials are not written to logs. Chat history has its own partition.

Screen-off turns off the backlight; it is not MCU deep sleep. Charging indication
is inferred from USB/battery measurements and is not a dedicated charge-status pin.

## Code organization

| Path | Responsibility |
| --- | --- |
| `main/app_model.*` | Navigation, focus, scene selection, hit counter and input state |
| `main/companion_model.*` | Animation state and duration |
| `main/pet_view.c`, `main/ui_*.c` | LVGL views, text and visual tokens |
| `main/app_store.*`, `main/focus_log.*` | Preferences and daily-record persistence |
| `main/net.*`, `main/net_form.*` | Wi-Fi setup and clock synchronization |
| `main/chat_*.c` | Chat transport, bounded history, codec memory and storage |
| `components/bsp/` | Board display, buttons, audio, battery and shared buses |

LVGL work outside its task must hold `bsp_lvgl_lock()`. Button callbacks enqueue
work and do not block. Audio and network operations run outside UI callbacks.
Close tasks, timers and callbacks before deleting their UI objects.

## Assets and building

See the [asset guide](../assets/README.md) for the active inputs and reproducible
tools. `device/sprites.json` defines the current frame paths and durations; use
that manifest when replacing a character. Keep existing dimensions, transparency,
resource formats and partition limits unless deliberately changing the format.

Use ESP-IDF 5.5.3, Python and Pillow. Generate both resource images before a build:

```sh
python tools/make_asset_pack.py
python tools/prepare_trailblaze.py --check
python tools/make_trailblaze_pack.py
./tools/validate.sh --static
./tools/validate.sh --firmware
```

The [partition table](../partitions.csv) reserves separate application, identity,
artwork and chat-history regions. Follow the [flash/data policy](development/engineering/firmware-layout.md)
when updating a device. A full merged flash can overwrite stored data. A reserved
Recovery region does not itself supply a recovery image or automatic rollback.

## Validation boundaries

Host tests cover state transitions, current asset geometry/decoding, storage
recovery, bounded history and protocol parsing. The native preview uses the real
model and view with synthetic status values. Neither proves hardware acceptance.
Check actual display, buttons, audio, network binding, repeated chat exchanges,
screen wake and power-loss behavior on a device before claiming a stable release.
