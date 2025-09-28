//
//  entrypoints.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-06.
//
#include <unordered_map>

#include "zterp.h"
#include "screen.h"
#include "memory.h"
#include "dict.h"

#include "arthur.hpp"
#include "journey.hpp"
#include "shogun.hpp"

#include "v6_shared.hpp"

#include "entrypoints.hpp"

uint8_t fg_global_idx = 0, bg_global_idx = 0;

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

#pragma mark Arthur

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
        { 0xEB, 0x7F, 0x02, 0xBE, 0x12, 0x57, 0x02, 0x01, 0x02, 0xa0 },
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

#pragma mark Journey

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
        { 0xf1, 0x7f, 0x00, 0xBB },
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
        { 0xFF, 0x7F, 0x01, 0xC5, 0x0D, 0x01, 0x04 },
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

#pragma mark Shogun

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
        "SCENE-SELECT alt ",
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

    {
        Game::Shogun,
        "CENTER-PIC-X",
        { 0xbe, 0x06, 0x8f, 0x01, WILDCARD, WILDCARD, 0x40 },
        0,
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
        { 0x02, 0xAB, 0x02, 0x04, 0xA0, WILDCARD, 0x51, 0xB2 },
        4,
        0,
        false,
        DESCRIBE_ROOM
    },

    {
        Game::Shogun,
        "DESCRIBE-OBJECTS",
        { 0xBB, 0xB0, 0x04, 0xA3, WILDCARD, 0x03, 0xA2, WILDCARD, 0x01, 0xC2 },
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
        { 0xBF, 0x00, 0xA0, 0x01, 0xC5, 0x8F },
        9,
        0,
        false,
        after_V_VERSION
    },

    {
        Game::Shogun,
        "V-CREDITS",
        { 0xF1, 0x7F, 0x02, 0xDA, 0x0F },
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

static int32_t find_pattern_in_mem(std::vector<uint8_t> pattern, uint32_t startpos, uint32_t length_to_search) {
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

static int32_t find_globals_in_pattern(std::vector<uint8_t> pattern, std::vector<uint8_t *> vars, uint32_t startpos, uint32_t length_to_search
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

static int32_t find_routines_in_pattern(std::vector<uint8_t> pattern, std::vector<uint32_t *> routines, uint32_t startpos, uint32_t length_to_search
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

static int32_t find_values_in_pattern(std::vector<uint8_t> pattern, std::vector<uint8_t *> vals, uint32_t startpos, uint32_t length_to_search
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
        }
    }
    return last_match_offset;
}

static int32_t find_16_bit_values_in_pattern(std::vector<uint8_t> pattern, std::vector<uint16_t *> vals, uint32_t startpos, uint32_t length_to_search) {
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    int valindex = 0;
    int32_t last_match_offset = offset;
    for (int i = 0; i < pattern.size() - 1; i++) {
        if (pattern[i] == WILDCARD && pattern[i + 1] == WILDCARD) {
            *vals[valindex++] = word(offset + i);
            last_match_offset = offset + i;
        }
    }
    return last_match_offset;
}

static void patch_arthur_pauses(void) {
    if (memory_size < 0x10014) {
        fprintf(stderr, "patch_arthur_pauses: Unexpected memory size: 0x%x\n", memory_size);
        return;
    }

    for (int32_t i = 0x10000; i < memory_size - 0x14; i++) {
        if (memory[i] == 0xf6 && memory[i + 2] == 0x01 && (memory[i + 1] == 0x53 || memory[i + 1] == 0x43)) {
            uint8_t high = memory[i + 3];
            uint8_t low = memory[i + 4];
            std::vector<uint8_t> pattern = {};
            if (high == 0x32) {
                pattern = {0xf6, 0x7f, 0x01, 0x00, 0xb4, 0xb4, 0xb4};
            } else if ((high == 0x02 && low == 0x58) || (high == 0x01 && low == 0x2c)) {
                pattern = {0xf6, 0x7f, 0x01, 0x00, 0xb4, 0xb4, 0xb4, 0xb4};
            }
            for (int j = 0; j < pattern.size(); j++) {
                memory[i + j] = pattern[j];
            }
        }
    }
}

static uint32_t end_of_color_addr = 0;

static void find_arthur_globals(void) {
    int start = 0;
    for (auto &entrypoint : entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x0d, WILDCARD, 0x09, 0x0d, WILDCARD, 0x02 }, { &bg_global_idx, &fg_global_idx }, entrypoint.found_at_address, 300);
            if (start == -1) {
                fprintf(stderr, "Error! Could not find color globals!\n");
            } else {
//                fprintf(stderr, "Global index of fg: 0x%x Global index of bg: 0x%x\n", fg_global_idx, bg_global_idx);
                start = find_pattern_in_mem({ 0xb8 }, start, 200);
                if (start != -1) {
                    end_of_color_addr = start;
//                    fprintf(stderr, "Found return from routine V_COLOR at address 0x%x\n", end_of_color_addr);
                }
            }
            entrypoint.found_at_address = 0; // V_COLOR
        } else if (entrypoint.fn == after_V_COLOR && end_of_color_addr != 0) {
            entrypoint.found_at_address = end_of_color_addr;
//            fprintf(stderr, "after_V_COLOR at address 0x%x\n", entrypoint.found_at_address);
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
            ag.UPDATE = memory[ar.UPDATE_STATUS_LINE + 0xd] - 0x10;
        } else if (entrypoint.fn == RT_UPDATE_PICT_WINDOW && entrypoint.found_at_address != 0) {
            ar.RT_UPDATE_PICT_WINDOW = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_TH_EXCALIBUR && entrypoint.found_at_address != 0) {
            start = find_pattern_in_mem({ 0xf1, 0x7f, 0x02 }, entrypoint.found_at_address, 300);
            if (start != -1) {
                // Patch <HLIGHT ,H-BOLD> to bold fixed-width, which we have
                // mapped to style_User1, (set to bold, centered text) which
                // matches the style of the original centered "THE END" text.

                memory[start + 2] = 0x0a;
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
            } else {
//                fprintf(stderr, "ag.GL_AUTHOR_SIZE = 0x%x\n", ag.GL_AUTHOR_SIZE);
            }
            start = find_16_bit_values_in_pattern({ 0xd9, 0x0f, WILDCARD, WILDCARD, WILDCARD, WILDCARD }, { &at.K_DIROUT_TBL, &at.K_DIROUT_TBL, &at.K_DIROUT_TBL }, start, 100);
            if (start == -1) {
                fprintf(stderr, "ERROR: Could not find at.K_DIROUT_TBL!!\n");
            } else {
//                fprintf(stderr, "at.K_DIROUT_TBL = 0x%x\n", at.K_DIROUT_TBL);
            }
        } else if (entrypoint.fn == DO_HINTS && entrypoint.found_at_address != 0) {
            start = find_16_bit_values_in_pattern({ 0xf3, 0x3f, 0xff, 0xfd, 0xcd, 0x4f, WILDCARD, WILDCARD, WILDCARD, 0xcf, 0x1f, WILDCARD, WILDCARD }, { &hints_table_addr, &hints_table_addr, &hints_table_addr }, entrypoint.found_at_address, 300);
            if (start != -1) {
//                fprintf(stderr, "hints_table_addr = 0x%x\n", hints_table_addr);
                start = find_16_bit_values_in_pattern({ 0xd4, 0x1f, WILDCARD, WILDCARD, 0x02, 0x09, 0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x00 }, { &at.K_HINT_ITEMS, &at.K_HINT_ITEMS }, start, 300);
                if (start != -1) {
//                    fprintf(stderr, "at.K_HINT_ITEMS = 0x%x\n", at.K_HINT_ITEMS);
                    start = find_globals_in_pattern({ 0xf7, 0xab, WILDCARD, 0x09, 0x00, 0x04, 0xc2 }, { &hint_chapter_global_idx }, start, 100);
                    if (start != -1) {
//                        fprintf(stderr, "hint_chapter_global_idx = 0x%x\n", hint_chapter_global_idx);
                        start = find_globals_in_pattern({ 0x2d, 0x04, 0x07, 0xda, 0x2f, WILDCARD, WILDCARD, 0x04, 0x0d, WILDCARD, 0x01 }, { &hint_quest_global_idx, &hint_quest_global_idx, &hint_quest_global_idx }, start, 200);
                        if (start != -1) {
//                            fprintf(stderr, "hint_quest_global_idx = 0x%x\n", hint_quest_global_idx);
                        }
                    }
                }
            }
        } else if (entrypoint.fn == DISPLAY_HINT && entrypoint.found_at_address != 0) {
            start = find_16_bit_values_in_pattern({ 0x01, 0x00, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x00, 0x04 }, { &seen_hints_table_addr }, entrypoint.found_at_address, 300);
            if (start != -1) {
//                fprintf(stderr, "seen_hints_table_addr = 0x%x\n", seen_hints_table_addr);
            }
        } else if (entrypoint.fn == RT_SEE_QST && entrypoint.found_at_address != 0) {
            ar.RT_SEE_QST = entrypoint.found_at_address - 1;
            entrypoint.found_at_address = 0;
        }
    }
    patch_arthur_pauses();
}

