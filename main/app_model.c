#include "app_model.h"

#include <stddef.h>
#include <string.h>

static const uint32_t SCREEN_OFF_MS[] = {0, 30000, 60000, 300000, 600000};
#define SCREEN_OFF_COUNT (sizeof(SCREEN_OFF_MS) / sizeof(SCREEN_OFF_MS[0]))

uint32_t app_screen_off_ms(uint8_t index) { return index < SCREEN_OFF_COUNT ? SCREEN_OFF_MS[index] : 0; }
unsigned app_screen_off_count(void) { return SCREEN_OFF_COUNT; }

void app_prefs_default(app_prefs_t *p)
{
    memset(p, 0, sizeof(*p));
    p->version = APP_PREFS_VERSION;
    p->brightness = 70;
    p->volume = 40;
    p->screen_off = 0;  // Never until the user picks a timeout
    p->scene = SCENE_ROOM;
}

bool app_prefs_valid(const app_prefs_t *p)
{
    if (p->version != APP_PREFS_VERSION) return false;
    if (p->brightness < 10 || p->brightness > 100 || p->brightness % 10) return false;
    if (p->volume > 100 || p->volume % 10) return false;
    if (p->screen_off >= SCREEN_OFF_COUNT || p->scene >= SCENE_COUNT) return false;
    if (p->trailblaze_index >= APP_TRAILBLAZE_COUNT || p->trailblaze_chosen > 1 || p->trailblaze_day < 0) return false;
    return true;
}

bool app_prefs_from_blob(const uint8_t *raw, size_t size, app_prefs_t *out)
{
    if (!raw || size < 5) return false;
    app_prefs_t p;
    app_prefs_default(&p);
    unsigned version = raw[0] | (unsigned)raw[1] << 8;
    p.brightness = raw[2];
    p.volume = raw[3];
    p.screen_off = raw[4];
    // v2/v3: u16 version, brightness, volume, screen_off, todo_count, 8 x {preset, done}
    // (22 bytes), then v3 added scene + reserved (24 bytes). v4 dropped the list.
    if (version == 2 && size == 22) {
        p.scene = SCENE_ROOM;
    } else if (version == 3 && size == 24) {
        p.scene = raw[22];
    } else if (version == 4 && size == 8) {
        p.scene = raw[5];
    } else if (version == APP_PREFS_VERSION && size == sizeof(app_prefs_t)) {
        memcpy(&p, raw, sizeof(p));
    } else {
        return false;
    }
    p.version = APP_PREFS_VERSION;
    if (!app_prefs_valid(&p)) return false;
    *out = p;
    return true;
}
void app_model_init(app_model_t *m, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    m->nav=NAV_FOCUS; // preserve the existing boot selection after reordering
    app_model_seed_scenes(m, now);
    m->clock_s = 12 * 3600;
    m->clock_ms = m->last_input_ms = now;
    app_prefs_default(&m->prefs);
    m->battle_storage_ready = true;
    focus_log_init(&m->log);
    companion_model_init(&m->companion);
    chat_model_init(&m->chat);
    chat_history_init(&m->chat_history);
    companion_model_play(&m->companion, COMPANION_MOTION_GREETING, 1, now);
}

void app_model_seed_scenes(app_model_t *m, uint32_t seed)
{
    m->scene_rng = seed ? seed : 0x9E3779B9U;
}

void app_model_display_ready(app_model_t *m,uint32_t now)
{
    m->display_greeting=!m->screen_off && app_page_shows_room(m->page);
    m->display_greeting_ms=now;
}
bool app_model_greeting_visible(const app_model_t *m,uint32_t now)
{
    return m->display_greeting && !m->screen_off && app_page_shows_room(m->page) &&
        (uint32_t)(now-m->display_greeting_ms)<companion_motion_duration_ms(COMPANION_MOTION_GREETING);
}

static void trailblaze_choose(app_model_t *m)
{
    uint32_t r = m->scene_rng;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    m->scene_rng = r;
    unsigned choices = APP_TRAILBLAZE_COUNT - (m->prefs.trailblaze_chosen ? 1U : 0U);
    unsigned next = r % choices;
    if (m->prefs.trailblaze_chosen && next >= m->prefs.trailblaze_index) next++;
    m->prefs.trailblaze_index = (uint8_t)next;
    m->prefs.trailblaze_chosen = 1;
    m->prefs.trailblaze_day = m->date_known ? m->day : 0;
    m->events |= APP_EV_PREFS_CHANGED;
}

