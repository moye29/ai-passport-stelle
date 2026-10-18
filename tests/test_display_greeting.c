#include "app_model.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    app_model_t m;
    app_model_init(&m,1000);
    app_model_tick(&m,9000); /* slow initialization must not consume the greeting */
    app_model_display_ready(&m,9000);
    assert(app_model_greeting_visible(&m,9000));
    assert(app_model_greeting_visible(&m,9999));
    assert(!app_model_greeting_visible(&m,10000));
    assert(m.page==PAGE_HOME && m.nav==NAV_FOCUS);
    assert(app_model_take_events(&m)==0);

    const app_page_t room[]={PAGE_HOME,PAGE_FOCUS,PAGE_PLAY,PAGE_CHAT,PAGE_BATTLE};
    for (unsigned i=0;i<sizeof(room)/sizeof(*room);i++) {
        m.page=room[i];m.screen_off=false;
        m.focus.state=FOCUS_RUNNING;
        m.battle_stage=BATTLE_ENCOUNTER;m.path_energy=42;
        companion_model_play_hold(&m.companion,COMPANION_MOTION_FOCUS,11000);
        companion_model_t before=m.companion;
        app_model_display_ready(&m,12000);
        assert(app_model_greeting_visible(&m,12400));
        assert(m.page==room[i] && m.focus.state==FOCUS_RUNNING && m.path_energy==42);
        assert(!memcmp(&before,&m.companion,sizeof(before)));
        assert(!app_model_greeting_visible(&m,13000));
    }
    const app_page_t detail[]={PAGE_SETTINGS,PAGE_CHAT_HISTORY,PAGE_CALENDAR,PAGE_ANNIV,PAGE_ADJUST,PAGE_SCENE,PAGE_SETUP};
    for (unsigned i=0;i<sizeof(detail)/sizeof(*detail);i++) {
        m.page=detail[i];app_model_display_ready(&m,14000);
        assert(!app_model_greeting_visible(&m,14000) && m.page==detail[i]);
    }
    app_model_init(&m,15000);m.page=PAGE_FOCUS;m.screen_off=true;
    app_model_handle(&m,IN_OK,16000); /* wake press must not start Focus */
    app_model_display_ready(&m,16000);
    assert(!m.screen_off && m.focus.state==FOCUS_READY);
    assert(app_model_greeting_visible(&m,16000));
    app_model_handle(&m,IN_OK,16100);
    assert(m.focus.state==FOCUS_RUNNING && !app_model_greeting_visible(&m,16100));
    m.page=PAGE_HOME;app_model_display_ready(&m,UINT32_MAX-100);
    assert(app_model_greeting_visible(&m,500));
    assert(!app_model_greeting_visible(&m,1000));
    m.screen_off=true;
    assert(!app_model_greeting_visible(&m,500));
    puts("Display greeting: visible-time start, preserved pages/actions, wake-only input and clock wrap: PASS");
}
