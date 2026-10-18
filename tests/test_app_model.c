#include "app_model.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static app_model_t m;
static uint32_t t;

static void start(void)
{
    t = 1000;
    app_model_init(&m, t);
    m.prefs.screen_off = 0;  // most tests do not want auto screen-off
}
static void press(app_input_t in) { app_model_handle(&m, in, t); }
static void advance(uint32_t ms)
{
    // Step like the 30 ms platform loop so deadlines are crossed realistically.
    while (ms) {
        uint32_t step = ms < 30 ? ms : 30;
        t += step;
        ms -= step;
        app_model_tick(&m, t);
    }
}

static void test_defaults_and_nav(void)
{
    start();
    assert(m.page == PAGE_HOME && m.nav == NAV_FOCUS);
    assert(app_prefs_valid(&m.prefs));
    assert(m.companion.motion == COMPANION_MOTION_GREETING);
    press(IN_UP);
    assert(m.nav == NAV_SETTINGS);
    press(IN_DOWN);
    assert(m.nav == NAV_FOCUS);
    press(IN_DOWN);
    assert(m.nav == NAV_CALENDAR);
    press(IN_DOWN);
    assert(m.nav == NAV_CHAT);
    press(IN_UP);
    assert(m.nav == NAV_CALENDAR);
    press(IN_UP);
    assert(m.nav == NAV_FOCUS);
    press(IN_UP);
    assert(m.nav == NAV_SETTINGS);
    press(IN_OK);
    assert(m.page == PAGE_SETTINGS);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_HOME && m.nav == NAV_SETTINGS);
}

static void test_nav_window_scrolls_only_at_the_edge(void)
{
    start();
    assert(m.nav == NAV_FOCUS && m.nav_first == 0);
    press(IN_DOWN); assert(m.nav == NAV_CALENDAR && m.nav_first == 0);
    press(IN_DOWN); assert(m.nav == NAV_CHAT && m.nav_first == 0);
    press(IN_DOWN); assert(m.nav == NAV_PLAY && m.nav_first == 1);
    press(IN_DOWN); assert(m.nav == NAV_BATTLE && m.nav_first == 3);
    press(IN_HOLD_OK);
    press(IN_DOWN); assert(m.nav == NAV_SETTINGS && m.nav_first == 3);
    press(IN_UP); assert(m.nav == NAV_BATTLE && m.nav_first == 3);
    press(IN_HOLD_OK);
    press(IN_UP); assert(m.nav == NAV_PLAY && m.nav_first == 3);
    press(IN_UP); assert(m.nav == NAV_CHAT && m.nav_first == 2);
    press(IN_UP); assert(m.nav == NAV_CALENDAR && m.nav_first == 1);
    press(IN_UP); assert(m.nav == NAV_FOCUS && m.nav_first == 0);
    press(IN_UP); assert(m.nav == NAV_SETTINGS && m.nav_first == 3);
    press(IN_DOWN); assert(m.nav == NAV_FOCUS && m.nav_first == 0);
    press(IN_DOWN); assert(m.nav == NAV_CALENDAR && m.nav_first == 0);
}

static void test_countdown_completes_once(void)
{
    start();
    press(IN_OK);
    assert(m.page == PAGE_FOCUS && m.focus.state == FOCUS_READY && m.focus.mode == FOCUS_COUNTDOWN);
    assert(app_focus_display_s(&m) == 25 * 60);
    press(IN_OK);
    assert(m.focus.state == FOCUS_RUNNING);
    (void)app_model_take_events(&m);
    advance(APP_FOCUS_COUNTDOWN_MS - 1);
    assert(m.focus.state == FOCUS_RUNNING);
    assert(app_focus_display_s(&m) == 1);  // never 00:00 before the deadline
    advance(1);
    assert(m.focus.state == FOCUS_DONE && app_focus_display_s(&m) == 0);
    assert(app_model_take_events(&m) & APP_EV_FOCUS_DONE);
    assert(m.companion.motion == COMPANION_MOTION_CELEBRATION);
    advance(10000);
    assert(!(app_model_take_events(&m) & APP_EV_FOCUS_DONE));  // no double award
    press(IN_OK);
    assert(m.focus.state == FOCUS_READY && m.focus.elapsed_ms == 0);
}

static void test_last_second_pause_settles_deadline_first(void)
{
    start();
    press(IN_OK);
    press(IN_OK);
    t += APP_FOCUS_COUNTDOWN_MS + 20;  // the platform loop stalled past the deadline
    press(IN_OK);                       // a pause press arrives late
    assert(m.focus.state == FOCUS_DONE);
}

