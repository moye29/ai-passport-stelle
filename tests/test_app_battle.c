#include "app_model.h"
#include <assert.h>
#include <stdio.h>

static app_model_t m;
static void press(app_input_t in, uint32_t now) { app_model_handle(&m, in, now); }
static void enter(void)
{
    app_model_init(&m, 1000);
    press(IN_DOWN, 2000); // Calendar
    press(IN_DOWN, 2050); // Chat
    press(IN_DOWN, 2100); // Play
    press(IN_DOWN, 2200); // Battle: selecting it immediately shows the ready pose
}
static void test_enemy_alternation(void)
{
    enter();
    assert(m.battle_enemy==0);
    press(IN_OK,2400);press(IN_OK,2500);
    assert(m.battle_enemy==0 && m.battle_stage==BATTLE_HIT);
    app_model_tick(&m,2750);
    assert(m.battle_enemy==0 && m.battle_stage==BATTLE_RECOVER);
    app_model_tick(&m,3000);
    assert(m.battle_enemy==1 && m.battle_stage==BATTLE_ENCOUNTER);
    press(IN_UP,3050);
    assert(m.battle_enemy==1 && m.path_energy==0);
    press(IN_OK,3100);
    assert(m.battle_enemy==1 && m.battle_stage==BATTLE_HIT);
    app_model_tick(&m,3600);
    assert(m.battle_enemy==0 && m.battle_stage==BATTLE_ENCOUNTER);
    press(IN_OK,3700);app_model_tick(&m,4200);
    assert(m.battle_enemy==1);
    press(IN_HOLD_OK,4300);press(IN_OK,4400);
    assert(m.battle_enemy==0 && m.battle_stage==BATTLE_READY);
    // A failed durable award leaves the same monster available for retry.
    enter();press(IN_OK,2400);press(IN_OK,2500);
    app_model_take_events(&m);app_model_energy_saved(&m,true);
    app_model_tick(&m,3000);assert(m.battle_enemy==1);
    press(IN_OK,3100);app_model_take_events(&m);app_model_energy_saved(&m,false);
    app_model_tick(&m,4000);
    assert(m.battle_enemy==1 && m.battle_stage==BATTLE_ENCOUNTER && m.path_energy==1);
    press(IN_OK,4100);app_model_tick(&m,4600);
    assert(m.battle_enemy==0 && m.path_energy==2);
}
int main(void)
{
    test_enemy_alternation();
    // A recovery pose lasts 250 ms and then shows the next monster on its own.
    enter(); press(IN_OK, 2400); press(IN_OK, 2500);
    app_model_tick(&m, 2750);
    assert(m.battle_stage == BATTLE_RECOVER);
    press(IN_OK, 2800);
    assert(m.battle_stage == BATTLE_RECOVER);
    app_model_tick(&m, 2999);
    assert(m.battle_stage == BATTLE_RECOVER);
    app_model_tick(&m, 3000);
    assert(m.battle_stage == BATTLE_ENCOUNTER && m.path_energy == 1);
    enter();
    assert(m.nav == NAV_BATTLE && m.page == PAGE_BATTLE && m.battle_stage == BATTLE_READY);
    m.path_energy = 7;
    press(IN_DOWN, 2300);
    assert(m.nav == NAV_SETTINGS && m.page == PAGE_HOME && m.path_energy == 7);
    enter(); m.path_energy=7;
    press(IN_UP, 2300);
    assert(m.nav == NAV_PLAY && m.page == PAGE_HOME && m.path_energy == 7);
    enter();
    press(IN_OK, 2400);
    assert(m.battle_stage == BATTLE_ENCOUNTER && m.path_energy == 0);
    (void)app_model_take_events(&m);
    press(IN_OK, 2500);
    assert(m.battle_stage == BATTLE_HIT && m.path_energy == 1);
    assert((m.events & (APP_EV_ENERGY_CHANGED | APP_EV_BATTLE_HIT)) ==
           (APP_EV_ENERGY_CHANGED | APP_EV_BATTLE_HIT));
    for (int in = IN_UP; in <= IN_HOLD_OK; in++) press((app_input_t)in, 2600);
    assert(m.page == PAGE_BATTLE && m.battle_stage == BATTLE_HIT && m.path_energy == 1);
    app_model_tick(&m, 2749);
    assert(m.battle_stage == BATTLE_HIT);
    app_model_tick(&m, 2750);
    assert(m.battle_stage == BATTLE_RECOVER && m.path_energy == 1);
    app_model_tick(&m, 3500);
    assert(m.battle_stage == BATTLE_ENCOUNTER && m.path_energy == 1);
    press(IN_HOLD_OK, 3600);
    assert(m.page == PAGE_HOME && m.nav == NAV_BATTLE);
    assert(m.companion.motion == COMPANION_MOTION_BAT_SWING);
    app_model_tick(&m, 4399);
    assert(m.companion.motion == COMPANION_MOTION_BAT_SWING);
    app_model_tick(&m, 4400);
    assert(m.companion.motion == COMPANION_MOTION_IDLE);
    app_model_tick(&m, 5599);
    assert(m.companion.motion == COMPANION_MOTION_IDLE);
    app_model_tick(&m, 5600);
    assert(m.companion.motion == COMPANION_MOTION_BAT_SWING);
    assert(!(m.events & APP_EV_PLAY_BAT)); // cosmetic navigation loop has no Play sound
    press(IN_OK, 5700);
    assert(m.page == PAGE_BATTLE && m.battle_stage == BATTLE_READY && m.path_energy == 1);
    press(IN_OK, 5750); // reset is now available only after a monster appears
    app_model_take_events(&m);
    press(IN_UP, 5800);
    assert(m.path_energy == 0 && (m.events & APP_EV_ENERGY_CHANGED));
    app_model_energy_saved(&m, false);
    assert(m.path_energy == 1 && m.battle_save_failed);
    app_model_take_events(&m);
    press(IN_UP, 5900);
    app_model_energy_saved(&m, true);
    assert(m.path_energy == 0 && !m.battle_save_failed);
    app_model_take_events(&m);
    press(IN_OK, 6000);
    press(IN_OK, 6100);
    app_model_energy_saved(&m, false);
    assert(m.path_energy == 0 && m.battle_stage == BATTLE_ENCOUNTER && m.battle_save_failed);
    app_model_take_events(&m);
    press(IN_OK, 6200);
    app_model_energy_saved(&m, true);
    assert(m.path_energy == 1 && m.battle_stage == BATTLE_HIT && !m.battle_save_failed);
    app_model_tick(&m, 6450);
    press(IN_UP, 6500);
    assert(m.path_energy == 0 && m.battle_stage == BATTLE_RECOVER);
    press(IN_HOLD_OK, 6600);
    press(IN_DOWN, 6700);
    assert(m.nav == NAV_SETTINGS && m.page == PAGE_HOME);
    app_model_tick(&m, 9500);
    assert(m.companion.motion == COMPANION_MOTION_IDLE);
    enter();
    m.path_energy = UINT32_MAX;
    press(IN_OK, 2400); press(IN_OK, 2500);
    assert(m.path_energy == UINT32_MAX && m.battle_stage == BATTLE_ENCOUNTER);
    m.battle_storage_ready = false; m.path_energy = 5;
    press(IN_OK, 2600);
    assert(m.path_energy == 5 && m.battle_stage == BATTLE_ENCOUNTER);
    enter();
    m.clock_set = true; m.clock_s = 3600;
    assert(!app_companion_asleep(&m));
    m.focus.state = FOCUS_RUNNING; m.focus.elapsed_ms = APP_FOCUS_COUNTDOWN_MS-100;
    m.focus.last_ms=2200;
    app_model_tick(&m, 2300);
    assert(m.page == PAGE_BATTLE && m.battle_stage == BATTLE_READY);
    enter();
    press(IN_OK, 2400); press(IN_OK, 2500);
    app_model_tick(&m, 2800);
    app_model_handle_queued(&m, IN_OK, 2600, 2800);
    assert(m.battle_stage == BATTLE_RECOVER && m.path_energy == 1);
    app_model_handle_queued(&m, IN_OK, 2800, 2800);
    assert(m.battle_stage == BATTLE_RECOVER);
    // Recovery-period OK is discarded even if a delayed callback arrives once
    // the automatic next encounter has appeared.
    app_model_tick(&m, 3500);
    app_model_handle_queued(&m, IN_OK, 2900, 3500);
    assert(m.battle_stage == BATTLE_ENCOUNTER && m.path_energy == 1);
    puts("Battle navigation: PASS");
    return 0;
}
