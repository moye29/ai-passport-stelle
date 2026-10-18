// Native preview adapter: drives the real model + view with scripted inputs.
// All fixture data (clock, battery, tasks) is simulated and labelled as such.
#include "preview_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_model.h"
#include "asset_pack.h"
#include "pet_view.h"
#include "ui_copy.h"

static app_model_t m;
static lv_obj_t *screen;
static uint32_t now;
static pet_view_status_t status = {87, true, true, false, NULL, NULL};
static net_status_t net;  // simulated network state
static int failures;
static size_t peak;

static void press(app_input_t in) { app_model_handle(&m, in, now); }
static void select_nav(app_nav_t nav) {
    while (m.nav != nav) {
        if (m.page == PAGE_BATTLE) press(IN_HOLD_OK);
        press(IN_DOWN);
    }
}
static void wait_ms(uint32_t ms)
{
    // Advance in small steps like the firmware loop so deadlines are exercised.
    for (uint32_t t = 0; t < ms; t += 40) { now += ms - t < 40 ? ms - t : 40; app_model_tick(&m, now); }
}
static void set_clock(unsigned h, unsigned mi)
{
    m.clock_s = h * 3600U + mi * 60U;
    m.clock_ms = now;
    m.clock_set = true;
}
static void hold_motion(companion_motion_t motion, uint32_t at_ms)
{
    companion_model_play(&m.companion, motion, 1, now - at_ms);
}
static void fresh(void)
{
    now = 100000;
    app_model_init(&m, now);
    m.prefs.screen_off = 0;
    // Simulated date and focus history (preview data only).
    m.date_known = true;
    m.day = m.cal_day = date_to_day(2026, 10, 16);
    for (int i = 1; i <= 40; i++)
        if (i % 3 != 0) focus_log_add(&m.log, m.day - i, (uint32_t)((i * 977) % 7600) + 120U, (uint16_t)(1 + i % 3));
    focus_log_add(&m.log, m.day, 50 * 60, 2);
    status = (pet_view_status_t){87, true, true, false, NULL, NULL};
    wait_ms(1200);  // boot greeting finished
    set_clock(10, 30);
}

