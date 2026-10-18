// main/ui_widgets.h - pixel-panel primitives built on plain LVGL objects.
//
// Panels draw hard 2 px outlines with stepped (chamfered) corners and a 2 px
// grey drop shadow in a draw callback, so no bitmap of the UI is used. Text is
// rendered by lv_label with the 1-bpp pixel fonts, never anti-aliased.
// To fit the 24 KB LVGL pool, labels share one style per font/alignment and
// list rows draw their own checkbox instead of using child objects.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef enum {
    UI_PANEL_CARD = 0,   // off-white card, ink outline, shadow
    UI_PANEL_SOFT,       // light-grey card (list items, tiles, nav)
    UI_PANEL_SELECTED,   // dark fill with four gold corner marks
    UI_PANEL_DARK,       // header bar
    UI_PANEL_FRAME,      // outline + shadow only (room window)
    UI_PANEL_CHIP,       // small dark square inside the header
    UI_PANEL_NIGHT,      // dark card: ink outline, header fill, shadow
} ui_panel_kind_t;

enum {
    UI_PANEL_TAIL = 1u << 8,      // speech-bubble tail under the centre
    UI_PANEL_FLAT = 1u << 9,      // no drop shadow
    UI_PANEL_CHECKBOX = 1u << 10, // 11 x 11 checkbox at (9, 9)
    UI_PANEL_CHECKED = 1u << 11,  // ... ticked; also strikes through the row text
    UI_PANEL_CAPSULE = 1u << 12,  // thin rounded outline behind the clock digits
};

// The object is 2 px larger than w x h to hold the drop shadow; children are
// positioned relative to the panel's top-left corner as usual.
lv_obj_t *ui_panel(lv_obj_t *parent, int x, int y, int w, int h, unsigned kind_and_flags);
void ui_panel_set_kind(lv_obj_t *panel, ui_panel_kind_t kind);
void ui_panel_set_flags(lv_obj_t *panel, unsigned flags, bool on);
void ui_panel_set_strike(lv_obj_t *panel, int x, int width);  // strike-through line for checked rows

lv_obj_t *ui_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align);
lv_obj_t *ui_text_bold(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w,
                       lv_text_align_t align);  // faux bold: twin drawn 1 px to the right
lv_obj_t *ui_icon(lv_obj_t *parent, const lv_font_t *font, const char *glyph, uint32_t color, int x, int y);
void ui_set_text(lv_obj_t *label, const char *text);   // no-op (and no redraw) when unchanged
void ui_set_color(lv_obj_t *label, uint32_t color);
void ui_set_hidden(lv_obj_t *obj, bool hidden);
void ui_set_x(lv_obj_t *obj, int x);
void ui_set_y(lv_obj_t *obj, int y);
void ui_set_w(lv_obj_t *obj, int w);
void ui_set_h(lv_obj_t *obj, int h);
int ui_text_width(const char *text, const lv_font_t *font);  // ink width, single line

lv_obj_t *ui_battery(lv_obj_t *parent, int x, int y);
// percent < 0: unknown level; charging draws a gold bolt over the cell.
void ui_battery_set(lv_obj_t *battery, int percent, bool charging);

lv_obj_t *ui_slider(lv_obj_t *parent, int x, int y, int w, int h);
void ui_slider_set(lv_obj_t *slider, unsigned filled, unsigned steps);

lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color);  // flat rectangle

lv_obj_t *ui_container(lv_obj_t *parent, int x, int y, int w, int h);

// Plain fill helpers for draw callbacks.
void ui_fill(lv_layer_t *layer, int x0, int y0, int x1, int y1, uint32_t color);
void ui_fill_chamfer(lv_layer_t *layer, int x0, int y0, int x1, int y1, int step, uint32_t color);