static void find_journey_globals(void) {
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
            offset = find_pattern_in_mem({ 0xed, 0x3f, 0xff, 0xff, 0xba }, offset, 13000);

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
            jg.UPDATE_FLAG = byte(entrypoint.found_at_address + 7) - 0x10;
            jr.FILL_CHARACTER_TBL = unpack_routine(word(entrypoint.found_at_address + 13));

            jg.PARTY = byte(entrypoint.found_at_address + 17) - 0x10;

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
            for (int i = offset; i < memory_size - 12; i++) {
                if (memory[i] == 0xb0) {
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
            store_byte(offset + 3, 0xab);
            store_byte(offset + 4, 0x01);

            find_routines_in_pattern({ 0xda, 0x2f, WILDCARD, WILDCARD, 0x02, 0x2d }, { &jr.MASSAGE_ELVISH }, offset + 1, 300);

            offset = find_globals_in_pattern({ 0xe2, 0x97, WILDCARD, 0x00, 0x14, 0xe2, 0x97, WILDCARD, 0x00, 0x32 }, { &jg.E_LEXV, &jg.E_INBUF }, entrypoint.found_at_address, 100);
            offset = find_globals_in_pattern({ 0x04, 0x05, 0x2d, 0x06, WILDCARD }, { &jg.E_TEMP }, offset, 200);

            offset = find_globals_in_pattern({ 0x2d, WILDCARD, 0x02 }, { &jg.E_TEMP_LEN }, offset, 200);

            uint8_t praxix = 0, bergon1, bergon2;
            offset = find_values_in_pattern({ 0xc1, 0x93, 0x01, WILDCARD, WILDCARD, WILDCARD, 0xc1 }, { &praxix, &bergon1, &bergon2 }, offset, 200);

            jo.PRAXIX = praxix;
            jo.BERGON = word(offset - 1);
            find_routines_in_pattern({ 0xc1, 0x8f, WILDCARD, WILDCARD, 0xb0, 0xc1 }, { &jr.PARSE_ELVISH }, offset, 200);
        } else if (entrypoint.fn == CHANGE_NAME && entrypoint.found_at_address != 0) {
            uint32_t offset = find_pattern_in_mem({ 0x2d, 0x04, WILDCARD, 0x8f }, entrypoint.found_at_address - 0x10, 1);
            if (offset != -1) {
                // Hack for later versions
                store_byte(offset, 0xb0);
            }
            uint8_t tag = 0, dummy;
            offset = find_values_in_pattern({ WILDCARD, 0x00, 0x74, WILDCARD, 0x00, 0x00 }, { &tag, &dummy }, entrypoint.found_at_address, 200);
            jo.TAG = tag;

            offset = find_routines_in_pattern({ 0xd9, 0x2f, WILDCARD, WILDCARD, 0x01, 0x00 }, { &jr.ILLEGAL_NAME }, offset, 200);
            offset = find_routines_in_pattern({ 0x8f, WILDCARD, WILDCARD, 0xfc, 0xa6 }, { &jr.REMOVE_TRAVEL_COMMAND }, offset, 200);
            offset = find_globals_in_pattern({ 0xfc, 0xa6, WILDCARD, 0x01, 0x02, WILDCARD }, { &jg.NAME_TBL, &jg.TAG_NAME }, offset - 3, 200);

            offset = find_values_in_pattern({ 0xe3, 0x1b, WILDCARD, WILDCARD, WILDCARD, 0x08 }, { &ja.KBD, &ja.KBD, &ja.KBD }, offset, 200);

            jo.TAG_OBJECT = word(offset - 2);

            offset = find_globals_in_pattern({ 0x08, 0x2d, WILDCARD, 0x01, }, { &jg.TAG_NAME_LENGTH }, offset, 200);

            // Change hyphens to underscores in CHANGE-NAME
            offset = find_pattern_in_mem({ 0xe5, 0x7f, 0x2d }, offset, 300);
            store_byte(offset + 2, 0x5f);
            offset = find_pattern_in_mem({ 0xe5, 0x7f, 0x2d }, offset + 3, 300);
            store_byte(offset + 2, 0x5f);

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
            store_byte(entrypoint.found_at_address, 0xab);
            store_byte(entrypoint.found_at_address + 1, 0x01);
            // Patch out repeated words when listing essences and reagents in later version when screen is narrow
        }  else if (entrypoint.fn == TELL_AMOUNTS && entrypoint.found_at_address != 0 && header.release >= 51) {
            uint32_t offset = entrypoint.found_at_address;
            store_byte(offset + 4, 0xb4);
            store_byte(offset + 5, 0xb4);
            store_byte(offset + 6, 0xb4);
        }
    }
}