static const char *const NAMES[] = {
    "home-day-focus", "home-dusk-calendar", "home-night-settings", "home-late-play",
    "home-boot-unset-clock", "focus-ready-countdown", "focus-ready-countup", "focus-running",
    "focus-paused", "focus-confirm-stop", "focus-done", "home-focus-running",
    "play-wave", "play-jump-apex", "calendar-today", "calendar-past-day",
    "anniversary", "calendar-date-unknown", "settings", "settings-auto-off",
    "adjust-brightness", "adjust-volume-no-audio", "adjust-auto-off", "setup-first-boot",
    "home-low-battery", "focus-running-night",
    "play-bat-materialize", "play-bat-contact", "home-charging", "home-charging-low",
    "home-train-day", "home-train-dusk-play", "home-train-late", "scene-picker-room",
    "setup-phone-joined", "setup-wrong-password", "settings-wifi", "home-syncing-time",
    "scene-picker-train-night", "focus-held-pose", "home-asleep-before-8",
    "chat-nav-off", "chat-card-off", "chat-recording", "chat-playing", "chat-waiting",
    "chat-history-empty", "chat-history-chinese", "chat-binding", "chat-train-recording",
    "chat-history-user",
    "history-scroll-top", "history-scroll-middle", "history-scroll-bottom",
    "history-long-bottom", "history-long-top", "history-full-ring",
    "history-scroll-one-line",
    "sleep-cycle-start", "sleep-cycle-pose-1", "sleep-cycle-pose-2", "sleep-cycle-wrap",
    "bat-00", "bat-01", "bat-02", "bat-03", "bat-04", "bat-05", "bat-06", "bat-07", "bat-return-idle-a", "bat-return-idle-b",
    "bat-return-idle",
    "sleep-cycle-wrap-new",
    "idle-00", "idle-01", "idle-02", "idle-04", "idle-05", "idle-wrap",
    "greeting-00", "greeting-01", "greeting-02", "greeting-03", "greeting-04",
    "focus-00", "focus-01", "focus-02", "focus-03", "focus-04", "focus-05", "focus-06", "focus-07", "focus-08",
    "celebration-00", "celebration-01", "celebration-02", "celebration-03", "celebration-04",
    "chat-audio-error", "chat-network-error",
    "sleep-04", "sleep-05", "sleep-06", "sleep-07", "sleep-wrap-eight",
    "saved-history-top", "saved-history-middle", "saved-history-bottom",
    "copy-morning", "copy-companion", "copy-night", "copy-focus-start",
    "copy-focus-complete", "copy-focus-complete-again",
    "scene-picker-trailblaze",
    "trailblaze-00", "trailblaze-01", "trailblaze-02", "trailblaze-03",
    "trailblaze-04", "trailblaze-05", "trailblaze-06", "trailblaze-07",
    "trailblaze-08", "trailblaze-09", "trailblaze-10", "trailblaze-11",
    "adjust-volume-normal",
    "battle-ready", "battle-encounter", "battle-hit", "battle-hit-499ms", "battle-recover",
    "battle-next-monster", "battle-reset", "battle-exit", "battle-reenter", "battle-night",
    "battle-train", "battle-trailblaze", "battle-long-counter", "battle-save-failed",
    "battle-auto-monster", "battle-nav-swing-end", "battle-nav-swing-repeat", "battle-ready-down",
    "battle-ready-hidden-counter",
    "battle-exit-swing-0", "battle-exit-swing-1", "battle-exit-swing-2", "battle-exit-swing-3",
    "battle-exit-swing-4", "battle-exit-swing-5", "battle-exit-swing-6", "battle-exit-swing-7", "battle-exit-idle-gap",
    "anniv-frame-0", "anniv-frame-1", "anniv-frame-2", "anniv-frame-3",
    "anniv-frame-4", "anniv-frame-5", "anniv-frame-6", "anniv-frame-7",
    "anniv-loop-wrap", "anniv-music-paused-animation-runs", "anniv-reenter-first-frame",
    "battle-first-enemy-A", "battle-next-enemy-B", "battle-hit-enemy-B", "battle-return-enemy-A", "battle-clear-enemy-B",
    "display-boot-greeting-start", "display-boot-greeting-middle", "display-wake-home",
    "display-wake-focus-running", "display-wake-chat", "display-wake-battle",
    "display-wake-settings", "display-wake-history", "display-greeting-finished",
    "settings-scene-selected-room", "settings-scene-selected-trailblaze", "settings-scene-idle-trailblaze",

};
#define SCENES (sizeof(NAMES) / sizeof(NAMES[0]))

// The same asset pack that is flashed to the resource partition, read from
// the file named by STELLE_PACK (set by run_preview.ps1) and bound once.
static void load_pack(void)
{
    static uint8_t *pack;
    if (pack) return;
    const char *path = getenv("STELLE_PACK");
    FILE *fp = path ? fopen(path, "rb") : NULL;
    if (!fp) { printf("asset pack: cannot open %s\n", path ? path : "(STELLE_PACK unset)"); exit(4); }
    pack = malloc(STELLE_PACK_SIZE);
    size_t n = pack ? fread(pack, 1, STELLE_PACK_SIZE, fp) : 0;
    fclose(fp);
    asset_pack_result_t r = asset_pack_bind(pack, n);
    printf("asset pack: %s (%zu bytes)\n", asset_pack_result_name(r), n);
    if (r != ASSET_PACK_OK) exit(4);
    path = getenv("TRAILBLAZE_PACK");
    fp = path ? fopen(path, "rb") : NULL;
    if (fp) {
        static uint8_t *destinations;
        destinations = malloc(TRAILBLAZE_PACK_SIZE);
        n = destinations ? fread(destinations, 1, TRAILBLAZE_PACK_SIZE, fp) : 0;
        fclose(fp);
        r = asset_trailblaze_bind(destinations, n);
        printf("Trailblaze: %s\n", asset_pack_result_name(r));
        if (r != ASSET_PACK_OK) exit(4);
    }
}

