//
//  entrypoints.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-06.
//
#include "zterp.h"
#include "screen.h"
#include "memory.h"
#include "dict.h"

#include "journey.hpp"

#include "entrypoints.hpp"

struct EntryPoint {
    Game game;
    std::string title;
    std::vector<uint8_t> pattern;
    int offset;
    uint32_t found_at_address;
    bool stub_original;
    void (*fn)();
};

#define WILDCARD 0xf8

static std::vector<EntryPoint> entrypoints = {
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

        {0x4f, WILDCARD, 0x00, 0x05, 0x4f, WILDCARD, 0x01, 0x06},
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
        { 0x41, WILDCARD, 0x03, 0x4b, 0x0d, WILDCARD, 0x00, 0x0d, 0x3f, 0x01, 0x8c, 0x00, 0x0c},
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
        {0x54, WILDCARD, 0x04, 0x00, 0x25, 0x01},
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
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x05},
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
        {  0x0D, 0x03, 0x05, 0x2D },
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
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x55, 0x02, 0x01, 0x00, 0x76, WILDCARD, 0x00, 0x00, 0x74, WILDCARD, 0x00, 0x03, 0xf9, WILDCARD, WILDCARD, 0x04, 0x3, 0xf1, 0xbf, WILDCARD, 0x55, 0x02},
        0,
        0,
        false,
        BOLD_OBJECT_CURSOR
    },

    {
        Game::Journey,
        "BOLD-OBJECT-CURSOR alt",
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x55, 0x02, 0x01, 0x00, 0x76, WILDCARD, 0x00, 0x00, 0x74, WILDCARD, 0x00, 0x03, 0xf9, WILDCARD, WILDCARD, 0x04, 0x3, 0xf1, 0xbf, WILDCARD, 0x55, 0x02},
        0,
        0,
        false,
        BOLD_OBJECT_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x2d, 0x03},
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR alt",
        { 0x1a, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x2d, 0x03, WILDCARD},
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR older",
        { 0xda, 0x1f, WILDCARD, WILDCARD, 0x01, 0x55, WILDCARD, 0x01, 0x00, 0x74, 0x00, 0x01, 0x04, 0x2d, 0x03, WILDCARD, 0xf9, 0x6b, 0x51, 0x04, 0x03  },
        0,
        0,
        false,
        BOLD_PARTY_CURSOR
    },

    {
        Game::Journey,
        "after INTRO",
        { 0xf1, 0x7f, 0x00, 0xBB },
        0,
        0,
        false,
        after_INTRO
    },

    {
        Game::Journey,
        "CHANGE-NAME",

        {0x0d, 0x07, 0x08, 0xda, 0x1f, WILDCARD, WILDCARD, 0x01},
        0,
        0,
        true,
        CHANGE_NAME
    },

    {
        Game::Journey,
        "CHANGE-NAME alt",

        {0x0d, 0x07, 0x08, 0x1a, WILDCARD, 0x01, 0xd9},
        0,
        0,
        true,
        CHANGE_NAME
    },

    {
        Game::Journey,
        "READ-ELVISH",

        {0xff, 0x7f, 0x01, 0xc5, 0x0d, 0x01, WILDCARD, 0x2d, 0x05  },
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
        { 0xFF, 0x7F, 0x01, 0xC5, 0x0D, 0x01, 0x04},
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

int32_t find_pattern_in_mem(std::vector<uint8_t> pattern, uint32_t startpos, uint32_t length_to_search) {
    int32_t end = startpos + length_to_search - pattern.size();
    for (int32_t i = startpos; i < end; i++) {
        bool found = true;
        for (int j = 0; j < pattern.size(); j++) {
            if (pattern[j] != WILDCARD && pattern[j] != memory[i + j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

int32_t find_globals_in_pattern(std::vector<uint8_t> pattern, std::vector<uint8_t *> vars, uint32_t startpos, uint32_t length_to_search
                                ) {
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    int varsindex = 0;
    int32_t last_match_offset = offset;
    for (int i = 0; i < pattern.size(); i++) {
        if (pattern[i] == WILDCARD) {
            *vars[varsindex++] = memory[offset + i] - 0x10;
            last_match_offset = offset + i;
        }
    }
    return last_match_offset;
}

int32_t find_routines_in_pattern(std::vector<uint8_t> pattern, std::vector<uint32_t *> routines, uint32_t startpos, uint32_t length_to_search
                                ) {
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    int routineindex = 0;
    int32_t last_match_offset = offset;
    for (int i = 0; i < pattern.size() - 1; i++) {
        if (pattern[i] == WILDCARD && pattern[i + 1] == WILDCARD) {
            *routines[routineindex++] = unpack_routine(word(offset + i));
            last_match_offset = offset + i;
        }
    }
    return last_match_offset;
}

int32_t find_values_in_pattern(std::vector<uint8_t> pattern, std::vector<uint8_t *> vals, uint32_t startpos, uint32_t length_to_search
                                 ) {
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    int valindex = 0;
    int32_t last_match_offset = offset;
    for (int i = 0; i < pattern.size(); i++) {
        if (pattern[i] == WILDCARD) {
            *vals[valindex++] = byte(offset + i);
            last_match_offset = offset + i;
            fprintf(stderr, "Found value 0x%x at offset 0x%x\n", *vals[valindex - 1], offset + i);
        }
    }
    return last_match_offset;
}

int32_t find_16_bit_values_in_pattern(std::vector<uint8_t> pattern, std::vector<uint16_t *> vals, uint32_t startpos, uint32_t length_to_search
                               ) {
    fprintf(stderr, "find_16_bit_values_in_pattern, starting at 0x%x\n", startpos);
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    int valindex = 0;
    int32_t last_match_offset = offset;
    for (int i = 0; i < pattern.size() - 1; i++) {
        if (pattern[i] == WILDCARD && pattern[i + 1] == WILDCARD) {
            *vals[valindex++] = word(offset + i);
            last_match_offset = offset + i;
            fprintf(stderr, "Found value 0x%x at offset 0x%x\n", *vals[valindex - 1], offset + i);
        }
    }
    return last_match_offset;
}

void find_globals(void) {
    for (auto &entrypoint : entrypoints) {
        if (entrypoint.fn == BOLD_PARTY_CURSOR && entrypoint.found_at_address != 0) {

            // Later versions of Journey change font by calling a routine named
            // CHANGE-FONT. (Earlier versions call the FONT/set_font opcode
            // directly.) This routine returns false unless the argument is 3 or 4,
            // meaning that it will never set the font back to the standard font 1
            // (unless font 3 or 4 are unavailable, when it will fall back to this.)

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
            offset = find_pattern_in_mem({ 0xed, 0x3f, 0xff, 0xff, 0xba}, offset, 13000);

            if (offset != -1) {
                store_byte(offset, 0xba);
            }

            jr.BOLD_PARTY_CURSOR = entrypoint.found_at_address - 1;

            // Ugly hack to move entrypoint to end of routine
            offset = find_pattern_in_mem({ 0xab, 0x05 }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == BOLD_CURSOR && entrypoint.found_at_address != 0) {
            jr.BOLD_CURSOR = entrypoint.found_at_address - 1;
            // Ugly hack to move entrypoint to end of routine
            uint32_t offset = find_pattern_in_mem({ 0xab, 0x06 }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == BOLD_OBJECT_CURSOR && entrypoint.found_at_address != 0) {
            jr.BOLD_OBJECT_CURSOR = entrypoint.found_at_address - 1;

            // Ugly hack to move entrypoint to end of routine
            uint32_t offset = find_pattern_in_mem({ 0xb8 }, entrypoint.found_at_address, 200);
            if (offset != -1)
                entrypoint.found_at_address = offset;
        } else if (entrypoint.fn == INIT_SCREEN && entrypoint.found_at_address != 0) {
            uint32_t offset = find_globals_in_pattern({ 0xb0, WILDCARD, 0x06, 0x51, 0x0d, WILDCARD, 00, 0x0d, WILDCARD, 00, 0x0d, WILDCARD, 00,0x0d, WILDCARD, 00 }, { &jg.INTERPRETER, &jg.BORDER_FLAG, &jg.FONT3_FLAG, &jg.FWC_FLAG, &jg.BLACK_PICTURE_BORDER}, entrypoint.found_at_address, 100);

            if (offset == -1)
                offset = entrypoint.found_at_address;
            offset = find_globals_in_pattern({ 0x00, 0x11, 0x00, 0x77, 0x00, WILDCARD, WILDCARD, 0x0f, 0x00, 0x12, 0x00, 0x77, 0x00, WILDCARD, WILDCARD }, { &jg.CHRH, &jg.SCREEN_WIDTH, &jg.CHRV, &jg.SCREEN_HEIGHT}, offset, 200);

            offset = find_globals_in_pattern({ 0x0d, WILDCARD, 0x01, 0x55, WILDCARD, 0x04, WILDCARD }, { &jg.TOP_SCREEN_LINE, &jg.SCREEN_HEIGHT, &jg.COMMAND_START_LINE}, offset + 1, 100);

            if (offset == -1) {

                 find_globals_in_pattern({ 0x2d, 0x01, WILDCARD, 0x55, WILDCARD, 0x01, 0x00 }, { &jg.TOP_SCREEN_LINE, &jg.COMMAND_START_LINE}, entrypoint.found_at_address, 200);

                offset = find_globals_in_pattern({ 0x56, WILDCARD, 0x04, 0x00, 0x75, WILDCARD, 0x00, 0x00, 0x55, 0x00, 0x01, WILDCARD  }, { &jg.COMMAND_WIDTH, &jg.SCREEN_WIDTH, &jg.NAME_WIDTH}, entrypoint.found_at_address, 100);

            }

            offset = find_globals_in_pattern({ 0x74, WILDCARD, WILDCARD, WILDCARD, 0x74, WILDCARD, WILDCARD, WILDCARD, 0x74, WILDCARD, WILDCARD, WILDCARD}, { &jg.PARTY_COMMAND_COLUMN, &jg.COMMAND_WIDTH, &jg.NAME_COLUMN, &jg.NAME_COLUMN, &jg.NAME_WIDTH, &jg.CHR_COMMAND_COLUMN, &jg.CHR_COMMAND_COLUMN, &jg.COMMAND_WIDTH, &jg.COMMAND_OBJECT_COLUMN}, offset + 1, 100);

            offset = find_values_in_pattern({ 0xbe, 0x17, 0x3f, 0xff, 0xff, 0xe0, WILDCARD, WILDCARD, WILDCARD, WILDCARD, WILDCARD, 0x00 }, {&ja.SEEN, &ja.SEEN, &ja.SEEN, &ja.SEEN, &ja.SEEN}, offset, 200);

            if (offset != -1) {
                jo.START_LOC = byte(offset - 1);

                find_globals_in_pattern({0x55, WILDCARD, 0x01, 0x00, 0xf9}, { &jg.TEXT_WINDOW_LEFT }, offset + 0x50, 200);
            } else {
                find_globals_in_pattern({ 0x2d, 0x01, WILDCARD, 0x55, WILDCARD, 0x01, 0x00, 0x61, 0x01, 0x00, 0xdd, 0x55, WILDCARD, 0x01, 0x00}, { &jg.TOP_SCREEN_LINE, &jg.COMMAND_START_LINE, &jg.TEXT_WINDOW_LEFT }, entrypoint.found_at_address, 200);
            }

        } else if (entrypoint.fn == PRINT_CHARACTER_COMMANDS && entrypoint.found_at_address != 0) {
            jg.UPDATE_FLAG = byte(entrypoint.found_at_address + 7) - 0x10;
            jr.FILL_CHARACTER_TBL = unpack_routine(word(entrypoint.found_at_address + 13));

            jg.PARTY = byte(entrypoint.found_at_address + 17) - 0x10;

            uint32_t offset = find_globals_in_pattern({ 0xa0, WILDCARD, 0xc1, 0x0d }, { &jg.SMART_DEFAULT_FLAG }, entrypoint.found_at_address, 100);

            offset = find_routines_in_pattern({ 0x00, 0x8f, WILDCARD, WILDCARD, 0xb0 }, { &jr.SMART_DEFAULT }, offset, 100);


            find_globals_in_pattern({ WILDCARD, 0x41, 0x09, 0x78, 0x50 }, { &jg.NAME_WIDTH_PIX }, offset, 100);

            offset = find_globals_in_pattern({0xcd, 0xa0, WILDCARD, 0xd6}, {&jg.SUBGROUP_MODE}, jr.FILL_CHARACTER_TBL + 1, 200);

            offset = find_values_in_pattern({0x4a, 0x02, WILDCARD, 0xd2}, {&ja.SUBGROUP}, offset, 200);
            offset = find_values_in_pattern({0x4a, 0x02, WILDCARD, 0xce}, {&ja.SHADOW}, offset, 200);
            find_globals_in_pattern({0x6f, WILDCARD, 0x01, 0x03 }, {&jg.CHARACTER_INPUT_TBL}, offset, 200);


        } else if (entrypoint.fn == after_INTRO && entrypoint.found_at_address != 0) {

            uint8_t graphic = 0;
            uint32_t offset = find_values_in_pattern({ 0x00, 0x3a, WILDCARD, 0x00}, {&graphic}, entrypoint.found_at_address - 0x20, 100);
            if (offset != -1) {
                jr.GRAPHIC = unpack_routine(graphic);
            }

            offset = find_values_in_pattern({ 0xed, 0x7f, WILDCARD, 0xb0}, {&ja.buffer_window_index}, entrypoint.found_at_address, 500);
            if (offset == -1) {
                fprintf(stderr, "Error!\n");
                offset = entrypoint.found_at_address;
            }
            for (int i = offset; i < memory_size - 12; i++) {
                if (memory[i] == 0xb0) {
                    entrypoint.found_at_address = i;
                    break;
                }
            }
        } else if (entrypoint.fn == ERASE_COMMAND && entrypoint.found_at_address != 0) {
            find_globals_in_pattern({ 0x01, 0xc5, 0x2d, 0x01,  WILDCARD }, { &jg.COMMAND_WIDTH_PIX}, entrypoint.found_at_address - 7, 50);
        } else if (entrypoint.fn == READ_ELVISH && entrypoint.found_at_address != 0) {

            uint32_t offset = find_pattern_in_mem({ 0x0d,  0x01,  WILDCARD, 0x2d, 0x05, WILDCARD }, entrypoint.found_at_address - 3, 100);

            // Change start of READ-ELVISH
            // to return local variable 1 (named OFF)
            // (0xab 0x01 means RET L00)
            store_byte(offset + 3, 0xab);
            store_byte(offset + 4, 0x01);

            find_routines_in_pattern({ 0xda, 0x2f, WILDCARD, WILDCARD, 0x02, 0x2d}, {&jr.MASSAGE_ELVISH}, offset + 1, 300);

            offset = find_globals_in_pattern({ 0xe2, 0x97, WILDCARD, 0x00, 0x14, 0xe2, 0x97, WILDCARD, 0x00, 0x32 }, { &jg.E_LEXV, &jg.E_INBUF}, entrypoint.found_at_address, 100);
            offset = find_globals_in_pattern({0x04, 0x05, 0x2d, 0x06, WILDCARD }, {&jg.E_TEMP}, offset, 200);

            offset = find_globals_in_pattern({0x2d, WILDCARD, 0x02 }, {&jg.E_TEMP_LEN}, offset, 200);

            uint8_t praxix = 0, bergon1, bergon2;
            offset = find_values_in_pattern({0xc1, 0x93, 0x01, WILDCARD, WILDCARD, WILDCARD, 0xc1}, {&praxix, &bergon1, &bergon2}, offset, 200);

            jo.PRAXIX = praxix;
            jo.BERGON = word(offset - 1);
            find_routines_in_pattern({0xc1, 0x8f, WILDCARD, WILDCARD, 0xb0, 0xc1}, {&jr.PARSE_ELVISH}, offset, 200);


        } else if (entrypoint.fn == CHANGE_NAME && entrypoint.found_at_address != 0) {

            uint32_t offset = find_pattern_in_mem({0x2d, 0x04, WILDCARD, 0x8f}, entrypoint.found_at_address - 0x10, 1);
            if (offset != -1) {
                // Hack for later versions
                store_byte(offset, 0xb0);
            }

            uint8_t tag = 0, dummy;
            offset = find_values_in_pattern({WILDCARD, 0x00, 0x74, WILDCARD, 0x00, 0x00}, {&tag, &dummy}, entrypoint.found_at_address, 200);
            jo.TAG = tag;

            offset = find_routines_in_pattern({0xd9, 0x2f, WILDCARD, WILDCARD, 0x01, 0x00}, {&jr.ILLEGAL_NAME}, offset, 200);
            offset = find_routines_in_pattern({0x8f, WILDCARD, WILDCARD, 0xfc, 0xa6}, {&jr.REMOVE_TRAVEL_COMMAND}, offset, 200);
            offset = find_globals_in_pattern({0xfc, 0xa6, WILDCARD, 0x01, 0x02, WILDCARD}, {&jg.NAME_TBL, &jg.TAG_NAME}, offset - 3, 200);

            offset = find_values_in_pattern({0xe3, 0x1b, WILDCARD, WILDCARD, WILDCARD, 0x08}, {&ja.KBD, &ja.KBD, &ja.KBD}, offset, 200);

            jo.TAG_OBJECT = word(offset - 2);

            offset = find_globals_in_pattern({0x08, 0x2d, WILDCARD, 0x01,}, {&jg.TAG_NAME_LENGTH}, offset, 200);

            // Change hyphens to underscores in CHANGE-NAME
            offset = find_pattern_in_mem({0xe5, 0x7f, 0x2d }, offset, 300);
            store_byte(offset + 2, 0x5f);
            offset = find_pattern_in_mem({0xe5, 0x7f, 0x2d }, offset + 3, 300);
            store_byte(offset + 2, 0x5f);

            find_globals_in_pattern({ 0xff, 0x7f, 0x01, 0xc5, 0x2d, 0x01, WILDCARD }, {&jg.HERE}, jr.REMOVE_TRAVEL_COMMAND, 200);

        } else if (entrypoint.fn == PRINT_COLUMNS && entrypoint.found_at_address != 0) {
            uint32_t offset = find_globals_in_pattern({0x2d, 0x06, WILDCARD, 0x8c, 0x00, 0x16}, { &jg.PARTY_COMMANDS }, entrypoint.found_at_address, 200);

            offset = find_globals_in_pattern({0x54, WILDCARD, 0x0a, 0x06}, { &jg.O_TABLE }, offset, 200);


            offset = find_16_bit_values_in_pattern({0xc1, 0x8f, 0x05, WILDCARD, WILDCARD, 0x4b}, { &jo.TAG_ROUTE_COMMAND }, offset, 200);

            uint32_t print_desc = 0;
            offset = find_routines_in_pattern({0xf9, 0x27, WILDCARD, WILDCARD, 0x05, 0x01, 0x95, 0x04}, {&print_desc}, offset, 200);

            if (offset == -1) {
                find_routines_in_pattern({0x8c, 0x00, 0x07, 0xda, 0x2f, WILDCARD, WILDCARD, 0x05, 0x95, 0x04}, {&print_desc}, entrypoint.found_at_address, 200);
            }

            find_values_in_pattern({0x4b, 0x51, 0x01, WILDCARD, 0x02}, {&ja.DESC8}, print_desc, 200);

            offset = find_values_in_pattern({0x4b, 0x51, 0x01, WILDCARD, 0x02, 0xa0, 0x02, 0xc4, 0xab, 0x02, 0x51, 0x01, WILDCARD, 0x00, 0xb8}, {&ja.DESC12, &ja.SDESC}, print_desc, 200);

            offset = find_pattern_in_mem({0x42, WILDCARD, 0x0c, 0x4b, 0x4f, 0x01}, offset, 100);

            // Change limit for short command variants
            // in GET-COMMAND from 12 to 13
            if (offset != -1)
                store_byte(offset + 2, 0xd);

        } else if (entrypoint.fn == COMPLETE_DIAL_GRAPHICS && entrypoint.found_at_address != 0) {
            jr.COMPLETE_DIAL_GRAPHICS = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == REFRESH_CHARACTER_COMMAND_AREA && entrypoint.found_at_address != 0) {
            store_byte(entrypoint.found_at_address, 0xab);
            store_byte(entrypoint.found_at_address + 1, 0x01);
        }  else if (entrypoint.fn == TELL_AMOUNTS && entrypoint.found_at_address != 0 && header.release >= 51) {
            uint32_t offset = entrypoint.found_at_address;
            store_byte(offset + 4, 0xb4);
            store_byte(offset + 5, 0xb4);
            store_byte(offset + 6, 0xb4);
        }
    }
}

extern uint8_t *memory;


void find_entrypoints(void) {

    int start = header.static_start;
    int end = memory_size - 12;

    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game)) {
            if (entrypoint.pattern.size()) {
                int32_t offset = find_pattern_in_mem(entrypoint.pattern, start, end - start);
                if (offset != -1) {
                    entrypoint.found_at_address = offset + entrypoint.offset;
                    fprintf(stderr, "Found routine %s at offset 0x%04x\n", entrypoint.title.c_str(), entrypoint.found_at_address);
                    start = entrypoint.found_at_address;
                    if (entrypoint.stub_original) {
                        // Overwrite original byte with rtrue;
                        store_byte(entrypoint.found_at_address, 0xb0);
                    }
                }
            }
        }
    }

    find_globals();

    fprintf(stderr, "\nFILL_CHARACTER_TBL: 0x%x\n", jr.FILL_CHARACTER_TBL);
    fprintf(stderr, "SMART_DEFAULT: 0x%x\n", jr.SMART_DEFAULT);
    fprintf(stderr, "MASSAGE_ELVISH: 0x%x\n", jr.MASSAGE_ELVISH);
    fprintf(stderr, "PARSE_ELVISH: 0x%x\n", jr.PARSE_ELVISH);
    fprintf(stderr, "ILLEGAL_NAME: 0x%x\n", jr.ILLEGAL_NAME);
    fprintf(stderr, "REMOVE_TRAVEL_COMMAND: 0x%x\n", jr.REMOVE_TRAVEL_COMMAND);
    fprintf(stderr, "BOLD_PARTY_CURSOR: 0x%x\n", jr.BOLD_PARTY_CURSOR);
    fprintf(stderr, "BOLD_CURSOR: 0x%x\n", jr.BOLD_CURSOR);
    fprintf(stderr, "BOLD_OBJECT_CURSOR: 0x%x\n", jr.BOLD_OBJECT_CURSOR);
    fprintf(stderr, "GRAPHIC: 0x%x\n", jr.GRAPHIC);
    fprintf(stderr, "COMPLETE_DIAL_GRAPHICS: 0x%x\n", jr.COMPLETE_DIAL_GRAPHICS);

    fprintf(stderr, "\nbuffer_window_index: 0x%x\n", ja.buffer_window_index);
    fprintf(stderr, "\n•SEEN: 0x%x\n", ja.SEEN);
    fprintf(stderr, "KBD: 0x%x\n", ja.KBD);
    fprintf(stderr, "SHADOW: 0x%x\n", ja.SHADOW);
    fprintf(stderr, "SUBGROUP: 0x%x\n", ja.SUBGROUP);
    fprintf(stderr, "SDESC: 0x%x\n", ja.SDESC);
    fprintf(stderr, "DESC8: 0x%x\n", ja.DESC8);
    fprintf(stderr, "DESC12: 0x%x\n", ja.DESC12);
    fprintf(stderr, "\n•START_LOC: 0x%x\n", jo.START_LOC);
    fprintf(stderr, "TAG_OBJECT: 0x%x\n", jo.TAG_OBJECT);
    fprintf(stderr, "TAG_ROUTE_COMMAND: 0x%x\n", jo.TAG_ROUTE_COMMAND);
    fprintf(stderr, "TAG: 0x%x\n", jo.TAG);
    fprintf(stderr, "PRAXIX: 0x%x\n", jo.PRAXIX);
    fprintf(stderr, "BERGON: 0x%x\n", jo.BERGON);

    fprintf(stderr, "\nCOMMAND_START_LINE: 0x%x (0x%x)\n", jg.COMMAND_START_LINE, GLOBAL(jg.COMMAND_START_LINE));
    fprintf(stderr, "NAME_COLUMN: 0x%x (0x%x)\n", jg.NAME_COLUMN, GLOBAL(jg.NAME_COLUMN));
    fprintf(stderr, "PARTY_COMMAND_COLUMN: 0x%x (0x%x)\n", jg.PARTY_COMMAND_COLUMN, GLOBAL(jg.PARTY_COMMAND_COLUMN));
    fprintf(stderr, "CHR_COMMAND_COLUMN: 0x%x(0x%x)\n", jg.CHR_COMMAND_COLUMN, GLOBAL(jg.CHR_COMMAND_COLUMN));
    fprintf(stderr, "COMMAND_OBJECT_COLUMN: 0x%x (0x%x)\n", jg.COMMAND_OBJECT_COLUMN, GLOBAL(jg.COMMAND_OBJECT_COLUMN));
    fprintf(stderr, "COMMAND_WIDTH: 0x%x (0x%x)\n", jg.COMMAND_WIDTH, GLOBAL(jg.COMMAND_WIDTH));
    fprintf(stderr, "•COMMAND_WIDTH_PIX: 0x%x (0x%x)\n", jg.COMMAND_WIDTH_PIX, GLOBAL(jg.COMMAND_WIDTH_PIX));
    fprintf(stderr, "NAME_WIDTH: 0x%x (0x%x)\n", jg.NAME_WIDTH, GLOBAL(jg.NAME_WIDTH));
    fprintf(stderr, "•NAME_WIDTH_PIX: 0x%x (0x%x)\n", jg.NAME_WIDTH_PIX, GLOBAL(jg.NAME_WIDTH_PIX));
    fprintf(stderr, "•INTERPRETER: 0x%x (0x%x)\n", jg.INTERPRETER, GLOBAL(jg.INTERPRETER));
    fprintf(stderr, "\nCHRV: 0x%x (0x%x)\n", jg.CHRV, GLOBAL(jg.CHRV));
    fprintf(stderr, "CHRH: 0x%x (0x%x)\n", jg.CHRH, GLOBAL(jg.CHRH));
    fprintf(stderr, "SCREEN_HEIGHT: 0x%x (0x%x)\n", jg.SCREEN_HEIGHT, GLOBAL(jg.SCREEN_HEIGHT));
    fprintf(stderr, "SCREEN_WIDTH: 0x%x (0x%x)\n", jg.SCREEN_WIDTH, GLOBAL(jg.SCREEN_WIDTH));
    fprintf(stderr, "\n•BORDER_FLAG: 0x%x\n", jg.BORDER_FLAG);
    fprintf(stderr, "•FONT3_FLAG: 0x%x\n", jg.FONT3_FLAG);

    fprintf(stderr, "•FWC_FLAG: 0x%x\n", jg.FWC_FLAG);
    fprintf(stderr, "•BLACK_PICTURE_BORDER: 0x%x\n", jg.BLACK_PICTURE_BORDER);
    fprintf(stderr, "\nHERE: 0x%x\n", jg.HERE);

    fprintf(stderr, "\nTOP_SCREEN_LINE: 0x%x\n", jg.TOP_SCREEN_LINE);
    fprintf(stderr, "TEXT_WINDOW_LEFT: 0x%x\n", jg.TEXT_WINDOW_LEFT);
    fprintf(stderr, "\nE_LEXV: 0x%x\n", jg.E_LEXV);
    fprintf(stderr, "E_INBUF: 0x%x\n", jg.E_INBUF);
    fprintf(stderr, "E_TEMP: 0x%x\n", jg.E_TEMP);
    fprintf(stderr, "E_TEMP_LEN: 0x%x\n", jg.E_TEMP_LEN);

    fprintf(stderr, "\nPARTY: 0x%x\n", jg.PARTY);
    fprintf(stderr, "PARTY_COMMANDS: 0x%x\n", jg.PARTY_COMMANDS);
    fprintf(stderr, "CHARACTER_INPUT_TBL: 0x%x\n", jg.CHARACTER_INPUT_TBL);
    fprintf(stderr, "O_TABLE: 0x%x\n", jg.O_TABLE);

    fprintf(stderr, "\nTAG_NAME: 0x%x\n", jg.TAG_NAME);
    fprintf(stderr, "NAME_TBL: 0x%x\n", jg.NAME_TBL);
    fprintf(stderr, "TAG_NAME_LENGTH: 0x%x\n", jg.TAG_NAME_LENGTH);

    fprintf(stderr, "\nUPDATE_FLAG: 0x%x\n", jg.UPDATE_FLAG);
    fprintf(stderr, "SMART_DEFAULT_FLAG: 0x%x\n", jg.SMART_DEFAULT_FLAG);
    fprintf(stderr, "SUBGROUP_MODE: 0x%x\n", jg.SUBGROUP_MODE);
}


void check_entrypoints(uint32_t pc) {
    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game) && pc == entrypoint.found_at_address) {
            fprintf(stderr, "Found entrypoint %s at pc 0x%04x. Calling function.\n", entrypoint.title.c_str(), entrypoint.found_at_address);
            (entrypoint.fn)();
        }
    }
}
