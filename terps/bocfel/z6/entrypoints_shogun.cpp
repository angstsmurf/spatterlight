//
//  entrypoints_shogun.cpp
//  bocfel6
//
// Shogun's rows of the entrypoint table and its globals finder. The table
// format and the search helpers are in entrypoints_internal.hpp, the scan
// that fills in the table in entrypoints.cpp.

#include "screen.h"
#include "dict.h"
#include "v6_shared.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp" // V_REFRESH, which Shogun hooks too

#include "entrypoints.hpp"
#include "entrypoints_internal.hpp"

// Hook functions that only name a row (see entrypoints_internal.hpp)
static void V_BOW(void) {}
static void MAZE_F(void) {}
static void DESCRIBE_ROOM(void) {}
static void DESCRIBE_OBJECTS(void) {}

std::vector<EntryPoint> shogun_entrypoints = {
    {
        Game::Shogun,
        "SCENE-SELECT",
        { 0xa0, 0x01, 0xca, 0xcd, 0x4f, 0x05 },
        0,
        0,
        false,
        SCENE_SELECT
    },

    {
        Game::Shogun,
        "SCENE-SELECT alt",
        { 0xbe, 0x13, 0x5f, 0x07, 0x02 },
        0,
        0,
        false,
        SCENE_SELECT
    },

    {
        Game::Shogun,
        "V-DEFINE",
        { 0xed, 0x3f, 0xff, 0xff, 0x36 },
        0,
        0,
        true,
        V_DEFINE
    },
    {
        Game::Shogun,
        "GET-FROM-MENU",
        { 0xff, 0x7f, 0x04, 0xc5, 0x0d, 0x04, 0x01 },
        0,
        0,
        false,
        GET_FROM_MENU
    },

    {
        Game::Shogun,
        "after GET-FROM-MENU",
        {},
        0,
        0,
        false,
        after_GET_FROM_MENU
    },

    {
        Game::Shogun,
        "SETUP-TEXT-AND-STATUS",
        { 0xff, 0x7f, 0x01, 0xc5 , 0x0d, 0x01, 0x02, 0x0f },
        0,
        0,
        true,
        SETUP_TEXT_AND_STATUS
    },

    {
        Game::Shogun,
        "UPDATE-STATUS-LINE",
        { 0xa0, 0x01, 0x4a, 0x0f, 0x00, 0x08, 0x00, 0x47, 0x00, 0x04, 0x47 },
        0,
        0,
        true,
        shogun_UPDATE_STATUS_LINE
    },

    {
        Game::Shogun,
        "UPDATE-STATUS-LINE alt",
        { 0x0f, 0x00, 0x08, 0x00, 0x47 },
        0,
        0,
        true,
        shogun_UPDATE_STATUS_LINE
    },

    {
        Game::Shogun,
        "INTERLUDE-STATUS-LINE alt",
        { 0xda, WILDCARD, WILDCARD, WILDCARD, 0x04, 0x0d, WILDCARD, 0x00, 0xeb, 0x7f, 0x01, 0x7b },
        0,
        0,
        true,
        INTERLUDE_STATUS_LINE
    },

    {
        Game::Shogun,
        "INTERLUDE-STATUS-LINE",
        { 0x01, 0x00, 0xb8, 0x02, 0xc1, 0x95, WILDCARD, 0x02, 0x09, 0x0a },
        4,
        0,
        true,
        INTERLUDE_STATUS_LINE
    },

    {
        Game::Shogun,
        "DISPLAY-BORDER alt 2",
        { 0x2d, 0x01, WILDCARD, 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40, 0xeb, 0x7f, 0x07, 0xbe, 0x05, 0x97, 0x01, 0x01, 0x01 },
        -4,
        0,
        true,
        shogun_DISPLAY_BORDER
    },

    {
        Game::Shogun,
        "DISPLAY-BORDER",
        { 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, WILDCARD, 0xeb, 0x7f, 0x07, 0xbe, 0x05, 0x97, 0x01, 0x01, 0x01 },
        0,
        0,
        true,
        shogun_DISPLAY_BORDER
    },

    {
        Game::Shogun,
        "DISPLAY-BORDER alt",
        { 0xb8, 0x04, 0xff, 0x7f, 0x01, 0xc5, 0x2d, 0x01 },
        2,
        0,
        true,
        shogun_DISPLAY_BORDER
    },

    // DISPLAY-BORDER, CENTER-PIC-X, CENTER-PIC and MARGINAL-PIC all open with
    // the same PICINF L01,TABLE ?~RFALSE (be 06 8f 01 .. .. 40), so the
    // CENTER-PIC-X rows include the locals count that precedes a routine
    // entry: 3 in the r278 s890209 beta, 4 in every later revision. Without
    // it the pattern hits the body of DISPLAY-BORDER first in r279-r292,
    // where the "alt 2" row has already claimed that routine.
    {
        Game::Shogun,
        "CENTER-PIC-X older",
        { 0x03, 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40 },
        1,
        0,
        true,
        CENTER_PIC_X
    },

    {
        Game::Shogun,
        "CENTER-PIC-X",
        { 0x04, 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40 },
        1,
        0,
        true,
        CENTER_PIC_X
    },

    {
        Game::Shogun,
        "CENTER-PIC",
        { 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40, 0xbe },
        0,
        0,
        true,
        CENTER_PIC
    },

    {
        Game::Shogun,
        "MARGINAL-PIC",
        { 0xff, 0x7f, 0x02, 0xc5, 0x0d, 0x02, 0x01, 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40 },
        0,
        0,
        true,
        MARGINAL_PIC
    },

    {
        Game::Shogun,
        "DESCRIBE-ROOM",
        { 0x02, 0xab, 0x02, 0x04, 0xa0, WILDCARD, 0x51, 0xb2 },
        4,
        0,
        false,
        DESCRIBE_ROOM
    },

    {
        Game::Shogun,
        "DESCRIBE-OBJECTS",
        { 0xbb, 0xb0, 0x04, 0xa3, WILDCARD, 0x03, 0xa2, WILDCARD, 0x01, 0xc2 },
        3,
        0,
        false,
        DESCRIBE_OBJECTS
    },

    {
        Game::Shogun,
        "V-VERSION",
        { 0x0d, 0x02, 0x12, 0xa0, 0x01 },
        0,
        0,
        false,
        V_VERSION
    },
    {
        Game::Shogun,
        "after V-VERSION",
        { 0xbf, 0x00, 0xa0, 0x01, 0xc5, 0x8f },
        9,
        0,
        false,
        after_V_VERSION
    },

    {
        Game::Shogun,
        "V-CREDITS",
        { 0xf1, 0x7f, 0x02, 0xda, 0x0f },
        0,
        0,
        false,
        V_CREDITS
    },

    {
        Game::Shogun,
        "after V-CREDITS",
        { },
        0,
        0,
        false,
        after_V_CREDITS
    },

    {
        Game::Shogun,
        "V-BOW",
        { 0x88, 0x2b, WILDCARD, 0x00, 0xa0, 0x00, 0xd5 },
        0,
        0,
        false,
        V_BOW
    },

    // Shared with Arthur
    {
        Game::Shogun,
        "V-COLOR",
        { 0x41, WILDCARD, 0x03, 0x00, WILDCARD, 0x88, WILDCARD, WILDCARD, 0x00 },
        0,
        0,
        false,
        V_COLOR
    },

    // Shared with Arthur
    {
        Game::Shogun,
        "after V-COLOR",
        { },
        0,
        0,
        false,
        after_V_COLOR
    },

    {
        Game::Shogun,
        "MAC-II?",
        { 0xbe, 0x13, 0x5f, 0x07, 0x03, 0x00 },
        0,
        0,
        false,
        MAC_II
    },

    {
        Game::Shogun,
        "after MAC-II?",
        { },
        0,
        0,
        false,
        after_MAC_II
    },

    {
        Game::Shogun,
        "V-REFRESH",
        { 0x0d, WILDCARD, 0x00, 0x8f, WILDCARD, WILDCARD, 0x41, 0x1a, WILDCARD, 0x45 },
        0,
        0,
        false,
        V_REFRESH
    },

    {
        Game::Shogun,
        "V-REFRESH alt",
        { 0x0d, WILDCARD, 0x00, 0x41, WILDCARD, 0x02, 0xc7 },
        0,
        0,
        false,
        V_REFRESH
    },

    {
        Game::Shogun,
        "MAZE-F",
        { 0x06, 0x41, 0x01, 0x05, 0x00 },
        1,
        0,
        false,
        MAZE_F
    },

    {
        Game::Shogun,
        "MAZE-F alt",
        { 0x07, 0x41, 0x01, 0x05, 0x00 },
        1,
        0,
        false,
        MAZE_F
    },

    {
        Game::Shogun,
        "MAZE-MOUSE-F",
        { 0xcf, 0x1f, WILDCARD, WILDCARD, 0x01, 0x05, 0xcf, 0x1f },
        0,
        0,
        false,
        MAZE_MOUSE_F
    },

    {
        Game::Shogun,
        "DISPLAY-MAZE-PIC",
        { 0xa0, 0x01, 0xc0, 0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x04 },
        0,
        0,
        true,
        DISPLAY_MAZE_PIC
    },

    {
        Game::Shogun,
        "DISPLAY-MAZE-PIC alt",
        { 0x41, 0x01, 0x23, 0xc0, 0xcf, 0x1f, 0x4f, WILDCARD, 0x00, 0x04 },
        0,
        0,
        true,
        DISPLAY_MAZE_PIC
    },

    {
        Game::Shogun,
        "DISPLAY-MAZE",
        { 0x8f, WILDCARD, WILDCARD, 0xed, 0x7f, 0x00, 0xbe },
        0,
        0,
        true,
        DISPLAY_MAZE
    },

    {
        Game::Shogun,
        "after BUILDMAZE",
        { 0xe0, 0x29, WILDCARD, WILDCARD, WILDCARD, WILDCARD, 0x00, 0x00, 0xb8 },
        8,
        0,
        false,
        after_BUILDMAZE
    },

    {
        Game::Shogun,
        "DO-HINTS",
        { 0xf1, 0x7f, 0x00, 0xef, 0x1f, 0xff, 0xff, 0x00 },
        0,
        0,
        true,
        DO_HINTS
    },

    {
        Game::Shogun,
        "DO-HINTS alt",
        { 0xf1, 0x7f, 0x00, 0x88, WILDCARD, WILDCARD, 0x02, 0xbe, 0x12, 0x57, 0x00, 0x01, 0x02 },
        0,
        0,
        true,
        DO_HINTS
    },

    {
        Game::Shogun,
        "DISPLAY-HINT",
        { 0xf1, 0x7f, 0x00, 0xed, 0x7f, 0x00, 0xeb, 0x7f, 0x01, 0xf9 },
        0,
        0,
        false,
        DISPLAY_HINT
    },
};

