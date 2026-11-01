//
//  zorkzero.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-30.
//

#ifndef zorkzero_hpp
#define zorkzero_hpp

extern "C" {
#include "glk.h"
}

#include "spatterlight-autosave.h"

#include <cstdint>

struct Window;

enum BorderType {
    CASTLE_BORDER = 5,
    OUTSIDE_BORDER = 6,
    UNDERGROUND_BORDER = 7,
    HINT_BORDER = 8
};

constexpr int TEXT_WINDOW_PIC_LOC = 387;

struct ZorkGlobals {
    uint8_t HERE;
    uint8_t NEW_COMPASS;
    uint8_t MOVES;
    uint8_t SCORE;
    uint8_t CURRENT_BORDER;
    uint8_t BORDER_ON;
    uint8_t CURRENT_SPLIT;
    uint8_t F_WIN_COUNT;
    uint8_t F_PLAYS;
    uint8_t YOUR_SCORE;
    uint8_t J_SCORE;
    uint8_t DEFAULT_FG;
    uint8_t DEFAULT_BG;
    uint8_t BLINK_TBL;
    uint8_t TOWER_CHANGED;
    uint8_t NARROW;
    uint8_t PEG_MOVE_NUMBER;
};

struct ZorkRoutines {
    uint32_t V_REFRESH; 
    uint32_t SETUP_SCREEN;
    uint32_t SET_BORDER;
    uint32_t DRAW_NEW_COMP;
    uint32_t DRAW_COMPASS_ROSE;
    uint32_t MAP_X;
    uint32_t MAP_Y;
    // Local-variable numbers that hold the "you are here" CX/CY coordinates
    // in the V-MAP-LOOP frame our hook fires in. These differ by revision
    // (the releases use 4/5; r242 inlines the loop into V-MAP and uses
    // 5/6), so they're recorded when the entrypoint is located.
    // 0 means "not set" -> V_MAP_LOOP falls back to the release layout.
    uint8_t MAP_CX_VAR;
    uint8_t MAP_CY_VAR;
    // The early revisions have no map-aware refresh, so to redraw the map on
    // resize/autorestore we re-invoke the game's own V-MAP routine with its
    // BLINK-WHILE-AWAITING-INPUT call patched to rtrue (so it draws the map
    // and returns instead of looping/exiting). V_MAP is the routine's byte
    // address; V_MAP_BLINK_CALL is the byte address of that call to patch. 0 =
    // not found.
    uint32_t V_MAP;
    uint32_t V_MAP_BLINK_CALL;
    uint32_t J_PLAY;
    uint32_t SCORE_CHECK;
    uint32_t DRAW_PEGS;
    // The mouse-click handlers of Peggleboz, Snarfem and Tower of Bozbar.
    // 0 = this revision has no mouse support in that game (r242), so the
    // game's clickable boxes are not drawn.
    uint32_t PBOZ_CLICK;
    uint32_t SN_CLICK;
    uint32_t B_MOUSE_PEG_PICK;
    uint32_t SET_B_PIC;
    uint32_t TOWER_WIN_CHECK;
    // Address of the branch in SNARFEM taken when the player wins. 0 = not
    // found.
    uint32_t SNARFEM_WIN;
    // Address of the code in PEG-GAME that follows a legal move and calls
    // PBOZ-WIN-CHECK. 0 = not found.
    uint32_t PEG_GAME_WIN;
    uint32_t PLAY_SELECTED;
};