static void find_shogun_globals(void) {
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
        memory[start + 1] = 1;
        memory[start - 2] = 1;
    }

    for (auto &entrypoint : entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x2d, 0x02, WILDCARD, 0x2d, 0x03, WILDCARD }, { &fg_global_idx, &bg_global_idx }, entrypoint.found_at_address, 300);
            if (start == -1) {
                start = find_globals_in_pattern({ 0x0d, WILDCARD, 0x02, 0x0d, WILDCARD, 0x09 }, { &bg_global_idx, &fg_global_idx }, entrypoint.found_at_address, 300);

            }

            if (start != -1) {
//                fprintf(stderr, "Global index of fg: 0x%x Global index of bg: 0x%x\n", fg_global_idx, bg_global_idx);
                int found = find_pattern_in_mem({ 0xb8 }, entrypoint.found_at_address, 300);

                if (found == -1) {
                    found = find_pattern_in_mem({ 0xb0 }, start, 200);
                }
                if (found != -1) {
                    end_of_color_addr = found;
//                    fprintf(stderr, "Found return from routine V_COLOR at address 0x%x\n", end_of_color_addr);
                } else {
                    fprintf(stderr, "Could not find return from routine V_COLOR!\n");
                }
            } else {
                fprintf(stderr, "Error! Could not find color globals!\n");
            }
            entrypoint.found_at_address = 0; // V_COLOR
        } else if (entrypoint.fn == after_V_COLOR && end_of_color_addr != 0) {
            entrypoint.found_at_address = end_of_color_addr;
//            fprintf(stderr, "after_V_COLOR at address 0x%x\n", entrypoint.found_at_address);
        } else if (entrypoint.fn == MAC_II && entrypoint.found_at_address != 0) {
            start = entrypoint.found_at_address;
            for (int i = start; i < memory_size - 12; i++) {
                if (memory[i] == 0xb1) {
                    mac_ii_return_address = i - 6;
                    break;
                }
            }
        } else if (entrypoint.fn == after_MAC_II && mac_ii_return_address != 0) {
            entrypoint.found_at_address = mac_ii_return_address;
//            fprintf(stderr, "after_MAC_II at: 0x%x\n", entrypoint.found_at_address);
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
            for (int i = 0; i < patch.size(); i++) {
//                fprintf(stderr, "Patching 0x%x at address 0x%x (previous value 0x%x)\n", patch[i], start + i, memory[start + i]);
                memory[start + i] = patch[i];
            }
            menu_return_address =  start + patch.size() - 2;

        } else if (entrypoint.fn == after_GET_FROM_MENU && menu_return_address != 0) {
            entrypoint.found_at_address = menu_return_address;
        } else if ((entrypoint.fn == V_VERSION || entrypoint.fn == V_CREDITS) && entrypoint.found_at_address != 0) {

            if (entrypoint.fn == V_CREDITS) {
                start = find_pattern_in_mem({0xb0}, entrypoint.found_at_address, 100);
                if (start != -1)
                    credits_return_address = start;
            }

            start = find_pattern_in_mem({ 0xf1, 0x7f, 0x02 }, entrypoint.found_at_address, 100);
            if (start != -1) {
                // Patch <HLIGHT ,H-BOLD> to bold fixed-width, which we have
                // mapped to style_User1, (set to bold, centered text) which
                // matches the style of the original centered "THE END" text.
                memory[start + 2] = 0x0a;
                start = find_pattern_in_mem({ 0xf1, 0x7f, 0x00 }, start, 100);
                if (start != -1) {
                    memory[start + 2] = 0x0e;
                } else {
                    fprintf(stderr, "Error 2!\n");
                }
            } else {
                fprintf(stderr, "Error!\n");
            }

        } else if (entrypoint.fn == after_V_CREDITS && credits_return_address != 0) {
            entrypoint.found_at_address = credits_return_address;
        } else if (entrypoint.fn == DO_HINTS && entrypoint.found_at_address != 0) {
            start = find_16_bit_values_in_pattern({ 0xf3, 0x3f, 0xff, 0xfd, 0xcd, 0x4f, WILDCARD, WILDCARD, WILDCARD, 0xcf, 0x1f, WILDCARD, WILDCARD }, { &hints_table_addr, &hints_table_addr, &hints_table_addr }, entrypoint.found_at_address, 300);
            if (start != -1) {
//                fprintf(stderr, "hints_table_addr = 0x%x\n", hints_table_addr);
            }

        } else if (entrypoint.fn == DISPLAY_HINT && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x0b, 0x54, WILDCARD, 0x01, 0x00 }, { &hint_quest_global_idx }, entrypoint.found_at_address, 300);
            if (start != -1) {
//                fprintf(stderr, "hint_quest_global_idx = 0x%x\n", hint_quest_global_idx);
                start = find_globals_in_pattern({ 0x01, 0x55, WILDCARD, 0x01, 0x00 }, { &hint_chapter_global_idx }, start, 200);
                if (start != -1) {
//                    fprintf(stderr, "hint_chapter_global_idx = 0x%x\n", hint_chapter_global_idx);
                    start = find_16_bit_values_in_pattern({ 0x01, 0x00, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x00, 0x04 }, { &seen_hints_table_addr }, start, 300);
                    if (start != -1) {
//                        fprintf(stderr, "seen_hints_table_addr = 0x%x\n", seen_hints_table_addr);
                    }
                } else {
                    fprintf(stderr, "Error! Could not find hint_chapter_global_idx!\n");
                }
            } else {
                fprintf(stderr, "Error! Could not fin hint_quest_global_idx!\n");
            }
            entrypoint.found_at_address = 0; // DISPLAY_HINT
        } else if (entrypoint.fn == V_DEFINE && entrypoint.found_at_address != 0) {

            start = find_16_bit_values_in_pattern({ 0xd4, 0x2f, WILDCARD, WILDCARD, 0x00, 0x06 }, { &fkeys_table_addr }, entrypoint.found_at_address, 300);

            if (start != -1) {
//                fprintf(stderr, "fkeys_table_addr = 0x%x\n", fkeys_table_addr);
                start = find_16_bit_values_in_pattern({ 0xf7, 0x8b, 0x0b, WILDCARD, WILDCARD, 0x00, 0x08, 0x66 }, { &fnames_table_addr }, start, 1100);
                if (start != -1) {
//                    fprintf(stderr, "fnames_table_addr = 0x%x\n", fnames_table_addr);
                }
            } else {
                fprintf(stderr, "Error! Could not find fkeys_table_addr!\n");
            }
        } else if (entrypoint.fn == shogun_UPDATE_STATUS_LINE && entrypoint.found_at_address != 0) {

            uint8_t dummy;
            start = find_globals_in_pattern({ 0x47, 0x00, 0x04, 0xc6, 0x61, WILDCARD, WILDCARD, 0xc5, 0x0d, 0x01, 0x01, 0xa0, 0x01, 0x4a, 0x61, WILDCARD, WILDCARD, 0x46, 0x61, WILDCARD, WILDCARD, 0xc0 }, { &sg.HERE, &dummy, &sg.SCORE, &dummy, &sg.MOVES, &dummy }, entrypoint.found_at_address, 100);

            start = find_pattern_in_mem({ 0xef, WILDCARD, 0x01, WILDCARD, 0xcf, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0x00 }, start, 200);

            if (start != -1) {
                sg.SCENE = memory[start + 8] - 0x10;
                st.SCENE_NAMES = (memory[start + 6] << 8) | memory[start + 7];

//                fprintf(stderr, "sg.HERE = 0x%x sg.SCORE = 0x%x sg.MOVES = 0x%x sg.SCENE = 0x%x st.SCENE_NAMES = 0x%x\n", sg.HERE, sg.SCORE, sg.MOVES, sg.SCENE, st.SCENE_NAMES);

                start = find_globals_in_pattern({ 0xc1, 0x97, WILDCARD, 0x01, 0x03, 0xe1 }, { &sg.MACHINE }, start, 300);

                start = find_globals_in_pattern({ 0xaa, 0x1a, 0xa3, WILDCARD, 0x03, 0x4a, 0x03, 0x2c, 0x5b }, { &sg.WINNER }, start, 300);


                if (start != -1) {
//                    fprintf(stderr, "sg.MACHINE = 0x%x sg.WINNER = 0x%x\n", sg.MACHINE, sg.WINNER);

                    start = find_pattern_in_mem({ 0xda, 0x2f, WILDCARD, WILDCARD, 0x03, 0xc1, 0x97, 0x1a }, start, 200);
                    sr.TELL_THE =
                    unpack_routine(word(start + 2));
//                    fprintf(stderr, "sr.TELL_THE = 0x%x\n", sr.TELL_THE);

                    start = find_values_in_pattern({0x41, 0x1a, WILDCARD, 0x48 }, { &so.BRIDGE_OF_ERASMUS }, start, 200);

                    start = find_16_bit_values_in_pattern({ 0xca, 0x1f, WILDCARD, WILDCARD, 0x13, 0xcc }, { &so.WHEEL }, start, 200);

                    start = find_16_bit_values_in_pattern({ 0xca, 0x1f, WILDCARD, WILDCARD, 0x13, 0x5c }, { &so.GALLEY_WHEEL }, start, 200);

                    so.GALLEY = memory[start - 4];
//                    fprintf(stderr, "so.BRIDGE_OF_ERASMUS = 0x%x so.GALLEY = 0x%x so.WHEEL == 0x%x so.GALLEY_WHEEL = 0x%x\n", so.BRIDGE_OF_ERASMUS, so.GALLEY, so.WHEEL, so.GALLEY_WHEEL);

                    start = find_pattern_in_mem({ 0xda, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0xa0, 0x01, 0x46, 0x61 }, start, 200);
                    sr.TELL_DIRECTION = unpack_routine(word(start + 2));
                    sg.SHIP_COURSE = memory[start + 4] - 0x10;
                    sg.SHIP_DIRECTION = memory[start - 8] - 0x10;
//                    fprintf(stderr, "sr.TELL_DIRECTION = 0x%x sg.SHIP_COURSE = 0x%x sg.SHIP_DIRECTION = 0x%x\n", sr.TELL_DIRECTION, sg.SHIP_COURSE, sg.SHIP_DIRECTION);
                } else {
                    fprintf(stderr, "Error! Could not find sr.TELL_THE!\n");
                }
            } else {
                fprintf(stderr, "Error! Could not find sg.SCENE!\n");
            }
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
//                fprintf(stderr, "sm.YOU_MAY_CHOOSE = 0x%x sm.PART_MENU = 0x%x sm.SCENE_SELECT_F = 0x%x\n", sm.YOU_MAY_CHOOSE, sm.PART_MENU, sm.SCENE_SELECT_F);
                found_scene_select_values = true;

                start = find_globals_in_pattern({ 0x55, 0x01, 0x01, 0x00, 0x76, 0x00, WILDCARD, 0x00, 0x54, 0x00, 0x01, 0x00, 0xb8 }, { &sg.FONT_X }, start, 2000);
                if (start != -1) {
                    start = find_globals_in_pattern({ 0x55, 0x01, 0x01, 0x00, 0x76, 0x00, WILDCARD, 0x00, 0x54, 0x00, 0x01, 0x00, 0xb8 }, { &sg.FONT_Y }, start, 100);
                    if (start != -1) {
//                        fprintf(stderr, "FONT_X: 0x%x FONT_Y: 0x%x\n", sg.FONT_X, sg.FONT_Y);
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
                for (int i = 0; i < patch.size(); i++) {
//                    fprintf(stderr, "Patching 0x%x at address 0x%x (previous value 0x%x)\n", patch[i], start + i, memory[start + i]);
                    memory[start + i] = patch[i];
                }
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

            // RET L00
            memory[entrypoint.found_at_address] = 0xab;
            memory[entrypoint.found_at_address + 1] = 0x01;

            start = find_globals_in_pattern({ 0x76, WILDCARD, 0x06, 0x00 }, { &sg.MAZE_Y }, entrypoint.found_at_address, 300);
            if (start != -1) {
                start = find_globals_in_pattern({ 0x76, WILDCARD, 0x05, 0x00 }, { &sg.MAZE_X }, start + 1, 300);
//                fprintf(stderr, "sg.MAZE_X = 0x%x sg.MAZE_Y = 0x%x\n", sg.MAZE_X, sg.MAZE_Y);

                start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x9f }, { &so.EAST }, start + 1, 300);
                start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x8f }, { &so.NORTH }, start + 1, 300);

                start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x67 }, { &so.SOUTH }, start + 1, 300);
                start = find_16_bit_values_in_pattern({ 0x4a, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x8c, 0x00, 0x43 }, { &so.WEST }, start + 1, 300);