// UPDATE-STATUS-LINE: the globals the status line is drawn from.
static void shogun_find_status_line_globals(EntryPoint &entrypoint, int &start) {

    uint8_t dummy;
    start = find_globals_in_pattern({ 0x47, 0x00, 0x04, 0xc6, 0x61, WILDCARD, WILDCARD, 0xc5, 0x0d, 0x01, 0x01, 0xa0, 0x01, 0x4a, 0x61, WILDCARD, WILDCARD, 0x46, 0x61, WILDCARD, WILDCARD, 0xc0 }, { &sg.HERE, &dummy, &sg.SCORE, &dummy, &sg.MOVES, &dummy }, entrypoint.found_at_address, 100);

    start = find_pattern_in_mem({ 0xef, WILDCARD, 0x01, WILDCARD, 0xcf, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0x00 }, start, 200);

    if (start != -1) {
        sg.SCENE = global_index(memory[start + 8]);
        st.SCENE_NAMES = (memory[start + 6] << 8) | memory[start + 7];

        start = find_globals_in_pattern({ 0xc1, 0x97, WILDCARD, 0x01, 0x03, 0xe1 }, { &sg.MACHINE }, start, 300);

        start = find_globals_in_pattern({ 0xaa, 0x1a, 0xa3, WILDCARD, 0x03, 0x4a, 0x03, 0x2c, 0x5b }, { &sg.WINNER }, start, 300);

        if (start != -1) {

            start = find_pattern_in_mem({ 0xda, 0x2f, WILDCARD, WILDCARD, 0x03, 0xc1, 0x97, 0x1a }, start, 200);
            if (start != -1)
                sr.TELL_THE = unpack_routine(word(start + 2));

            start = find_values_in_pattern({0x41, 0x1a, WILDCARD, 0x48 }, { &so.BRIDGE_OF_ERASMUS }, start, 200);

            start = find_16_bit_values_in_pattern({ 0xca, 0x1f, WILDCARD, WILDCARD, 0x13, 0xcc }, { &so.WHEEL }, start, 200);

            start = find_16_bit_values_in_pattern({ 0xca, 0x1f, WILDCARD, WILDCARD, 0x13, 0x5c }, { &so.GALLEY_WHEEL }, start, 200);

            if (start != -1)
                so.GALLEY = memory[start - 4];

            start = find_pattern_in_mem({ 0xda, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0xa0, 0x01, 0x46, 0x61 }, start, 200);
            // A mid-chain miss leaves start == -1; guard the dereferences
            // so memory[start - 8] doesn't read at a negative index.
            if (start != -1) {
                sr.TELL_DIRECTION = unpack_routine(word(start + 2));
                sg.SHIP_COURSE = global_index(memory[start + 4]);
                sg.SHIP_DIRECTION = global_index(memory[start - 8]);
            }
        } else {
            fprintf(stderr, "Error! Could not find sr.TELL_THE!\n");
        }
    } else {
        fprintf(stderr, "Error! Could not find sg.SCENE!\n");
    }
}