struct ZorkTables {
    uint16_t K_HINT_ITEMS;
    uint16_t F_PLAY_TABLE;
    uint16_t F_PLAY_TABLE_APPLE;
    uint16_t LEFT_PEG_TABLE;
    uint16_t CENTER_PEG_TABLE;
    uint16_t RIGHT_PEG_TABLE;
    uint16_t PILE_TABLE;
    uint16_t PBOZ_PIC_TABLE;
    uint16_t BOARD_TABLE;
    // r242 and r296 only: the Peggleboz pegs as a table of words (1 = peg
    // present), indexed 1..21. Later revisions use attributes 1..21 of
    // NOT-HERE-OBJECT instead (set = peg gone).
    uint16_t PEG_TABLE;
    uint16_t B_X_TBL;
    uint16_t B_Y_TBL;
    uint16_t F_CARD_TABLE;
    // r296 only: DRAW-CARDS-TABLE, five words flagging the card slots that
    // need redrawing (later revisions use attributes on NOT-HERE-OBJECT).
    // Non-zero also means that F_CARD_TABLE is a table of words, not bytes.
    uint16_t DRAW_CARDS_TABLE;
    uint16_t PICINF_TBL;
    uint16_t SL_LOC_TBL;
};

struct ZorkObjects {
    uint16_t PHIL_HALL;
    uint16_t G_U_MOUNTAIN;
    uint16_t G_U_SAVANNAH;
    uint16_t G_U_HIGHWAY;
    uint16_t NOT_HERE_OBJECT;
    uint16_t LEFT_PEG;
    uint16_t RIGHT_PEG;
    uint16_t CENTER_PEG;
    uint16_t ONE_WEIGHT;
    uint16_t TWO_WEIGHT;
    uint16_t THREE_WEIGHT;
    uint16_t FOUR_WEIGHT;
    uint16_t FIVE_WEIGHT;
    uint16_t SIX_WEIGHT;
    uint16_t PBOZ_OBJECT;
    uint16_t PYRAMID_L;
    uint16_t PYRAMID;
    uint16_t PYRAMID_R;
    uint16_t BROOM;
    uint16_t ROOMS; // 0 if the game keeps no compass flags on it
};

struct ZorkProperties {
    uint8_t P_REGION;
    uint8_t P_APPLE_DESC;
    uint8_t TOUCHBIT;
    uint8_t TRYTAKEBIT;
    uint8_t P_MAP_LOC;
};

// The rows of the entrypoint table: entered when the game reaches the routine
void after_SPLIT_BY_PICTURE(void);
void CENTER_2(void);
void CENTER_3(void);
void V_MODE(void);
void DEFAULT_COLORS(void);
void INIT_STATUS_LINE(void);
void SMALL_DOOR_F(void);
void J_PLAY(void);
void TOWER_MODE(void);
void z0_UPDATE_STATUS_LINE(void);
void DRAW_TOWER(void);
void B_MOUSE_PEG_PICK(void);
void B_MOUSE_WEIGHT_PICK(void);
void SETUP_PBOZ(void);
void PBOZ_CLICK(void);
void PEG_GAME(void);
void PBOZ_WIN_CHECK(void);
void DISPLAY_MOVES(void);
void SNARFEM(void);
void SETUP_SN(void);
void DRAW_SN_BOXES(void);
void DRAW_PILE(void);
void DRAW_FLOWERS(void);
void SN_CLICK(void);
void FANUCCI(void);
void SETUP_FANUCCI(void);
void SLIDE_SHOW_END(void);
void V_MAP_LOOP(void);

// The V6GameHooks of this game (v6_shared.cpp)
void z0_update_on_resize(void);
void z0_update_after_restore(void);
bool z0_display_picture(int x, int y, Window *win);
void z0_stash_state(library_state_data *dat);
void z0_recover_state(library_state_data *dat);

// Called from other files
void CENTER_1(void);
void z0_erase_window(int16_t index);
bool z0_border_on(void);
void z0_update_after_autorestore(void);
void z0_update_colors(void);
void DISPLAY_BORDER(BorderType border);
void z0_leave_stale_map_for_line_input(void);
void z0_erase_screen(void);
void z0_display_hint_border(void);

// State other files read
extern winid_t z0_right_status_window;
extern ZorkGlobals zg;
extern ZorkRoutines zr;
extern ZorkTables zt;
extern ZorkObjects zo;
extern ZorkProperties zp;

#endif /* zorkzero_hpp */