//                fprintf(stderr, "so.NORTH = 0x%x so.EAST = 0x%x so.SOUTH = 0x%x so.WEST = 0x%x\n", so.NORTH, so.EAST, so.SOUTH, so.WEST);

                uint16_t fnc = 0;
                start = find_16_bit_values_in_pattern({ 0xf9, 0x27, WILDCARD, WILDCARD, 0x02, 0x0d }, { &fnc }, start + 1, 300);
                if (start != -1) {
                    sr.ADD_TO_INPUT = unpack_routine(fnc);
//                    fprintf(stderr, "sr.ADD_TO_INPUT = 0x%x\n", sr.ADD_TO_INPUT);

                    start = find_globals_in_pattern({ 0x70, WILDCARD, 0x00, 0x00 }, { &sg.MAZE_MAP }, start, 300);

                    if (start != -1) {
//                        fprintf(stderr, "sg.MAZE_MAP = 0x%x\n", sg.MAZE_MAP);
                    } else {
                        fprintf(stderr, "Could not find sg.MAZE_MAP!\n");
                    }
                }
            } else {
                fprintf(stderr, "Could not find sg.MAZE_MAP!\n");
            }
        } else if (entrypoint.fn == DISPLAY_MAZE && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xbe, 0x05, 0x57, 0x2c, 0x01, 0x01, 0x62, 0x03, WILDCARD, 0x41, 0x0d, 0x04, 0x00, 0x62, 0x04, WILDCARD, 0x58 }, { &sg.MAZE_HEIGHT, &sg.MAZE_WIDTH }, entrypoint.found_at_address, 300);

            if (start == -1)
                fprintf(stderr, "Did not find MAZE_HEIGHT or MAZE_WIDTH!\n");