static void test_pause_freezes_and_resume_continues(void)
{
    start();
    press(IN_OK);
    press(IN_OK);
    advance(10000);
    press(IN_OK);
    assert(m.focus.state == FOCUS_PAUSED);
    uint32_t frozen = m.focus.elapsed_ms;
    advance(60000);
    assert(m.focus.elapsed_ms == frozen);
    press(IN_OK);
    assert(m.focus.state == FOCUS_RUNNING);
    advance(1000);
    assert(m.focus.elapsed_ms >= frozen + 990 && m.focus.elapsed_ms <= frozen + 1000);
}

static void test_stop_and_reset_confirmations(void)
{
    start();
    press(IN_OK);
    press(IN_OK);
    advance(3000);
    press(IN_UP);
    assert(m.focus.confirm == CONFIRM_STOP && m.focus.state == FOCUS_RUNNING);
    advance(APP_CONFIRM_TIMEOUT_MS);
    assert(m.focus.confirm == CONFIRM_NONE && m.focus.state == FOCUS_RUNNING);  // safe timeout
    press(IN_UP);
    press(IN_DOWN);
    assert(m.focus.confirm == CONFIRM_NONE && m.focus.state == FOCUS_RUNNING);  // cancel
    press(IN_UP);
    press(IN_OK);
    assert(m.focus.state == FOCUS_READY && m.focus.elapsed_ms == 0);  // countdown stop discards

    press(IN_OK);
    advance(2000);
    press(IN_OK);  // pause
    press(IN_UP);
    assert(m.focus.confirm == CONFIRM_RESET);
    press(IN_OK);
    assert(m.focus.state == FOCUS_READY);
}

static void test_count_up_stop_records_long_sessions(void)
{
    start();
    press(IN_OK);
    press(IN_DOWN);
    assert(m.focus.mode == FOCUS_COUNTUP && app_focus_display_s(&m) == 0);
    press(IN_OK);
    advance(30000);
    press(IN_UP);
    press(IN_OK);
    assert(m.focus.state == FOCUS_READY);  // under a minute: nothing to celebrate

    press(IN_OK);
    advance(APP_FOCUS_MIN_RECORD_MS + 5000);
    (void)app_model_take_events(&m);
    press(IN_UP);
    press(IN_OK);
    assert(m.focus.state == FOCUS_DONE && app_focus_display_s(&m) == 65);
    assert(app_model_take_events(&m) & APP_EV_FOCUS_DONE);
}

static void test_count_up_caps_at_99_59(void)
{
    start();
    press(IN_OK);
    press(IN_UP);
    press(IN_OK);
    t += APP_FOCUS_COUNTUP_MAX_MS + 5000;
    app_model_tick(&m, t);
    assert(m.focus.state == FOCUS_DONE && app_focus_display_s(&m) == 99 * 60 + 59);
}

static void test_focus_keeps_running_off_page_and_screen(void)
{
    start();
    m.prefs.screen_off = 2;  // 1 minute
    press(IN_OK);
    press(IN_OK);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_HOME && m.focus.state == FOCUS_RUNNING);
    press(IN_DOWN);
    press(IN_OK);
    assert(m.page == PAGE_CALENDAR && m.focus.state == FOCUS_RUNNING);
    advance(60000);
    assert(m.screen_off && app_backlight_percent(&m) == 0);
    press(IN_OK);  // waking press only wakes
    assert(!m.screen_off && m.page == PAGE_CALENDAR);
    press(IN_HOLD_DOWN);  // long-press DOWN no longer turns the screen off
    assert(!m.screen_off && m.page == PAGE_CALENDAR);
    advance(60000);       // auto screen-off still does
    assert(m.screen_off);
    while (m.focus.state == FOCUS_RUNNING) {
        advance(30);
        if (m.focus.state == FOCUS_RUNNING) assert(m.screen_off);  // stays dark until the end
    }
    assert(m.focus.state == FOCUS_DONE);
    assert(!m.screen_off);              // completion wakes the screen
    assert(m.page == PAGE_CALENDAR);    // but does not yank the user off a full page
}

static void test_completion_from_home_shows_focus(void)
{
    start();
    press(IN_OK);
    press(IN_OK);
    press(IN_HOLD_OK);
    press(IN_DOWN);
    advance(APP_FOCUS_COUNTDOWN_MS);
    assert(m.page == PAGE_FOCUS && m.nav == NAV_FOCUS);
}

static const int32_t TODAY = 20742;  // 2026-10-16

