#include "ui_widgets.h"

#include <string.h>
#include "ui_theme.h"

#define SHADOW 2  // offset drop shadow under every panel, like the Miki firmware
#define CORNER 0  // square corners everywhere (no stepped/rounded corners), as in Miki
#define TAIL_H 5

void ui_fill(lv_layer_t *layer, int x0, int y0, int x1, int y1, uint32_t color)
{
    if (x1 < x0 || y1 < y0) return;
    lv_draw_fill_dsc_t d;
    lv_draw_fill_dsc_init(&d);
    d.color = lv_color_hex(color);
    d.opa = LV_OPA_COVER;
    d.radius = 0;
    lv_area_t a = {x0, y0, x1, y1};
    lv_draw_fill(layer, &d, &a);
}

// Rectangle whose corners are cut by a `step`-pixel staircase.
void ui_fill_chamfer(lv_layer_t *layer, int x0, int y0, int x1, int y1, int step, uint32_t color)
{
    for (int i = 0; i < step; i++) {
        int inset = step - i;
        ui_fill(layer, x0 + inset, y0 + i, x1 - inset, y0 + i, color);
        ui_fill(layer, x0 + inset, y1 - i, x1 - inset, y1 - i, color);
    }
    ui_fill(layer, x0, y0 + step, x1, y1 - step, color);
}

static lv_obj_t *bare(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    return o;
}

lv_obj_t *ui_container(lv_obj_t *parent, int x, int y, int w, int h) { return bare(parent, x, y, w, h); }

/* ---------- panels ---------- */

// user_data bits: 0-7 kind, 8-15 flags, 16-23 strike x, 24-31 strike width / 2
static uintptr_t bits_of(const lv_obj_t *obj) { return (uintptr_t)lv_obj_get_user_data((lv_obj_t *)obj); }
static void set_bits(lv_obj_t *obj, uintptr_t bits)
{
    if (bits_of(obj) == bits) return;
    lv_obj_set_user_data(obj, (void *)bits);
    lv_obj_invalidate(obj);
}

static void corner_marks(lv_layer_t *l, int x0, int y0, int x1, int y1)
{
    const int in = 3, arm = 8, t = 2;
    ui_fill(l, x0 + in, y0 + in, x0 + in + arm - 1, y0 + in + t - 1, UI_GOLD);
    ui_fill(l, x0 + in, y0 + in, x0 + in + t - 1, y0 + in + arm - 1, UI_GOLD);
    ui_fill(l, x1 - in - arm + 1, y0 + in, x1 - in, y0 + in + t - 1, UI_GOLD);
    ui_fill(l, x1 - in - t + 1, y0 + in, x1 - in, y0 + in + arm - 1, UI_GOLD);
    ui_fill(l, x0 + in, y1 - in - t + 1, x0 + in + arm - 1, y1 - in, UI_GOLD);
    ui_fill(l, x0 + in, y1 - in - arm + 1, x0 + in + t - 1, y1 - in, UI_GOLD);
    ui_fill(l, x1 - in - arm + 1, y1 - in - t + 1, x1 - in, y1 - in, UI_GOLD);
    ui_fill(l, x1 - in - t + 1, y1 - in - arm + 1, x1 - in, y1 - in, UI_GOLD);
}

static void tail(lv_layer_t *l, int cx, int y1, uint32_t fill)
{
    // Downward speech tail cut into the bottom outline; drop shadow first.
    for (int k = 0; k <= TAIL_H; k++) {
        int half = TAIL_H - k;
        ui_fill(l, cx - half + SHADOW, y1 + k - 1 + SHADOW, cx + half + SHADOW, y1 + k - 1 + SHADOW, UI_BG_SHADOW);
    }
    for (int k = 0; k <= TAIL_H; k++) {
        int y = y1 - 1 + k, half = TAIL_H - k;
        ui_fill(l, cx - half, y, cx + half, y, UI_INK);
        if (half >= 2) ui_fill(l, cx - half + 2, y, cx + half - 2, y, fill);
    }
}

