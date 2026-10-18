#pragma once
#include "lvgl.h"
#include <stddef.h>
/* Adapter contract: use real application code, with explicit fixture data. */
lv_obj_t *pet_preview_create(void); /* Return a new screen, do not load it. */
size_t pet_preview_scene_count(void);
/* Establish fixture state and update the real view; no fake view renderer. */
void pet_preview_apply(size_t scene);
/* Stop scene-owned timers before deleting the screen and clear view handles. */
void pet_preview_destroy(void);
