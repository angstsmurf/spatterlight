//
//  entrypoints_arthur.cpp
//  bocfel6
//
// Arthur's rows of the entrypoint table and its globals finder. The table
// format and the search helpers are in entrypoints_internal.hpp, the scan
// that fills in the table in entrypoints.cpp.

#include "screen.h"
#include "dict.h"
#include "v6_shared.hpp"
#include "arthur.hpp"

#include "entrypoints.hpp"
#include "entrypoints_internal.hpp"

// Hook functions that only name a row (see entrypoints_internal.hpp)
static void RT_SEE_QST(void) {}
static void RT_TH_EXCALIBUR(void) {}

std::vector<EntryPoint> arthur_entrypoints = {
    {
        Game::Arthur,
        "Arthur INIT-STATUS-LINE",
        { 0x41, WILDCARD, 0x00, 0x71, 0x54 },
        0,
        0,
        true,
        arthur_INIT_STATUS_LINE,
    },

    {
        Game::Arthur,
        "UPDATE-STATUS-LINE",
        { 0xeb, 0x7f, 0x01, 0xf1, 0x7f, 0x01 },
        0,
        0,
        false,
        ARTHUR_UPDATE_STATUS_LINE
    },

    {
        Game::Arthur,
        "RT-AUTHOR-OFF",
        { 0xf3, 0x3f, 0xff, 0xfd, 0x43, WILDCARD, 0x00, 0x45 },
        0,
        0,
        true,
        RT_AUTHOR_OFF
    },

    {
        Game::Arthur,
        "V-COLOR",
        { 0x41, WILDCARD, 0x03, 0x00, WILDCARD, 0x88, WILDCARD, WILDCARD, 0x00 },
        0,
        0,
        false,
        V_COLOR
    },

    {
        Game::Arthur,
        "after V-COLOR",
        { },
        0,
        0,
        false,
        after_V_COLOR
    },

    {
        Game::Arthur,
        "RT-HOT-KEY",
        { 0xff, 0x7f, 0x02, 0xc7, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0xff, 0x7f, 0x03, 0xc5 },
        0,
        0,
        true,
        RT_HOT_KEY
    },

    {
        Game::Arthur,
        "RT-HOT-KEY alt",
        { 0xff, 0x7f, 0x02, 0xc7, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x41, 0x01, 0x85, 0x52 },
        0,
        0,
        true,
        RT_HOT_KEY
    },

    {
        Game::Arthur,
        "RT-UPDATE-STAT-WINDOW",
        { 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0xa0 },
        0,
        0,
        false,
        RT_UPDATE_STAT_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-INVT-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0xbe },
        0,
        0,
        false,
        RT_UPDATE_INVT_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-DESC-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x01, 0xa0, 0x01, 0x4a },
        0,
        0,
        false,
        RT_UPDATE_DESC_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-MAP-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0xa0 },
        -0x16,
        0,
        false,
        RT_UPDATE_MAP_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-PICT-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0x41 },
        0,
        0,
        false,
        RT_UPDATE_PICT_WINDOW
    },

    {
        Game::Arthur,
        "RT-TH-EXCALIBUR",
        { 0x54, 0x00, 0x01, 0x00, 0xef, 0xaf, 0x04, 0x00, 0xf1, 0x7f, 0x02 },
        0,
        0,
        false,
        RT_TH_EXCALIBUR
    },

    {
        Game::Arthur,
        "DO-HINTS",
        { 0xf1, 0x7f, 0x00, 0xef, 0x1f, 0xff, 0xff, 0x00,  0x88, WILDCARD, WILDCARD, 0x02 },
        0,
        0,
        true,
        DO_HINTS
    },

    {
        Game::Arthur,
        "DO-HINTS alt",
        { 0xf1, 0x7f, 0x00, 0x88, WILDCARD, WILDCARD, 0x02, 0xbe, 0x12, 0x57, 0x00, 0x01, 0x02 },
        0,
        0,
        true,
        DO_HINTS
    },

    {
        Game::Arthur,
        "DISPLAY-HINT",
        { 0xf1, 0x7f, 0x00, 0xed, 0x7f, 0x00, 0xeb, 0x7f, 0x01, 0xf9 },
        0,
        0,
        false,
        DISPLAY_HINT
    },

    {
        Game::Arthur,
        "RT-SEE-QST",
        { 0xa0, 0x01, 0xc1, 0x22, 0x00, 0x01, 0x48 },
        0,
        0,
        false,
        RT_SEE_QST
    },
};

