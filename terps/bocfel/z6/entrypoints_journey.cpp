//
//  entrypoints_journey.cpp
//  bocfel6
//
// Journey's rows of the entrypoint table and its globals finder. The table
// format and the search helpers are in entrypoints_internal.hpp, the scan
// that fills in the table in entrypoints.cpp.

#include "screen.h"
#include "dict.h"
#include "v6_shared.hpp"
#include "journey.hpp"

#include "entrypoints.hpp"
#include "entrypoints_internal.hpp"

// Hook functions that only name a row (see entrypoints_internal.hpp)
static void TELL_AMOUNTS(void) {}
static void COMPLETE_DIAL_GRAPHICS(void) {}

std::vector<EntryPoint> journey_entrypoints = {
    {
        Game::Journey,
        "WCENTER",
        { 0xd9, 0x2f, 0x02, 0x81, 0x01, 0x02 },
        0,
        0,
        true,
        WCENTER
    },

    {
        Game::Journey,
        "WCENTER alt",
        { 0xf3, 0x4f, 0x03, 0x38, WILDCARD, 0xad, 0x01, 0xf3, 0x3f, 0xff, 0xfd, 0x0f, 0x00, 0x18, 0x02 },
        0,
        0,
        true,
        WCENTER
    },

    {
        Game::Journey,
        "WCENTER alt 2",
        { 0xa0, WILDCARD, 0xd1, 0x75, WILDCARD, WILDCARD, 0x00, 0x75, 0x00, 0x02, 0x00 },
        0,
        0,
        true,
        WCENTER
    },

    {
        Game::Journey,
        "WCENTER older",
        { 0x75, WILDCARD, WILDCARD, 0x00, 0x75, 0x00, 0x02, 0x00, 0x57, 0x00 },
        0,
        0,
        true,
        WCENTER
    },

    {
        Game::Journey,
        "GRAPHIC-STAMP",

        { 0x4f, WILDCARD, 0x00, 0x05, 0x4f, WILDCARD, 0x01, 0x06 },
        0,
        0,
        true,
        GRAPHIC_STAMP
    },

    {
        Game::Journey,
        "INIT-SCREEN",
        { 0x41, WILDCARD, 0x06, 0x51, 0x0d, WILDCARD, 0x00 },
        0,
        0,
        true,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "INIT-SCREEN alt",
        { 0x41, WILDCARD, 0x03, 0x4b, 0x0d, WILDCARD, 0x00, 0x0d, 0x3f, 0x01, 0x8c, 0x00, 0x0c },
        0,
        0,
        true,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "INIT-SCREEN older",
        { 0x0f, 0x00, 0x11, 0x00, 0x77, 0x00, WILDCARD, WILDCARD, 0x0f, 0x00, 0x12, 0x00, 0x77, 0x00, WILDCARD, WILDCARD, 0x55 },
        0,
        0,
        true,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "REFRESH-CHARACTER-COMMAND-AREA",
        { 0x54, WILDCARD, 0x04, 0x00, 0x25, 0x01 },
        0,
        0,
        false,
        REFRESH_CHARACTER_COMMAND_AREA
    },

    {
        Game::Journey,
        "ERASE-COMMAND",

        {  0xee, 0xbf, 0x01, 0xb0 },
        0,
        0,
        true,
        ERASE_COMMAND
    },

    {
        Game::Journey,
        "ERASE-COMMAND alt",

        {  0xff, 0x7f, 0x01, 0xc5, 0x2d, 0x01, WILDCARD, 0xa0, WILDCARD, 0x47 },
        7,
        0,
        true,
        ERASE_COMMAND
    },

    {
        Game::Journey,
        "BOLD-CURSOR",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x55 },
        0,
        0,
        false,
        BOLD_CURSOR
    },

    {
        Game::Journey,
        "BOLD-CURSOR alt",
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x05 },
        0,
        0,
        false,
        BOLD_CURSOR
    },

    {
        Game::Journey,
        "REFRESH-SCREEN",
        { 0xff, 0x7f, 0x01, 0xc5, 0x0d, 0x01, 0x01, 0xa0 },
        0,
        0,
        false,
        REFRESH_SCREEN
    },

    {
        Game::Journey,
        "PRINT-CHARACTER-COMMANDS",
        {  0x0d, 0x03, 0x05, 0x2d },
        0,
        0,
        true,
        PRINT_CHARACTER_COMMANDS
    },

    {
        Game::Journey,
        "PRINT-COLUMNS",
        {  0x2d, 0x04, WILDCARD, 0xda, 0x1f, WILDCARD, WILDCARD, 0x01 },
        0,
        0,
        true,
        PRINT_COLUMNS
    },

    {
        Game::Journey,
        "PRINT-COLUMNS alt",
        {  0x2d, 0x04, WILDCARD, 0x1a, WILDCARD, 0x01, 0xa0, 0x01, 0xcb },
        0,
        0,
        true,
        PRINT_COLUMNS
    },

    {
        Game::Journey,
        "BOLD-OBJECT-CURSOR",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x55, 0x02, 0x01, 0x00, 0x76, WILDCARD, 0x00, 0x00, 0x74, WILDCARD, 0x00, 0x03, 0xf9, WILDCARD, WILDCARD, 0x04, 0x3, 0xf1, 0xbf, WILDCARD, 0x55, 0x02 },
        0,
        0,
        false,
        BOLD_OBJECT_CURSOR
    },

    {
        Game::Journey,
        "BOLD-OBJECT-CURSOR alt",
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x55, 0x02, 0x01, 0x00, 0x76, WILDCARD, 0x00, 0x00, 0x74, WILDCARD, 0x00, 0x03, 0xf9, WILDCARD, WILDCARD, 0x04, 0x3, 0xf1, 0xbf, WILDCARD, 0x55, 0x02 },
        0,
        0,
        false,
        BOLD_OBJECT_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x2d, 0x03 },
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR alt",
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x2d, 0x03, WILDCARD },
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR older",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x2d, 0x03, WILDCARD, 0xf9, 0x6b, 0x51, 0x04, 0x03 },
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "after INTRO",
        { 0xf1, 0x7f, 0x00, 0xbb },
        0,
        0,
        false,
        after_INTRO
    },

    {
        Game::Journey,
        "CHANGE-NAME",

        { 0x0d, 0x07, 0x08, 0xda, 0x1f, WILDCARD, WILDCARD, 0x01 },
        0,
        0,
        true,
        CHANGE_NAME
    },

    {
        Game::Journey,
        "CHANGE-NAME alt",

        { 0x0d, 0x07, 0x08, 0x1a, WILDCARD, 0x01, 0xd9 },
        0,
        0,
        true,
        CHANGE_NAME
    },

    {
        Game::Journey,
        "READ-ELVISH",

        { 0xff, 0x7f, 0x01, 0xc5, 0x0d, 0x01, WILDCARD, 0x2d, 0x05 },
        7,
        0,
        false,
        READ_ELVISH
    },

    {
        Game::Journey,
        "TELL-AMOUNTS",
        { 0x41, 0x01, 0x0f, 0xc5, 0xb2 },
        0,
        0,
        false,
        TELL_AMOUNTS
    },

    {
        Game::Journey,
        "DIVIDER",
        { 0xff, 0x7f, 0x01, 0xc5, 0x0d, 0x01, 0x04 },
        0,
        0,
        true,
        DIVIDER
    },

    {
        Game::Journey,
        "COMPLETE-DIAL-GRAPHICS",
        { 0x0d, WILDCARD, 0x00, 0x1a, WILDCARD, WILDCARD, 0x88, WILDCARD, WILDCARD, 0x00, 0xb8, 0x00, 0x6f },
        0,
        0,
        false,
        COMPLETE_DIAL_GRAPHICS
    },
};

