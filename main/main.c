// main/main.c - platform layer: buttons, backlight, audio, battery, storage, LVGL.
//
// Button callbacks only enqueue. One platform task owns the model, I2C (volume,
// fuel gauge) and NVS; a small audio task only streams PCM over I2S so a chime
// never stalls the UI. The view is touched only with the LVGL lock held.
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "app_model.h"
#include "app_store.h"
#include "asset_pack.h"
#include "key_sound.h"
#include "anniv_bgm.h"
#include "play_sounds.h"
#include "battle_pack.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "charge_detect.h"
#include "chat_service.h"
#include "chat_store.h"
#include "net.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pet_view.h"

static const char *TAG = "stelle";
static anniv_bgm_t s_bgm;
static play_sounds_t s_play_sounds;
static anniv_bgm_t s_hit;

static void mount_play_sounds(const esp_partition_t *part, size_t offset)
{
    uint8_t header[32];
    if (offset > part->size || sizeof(header) > part->size-offset ||
        esp_partition_read(part, offset, header, sizeof(header)) != ESP_OK ||
        memcmp(header, "STFX", 4)) return;
    uint32_t bytes = (uint32_t)header[8] | (uint32_t)header[9] << 8 |
                     (uint32_t)header[10] << 16 | (uint32_t)header[11] << 24;
    if (bytes < 64 || bytes > part->size-offset) return;
    const void *ptr = NULL;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, offset, bytes, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK) return;
    if (!play_sounds_bind(&s_play_sounds, ptr, bytes)) esp_partition_munmap(handle);
}

static bool mount_bgm(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                         (esp_partition_subtype_t)0x40, "assets");
    size_t offset = (STELLE_PACK_SIZE + 4095U) & ~4095U;
    uint8_t header[32];
    if (!part || part->size < offset + sizeof(header) ||
        esp_partition_read(part, offset, header, sizeof(header)) != ESP_OK ||
        memcmp(header, "SBGM", 4)) return false;
    uint32_t blocks = (uint32_t)header[12] | (uint32_t)header[13] << 8 |
                      (uint32_t)header[14] << 16 | (uint32_t)header[15] << 24;
    if (!blocks || blocks > (part->size - offset - sizeof(header)) / 124U) return false;
    size_t bytes = sizeof(header) + (size_t)blocks * 124U;
    const void *ptr = NULL;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, offset, bytes, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK) return false;
    if (anniv_bgm_bind(&s_bgm, ptr, bytes)) {
        mount_play_sounds(part, offset+bytes);
        return true;
    }
    esp_partition_munmap(handle);
    return false;
}

// Map the resource partition (type data, subtype 0x40 at 0x420000) read-only
// and validate it, as the Miki firmware does with its asset partition. The
// mapping stays for the life of the app, so images draw straight from Flash.
static asset_pack_result_t mount_assets(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, NULL);
    if (!part || part->size < STELLE_PACK_SIZE) return ASSET_PACK_MISSING;
    const void *ptr = NULL;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, 0, STELLE_PACK_SIZE, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK)
        return ASSET_PACK_MISSING;
    asset_pack_result_t r = asset_pack_bind((const uint8_t *)ptr, STELLE_PACK_SIZE);
    if (r != ASSET_PACK_OK) esp_partition_munmap(handle);
    return r;
}

static asset_pack_result_t mount_trailblaze(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                         (esp_partition_subtype_t)0x42, "trailblaze");
    if (!part || part->size < TRAILBLAZE_PACK_SIZE) return ASSET_PACK_MISSING;
    const void *ptr = NULL;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, 0, TRAILBLAZE_PACK_SIZE, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK)
        return ASSET_PACK_MISSING;
    asset_pack_result_t result = asset_trailblaze_bind(ptr, TRAILBLAZE_PACK_SIZE);
    if (result != ASSET_PACK_OK) esp_partition_munmap(handle);
    return result;
}