static void trailblaze_sync_day(app_model_t *m)
{
    if (m->prefs.scene != SCENE_TRAILBLAZE || !m->date_known) return;
    if (!m->prefs.trailblaze_chosen ||
        (m->prefs.trailblaze_day && m->day > m->prefs.trailblaze_day)) {
        trailblaze_choose(m);
    } else if (!m->prefs.trailblaze_day) {
        // A choice made before time sync already counts as today's choice.
        m->prefs.trailblaze_day = m->day;
        m->events |= APP_EV_PREFS_CHANGED;
    }
}

void app_model_load_prefs(app_model_t *m, const app_prefs_t *p)
{
    if (app_prefs_valid(p)) m->prefs = *p;
}

void app_model_load_log(app_model_t *m, const focus_log_t *log)
{
    if (focus_log_valid(log)) m->log = *log;
}

void app_model_energy_saved(app_model_t *m, bool success)
{
    m->battle_save_failed = !success;
    if (!success) {
        m->path_energy = m->energy_before;
        if (m->battle_stage == BATTLE_HIT) m->battle_stage = BATTLE_ENCOUNTER;
        m->events &= ~APP_EV_BATTLE_HIT;
    }
}

/* ---------- per-day focus log ---------- */

static void log_changed(app_model_t *m)
{
    m->events |= APP_EV_LOG_CHANGED;
    m->log_save_ms = 0;
}

// Running time is logged as it passes, so stopping or resetting a session
// keeps whatever was focused before it ended.
static void log_focus_ms(app_model_t *m, uint32_t ms)
{
    m->log_ms += ms;
    uint32_t secs = m->log_ms / 1000U;
    m->log_ms %= 1000U;
    if (secs) {
        if (m->date_known) focus_log_add(&m->log, m->day, secs, 0);
        else m->pend_secs += secs;
    }
    m->log_save_ms += ms;
    if (m->log_save_ms >= APP_LOG_SAVE_MS) log_changed(m);
}

static void log_session_start(app_model_t *m)
{
    if (m->date_known) focus_log_add(&m->log, m->day, 0, 1);
    else if (m->pend_sessions < 0xFFFFU) m->pend_sessions++;
    log_changed(m);
}

focus_day_t app_focus_on_day(const app_model_t *m, int32_t day)
{
    return focus_log_get(&m->log, day);
}

uint64_t app_since_hsr_launch_s(const app_model_t *m)
{
    if (!m->date_known || !m->clock_set) return 0;
    int64_t launch = (int64_t)date_to_day(APP_HSR_LAUNCH_YEAR, APP_HSR_LAUNCH_MONTH, APP_HSR_LAUNCH_DAY) * 86400 +
                     APP_HSR_LAUNCH_SECOND;
    int64_t now = (int64_t)m->day * 86400 + m->clock_s;
    return now > launch ? (uint64_t)(now - launch) : 0;
}

bool app_page_shows_room(app_page_t page)
{
    return page == PAGE_HOME || page == PAGE_FOCUS || page == PAGE_PLAY || page == PAGE_CHAT || page == PAGE_BATTLE;
}

bool app_focus_active(const app_model_t *m)
{
    return m->focus.state == FOCUS_RUNNING || m->focus.state == FOCUS_PAUSED;
}

uint32_t app_focus_display_s(const app_model_t *m)
{
    const app_focus_t *f = &m->focus;
    if (f->mode == FOCUS_COUNTDOWN) {
        uint32_t left = f->elapsed_ms >= APP_FOCUS_COUNTDOWN_MS ? 0 : APP_FOCUS_COUNTDOWN_MS - f->elapsed_ms;
        return (left + 999U) / 1000U;  // never show 00:00 before the deadline
    }
    return f->elapsed_ms / 1000U;
}

