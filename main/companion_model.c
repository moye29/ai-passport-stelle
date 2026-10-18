#include "companion_model.h"

#include <stddef.h>

// Sum of each sprite state's frame durations (sprites.json).
static const uint32_t MOTION_MS[COMPANION_MOTION_COUNT] = {
    [COMPANION_MOTION_IDLE] = 0,
    [COMPANION_MOTION_GREETING] = 1000,
    [COMPANION_MOTION_FOCUS] = 1800,
    [COMPANION_MOTION_CELEBRATION] = 1000,
    [COMPANION_MOTION_BAT_SWING] = 800,
};

void companion_model_init(companion_model_t *c)
{
    if (c == NULL) return;
    c->selected = COMPANION_ACTION_WAVE;
    c->last_action = COMPANION_ACTION_WAVE;
    c->from_play = false;
    c->motion = COMPANION_MOTION_IDLE;
    c->repeats = 0;
    c->hold = false;
    c->started_ms = 0;
}

void companion_model_select_previous(companion_model_t *c)
{
    if (c == NULL) return;
    c->selected = c->selected == 0 ? COMPANION_ACTION_COUNT - 1
                                   : (companion_action_t)(c->selected - 1);
}

void companion_model_select_next(companion_model_t *c)
{
    if (c == NULL) return;
    c->selected = (companion_action_t)((c->selected + 1) % COMPANION_ACTION_COUNT);
}

void companion_model_play(companion_model_t *c, companion_motion_t motion, uint8_t repeats, uint32_t now_ms)
{
    if (c == NULL || motion >= COMPANION_MOTION_COUNT) return;
    c->motion = motion;
    c->repeats = motion == COMPANION_MOTION_IDLE ? 0 : (repeats ? repeats : 1);
    c->started_ms = now_ms;
    c->from_play = false;
    c->hold = false;
}

void companion_model_play_hold(companion_model_t *c, companion_motion_t motion, uint32_t now_ms)
{
    companion_model_play(c, motion, 1, now_ms);
    if (c && motion != COMPANION_MOTION_IDLE) c->hold = true;
}

void companion_model_play_selected(companion_model_t *c, uint32_t now_ms)
{
    if (c == NULL) return;
    companion_model_play(c, companion_action_motion(c->selected), 1, now_ms);
    c->last_action = c->selected;
    c->from_play = true;
}

uint32_t companion_model_elapsed(const companion_model_t *c, uint32_t now_ms)
{
    if (c == NULL || c->motion == COMPANION_MOTION_IDLE) return 0;
    uint32_t t = now_ms - c->started_ms, clip = MOTION_MS[c->motion];
    return c->hold && t >= clip ? clip - 1 : t;  // held clips stay on their last frame
}

bool companion_model_tick(companion_model_t *c, uint32_t now_ms)
{
    if (c == NULL || c->motion == COMPANION_MOTION_IDLE) return false;
    uint32_t clip = MOTION_MS[c->motion];
    if (c->hold) return false;  // stays on the last frame until replaced
    while (c->repeats && now_ms - c->started_ms >= clip) {
        c->started_ms += clip;
        c->repeats--;
    }
    if (c->repeats) return false;
    c->motion = COMPANION_MOTION_IDLE;
    c->started_ms = now_ms;
    return true;
}

uint32_t companion_motion_duration_ms(companion_motion_t motion)
{
    return motion < COMPANION_MOTION_COUNT ? MOTION_MS[motion] : 0;
}

companion_motion_t companion_action_motion(companion_action_t action)
{
    static const companion_motion_t map[COMPANION_ACTION_COUNT] = {
        COMPANION_MOTION_GREETING, COMPANION_MOTION_FOCUS,
        COMPANION_MOTION_CELEBRATION, COMPANION_MOTION_BAT_SWING,
    };
    return action < COMPANION_ACTION_COUNT ? map[action] : COMPANION_MOTION_GREETING;
}

const char *companion_action_name(companion_action_t action)
{
    static const char *const names[COMPANION_ACTION_COUNT] = {"Wave", "Cheer", "Jump", "Bat Swing"};
    return action < COMPANION_ACTION_COUNT ? names[action] : "Wave";
}
