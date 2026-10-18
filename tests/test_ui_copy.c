#include "ui_copy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static app_model_t m;

static void fresh(void)
{
    app_model_init(&m, 0);
    m.prefs.screen_off = 0;
    app_model_tick(&m, 5000);  // boot greeting over
}

static void test_speech_follows_state(void)
{
    fresh();
    assert(strcmp(ui_copy_speech(&m), "Reach the end in your own way.") == 0);
    m.clock_set = true;
    m.clock_s = 18 * 3600;
    assert(strcmp(ui_copy_speech(&m), "Journeys never end.") == 0);
    m.clock_s = 2 * 3600;
    assert(strcmp(ui_copy_speech(&m), "Good night. Rest easy.") == 0);
    m.nav=NAV_CHAT; // Chat keeps her standing even late at night
    assert(strcmp(ui_copy_speech(&m),"May this journey lead us starward.")==0);
    m.nav=NAV_FOCUS;
    app_model_handle(&m, IN_OK, 6000);
    assert(strcmp(ui_copy_speech(&m), "Ready? Let's go.") == 0);
    app_model_handle(&m, IN_OK, 6000);
    assert(strcmp(ui_copy_speech(&m), "One thing at a time.") == 0);
    app_model_handle(&m, IN_OK, 7000);
    assert(strcmp(ui_copy_speech(&m), "Take a breath. I'll be here.") == 0);
    app_model_handle(&m, IN_UP, 7000);
    assert(strcmp(ui_copy_speech(&m), "Again? Let's do it.") == 0);
}

static void test_info_lines(void)
{
    ui_info_t info;
    fresh();
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "--:--") == 0 && strcmp(info.sub, "Set up Wi-Fi to get the time") == 0 && !info.sparkles);
    m.clock_set = true;
    m.clock_s = 9 * 3600 + 5 * 60 + 59;
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "09:05") == 0 && info.sub[0] == '\0');

    app_model_handle(&m, IN_OK, 6000);
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "25:00") == 0 && info.arrows && info.sparkles);
    assert(strcmp(info.sub, "Countdown / OK: start") == 0);
    app_model_handle(&m, IN_OK, 6000);
    app_model_tick(&m, 8000);
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "24:58") == 0 && !info.arrows && strstr(info.sub, "OK: pause") != NULL);
    app_model_handle(&m, IN_HOLD_OK, 8000);
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "09:06") == 0 && strcmp(info.sub, "Focus 24:58 left") == 0);

    fresh();
    m.nav = NAV_CHAT;
    ui_copy_info(&m, &info);
    assert(strcmp(info.sub, "OK: start / Hold: history") == 0);

    fresh();
    m.page = PAGE_PLAY;
    m.nav = NAV_PLAY;
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "Wave") == 0 && info.arrows && !info.big_is_digits);
    assert(strcmp(ui_copy_speech(&m), "What are we playing?") == 0);
    app_model_handle(&m, IN_OK, 6000);
    assert(strcmp(ui_copy_speech(&m), "Hi there, partner!") == 0);
    app_model_tick(&m, 9000);  // the wave is over: its line stays
    assert(strcmp(ui_copy_speech(&m), "Hi there, partner!") == 0);
    app_model_handle(&m, IN_DOWN, 9100);  // choosing another action does not reset it
    assert(strcmp(ui_copy_speech(&m), "Hi there, partner!") == 0);
    app_model_handle(&m, IN_OK, 9200);  // playing another one does
    assert(strcmp(ui_copy_speech(&m), "Fighting!") == 0);
    app_model_handle(&m, IN_HOLD_OK, 12000);  // back Home, Play still highlighted
    assert(m.page == PAGE_HOME && m.nav == NAV_PLAY);
    assert(strcmp(ui_copy_speech(&m), "Fighting!") == 0);
    app_model_handle(&m, IN_UP, 12100);  // nav leaves Play
    assert(m.nav != NAV_PLAY && strcmp(ui_copy_speech(&m), "Reach the end in your own way.") == 0);
    app_model_handle(&m, IN_DOWN, 12200);  // back on Play: the line was forgotten
    assert(m.nav == NAV_PLAY && strcmp(ui_copy_speech(&m), "Reach the end in your own way.") == 0);
}

static void test_chat_stop_hint_is_single_action(void)
{
    ui_info_t info;
    fresh(); m.page=PAGE_CHAT; m.nav=NAV_CHAT;
    const chat_phase_t phases[]={CHAT_CONNECTING,CHAT_BINDING,CHAT_RECORDING,CHAT_WAITING,CHAT_PLAYING};
    for (unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);i++) {
        m.chat.phase=phases[i]; ui_copy_info(&m,&info);
        assert(strcmp(info.sub,"OK: stop")==0);
    }
    m.chat.phase=CHAT_OFF;ui_copy_info(&m,&info);
    assert(strcmp(info.sub,"OK: start / Hold: history")==0);
}