uint8_t app_backlight_percent(const app_model_t *m)
{
    if (m->screen_off) return 0;
    if (m->page == PAGE_ADJUST && m->setting_sel == SET_BRIGHTNESS) return m->draft;
    return m->prefs.brightness;
}

uint8_t app_volume_percent(const app_model_t *m)
{
    if (m->page == PAGE_ANNIV) return m->anniv_volume;
    if (m->page == PAGE_ADJUST && m->setting_sel == SET_VOLUME) return m->draft;
    return m->prefs.volume;
}

room_phase_t app_room_phase(const app_model_t *m)
{
    if (!m->clock_set) return ROOM_DAY;
    unsigned h = m->clock_s / 3600U;
    if (h >= 6 && h < 17) return ROOM_DAY;
    if (h >= 17 && h < 20) return ROOM_DUSK;
    if (h >= 20 && h < 23) return ROOM_NIGHT;
    return ROOM_LATE;
}

bool app_play_in_focus(const app_model_t *m)
{
    return m->page == PAGE_PLAY || (m->page == PAGE_HOME && m->nav == NAV_PLAY);
}

bool app_companion_asleep(const app_model_t *m)
{
    // Play is highlighted in the nav (or open): she stands up, ready to play.
    if (app_play_in_focus(m) || app_chat_in_focus(m) || m->nav == NAV_BATTLE || m->page == PAGE_BATTLE) return false;
    return m->clock_set && m->clock_s / 3600U < APP_SLEEP_END_HOUR &&
           m->companion.motion == COMPANION_MOTION_IDLE;
}

bool app_chat_in_focus(const app_model_t *m)
{
    return m->page == PAGE_CHAT || (m->page == PAGE_HOME && m->nav == NAV_CHAT);
}

static void log_changed(app_model_t *m);

static void focus_reset(app_model_t *m)
{
    if (m->focus.state != FOCUS_READY) log_changed(m);  // a stopped session keeps its time
    m->focus.state = FOCUS_READY;
    m->focus.confirm = CONFIRM_NONE;
    m->focus.elapsed_ms = 0;
}

static void focus_finish(app_model_t *m, uint32_t now)
{
    if (m->date_known) {
        focus_log_add_completions(&m->log, m->day, 1);
        m->focus.repeated_completion = focus_log_get(&m->log, m->day).completed > 1;
    } else {
        if (m->pend_completions < UINT16_MAX) m->pend_completions++;
        m->focus.repeated_completion = m->pend_completions > 1;
    }
    m->focus.state = FOCUS_DONE;
    m->focus.confirm = CONFIRM_NONE;
    m->events |= APP_EV_FOCUS_DONE;
    log_changed(m);
    companion_model_play(&m->companion, COMPANION_MOTION_CELEBRATION, 4, now);
    // Completion wakes the screen and is shown in the room, even from Home.
    m->screen_off = false;
    m->last_input_ms = now;
    if (app_page_shows_room(m->page) && m->page != PAGE_BATTLE) {
        m->page = PAGE_FOCUS;
        m->nav = NAV_FOCUS;
        m->nav_first = 0;
    }
}

