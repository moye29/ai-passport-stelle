#include "pet_view.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "asset_pack.h"
#include "battle_pack.h"
#include "anniv_art.h"
#include "anniv_image.h"
#include "enemy_b_image.h"
#include "ui_assets.h"
#include "ui_copy.h"
#include "ui_theme.h"
#include "ui_widgets.h"

// Text uses LVGL's anti-aliased (4 bpp) Montserrat, as the Miki firmware does;
// the big clock digits stay the hand-drawn 18 x 22 pixel set.
#define F_BODY (&lv_font_montserrat_12)   // hints, nav labels, values
#define F_BOLD (&lv_font_montserrat_12)   // emphasised hint lines
#define F_MED (&lv_font_montserrat_14)    // list items, tile names
#define F_SPEECH (&lv_font_montserrat_14) // speech bubble
#define F_TITLE (&lv_font_montserrat_16)  // titles, STELLE, Play action (faux bold)
#define F_DIGITS (&ui_font_digits) // 18 x 22: clock and timer
#define F_ICON (&ui_icons_1x)
#define F_ICON2 (&ui_icons_2x)
#define F_ICON3 (&ui_icons_3x)
#define ICON_LINE 17               // ui_icons_1x line height: glyphs are centred in it
#define HISTORY_SLOTS 6 // max visible entries including the clipped top/bottom rows
LV_FONT_DECLARE(stelle_chat_font);

typedef struct {
    lv_obj_t *panel, *icon, *label;
} nav_slot_t;



typedef struct {
    lv_obj_t *panel, *icon, *label, *value;
} tile_t;

// Exactly one page layer exists at a time (room layer or one full page), so
// the LVGL pool only holds the objects that are on screen.
static struct {
    lv_obj_t *screen, *battery, *battery_text, *layer;
    bool built;
    app_page_t page;
    uint8_t setting;  // which setting the Adjust page was built for
    bool setup_from;  // whether the setup page was built for "back to Settings"
    union {
        struct {
            lv_obj_t *speech, *speech_small, *room_img, *sprite;
            lv_obj_t *voice_bubble, *voice_mark;
            lv_obj_t *info, *digits, *title, *sub, *right, *arrow_l, *arrow_r;
            lv_obj_t *energy;
            uint32_t energy_count;
            bool energy_plus;
            nav_slot_t nav[UI_NAV_VISIBLE];
            const lv_image_dsc_t *frame, *room_src;
        } room;
        struct { lv_obj_t *grid, *month, *date, *dur, *none, *entry_days, *empty; int32_t sel, today; bool known; } cal;
        struct { lv_obj_t *img, *days, *unit; } anniv;
        struct { tile_t tile[SET_COUNT]; lv_obj_t *hint; } set;
        struct { lv_obj_t *slider, *value, *min, *max, *note; } adj;
        struct { lv_obj_t *qr, *wait, *ap, *status; char qr_text[80]; } setup;
        struct { lv_obj_t *img; lv_obj_t *opt[SCENE_COUNT]; lv_obj_t *opt_label[SCENE_COUNT]; lv_obj_t *daily; } scene;
        struct { lv_obj_t *clip, *body[HISTORY_SLOTS], *role[HISTORY_SLOTS];
                 uint32_t revision; uint16_t scroll; bool synced; } history;
    } p;
} v;

// Layout y values were tuned for the pixel fonts (cap top 1 px below the
// label top). Montserrat sits lower in its line box, so shift it up to keep
// the same visual centre.
static int font_dy(const lv_font_t *font)
{
    if (font == &lv_font_montserrat_16) return -4;
    if (font == &lv_font_montserrat_14 || font == &lv_font_montserrat_12) return -3;
    return 0;
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align)
{
    if (font == F_TITLE) return ui_text_bold(parent, font, color, x, y + font_dy(font), w, align);
    return ui_text(parent, font, color, x, y + font_dy(font), w, align);
}