#define LOOP_MS 30
#define BATTERY_PERIOD_MS 30000U
#define SAVE_DEBOUNCE_MS 800U
#define AUDIO_HZ 16000
_Static_assert(AUDIO_HZ == KEY_SOUND_HZ, "key feedback must use the codec sample rate");
#define RESYNC_PERIOD_MS (6U * 3600U * 1000U)   // refresh network time every 6 h

typedef enum { SOUND_CHIME = 0, SOUND_BLIP, SOUND_KEY, SOUND_BAT, SOUND_HIT } sound_t;
typedef struct { sound_t sound; uint32_t queued_ms; } sound_job_t;
typedef struct { app_input_t input; uint32_t received_ms; } key_job_t;
typedef struct { uint32_t session; bool wanted, play_active, battle_active; } bgm_request_t;

static QueueHandle_t s_keys, s_sounds;
static QueueHandle_t s_bgm_requests;
static SemaphoreHandle_t s_audio_mutex;
static app_model_t s_model;
// Boot loads the focus log once. Reuse that existing static allocation for
// journal I/O afterwards, rather than adding a second permanent scratch buffer.
static union { focus_log_t focus; chat_journal_workspace_t chat; } s_store_work;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void push(app_input_t in, uint32_t received_ms)
{
    key_job_t job = {in, received_ms};
    (void)xQueueSend(s_keys, &job, 0);
}

static void on_key(bsp_btn_t key, bsp_btn_ev_t ev, void *ctx)
{
    static uint32_t pressed_ms[3];
    (void)ctx;
    // SINGLE_CLICK may be reported only after the double-click window. Keep
    // the actual press time so a hit-period press cannot act after recovery.
    if (ev == BSP_BTN_PRESS) { pressed_ms[key] = now_ms(); return; }
    app_input_t base = key == BSP_BTN_UP ? IN_UP : key == BSP_BTN_DOWN ? IN_DOWN : IN_OK;
    if (ev == BSP_BTN_CLICK) {
        push(base, pressed_ms[key]);
    } else if (ev == BSP_BTN_DOUBLE) {
        // The driver reports a fast double tap instead of two clicks. Two steps
        // for UP/DOWN keeps list scrolling responsive; OK stays a single action.
        push(base, pressed_ms[key]);
        if (base != IN_OK) push(base, pressed_ms[key]);
    } else if (ev == BSP_BTN_LONG) {
        push(key == BSP_BTN_UP ? IN_HOLD_UP : key == BSP_BTN_DOWN ? IN_HOLD_DOWN : IN_HOLD_OK, pressed_ms[key]);
    }
}

static void tone(int16_t *buf, size_t n, float hz, float amp, size_t offset, size_t total)
{
    for (size_t i = 0; i < n; i++) {
        size_t k = offset + i;
        float attack = (float)k / 160.0f, decay = (float)(total - k) / (float)total;
        float env = attack < decay ? attack : decay;  // soft attack, linear decay: no clicks
        buf[i] = (int16_t)(amp * env * sinf(6.2831853f * hz * (float)k / AUDIO_HZ));
    }
}

