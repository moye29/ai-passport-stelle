[简体中文](stelle-chat.zh_CN.md) · **English**

# Voice chat integration

The Chat feature performs one manually started voice exchange through the
external XiaoZhi AI service. It requires Wi-Fi and an account/device binding at
[xiaozhi.me](https://xiaozhi.me/). No wake word, always-listening mode or automatic
reply loop is enabled. This guide describes the protocol and resource limits,
independently of the character's fictional setting.

## Controls

| State | Action | Result |
| --- | --- | --- |
| Chat selected or inactive | OK | Connect, then start recording |
| Recording | OK | Finish recording and wait for the reply |
| Connecting, binding, waiting or playing | OK | Cancel the current exchange |
| Active chat | Hold OK | Stop chat |
| Inactive or error | Hold OK | Open local text history |
| Chat card | UP / DOWN | Return to navigation and stop chat |
| History | UP / DOWN | Scroll one text line |
| History | OK / Hold DOWN | Jump to the latest entry |
| History | Hold OK | Return to inactive Chat |

The microphone runs only after explicit activation and channel readiness.
Recording and reply playback each have a two-minute limit. Focus completion
cancels Chat so its notification can play. A screen-wake press only wakes the screen.

## Transport and task ownership

Configuration is obtained from `https://api.tenclass.net/xiaozhi/ota/`.
The integration implements activation v1, MQTT session control, AES-CTR UDP
voice packets and 60 ms Opus frames on ESP-IDF 5.5.3. Configuration discovery does
not install firmware. Remote tools and automatic firmware updates are disabled.
The protocol reference is [FoloToy's XiaoZhi implementation](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/72da544d1b51678f5d88967adc299b3aec956946);
its retained license is [here](../main/xiaozhi-reference.LICENSE).

`chat_model` owns state transitions; `chat_service` owns HTTPS/MQTT/UDP and codec
objects. Network callbacks post bounded messages and do not touch LVGL. Chat
and other audio share a mutex. TLS verifies peers using trusted certificates;
credentials, activation codes, device identity and conversation text are not logged.
Device binding identity persists locally; transport credentials are temporary.

## History and privacy

The dedicated `chatlog` partition is a 128 KB ring of 32 erase sectors. It retains
15 text entries, each up to 767 UTF-8 bytes plus a terminator. Recognized speech
and synthesized reply fragments are separate entries, so this is not 15 complete
conversations. Long text is truncated at UTF-8 boundaries or uses multiple entries.

Records use sequence numbers, length, checksum and a final commit marker. Torn
or corrupt writes are ignored, and a new write does not erase the current valid
history. Completed text is saved promptly; in-flight or unfinished writes can
still be lost. Storage failure falls back to a bounded 12-entry RAM history,
which is not guaranteed to survive restart. Ordinary application-only updates
can preserve the history; a full erase or an incompatible image can remove it.

History reads use mapped Flash and render only visible text. The 16-pixel font
covers ordinary CJK characters, punctuation and kana; emoji and supplementary
CJK require additional glyphs. See the [font license](../assets/fonts/stelle-chat/LICENSE.txt).
Voice and recognized text are sent to the configured external service; the local
history is stored on the device. Do not publish device credentials or real histories.

## Memory and connection reuse

The ESP32-C3 has no PSRAM. The chat task reserves a 25 KB codec pool before
Wi-Fi/TLS starts; encoder and decoder run in separate phases and reuse that pool.
The integration hooks the pinned audio library's allocation functions only for
the Chat task. Allocation failure does not silently fall back to a fragmented heap.
Other tasks continue using their original allocators. The task stack is 27 KB.

After a successful exchange, a foreground Chat/history view can retain the
Wi-Fi and MQTT connection for up to 120 seconds. The UDP voice channel closes
between exchanges and uses new session parameters next time. Leaving Chat,
screen-off, idle expiry or a transport error releases connections and temporary
credentials. A failed warm reconnect falls back once to a complete connection.
Keeping a codec allocation ready does not activate the microphone.

## Verification

Host tests cover state transitions, packet bounds, ordering, UTF-8 layout,
connection reuse, codec-pool isolation and power-loss storage recovery. Verify
binding, microphone quality, uninterrupted playback, actual codec memory peaks,
long replies and repeated exchanges on the physical device. Build success and
native previews do not establish these hardware results.