// Second-pass scanner for Journey. Locates globals for the party screen,
// command-name table, scene-select menu, and patches a buggy V-BOW in
// older releases. See per-block comments for individual quirks.
void find_journey_globals(void) {
    for (auto &entrypoint : journey_entrypoints) {
        if (entrypoint.fn == BOLD_PARTY_CURSOR && entrypoint.found_at_address != 0) {
            // Later versions of Journey change font by calling a routine named
            // CHANGE-FONT. (Earlier versions call the FONT/set_font opcode
            // directly.) This routine returns false unless the argument is 3 or 4,
            // meaning that it will never set the font back to the standard font 1
            // (unless font 3 or 4 are unavailable - then it will fall back to 1.)

            // This is likely unintentional, and we want it to be able to set the font
            // in the main buffer text window back to 1, otherwise it will always use
            // fixed-width font 4. We patch this by replacing the RFALSE label
            // with a jump to the <FONT 1> instruction at the end of the routine.
            uint32_t offset = find_pattern_in_mem({ 0x41, 0x01, 0x04, 0x40, 0xa0 }, header.static_start, 3000);

            if (offset != -1) {
                store_byte(offset + 3, 0x4b);
            } else {
                offset = header.static_start;
            }

            // Don't blank screen on quit
            offset = find_pattern_in_mem({ 0xed, 0x3f, 0xff, 0xff, 0xba }, offset, 13000);

            if (offset != -1) {
                store_byte(offset, kOpQuit);
            }

            jr.BOLD_PARTY_CURSOR = entrypoint.found_at_address - 1;

            // Ugly hack to move entrypoint to end of routine
            offset = find_pattern_in_mem({ kOpRet, 0x05 }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == BOLD_CURSOR && entrypoint.found_at_address != 0) {
            jr.BOLD_CURSOR = entrypoint.found_at_address - 1;
            // Ugly hack to move entrypoint to end of routine
            uint32_t offset = find_pattern_in_mem({ kOpRet, 0x06 }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == BOLD_OBJECT_CURSOR && entrypoint.found_at_address != 0) {
            jr.BOLD_OBJECT_CURSOR = entrypoint.found_at_address - 1;

            // Ugly hack to move entrypoint to end of routine
            uint32_t offset = find_pattern_in_mem({ kOpRetPopped }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == INIT_SCREEN && entrypoint.found_at_address != 0) {
            uint32_t offset = find_globals_in_pattern({ 0xb0, WILDCARD, 0x06, 0x51, 0x0d, WILDCARD, 00, 0x0d, WILDCARD, 00, 0x0d, WILDCARD, 00,0x0d, WILDCARD, 00 }, { &jg.INTERPRETER, &jg.BORDER_FLAG, &jg.FONT3_FLAG, &jg.FWC_FLAG, &jg.BLACK_PICTURE_BORDER }, entrypoint.found_at_address, 100);

            if (offset == -1)
                offset = entrypoint.found_at_address;
            offset = find_globals_in_pattern({ 0x00, 0x11, 0x00, 0x77, 0x00, WILDCARD, WILDCARD, 0x0f, 0x00, 0x12, 0x00, 0x77, 0x00, WILDCARD, WILDCARD }, { &jg.CHRH, &jg.SCREEN_WIDTH, &jg.CHRV, &jg.SCREEN_HEIGHT }, offset, 200);

            offset = find_globals_in_pattern({ 0x0d, WILDCARD, 0x01, 0x55, WILDCARD, 0x04, WILDCARD }, { &jg.TOP_SCREEN_LINE, &jg.SCREEN_HEIGHT, &jg.COMMAND_START_LINE }, offset + 1, 100);

            if (offset == -1) {

                find_globals_in_pattern({ 0x2d, 0x01, WILDCARD, 0x55, WILDCARD, 0x01, 0x00 }, { &jg.TOP_SCREEN_LINE, &jg.COMMAND_START_LINE }, entrypoint.found_at_address, 200);

                offset = find_globals_in_pattern({ 0x56, WILDCARD, 0x04, 0x00, 0x75, WILDCARD, 0x00, 0x00, 0x55, 0x00, 0x01, WILDCARD }, { &jg.COMMAND_WIDTH, &jg.SCREEN_WIDTH, &jg.NAME_WIDTH }, entrypoint.found_at_address, 100);

            }

            offset = find_globals_in_pattern({ 0x74, WILDCARD, WILDCARD, WILDCARD, 0x74, WILDCARD, WILDCARD, WILDCARD, 0x74, WILDCARD, WILDCARD, WILDCARD }, { &jg.PARTY_COMMAND_COLUMN, &jg.COMMAND_WIDTH, &jg.NAME_COLUMN, &jg.NAME_COLUMN, &jg.NAME_WIDTH, &jg.CHR_COMMAND_COLUMN, &jg.CHR_COMMAND_COLUMN, &jg.COMMAND_WIDTH, &jg.COMMAND_OBJECT_COLUMN }, offset + 1, 100);

            offset = find_values_in_pattern({ 0xbe, 0x17, 0x3f, 0xff, 0xff, 0xe0, WILDCARD, WILDCARD, WILDCARD, WILDCARD, WILDCARD, 0x00 }, { &ja.SEEN, &ja.SEEN, &ja.SEEN, &ja.SEEN, &ja.SEEN }, offset, 200);

            if (offset != -1) {
                jo.START_LOC = byte(offset - 1);

                find_globals_in_pattern({ 0x55, WILDCARD, 0x01, 0x00, 0xf9 }, { &jg.TEXT_WINDOW_LEFT }, offset + 0x50, 200);
            } else {
                find_globals_in_pattern({ 0x2d, 0x01, WILDCARD, 0x55, WILDCARD, 0x01, 0x00, 0x61, 0x01, 0x00, 0xdd, 0x55, WILDCARD, 0x01, 0x00 }, { &jg.TOP_SCREEN_LINE, &jg.COMMAND_START_LINE, &jg.TEXT_WINDOW_LEFT }, entrypoint.found_at_address, 200);
            }
        } else if (entrypoint.fn == PRINT_CHARACTER_COMMANDS && entrypoint.found_at_address != 0) {
            jg.UPDATE_FLAG = global_index(byte(entrypoint.found_at_address + 7));
            jr.FILL_CHARACTER_TBL = unpack_routine(word(entrypoint.found_at_address + 13));

            jg.PARTY = global_index(byte(entrypoint.found_at_address + 17));

            uint32_t offset = find_globals_in_pattern({ 0xa0, WILDCARD, 0xc1, 0x0d }, { &jg.SMART_DEFAULT_FLAG }, entrypoint.found_at_address, 100);

            offset = find_routines_in_pattern({ 0x00, 0x8f, WILDCARD, WILDCARD, 0xb0 }, { &jr.SMART_DEFAULT }, offset, 100);
            find_globals_in_pattern({ WILDCARD, 0x41, 0x09, 0x78, 0x50 }, { &jg.NAME_WIDTH_PIX }, offset, 100);
            offset = find_globals_in_pattern({ 0xcd, 0xa0, WILDCARD, 0xd6 }, { &jg.SUBGROUP_MODE }, jr.FILL_CHARACTER_TBL + 1, 200);

            offset = find_values_in_pattern({ 0x4a, 0x02, WILDCARD, 0xd2 }, { &ja.SUBGROUP }, offset, 200);
            offset = find_values_in_pattern({ 0x4a, 0x02, WILDCARD, 0xce }, { &ja.SHADOW }, offset, 200);
            find_globals_in_pattern({ 0x6f, WILDCARD, 0x01, 0x03 }, { &jg.CHARACTER_INPUT_TBL }, offset, 200);
        } else if (entrypoint.fn == after_INTRO && entrypoint.found_at_address != 0) {
            uint8_t graphic = 0;
            uint32_t offset = find_values_in_pattern({ 0x00, 0x3a, WILDCARD, 0x00 }, { &graphic }, entrypoint.found_at_address - 0x20, 100);
            if (offset != -1) {
                jr.GRAPHIC = unpack_routine(graphic);
            }

            offset = find_values_in_pattern({ 0xed, 0x7f, WILDCARD, 0xb0 }, { &ja.buffer_window_index }, entrypoint.found_at_address, 500);
            if (offset == -1) {
                fprintf(stderr, "Error! Did not find values in pattern!\n");
                offset = entrypoint.found_at_address;
            }
            for (int i = offset; i < memory_size - kScanTailMargin; i++) {
                if (memory[i] == kOpRtrue) {
                    entrypoint.found_at_address = i;
                    break;
                }
            }
        } else if (entrypoint.fn == ERASE_COMMAND && entrypoint.found_at_address != 0) {
            find_globals_in_pattern({ 0x01, 0xc5, 0x2d, 0x01,  WILDCARD }, { &jg.COMMAND_WIDTH_PIX }, entrypoint.found_at_address - 7, 50);
        } else if (entrypoint.fn == READ_ELVISH && entrypoint.found_at_address != 0) {
            uint32_t offset = find_pattern_in_mem({ 0x0d,  0x01,  WILDCARD, 0x2d, 0x05, WILDCARD }, entrypoint.found_at_address - 3, 100);

            // Change start of READ-ELVISH
            // to return local variable 1 (named OFF)
            // (0xab 0x01 means RET L00)
            // Guard the miss case: -1 as a uint32_t offset would clobber the header.
            if (offset != -1) {
                patch_ret_local(offset + 3, 1);
            }

            find_routines_in_pattern({ 0xda, 0x2f, WILDCARD, WILDCARD, 0x02, 0x2d }, { &jr.MASSAGE_ELVISH }, offset + 1, 300);

            offset = find_globals_in_pattern({ 0xe2, 0x97, WILDCARD, 0x00, 0x14, 0xe2, 0x97, WILDCARD, 0x00, 0x32 }, { &jg.E_LEXV, &jg.E_INBUF }, entrypoint.found_at_address, 100);
            offset = find_globals_in_pattern({ 0x04, 0x05, 0x2d, 0x06, WILDCARD }, { &jg.E_TEMP }, offset, 200);

            offset = find_globals_in_pattern({ 0x2d, WILDCARD, 0x02 }, { &jg.E_TEMP_LEN }, offset, 200);

            uint8_t praxix = 0, bergon1, bergon2;
            offset = find_values_in_pattern({ 0xc1, 0x93, 0x01, WILDCARD, WILDCARD, WILDCARD, 0xc1 }, { &praxix, &bergon1, &bergon2 }, offset, 200);

            jo.PRAXIX = praxix;
            if (offset != -1)
                jo.BERGON = word(offset - 1);
            find_routines_in_pattern({ 0xc1, 0x8f, WILDCARD, WILDCARD, 0xb0, 0xc1 }, { &jr.PARSE_ELVISH }, offset, 200);
        } else if (entrypoint.fn == CHANGE_NAME && entrypoint.found_at_address != 0) {
            uint8_t tag = 0, dummy;
            uint32_t offset = find_values_in_pattern({ WILDCARD, 0x00, 0x74, WILDCARD, 0x00, 0x00 }, { &tag, &dummy }, entrypoint.found_at_address, 200);
            jo.TAG = tag;

            offset = find_routines_in_pattern({ 0xd9, 0x2f, WILDCARD, WILDCARD, 0x01, 0x00 }, { &jr.ILLEGAL_NAME }, offset, 200);
            offset = find_routines_in_pattern({ 0x8f, WILDCARD, WILDCARD, 0xfc, 0xa6 }, { &jr.REMOVE_TRAVEL_COMMAND }, offset, 200);
            offset = find_globals_in_pattern({ 0xfc, 0xa6, WILDCARD, 0x01, 0x02, WILDCARD }, { &jg.NAME_TBL, &jg.TAG_NAME }, offset - 3, 200);

            offset = find_values_in_pattern({ 0xe3, 0x1b, WILDCARD, WILDCARD, WILDCARD, 0x08 }, { &ja.KBD, &ja.KBD, &ja.KBD }, offset, 200);

            if (offset != -1)
                jo.TAG_OBJECT = word(offset - 2);

            offset = find_globals_in_pattern({ 0x08, 0x2d, WILDCARD, 0x01, }, { &jg.TAG_NAME_LENGTH }, offset, 200);

            // Change hyphens to underscores in CHANGE-NAME. Guard each
            // patch: on a miss find_pattern_in_mem returns -1, which as a
            // uint32_t offset would make store_byte write into the header.
            offset = find_pattern_in_mem({ 0xe5, 0x7f, 0x2d }, offset, 300);
            if (offset != -1) {
                store_byte(offset + 2, 0x5f);
                offset = find_pattern_in_mem({ 0xe5, 0x7f, 0x2d }, offset + 3, 300);
                if (offset != -1)
                    store_byte(offset + 2, 0x5f);
            }

            find_globals_in_pattern({ 0xff, 0x7f, 0x01, 0xc5, 0x2d, 0x01, WILDCARD }, { &jg.HERE }, jr.REMOVE_TRAVEL_COMMAND, 200);

        } else if (entrypoint.fn == PRINT_COLUMNS && entrypoint.found_at_address != 0) {
            uint32_t offset = find_globals_in_pattern({ 0x2d, 0x06, WILDCARD, 0x8c, 0x00, 0x16 }, { &jg.PARTY_COMMANDS }, entrypoint.found_at_address, 200);

            offset = find_globals_in_pattern({ 0x54, WILDCARD, 0x0a, 0x06 }, { &jg.O_TABLE }, offset, 200);

            offset = find_16_bit_values_in_pattern({ 0xc1, 0x8f, 0x05, WILDCARD, WILDCARD, 0x4b }, { &jo.TAG_ROUTE_COMMAND }, offset, 200);

            uint32_t print_desc = 0;
            offset = find_routines_in_pattern({ 0xf9, 0x27, WILDCARD, WILDCARD, 0x05, 0x01, 0x95, 0x04 }, { &print_desc }, offset, 200);

            if (offset == -1) {
                find_routines_in_pattern({ 0x8c, 0x00, 0x07, 0xda, 0x2f, WILDCARD, WILDCARD, 0x05, 0x95, 0x04 }, { &print_desc }, entrypoint.found_at_address, 200);
            }

            find_values_in_pattern({ 0x4b, 0x51, 0x01, WILDCARD, 0x02 }, { &ja.DESC8 }, print_desc, 200);

            offset = find_values_in_pattern({ 0x4b, 0x51, 0x01, WILDCARD, 0x02, 0xa0, 0x02, 0xc4, 0xab, 0x02, 0x51, 0x01, WILDCARD, 0x00, 0xb8 }, { &ja.DESC12, &ja.SDESC }, print_desc, 200);

            offset = find_pattern_in_mem({ 0x42, WILDCARD, 0x0c, 0x4b, 0x4f, 0x01 }, offset, 100);

            // Change limit for short command variants
            // in GET-COMMAND from 12 to 13
            if (offset != -1)
                store_byte(offset + 2, 0xd);

        } else if (entrypoint.fn == COMPLETE_DIAL_GRAPHICS && entrypoint.found_at_address != 0) {
            jr.COMPLETE_DIAL_GRAPHICS = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == REFRESH_CHARACTER_COMMAND_AREA && entrypoint.found_at_address != 0) {
            patch_ret_local(entrypoint.found_at_address, 1);
            // Patch out repeated words when listing essences and reagents in later version when screen is narrow
        }  else if (entrypoint.fn == TELL_AMOUNTS && entrypoint.found_at_address != 0 && header.release >= 51) {
            uint32_t offset = entrypoint.found_at_address;
            store_byte(offset + 4, kOpNop);
            store_byte(offset + 5, kOpNop);
            store_byte(offset + 6, kOpNop);
        }
    }
}