static void test_formatting(void)
{
    char s[24];
    ui_copy_mmss(0, s, sizeof(s));
    assert(strcmp(s, "00:00") == 0);
    ui_copy_mmss(99 * 60 + 59, s, sizeof(s));
    assert(strcmp(s, "99:59") == 0);
    ui_copy_setting_value(SET_BRIGHTNESS, 70, s, sizeof(s));
    assert(strcmp(s, "70%") == 0);
    ui_copy_setting_value(SET_SCREEN_OFF, 0, s, sizeof(s));
    assert(strcmp(s, "Never") == 0);
    ui_copy_setting_value(SET_SCREEN_OFF, 1, s, sizeof(s));
    assert(strcmp(s, "30 s") == 0);
    ui_copy_setting_value(SET_SCREEN_OFF, 3, s, sizeof(s));
    assert(strcmp(s, "5 min") == 0);
    ui_copy_setting_value(SET_SCREEN_OFF, 4, s, sizeof(s));
    assert(strcmp(s, "10 min") == 0);
    // Calendar text.
    int32_t day = date_to_day(2026, 10, 16);
    ui_copy_month(day, s, sizeof(s));
    assert(strcmp(s, "October 2026") == 0);
    ui_copy_date(day, s, sizeof(s));
    assert(strcmp(s, "Oct 16") == 0);
    ui_copy_duration(85 * 60 + 20, s, sizeof(s));
    assert(strcmp(s, "1 h 25 min") == 0);
    ui_copy_duration(12 * 60, s, sizeof(s));
    assert(strcmp(s, "12 min") == 0);
    ui_copy_duration(40, s, sizeof(s));
    assert(strcmp(s, "40 s") == 0);
    char days[16], clock[12];
    ui_copy_since_launch(1269ULL * 86400U + 3723U, days, sizeof(days), clock, sizeof(clock));
    assert(strcmp(days, "1269") == 0 && strcmp(clock, "01:02:03") == 0);
    assert(strcmp(ui_copy_nav_name(NAV_CALENDAR), "Days") == 0);
}

static void test_network_copy(void)
{
    char s[UI_COPY_MAX];
    net_status_t n;
    memset(&n, 0, sizeof(n));
    ui_copy_wifi_value(NULL, s, sizeof(s));
    assert(strcmp(s, "Not set") == 0);
    n.phase = NET_PHASE_PORTAL;
    ui_copy_setup_status(&n, s, sizeof(s));
    assert(strcmp(s, "Waiting for your phone...") == 0);
    n.phone_joined = true;
    ui_copy_setup_status(&n, s, sizeof(s));
    assert(strcmp(s, "Phone connected - fill the page") == 0);
    n.phase = NET_PHASE_JOINING;
    strcpy(n.sta_ssid, "\xE5\xAE\xB6WiFi");  // Chinese name: shown with ? placeholders
    ui_copy_setup_status(&n, s, sizeof(s));
    assert(strcmp(s, "Joining ?WiFi...") == 0);
    strcpy(n.sta_ssid, "AVeryLongNetworkName2G");
    ui_copy_setup_status(&n, s, sizeof(s));
    assert(strcmp(s, "Joining AVeryLongNetw..") == 0 || strlen(s) < 30);
    n.phase = NET_PHASE_JOIN_FAILED;
    strcpy(n.message, "Wrong password. Try again.");
    ui_copy_setup_status(&n, s, sizeof(s));
    assert(strcmp(s, "Wrong password. Try again.") == 0);
    n.has_credentials = true;
    n.phase = NET_PHASE_SYNCED;
    ui_copy_wifi_value(&n, s, sizeof(s));
    assert(strcmp(s, "Time synced") == 0);
    n.phase = NET_PHASE_OFFLINE;
    ui_copy_wifi_value(&n, s, sizeof(s));
    assert(strcmp(s, "Offline") == 0);

    ui_info_t info;
    fresh();
    m.time_sync = SYNC_RUNNING;
    ui_copy_info(&m, &info);
    assert(strcmp(info.big, "--:--") == 0 && strcmp(info.sub, "Syncing time over Wi-Fi...") == 0);
    m.time_sync = SYNC_FAILED;
    ui_copy_info(&m, &info);
    assert(strstr(info.sub, "Offline") != NULL);
}