void app_model_tick(app_model_t *m, uint32_t now)
{
    if (m->display_greeting && !app_model_greeting_visible(m,now)) m->display_greeting=false;
    uint32_t whole = (now - m->clock_ms) / 1000U;
    if (whole) {
        uint32_t total = m->clock_s + whole;
        if (m->date_known) m->day += (int32_t)(total / 86400U);  // past midnight: a new day
        m->clock_s = total % 86400U;
        m->clock_ms += whole * 1000U;
    }
    trailblaze_sync_day(m);
    if (m->page == PAGE_BATTLE && (m->battle_stage == BATTLE_HIT || m->battle_stage == BATTLE_RECOVER)) {
        uint32_t elapsed = now - m->battle_started_ms;
        if (elapsed >= APP_BATTLE_HIT_MS + APP_BATTLE_RECOVERY_MS) {
            m->battle_stage = BATTLE_ENCOUNTER;
            m->battle_enemy ^= 1U;
        }
        else if (elapsed >= APP_BATTLE_HIT_MS) m->battle_stage = BATTLE_RECOVER;
    }

    app_focus_t *f = &m->focus;
    if (f->state == FOCUS_RUNNING) {
        uint32_t limit = f->mode == FOCUS_COUNTDOWN ? APP_FOCUS_COUNTDOWN_MS : APP_FOCUS_COUNTUP_MAX_MS;
        uint32_t step = now - f->last_ms;
        f->last_ms = now;
        if (step >= limit - f->elapsed_ms) {
            log_focus_ms(m, limit - f->elapsed_ms);
            f->elapsed_ms = limit;
            focus_finish(m, now);
        } else {
            f->elapsed_ms += step;
            log_focus_ms(m, step);
        }
    }
    if (f->confirm != CONFIRM_NONE && now - f->confirm_ms >= APP_CONFIRM_TIMEOUT_MS) f->confirm = CONFIRM_NONE;

    if (m->battle_idle) {
        if (m->page != PAGE_HOME || m->nav != NAV_BATTLE) {
            m->battle_idle = false;
            if (m->companion.motion == COMPANION_MOTION_BAT_SWING)
                companion_model_play(&m->companion, COMPANION_MOTION_IDLE, 0, now);
        } else if (!m->screen_off && now - m->battle_idle_ms >= 2000U) {
            m->battle_idle_ms += (now - m->battle_idle_ms) / 2000U * 2000U;
            companion_model_play(&m->companion, COMPANION_MOTION_BAT_SWING, 1, m->battle_idle_ms);
        }
    }
    companion_model_tick(&m->companion, now);
    // While a session runs Stelle plays the focus clip once and holds its last
    // frame; pausing, stopping or resetting releases her back to idle.
    bool holding_focus = m->companion.motion == COMPANION_MOTION_FOCUS && m->companion.hold;
    if (f->state == FOCUS_RUNNING && m->companion.motion == COMPANION_MOTION_IDLE &&
        m->page != PAGE_BATTLE && m->nav != NAV_BATTLE)
        companion_model_play_hold(&m->companion, COMPANION_MOTION_FOCUS, now);
    else if (f->state != FOCUS_RUNNING && holding_focus)
        companion_model_play(&m->companion, COMPANION_MOTION_IDLE, 0, now);

    // Auto screen-off; the Wi-Fi setup page stays lit while the user is on the phone.
    uint32_t timeout = app_screen_off_ms(m->prefs.screen_off);
    if (timeout && !m->screen_off && m->page != PAGE_SETUP && now - m->last_input_ms >= timeout) m->screen_off = true;
    if (!app_play_in_focus(m)) m->play_said = false;  // e.g. focus completion moved to Focus
}

static unsigned wrap_prev(unsigned v, unsigned n) { return (v + n - 1) % n; }
static unsigned wrap_next(unsigned v, unsigned n) { return (v + 1) % n; }

static void prefs_changed(app_model_t *m) { m->events |= APP_EV_PREFS_CHANGED; }

// Keep the selected item inside the visible nav window. The window only moves
// when the selection steps past its first or last button.
static void nav_follow(app_model_t *m)
{
    if (m->nav < m->nav_first) m->nav_first = (uint8_t)m->nav;
    if ((unsigned)m->nav >= (unsigned)m->nav_first + APP_NAV_VISIBLE) m->nav_first = (uint8_t)(m->nav - (APP_NAV_VISIBLE - 1));
}

static void handle_chat(app_model_t *m, app_input_t in);