static void play(float hz, unsigned ms, float amp)
{
    int16_t pcm[160];
    size_t total = (size_t)AUDIO_HZ * ms / 1000U;
    for (size_t done = 0; done < total; done += 160) {
        size_t n = total - done < 160 ? total - done : 160;
        tone(pcm, n, hz, amp, done, total);
        if (bsp_audio_write(pcm, n * sizeof(int16_t)) != ESP_OK) return;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    sound_job_t job;
    bgm_request_t music = {0};
    for (;;) {
        bgm_request_t next_music;
        if (xQueueReceive(s_bgm_requests, &next_music, 0) == pdTRUE) music = next_music;
        chat_service_status_t bgm_voice;
        chat_service_get_status(&bgm_voice);
        bool voice_idle = bgm_voice.phase == CHAT_OFF || bgm_voice.phase == CHAT_ERROR;
        anniv_bgm_sync(&s_bgm, music.session, music.wanted && voice_idle);
        if (xQueueReceive(s_sounds, &job, 0) != pdTRUE) {
            // One owner for PCM. Release between 20 ms blocks so Chat and
            // storage can acquire the codec; never access LVGL from this task.
            bool wrote = false;
            if ((s_bgm.wanted || s_bgm.gain) && xSemaphoreTake(s_audio_mutex, 0) == pdTRUE) {
                int16_t pcm[ANNIV_BGM_SAMPLES];
                if (anniv_bgm_render(&s_bgm, pcm)) {
                    wrote = bsp_audio_write(pcm, sizeof(pcm)) == ESP_OK;
                    if (!wrote) { s_bgm.data = NULL; s_bgm.wanted = false; s_bgm.gain = 0; }
                }
                xSemaphoreGive(s_audio_mutex);
            }
            // I2S normally paces writes. Yield at least a tick after each block;
            // when paused/missing/busy, sleep without advancing the music.
            vTaskDelay(pdMS_TO_TICKS(wrote ? 1 : 10));
            continue;
        }
        if (job.sound == SOUND_KEY) {
            // Feedback is disposable: do not replay old clicks after a chime
            // or wait for Chat to finish using the codec.
            if (now_ms() - job.queued_ms > 120U || xSemaphoreTake(s_audio_mutex, 0) != pdTRUE) continue;
            chat_service_status_t voice;
            chat_service_get_status(&voice);
            if ((voice.phase == CHAT_OFF || voice.phase == CHAT_ERROR) && now_ms() - job.queued_ms <= 120U) {
                int16_t pcm[160];
                for (unsigned offset = 0; offset < KEY_SOUND_BELL_SAMPLES; offset += 160U) {
                    sound_job_t next;
                    chat_service_get_status(&voice);
                    if ((voice.phase != CHAT_OFF && voice.phase != CHAT_ERROR) ||
                        (xQueuePeek(s_sounds, &next, 0) == pdTRUE && next.sound != SOUND_KEY)) break;
                    unsigned count = KEY_SOUND_BELL_SAMPLES - offset;
                    if (count > 160U) count = 160U;
                    key_sound_bell_render(pcm, count, offset);
                    if (bsp_audio_write(pcm, count * sizeof(pcm[0])) != ESP_OK) break;
                }
            }
            xSemaphoreGive(s_audio_mutex);
            continue;
        }
        if (job.sound == SOUND_HIT) {
            if (xQueueReceive(s_bgm_requests, &next_music, 0) == pdTRUE) music = next_music;
            chat_service_get_status(&bgm_voice);
            if (!music.battle_active || !s_hit.data || now_ms()-job.queued_ms > 200U ||
                (bgm_voice.phase != CHAT_OFF && bgm_voice.phase != CHAT_ERROR) ||
                xSemaphoreTake(s_audio_mutex, 0) != pdTRUE) continue;
            s_bgm.gain = 0;
            anniv_bgm_sync(&s_hit, s_hit.session+1U, true);
            s_hit.gain = ANNIV_BGM_SAMPLES; // impact already has a 2ms edge ramp; keep its attack crisp
            for (uint32_t remaining=s_hit.blocks; remaining; remaining--) {
                if (xQueueReceive(s_bgm_requests, &next_music, 0) == pdTRUE) music = next_music;
                chat_service_get_status(&bgm_voice);
                sound_job_t next;
                if (!music.battle_active || (bgm_voice.phase != CHAT_OFF && bgm_voice.phase != CHAT_ERROR) ||
                    (xQueuePeek(s_sounds, &next, 0) == pdTRUE && next.sound != SOUND_KEY)) break;
                int16_t pcm[ANNIV_BGM_SAMPLES];
                if (!anniv_bgm_render(&s_hit, pcm) || bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) break;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            s_hit.wanted = false;
            s_hit.gain = 0;
            xSemaphoreGive(s_audio_mutex);
            continue;
        }
        if (job.sound == SOUND_BAT) {
            // Rotation advances only when one clip really starts. Missing,
            // stale, muted or Chat-blocked jobs do not consume a clip number.
            if (xQueueReceive(s_bgm_requests, &next_music, 0) == pdTRUE) music = next_music;
            chat_service_get_status(&bgm_voice);
            if (!music.play_active || !play_sounds_ready(&s_play_sounds) ||
                (bgm_voice.phase != CHAT_OFF && bgm_voice.phase != CHAT_ERROR) ||
                now_ms()-job.queued_ms > 200U || xSemaphoreTake(s_audio_mutex, 0) != pdTRUE) continue;
            s_bgm.gain = 0;
            if (play_sounds_start(&s_play_sounds) >= 0) {
                int16_t pcm[ANNIV_BGM_SAMPLES];
                for (;;) {
                    if (xQueueReceive(s_bgm_requests, &next_music, 0) == pdTRUE) music = next_music;
                    chat_service_get_status(&bgm_voice);
                    sound_job_t next;
                    if (!music.play_active ||
                        (bgm_voice.phase != CHAT_OFF && bgm_voice.phase != CHAT_ERROR) ||
                        (xQueuePeek(s_sounds, &next, 0) == pdTRUE && next.sound != SOUND_KEY)) break;
                    if (!play_sounds_render(&s_play_sounds, pcm) ||
                        bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) break;
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                play_sounds_stop(&s_play_sounds);
            }
            xSemaphoreGive(s_audio_mutex);
            continue;
        }
        xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
        s_bgm.gain = 0; // fade music back in after higher-priority feedback
        if (job.sound == SOUND_CHIME) {
            int16_t pcm[160];
            int64_t max_render_us = 0;
            for (unsigned offset = 0; offset < KEY_SOUND_CHIME_SAMPLES; offset += 160U) {
                int64_t render_at = esp_timer_get_time();
                key_sound_chime_render(pcm, 160U, offset);
                int64_t render_us = esp_timer_get_time()-render_at;
                if (render_us > max_render_us) max_render_us = render_us;
                if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) break;
                // The low-priority model owner must advance the animation
                // even while DMA has room and the audio writer would not block.
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            ESP_LOGI(TAG, "chime max render time per 10 ms block: %lld us", max_render_us);
        } else {
            play(1320.0f, 45, 3500.0f);
        }
        xSemaphoreGive(s_audio_mutex);
    }
}

void app_main(void)
{
    app_model_init(&s_model, now_ms());
    app_model_seed_scenes(&s_model, esp_random());
    pet_view_status_t status = {.battery = -1};
    status.storage_ok = app_store_open();
    s_model.battle_storage_ready = status.storage_ok && app_store_load_energy(&s_model.path_energy);
    if (!s_model.battle_storage_ready) ESP_LOGW(TAG, "Battle counter unavailable; preserving stored key");
    if (!anniv_bgm_bind(&s_hit, battle_hit_data, battle_hit_size)) ESP_LOGW(TAG, "Battle hit cue unavailable");
    app_prefs_t stored;
    if (status.storage_ok && app_store_load(&stored)) app_model_load_prefs(&s_model, &stored);
    if (status.storage_ok && app_store_load_log(&s_store_work.focus)) app_model_load_log(&s_model, &s_store_work.focus);
    bool history_ok=chat_store_open(&s_model.chat_history,&s_store_work.chat);
    if(!history_ok)ESP_LOGW(TAG,"saved history unavailable; bounded RAM fallback");

    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display init failed");
        return;
    }
    bool battery_ok = bsp_battery_init() == ESP_OK;
    status.audio_ok = bsp_audio_init() == ESP_OK && bsp_audio_set_format(AUDIO_HZ, 16, 1) == ESP_OK;
    uint8_t volume = app_volume_percent(&s_model), backlight = app_backlight_percent(&s_model);
    if (status.audio_ok) bsp_audio_set_volume(volume);
    if (battery_ok) status.battery = bsp_battery_soc();

    // Network: no saved Wi-Fi yet (first flash) -> setup page with the hotspot QR;
    // otherwise join the saved network in the background and fetch the time.
    static net_status_t net;
    bool net_ok = net_init();
    if (net_ok) {
        net_get_status(&net);
        status.net = &net;
        if (!net.has_credentials) app_model_begin_setup(&s_model, false);
        else net_sync_time();
    }

    s_keys = xQueueCreate(16, sizeof(key_job_t));
    s_sounds = xQueueCreate(4, sizeof(sound_job_t));
    s_bgm_requests = xQueueCreate(1, sizeof(bgm_request_t));
    s_audio_mutex = xSemaphoreCreateMutex();
    if (!s_keys || !s_sounds || !s_audio_mutex || !s_bgm_requests) return;
    bool bgm_ok = mount_bgm();
    ESP_LOGI(TAG, "anniversary BGM: %s", bgm_ok ? "ready" : "unavailable (silent)");
    ESP_LOGI(TAG, "Play bat sounds: %s", play_sounds_ready(&s_play_sounds) ? "ready (3 sequential clips)" : "unavailable (silent)");
    if (status.audio_ok && xTaskCreate(audio_task, "audio", 3072, NULL, 4, NULL) != pdPASS) status.audio_ok = false;
    bool chat_ok = status.audio_ok && net_ok && chat_service_init(s_audio_mutex);
    static chat_service_status_t chat_status;
    status.chat = &chat_status;

    int64_t t0 = esp_timer_get_time();
    asset_pack_result_t assets = mount_assets();
    asset_pack_result_t destinations = mount_trailblaze();
    ESP_LOGI(TAG, "Trailblaze destinations: %s", asset_pack_result_name(destinations));
    ESP_LOGI(TAG, "asset pack: %s (%u bytes, crc 0x%08lx) in %lld ms", asset_pack_result_name(assets),
             (unsigned)STELLE_PACK_SIZE, (unsigned long)STELLE_PACK_CRC, (esp_timer_get_time() - t0) / 1000);

    if (bsp_lvgl_lock(1000)) {
        lv_screen_load(pet_view_create());
        app_model_display_ready(&s_model,now_ms());
        pet_view_update(&s_model, &status, now_ms());
        bsp_lvgl_unlock();
    }
    bsp_display_backlight(backlight);
    bool buttons_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    ESP_LOGI(TAG, "ready buttons=%d battery=%d storage=%d audio=%d net=%d heap=%u largest=%u", buttons_ok, battery_ok,
             status.storage_ok, status.audio_ok, net_ok, (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    uint32_t battery_at = now_ms(), dirty_at = 0, charge_at = 0;
    charge_detect_t charge;
    charge_detect_init(&charge);
    bool dirty = false, log_dirty = false;
    uint32_t log_at = 0;
    app_page_t shown_page = PAGE_HOME;
    bool portal_seen = false;  // this setup session has had its hotspot up
    uint32_t sync_at = now_ms();
    if (net_ok && s_model.page == PAGE_SETUP) { net_start_portal(); shown_page = PAGE_SETUP; }
    for (;;) {
        key_job_t in;
        if (xQueueReceive(s_keys, &in, pdMS_TO_TICKS(LOOP_MS)) == pdTRUE) {
            app_model_handle_queued(&s_model, in.input, in.received_ms, now_ms());
            while (xQueueReceive(s_keys, &in, 0) == pdTRUE)
                app_model_handle_queued(&s_model, in.input, in.received_ms, now_ms());
        }
        uint32_t now = now_ms();
        app_model_tick(&s_model, now);
        if (chat_ok) chat_service_set_foreground(!s_model.screen_off &&
            (app_chat_in_focus(&s_model) || s_model.page==PAGE_CHAT_HISTORY));

        uint32_t ev = app_model_take_events(&s_model);
        if (ev & APP_EV_ENERGY_CHANGED) {
            bool saved = app_store_save_energy(s_model.path_energy);
            app_model_energy_saved(&s_model, saved);
            if (!saved) {
                ev &= ~APP_EV_BATTLE_HIT;
                ESP_LOGW(TAG, "Battle counter commit failed; award/reset rolled back");
            }
        }
        if ((ev & APP_EV_FOCUS_DONE) && chat_model_active(&s_model.chat)) {
            chat_service_command(CHAT_CMD_CANCEL);
            chat_model_set_phase(&s_model.chat, CHAT_OFF);
        }
        if (ev & APP_EV_CHAT_START) {
            if (chat_ok) chat_service_command(CHAT_CMD_START);
            else chat_model_set_phase(&s_model.chat, CHAT_ERROR);
        }
        if (ev & APP_EV_CHAT_STOP_RECORDING) chat_service_command(CHAT_CMD_STOP_RECORDING);
        if (ev & APP_EV_CHAT_CANCEL) chat_service_command(CHAT_CMD_CANCEL);
        if (chat_ok) {
            chat_service_get_status(&chat_status);
            // A freshly queued start must not consume the previous idle snapshot.
            if (chat_model_active(&s_model.chat) && !(ev & APP_EV_CHAT_START))
                chat_model_set_phase(&s_model.chat, chat_status.phase);
            static chat_entry_t entry;
            while (chat_service_take_text(&entry)) {
                if(history_ok && !chat_store_append(&s_model.chat_history,&entry))history_ok=false;
                if(!history_ok)chat_history_add(&s_model.chat_history,entry.assistant,entry.text);
            }
            if (chat_model_active(&s_model.chat) && !app_chat_in_focus(&s_model)) {
                chat_service_command(CHAT_CMD_CANCEL);
                chat_model_set_phase(&s_model.chat, CHAT_OFF);
            }
        }
        if (ev & APP_EV_PREFS_CHANGED) { dirty = true; dirty_at = now; }
        if (ev & APP_EV_LOG_CHANGED) { log_dirty = true; log_at = now; }
        uint8_t want_volume = app_volume_percent(&s_model);
        if (status.audio_ok && want_volume != volume) { bsp_audio_set_volume(want_volume); volume = want_volume; }
        bgm_request_t music = {
            .session = s_model.anniv_session,
            .wanted = s_model.page == PAGE_ANNIV && !s_model.anniv_paused &&
                      !s_model.screen_off && volume && !chat_model_active(&s_model.chat),
            .play_active = s_model.page == PAGE_PLAY && !s_model.screen_off &&
                           volume && !chat_model_active(&s_model.chat),
            .battle_active = s_model.page == PAGE_BATTLE && !s_model.screen_off &&
                             volume && !chat_model_active(&s_model.chat),
        };
        xQueueOverwrite(s_bgm_requests, &music);
        if (status.audio_ok && volume) {
            sound_job_t job = {.queued_ms = now};
            if (ev & APP_EV_BATTLE_HIT) { job.sound = SOUND_HIT; xQueueSend(s_sounds, &job, 0); }
            if (ev & (APP_EV_FOCUS_DONE | APP_EV_PLAY_JUMP)) { job.sound = SOUND_CHIME; xQueueSend(s_sounds, &job, 0); }
            if ((ev & APP_EV_PLAY_BAT) && !(ev & (APP_EV_FOCUS_DONE | APP_EV_PLAY_JUMP))) {
                job.sound = SOUND_BAT;
                xQueueSend(s_sounds, &job, 0);
            }
            if (ev & APP_EV_VOLUME_PREVIEW) { job.sound = SOUND_BLIP; xQueueSend(s_sounds, &job, 0); }
            if ((ev & APP_EV_KEY_SOUND) && !chat_model_active(&s_model.chat) &&
                !(ev & (APP_EV_BATTLE_HIT | APP_EV_FOCUS_DONE | APP_EV_PLAY_JUMP | APP_EV_PLAY_BAT | APP_EV_VOLUME_PREVIEW | APP_EV_CHAT_START |
                        APP_EV_CHAT_STOP_RECORDING | APP_EV_CHAT_CANCEL)) &&
                xSemaphoreTake(s_audio_mutex, 0) == pdTRUE) {
                // Do not queue more key sounds while a bell, voice or chime is using the codec.
                xSemaphoreGive(s_audio_mutex);
                job.sound = SOUND_KEY;
                xQueueSend(s_sounds, &job, 0);
            }
        }

        uint8_t want_light = app_backlight_percent(&s_model);
        if (want_light != backlight) {
            bool waking=backlight==0 && want_light>0;
            bsp_display_backlight(want_light);backlight=want_light;
            if (waking) app_model_display_ready(&s_model,now);
        }

        if (battery_ok && now - battery_at >= BATTERY_PERIOD_MS) { status.battery = bsp_battery_soc(); battery_at = now; }
        if (now - charge_at >= CHARGE_SAMPLE_MS) {
            // Keeps sampling while the screen is off so the state is current on wake.
            bool was = status.charging;
            status.charging = charge_detect_sample(&charge, battery_ok ? bsp_battery_mv() : -1,
                                                   usb_serial_jtag_is_connected());
            charge_at = now;
            if (status.charging != was) {
                ESP_LOGI(TAG, "charging=%d", status.charging);
                if (battery_ok) { status.battery = bsp_battery_soc(); battery_at = now; }
            }
        }
        if (net_ok) {
            net_get_status(&net);
            // The hotspot follows the setup page: up while shown, down when left.
            if (s_model.page == PAGE_SETUP && shown_page != PAGE_SETUP) { net_start_portal(); portal_seen = false; }
            if (s_model.page != PAGE_SETUP && shown_page == PAGE_SETUP) {
                net_stop_portal();  // queued after any join in progress
                if (net.has_credentials) { net_sync_time(); sync_at = now; }  // keep using the saved network
            }
            shown_page = s_model.page;
            if (net.phase >= NET_PHASE_PORTAL && net.phase <= NET_PHASE_JOIN_FAILED) portal_seen = true;
            // The phone sent a network that worked (it is saved and time sync has
            // started): leave setup; the clock fills in when the time arrives.
            if (s_model.page == PAGE_SETUP && portal_seen && net.has_credentials && net.phase >= NET_PHASE_CONNECTING) {
                app_model_setup_finished(&s_model);
                shown_page = s_model.page;
                sync_at = now;
            }            s_model.time_sync = !net.has_credentials                                         ? SYNC_OFF
                              : net.phase == NET_PHASE_SYNCED                                ? SYNC_OK
                              : net.phase == NET_PHASE_OFFLINE                               ? SYNC_FAILED
                              : net.phase == NET_PHASE_CONNECTING || net.phase == NET_PHASE_SYNCING ? SYNC_RUNNING
                                                                                             : s_model.time_sync;
            uint32_t sod;
            int32_t day;
            if (net_take_time(&sod, &day)) app_model_set_time(&s_model, sod, day, now);
            if (net.has_credentials && s_model.page != PAGE_SETUP && now - sync_at >= RESYNC_PERIOD_MS) {
                net_sync_time();
                sync_at = now;
            }
        }
        if (dirty && now - dirty_at >= SAVE_DEBOUNCE_MS) {
            status.storage_ok = app_store_save(&s_model.prefs);
            dirty = false;
            if (!status.storage_ok) ESP_LOGW(TAG, "prefs not saved");
        }
        if (log_dirty && now - log_at >= SAVE_DEBOUNCE_MS) {  // focus log: session events and every 5 min
            status.storage_ok = app_store_save_log(&s_model.log);
            log_dirty = false;
            if (!status.storage_ok) ESP_LOGW(TAG, "focus log not saved");
        }
        // Preparing unused history sectors may erase Flash for longer than the
        // music DMA reserve. Defer that optional housekeeping while BGM plays.
        if(history_ok && !(music.wanted && bgm_ok) && !chat_model_active(&s_model.chat) &&
           xSemaphoreTake(s_audio_mutex,0)==pdTRUE) {
            chat_store_prepare();
            xSemaphoreGive(s_audio_mutex);
        }

        // Screen off: backlight is 0 and the view is not touched, so LVGL has
        // nothing to redraw. Timers keep running in the model.
        if (s_model.screen_off) continue;
        if (bsp_lvgl_lock(100)) {
            pet_view_update(&s_model, &status, now);
            bsp_lvgl_unlock();
        }
    }
}