static void test_calendar_navigation(void)
{
    start();
    m.nav = NAV_CALENDAR;
    press(IN_OK);
    assert(m.page == PAGE_CALENDAR);
    press(IN_UP);  // no date yet: nothing to move
    assert(!m.date_known);
    press(IN_OK);
    assert(m.page == PAGE_ANNIV && app_since_hsr_launch_s(&m) == 0);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_CALENDAR);
    app_model_set_time(&m, 10 * 3600 + 30 * 60, TODAY, t);
    assert(m.date_known && m.cal_day == TODAY);
    press(IN_DOWN);
    assert(m.cal_day == TODAY);  // never past today
    for (int i = 0; i < 20; i++) press(IN_UP);
    assert(m.cal_day == TODAY - 20);
    for (int i = 0; i < 400; i++) press(IN_UP);
    assert(m.cal_day == TODAY - (FOCUS_LOG_DAYS - 1));  // only the logged year
    press(IN_HOLD_UP);
    assert(m.cal_day == TODAY);
    press(IN_OK);
    // 2023-04-26 10:00 -> 2026-10-16 10:30 is 1269 days and 30 minutes.
    assert(m.page == PAGE_ANNIV && app_since_hsr_launch_s(&m) == 1269ULL * 86400U + 30U * 60U);
    press(IN_OK); // pause, without leaving the anniversary page
    assert(m.page == PAGE_ANNIV);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_CALENDAR);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_HOME);
}

static void test_anniversary_playback_controls(void)
{
    start(); m.page = PAGE_CALENDAR;
    press(IN_OK);
    uint32_t session = m.anniv_session;
    uint32_t entered = m.anniv_entered_ms;
    assert(entered == t);
    advance(500);
    assert(m.page == PAGE_ANNIV && !m.anniv_paused && session != 0);
    press(IN_OK);
    assert(m.page == PAGE_ANNIV && m.anniv_paused);
    assert(m.anniv_entered_ms == entered);
    advance(1000);
    press(IN_OK);
    assert(!m.anniv_paused && m.anniv_session == session);
    press(IN_OK);
    press(IN_HOLD_OK);
    press(IN_OK);
    assert(m.page == PAGE_ANNIV && !m.anniv_paused && m.anniv_session != session);
    m.screen_off = true;
    press(IN_OK); // waking does not pause
    assert(!m.screen_off && !m.anniv_paused);
    assert(!(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
}

static void test_anniversary_temporary_volume_keys(void)
{
    start(); m.prefs.volume = 40; m.page = PAGE_CALENDAR;
    press(IN_OK);
    assert(app_volume_percent(&m) == 40);
    press(IN_UP); assert(app_volume_percent(&m) == 50);
    press(IN_DOWN); assert(app_volume_percent(&m) == 40);
    for (unsigned i=0; i<20; i++) press(IN_UP);
    assert(app_volume_percent(&m) == 100);
    for (unsigned i=0; i<20; i++) press(IN_DOWN);
    assert(app_volume_percent(&m) == 0);
    assert(m.prefs.volume == 40);
    assert(!(app_model_take_events(&m) & (APP_EV_PREFS_CHANGED | APP_EV_VOLUME_PREVIEW | APP_EV_KEY_SOUND)));
    press(IN_OK); assert(m.anniv_paused);
    press(IN_UP); assert(app_volume_percent(&m) == 10 && m.anniv_paused);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_CALENDAR && app_volume_percent(&m) == 40);
    press(IN_OK); assert(app_volume_percent(&m) == 40 && !m.anniv_paused);
}

static void test_focus_log_counts_stopped_sessions(void)
{
    start();
    app_model_set_time(&m, 9 * 3600, TODAY, t);
    (void)app_model_take_events(&m);
    press(IN_OK);
    press(IN_OK);  // start a countdown
    assert(app_model_take_events(&m) & APP_EV_LOG_CHANGED);
    assert(app_focus_on_day(&m, TODAY).sessions == 1);
    advance(10 * 60000);
    press(IN_OK);  // pause: paused time is not focus
    assert(app_model_take_events(&m) & APP_EV_LOG_CHANGED);
    advance(30 * 60000);
    press(IN_UP);
    press(IN_OK);  // reset the paused session
    focus_day_t d = app_focus_on_day(&m, TODAY);
    assert(d.secs == 600 && d.sessions == 1);  // the 10 minutes before the stop are kept
    press(IN_OK);  // a second session, stopped while running
    advance(3 * 60000 + 500);
    press(IN_UP);
    press(IN_OK);
    d = app_focus_on_day(&m, TODAY);
    assert(d.secs == 780 && d.sessions == 2);
    press(IN_OK);  // a full countdown
    while (m.focus.state == FOCUS_RUNNING) advance(30);
    assert(app_focus_on_day(&m, TODAY).secs == 780 + 25 * 60);
}

static void test_focus_log_midnight_and_unknown_date(void)
{
    start();
    press(IN_OK);
    press(IN_DOWN);  // count up
    press(IN_OK);    // started before the date is known
    advance(5 * 60000);
    assert(m.pend_secs == 300 && m.pend_sessions == 1);
    app_model_set_time(&m, 23 * 3600 + 58 * 60, TODAY, t);  // time arrives: put on today
    assert(app_focus_on_day(&m, TODAY).secs == 300 && app_focus_on_day(&m, TODAY).sessions == 1);
    advance(4 * 60000);  // crosses midnight while running
    assert(m.day == TODAY + 1);
    uint32_t a = app_focus_on_day(&m, TODAY).secs, b = app_focus_on_day(&m, TODAY + 1).secs;
    assert(a + b == 540 && a >= 410 && a <= 425);  // split at midnight
    (void)app_model_take_events(&m);
    advance(APP_LOG_SAVE_MS);  // a long session persists itself every 5 minutes
    assert(app_model_take_events(&m) & APP_EV_LOG_CHANGED);
}
static void test_settings_adjust_preview_cancel_save(void)
{
    start();
    m.nav = NAV_SETTINGS;
    press(IN_OK);
    press(IN_OK);  // brightness
    assert(m.page == PAGE_ADJUST && m.draft == 70);
    for (int i = 0; i < 6; i++) press(IN_UP);
    assert(m.draft == 100 && app_backlight_percent(&m) == 100 && m.prefs.brightness == 70);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_SETTINGS && app_backlight_percent(&m) == 70);
    press(IN_OK);
    for (int i = 0; i < 12; i++) press(IN_DOWN);
    assert(m.draft == 10);  // never fully dark
    press(IN_OK);
    assert(m.prefs.brightness == 10 && (app_model_take_events(&m) & APP_EV_PREFS_CHANGED));

    press(IN_DOWN);
    press(IN_OK);  // volume
    for (int i = 0; i < 6; i++) press(IN_DOWN);
    assert(m.draft == 0 && app_volume_percent(&m) == 0);
    assert(app_model_take_events(&m) & APP_EV_VOLUME_PREVIEW);
    press(IN_OK);
    assert(m.prefs.volume == 0);

    press(IN_DOWN);
    press(IN_OK);  // auto screen off
    for (int i = 0; i < 9; i++) press(IN_UP);
    assert(m.draft == app_screen_off_count() - 1);
    press(IN_OK);
    assert(app_screen_off_ms(m.prefs.screen_off) == 600000);
    press(IN_DOWN);
    press(IN_OK);  // Wi-Fi tile: setup page again, hold OK goes back to Settings
    assert(m.page == PAGE_SETUP && m.setup_from_settings);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_SETTINGS);
    press(IN_HOLD_OK);  // long-press OK is the way back
    assert(m.page == PAGE_HOME);
    assert(app_prefs_valid(&m.prefs));
}