static void handle_home(app_model_t *m, app_input_t in, uint32_t now)
{
    static const app_page_t target[NAV_COUNT] = {
        [NAV_FOCUS]=PAGE_FOCUS,[NAV_CALENDAR]=PAGE_CALENDAR,[NAV_CHAT]=PAGE_CHAT,
        [NAV_PLAY]=PAGE_PLAY,[NAV_BATTLE]=PAGE_BATTLE,[NAV_SETTINGS]=PAGE_SETTINGS
    };
    (void)now;
    if (m->nav == NAV_CHAT && (in == IN_OK || in == IN_HOLD_OK)) {
        m->page = PAGE_CHAT;
        handle_chat(m, in);
        return;
    }
    if (in == IN_UP) m->nav = (app_nav_t)wrap_prev(m->nav, NAV_COUNT);
    else if (in == IN_DOWN) m->nav = (app_nav_t)wrap_next(m->nav, NAV_COUNT);
    else if (in == IN_OK) {
        m->page = target[m->nav];
        m->cal_day = m->day;  // the calendar opens on today
        m->setting_sel = 0;
    }
    nav_follow(m);
    if (m->battle_idle && m->nav != NAV_BATTLE) {
        m->battle_idle = false;
        companion_model_play(&m->companion, COMPANION_MOTION_IDLE, 0, now);
    }
    if (m->nav == NAV_BATTLE && (in == IN_UP || in == IN_DOWN || in == IN_OK)) {
        m->page = PAGE_BATTLE;
        m->battle_stage = BATTLE_READY;
        m->battle_enemy = 0;
        m->battle_idle = false;
        m->battle_has_hit = false;
        m->nav_first = NAV_PLAY; // Play / Battle / Settings, matching the approved preview
        companion_model_play(&m->companion, COMPANION_MOTION_IDLE, 0, now);
    }
}

static void handle_chat(app_model_t *m, app_input_t in)
{
    if (in == IN_OK) {
        chat_cmd_t cmd = chat_model_click(&m->chat);
        if (cmd == CHAT_CMD_START) m->events |= APP_EV_CHAT_START;
        else if (cmd == CHAT_CMD_STOP_RECORDING) m->events |= APP_EV_CHAT_STOP_RECORDING;
        else if (cmd == CHAT_CMD_CANCEL) m->events |= APP_EV_CHAT_CANCEL;
    } else if (in == IN_HOLD_OK) {
        if (!chat_model_active(&m->chat)) {
            m->page = PAGE_CHAT_HISTORY;
            m->chat_history_scroll = (uint16_t)chat_history_max_scroll(&m->chat_history);
        } else {
            m->events |= APP_EV_CHAT_CANCEL;
            chat_model_set_phase(&m->chat, CHAT_OFF);
        }
    } else if (in == IN_UP || in == IN_DOWN) {
        m->events |= APP_EV_CHAT_CANCEL;
        chat_model_set_phase(&m->chat, CHAT_OFF);
        m->page = PAGE_HOME;
    }
}

static void handle_chat_history(app_model_t *m, app_input_t in)
{
    if (in == IN_HOLD_OK) { m->page = PAGE_CHAT; return; }
    if (in == IN_OK || in == IN_HOLD_DOWN) {
        m->chat_history_scroll = (uint16_t)chat_history_max_scroll(&m->chat_history);
        return;
    }
    unsigned bottom = chat_history_max_scroll(&m->chat_history);
    unsigned pos = m->chat_history_scroll > bottom ? bottom : m->chat_history_scroll;
    if (in == IN_UP) {
        pos = pos > CHAT_HISTORY_LINE_PX ? pos-CHAT_HISTORY_LINE_PX : 0;
    } else if (in == IN_DOWN) {
        pos = pos+CHAT_HISTORY_LINE_PX < bottom ? pos+CHAT_HISTORY_LINE_PX : bottom;
    }
    m->chat_history_scroll = (uint16_t)pos;
}