static void upper(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    for (; src[i] && i + 1 < n; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ---------- persistent chrome: bezel, rim, plate, header ---------- */

static void screen_draw(lv_event_t *e)
{
    lv_layer_t *l = lv_event_get_layer(e);
    const int b = UI_BEZEL, r = UI_BEZEL + UI_RIM_W;
    ui_fill(l, 0, 0, UI_W - 1, b - 1, UI_INK);
    ui_fill(l, 0, UI_H - b, UI_W - 1, UI_H - 1, UI_INK);
    ui_fill(l, 0, b, b - 1, UI_H - b - 1, UI_INK);
    ui_fill(l, UI_W - b, b, UI_W - 1, UI_H - b - 1, UI_INK);
    ui_fill(l, b, b, UI_W - b - 1, r - 1, UI_BG_RIM);
    ui_fill(l, b, UI_H - r, UI_W - b - 1, UI_H - b - 1, UI_BG_RIM);
    ui_fill(l, b, r, r - 1, UI_H - r - 1, UI_BG_RIM);
    ui_fill(l, UI_W - r, r, UI_W - b - 1, UI_H - r - 1, UI_BG_RIM);
}

static void build_header(void)
{
    lv_obj_t *h = ui_panel(v.screen, UI_HEADER_X, UI_HEADER_Y, UI_HEADER_W, UI_HEADER_H, UI_PANEL_DARK);
    lv_obj_t *chip = ui_panel(h, 3, 3, 21, 20, UI_PANEL_CHIP);
    ui_icon(chip, F_ICON, UI_ICON_STAR_L, UI_GOLD, 5, 1);
    lv_obj_t *name = text(h, F_TITLE, UI_PAPER, 30, 8, 90, LV_TEXT_ALIGN_LEFT);
    ui_set_text(name, "STELLE");
    v.battery_text = text(h, F_MED, UI_PANEL, 118, 6, 48, LV_TEXT_ALIGN_RIGHT);
    v.battery = ui_battery(h, 171, 7);
    ui_icon(h, F_ICON, UI_ICON_STAR_S, UI_GOLD, 209, 4);
}

lv_obj_t *pet_view_create(void)
{
    memset(&v, 0, sizeof(v));
    v.screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(v.screen);
    lv_obj_set_size(v.screen, UI_W, UI_H);
    lv_obj_remove_flag(v.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(v.screen, lv_color_hex(UI_BG_PLATE), 0);
    lv_obj_set_style_bg_opa(v.screen, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(v.screen, screen_draw, LV_EVENT_DRAW_MAIN, NULL);
    build_header();
    return v.screen;
}

void pet_view_destroy(void)
{
    if (v.screen) lv_obj_delete(v.screen);
    memset(&v, 0, sizeof(v));
}

/* ---------- room layer: Home / Focus / Play ---------- */

static void energy_line(lv_layer_t *layer, const lv_area_t *bounds, const char *value, int y)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = &ui_font_bold;
    d.letter_space = 1;
    d.text = value;
    d.text_local = 1; // Draw tasks can outlive the callback's formatted stack string.
    lv_point_t size;
    lv_text_get_size(&size, value, d.font, d.letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t a = {bounds->x2-7-size.x, bounds->y1+y, bounds->x2-7, bounds->y1+y+size.y-1};
    for (int distance=2; distance>=1; distance--) {
        d.color = lv_color_hex(distance == 2 ? 0xFFB822 : 0xF5BB37);
        d.opa = distance == 2 ? LV_OPA_30 : LV_OPA_COVER;
        for (int dy=-1; dy<=1; dy++) for (int dx=-1; dx<=1; dx++) {
            if (!dx && !dy) continue;
            lv_area_t stroke=a;
            lv_area_move(&stroke, dx*distance, dy*distance);
            lv_draw_label(layer, &d, &stroke);
        }
    }
    d.color = lv_color_hex(0xFFFFFF);
    d.opa = LV_OPA_COVER;
    lv_draw_label(layer, &d, &a);
}

static void energy_draw(lv_event_t *e)
{
    lv_area_t bounds;
    lv_obj_get_coords(lv_event_get_target_obj(e), &bounds);
    char value[40];
    snprintf(value, sizeof(value), "Path Energy = %lu", (unsigned long)v.p.room.energy_count);
    energy_line(lv_event_get_layer(e), &bounds, value, 5);
    if (v.p.room.energy_plus) energy_line(lv_event_get_layer(e), &bounds, "+1", 22);
}

static void build_room(void)
{
    lv_obj_t *L = v.layer;
    lv_obj_t *bubble = ui_panel(L, UI_PANEL_X, UI_BUBBLE_Y, UI_PANEL_W, UI_BUBBLE_H, UI_PANEL_CARD | UI_PANEL_TAIL);
    ui_icon(bubble, F_ICON, UI_ICON_STAR_M, UI_MID, 7, 6);
    v.p.room.speech = text(bubble, F_SPEECH, UI_INK, 22, 8, 194, LV_TEXT_ALIGN_LEFT);
    v.p.room.speech_small = text(bubble, F_BODY, UI_INK, 22, 9, 194, LV_TEXT_ALIGN_LEFT);

    ui_panel(L, UI_PANEL_X, UI_ROOM_Y, UI_PANEL_W, UI_ROOM_PANEL_H, UI_PANEL_FRAME);
    lv_obj_t *img = lv_image_create(L);
    lv_obj_remove_style_all(img);
    lv_obj_set_pos(img, UI_PANEL_X + 2, UI_ROOM_Y + 2);
    lv_obj_set_size(img, UI_ROOM_W, UI_ROOM_H);
    lv_obj_t *sprite = lv_image_create(img);  // child: clipped to the room
    lv_obj_remove_style_all(sprite);
    lv_obj_set_pos(sprite, UI_RUG_ROOM_X - UI_SPRITE_ANCHOR_X, UI_RUG_ROOM_Y - UI_SPRITE_ANCHOR_Y);
    // Size follows each cropped frame; its crop offset is added to the position.
    v.p.room.room_img = img;
    v.p.room.sprite = sprite;
    v.p.room.energy = ui_container(img, 0, 0, UI_ROOM_W, 42);
    lv_obj_add_event_cb(v.p.room.energy, energy_draw, LV_EVENT_DRAW_MAIN, NULL);
    ui_set_hidden(v.p.room.energy, true);
    // Reference bubble at native 240x320 scale. Existing scene/sprite anchors stay intact.
    v.p.room.voice_bubble = ui_panel(img, 139, 14, 24, 16, UI_PANEL_CARD | UI_PANEL_TAIL | UI_PANEL_FLAT);
    v.p.room.voice_mark = text(v.p.room.voice_bubble, F_BODY, UI_INK, 2, 3, 20, LV_TEXT_ALIGN_CENTER);
    ui_set_hidden(v.p.room.voice_bubble, true);

    lv_obj_t *info = ui_panel(L, UI_PANEL_X, UI_INFO_Y, UI_PANEL_W, UI_INFO_H, UI_PANEL_CARD | UI_PANEL_CAPSULE);
    v.p.room.info = info;
    ui_icon(info, F_ICON, UI_ICON_STAR_XL, UI_GOLD, 10, 15);
    v.p.room.right = ui_icon(info, F_ICON, UI_ICON_CLOCK_N, UI_RIVET, 192, 15);
    v.p.room.digits = text(info, F_DIGITS, UI_INK, 30, 10, 160, LV_TEXT_ALIGN_CENTER);
    v.p.room.title = text(info, F_TITLE, UI_INK, 30, 8, 160, LV_TEXT_ALIGN_CENTER);
    v.p.room.sub = text(info, F_BODY, UI_MID, 37, 29, 146, LV_TEXT_ALIGN_CENTER);  // inside the capsule
    v.p.room.arrow_l = ui_icon(info, F_ICON, UI_ICON_TRI_L, UI_MID, 50, 8);
    v.p.room.arrow_r = ui_icon(info, F_ICON, UI_ICON_TRI_R, UI_MID, 170, 8);

    for (int i = 0; i < UI_NAV_VISIBLE; i++) {
        nav_slot_t *s = &v.p.room.nav[i];
        s->panel = ui_panel(L, UI_PANEL_X + i * (UI_NAV_W + UI_NAV_GAP), UI_NAV_Y, UI_NAV_W, UI_NAV_H, UI_PANEL_SOFT);
        s->icon = ui_icon(s->panel, F_ICON, UI_ICON_CLOCK_N, UI_INK, 5, (UI_NAV_H - ICON_LINE) / 2);
        s->label = text(s->panel, F_BODY, UI_INK, 25, 11, 42, LV_TEXT_ALIGN_LEFT);
    }
}

static void sync_nav(const app_model_t *m)
{
    static const char *const glyphs[NAV_COUNT] = {
        [NAV_FOCUS]=UI_ICON_CLOCK_N,[NAV_CALENDAR]=UI_ICON_CALENDAR_N,[NAV_CHAT]=UI_ICON_CHAT_N,
        [NAV_PLAY]=UI_ICON_HEART_N,[NAV_BATTLE]=UI_ICON_BATTLE,[NAV_SETTINGS]=UI_ICON_GEAR_N
    };
    // Three of the five items are visible; the window follows the selection.
    // The model keeps the window: it only scrolls when the selection steps
    // past the first or last visible button.
    unsigned first = m->nav_first;
    if (first > NAV_COUNT - UI_NAV_VISIBLE) first = NAV_COUNT - UI_NAV_VISIBLE;
    for (unsigned i = 0; i < UI_NAV_VISIBLE; i++) {
        nav_slot_t *s = &v.p.room.nav[i];
        unsigned item = first + i;
        bool on = item == (unsigned)m->nav;
        ui_panel_set_kind(s->panel, on ? UI_PANEL_SELECTED : UI_PANEL_SOFT);
        lv_obj_set_style_text_font(s->icon, F_ICON, 0);
        ui_set_text(s->icon, glyphs[item]);
        ui_set_color(s->icon, on ? UI_GOLD : UI_INK);
        ui_set_text(s->label, ui_copy_nav_name(item));
        ui_set_color(s->label, on ? UI_PAPER : UI_INK);
    }
}

static const lv_image_dsc_t *scene_image(unsigned scene, room_phase_t phase);

static void sync_room(const app_model_t *m, const pet_view_status_t *st, uint32_t now)
{
    static const int16_t rug[SCENE_COUNT][2] = {{UI_RUG_ROOM_X, UI_RUG_ROOM_Y}, {UI_RUG_TRAIN_X, UI_RUG_TRAIN_Y},
                                             {UI_RUG_ROOM_X, UI_RUG_ROOM_Y}};
    unsigned scene = m->prefs.scene < SCENE_COUNT ? m->prefs.scene : SCENE_ROOM;
    const lv_image_dsc_t *room = scene == SCENE_TRAILBLAZE ? asset_trailblaze(m->prefs.trailblaze_index)
                                                        : scene_image(scene, app_room_phase(m));
    if (!room) room = asset_room(SCENE_ROOM, app_room_phase(m));
    if (room != v.p.room.room_src) {
        lv_image_set_src(v.p.room.room_img, room);
        v.p.room.room_src = room;
    }
    const companion_model_t *c = &m->companion;
    uint32_t t = c->motion == COMPANION_MOTION_IDLE ? now : companion_model_elapsed(c, now);
    stelle_state_t state = app_companion_asleep(m) ? STELLE_SLEEP : (stelle_state_t)c->motion;
    if (app_chat_in_focus(m)) {
        state = STELLE_IDLE;
        if (m->chat.phase == CHAT_RECORDING || m->chat.phase == CHAT_PLAYING) t = 0;
        else if (m->chat.phase == CHAT_WAITING) {
            // Existing idle frame 02 is the closed-eye pose; hold its exact start.
            const stelle_pack_state_t *idle = &stelle_pack_states[STELLE_IDLE];
            t = idle->durations[0] + idle->durations[1];
        }
        else t = now;
    }
    stelle_frame_t frame = stelle_frame_at(state, t);
    bool battle = m->page == PAGE_BATTLE;
    if (battle) {
        const battle_frame_t *b = &battle_frames[m->battle_stage == BATTLE_RECOVER ? 0 : m->battle_stage];
        frame = (stelle_frame_t){&b->image, b->ox, b->oy};
        if(m->battle_enemy==1 && (m->battle_stage==BATTLE_ENCOUNTER || m->battle_stage==BATTLE_HIT))
            frame=(stelle_frame_t){enemy_b_image(m->battle_stage==BATTLE_HIT ? 1U : 0U),0,0};
    }
    bool greeting=app_model_greeting_visible(m,now);
    if (greeting) frame=stelle_frame_at(STELLE_GREETING,now-m->display_greeting_ms);
    // Stand on the middle of this scene's rug: the frames' ground-shadow centre
    // goes there; a cropped frame is shifted by where it sat in the full cell.
    ui_set_x(v.p.room.sprite, (battle && !greeting ? 0 : rug[scene][0] - UI_SPRITE_ANCHOR_X) + frame.ox);
    ui_set_y(v.p.room.sprite, (battle && !greeting ? 0 : rug[scene][1] - UI_SPRITE_ANCHOR_Y) + frame.oy);
    ui_set_hidden(v.p.room.sprite, frame.image == NULL);
    if (frame.image != v.p.room.frame) {
        lv_image_set_src(v.p.room.sprite, frame.image);
        v.p.room.frame = frame.image;
    }
    bool plus = battle && m->battle_has_hit && m->path_energy>0 &&
        (m->battle_stage==BATTLE_HIT || m->battle_stage==BATTLE_RECOVER) &&
        now-m->battle_started_ms<500U;
    if (v.p.room.energy_count != m->path_energy || v.p.room.energy_plus != plus) {
        v.p.room.energy_count = m->path_energy;
        v.p.room.energy_plus = plus;
        lv_obj_invalidate(v.p.room.energy);
    }
    ui_set_hidden(v.p.room.energy, !battle || m->battle_stage == BATTLE_READY);
    bool voice = m->page == PAGE_CHAT && (m->chat.phase == CHAT_RECORDING || m->chat.phase == CHAT_PLAYING);
    ui_set_hidden(v.p.room.voice_bubble, !voice);
    if (voice) ui_set_text(v.p.room.voice_mark, m->chat.phase == CHAT_RECORDING ? "?" : "...");

    // 14 px when the line fits (allowing 1 px tighter spacing), else 12 px.
    const char *say = asset_pack_ready() ? ui_copy_speech(m) : "Art missing: flash the asset pack";
    bool small = ui_text_width(say, F_SPEECH) - (int)strlen(say) > 194;
    ui_set_hidden(v.p.room.speech, small);
    ui_set_hidden(v.p.room.speech_small, !small);
    ui_set_text(small ? v.p.room.speech_small : v.p.room.speech, say);

    ui_info_t info;
    ui_copy_info(m, &info);
    if (app_chat_in_focus(m) && m->chat.phase == CHAT_ERROR && st->chat && st->chat->hint[0])
        snprintf(info.sub, sizeof(info.sub), "%s", st->chat->hint);
    if (app_chat_in_focus(m) && m->chat.phase == CHAT_BINDING && st->chat && st->chat->code[0]) {
        snprintf(info.big, sizeof(info.big), "%.11s", st->chat->code);
        snprintf(info.sub, sizeof(info.sub), "Bind at xiaozhi.me");
    }
    bool has_sub = info.sub[0] != '\0';
    const lv_font_t *font = info.big_is_digits ? F_DIGITS : F_TITLE;
    lv_obj_t *big = info.big_is_digits ? v.p.room.digits : v.p.room.title;
    char caps[sizeof(info.big)];
    upper(caps, sizeof(caps), info.big);
    ui_set_hidden(v.p.room.digits, !info.big_is_digits);
    ui_set_hidden(v.p.room.title, info.big_is_digits);
    ui_set_text(big, info.big_is_digits ? info.big : caps);
    int y = has_sub ? (info.big_is_digits ? 4 : 8) : (UI_INFO_H - 1 - font->line_height) / 2;
    ui_set_y(big, y + font_dy(font));
    ui_set_text(v.p.room.sub, info.sub);
    ui_set_hidden(v.p.room.sub, !has_sub);
    ui_panel_set_flags(v.p.room.info, UI_PANEL_CAPSULE, !has_sub);  // the outline would cut a hint line
    ui_set_text(v.p.room.right, info.sparkles ? UI_ICON_STAR_XL : UI_ICON_CLOCK_N);
    ui_set_color(v.p.room.right, info.sparkles ? UI_GOLD : UI_RIVET);
    ui_set_hidden(v.p.room.arrow_l, !info.arrows);
    ui_set_hidden(v.p.room.arrow_r, !info.arrows);
    if (info.arrows) {
        int w = ui_text_width(big == v.p.room.title ? caps : info.big, font), center = 110;
        int ay = y + (font->line_height - ICON_LINE) / 2;
        ui_set_x(v.p.room.arrow_l, center - w / 2 - 13);
        ui_set_x(v.p.room.arrow_r, center + (w + 1) / 2 + 9);
        ui_set_y(v.p.room.arrow_l, ay);
        ui_set_y(v.p.room.arrow_r, ay);
    }
    sync_nav(m);
    bool battle_nav = battle || (m->page == PAGE_HOME && m->nav == NAV_BATTLE);
    // Share Play's typography and vertical placement. Battle's longer single
    // instruction line needs the full width below the title and side stars.
    ui_set_x(v.p.room.sub, battle_nav ? 6 : 37);
    ui_set_w(v.p.room.sub, battle_nav ? 208 : 146);
    ui_set_text(v.p.room.sub, info.sub);
}

/* ---------- full pages ---------- */

static void title_row(const char *glyph, const char *title)
{
    lv_obj_t *chip = ui_panel(v.layer, UI_PANEL_X, UI_CONTENT_Y, 24, UI_TITLE_H, UI_PANEL_CARD | UI_PANEL_FLAT);
    ui_icon(chip, F_ICON, glyph, UI_INK, 4, 3);
    lv_obj_t *t = text(v.layer, F_TITLE, UI_INK, 42, UI_CONTENT_Y + 7,
                       UI_PANEL_X + UI_PANEL_W - 42, LV_TEXT_ALIGN_LEFT);
    ui_set_text(t, title);
}

static lv_obj_t *line(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int y, const char *s)
{
    lv_obj_t *l = text(parent, font, color, 6, y, UI_PANEL_W - 12, LV_TEXT_ALIGN_CENTER);
    if (s) ui_set_text(l, s);
    return l;
}

static lv_obj_t *card(int y, int h) { return ui_panel(v.layer, UI_PANEL_X, y, UI_PANEL_W, h, UI_PANEL_CARD); }

/* ---------- calendar ---------- */

#define CAL_X 5       // grid origin inside the card
#define CAL_Y 20       // first row: weekday names, then up to 6 weeks
#define CAL_CELL_W 30
#define CAL_CELL_H 16

static const app_model_t *s_cal_model;  // read by the grid's draw callback

static int32_t month_first_day(int32_t day)
{
    int y, mo, d;
    date_from_day(day, &y, &mo, &d);
    return day - (d - 1);
}

// One object draws the whole month: 7 x 6 cells, the selected day as a dark
// cell with gold corners, today outlined, and a gold bar under each day with
// focus (longer bar = more focus). No per-day objects, so the pool stays small.
static void grid_draw(lv_event_t *e)
{
    const app_model_t *m = s_cal_model;
    if (!m || !m->date_known) return;
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t first = month_first_day(m->cal_day);
    int y, mo, d;
    date_from_day(first, &y, &mo, &d);
    int days = date_days_in_month(y, mo), lead = date_weekday_mon0(first);
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    ld.font = F_BODY;
    ld.align = LV_TEXT_ALIGN_CENTER;
    static const char *const names[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    ld.color = lv_color_hex(UI_MID);
    for (int c = 0; c < 7; c++) {
        lv_area_t ha = {a.x1 + c * CAL_CELL_W, a.y1, a.x1 + (c + 1) * CAL_CELL_W - 1, a.y1 + CAL_CELL_H - 1};
        ld.text = names[c];
        lv_draw_label(l, &ld, &ha);
    }
    char num[12];
    for (int i = 0; i < days; i++) {
        int cell = lead + i, cx = a.x1 + (cell % 7) * CAL_CELL_W, cy = a.y1 + (cell / 7 + 1) * CAL_CELL_H;
        int32_t day = first + i;
        bool sel = day == m->cal_day, today = day == m->day, future = day > m->day;
        if (sel) {
            ui_fill(l, cx + 1, cy, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 1, UI_SELECT);
            ui_fill(l, cx + 1, cy, cx + 4, cy, UI_GOLD);
            ui_fill(l, cx + 1, cy, cx + 1, cy + 3, UI_GOLD);
            ui_fill(l, cx + CAL_CELL_W - 5, cy + CAL_CELL_H - 1, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 1, UI_GOLD);
            ui_fill(l, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 4, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 1, UI_GOLD);
        } else if (today) {
            ui_fill(l, cx + 1, cy, cx + CAL_CELL_W - 2, cy, UI_INK);
            ui_fill(l, cx + 1, cy + CAL_CELL_H - 1, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 1, UI_INK);
            ui_fill(l, cx + 1, cy, cx + 1, cy + CAL_CELL_H - 1, UI_INK);
            ui_fill(l, cx + CAL_CELL_W - 2, cy, cx + CAL_CELL_W - 2, cy + CAL_CELL_H - 1, UI_INK);
        }
        uint32_t secs = future ? 0 : app_focus_on_day(m, day).secs;
        if (secs) {  // any focus that day: one fixed-length gold bar
            const int len = 10;
            int bx = cx + (CAL_CELL_W - len) / 2;
            ui_fill(l, bx, cy + CAL_CELL_H - 3, bx + len - 1, cy + CAL_CELL_H - 2, UI_GOLD);
        }
        snprintf(num, sizeof(num), "%d", i + 1);
        ld.text = num;
        ld.text_local = 1;
        ld.color = lv_color_hex(sel ? UI_PAPER : future ? UI_RIVET : UI_INK);
        lv_area_t ta = {cx, cy, cx + CAL_CELL_W - 1, cy + CAL_CELL_H - 4};
        lv_draw_label(l, &ld, &ta);
    }
}

static void build_calendar(void)
{
    title_row(UI_ICON_CALENDAR_N, "CALENDAR");
    // Month card: title, weekday row + up to 6 weeks, then the key hint at the
    // bottom under a thin rule.
    lv_obj_t *c = card(66, 156);
    v.p.cal.month = line(c, F_MED, UI_INK, 5, NULL);
    v.p.cal.grid = ui_container(c, CAL_X, CAL_Y, 7 * CAL_CELL_W, 7 * CAL_CELL_H);
    lv_obj_add_event_cb(v.p.cal.grid, grid_draw, LV_EVENT_DRAW_MAIN, NULL);
    v.p.cal.empty = line(c, F_BODY, UI_MID, 60, "Connect Wi-Fi to get the date");
    ui_box(c, 10, 134, UI_PANEL_W - 20, 1, UI_RIVET);
    line(c, F_BODY, UI_MID, 138, "UP/DOWN: day / Hold UP: today");
    // One line, two voices: the date small and grey on the left, the focused
    // time bold on the right.
    lv_obj_t *d = card(228, 30);
    ui_icon(d, F_ICON, UI_ICON_STAR_S, UI_GOLD, 10, 5);
    v.p.cal.date = text(d, F_BODY, UI_MID, 23, 9, 80, LV_TEXT_ALIGN_LEFT);
    v.p.cal.dur = text(d, F_TITLE, UI_INK, 112, 8, 96, LV_TEXT_ALIGN_RIGHT);
    v.p.cal.none = text(d, F_BODY, UI_RIVET, 112, 9, 96, LV_TEXT_ALIGN_RIGHT);
    // Entry to the anniversary page (OK).
    lv_obj_t *en = ui_panel(v.layer, UI_PANEL_X, 268, UI_PANEL_W, 30, UI_PANEL_SELECTED);
    ui_icon(en, F_ICON, UI_ICON_STAR_L, UI_GOLD, 10, 7);
    lv_obj_t *lbl = text(en, F_BODY, UI_PAPER, 28, 9, 100, LV_TEXT_ALIGN_LEFT);
    ui_set_text(lbl, "Trailblazing");
    v.p.cal.entry_days = text(en, F_BODY, UI_GOLD, 96, 9, 100, LV_TEXT_ALIGN_RIGHT);
    ui_icon(en, F_ICON, UI_ICON_TRI_R, UI_GOLD, 202, 6);
    v.p.cal.sel = INT32_MIN;
}

static void sync_calendar(const app_model_t *m)
{
    char s[40];
    s_cal_model = m;
    bool known = m->date_known;
    ui_set_hidden(v.p.cal.grid, !known);
    ui_set_hidden(v.p.cal.empty, known);
    if (known != v.p.cal.known || m->cal_day != v.p.cal.sel || m->day != v.p.cal.today) {
        lv_obj_invalidate(v.p.cal.grid);  // redraw the month only when something moved
        v.p.cal.known = known;
        v.p.cal.sel = m->cal_day;
        v.p.cal.today = m->day;
    }
    if (!known) {
        ui_set_text(v.p.cal.month, "Calendar");
        ui_set_text(v.p.cal.date, "NO DATE YET");
        ui_set_hidden(v.p.cal.dur, true);
        ui_set_hidden(v.p.cal.none, false);
        ui_set_text(v.p.cal.none, "Waiting for sync");
        ui_set_text(v.p.cal.entry_days, "");
        return;
    }
    ui_copy_month(m->cal_day, s, sizeof(s));
    ui_set_text(v.p.cal.month, s);
    char caps[16];
    ui_copy_date(m->cal_day, s, sizeof(s));
    upper(caps, sizeof(caps), s);
    ui_set_text(v.p.cal.date, caps);
    focus_day_t fd = app_focus_on_day(m, m->cal_day);
    ui_set_hidden(v.p.cal.dur, fd.secs == 0);
    ui_set_hidden(v.p.cal.none, fd.secs != 0);
    if (fd.secs) {
        ui_copy_duration(fd.secs, s, sizeof(s));
        ui_set_text(v.p.cal.dur, s);
    } else {
        ui_set_text(v.p.cal.none, "No focus");
    }
    char days[16], clock[12];
    ui_copy_since_launch(app_since_hsr_launch_s(m), days, sizeof(days), clock, sizeof(clock));
    snprintf(s, sizeof(s), "Day %s", days);
    ui_set_text(v.p.cal.entry_days, s);
}

// Pixel starfield for the dark anniversary card, drawn after the panel
// itself: single-pixel stars, small plus stars, two gold sparkles in the
// corners and a thin rule with a gold diamond between the number and the date.
static void night_draw(lv_event_t *e)
{
    lv_layer_t *l = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    int x = a.x1, y = a.y1;
    static const uint8_t dots[][2] = {{9, 31}, {24, 52}, {12, 74}, {44, 86}, {66, 30}, {150, 29},
                                      {206, 34}, {196, 56}, {210, 78}, {172, 86}, {40, 30}, {186, 44}};
    for (size_t i = 0; i < sizeof(dots) / sizeof(dots[0]); i++)
        ui_fill(l, x + dots[i][0], y + dots[i][1], x + dots[i][0], y + dots[i][1], i % 3 ? UI_MID : UI_RIVET);
    static const uint8_t pluses[][2] = {{14, 64}, {198, 64}, {30, 82}};
    for (size_t i = 0; i < sizeof(pluses) / sizeof(pluses[0]); i++) {
        int px = x + pluses[i][0], py = y + pluses[i][1];
        ui_fill(l, px - 1, py, px + 1, py, UI_RIVET);
        ui_fill(l, px, py - 1, px, py + 1, UI_RIVET);
    }
    static const uint8_t sparkles[][2] = {{20, 44}, {204, 86}};
    for (size_t i = 0; i < 2; i++) {
        int sx = x + sparkles[i][0], sy = y + sparkles[i][1];
        ui_fill(l, sx - 3, sy, sx + 3, sy, UI_GOLD);
        ui_fill(l, sx, sy - 3, sx, sy + 3, UI_GOLD);
        ui_fill(l, sx - 1, sy - 1, sx + 1, sy + 1, UI_GOLD);
    }
    int cx = x + UI_PANEL_W / 2, ry = y + 65;
    ui_fill(l, cx - 60, ry, cx - 5, ry, UI_SELECT);
    ui_fill(l, cx + 5, ry, cx + 60, ry, UI_SELECT);
    ui_fill(l, cx - 2, ry, cx + 2, ry, UI_GOLD);
    ui_fill(l, cx - 1, ry - 1, cx + 1, ry + 1, UI_GOLD);
    ui_fill(l, cx, ry - 2, cx, ry + 2, UI_GOLD);
}

static void build_anniv(void)
{
    title_row(UI_ICON_STAR_L, "TRAILBLAZING");
    ui_panel(v.layer, UI_PANEL_X, 66, UI_PANEL_W, 140, UI_PANEL_FRAME);
    lv_obj_t *img = lv_image_create(v.layer);
    lv_obj_remove_style_all(img);
    lv_obj_set_pos(img, UI_PANEL_X + 2, 68);
    const lv_image_dsc_t *art = anniv_image_frame(0);
    if (art) lv_image_set_src(img, art);
    v.p.anniv.img = img;
    // Three lines on a night-sky card: what is counted, "<number> days" in
    // gold, and the start date. Stars and a rule are drawn by night_draw.
    lv_obj_t *c = ui_panel(v.layer, UI_PANEL_X, 212, UI_PANEL_W, 96, UI_PANEL_NIGHT);
    lv_obj_add_event_cb(c, night_draw, LV_EVENT_DRAW_MAIN, NULL);
    line(c, F_BODY, UI_RIVET, 10, "Honkai: Star Rail has launched");
    v.p.anniv.days = text(c, F_DIGITS, UI_GOLD, 0, 34, 120, LV_TEXT_ALIGN_LEFT);
    v.p.anniv.unit = text(c, F_MED, UI_PAPER, 0, 44, 50, LV_TEXT_ALIGN_LEFT);
    ui_set_text(v.p.anniv.unit, "days");
    line(c, F_BODY, UI_RIVET, 75, "since 2023-04-26");
}

static void sync_anniv(const app_model_t *m, uint32_t now)
{
    const lv_image_dsc_t *art=anniv_image_frame(anniv_art_frame_at(now-m->anniv_entered_ms));
    if (art && lv_image_get_src(v.p.anniv.img)!=art) lv_image_set_src(v.p.anniv.img,art);
    char days[16], clock[12];
    uint64_t secs = app_since_hsr_launch_s(m);
    if (!secs) {  // date unknown until the Wi-Fi time sync
        ui_set_text(v.p.anniv.days, "--");
        int wd = ui_text_width("--", F_DIGITS), wu = ui_text_width("days", F_MED);
        ui_set_x(v.p.anniv.days, (UI_PANEL_W - (wd + 6 + wu)) / 2);
        ui_set_x(v.p.anniv.unit, (UI_PANEL_W - (wd + 6 + wu)) / 2 + wd + 6);
        return;
    }
    ui_copy_since_launch(secs, days, sizeof(days), clock, sizeof(clock));
    ui_set_text(v.p.anniv.days, days);
    // Centre "<number> days" as one group: the unit follows the digits.
    int wd = ui_text_width(days, F_DIGITS), wu = ui_text_width("days", F_MED), gap = 6;
    int x0 = (UI_PANEL_W - (wd + gap + wu)) / 2;
    ui_set_x(v.p.anniv.days, x0);
    ui_set_x(v.p.anniv.unit, x0 + wd + gap);
}
static void build_settings(void)
{
    static const char *const glyphs[SET_COUNT] = {UI_ICON_SUN, UI_ICON_SPEAKER, UI_ICON_MOON, UI_ICON_WIFI,
                                                  UI_ICON_HOME};
    title_row(UI_ICON_GEAR, "SETTINGS");
    // Two columns of tiles; the last (Scene) spans the full width. Icon on the
    // left, name and current value on the right.
    for (int i = 0; i < SET_COUNT; i++) {
        tile_t *t = &v.p.set.tile[i];
        bool wide = i == SET_COUNT - 1 && SET_COUNT % 2;
        int w = wide ? UI_PANEL_W : 107;
        t->panel = ui_panel(v.layer, UI_PANEL_X + (i % 2) * 113, 70 + (i / 2) * 60, w, 54, UI_PANEL_SOFT);
        t->icon = ui_icon(t->panel, F_ICON2, glyphs[i], UI_INK, 7, 10);
        t->label = text(t->panel, F_BODY, UI_INK, 39, 12, 66, LV_TEXT_ALIGN_LEFT);
        ui_set_text(t->label, ui_copy_setting_name((unsigned)i));
        t->value = text(t->panel, F_BODY, UI_MID, 39, 30, 66, LV_TEXT_ALIGN_LEFT);
        if (wide) {
            lv_obj_t *hint = text(t->panel, F_BODY, UI_MID, 110, 14, 100, LV_TEXT_ALIGN_RIGHT);
            ui_set_text(hint, "Room / Train\nTrailblaze");
            v.p.set.hint = hint;
        }
    }
    lv_obj_t *help = text(v.layer, F_BODY, UI_MID, UI_PANEL_X + 4, 264,
                          UI_PANEL_W - 8, LV_TEXT_ALIGN_CENTER);
    ui_set_text(help, "UP / DOWN: select / OK: open");
    help = text(v.layer, F_BODY, UI_MID, UI_PANEL_X + 4, 280,
                UI_PANEL_W - 8, LV_TEXT_ALIGN_CENTER);
    ui_set_text(help, "Hold OK: back");
}

static void build_adjust(const app_model_t *m)
{
    static const char *const glyphs[3] = {UI_ICON_SUN, UI_ICON_SPEAKER, UI_ICON_MOON};
    unsigned s = m->setting_sel < 3 ? m->setting_sel : 0;
    lv_obj_t *top = card(UI_CONTENT_Y, 172);
    line(top, F_MED, UI_INK, 26, s == SET_SCREEN_OFF ? "Auto screen off" : ui_copy_setting_name(s));
    ui_icon(top, F_ICON3, glyphs[s], UI_INK, (UI_PANEL_W - ui_text_width(glyphs[s], F_ICON3)) / 2, 40);
    v.p.adj.slider = ui_slider(top, 18, 110, 184, 18);
    v.p.adj.min = text(top, F_BODY, UI_MID, 18, 135, 80, LV_TEXT_ALIGN_LEFT);
    v.p.adj.max = text(top, F_BODY, UI_MID, 122, 135, 80, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *bottom = card(220, 88);
    v.p.adj.value = text(bottom, F_TITLE, UI_INK, 6, 8, UI_PANEL_W - 12, LV_TEXT_ALIGN_CENTER);
    line(bottom, F_BODY, UI_INK, 30, s == SET_SCREEN_OFF ? "UP / DOWN: change" : "UP / DOWN: 10% steps");
    line(bottom, F_BODY, UI_INK, 48, "OK: save / Hold OK: cancel");
    v.p.adj.note = line(bottom, F_BODY, UI_MID, 66, NULL);
}

// Wi-Fi setup: a scannable hotspot QR code (dark on white with a 4-module
// quiet zone), the hotspot name/password for manual joining, and live status.
static const lv_image_dsc_t *scene_image(unsigned scene, room_phase_t phase)
{
    if (scene == SCENE_TRAILBLAZE) return asset_trailblaze_preview();
    return asset_room(scene < SCENE_COUNT ? scene : SCENE_ROOM, phase);  // NULL until the pack is bound
}

// Background picker: a full-size preview of the highlighted scene (at the
// current time of day), Room/Train above the full-width daily random option.
static void build_scene(void)
{
    title_row(UI_ICON_SCENE, "SCENE");
    ui_panel(v.layer, UI_PANEL_X, 68, UI_PANEL_W, UI_ROOM_PANEL_H, UI_PANEL_FRAME);
    lv_obj_t *img = lv_image_create(v.layer);
    lv_obj_remove_style_all(img);
    lv_obj_set_pos(img, UI_PANEL_X + 2, 70);
    lv_obj_set_size(img, UI_ROOM_W, UI_ROOM_H);
    v.p.scene.img = img;
    for (int i = 0; i < SCENE_COUNT; i++) {
        bool daily = i == SCENE_TRAILBLAZE;
        lv_obj_t *o = ui_panel(v.layer, daily ? UI_PANEL_X : UI_PANEL_X + i * 113,
                               daily ? 264 : 224, daily ? UI_PANEL_W : 107, daily ? 44 : 34, UI_PANEL_SOFT);
        v.p.scene.opt[i] = o;
        v.p.scene.opt_label[i] = text(o, F_MED, UI_INK, 2, daily ? 8 : 10,
                                     daily ? UI_PANEL_W - 4 : 103, LV_TEXT_ALIGN_CENTER);
        if (daily) v.p.scene.daily = text(o, F_BODY, UI_MID, 2, 25, UI_PANEL_W - 4, LV_TEXT_ALIGN_CENTER);
    }
    ui_set_text(v.p.scene.daily, "Visit a random Star Rail spot daily.");
}

static void sync_scene(const app_model_t *m)
{
    unsigned draft = m->draft < SCENE_COUNT ? m->draft : SCENE_ROOM;
    static const char *const names[SCENE_COUNT] = {"Room", "Train", "Trailblaze"};
    const lv_image_dsc_t *src = scene_image(draft, app_room_phase(m));
    if (lv_image_get_src(v.p.scene.img) != src) lv_image_set_src(v.p.scene.img, src);
    for (unsigned i = 0; i < SCENE_COUNT; i++) {
        bool on = i == draft;
        ui_set_text(v.p.scene.opt_label[i], names[i]);
        ui_panel_set_kind(v.p.scene.opt[i], on ? UI_PANEL_SELECTED : UI_PANEL_SOFT);
        ui_set_color(v.p.scene.opt_label[i], on ? UI_PAPER : UI_INK);
    }
    ui_set_color(v.p.scene.daily, draft == SCENE_TRAILBLAZE ? UI_PAPER : UI_MID);
}

static void build_setup(const app_model_t *m)
{
    lv_obj_t *c = card(UI_CONTENT_Y, 200);
    ui_box(c, (UI_PANEL_W - 148) / 2, 6, 148, 148, 0xFFFFFF);
    lv_obj_t *qr = lv_qrcode_create(c);
    lv_qrcode_set_size(qr, 116);
    lv_qrcode_set_dark_color(qr, lv_color_hex(UI_INK));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(qr, (UI_PANEL_W - 116) / 2, 22);
    v.p.setup.qr = qr;
    v.p.setup.qr_text[0] = '\0';
    v.p.setup.wait = line(c, F_MED, UI_MID, 74, "Starting hotspot...");
    line(c, F_BOLD, UI_INK, 158, "Scan with your phone camera");
    v.p.setup.ap = line(c, F_BODY, UI_INK, 172, NULL);
    line(c, F_BODY, UI_MID, 185, "No page? Open 192.168.4.1");
    lv_obj_t *f = card(246, 62);
    v.p.setup.status = line(f, F_MED, UI_INK, 10, NULL);
    line(f, F_BODY, UI_MID, 34, m->setup_from_settings ? "Hold OK: back to Settings" : "Hold OK: skip for now");
}

static void sync_setup(const app_model_t *m, const pet_view_status_t *st)
{
    static const net_status_t none = {0};
    const net_status_t *n = st->net ? st->net : &none;
    bool ready = n->ap_qr[0] != '\0' && n->phase != NET_PHASE_STARTING && n->phase != NET_PHASE_IDLE;
    ui_set_hidden(v.p.setup.qr, !ready);
    ui_set_hidden(v.p.setup.wait, ready);
    if (ready && strcmp(v.p.setup.qr_text, n->ap_qr) != 0) {  // re-encode only when it changes
        snprintf(v.p.setup.qr_text, sizeof(v.p.setup.qr_text), "%s", n->ap_qr);
        lv_qrcode_update(v.p.setup.qr, n->ap_qr, (uint32_t)strlen(n->ap_qr));
    }
    char ap[sizeof(n->ap_ssid) + sizeof(n->ap_pass) + 24], s[UI_COPY_MAX];
    if (ready) snprintf(ap, sizeof(ap), "%s / pass %s", n->ap_ssid, n->ap_pass);
    else ap[0] = '\0';
    ui_set_text(v.p.setup.ap, ap);
    ui_copy_setup_status(n, s, sizeof(s));
    ui_set_text(v.p.setup.status, s);
    (void)m;
}
static void build_history(void)
{
    title_row(UI_ICON_CALENDAR_N, "Chat history");
    // ui_panel adds a 2 px shadow; the complete frame ends at the screen edge.
    lv_obj_t *card = ui_panel(v.layer, UI_PANEL_X, 72, UI_PANEL_W, UI_H-72-2, UI_PANEL_CARD);
    v.p.history.clip = ui_container(card, 12, 8, 192, CHAT_HISTORY_VIEW_PX);
    lv_obj_add_flag(v.p.history.clip, LV_OBJ_FLAG_USER_2); // intentional clipped scroll viewport
    for (unsigned i=0;i<HISTORY_SLOTS;i++) {
        v.p.history.role[i] = ui_text(v.p.history.clip,F_BODY,UI_MID,0,0,192,LV_TEXT_ALIGN_LEFT);
        v.p.history.body[i] = ui_text(v.p.history.clip,&stelle_chat_font,UI_INK,0,18,192,LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_text_line_space(v.p.history.body[i],2,0);
    }
    // Keep half a text line clear above the fixed footer. The footer belongs
    // to the card, not the clipped scroll viewport.
    lv_obj_t *hint = text(card,F_BODY,UI_MID,2,
        8+(int)CHAT_HISTORY_VIEW_PX+(int)CHAT_HISTORY_LINE_PX/2-font_dy(F_BODY),216,LV_TEXT_ALIGN_CENTER);
    ui_set_text(hint, "OK: latest / Hold: back");
}

static void sync_history(const app_model_t *m)
{
    unsigned bottom=chat_history_max_scroll(&m->chat_history);
    unsigned offset=m->chat_history_scroll>bottom ? bottom : m->chat_history_scroll;
    if (v.p.history.synced && v.p.history.revision==m->chat_history.revision && v.p.history.scroll==offset) return;
    v.p.history.synced=true; v.p.history.revision=m->chat_history.revision; v.p.history.scroll=(uint16_t)offset;
    static char wrapped[CHAT_WRAP_BYTES]; // LVGL lock protects this shared scratch buffer
    unsigned used=0, top=0;
    for (unsigned i=0;i<m->chat_history.count && used<HISTORY_SLOTS;i++) {
        const chat_entry_t *entry=chat_history_get(&m->chat_history,i);
        unsigned height=chat_history_entry_height(entry);
        int y=(int)top-(int)offset;
        top+=height;
        if (y+(int)height<=0 || y>=(int)CHAT_HISTORY_VIEW_PX) continue;
        bool user=!entry->assistant;
        lv_text_align_t align=user ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT;
        lv_obj_t *role=v.p.history.role[used], *body=v.p.history.body[used];
        lv_obj_set_style_text_align(role,align,0);
        lv_obj_set_style_text_align(body,align,0);
        ui_set_color(role,user ? UI_HEADER : UI_CHAT_SPEAKER);
        ui_set_color(body,user ? UI_MID : UI_INK);
        ui_set_y(role,y); ui_set_y(body,y+(int)CHAT_HISTORY_ROLE_PX);
        ui_set_text(role,user ? "You" : "Stelle");
        chat_history_wrap(entry->text,CHAT_HISTORY_COLUMNS,wrapped,sizeof(wrapped));
        if (strcmp(lv_label_get_text(body),wrapped)) lv_label_set_text(body,wrapped);
        ui_set_hidden(role,false); ui_set_hidden(body,false);
        used++;
    }
    if (!m->chat_history.count) {
        ui_set_y(v.p.history.role[0],0); ui_set_y(v.p.history.body[0],18);
        ui_set_text(v.p.history.role[0],"Device history");
        lv_label_set_text(v.p.history.body[0],"No messages this boot.");
        ui_set_hidden(v.p.history.role[0],false); ui_set_hidden(v.p.history.body[0],false);
        used=1;
    }
    for (unsigned i=used;i<HISTORY_SLOTS;i++) {
        ui_set_hidden(v.p.history.role[i],true); ui_set_hidden(v.p.history.body[i],true);
    }
}

static void build_page(const app_model_t *m)
{
    if (v.layer) lv_obj_delete(v.layer);
    memset(&v.p, 0, sizeof(v.p));
    v.layer = ui_container(v.screen, 0, 0, UI_W, UI_H);
    lv_obj_move_to_index(v.layer, 0);  // below the header
    switch (m->page) {
    case PAGE_HOME:
    case PAGE_FOCUS:
    case PAGE_PLAY:
    case PAGE_CHAT:
    case PAGE_BATTLE: build_room(); break;
    case PAGE_CHAT_HISTORY: build_history(); break;
    case PAGE_CALENDAR: build_calendar(); break;
    case PAGE_ANNIV: build_anniv(); break;
    case PAGE_SETTINGS: build_settings(); break;
    case PAGE_ADJUST: build_adjust(m); break;
    case PAGE_SETUP: build_setup(m); break;
    case PAGE_SCENE: build_scene(); break;
    }
    v.page = m->page;
    v.setting = m->setting_sel;
    v.setup_from = m->setup_from_settings;
    v.built = true;
}

static void sync_settings(const app_model_t *m, const pet_view_status_t *st)
{
    for (unsigned i = 0; i < SET_COUNT; i++) {
        tile_t *t = &v.p.set.tile[i];
        bool on = i == m->setting_sel;
        ui_panel_set_kind(t->panel, on ? UI_PANEL_SELECTED : UI_PANEL_SOFT);
        ui_set_color(t->icon, on ? UI_GOLD : UI_INK);
        ui_set_color(t->label, on ? UI_PAPER : UI_INK);
        ui_set_color(t->value, on ? UI_PANEL : UI_MID);
        char s[16];
        if (i == SET_WIFI) {
            ui_copy_wifi_value(st->net, s, sizeof(s));
        } else if (i == SET_SCENE) {
            ui_copy_setting_value(i, m->prefs.scene, s, sizeof(s));
            ui_set_color(v.p.set.hint, on ? UI_PANEL : UI_MID);
        } else {
            unsigned value = i == SET_BRIGHTNESS ? m->prefs.brightness : i == SET_VOLUME ? m->prefs.volume : m->prefs.screen_off;
            ui_copy_setting_value(i, value, s, sizeof(s));
        }
        ui_set_text(t->value, s);
    }
}

static void sync_adjust(const app_model_t *m, const pet_view_status_t *st)
{
    char s[16], caps[16];
    unsigned set = m->setting_sel;
    ui_copy_setting_value(set, m->draft, s, sizeof(s));
    upper(caps, sizeof(caps), s);
    ui_set_text(v.p.adj.value, caps);
    if (set == SET_SCREEN_OFF) {
        ui_slider_set(v.p.adj.slider, m->draft + 1u, app_screen_off_count());
        ui_set_text(v.p.adj.min, "Never");
        ui_set_text(v.p.adj.max, "5 min");
        ui_set_text(v.p.adj.note, "Any key wakes the screen");
    } else {
        ui_slider_set(v.p.adj.slider, m->draft / 10u, 10);
        ui_set_text(v.p.adj.min, set == SET_BRIGHTNESS ? "10%" : "0%");
        ui_set_text(v.p.adj.max, "100%");
        ui_set_text(v.p.adj.note, set == SET_VOLUME && !st->audio_ok ? "Speaker unavailable" : "");
    }
}

void pet_view_update(const app_model_t *m, const pet_view_status_t *st, uint32_t now)
{
    if (!v.screen) return;
    char t[8];
    if (st->battery < 0) snprintf(t, sizeof(t), "--%%");
    else snprintf(t, sizeof(t), "%d%%", st->battery > 100 ? 100 : st->battery);
    ui_set_text(v.battery_text, t);
    ui_battery_set(v.battery, st->battery, st->charging);

    bool room = app_page_shows_room(m->page);
    // Home, Focus and Play share one room layer; any other change rebuilds.
    bool stale = room ? !app_page_shows_room(v.page)
                      : v.page != m->page || (m->page == PAGE_ADJUST && v.setting != m->setting_sel) ||
                            (m->page == PAGE_SETUP && v.setup_from != m->setup_from_settings);
    if (!v.built || stale) build_page(m);
    v.page = m->page;
    switch (m->page) {
    case PAGE_HOME:
    case PAGE_FOCUS:
    case PAGE_PLAY:
    case PAGE_CHAT:
    case PAGE_BATTLE: sync_room(m, st, now); break;
    case PAGE_CHAT_HISTORY: sync_history(m); break;
    case PAGE_CALENDAR: sync_calendar(m); break;
    case PAGE_ANNIV: sync_anniv(m, now); break;
    case PAGE_SETTINGS: sync_settings(m, st); break;
    case PAGE_ADJUST: sync_adjust(m, st); break;
    case PAGE_SETUP: sync_setup(m, st); break;
    case PAGE_SCENE: sync_scene(m); break;
    }
}