static void test_network_time_and_room_phase(void)
{
    start();
    assert(!m.clock_set && app_room_phase(&m) == ROOM_DAY);
    press(IN_HOLD_OK);  // Home has no clock shortcut
    assert(m.page == PAGE_HOME);
    app_model_set_time(&m, 18 * 3600 - 60, TODAY, t);  // network time arrives
    assert(m.clock_set && m.clock_s == 18 * 3600 - 60 && app_room_phase(&m) == ROOM_DUSK);
    advance(2 * 3600 * 1000U + 60000);
    assert(m.clock_s / 3600 == 20 && app_room_phase(&m) == ROOM_NIGHT);
    m.clock_s = 23 * 3600;
    assert(app_room_phase(&m) == ROOM_LATE);
    m.clock_s = 5 * 3600 + 59 * 60;
    assert(app_room_phase(&m) == ROOM_LATE);
    m.clock_s = 6 * 3600;
    assert(app_room_phase(&m) == ROOM_DAY);
    app_model_set_time(&m, 90000, TODAY, t);  // wraps into the day
    assert(m.clock_s == 3600);
}

static void test_first_boot_setup(void)
{
    start();
    m.prefs.screen_off = 1;  // 30 s
    app_model_begin_setup(&m, false);
    assert(m.page == PAGE_SETUP && !m.setup_from_settings);
    press(IN_UP);
    press(IN_OK);
    assert(m.page == PAGE_SETUP);  // keys do nothing while the phone does the setup
    advance(120000);
    assert(!m.screen_off);         // the QR code stays visible
    app_model_setup_finished(&m);  // network saved
    assert(m.page == PAGE_HOME);
    app_model_begin_setup(&m, false);
    press(IN_HOLD_OK);             // or skip for now
    assert(m.page == PAGE_HOME);
    m.page = PAGE_SETTINGS;
    m.setting_sel = SET_WIFI;
    press(IN_OK);
    assert(m.page == PAGE_SETUP && m.setup_from_settings);
    app_model_setup_finished(&m);
    assert(m.page == PAGE_SETTINGS);
    app_model_setup_finished(&m);  // only acts on the setup page
    assert(m.page == PAGE_SETTINGS);
}
static void test_jump_requests_existing_chime_only_when_played(void)
{
    start(); m.page = PAGE_PLAY;
    m.companion.selected = COMPANION_ACTION_JUMP;
    (void)app_model_take_events(&m);
    press(IN_OK);
    uint32_t events = app_model_take_events(&m);
    assert(events & APP_EV_PLAY_JUMP);
    assert(!(events & (APP_EV_FOCUS_DONE | APP_EV_LOG_CHANGED | APP_EV_PREFS_CHANGED | APP_EV_KEY_SOUND)));
    assert(m.companion.last_action == COMPANION_ACTION_JUMP);
    press(IN_DOWN); // merely selecting Bat Swing must not play the jump sound
    assert(!(app_model_take_events(&m) & APP_EV_PLAY_JUMP));
    m.companion.selected = COMPANION_ACTION_JUMP;
    m.screen_off = true;
    press(IN_OK); // wake-only press
    assert(!(app_model_take_events(&m) & APP_EV_PLAY_JUMP));
    press(IN_OK);
    assert(app_model_take_events(&m) & APP_EV_PLAY_JUMP);
    assert(app_model_take_events(&m) == 0);
}

