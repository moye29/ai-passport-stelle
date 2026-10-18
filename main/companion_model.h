// main/companion_model.h - which motion Stelle is playing (pure C).
//
// Motions are one-shot clips that return to idle. Durations match the sprite
// cycles in generated/pack/stelle_pack.c (checked by host tests), so a
// clip always ends on its last frame before idle resumes.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    COMPANION_ACTION_WAVE = 0,
    COMPANION_ACTION_CHEER,
    COMPANION_ACTION_JUMP,
    COMPANION_ACTION_BAT_SWING,
    COMPANION_ACTION_COUNT,
} companion_action_t;

// Values match the exported sprite states (STELLE_IDLE..STELLE_CELEBRATION).
typedef enum {
    COMPANION_MOTION_IDLE = 0,
    COMPANION_MOTION_GREETING,
    COMPANION_MOTION_FOCUS,
    COMPANION_MOTION_CELEBRATION,
    COMPANION_MOTION_BAT_SWING,
    COMPANION_MOTION_COUNT,
} companion_motion_t;

typedef struct {
    companion_action_t selected;   // Play page choice
    companion_action_t last_action; // what the current/last Play motion was
    bool from_play;                // current motion was started on the Play page
    companion_motion_t motion;
    uint8_t repeats;               // remaining plays including the current one
    bool hold;                     // after one play, stay on the last frame
    uint32_t started_ms;
} companion_model_t;

void companion_model_init(companion_model_t *c);
void companion_model_select_previous(companion_model_t *c);
void companion_model_select_next(companion_model_t *c);
// Play the selected Play action once.
void companion_model_play_selected(companion_model_t *c, uint32_t now_ms);
// Play a motion `repeats` times (>= 1) and then return to idle.
void companion_model_play(companion_model_t *c, companion_motion_t motion, uint8_t repeats, uint32_t now_ms);
// Play a motion once and then hold its last frame until another motion replaces it.
void companion_model_play_hold(companion_model_t *c, companion_motion_t motion, uint32_t now_ms);
// Returns true when a clip finished and the companion returned to idle.
bool companion_model_tick(companion_model_t *c, uint32_t now_ms);
uint32_t companion_model_elapsed(const companion_model_t *c, uint32_t now_ms);
uint32_t companion_motion_duration_ms(companion_motion_t motion);
companion_motion_t companion_action_motion(companion_action_t action);
const char *companion_action_name(companion_action_t action);