// Thin rounded "capsule" outline behind the clock digits: each arc is merged
// into vertical runs so it costs a handful of fills, not one per row.
static void capsule(lv_layer_t *l, int xl, int xr, int y0, int y1)
{
    int h = y1 - y0 + 1, r = h / 2;
    int run_start = y0, prev = -1;
    for (int y = y0; y <= y1 + 1; y++) {
        int off = -1;
        if (y <= y1) {
            int dy = y - y0 < r ? r - (y - y0) : (y - y0) - (h - 1 - r);
            if (dy < 0) dy = 0;
            int s = (int)(r * r - dy * dy);
            int root = 0;
            while ((root + 1) * (root + 1) <= s) root++;
            off = (r - root) / 3;  // a shallow arc
        }
        if (off != prev) {
            if (prev >= 0) {
                ui_fill(l, xl + prev, run_start, xl + prev, y - 1, UI_BG_RIVET);
                ui_fill(l, xr - prev, run_start, xr - prev, y - 1, UI_BG_RIVET);
            }
            run_start = y;
            prev = off;
        }
    }
}

static void panel_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    uintptr_t bits = bits_of(obj);
    ui_panel_kind_t kind = (ui_panel_kind_t)(bits & 0xFF);
    bool selected = kind == UI_PANEL_SELECTED;
    int x0 = a.x1, y0 = a.y1, x1 = a.x2 - SHADOW, y1 = a.y2 - SHADOW;
    if (bits & UI_PANEL_TAIL) y1 -= TAIL_H;
    bool shadow = !(bits & UI_PANEL_FLAT) && kind != UI_PANEL_DARK && kind != UI_PANEL_CHIP;
    uint32_t fill = kind == UI_PANEL_CARD ? UI_BG_PAPER
                  : kind == UI_PANEL_SOFT ? UI_BG_PANEL
                  : kind == UI_PANEL_SELECTED ? UI_SELECT
                  : kind == UI_PANEL_CHIP ? UI_CHIP : UI_HEADER;  // DARK and NIGHT
    if (shadow) ui_fill_chamfer(l, x0 + SHADOW, y0 + SHADOW, x1 + SHADOW, y1 + SHADOW, CORNER, UI_BG_SHADOW);
    if (kind == UI_PANEL_FRAME) {  // outline only: the room image fills the inside
        ui_fill(l, x0, y0, x1, y0 + 1, UI_INK);
        ui_fill(l, x0, y1 - 1, x1, y1, UI_INK);
        ui_fill(l, x0, y0 + 2, x0 + 1, y1 - 2, UI_INK);
        ui_fill(l, x1 - 1, y0 + 2, x1, y1 - 2, UI_INK);
        return;
    }
    ui_fill_chamfer(l, x0, y0, x1, y1, CORNER, kind == UI_PANEL_DARK ? UI_EDGE : UI_INK);
    ui_fill_chamfer(l, x0 + 2, y0 + 2, x1 - 2, y1 - 2, CORNER, fill);
    if (kind == UI_PANEL_CARD || kind == UI_PANEL_SOFT)  // 1 px inner highlight along the top
        ui_fill(l, x0 + 3, y0 + 2, x1 - 3, y0 + 2, 0xFFFFFF);
    if (selected) corner_marks(l, x0, y0, x1, y1);
    if (kind == UI_PANEL_CARD) {  // small rivet dots in the inner corners, as in the reference
        ui_fill(l, x0 + 4, y0 + 4, x0 + 4, y0 + 4, UI_BG_RIVET);
        ui_fill(l, x1 - 4, y0 + 4, x1 - 4, y0 + 4, UI_BG_RIVET);
        ui_fill(l, x0 + 4, y1 - 4, x0 + 4, y1 - 4, UI_BG_RIVET);
        ui_fill(l, x1 - 4, y1 - 4, x1 - 4, y1 - 4, UI_BG_RIVET);
    }
    if (bits & UI_PANEL_CAPSULE) capsule(l, x0 + 37, x1 - 37, y0 + 3, y1 - 3);
    if (bits & UI_PANEL_CHECKBOX) {
        int bx = x0 + 9, by = y0 + (y1 - y0 + 1 - 11) / 2;
        ui_fill(l, bx, by, bx + 10, by + 10, selected ? UI_BG_PANEL : UI_INK);
        ui_fill(l, bx + 2, by + 2, bx + 8, by + 8, selected ? UI_SELECT : UI_BG_PAPER);
    }
    if (bits & UI_PANEL_CHECKED) {
        int sx = x0 + (int)((bits >> 16) & 0xFF), sw = (int)((bits >> 24) & 0xFF) * 2;
        int sy = y0 + (y1 - y0 + 1) / 2;
        if (sw) ui_fill(l, sx, sy, sx + sw - 1, sy, selected ? UI_BG_PANEL : UI_MID);
    }
    if (bits & UI_PANEL_TAIL) tail(l, (x0 + x1) / 2, y1, fill);
}

