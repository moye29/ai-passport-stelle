#include "preview_adapter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PREVIEW_CYCLES
#define PREVIEW_CYCLES 24
#endif
#ifndef PREVIEW_WIDTH
#define PREVIEW_WIDTH 240
#endif
#ifndef PREVIEW_HEIGHT
#define PREVIEW_HEIGHT 320
#endif
static uint16_t pixels[PREVIEW_WIDTH * PREVIEW_HEIGHT];
static uint16_t draw_buffer[PREVIEW_WIDTH * 24];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *map)
{
    uint16_t *source = (uint16_t *)map;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++)
            pixels[y * PREVIEW_WIDTH + x] = *source++;
    lv_display_flush_ready(display);
}

static int snapshot(const char *directory, size_t scene)
{
    char path[2048];
    int length = snprintf(path, sizeof(path), "%s/scene-%03zu.ppm", directory, scene);
    if (length < 0 || (size_t)length >= sizeof(path)) return 1;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", PREVIEW_WIDTH, PREVIEW_HEIGHT);
    for (size_t i = 0; i < PREVIEW_WIDTH * PREVIEW_HEIGHT; i++) {
        uint16_t p = pixels[i];
        unsigned char rgb[] = {((p >> 11) & 31) * 255 / 31,
                              ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31};
        if (fwrite(rgb, 1, 3, f) != 3) { fclose(f); return 1; }
    }
    return fclose(f) != 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: pet_preview existing-output-directory\n"); return 2; }
    lv_init();
    lv_display_t *display = lv_display_create(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    size_t count = pet_preview_scene_count();
    if (!count) { fprintf(stderr, "Adapter has no scenes\n"); return 2; }
    lv_obj_t *blank = lv_screen_active();
    lv_screen_load(pet_preview_create()); lv_obj_delete(blank);
    for (size_t i = 0; i < count; i++) {
        pet_preview_apply(i); lv_refr_now(NULL);
        if (snapshot(argv[1], i)) return 1;
    }
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    for (size_t cycle = 0; cycle < PREVIEW_CYCLES; cycle++) {
        blank = lv_obj_create(NULL); lv_screen_load(blank);
        pet_preview_destroy();
        lv_screen_load(pet_preview_create()); lv_obj_delete(blank);
        /* End each cycle in the same scene as the initial pass. */
        for (size_t i = 0; i < count; i++) { pet_preview_apply(i); lv_refr_now(NULL); }
        if (cycle < 2 || cycle + 1 == PREVIEW_CYCLES) {
            lv_mem_monitor_t state;
            lv_mem_monitor(&state);
            printf("Lifecycle cycle %zu free=%lu\n",cycle+1,(unsigned long)state.free_size);
        }
    }
    lv_mem_monitor(&after);
    printf("LVGL free bytes before=%lu after=%lu largest=%lu; %d lifecycle cycles completed\n",
           (unsigned long)before.free_size, (unsigned long)after.free_size,
           (unsigned long)after.free_biggest_size, PREVIEW_CYCLES);
    puts("Host render only; fixture data is simulated; inspect memory deltas and screenshots.");
    return 0;
}
