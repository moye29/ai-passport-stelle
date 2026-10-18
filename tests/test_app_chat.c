#include "app_model.h"
#include <assert.h>
#include <stdio.h>
static app_model_t m;
static void key(app_input_t in) { app_model_handle(&m, in, 1000); }
int main(void)
{
    app_model_init(&m, 1000);
    key(IN_DOWN); assert(m.nav == NAV_CALENDAR);
    key(IN_DOWN); assert(m.nav == NAV_CHAT && m.chat.phase == CHAT_OFF);
    m.clock_set=true; m.clock_s=3600;
    assert(!app_companion_asleep(&m));
    key(IN_HOLD_OK); assert(m.page == PAGE_CHAT_HISTORY && m.chat.phase == CHAT_OFF);
    key(IN_HOLD_OK); assert(m.page == PAGE_CHAT);
    key(IN_UP); assert(m.page == PAGE_HOME && m.nav == NAV_CHAT);
    key(IN_HOLD_OK); assert(m.page == PAGE_CHAT_HISTORY);
    key(IN_HOLD_OK); assert(m.page == PAGE_CHAT);
    key(IN_UP); assert(m.page == PAGE_HOME);
    key(IN_OK); assert(m.page == PAGE_CHAT && m.chat.phase == CHAT_CONNECTING);
    assert(app_model_take_events(&m) & APP_EV_CHAT_START);
    chat_model_set_phase(&m.chat, CHAT_RECORDING);
    key(IN_OK); assert(m.chat.phase == CHAT_WAITING);
    assert(app_model_take_events(&m) & APP_EV_CHAT_STOP_RECORDING);
    chat_model_set_phase(&m.chat, CHAT_PLAYING);
    key(IN_OK); assert(m.chat.phase == CHAT_OFF);
    assert(app_model_take_events(&m) & APP_EV_CHAT_CANCEL);
    chat_history_add(&m.chat_history, false, "Hello");
    chat_history_add(&m.chat_history, true, "Hi!");
    for (unsigned i=0; i<5; i++) chat_history_add(&m.chat_history, i%2, "Another line");
    key(IN_HOLD_OK); assert(m.page == PAGE_CHAT_HISTORY);
    unsigned bottom = chat_history_max_scroll(&m.chat_history);
    assert(bottom > 0 && m.chat_history_scroll == bottom);
    key(IN_UP); assert(m.chat_history_scroll == bottom-CHAT_HISTORY_LINE_PX);
    key(IN_DOWN); assert(m.chat_history_scroll == bottom);
    key(IN_DOWN); assert(m.chat_history_scroll == bottom);
    for (unsigned i=0; i<50; i++) key(IN_UP);
    assert(m.chat_history_scroll == 0);
    key(IN_UP); assert(m.chat_history_scroll == 0);
    key(IN_OK); assert(m.chat_history_scroll == bottom);
    key(IN_HOLD_OK); key(IN_DOWN); assert(m.page == PAGE_HOME);
    key(IN_DOWN); assert(m.nav == NAV_PLAY);
    key(IN_DOWN); assert(m.nav == NAV_BATTLE);
    key(IN_HOLD_OK);
    key(IN_DOWN); assert(m.nav == NAV_SETTINGS);
    key(IN_DOWN); assert(m.nav == NAV_FOCUS);
    key(IN_DOWN); assert(m.nav == NAV_CALENDAR);
    key(IN_DOWN); assert(m.nav == NAV_CHAT);
    // Preserve the established auto-off policy and consume the wake-up press.
    m.nav=NAV_CHAT; m.prefs.screen_off=1;
    app_model_tick(&m, 31001); assert(m.screen_off);
    app_model_handle(&m, IN_OK, 31001);
    assert(!m.screen_off && m.chat.phase == CHAT_OFF);
    puts("Chat navigation, history, cancel and screen-off: PASS");
}