static void handle_focus(app_model_t *m, app_input_t in, uint32_t now)
{
    app_focus_t *f = &m->focus;
    if (in == IN_HOLD_OK) {
        f->confirm = CONFIRM_NONE;
        m->page = PAGE_HOME;  // the session keeps running in the background
        return;
    }
    if (f->confirm != CONFIRM_NONE) {
        if (in == IN_OK) {
            bool keep = f->confirm == CONFIRM_STOP && f->mode == FOCUS_COUNTUP &&
                        f->elapsed_ms >= APP_FOCUS_MIN_RECORD_MS;
            if (keep) focus_finish(m, now);
            else focus_reset(m);
        } else if (in == IN_UP || in == IN_DOWN) {
            f->confirm = CONFIRM_NONE;
        }
        return;
    }
    switch (f->state) {
    case FOCUS_READY:
        if (in == IN_UP || in == IN_DOWN) f->mode = f->mode == FOCUS_COUNTDOWN ? FOCUS_COUNTUP : FOCUS_COUNTDOWN;
        if (in == IN_OK) {
            f->state = FOCUS_RUNNING;
            f->elapsed_ms = 0;
            f->last_ms = now;
            log_session_start(m);
            companion_model_play_hold(&m->companion, COMPANION_MOTION_FOCUS, now);
        }
        break;
    case FOCUS_RUNNING:
        if (in == IN_OK) {
            f->state = FOCUS_PAUSED;
            log_changed(m);
        }
        if (in == IN_UP) { f->confirm = CONFIRM_STOP; f->confirm_ms = now; }
        break;
    case FOCUS_PAUSED:
        if (in == IN_OK) {
            f->state = FOCUS_RUNNING;
            f->last_ms = now;
            companion_model_play_hold(&m->companion, COMPANION_MOTION_FOCUS, now);
        }
        if (in == IN_UP) { f->confirm = CONFIRM_RESET; f->confirm_ms = now; }
        break;
    case FOCUS_DONE:
        if (in == IN_OK) {
            focus_reset(m);
            companion_model_play(&m->companion, COMPANION_MOTION_IDLE, 0, now);
        }
        break;
    }
}

// Calendar: UP/DOWN step one day (within the logged year, never past today),
// hold UP jumps back to today, OK opens the anniversary page.
static void handle_calendar(app_model_t *m, app_input_t in, uint32_t now)
{
    if (in == IN_HOLD_OK) {
        m->page = PAGE_HOME;
        return;
    }
    if (in == IN_OK) {
        m->page = PAGE_ANNIV;
        m->anniv_paused = false;
        m->anniv_volume = m->prefs.volume;
        m->anniv_session++;
        m->anniv_entered_ms=now;
        return;
    }
    if (!m->date_known) return;
    int32_t oldest = m->day - (FOCUS_LOG_DAYS - 1);
    if (in == IN_UP && m->cal_day > oldest) m->cal_day--;
    else if (in == IN_DOWN && m->cal_day < m->day) m->cal_day++;
    else if (in == IN_HOLD_UP) m->cal_day = m->day;
}
static void handle_settings(app_model_t *m, app_input_t in)
{
    if (in == IN_UP) m->setting_sel = (uint8_t)wrap_prev(m->setting_sel, SET_COUNT);
    else if (in == IN_DOWN) m->setting_sel = (uint8_t)wrap_next(m->setting_sel, SET_COUNT);
    else if (in == IN_HOLD_OK) m->page = PAGE_HOME;
    else if (in == IN_OK) {
        if (m->setting_sel == SET_WIFI) {
            app_model_begin_setup(m, true);
            return;
        }
        if (m->setting_sel == SET_SCENE) {  // its own page with a live preview
            m->draft = m->prefs.scene;
            m->page = PAGE_SCENE;
            return;
        }
        m->draft = m->setting_sel == SET_BRIGHTNESS ? m->prefs.brightness
                 : m->setting_sel == SET_VOLUME     ? m->prefs.volume
                                                    : m->prefs.screen_off;
        m->page = PAGE_ADJUST;
    }
}

static void handle_adjust(app_model_t *m, app_input_t in)
{
    if (in == IN_HOLD_OK) {  // cancel: drop the draft, live preview reverts
        m->page = PAGE_SETTINGS;
        return;
    }
    if (in == IN_OK) {
        if (m->setting_sel == SET_BRIGHTNESS) m->prefs.brightness = m->draft;
        else if (m->setting_sel == SET_VOLUME) m->prefs.volume = m->draft;
        else m->prefs.screen_off = m->draft;
        prefs_changed(m);
        m->page = PAGE_SETTINGS;
        return;
    }
    if (in != IN_UP && in != IN_DOWN) return;
    bool up = in == IN_UP;
    if (m->setting_sel == SET_SCREEN_OFF) {
        if (up && m->draft + 1u < SCREEN_OFF_COUNT) m->draft++;
        if (!up && m->draft > 0) m->draft--;
        return;
    }
    int minimum = m->setting_sel == SET_BRIGHTNESS ? 10 : 0;
    int value = m->draft + (up ? 10 : -10);
    m->draft = (uint8_t)(value < minimum ? minimum : value > 100 ? 100 : value);
    if (m->setting_sel == SET_VOLUME) m->events |= APP_EV_VOLUME_PREVIEW;
}