//            fprintf(stderr, "sg.MAZE_HEIGHT = 0x%x sg.MAZE_WIDTH = 0x%x\n", sg.MAZE_HEIGHT, sg.MAZE_WIDTH);
        } else if (entrypoint.fn == MAZE_F && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xbb, 0x8c, 0x00, 0x17, 0x2d, 0x02, WILDCARD, 0x2d, 0x03, WILDCARD, 0xb2 }, { &sg.MAZE_XSTART, &sg.MAZE_YSTART }, entrypoint.found_at_address, 400);
            if (start == -1)
                fprintf(stderr, "Did not find MAZE_XSTART or MAZE_YSTART!\n");
//            fprintf(stderr, "sg.MAZE_XSTART = 0x%x sg.MAZE_YSTART = 0x%x\n", sg.MAZE_XSTART, sg.MAZE_YSTART);
            entrypoint.found_at_address = 0; // MAZE_F
        } else if (entrypoint.fn == shogun_DISPLAY_BORDER && entrypoint.found_at_address != 0 && !found_border_values) {
            start = find_globals_in_pattern({ 0x2d, 0x01, WILDCARD }, { &sg.CURRENT_BORDER }, entrypoint.found_at_address - 27, 100);
            if (start == -1) {
                fprintf(stderr, "Could not find sg.CURRENT_BORDER. Setting it to 0.\n");
                sg.CURRENT_BORDER = 0;
            } else {
//                fprintf(stderr, "sg.CURRENT_BORDER = 0x%x\n", sg.CURRENT_BORDER);
            }
            start = entrypoint.found_at_address + 10;
            if (byte(start) == 0xbe)
                store_byte(start, 0xb0);
            found_border_values = true;
        } else if (entrypoint.fn == V_BOW && entrypoint.found_at_address != 0) {
            // Versions 283 through 295 (a total of 9 versions) assign values to the variable that
            // is referenced by variable 1, rather than to variable directly, as later versions do.
            // This causes a crash when variable 1 is 0, which is usually (always?) the case.
            // We fix this by replacing the dereference with a direct assignment, as per the
            // later versions. This seems to have no ill side-effects.

            start = find_pattern_in_mem({ 0x6d, 0x01, WILDCARD, 0x8c, 0x00 }, entrypoint.found_at_address, 100);
            if (start != -1 && memory[start] == 0x6d) {
                memory[start] = 0x2d;
                fprintf(stderr, "Patched V-BOW offset 0x%x\n", start);
                if (memory[start + 13] == 0x6d) {
                    fprintf(stderr, "Patched V-BOW offset 0x%x\n", start + 9);
                    memory[start + 13] = 0x2d;
                }
            }
            entrypoint.found_at_address = 0; // V_BOW
        }
    }
}