static void test_bat_sound_requested_only_on_play_action(void)
{
    start(); m.page = PAGE_PLAY;
    m.companion.selected = COMPANION_ACTION_BAT_SWING;
    (void)app_model_take_events(&m);
    press(IN_OK);
    uint32_t events = app_model_take_events(&m);
    assert(events & APP_EV_PLAY_BAT);
    assert(!(events & (APP_EV_PLAY_JUMP | APP_EV_FOCUS_DONE | APP_EV_KEY_SOUND)));
    press(IN_UP);
    assert(!(app_model_take_events(&m) & APP_EV_PLAY_BAT));
    m.companion.selected = COMPANION_ACTION_BAT_SWING;
    m.screen_off = true;
    press(IN_OK);
    assert(!(app_model_take_events(&m) & APP_EV_PLAY_BAT));
    press(IN_OK);
    assert(app_model_take_events(&m) & APP_EV_PLAY_BAT);
}

static void test_play_page(void)
{
    start();
    advance(2000);
    m.nav = NAV_PLAY;
    press(IN_OK);
    assert(m.page == PAGE_PLAY);
    press(IN_DOWN);
    press(IN_DOWN);
    press(IN_OK);
    assert(m.companion.motion == COMPANION_MOTION_CELEBRATION && m.companion.from_play);
    press(IN_HOLD_OK);
    assert(m.page == PAGE_HOME);
}

static void test_prefs_validation(void)
{
    app_prefs_t p;
    app_prefs_default(&p);
    assert(app_prefs_valid(&p));
    assert(app_screen_off_ms(p.screen_off) == 0);  // a fresh device never turns the screen off
    assert(app_screen_off_count() == 5 && app_screen_off_ms(1) == 30000 && app_screen_off_ms(2) == 60000 &&
           app_screen_off_ms(3) == 300000 && app_screen_off_ms(4) == 600000);
    app_prefs_t bad = p;
    bad.version = 1;
    assert(!app_prefs_valid(&bad));
    bad = p; bad.brightness = 0; assert(!app_prefs_valid(&bad));
    bad = p; bad.brightness = 75; assert(!app_prefs_valid(&bad));
    bad = p; bad.volume = 110; assert(!app_prefs_valid(&bad));
    bad = p; bad.screen_off = 9; assert(!app_prefs_valid(&bad));
    start();
    app_model_load_prefs(&m, &bad);
    assert(app_prefs_valid(&m.prefs) && m.prefs.screen_off == 0);
}