lv_obj_t *ui_panel(lv_obj_t *parent, int x, int y, int w, int h, unsigned kind_and_flags)
{
    int extra = (kind_and_flags & UI_PANEL_TAIL) ? TAIL_H : 0;
    lv_obj_t *o = bare(parent, x, y, w + SHADOW, h + SHADOW + extra);
    lv_obj_set_user_data(o, (void *)(uintptr_t)kind_and_flags);
    lv_obj_add_event_cb(o, panel_draw, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}

void ui_panel_set_kind(lv_obj_t *panel, ui_panel_kind_t kind)
{
    set_bits(panel, (bits_of(panel) & ~(uintptr_t)0xFF) | (uintptr_t)kind);
}

void ui_panel_set_flags(lv_obj_t *panel, unsigned flags, bool on)
{
    uintptr_t b = bits_of(panel);
    set_bits(panel, on ? (b | flags) : (b & ~(uintptr_t)flags));
}

void ui_panel_set_strike(lv_obj_t *panel, int x, int width)
{
    uintptr_t b = bits_of(panel) & 0xFFFFu;
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    if (width < 0) width = 0;
    if (width > 510) width = 510;
    set_bits(panel, b | ((uintptr_t)x << 16) | ((uintptr_t)((width + 1) / 2) << 24));
}

/* ---------- labels ---------- */

// One shared style per (font, alignment): labels then only carry local x/y/w
// and text colour, which keeps each label small in the LVGL pool.
#define STYLE_SLOTS 16
static struct {
    const lv_font_t *font;
    lv_text_align_t align;
    lv_style_t style;
} styles[STYLE_SLOTS];

static lv_style_t *shared_style(const lv_font_t *font, lv_text_align_t align)
{
    for (int i = 0; i < STYLE_SLOTS; i++) {
        if (styles[i].font == font && styles[i].align == align) return &styles[i].style;
        if (styles[i].font == NULL) {
            styles[i].font = font;
            styles[i].align = align;
            lv_style_init(&styles[i].style);
            lv_style_set_text_font(&styles[i].style, font);
            lv_style_set_text_align(&styles[i].style, align);
            return &styles[i].style;
        }
    }
    LV_ASSERT_MSG(false, "ui_widgets: increase STYLE_SLOTS");
    return &styles[0].style;
}

lv_obj_t *ui_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(l, shared_style(font, align), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_text_static(l, "");
    return l;
}

// Faux-bold label: a twin child label drawn 1 px to the right. LVGL's
// built-in Montserrat has a single weight; ui_set_text/ui_set_color and the
// letter-spacing fit keep the twin in sync (marked with LV_OBJ_FLAG_USER_1).
lv_obj_t *ui_text_bold(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w,
                       lv_text_align_t align)
{
    lv_obj_t *l = ui_text(parent, font, color, x, y, w, align);
    lv_obj_add_flag(l, LV_OBJ_FLAG_USER_1 | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    ui_text(l, font, color, 1, 0, w, align);
    return l;
}

static lv_obj_t *bold_twin(lv_obj_t *label)
{
    return lv_obj_has_flag(label, LV_OBJ_FLAG_USER_1) ? lv_obj_get_child(label, 0) : NULL;
}

int ui_text_width(const char *text, const lv_font_t *font)
{
    lv_point_t p;
    lv_text_get_size(&p, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    // Each glyph advance ends with a gap of one source pixel times the scale.
    lv_font_glyph_dsc_t g;
    int gap = 1;
    if (lv_font_get_glyph_dsc(font, &g, '0', 0) && g.box_w) gap = (g.adv_w - g.box_w - g.ofs_x) > 0 ? (g.adv_w - g.box_w - g.ofs_x) : 1;
    return p.x > gap ? p.x - gap : 0;
}

lv_obj_t *ui_icon(lv_obj_t *parent, const lv_font_t *font, const char *glyph, uint32_t color, int x, int y)
{
    lv_obj_t *l = ui_text(parent, font, color, x, y, 1, LV_TEXT_ALIGN_LEFT);
    ui_set_text(l, glyph);
    return l;
}

void ui_set_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) == 0) return;
    lv_label_set_text(label, text);
    lv_obj_t *twin = bold_twin(label);  // inherits the letter spacing set below
    if (twin) lv_label_set_text(twin, text);
    // Icon labels (width 1 at creation) track their glyph's exact width.
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    if (font->line_height >= 15 && font->base_line == 0 && (unsigned char)text[0] == 0xEE) {
        lv_point_t p;
        lv_text_get_size(&p, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        ui_set_w(label, p.x);
        return;
    }
    // A line that is a few pixels too wide for its box gets 1 px tighter
    // letter spacing instead of being clipped; normal lines keep 0.
    lv_point_t p;
    lv_text_get_size(&p, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int space = p.x - 1 > lv_obj_get_style_width(label, 0) ? -1 : 0;
    if (lv_obj_get_style_text_letter_space(label, 0) != space) lv_obj_set_style_text_letter_space(label, space, 0);
}

void ui_set_color(lv_obj_t *label, uint32_t color)
{
    lv_color_t c = lv_color_hex(color);
    if (lv_color_eq(lv_obj_get_style_text_color(label, 0), c)) return;
    lv_obj_set_style_text_color(label, c, 0);
    lv_obj_t *twin = bold_twin(label);
    if (twin) lv_obj_set_style_text_color(twin, c, 0);
}

void ui_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Geometry setters compare against the stored style value (not layout
// coordinates, which are stale until the next refresh) to avoid redraws.
void ui_set_x(lv_obj_t *obj, int x)
{
    if (lv_obj_get_style_x(obj, 0) != x) lv_obj_set_x(obj, x);
}

void ui_set_y(lv_obj_t *obj, int y)
{
    if (lv_obj_get_style_y(obj, 0) != y) lv_obj_set_y(obj, y);
}

void ui_set_w(lv_obj_t *obj, int w)
{
    if (lv_obj_get_style_width(obj, 0) != w) lv_obj_set_width(obj, w);
}

void ui_set_h(lv_obj_t *obj, int h)
{
    if (lv_obj_get_style_height(obj, 0) != h) lv_obj_set_height(obj, h);
}

/* ---------- battery, slider, box ---------- */

// Charging bolt, 7 x 9, one bit per pixel (bit 6 = leftmost column):
// ....###  ...###.  ..###..  .######  ######.  ..###..  .###...  ###....  ##.....
static const uint8_t BOLT[9] = {0x07, 0x0E, 0x1C, 0x3F, 0x7E, 0x1C, 0x38, 0x70, 0x60};
#define BOLT_H 9
#define BOLT_W 7

// Draw each row of a 1-bit mask as horizontal runs (few fills, hard edges).
static void mask_runs(lv_layer_t *l, int x, int y, const uint16_t *rows, int n, int width, uint32_t color)
{
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < width;) {
            if (!(rows[r] & (1u << (width - 1 - c)))) { c++; continue; }
            int start = c;
            while (c < width && (rows[r] & (1u << (width - 1 - c)))) c++;
            ui_fill(l, x + start, y + r, x + c - 1, y + r, color);
        }
    }
}

// Gold bolt with a one-pixel dark halo so it reads over any fill colour.
static void bolt(lv_layer_t *l, int x, int y)
{
    uint16_t body[BOLT_H + 2] = {0}, halo[BOLT_H + 2] = {0};
    for (int r = 0; r < BOLT_H; r++) body[r + 1] = (uint16_t)(BOLT[r] << 1);  // 1 px margin all round
    for (int r = 0; r < BOLT_H + 2; r++) {
        uint16_t m = body[r] | (r ? body[r - 1] : 0) | (r < BOLT_H + 1 ? body[r + 1] : 0);
        halo[r] = (uint16_t)((m | (m << 1) | (m >> 1)) & 0x1FF);
    }
    mask_runs(l, x - 1, y - 1, halo, BOLT_H + 2, BOLT_W + 2, UI_HEADER);
    mask_runs(l, x - 1, y - 1, body, BOLT_H + 2, BOLT_W + 2, UI_GOLD);
}

// Battery: 30 x 11 two-pixel outline with a 2 x 5 nub; the fill turns gold below
// 20 %. While charging a lightning bolt sits in the middle of the cell.
static void battery_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    intptr_t v = (intptr_t)lv_obj_get_user_data(obj);
    int percent = (int)(v & 0xFF) - 1;
    bool charging = (v >> 8) & 1;
    int x = a.x1, y = a.y1;
    ui_fill_chamfer(l, x, y, x + 29, y + 10, CORNER, UI_BG_RIVET);
    ui_fill(l, x + 2, y + 2, x + 27, y + 8, UI_INK);
    ui_fill(l, x + 30, y + 3, x + 31, y + 7, UI_BG_RIVET);
    if (percent >= 0) {
        int w = (24 * percent + 99) / 100;
        if (w > 0) ui_fill(l, x + 3, y + 3, x + 2 + w, y + 7, percent < 20 ? UI_GOLD : UI_BATTERY);
    }
    if (charging) bolt(l, x + 15 - BOLT_W / 2, y + 1);
}