static void test_requested_nav_order(void)
{
    const char *names[]={"Focus","Days","Chat","Play","Battle","Settings"};
    for(unsigned i=0;i<NAV_COUNT;i++)assert(strcmp(ui_copy_nav_name(i),names[i])==0);
    const char *battle[]={"Stay alert.","Batter up.","Take this!","Who's next?"};
    fresh(); m.page=PAGE_BATTLE; m.nav=NAV_BATTLE;
    for(unsigned i=0;i<4;i++) {
        m.battle_stage=(battle_stage_t)i;
        assert(strcmp(ui_copy_speech(&m),battle[i])==0);
    }
    m.battle_stage=BATTLE_ENCOUNTER;
    ui_info_t battle_info;
    ui_copy_info(&m,&battle_info);
    assert(strstr(battle_info.sub,"OK:") && strstr(battle_info.sub,"Hold:") && strstr(battle_info.sub,"UP:"));
    assert(!strchr(battle_info.sub,'\n'));
    m.battle_stage=BATTLE_HIT;
    ui_copy_info(&m,&battle_info);
    assert(strcmp(battle_info.sub,"Please wait.")==0);
    m.page=PAGE_HOME; m.nav=NAV_BATTLE; m.battle_idle=true;
    assert(strcmp(ui_copy_speech(&m),"I'm ready for fight.")==0);
    m.battle_idle=false; m.page=PAGE_BATTLE; m.battle_stage=BATTLE_READY;
    assert(strcmp(ui_copy_speech(&m),"Stay alert.")==0);
}

static void test_requested_idle_copy(void)
{
    fresh();
    m.clock_set = true;
    m.clock_s = 9 * 3600;
    assert(strcmp(ui_copy_speech(&m), "A new day. What to do?") == 0);
    m.clock_s = 13 * 3600;
    assert(strcmp(ui_copy_speech(&m), "Reach the end in your own way.") == 0);
    m.clock_s = 21 * 3600;
    assert(strcmp(ui_copy_speech(&m), "May this journey lead us starward.") == 0);
    m.clock_s = 2 * 3600;
    assert(strcmp(ui_copy_speech(&m), "Good night. Rest easy.") == 0);
}

static void test_completion_copy_counts_finishes(void)
{
    fresh();
    app_model_set_time(&m, 13 * 3600, date_to_day(2026, 10, 17), 5000);
    app_model_handle(&m, IN_OK, 5000); // Focus
    app_model_handle(&m, IN_OK, 5000); // start
    assert(strcmp(ui_copy_speech(&m), "One thing at a time.") == 0);
    app_model_tick(&m, 5000 + APP_FOCUS_COUNTDOWN_MS);
    assert(strcmp(ui_copy_speech(&m), "Nice. What's next?") == 0);
    uint32_t now = 5000 + APP_FOCUS_COUNTDOWN_MS + 1;
    app_model_handle(&m, IN_OK, now); // ready
    app_model_handle(&m, IN_OK, now); // start again
    app_model_tick(&m, now + APP_FOCUS_COUNTDOWN_MS);
    assert(strcmp(ui_copy_speech(&m), "Another one. Is that all?") == 0);
    static focus_log_t saved;
    saved=m.log;
    fresh(); app_model_load_log(&m,&saved); // reboot, date not known yet
    app_model_handle(&m,IN_OK,5000); app_model_handle(&m,IN_OK,5000);
    app_model_tick(&m,5000+APP_FOCUS_COUNTDOWN_MS);
    app_model_set_time(&m,14*3600,date_to_day(2026,10,17),5000+APP_FOCUS_COUNTDOWN_MS);
    assert(strcmp(ui_copy_speech(&m),"Another one. Is that all?")==0);
}

static void test_dates_do_not_add_festival_messages(void)
{
    static const int dates[][2]={{1,1},{2,16},{2,17},{4,5},{5,1},{6,19},{9,25},{10,1}};
    for (unsigned i=0;i<sizeof(dates)/sizeof(dates[0]);i++) {
        fresh();
        app_model_set_time(&m,13*3600,date_to_day(2026,dates[i][0],dates[i][1]),5000);
        for (unsigned sec=0;sec<80;sec++) {
            m.clock_s=13*3600+sec;
            assert(strcmp(ui_copy_speech(&m),"Reach the end in your own way.")==0);
        }
    }
}

int main(void)
{
    app_model_init(&m, 0);
    assert(strcmp(ui_copy_speech(&m), "Hey, you're a Trailblazer too?") == 0);
    fresh(); m.page=PAGE_PLAY; m.nav=NAV_PLAY;
    m.companion.selected=COMPANION_ACTION_JUMP;
    app_model_handle(&m, IN_OK, 6000);
    assert(strcmp(ui_copy_speech(&m), "Mom, I have knees!") == 0);
    test_dates_do_not_add_festival_messages();
    test_requested_idle_copy();
    test_completion_copy_counts_finishes();
    test_speech_follows_state();
    test_info_lines();
    test_chat_stop_hint_is_single_action();
    test_formatting();
    test_network_copy();
    test_requested_nav_order();
    puts("ui copy tests: PASS");
    return 0;
}