static void handle_play(app_model_t *m, app_input_t in, uint32_t now)
{
    if (in == IN_UP) companion_model_select_previous(&m->companion);
    else if (in == IN_DOWN) companion_model_select_next(&m->companion);
    else if (in == IN_OK) {
        companion_model_play_selected(&m->companion, now);
        m->play_said = true;
        if (m->companion.last_action == COMPANION_ACTION_JUMP) m->events |= APP_EV_PLAY_JUMP;
        if (m->companion.last_action == COMPANION_ACTION_BAT_SWING) m->events |= APP_EV_PLAY_BAT;
    } else if (in == IN_HOLD_OK) m->page = PAGE_HOME;
}

static void key_sound_feedback(app_model_t *m, bool voice_was_active)
{
    if (m->page == PAGE_ANNIV || !app_volume_percent(m) || voice_was_active || chat_model_active(&m->chat) ||
        (m->events & (APP_EV_VOLUME_PREVIEW | APP_EV_FOCUS_DONE | APP_EV_PLAY_JUMP | APP_EV_PLAY_BAT | APP_EV_BATTLE_HIT))) {
        // Also discard feedback from earlier keys batched before a Chat start.
        m->events &= ~APP_EV_KEY_SOUND;
    } else m->events |= APP_EV_KEY_SOUND;
}

void app_model_handle(app_model_t *m, app_input_t in, uint32_t now)
{
    bool battle_locked = m->page == PAGE_BATTLE && m->battle_stage == BATTLE_HIT &&
                         now - m->battle_started_ms < APP_BATTLE_HIT_MS;
    focus_state_t before = m->focus.state;
    bool voice_was_active = chat_model_active(&m->chat);
    app_model_tick(m, now);  // settle deadlines before a last-second pause/stop
    // Inputs received during the hit are discarded even if this tick reaches
    // the recovery deadline. Nothing is queued into the next encounter.
    if (battle_locked) return;
    m->last_input_ms = now;
    // A press that lands as the session completes was meant for the running
    // timer; do not let it act on the completion screen ("OK: again").
    if (before == FOCUS_RUNNING && m->focus.state == FOCUS_DONE && m->page != PAGE_BATTLE) {
        m->screen_off = false;
        key_sound_feedback(m, voice_was_active);
        return;
    }
    if (m->screen_off) {  // the waking press only wakes
        m->screen_off = false;
        key_sound_feedback(m, voice_was_active);
        return;
    }
    m->display_greeting=false; // a deliberate action takes priority over the greeting
    if (m->page == PAGE_BATTLE &&
        (in == IN_HOLD_DOWN || in == IN_HOLD_UP || (in == IN_DOWN && m->battle_stage != BATTLE_READY))) return;
    switch (m->page) {
    case PAGE_HOME: handle_home(m, in, now); break;
    case PAGE_FOCUS: handle_focus(m, in, now); break;
    case PAGE_PLAY: handle_play(m, in, now); break;
    case PAGE_BATTLE:
        if (m->battle_stage == BATTLE_READY && (in == IN_UP || in == IN_DOWN)) {
            m->page = PAGE_HOME;
            handle_home(m, in, now);
            break;
        }
        if (in == IN_HOLD_OK) {
            m->page = PAGE_HOME;
            m->battle_idle = true;
            m->battle_idle_ms = now;
            companion_model_play(&m->companion, COMPANION_MOTION_BAT_SWING, 1, now);
        } else if (in == IN_OK) {
            if (m->battle_stage == BATTLE_ENCOUNTER) {
                if (m->battle_storage_ready && m->path_energy < UINT32_MAX) {
                    if (!(m->events & APP_EV_ENERGY_CHANGED)) m->energy_before = m->path_energy;
                    m->path_energy++;
                    m->battle_stage = BATTLE_HIT;
                    m->battle_started_ms = now;
                    m->battle_has_hit = true;
                    m->events |= APP_EV_ENERGY_CHANGED | APP_EV_BATTLE_HIT;
                }
            } else if (m->battle_stage == BATTLE_READY) m->battle_stage = BATTLE_ENCOUNTER;
        } else if (in == IN_UP && m->battle_storage_ready) {
            if (!(m->events & APP_EV_ENERGY_CHANGED)) m->energy_before = m->path_energy;
            m->path_energy = 0;
            m->events |= APP_EV_ENERGY_CHANGED;
        }
        break;
    case PAGE_CHAT: handle_chat(m, in); break;
    case PAGE_CHAT_HISTORY: handle_chat_history(m, in); break;
    case PAGE_CALENDAR: handle_calendar(m, in, now); break;
    case PAGE_ANNIV:
        if (in == IN_OK) m->anniv_paused = !m->anniv_paused;
        else if (in == IN_HOLD_OK) m->page = PAGE_CALENDAR;
        else if (in == IN_UP && m->anniv_volume < 100) m->anniv_volume += 10;
        else if (in == IN_DOWN && m->anniv_volume >= 10) m->anniv_volume -= 10;
        break;
    case PAGE_SETTINGS: handle_settings(m, in); break;
    case PAGE_ADJUST: handle_adjust(m, in); break;
    case PAGE_SCENE:  // UP/DOWN preview; OK applies and returns Home; hold applies/back to Settings
        if (in == IN_UP || in == IN_DOWN) {
            m->draft = (uint8_t)((m->draft + (in == IN_UP ? SCENE_COUNT - 1u : 1u)) % SCENE_COUNT);
        } else if (in == IN_OK || in == IN_HOLD_OK) {
            if (m->prefs.scene != m->draft) {
                m->prefs.scene = m->draft;
                if (m->draft == SCENE_TRAILBLAZE) trailblaze_choose(m);
                prefs_changed(m);
            }
            m->page = in == IN_OK ? PAGE_HOME : PAGE_SETTINGS;
        }
        break;
    case PAGE_SETUP:  // the phone does the work; long-press OK leaves (skip / back)
        if (in == IN_HOLD_OK) m->page = m->setup_from_settings ? PAGE_SETTINGS : PAGE_HOME;
        break;
    }
    if (!app_play_in_focus(m)) m->play_said = false;  // leaving Play forgets the line
    key_sound_feedback(m, voice_was_active);
}