// Walks Arthur's dynamic memory looking for the original interpreter's
// 50/300/600-tick busy-wait pauses (which used real wallclock delays)
// and replaces each with a sequence of `nop` opcodes so modern players
// don't have to sit through them.
static void patch_arthur_pauses(void) {
    // The pauses are all in the upper part of the story file.
    const uint32_t kSearchStart = 0x10000;
    // Leave room for the longest thing matched or written
    const uint32_t kTailMargin = 0x14;
    // READ_CHAR 1 <time> <routine>, time in tenths of a second, as a byte
    // constant or as a word
    const uint8_t kOpReadChar = 0xf6;
    const uint8_t kTypesByteTime = 0x53, kTypesWordTime = 0x43;
    // What it is replaced with: READ_CHAR 1 with no timeout, storing to the
    // stack, and NOPs over the operands that are no longer there.
    static const std::vector<uint8_t> untimed_short = {kOpReadChar, 0x7f, 0x01, 0x00, kOpNop, kOpNop, kOpNop};
    static const std::vector<uint8_t> untimed_long = {kOpReadChar, 0x7f, 0x01, 0x00, kOpNop, kOpNop, kOpNop, kOpNop};

    if (memory_size < kSearchStart + kTailMargin) {
        fprintf(stderr, "patch_arthur_pauses: Unexpected memory size: 0x%x\n", memory_size);
        return;
    }

    for (uint32_t i = kSearchStart; i < memory_size - kTailMargin; i++) {
        if (memory[i] == kOpReadChar && memory[i + 2] == 0x01 && (memory[i + 1] == kTypesByteTime || memory[i + 1] == kTypesWordTime)) {
            const std::vector<uint8_t> *replacement = nullptr;
            uint16_t time = word(i + 3);
            if (memory[i + 3] == 50) {
                replacement = &untimed_short;
            } else if (time == 600 || time == 300) {
                replacement = &untimed_long;
            }
            if (replacement != nullptr)
                patch_bytes(i, *replacement);
        }
    }
}