static std::unordered_map<uint32_t, EntryPoint *> entrypoint_map;

static uint32_t lowest_entrypoint = UINT32_MAX;
static uint32_t highest_entrypoint = 0;

void find_entrypoints(void) {

    int start = header.static_start;
    int end = memory_size - 12;

    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game)) {
//            fprintf(stderr, "Looking for entrypoint %s (starting at 0x%x)\n", entrypoint.title.c_str(), start);
            if (entrypoint.pattern.size()) {
                int32_t offset = find_pattern_in_mem(entrypoint.pattern, start, end - start);
                if (offset != -1) {
                    entrypoint.found_at_address = offset + entrypoint.offset;
                    start = entrypoint.found_at_address;
//                    fprintf(stderr, "Found routine %s at offset 0x%04x\n", entrypoint.title.c_str(), start);
                    if (entrypoint.stub_original) {
                        // Overwrite original byte with rtrue;
                        store_byte(entrypoint.found_at_address, 0xb0);
                    }
//                } else {
//                    fprintf(stderr, "Did not find it!\n");
                }
//            } else {
//                fprintf(stderr, "Did not find it! (No pattern)\n");
            }
        }
    }

    if (is_spatterlight_arthur) {
        find_arthur_globals();
    } else if (is_spatterlight_journey) {
        find_journey_globals();
    } else if (is_spatterlight_shogun) {
        find_shogun_globals();
    }

    for (auto &entrypoint : entrypoints) {
        if (entrypoint.found_at_address != 0) {
            entrypoint_map[entrypoint.found_at_address] = &entrypoint;
            if (entrypoint.found_at_address < lowest_entrypoint)
                lowest_entrypoint = entrypoint.found_at_address;
            if (entrypoint.found_at_address > highest_entrypoint)
                highest_entrypoint = entrypoint.found_at_address;
        }
    }
}

void check_entrypoints(uint32_t pc) {
    if (pc > highest_entrypoint || pc < lowest_entrypoint)
        return;
    auto found = entrypoint_map.find(pc);

    if (found != entrypoint_map.end()) {
        EntryPoint *entrypoint = entrypoint_map.at(pc);
//        fprintf(stderr, "Found entrypoint %s at 0x%x\n", entrypoint->title.c_str(), pc);
        (entrypoint->fn)();
   }
}
