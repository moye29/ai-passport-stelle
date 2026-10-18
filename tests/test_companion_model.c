#include "companion_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_action_selection_wraps(void)
{
    companion_model_t c;
    companion_model_init(&c);
    assert(c.selected == COMPANION_ACTION_WAVE);
    companion_model_select_previous(&c);
    assert(c.selected == COMPANION_ACTION_BAT_SWING);
    companion_model_select_next(&c);
    assert(c.selected == COMPANION_ACTION_WAVE);
    companion_model_select_next(&c);
    assert(c.selected == COMPANION_ACTION_CHEER);
    companion_model_select_next(&c);
    assert(c.selected == COMPANION_ACTION_JUMP);
    companion_model_select_next(&c);
    assert(c.selected == COMPANION_ACTION_BAT_SWING);
}

static void test_play_selected_runs_once_then_idles(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_select_next(&c);  // Cheer -> focus clip
    companion_model_play_selected(&c, 1000);
    assert(c.motion == COMPANION_MOTION_FOCUS && c.from_play && c.last_action == COMPANION_ACTION_CHEER);
    assert(companion_model_elapsed(&c, 1250) == 250);
    uint32_t clip = companion_motion_duration_ms(COMPANION_MOTION_FOCUS);
    assert(!companion_model_tick(&c, 1000 + clip - 1));
    assert(companion_model_tick(&c, 1000 + clip));
    assert(c.motion == COMPANION_MOTION_IDLE);
    assert(companion_model_elapsed(&c, 5000) == 0);
}

static void test_repeats_play_whole_clips(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_play(&c, COMPANION_MOTION_CELEBRATION, 4, 0);
    uint32_t clip = companion_motion_duration_ms(COMPANION_MOTION_CELEBRATION);
    assert(!c.from_play);
    assert(!companion_model_tick(&c, clip * 4 - 1));
    assert(c.motion == COMPANION_MOTION_CELEBRATION);
    assert(companion_model_elapsed(&c, clip * 4 - 1) == clip - 1);  // restarts at frame 0 each clip
    assert(companion_model_tick(&c, clip * 4));
    assert(c.motion == COMPANION_MOTION_IDLE);
}

static void test_late_tick_still_finishes(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_play(&c, COMPANION_MOTION_GREETING, 2, 100);
    assert(companion_model_tick(&c, 100000));  // a long stall never leaves a clip stuck
    assert(c.motion == COMPANION_MOTION_IDLE);
}

static void test_clock_wrap_is_safe(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_play(&c, COMPANION_MOTION_GREETING, 1, UINT32_MAX - 100);
    assert(companion_model_elapsed(&c, 49) == 150);
    assert(!companion_model_tick(&c, 49));
}

static void test_action_mapping(void)
{
    assert(companion_action_motion(COMPANION_ACTION_WAVE) == COMPANION_MOTION_GREETING);
    assert(companion_action_motion(COMPANION_ACTION_CHEER) == COMPANION_MOTION_FOCUS);
    assert(companion_action_motion(COMPANION_ACTION_JUMP) == COMPANION_MOTION_CELEBRATION);
    assert(companion_action_motion(COMPANION_ACTION_BAT_SWING) == COMPANION_MOTION_BAT_SWING);
    assert(companion_motion_duration_ms(COMPANION_MOTION_BAT_SWING) == 800);
    assert(strcmp(companion_action_name(COMPANION_ACTION_BAT_SWING), "Bat Swing") == 0);
}

static void test_hold_parks_on_the_last_frame(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_play_hold(&c, COMPANION_MOTION_FOCUS, 100);
    uint32_t clip = companion_motion_duration_ms(COMPANION_MOTION_FOCUS);
    assert(companion_model_elapsed(&c, 100 + 50) == 50);
    assert(!companion_model_tick(&c, 100 + clip * 5));
    assert(c.motion == COMPANION_MOTION_FOCUS);
    assert(companion_model_elapsed(&c, 100 + clip * 5) == clip - 1);
    companion_model_play(&c, COMPANION_MOTION_CELEBRATION, 1, 9000);  // replaced: hold ends
    assert(!c.hold);
    companion_model_play_hold(&c, COMPANION_MOTION_IDLE, 9000);        // idle never holds
    assert(!c.hold && c.motion == COMPANION_MOTION_IDLE);
}
static void test_faster_bat_returns_to_idle_after_eight_frames(void)
{
    companion_model_t c;
    companion_model_init(&c);
    companion_model_play(&c, COMPANION_MOTION_BAT_SWING, 1, 1000);
    assert(!companion_model_tick(&c, 1799));
    assert(c.motion == COMPANION_MOTION_BAT_SWING);
    assert(companion_model_tick(&c, 1800));
    assert(c.motion == COMPANION_MOTION_IDLE);
}

int main(void)
{
    test_faster_bat_returns_to_idle_after_eight_frames();
    test_action_selection_wraps();
    test_play_selected_runs_once_then_idles();
    test_repeats_play_whole_clips();
    test_late_tick_still_finishes();
    test_clock_wrap_is_safe();
    test_action_mapping();
    test_hold_parks_on_the_last_frame();
    puts("companion model tests: PASS");
    return 0;
}