// Second-pass scanner for Arthur. Uses each already-located entrypoint
// as an anchor to find nearby globals (color indices, window-type flags,
// timing values...), routine addresses, tables, and to apply small byte
// patches that fix original-game bugs or hardcoded assumptions.
void find_arthur_globals(void) {
    int start = 0;
    for (auto &entrypoint : arthur_entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x0d, WILDCARD, 0x09, 0x0d, WILDCARD, 0x02 }, { &bg_global_idx, &fg_global_idx }, entrypoint.found_at_address, 300);
            if (start == -1) {
                fprintf(stderr, "Error! Could not find color globals!\n");
            } else {
                start = find_pattern_in_mem({ kOpRetPopped }, start, 200);
                if (start != -1) {
                    end_of_color_addr = start;
                }
            }
            entrypoint.found_at_address = 0; // V_COLOR
        } else if (entrypoint.fn == after_V_COLOR && end_of_color_addr != 0) {
            entrypoint.found_at_address = end_of_color_addr;
        } else if (entrypoint.fn == arthur_INIT_STATUS_LINE && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xb0, WILDCARD, 0x00, 0x71 }, { &ag.GL_WINDOW_TYPE }, entrypoint.found_at_address, 100);
            if (start == -1) {
                fprintf(stderr, "Error! Did not find ag.GL_WINDOW_TYPE!\n");
                start = entrypoint.found_at_address;
            }

            int oldstart = start;
            start = find_globals_in_pattern({ 0xf1, 0x7f, 0x00, 0x0d, WILDCARD, 0x00, 0xef, 0x5f, 0x01, 0x01 }, { &ag.GL_TIME_WIDTH }, start, 300);
            if (start == -1) {
                ag.GL_TIME_WIDTH = 0;
                start = oldstart;
            }

            start = find_globals_in_pattern({ 0xef, 0x5f, 0x01, 0x01, 0xeb, 0x7f, 0x00,
                0x0d, WILDCARD, 0x00,
                0x0d, WILDCARD, 0x00,
                0x0d, WILDCARD, 0x00,
                0x0d, WILDCARD, 0x00,
                0x0d, WILDCARD, 0x00,
                0xb0 }, { &ag.GL_SL_HERE, &ag.GL_SL_VEH, &ag.GL_SL_HIDE, &ag.GL_SL_TIME, &ag.GL_SL_FORM }, start, 300);

            if (start == -1) {
                fprintf(stderr, "Error! Did not find global in pattern!\n");
            }
        } else if (entrypoint.fn == ARTHUR_UPDATE_STATUS_LINE && entrypoint.found_at_address != 0) {
            ar.UPDATE_STATUS_LINE = entrypoint.found_at_address - 1;
            ag.UPDATE = global_index(memory[ar.UPDATE_STATUS_LINE + 0xd]);
        } else if (entrypoint.fn == RT_UPDATE_PICT_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_PICT_WINDOW = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_TH_EXCALIBUR && entrypoint.found_at_address != 0) {
            start = find_pattern_in_mem({ 0xf1, 0x7f, 0x02 }, entrypoint.found_at_address, 300);
            if (start != -1) {
                // Patch <HLIGHT ,H-BOLD> to bold fixed-width, which we have
                // mapped to style_User1, (set to bold, centered text) which
                // matches the style of the original centered "THE END" text.

                store_byte(start + 2, 0x0a);
            }
        } else if (entrypoint.fn == RT_UPDATE_INVT_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_INVT_WINDOW = entrypoint.found_at_address - 1;

            // Patch to to fix second vertical bar not being drawn at certain sizes.
            // The purpose of the original code seems to be to avoid drawing at third bar at the right edge
            // but instead it skips the second bar if the window width in characters is evenly divisible by 3.
            // Not sure if it is broken in the original as well or if it is due to us measuring window width
            // differently.

            // We just patch the line that adds 1 to the width to subtract 1 instead.

            // ADD (SP)+,#01 -> -(SP) becomes
            // SUB (SP)+,#01 -> -(SP)
            start = find_pattern_in_mem({ 0x54, 0x00, 0x01, 0x00 }, entrypoint.found_at_address, 300);
            if (start != -1) {
                store_byte(start, 0x55);
            }
        } else if (entrypoint.fn == RT_UPDATE_STAT_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_STAT_WINDOW = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_UPDATE_DESC_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_DESC_WINDOW = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_UPDATE_MAP_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_MAP_WINDOW = entrypoint.found_at_address - 1;
            find_globals_in_pattern({ 0xa0, WILDCARD, 0x55, 0xbe, 0x06 }, { &ag.GL_MAP_GRID_Y }, entrypoint.found_at_address, 10);
        } else if (entrypoint.fn == RT_AUTHOR_OFF && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x3f, 0xff, 0xfd, 0x43, WILDCARD, 0x00, 0x45 }, { &ag.GL_AUTHOR_SIZE }, entrypoint.found_at_address, 100);
            if (start == -1) {
                fprintf(stderr, "ERROR: Could not find ag.GL_AUTHOR_SIZE!!\n");
            }
            start = find_16_bit_values_in_pattern({ 0xd9, 0x0f, WILDCARD, WILDCARD, WILDCARD, WILDCARD }, { &at.K_DIROUT_TBL, &at.K_DIROUT_TBL, &at.K_DIROUT_TBL }, start, 100);
            if (start == -1) {
                fprintf(stderr, "ERROR: Could not find at.K_DIROUT_TBL!!\n");
            }
        } else if (entrypoint.fn == DO_HINTS && entrypoint.found_at_address != 0) {
            start = find_hints_table(entrypoint.found_at_address);
            if (start != -1) {
                start = find_16_bit_values_in_pattern({ 0xd4, 0x1f, WILDCARD, WILDCARD, 0x02, 0x09, 0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x00 }, { &at.K_HINT_ITEMS, &at.K_HINT_ITEMS }, start, 300);
                if (start != -1) {
                    start = find_globals_in_pattern({ 0xf7, 0xab, WILDCARD, 0x09, 0x00, 0x04, 0xc2 }, { &hint_chapter_global_idx }, start, 100);
                    if (start != -1) {
                        find_globals_in_pattern({ 0x2d, 0x04, 0x07, 0xda, 0x2f, WILDCARD, WILDCARD, 0x04, 0x0d, WILDCARD, 0x01 }, { &hint_quest_global_idx, &hint_quest_global_idx, &hint_quest_global_idx }, start, 200);
                    }
                }
            }
        } else if (entrypoint.fn == DISPLAY_HINT && entrypoint.found_at_address != 0) {
            find_seen_hints_table(entrypoint.found_at_address);
        } else if (entrypoint.fn == RT_SEE_QST && entrypoint.found_at_address != 0) {
            ar.RT_SEE_QST = entrypoint.found_at_address - 1;
            entrypoint.found_at_address = 0;
        }
    }
    patch_arthur_pauses();
}
