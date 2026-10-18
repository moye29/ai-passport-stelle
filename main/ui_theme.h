// main/ui_theme.h - the single design system shared by every page.
//
// Cool blue-violet greys, near-black and off-white, with gold accents.
// Geometry targets a 240 x 320 panel; the application configures BSP rounding.
#pragma once

#define UI_INK      0x262833  // dark top bar, outlines, primary text
#define UI_SELECT   0x424453  // selected control fill (sampled from the reference)
#define UI_MID      0x7E7D91  // neutral grey: hints, bubble star
#define UI_PANEL    0xE0DEE8  // light grey list cards, tiles, nav
#define UI_PAPER    0xEAE8F2  // off-white cards and bubble
#define UI_GOLD     0xF5C15D  // the one accent: marks, stars, progress
#define UI_CHAT_SPEAKER UI_GOLD  // speaker-label highlight
#define UI_PLATE    0xDDDBE6  // face plate between panels
#define UI_RIM      0x9091A2  // grey rim between the dark bezel and the plate
#define UI_HEADER   0x282B35  // top bar fill
#define UI_CHIP     0x3B3D4A  // star chip inside the top bar
#define UI_EDGE     0x3B3D4C  // top bar rim, a lighter step of UI_HEADER
#define UI_SHADOW   0x9E9CB5  // 2 px panel drop shadow (bottom-right)
#define UI_BATTERY  0xC7BCDC  // pale lavender battery fill, as in the reference
#define UI_RIVET    0xB4B5C9  // corner dots and the clock capsule line

#define UI_BG_PANEL  UI_PANEL
#define UI_BG_PAPER  UI_PAPER
#define UI_BG_PLATE  UI_PLATE
#define UI_BG_RIM    UI_RIM
#define UI_BG_SHADOW UI_SHADOW
#define UI_BG_RIVET  UI_RIVET

#define UI_W 240
#define UI_H 320
#define UI_BEZEL 6            // dark bezel strip, then a 2 px rim
#define UI_RIM_W 2

#define UI_PANEL_X 10
#define UI_PANEL_W 220

#define UI_HEADER_X 8
#define UI_HEADER_Y 9
#define UI_HEADER_W 224
#define UI_HEADER_H 26

#define UI_BUBBLE_Y 39
#define UI_BUBBLE_H 28

#define UI_ROOM_Y 72
#define UI_ROOM_PANEL_H 150   // room image is UI_ROOM_W x UI_ROOM_H inside a 2 px frame

#define UI_INFO_Y 225
#define UI_INFO_H 47

#define UI_NAV_Y 276
#define UI_NAV_H 32
#define UI_NAV_W 70
#define UI_NAV_GAP 5
#define UI_NAV_VISIBLE 3

#define UI_CONTENT_Y 40       // first row below the header on full pages
#define UI_TITLE_H 24
#define UI_TODO_ROW_Y 70
#define UI_TODO_ROW_H 30
#define UI_TODO_ROW_STEP 34
#define UI_TODO_VISIBLE 5

// Sprite cell (generated/companion). The centre of the ground shadow drawn in
// every frame (UI_SPRITE_ANCHOR_*) is placed on the current scene's rug centre
// (UI_RUG_*_X/Y in ui_assets.h), so Stelle stands in the middle of the rug.
#define UI_SPRITE_W 160
#define UI_SPRITE_H 160
#define UI_SPRITE_ANCHOR_X 80
#define UI_SPRITE_ANCHOR_Y 149