void app_model_handle_queued(app_model_t *m, app_input_t in, uint32_t received_ms, uint32_t now)
{
    if (m->page == PAGE_BATTLE && m->battle_has_hit) {
        uint32_t age = received_ms - m->battle_started_ms;
        if (age < APP_BATTLE_HIT_MS || (age < APP_BATTLE_HIT_MS + APP_BATTLE_RECOVERY_MS && in == IN_OK)) return;
    }
    app_model_handle(m, in, now);
}

void app_model_begin_setup(app_model_t *m, bool from_settings)
{
    m->page = PAGE_SETUP;
    m->setup_from_settings = from_settings;
    m->screen_off = false;
}

void app_model_setup_finished(app_model_t *m)
{
    if (m->page == PAGE_SETUP) m->page = m->setup_from_settings ? PAGE_SETTINGS : PAGE_HOME;
}

void app_model_set_time(app_model_t *m, uint32_t seconds_of_day, int32_t day, uint32_t now)
{
    bool first = !m->date_known;
    m->clock_s = seconds_of_day % 86400U;
    m->clock_ms = now;
    m->clock_set = true;
    m->date_known = true;
    m->day = day;
    trailblaze_sync_day(m);
    if (first) m->cal_day = day;
    if (m->pend_secs || m->pend_sessions || m->pend_completions) {  // unknown-date focus belongs to today
        focus_log_add(&m->log, day, m->pend_secs, m->pend_sessions);
        focus_log_add_completions(&m->log, day, m->pend_completions);
        if (m->pend_completions && m->focus.state == FOCUS_DONE)
            m->focus.repeated_completion = focus_log_get(&m->log, day).completed > 1;
        m->pend_secs = 0;
        m->pend_sessions = 0;
        m->pend_completions = 0;
        log_changed(m);
    }
}

uint32_t app_model_take_events(app_model_t *m)
{
    uint32_t e = m->events;
    m->events = 0;
    return e;
}