lv_obj_t *ui_battery(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *o = bare(parent, x, y, 32, 11);
    lv_obj_set_user_data(o, (void *)(intptr_t)0);
    lv_obj_add_event_cb(o, battery_draw, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}

void ui_battery_set(lv_obj_t *battery, int percent, bool charging)
{
    if (percent > 100) percent = 100;
    intptr_t v = (percent < 0 ? 0 : percent + 1) | (charging ? 0x100 : 0);
    if ((intptr_t)lv_obj_get_user_data(battery) == v) return;
    lv_obj_set_user_data(battery, (void *)v);
    lv_obj_invalidate(battery);
}

// Segmented slider: ink frame, gold filled steps, light empty steps, shadow.
static void slider_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int x0 = a.x1, y0 = a.y1, x1 = a.x2 - SHADOW, y1 = a.y2 - SHADOW;
    unsigned v = (unsigned)(uintptr_t)lv_obj_get_user_data(obj);
    unsigned filled = v & 0xFF, steps = (v >> 8) & 0xFF;
    ui_fill_chamfer(l, x0 + SHADOW, y0 + SHADOW, x1 + SHADOW, y1 + SHADOW, CORNER, UI_BG_SHADOW);
    ui_fill_chamfer(l, x0, y0, x1, y1, CORNER, UI_INK);
    if (!steps) return;
    int inner_x = x0 + 3, inner_w = (x1 - 3) - inner_x + 1, gap = 2;
    int seg = (inner_w - gap * ((int)steps - 1)) / (int)steps;
    int used = seg * (int)steps + gap * ((int)steps - 1);
    int x = inner_x + (inner_w - used) / 2;
    for (unsigned i = 0; i < steps; i++, x += seg + gap) {
        bool on = i < filled;
        ui_fill(l, x, y0 + 3, x + seg - 1, y1 - 3, on ? UI_GOLD : UI_BG_PANEL);
        if (on) ui_fill(l, x, y0 + 3, x + seg - 1, y0 + 3, 0xFBDDA0);
    }
}

lv_obj_t *ui_slider(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = bare(parent, x, y, w + SHADOW, h + SHADOW);
    lv_obj_add_event_cb(o, slider_draw, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}

void ui_slider_set(lv_obj_t *slider, unsigned filled, unsigned steps)
{
    uintptr_t v = (filled & 0xFF) | ((steps & 0xFF) << 8);
    if ((uintptr_t)lv_obj_get_user_data(slider) == v) return;
    lv_obj_set_user_data(slider, (void *)v);
    lv_obj_invalidate(slider);
}

static void box_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    ui_fill(lv_event_get_layer(e), a.x1, a.y1, a.x2, a.y2, (uint32_t)(uintptr_t)lv_obj_get_user_data(obj));
}

lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = bare(parent, x, y, w, h);
    lv_obj_set_user_data(o, (void *)(uintptr_t)color);
    lv_obj_add_event_cb(o, box_draw, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}
