// Pure PCM renderer shared by firmware and host audio checks.
#pragma once
#include <stdint.h>

#define KEY_SOUND_HZ 16000U
#define KEY_SOUND_SAMPLES (KEY_SOUND_HZ * 12U / 1000U)
#define KEY_SOUND_BELL_SAMPLES (KEY_SOUND_HZ * 120U / 1000U)
#define KEY_SOUND_CHIME_SAMPLES (KEY_SOUND_HZ * 600U / 1000U)

void key_sound_render(int16_t pcm[KEY_SOUND_SAMPLES]);
// Render in small, independently addressable blocks rather than a full-clip buffer.
void key_sound_bell_render(int16_t *pcm, unsigned count, unsigned offset);
void key_sound_chime_render(int16_t *pcm, unsigned count, unsigned offset);