lv_obj_t *pet_preview_create(void)
{
    load_pack();
    fresh();
    screen = pet_view_create();
    static bool measured;
    if (!measured) {
        measured = true;
        const char *const *lines;
        size_t count = ui_copy_all_fixed(&lines);
        for (size_t i=0;i<count;i++) {
            lv_point_t a,b;
            lv_text_get_size(&a,lines[i],&lv_font_montserrat_14,-1,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            lv_text_get_size(&b,lines[i],&lv_font_montserrat_12,-1,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            printf("speech-fit 14px=%d 12px=%d limit=194: %s\n",(int)a.x,(int)b.x,lines[i]);
            if (a.x>194 && b.x>194) { printf("Speech cannot fit existing fonts\n"); exit(3); }
        }
    }
    return screen;
}

size_t pet_preview_scene_count(void) { return SCENES; }

static void check_fit(lv_obj_t *obj, size_t scene)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *c = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t a;
        lv_obj_get_coords(c, &a);
        // Continuous history labels intentionally extend beyond their clipped viewport.
        for (lv_obj_t *p=lv_obj_get_parent(c);p;p=lv_obj_get_parent(p)) {
            if (lv_obj_has_flag(p,LV_OBJ_FLAG_USER_2)) {
                lv_area_t clip; lv_obj_get_coords(p,&clip);
                if (a.x1<clip.x1) a.x1=clip.x1;
                if (a.y1<clip.y1) a.y1=clip.y1;
                if (a.x2>clip.x2) a.x2=clip.x2;
                if (a.y2>clip.y2) a.y2=clip.y2;
                if (a.x1>a.x2 || a.y1>a.y2) { a=clip; break; }
            }
        }
        if (a.x1 < 0 || a.y1 < 0 || a.x2 > 239 || a.y2 > 319) {
            // Sprites may extend above the room; they are clipped by it.
            if (!lv_obj_check_type(c, &lv_image_class)) {
                printf("FIT FAIL scene %zu (%s): object outside screen %d,%d..%d,%d\n", scene, NAMES[scene],
                       (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2);
                failures++;
            }
        }
        if (lv_obj_check_type(c, &lv_label_class)) {
            const char *text = lv_label_get_text(c);
            const lv_font_t *font = lv_obj_get_style_text_font(c, 0);
            lv_point_t p;
            lv_text_get_size(&p, text, font, lv_obj_get_style_text_letter_space(c, 0), 0, LV_COORD_MAX,
                             LV_TEXT_FLAG_NONE);
            for (const unsigned char *s = (const unsigned char *)text; *s;) {
                uint32_t cp = *s++;
                if (cp >= 0xE0) { cp = ((cp & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F); s += 2; }
                else if (cp >= 0xC0) { cp = ((cp & 0x1F) << 6) | (s[0] & 0x3F); s += 1; }
                lv_font_glyph_dsc_t g;
                if (cp != '\n' && cp != '\r' && !lv_font_get_glyph_dsc(font, &g, cp, 0)) {
                    printf("FIT FAIL scene %zu (%s): \"%s\" has no glyph U+%04X in its font\n", scene, NAMES[scene], text,
                           (unsigned)cp);
                    failures++;
                }
            }
            if (p.x - 1 > lv_obj_get_width(c)) {
                printf("FIT FAIL scene %zu (%s): \"%s\" needs %d px, label is %d px (letter space %d)\n", scene,
                       NAMES[scene], text, (int)p.x - 1, (int)lv_obj_get_width(c),
                       (int)lv_obj_get_style_text_letter_space(c, 0));
                failures++;
            }
        }
        check_fit(c, scene);
    }
}

void pet_preview_apply(size_t scene)
{
    fresh();
    switch (scene) {
    case 168:case 169:case 170:case 171:case 172:
        select_nav(NAV_BATTLE);press(IN_OK);
        if(scene>=169){press(IN_OK);wait_ms(APP_BATTLE_HIT_MS+APP_BATTLE_RECOVERY_MS);}
        if(scene==170 || scene==171)press(IN_OK);
        if(scene==171)wait_ms(APP_BATTLE_HIT_MS+APP_BATTLE_RECOVERY_MS);
        if(scene==172)press(IN_UP);
        break;
    case 157: case 158: case 159: case 160: case 161: case 162: case 163: case 164:
    case 165: case 166: case 167:
        select_nav(NAV_CALENDAR);press(IN_OK);press(IN_OK);
        if(scene<=165)wait_ms((uint32_t)(scene-157)*200U);
        if(scene==166){wait_ms(400);press(IN_OK);wait_ms(200);}
        if(scene==167){wait_ms(400);press(IN_HOLD_OK);press(IN_OK);}
        break;
    case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156:
        select_nav(NAV_BATTLE); press(IN_HOLD_OK); wait_ms((uint32_t)(scene-148)*100U); break;
    case 147:
        select_nav(NAV_BATTLE); m.path_energy=UINT32_MAX; break;
    case 143: case 144: case 145: case 146:
        select_nav(NAV_BATTLE);
        if (scene == 146) { press(IN_DOWN); break; }
        if (scene == 143) { m.path_energy=1; press(IN_OK); press(IN_OK); wait_ms(1000); }
        else { press(IN_HOLD_OK); wait_ms(scene == 144 ? 800 : 2000); }
        break;
    case 129: case 130: case 131: case 132: case 133: case 134: case 135:
    case 136: case 137: case 138: case 139: case 140: case 141: case 142:
        select_nav(NAV_BATTLE);
        m.path_energy = 1;
        if (scene >= 130 && scene <= 135) press(IN_OK);
        if (scene >= 131 && scene <= 135) press(IN_OK);
        if (scene == 132) wait_ms(APP_BATTLE_HIT_MS-1);
        if (scene >= 133 && scene <= 135) wait_ms(APP_BATTLE_HIT_MS);
        if (scene == 134) wait_ms(APP_BATTLE_RECOVERY_MS);
        if (scene == 135) press(IN_UP);
        if (scene == 136 || scene == 137) press(IN_HOLD_OK);
        if (scene == 137) press(IN_OK);
        if (scene == 138) set_clock(3, 0);
        if (scene == 139) m.prefs.scene = SCENE_TRAIN;
        if (scene == 140) { m.prefs.scene=SCENE_TRAILBLAZE; m.prefs.trailblaze_index=0; }
        if (scene == 141) m.path_energy=UINT32_MAX;
        if (scene == 141) press(IN_OK);
        if (scene == 142) { press(IN_OK); press(IN_OK); app_model_energy_saved(&m, false); }
        break;
    case 128:
        select_nav(NAV_SETTINGS); press(IN_OK); press(IN_DOWN); press(IN_OK);
        break;
    case 116: case 117: case 118: case 119: case 120: case 121:
    case 122: case 123: case 124: case 125: case 126: case 127:
        m.prefs.scene = SCENE_TRAILBLAZE;
        m.prefs.trailblaze_chosen = 1;
        m.prefs.trailblaze_day = m.day;
        m.prefs.trailblaze_index = (uint8_t)(scene - 116);
        break;
    case 115:
        select_nav(NAV_SETTINGS); press(IN_OK); press(IN_UP); press(IN_OK);
        press(IN_DOWN); press(IN_DOWN);
        break;
    case 109: set_clock(9,0); break;
    case 110: set_clock(13,0); break;
    case 111: set_clock(21,0); break;
    case 112: press(IN_OK); press(IN_OK); break;
    case 113: case 114:
        press(IN_OK); press(IN_OK); wait_ms(APP_FOCUS_COUNTDOWN_MS);
        if (scene==114) { press(IN_OK); press(IN_OK); wait_ms(APP_FOCUS_COUNTDOWN_MS); }
        break;
    case 0: break;
    case 1: set_clock(18, 20); press(IN_DOWN); break;
    case 2: set_clock(21, 40); select_nav(NAV_SETTINGS); break;
    case 3: set_clock(1, 10); select_nav(NAV_PLAY); break;
    case 4: m.clock_set = false; hold_motion(COMPANION_MOTION_GREETING, 600); status.battery = -1; break;
    case 5: press(IN_OK); break;
    case 6: press(IN_OK); press(IN_DOWN); break;
    case 7: press(IN_OK); press(IN_OK); wait_ms(2000); hold_motion(COMPANION_MOTION_FOCUS, 700); break;
    case 8: press(IN_OK); press(IN_OK); wait_ms(2000); press(IN_OK); break;
    case 9: press(IN_OK); press(IN_OK); wait_ms(65000); press(IN_UP); break;
    case 10: press(IN_OK); press(IN_OK); wait_ms(APP_FOCUS_COUNTDOWN_MS); hold_motion(COMPANION_MOTION_CELEBRATION, 200); break;
    case 11: press(IN_OK); press(IN_OK); wait_ms(5 * 60000 + 3000); press(IN_HOLD_OK); break;
    case 12: select_nav(NAV_PLAY); press(IN_OK); press(IN_OK); wait_ms(400); break;
    case 13: select_nav(NAV_PLAY); press(IN_OK); press(IN_DOWN); press(IN_DOWN); press(IN_OK); wait_ms(200); break;
    case 14: press(IN_DOWN); press(IN_OK); break;
    case 15: press(IN_DOWN); press(IN_OK); for (int i = 0; i < 17; i++) press(IN_UP); break;  // into September
    case 16: press(IN_DOWN); press(IN_OK); press(IN_OK); wait_ms(3000); break;
    case 17: m.date_known = false; press(IN_DOWN); press(IN_OK); break;
    case 182: case 183: case 184:
        select_nav(NAV_SETTINGS);press(IN_OK);
        m.prefs.scene=scene==182 ? SCENE_ROOM : SCENE_TRAILBLAZE;
        if (scene!=184) press(IN_UP);
        break;
    case 18: select_nav(NAV_SETTINGS); press(IN_OK); break;
    case 19: select_nav(NAV_SETTINGS); press(IN_OK); press(IN_DOWN); press(IN_DOWN); break;
    case 20: select_nav(NAV_SETTINGS); press(IN_OK); press(IN_OK); press(IN_UP); break;
    case 21: status.audio_ok = false; select_nav(NAV_SETTINGS); press(IN_OK); press(IN_DOWN); press(IN_OK);
             for (int i = 0; i < 7; i++) press(IN_UP);
             break;
    case 22: select_nav(NAV_SETTINGS); press(IN_OK); press(IN_DOWN); press(IN_DOWN); press(IN_OK); break;
    case 23: case 34: case 35:
        memset(&net, 0, sizeof(net));
        net.phase = scene == 35 ? NET_PHASE_JOIN_FAILED : NET_PHASE_PORTAL;
        net.phone_joined = scene != 23;
        strcpy(net.ap_ssid, "Stelle-3F2A");
        strcpy(net.ap_pass, "40718263");
        snprintf(net.ap_qr, sizeof(net.ap_qr), "WIFI:T:WPA;S:%s;P:%s;;", net.ap_ssid, net.ap_pass);
        strcpy(net.sta_ssid, "STARNET");
        strcpy(net.message, "Wrong password. Try again.");
        status.net = &net;
        m.clock_set = false;
        app_model_begin_setup(&m, scene == 35);
        break;
    case 36:
        memset(&net, 0, sizeof(net));
        net.has_credentials = true;
        net.phase = NET_PHASE_SYNCED;
        status.net = &net;
        select_nav(NAV_SETTINGS); press(IN_OK); press(IN_UP); press(IN_UP);
        break;
    case 37: m.clock_set = false; m.time_sync = SYNC_RUNNING; break;
    case 38: set_clock(21, 0); select_nav(NAV_SETTINGS); press(IN_OK); press(IN_UP); press(IN_OK); press(IN_DOWN); break;
    case 39: press(IN_OK); press(IN_OK); wait_ms(8000); break;
    case 40: set_clock(7, 59); break;  // still asleep in the box until 8:00
    case 62: case 63: case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72: {
        static const uint32_t times[]={0,100,200,300,400,500,600,700,800,800,800};
        select_nav(NAV_PLAY); press(IN_OK);
        press(IN_DOWN); press(IN_DOWN); press(IN_DOWN); press(IN_OK);
        wait_ms(times[scene-62]);
        break;
    }
    case 173: case 174: case 175: case 176: case 177: case 178: case 179: case 180: case 181:
        if (scene==176) {
            m.page=PAGE_FOCUS;m.focus.state=FOCUS_RUNNING;
            companion_model_play_hold(&m.companion,COMPANION_MOTION_FOCUS,now);
        }
        if (scene==177) { m.page=PAGE_CHAT;m.nav=NAV_CHAT; }
        if (scene==178) { m.page=PAGE_BATTLE;m.nav=NAV_BATTLE;m.battle_stage=BATTLE_ENCOUNTER; }
        if (scene==179) m.page=PAGE_SETTINGS;
        if (scene==180) {
            m.page=PAGE_CHAT_HISTORY;
            chat_history_add(&m.chat_history,true,"History stays open after waking.");
        }
        if (scene>=175) { m.screen_off=true;press(IN_OK); }
        app_model_display_ready(&m,now);
        wait_ms(scene==173 ? 0 : scene==181 ? 1000 : 400);
        break;
    case 106: case 107: case 108: {
        static chat_entry_t saved[CHAT_HISTORY_SAVED_COUNT];
        const chat_entry_t *refs[CHAT_HISTORY_SAVED_COUNT];
        for(unsigned i=0;i<CHAT_HISTORY_SAVED_COUNT;i++) {
            saved[i].assistant=i%2;
            snprintf(saved[i].text,sizeof(saved[i].text),"Saved message %u\nRestored after reboot.\nA quiet little moment.",i+1);
            refs[i]=&saved[i];
        }
        select_nav(NAV_CHAT); press(IN_HOLD_OK);
        chat_history_set_saved(&m.chat_history,refs,CHAT_HISTORY_SAVED_COUNT);
        unsigned bottom=chat_history_max_scroll(&m.chat_history);
        m.chat_history_scroll=scene==106 ? 0 : scene==107 ? (uint16_t)(bottom/2) : (uint16_t)bottom;
        break;
    }
    case 101: case 102: case 103: case 104: case 105:
        now=440000U+3600U+(scene-101)*200U; set_clock(3,0); break;
    case 99: case 100: {
        static chat_service_status_t failure;
        select_nav(NAV_CHAT); press(IN_OK);
        memset(&failure,0,sizeof(failure)); failure.phase=CHAT_ERROR;
        snprintf(failure.hint,sizeof(failure.hint),"%s", scene==99 ? "Audio unavailable / OK: retry" : "Connection failed / OK: retry");
        m.chat.phase=CHAT_ERROR; status.chat=&failure; break;
    }
    case 73: now=444400U; set_clock(3,0); break;
    case 74: case 75: case 76: case 77: case 78: case 79: {
        static const uint32_t times[]={0,2500,2600,2700,2800,2900};
        now=290000U+times[scene-74]; set_clock(10,30); break;
    }
    case 80: case 81: case 82: case 83: case 84:
        hold_motion(COMPANION_MOTION_GREETING,(scene-80)*200U); break;
    case 85: case 86: case 87: case 88: case 89: case 90: case 91: case 92: case 93:
        hold_motion(COMPANION_MOTION_FOCUS,(scene-85)*200U); break;
    case 94: case 95: case 96: case 97: case 98:
        hold_motion(COMPANION_MOTION_CELEBRATION,(scene-94)*200U); break;
    case 58: case 59: case 60: case 61: {
        static const uint32_t times[]={0,3000,3200,3400};
        now=440000U+times[scene-58];
        set_clock(3,0);
        break;
    }
    case 51: case 52: case 53: case 54: case 55: case 56: case 57:
        select_nav(NAV_CHAT); press(IN_OK);
        for (unsigned i=0;i<CHAT_HISTORY_RAM_COUNT;i++) {
            char message[CHAT_TEXT_BYTES];
            if (scene>=54 && scene<=56) { memset(message,'x',sizeof(message)-1); message[sizeof(message)-1]=0; }
            else snprintf(message,sizeof(message),"Message %u\nA quiet little moment.\nLet's keep moving together.",i+1);
            chat_history_add(&m.chat_history,i%2!=0,message);
        }
        m.chat.phase=CHAT_OFF; press(IN_HOLD_OK);
        if (scene==51 || scene==55) m.chat_history_scroll=0;
        if (scene==52) m.chat_history_scroll=(uint16_t)(chat_history_max_scroll(&m.chat_history)/2);
        if (scene==56) { for (unsigned i=0;i<9;i++) press(IN_UP); }
        if (scene==57) { m.chat_history_scroll=0; press(IN_DOWN); }
        break;
    case 41: select_nav(NAV_CHAT); set_clock(1, 0); break;
    case 42: case 43: case 44: case 45: case 46: case 47: case 48: case 49: case 50:
        select_nav(NAV_CHAT); press(IN_OK);
        if (scene == 43 || scene == 49) m.chat.phase = CHAT_RECORDING;
        if (scene == 44) m.chat.phase = CHAT_PLAYING;
        if (scene == 45) m.chat.phase = CHAT_WAITING;
        if (scene == 46) press(IN_HOLD_OK);
        if (scene == 47) {
            chat_history_add(&m.chat_history, false, "\xe4\xbd\xa0\xe5\xa5\xbd\xef\xbc\x8c\xe6\x98\x9f\xe5\xae\x9d\xe3\x80\x82");
            chat_history_add(&m.chat_history, true, "\xe5\xbc\x80\xe6\x8b\x93\xe8\x80\x85\xef\xbc\x8c\xe4\xbb\x8a\xe5\xa4\xa9\xe4\xb9\x9f\xe4\xb8\x80\xe8\xb5\xb7\xe5\x89\x8d\xe8\xbf\x9b\xe5\x90\xa7\xef\xbc\x81\nFocus, then a quiet little moment.");
            m.chat.phase=CHAT_OFF;
            press(IN_HOLD_OK);
        }
        if (scene == 48) {
            static chat_service_status_t binding = {.phase=CHAT_BINDING,.code="123456",.hint="Bind at xiaozhi.me"};
            m.chat.phase=CHAT_BINDING; status.chat=&binding;
        }
        if (scene == 49) m.prefs.scene=SCENE_TRAIN;
        if (scene == 50) {
            chat_history_add(&m.chat_history, false, "\xe4\xbd\xa0\xe5\xa5\xbd\xef\xbc\x8c\xe6\x98\x9f\xe5\xae\x9d\xe3\x80\x82\nHow was your day?");
            m.chat.phase=CHAT_OFF;
            press(IN_HOLD_OK);
        }
        break;
    case 24: status.battery = 12; status.storage_ok = false; set_clock(16, 5); break;
    case 25: set_clock(22, 15); press(IN_OK); press(IN_OK); wait_ms(10 * 60000); press(IN_DOWN); hold_motion(COMPANION_MOTION_IDLE, 0); break;
    case 26:
        select_nav(NAV_PLAY); press(IN_OK);
        press(IN_DOWN); press(IN_DOWN); press(IN_DOWN); press(IN_OK);
        wait_ms(75);
        break;
    case 27:
        select_nav(NAV_PLAY); press(IN_OK);
        press(IN_DOWN); press(IN_DOWN); press(IN_DOWN); press(IN_OK);
        wait_ms(370);
        break;
    case 28: status.battery = 64; status.charging = true; break;
    case 29: status.battery = 9; status.charging = true; set_clock(20, 45); break;
    case 30: m.prefs.scene = SCENE_TRAIN; break;
    case 31: m.prefs.scene = SCENE_TRAIN; set_clock(18, 30); select_nav(NAV_PLAY); press(IN_OK); press(IN_DOWN); press(IN_DOWN); press(IN_OK); wait_ms(200); break;
    case 32: m.prefs.scene = SCENE_TRAIN; set_clock(2, 0); break;
    case 33: select_nav(NAV_SETTINGS); press(IN_OK); press(IN_UP); press(IN_OK); break;
    }
    (void)app_model_take_events(&m);
    pet_view_update(&m, &status, now);
    lv_obj_update_layout(screen);
    check_fit(screen, scene);
    static size_t applied;
    lv_mem_monitor_t now_mon;
    lv_mem_monitor(&now_mon);
    if (applied++ < SCENES)
        printf("scene-%03zu %s page=%d pool_used=%lu\n", scene, NAMES[scene], (int)m.page,
               (unsigned long)(now_mon.total_size - now_mon.free_size));
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    if (applied <= SCENES && mon.max_used > peak) peak = mon.max_used;
    if (applied == SCENES) printf("LVGL pool: %lu bytes total, peak used %lu bytes (%lu%%)\n", (unsigned long)mon.total_size,
                                  (unsigned long)peak, (unsigned long)(peak * 100U / mon.total_size));
    if (applied == SCENES && failures) {
        printf("%d layout fit failures\n", failures);
        exit(3);
    }
}

void pet_preview_destroy(void)
{
    pet_view_destroy();
    screen = NULL;
}