// MAZE-MOUSE-F: the globals and tables of the maze.
static void shogun_find_maze_globals(EntryPoint &entrypoint, int &start) {

    // RET L00
    patch_ret_local(entrypoint.found_at_address, 1);

    start = find_globals_in_pattern({ 0x76, WILDCARD, 0x06, 0x00 }, { &sg.MAZE_Y }, entrypoint.found_at_address, 300);
    if (start != -1) {
        start = find_globals_in_pattern({ 0x76, WILDCARD, 0x05, 0x00 }, { &sg.MAZE_X }, start + 1, 300);

        start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x9f }, { &so.EAST }, start + 1, 300);
        start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x8f }, { &so.NORTH }, start + 1, 300);

        start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x67 }, { &so.SOUTH }, start + 1, 300);
        start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x43 }, { &so.WEST }, start + 1, 300);

        uint16_t fnc = 0;
        start = find_16_bit_values_in_pattern({ 0xf9, 0x27, WILDCARD, WILDCARD, 0x02, 0x0d }, { &fnc }, start + 1, 300);
        if (start != -1) {
            sr.ADD_TO_INPUT = unpack_routine(fnc);

            start = find_globals_in_pattern({ 0x70, WILDCARD, 0x00, 0x00 }, { &sg.MAZE_MAP }, start, 300);

            if (start != -1) {
            } else {
                fprintf(stderr, "Could not find sg.MAZE_MAP!\n");
            }
        }
    } else {
        fprintf(stderr, "Could not find sg.MAZE_MAP!\n");
    }
}