static void test_scene_switch_and_prefs_upgrade(void)
{
    start();
    assert(m.prefs.scene == SCENE_ROOM);
    m.nav = NAV_SETTINGS;
    press(IN_OK);
    press(IN_UP);  // wraps to the last tile
    assert(m.setting_sel == SET_SCENE);
    (void)app_model_take_events(&m);
    press(IN_OK);  // opens the picker with a preview of the current scene
    assert(m.page == PAGE_SCENE && m.draft == SCENE_ROOM);
    press(IN_DOWN);
    assert(m.draft == SCENE_TRAIN && m.prefs.scene == SCENE_ROOM);  // only the preview changes
    press(IN_DOWN);
    assert(m.draft == SCENE_TRAILBLAZE);
    press(IN_UP);
    assert(m.draft == SCENE_TRAIN);
    assert(!(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
    press(IN_HOLD_OK);  // long-press OK keeps it and goes back
    assert(m.page == PAGE_SETTINGS && m.prefs.scene == SCENE_TRAIN);
    assert(app_model_take_events(&m) & APP_EV_PREFS_CHANGED);
    press(IN_OK);
    press(IN_HOLD_OK);  // unchanged: nothing to save
    assert(m.prefs.scene == SCENE_TRAIN && !(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
    // Older blobs (with the old to-do list) keep their settings; v2 had no scene.
    uint8_t v2[22] = {2, 0, 40, 30, 1, 3}, v3[24] = {3, 0, 50, 20, 4, 0};
    v3[22] = SCENE_TRAIN;
    app_prefs_t p;
    assert(app_prefs_from_blob(v2, sizeof(v2), &p));
    assert(p.version == APP_PREFS_VERSION && p.brightness == 40 && p.volume == 30 && p.screen_off == 1 &&
           p.scene == SCENE_ROOM);
    assert(app_prefs_from_blob(v3, sizeof(v3), &p) && p.brightness == 50 && p.scene == SCENE_TRAIN);
    assert(!app_prefs_from_blob(v3, sizeof(v3) - 1, &p));
    v3[0] = 1;
    assert(!app_prefs_from_blob(v3, sizeof(v3), &p));
    app_prefs_default(&p);
    p.brightness = 90;
    app_prefs_t q;
    assert(app_prefs_from_blob((const uint8_t *)&p, sizeof(p), &q) && q.brightness == 90);    app_prefs_default(&p);
    p.scene = SCENE_COUNT;
    assert(!app_prefs_valid(&p));
}

static void test_sleeps_in_the_box_before_eight(void)
{
    start();
    advance(2000);  // boot greeting over
    assert(!app_companion_asleep(&m));  // unknown time: never asleep
    app_model_set_time(&m, 0, TODAY, t);
    assert(app_companion_asleep(&m));
    app_model_set_time(&m, 8 * 3600 - 1, TODAY, t);
    assert(app_companion_asleep(&m));
    advance(1000);  // 8:00 sharp: standing idle again
    assert(!app_companion_asleep(&m));
    app_model_set_time(&m, 23 * 3600 + 59 * 60, TODAY, t);
    assert(!app_companion_asleep(&m));
    app_model_set_time(&m, 3 * 3600, TODAY, t);
    companion_model_play(&m.companion, COMPANION_MOTION_GREETING, 1, t);
    assert(!app_companion_asleep(&m));  // a played motion wakes her up while it runs
    advance(1000);
    assert(app_companion_asleep(&m));

    // Play highlighted in the nav or open: standing idle; leaving it: asleep again.
    assert(m.page == PAGE_HOME);
    while (m.nav != NAV_PLAY) press(IN_DOWN);
    assert(!app_companion_asleep(&m));
    press(IN_OK);
    assert(m.page == PAGE_PLAY && !app_companion_asleep(&m));
    press(IN_HOLD_OK);
    assert(m.page == PAGE_HOME && m.nav == NAV_PLAY && !app_companion_asleep(&m));
    press(IN_UP); assert(m.nav == NAV_CHAT && !app_companion_asleep(&m));
    press(IN_UP); assert(m.nav == NAV_CALENDAR && app_companion_asleep(&m));
    press(IN_UP); assert(m.nav == NAV_FOCUS && app_companion_asleep(&m));
}

static void test_focus_pose_holds_while_running(void)
{
    start();
    advance(3000);  // boot greeting over
    press(IN_OK);
    press(IN_OK);   // start
    assert(m.companion.motion == COMPANION_MOTION_FOCUS && m.companion.hold);
    uint32_t clip = companion_motion_duration_ms(COMPANION_MOTION_FOCUS);
    advance(clip * 3);
    assert(m.companion.motion == COMPANION_MOTION_FOCUS);           // not looping back to idle
    assert(companion_model_elapsed(&m.companion, t) == clip - 1);   // parked on the last frame
    press(IN_OK);   // pause releases her
    advance(30);
    assert(m.companion.motion == COMPANION_MOTION_IDLE);
    press(IN_OK);   // resume plays it again and holds
    assert(m.companion.motion == COMPANION_MOTION_FOCUS && m.companion.hold);
    press(IN_HOLD_OK);  // back on Home the session still runs: the pose stays
    advance(5000);
    assert(m.companion.motion == COMPANION_MOTION_FOCUS);
    m.page = PAGE_FOCUS;
    press(IN_UP);
    press(IN_OK);   // stop (countdown): back to ready and idle
    advance(30);
    assert(m.focus.state == FOCUS_READY && m.companion.motion == COMPANION_MOTION_IDLE);
}
static void test_tick_clock_wrap(void)
{
    t = UINT32_MAX - 5000;
    app_model_init(&m, t);
    m.prefs.screen_off = 0;
    press(IN_OK);
    press(IN_OK);
    advance(20000);  // crosses the 32-bit millisecond wrap
    assert(m.focus.state == FOCUS_RUNNING && m.focus.elapsed_ms >= 19990 && m.focus.elapsed_ms <= 20000);
}

static void test_completed_sessions_survive_log_restore(void)
{
    static focus_log_t saved;
    start();
    press(IN_OK); press(IN_OK);
    advance(APP_FOCUS_COUNTDOWN_MS);
    assert(!m.focus.repeated_completion && m.pend_completions==1);
    press(IN_OK); press(IN_OK);
    advance(APP_FOCUS_COUNTDOWN_MS);
    assert(m.focus.repeated_completion && m.pend_completions==2);
    app_model_set_time(&m,13*3600,TODAY,t);
    assert(m.pend_completions==0 && app_focus_on_day(&m,TODAY).completed==2);
    saved=m.log;
    start();
    app_model_load_log(&m,&saved);
    app_model_set_time(&m,14*3600,TODAY,t);
    press(IN_OK); press(IN_OK); advance(APP_FOCUS_COUNTDOWN_MS);
    assert(m.focus.repeated_completion && app_focus_on_day(&m,TODAY).completed==3);
    advance(12*3600*1000U); // the completion line stays stable across midnight
    assert(m.day==TODAY+1 && m.focus.repeated_completion);
    press(IN_OK); press(IN_OK); advance(APP_FOCUS_COUNTDOWN_MS);
    assert(!m.focus.repeated_completion && app_focus_on_day(&m,TODAY+1).completed==1);

    start(); app_model_set_time(&m,13*3600,TODAY,t);
    press(IN_OK); press(IN_OK); advance(1000);
    press(IN_UP); press(IN_OK); // stopped, not completed
    assert(app_focus_on_day(&m,TODAY).completed==0);
    press(IN_OK); advance(APP_FOCUS_COUNTDOWN_MS);
    assert(!m.focus.repeated_completion && app_focus_on_day(&m,TODAY).completed==1);
}

static void test_trailblaze_enter_and_keep(void)
{
    start();
    app_model_set_time(&m, 12 * 3600, TODAY, t);
    m.page = PAGE_SETTINGS;
    m.setting_sel = SET_SCENE;
    press(IN_OK);
    press(IN_DOWN); press(IN_DOWN);
    assert(m.draft == SCENE_TRAILBLAZE && m.prefs.scene == SCENE_ROOM);
    press(IN_HOLD_OK);
    assert(m.prefs.scene == SCENE_TRAILBLAZE && m.prefs.trailblaze_chosen);
    assert(m.prefs.trailblaze_index < APP_TRAILBLAZE_COUNT);
    unsigned chosen = m.prefs.trailblaze_index;
    (void)app_model_take_events(&m);
    press(IN_OK); press(IN_HOLD_OK);
    assert(m.prefs.trailblaze_index == chosen);
    assert(!(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
}

static void test_trailblaze_midnight_and_restore(void)
{
    start();
    app_model_set_time(&m, 86399, TODAY, t);
    m.page = PAGE_SCENE; m.draft = SCENE_TRAILBLAZE;
    press(IN_HOLD_OK);
    unsigned before = m.prefs.trailblaze_index;
    advance(1000);
    assert(m.prefs.trailblaze_day == TODAY + 1);
    assert(m.prefs.trailblaze_index != before);
    app_prefs_t saved = m.prefs;
    start(); app_model_load_prefs(&m, &saved);
    app_model_set_time(&m, 600, TODAY + 1, t);
    assert(m.prefs.trailblaze_index == saved.trailblaze_index);
    assert(!(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
    app_model_set_time(&m, 600, TODAY + 3, t);
    assert(m.prefs.trailblaze_index != saved.trailblaze_index);
    assert(m.prefs.trailblaze_day == TODAY + 3);
}

static void test_trailblaze_unknown_time_reenter_and_migrate(void)
{
    start();
    app_model_seed_scenes(&m, 12345);
    m.page = PAGE_SCENE; m.draft = SCENE_TRAILBLAZE;
    press(IN_HOLD_OK);
    unsigned before = m.prefs.trailblaze_index;
    advance(86400U * 1000U);
    assert(m.prefs.trailblaze_index == before && m.prefs.trailblaze_day == 0);
    app_model_set_time(&m, 12 * 3600, TODAY, t);
    assert(m.prefs.trailblaze_index == before && m.prefs.trailblaze_day == TODAY);
    app_model_set_time(&m, 12 * 3600, TODAY - 1, t);
    assert(m.prefs.trailblaze_index == before);
    app_model_set_time(&m, 12 * 3600, TODAY, t);
    assert(m.prefs.trailblaze_index == before);
    m.page = PAGE_SCENE; m.draft = SCENE_TRAIN; press(IN_HOLD_OK);
    m.page = PAGE_SCENE; m.draft = SCENE_TRAILBLAZE; press(IN_HOLD_OK);
    assert(m.prefs.trailblaze_index != before);
    uint8_t v4[8] = {4, 0, 80, 20, 3, SCENE_TRAIN, 0, 0};
    app_prefs_t upgraded;
    assert(app_prefs_from_blob(v4, sizeof(v4), &upgraded));
    assert(upgraded.scene == SCENE_TRAIN && upgraded.brightness == 80 && upgraded.volume == 20);
    assert(upgraded.screen_off == 3 && !upgraded.trailblaze_chosen && upgraded.trailblaze_day == 0);
    app_prefs_t invalid = m.prefs;
    invalid.trailblaze_index = APP_TRAILBLAZE_COUNT;
    assert(!app_prefs_valid(&invalid));
}

static void test_scene_ok_applies_and_returns_home(void)
{
    for (unsigned scene = 0; scene < SCENE_COUNT; scene++) {
        start();
        m.nav = NAV_SETTINGS;
        press(IN_OK); m.setting_sel = SET_SCENE; press(IN_OK);
        for (unsigned i = 0; i < scene; i++) press(IN_DOWN);
        assert(m.page == PAGE_SCENE && m.draft == scene);
        press(IN_OK);
        assert(m.page == PAGE_HOME && m.prefs.scene == scene);
        unsigned chosen = m.prefs.trailblaze_index;
        (void)app_model_take_events(&m);
        press(IN_OK); press(IN_UP); press(IN_OK); press(IN_OK);
        assert(m.page == PAGE_HOME && m.prefs.scene == scene);
        assert(m.prefs.trailblaze_index == chosen);
        assert(!(app_model_take_events(&m) & APP_EV_PREFS_CHANGED));
    }
}

static void test_key_sound_follows_volume_and_preserves_wake_behavior(void)
{
    start(); press(IN_UP);
    assert(app_model_take_events(&m) & APP_EV_KEY_SOUND);
    m.prefs.volume = 0; press(IN_DOWN);
    assert(!(app_model_take_events(&m) & APP_EV_KEY_SOUND));
    m.prefs.volume = 40; m.screen_off = true;
    app_nav_t nav = m.nav;
    press(IN_DOWN);
    assert(!m.screen_off && m.nav == nav);
    assert(app_model_take_events(&m) & APP_EV_KEY_SOUND);
}

static void test_key_sound_does_not_overlap_voice_or_other_feedback(void)
{
    start(); press(IN_DOWN); press(IN_DOWN); // pending navigation sound, then start Chat in same batch
    press(IN_OK);
    uint32_t events = app_model_take_events(&m);
    assert(events & APP_EV_CHAT_START);
    assert(!(events & APP_EV_KEY_SOUND));
    for (unsigned phase = CHAT_CONNECTING; phase <= CHAT_PLAYING; phase++) {
        start(); m.page = PAGE_CHAT; m.chat.phase = (chat_phase_t)phase;
        press(IN_OK); // recording->waiting, or abort to off
        assert(!(app_model_take_events(&m) & APP_EV_KEY_SOUND));
    }
    start(); m.page = PAGE_ADJUST; m.setting_sel = SET_VOLUME; m.draft = 40;
    press(IN_UP);
    events = app_model_take_events(&m);
    assert(events & APP_EV_VOLUME_PREVIEW);
    assert(!(events & APP_EV_KEY_SOUND));
    start(); m.events = APP_EV_FOCUS_DONE;
    press(IN_DOWN);
    assert(!(app_model_take_events(&m) & APP_EV_KEY_SOUND));
}

int main(void)
{
    test_jump_requests_existing_chime_only_when_played();
    test_bat_sound_requested_only_on_play_action();
    test_anniversary_playback_controls();
    test_anniversary_temporary_volume_keys();
    test_key_sound_follows_volume_and_preserves_wake_behavior();
    test_key_sound_does_not_overlap_voice_or_other_feedback();
    test_scene_ok_applies_and_returns_home();
    test_trailblaze_enter_and_keep();
    test_trailblaze_midnight_and_restore();
    test_trailblaze_unknown_time_reenter_and_migrate();
    test_completed_sessions_survive_log_restore();
    test_defaults_and_nav();
    test_nav_window_scrolls_only_at_the_edge();
    test_countdown_completes_once();
    test_last_second_pause_settles_deadline_first();
    test_pause_freezes_and_resume_continues();
    test_stop_and_reset_confirmations();
    test_count_up_stop_records_long_sessions();
    test_count_up_caps_at_99_59();
    test_focus_keeps_running_off_page_and_screen();
    test_completion_from_home_shows_focus();
    test_calendar_navigation();
    test_focus_log_counts_stopped_sessions();
    test_focus_log_midnight_and_unknown_date();
    test_settings_adjust_preview_cancel_save();
    test_network_time_and_room_phase();
    test_first_boot_setup();
    test_play_page();
    test_prefs_validation();
    test_scene_switch_and_prefs_upgrade();
    test_focus_pose_holds_while_running();
    test_sleeps_in_the_box_before_eight();
    test_tick_clock_wrap();
    puts("app model tests: PASS");
    return 0;
}