// Second-pass scanner for Shogun. Locates maze globals, scene-select
// table addresses, border globals, and patches V-BOW versions 283-295
// to use direct (rather than indirect) variable assignment.
void find_shogun_globals(void) {
    int start = 0;
    int mac_ii_return_address = 0;
    int credits_return_address = 0;
    int menu_return_address = 0;

    bool found_scene_select_values = false;
    bool found_border_values = false;

    // Early versions (up to r288) will set text colours
    // to black on white in their main routine.
    // We change them to default here.
    start = unpack_routine(header.pc); // We already know the address of the main routine
    start = find_globals_in_pattern({ 0x0d, WILDCARD, 0x02, 0x0d, WILDCARD, 0x09 }, { &fg_global_idx, &bg_global_idx }, start, 30);

    if (start != -1) {
        store_byte(start + 1, 1);
        store_byte(start - 2, 1);
    }

    for (auto &entrypoint : shogun_entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            start = find_v_color_globals(entrypoint.found_at_address);
            entrypoint.found_at_address = 0; // V_COLOR
        } else if (entrypoint.fn == after_V_COLOR && end_of_color_addr != 0) {
            entrypoint.found_at_address = end_of_color_addr;
        } else if (entrypoint.fn == MAC_II && entrypoint.found_at_address != 0) {
            start = entrypoint.found_at_address;
            for (int i = start; i < memory_size - kScanTailMargin; i++) {
                if (memory[i] == kOpRfalse) {
                    mac_ii_return_address = i - 6;
                    break;
                }
            }
        } else if (entrypoint.fn == after_MAC_II && mac_ii_return_address != 0) {
            entrypoint.found_at_address = mac_ii_return_address;
        } else if (entrypoint.fn == GET_FROM_MENU && entrypoint.found_at_address != 0) {

            // We rip out most of the original code
            // and replace it with our own C(++) function
            // (GET_FROM_MENU(), called by find_entrypoints())
            // But we want call to the function associated with
            // the menu to be done in Z-code, so that we can autosave
            // in it properly.

            //  e0 ab 03 09 02 07       CALL_VS         L02 (L08,L01) -> L06 ; LABEL
            //  a0 07 bf f7             JZ              L06 [TRUE] LABEL
            //  ab 07                   RET             L06

            std::vector<uint8_t> patch = {0xe0, 0xab, 0x03, 0x09, 0x02, 0x07,
                0xa0, 0x07, 0xbf, 0xf8,
                0xab, 0x07};
            start = entrypoint.found_at_address;
            patch_bytes(start, patch);
            menu_return_address =  start + patch.size() - 2;

        } else if (entrypoint.fn == after_GET_FROM_MENU && menu_return_address != 0) {
            entrypoint.found_at_address = menu_return_address;
        } else if ((entrypoint.fn == V_VERSION || entrypoint.fn == V_CREDITS) && entrypoint.found_at_address != 0) {

            if (entrypoint.fn == V_CREDITS) {
                start = find_pattern_in_mem({kOpRtrue}, entrypoint.found_at_address, 100);
                if (start != -1)
                    credits_return_address = start;
            }

            // Shogun's V-CREDITS prints the credits inline, bracketed by
            // <HLIGHT ,H-BOLD> ... <HLIGHT ,H-NORMAL> (@set_text_style 2 /
            // @set_text_style 0). Route that text through the centered
            // styles by adding the fixed-width bit (bold+fixed ->
            // style_User1, bold+italic+fixed -> style_Note). Zork Zero's
            // V-CREDITS has no inline styling -- it calls CENTER-1/2/3,
            // which are reimplemented directly (see CENTER_1/2/3, which
            // print centered and then do_return out of the original) --
            // so this pattern isn't present there and the patch is skipped.
            if (!is_spatterlight_zork0) {
                start = find_pattern_in_mem({ 0xf1, 0x7f, 0x02 }, entrypoint.found_at_address, 100);
                if (start != -1) {
                    store_byte(start + 2, 0x0a);
                    start = find_pattern_in_mem({ 0xf1, 0x7f, 0x00 }, start, 100);
                    if (start != -1) {
                        store_byte(start + 2, 0x0e);
                    } else {
                        fprintf(stderr, "V-CREDITS: could not find the closing @set_text_style to patch!\n");
                    }
                } else {
                    fprintf(stderr, "V-CREDITS: could not find the opening @set_text_style to patch!\n");
                }
            }

        } else if (entrypoint.fn == after_V_CREDITS && credits_return_address != 0) {
            entrypoint.found_at_address = credits_return_address;
        } else if (entrypoint.fn == DO_HINTS && entrypoint.found_at_address != 0) {
            start = find_hints_table(entrypoint.found_at_address);
        } else if (entrypoint.fn == DISPLAY_HINT && entrypoint.found_at_address != 0) {
            start = find_display_hint_globals(entrypoint.found_at_address);
            entrypoint.found_at_address = 0; // DISPLAY_HINT
        } else if (entrypoint.fn == V_DEFINE && entrypoint.found_at_address != 0) {
            start = find_function_key_tables(entrypoint.found_at_address);
        } else if (entrypoint.fn == shogun_UPDATE_STATUS_LINE && entrypoint.found_at_address != 0) {
            shogun_find_status_line_globals(entrypoint, start);
        } else if (entrypoint.fn == SCENE_SELECT && entrypoint.found_at_address != 0  && !found_scene_select_values) {

            if (memory[entrypoint.found_at_address - 10] == 0xcf &&
                memory[entrypoint.found_at_address - 9] == 0x1f) {
                entrypoint.found_at_address -= 10;
            }

            start = find_16_bit_values_in_pattern({ 0x8c, 0x00, 0x07, 0xcd, 0x4f, 0x05, WILDCARD, WILDCARD }, { &sm.PART_MENU }, entrypoint.found_at_address, 20);

            bool is_old_version = (start != -1);

            start = find_pattern_in_mem({ 0xed, 0x7f, 0x00, 0xeb, 0x7f, 0x00, 0xec }, entrypoint.found_at_address, 300);
            if (start != -1) {
                int address_of_call = start + 6;
                start += 12;
                sm.YOU_MAY_CHOOSE = memory[start];
                if (!is_old_version) {
                    sm.PART_MENU = word(start + 1);
                } else {
                    start--;
                }
                sm.SCENE_SELECT_F = word(start + 3);
                found_scene_select_values = true;

                start = find_globals_in_pattern({ 0x55, 0x01, 0x01, 0x00, 0x76, 0x00, WILDCARD, 0x00, 0x54, 0x00, 0x01, 0x00, 0xb8 }, { &sg.FONT_X }, start, 2000);
                if (start != -1) {
                    start = find_globals_in_pattern({ 0x55, 0x01, 0x01, 0x00, 0x76, 0x00, WILDCARD, 0x00, 0x54, 0x00, 0x01, 0x00, 0xb8 }, { &sg.FONT_Y }, start, 100);
                    if (start != -1) {
                    } else {
                        fprintf(stderr, "Error! Could not find FONT_X or !\n");
                    }
                }

                // We rip out most of the original code
                // and replace it with our own C(++) function
                // (SCENE_SELECT(), called by find_entrypoints())
                // But we want the actual call to GET-FROM-MENU
                // be done in Z-code, so that we can autosave
                // properly.

                // The below will take over when our own
                // SCENE_SELECT() C(++) function returns.

//               <SET WHICH
//               <GET-FROM-MENU "You may choose to: "
//               ,PART-MENU
//               ,SCENE-SELECT-F
//               1>>

//              ec 08 7f 16 06 00 56 05 13 33 01 06
//              (LABEL)           CALL_VS2        GET-FROM-MENU (S012,L04,#1333,#01) -> L05
//              a0 06 bf ab       JZ              L05 [TRUE] LABEL
//              b0                RTRUE

                std::vector<uint8_t> patch = {0xec, 0x08, 0x7f, 0x16, 0x06, 0x00, 0x56, 0x05, 0x13, 0x33, 0x01, 0x06,
                    0xa0, 0x06, 0xbf, 0xfe,
                    0xb0};
                patch[3] = memory[address_of_call + 3]; // Replace with address
                patch[4] = memory[address_of_call + 4]; // of function GET-FROM-MENU

                patch[6] = memory[address_of_call + 6]; // Replace with message string address

                patch[8] = sm.SCENE_SELECT_F >> 8;      // Replace with address
                patch[9] = sm.SCENE_SELECT_F & 0xff;    // of function SCENE-SELECT-F

                start = entrypoint.found_at_address;
                patch_bytes(start, patch);
            } else {
                fprintf(stderr, "Error! Could not find sm.YOU_MAY_CHOOSE!\n");
            }
        } else if (entrypoint.fn == V_REFRESH && entrypoint.found_at_address != 0) {
            sr.V_REFRESH = entrypoint.found_at_address - 1;
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == DESCRIBE_ROOM && entrypoint.found_at_address != 0) {
            sr.DESCRIBE_ROOM = entrypoint.found_at_address - 1;
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == DESCRIBE_OBJECTS && entrypoint.found_at_address != 0) {
            sr.DESCRIBE_OBJECTS = entrypoint.found_at_address - 1;
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == MAZE_MOUSE_F && entrypoint.found_at_address != 0) {
            shogun_find_maze_globals(entrypoint, start);
        } else if (entrypoint.fn == DISPLAY_MAZE && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xbe, 0x05, 0x57, 0x2c, 0x01, 0x01, 0x62, 0x03, WILDCARD, 0x41, 0x0d, 0x04, 0x00, 0x62, 0x04, WILDCARD, 0x58 }, { &sg.MAZE_HEIGHT, &sg.MAZE_WIDTH }, entrypoint.found_at_address, 300);

            if (start == -1)
                fprintf(stderr, "Did not find MAZE_HEIGHT or MAZE_WIDTH!\n");
        } else if (entrypoint.fn == MAZE_F && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xbb, 0x8c, 0x00, 0x17, 0x2d, 0x02, WILDCARD, 0x2d, 0x03, WILDCARD, 0xb2 }, { &sg.MAZE_XSTART, &sg.MAZE_YSTART }, entrypoint.found_at_address, 400);
            if (start == -1)
                fprintf(stderr, "Did not find MAZE_XSTART or MAZE_YSTART!\n");
            entrypoint.found_at_address = 0; // MAZE_F
        } else if (entrypoint.fn == shogun_DISPLAY_BORDER && entrypoint.found_at_address != 0 && !found_border_values) {
            start = find_globals_in_pattern({ 0x2d, 0x01, WILDCARD }, { &sg.CURRENT_BORDER }, entrypoint.found_at_address - 27, 100);
            if (start == -1) {
                fprintf(stderr, "Could not find sg.CURRENT_BORDER. Setting it to 0.\n");
                sg.CURRENT_BORDER = 0;
            }
            start = entrypoint.found_at_address + 10;
            if (byte(start) == 0xbe)
                store_byte(start, kOpRtrue);
            found_border_values = true;
        } else if (entrypoint.fn == V_BOW && entrypoint.found_at_address != 0) {
            // Versions 283 through 295 (a total of 9 versions) assign values to the variable that
            // is referenced by variable 1, rather than to variable directly, as later versions do.
            // This causes a crash when variable 1 is 0, which is usually (always?) the case.
            // We fix this by replacing the dereference with a direct assignment, as per the
            // later versions. This seems to have no ill side-effects.

            start = find_pattern_in_mem({ 0x6d, 0x01, WILDCARD, 0x8c, 0x00 }, entrypoint.found_at_address, 100);
            if (start != -1 && memory[start] == 0x6d) {
                store_byte(start, 0x2d);
                fprintf(stderr, "Patched V-BOW offset 0x%x\n", start);
                if (memory[start + 13] == 0x6d) {
                    fprintf(stderr, "Patched V-BOW offset 0x%x\n", start + 9);
                    store_byte(start + 13, 0x2d);
                }
            }
            entrypoint.found_at_address = 0; // V_BOW
        }
    }
}
