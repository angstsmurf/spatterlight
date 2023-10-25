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

#include "entrypoints.hpp"

uint8_t fg_global_idx = 0, bg_global_idx = 0;
uint32_t update_status_bar_address = 0;
uint32_t update_picture_address = 0;
uint32_t update_inventory_address = 0;
uint32_t update_stats_address = 0;
uint32_t update_map_address = 0;
uint32_t init_screen_address = 0;
uint32_t refresh_screen_address = 0;

uint8_t update_global_idx = 0;
uint8_t global_map_grid_y_idx = 0;
uint8_t global_text_window_left_idx = 0;

struct EntryPoint {
    Game game;
    std::string title;
    std::vector<uint8_t> pattern;
    int offset;
    uint32_t found_at_address;
    void (*fn)();
};

static std::vector<EntryPoint> entrypoints = {

#pragma mark Zork Zero

    {
        Game::ZorkZero,
        "after SPLIT-BY-PICTURE",
        { 0x11, 0x6B, 0x01, 0x03, 0x00, 0xB0 },
        5,
        0,
        after_SPLIT_BY_PICTURE
    },

    {
        Game::ZorkZero,
        "INIT-HINT-SCREEN",
        { 0xED, 0x3F, 0xFF, 0xFF, 0xEB, 0x7F, 0x07, 0xA0 },
        0,
        0,
        INIT_HINT_SCREEN
    },

    {
        Game::ZorkZero,
        "V-MODE",
        { 0x00,0xED, 0x3F, 0xFF, 0xFF, 0xA0 },
        1,
        0,
        V_MODE
    },

//    {
//        Game::ZorkZero,
//        "INIT-HINT-SCREEN",
//        { 0xF1, 0x7F, 0x00, 0xEF, 0x1F, 0xFF, 0xFF},
//        0,
//        0,
//        INIT_HINT_SCREEN
//    },



    {
        Game::ZorkZero,
        "LEAVE_HINT_SCREEN",
        { 0x10, 0xE6, 0x22, 0x00},
        -4,
        0,
        LEAVE_HINT_SCREEN
    },

//    B310E622 00
    {
        Game::ZorkZero,
        "V-DEFINE",
        { 0xA0, 0x00, 0xCA, 0xBE, 0x06 },
        0,
        0,
        V_DEFINE
    },

    {
        Game::ZorkZero,
        "V-DEFINE alt",
        { 0xED, 0x3F, 0xFF, 0xFF, 0x36, 0x04, 0x01, 0x00 },
        0,
        0,
        V_DEFINE
    },



    {
        Game::ZorkZero,
        "V_REFRESH",
        { 0x00, 0x00, 0x46, 0x4F, 0x05, 0x04, 0x04, 0x2D },
        14,
        0,
        V_REFRESH
    },

    {
        Game::ZorkZero,
        "DISPLAY-HINT",
        { 0xF1, 0x7F, 0x00, 0xED, 0x7F, 0x00, 0xEB },
        0,
        0,
        DISPLAY_HINT
    },

    {
        Game::ZorkZero,
        "after DISPLAY-HINT",
        { 0xBE, 0x12, 0x17, 0xFF, 0xFD, 0x04, 0x02, 0xBE, 0x12, 0x57},
        0,
        0,
        after_DISPLAY_HINT
    },

    {
        Game::ZorkZero,
        "after DISPLAY-HINT alt",
        { 0xA0, 0x03, 0xBE, 0xC8, 0xB1},
        0,
        0,
        after_DISPLAY_HINT
    },

    {
        Game::ZorkZero,
        "V-CREDITS",
        { 0xF9, 0x07, 0x3A, 0x87, 0x07, 0xE8},
        0,
        0,
        V_CREDITS
    },

    {
        Game::ZorkZero,
        "after V-CREDITS",
        { 0x82, 0x00, 0xB8, 0x00},
        2,
        0,
        after_V_CREDITS
    },

    {
        Game::ZorkZero,
        "after V-CREDITS alt",
        { 0x40, 0x00, 0xB8, 0x00, 0x05},
        2,
        0,
        after_V_CREDITS
    },

    {
        Game::ZorkZero,
        "CENTER-1",
        { 0x87, 0x00, 0x03, 0xbe, 0x13, 0x5F, 0x00, 0x03, 0x00, 0x57, 0x00, 0x02},
        -3,
        0,
        CENTER
    },

    {
        Game::ZorkZero,
        "CENTER-1 alt",
        { 0x00, 0xBB, 0xB0, 0x00, 0x00},
        -6,
        0,
        CENTER
    },

    {
        Game::ZorkZero,
        "CENTER-2",
        { 0x06, 0xF0, 0x3F},
        1,
        0,
        CENTER
    },


    {
        Game::ZorkZero,
        "CENTER-3",
        { 0x08, 0xF0, 0x3F },
        1,
        0,
        CENTER
    },

    {
        Game::ZorkZero,
        "V-COLOR",
        { 0x80, 0xA5, 0xCF, 0x2F},
        2,
        0,
        V_COLOR
    },

    {
        Game::ZorkZero,
        "after V-COLOR",
        {},
        0,
        0,
        after_V_COLOR
    },

#pragma mark Arthur

    {
        Game::Arthur,
        "INIT-HINT-SCREEN",
        { 0xEB, 0x7F, 0x00, 0x9B, 0x01, 0x00, 0x00, 0x9F },
        -9,
        0,
        INIT_HINT_SCREEN
    },

    {
        Game::Arthur,
        "LEAVE_HINT_SCREEN",
        { 0x03, 0xFF, 0x7F, 0x03, 0xC5, 0x0D, 0x03, 0x01, 0xeb  },
        -3,
        0,
        LEAVE_HINT_SCREEN
    },

    {
        Game::Arthur,
        "DISPLAY-HINT",
        { 0xF1, 0x7F, 0x00, 0xED, 0x7F, 0x00, 0xEB },
        0,
        0,
        DISPLAY_HINT
    },

    {
        Game::Arthur,
        "after DISPLAY-HINT",
        { 0xBE, 0x12, 0x17, 0xFF, 0xFD, 0x04, 0x02, 0xBE, 0x12, 0x57},
        0,
        0,
        after_DISPLAY_HINT
    },

//    {
//        Game::Arthur,
//        "V-COLOR",
//        { 0x80, 0xA5, 0xCF, 0x2F},
//        2,
//        0,
//        V_COLOR
//    },

    {
        Game::Arthur,
        "V-COLOR",
        { 0xB2, 0x10, 0xCA},
        0,
        0,
        V_COLOR
    },

    {
        Game::Arthur,
        "after V-COLOR",
        {},
        0,
        0,
        after_V_COLOR
    },

    {
        Game::Arthur,
        "UPDATE-STATUS-LINE",
        { 0xeb, 0x7f, 0x01, 0xf1, 0x7f, 0x01 },
        0,
        0,
        UPDATE_STATUS_LINE
    },

    {
        Game::Arthur,
        "RT-UPDATE-PICT-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0x41 },
        0,
        0,
        RT_UPDATE_PICT_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-INVT-WINDOW",
        { 0xeb, 0x7f, 0x02, 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0xbe },
        0,
        0,
        RT_UPDATE_INVT_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-STAT-WINDOW",
        { 0xbe, 0x12, 0x57, 0x02, 0x01, 0x02, 0xa0 },
        0,
        0,
        RT_UPDATE_STAT_WINDOW
    },

    {
        Game::Arthur,
        "RT-UPDATE-MAP-WINDOW",
        { 0xEB, 0x7F, 0x02, 0xBE, 0x12, 0x57, 0x02, 0x01, 0x02, 0xA0 },
        -0x16,
        0,
        RT_UPDATE_MAP_WINDOW
    },

#pragma mark Journey

//    {
//        Game::Journey,
//        "INTRO",
//        { 0xF1, 0x7F, 0x00, 0xBB},
//        -6,
//        0,
//        INTRO
//    },

    {
        Game::Journey,
        "INTRO alt",
        { 0xF1, 0x7F, 0x00, 0xBB},
        -5,
        0,
        INTRO
    },

    {
        Game::Journey,
        "after TITLE PAGE",
        { 0xf6, 0x7f, 0x01, 0x00, 0xa0, 0x3f, 0xc7},
        4,
        0,
        after_TITLE_PAGE
    },

    {
        Game::Journey,
        "after TITLE PAGE alt",
        { 0xf6, 0x7f, 0x01, 0x00, 0xa0, 0x41, 0xc5},
        4,
        0,
        after_TITLE_PAGE
    },

    {
        Game::Journey,
        "after TITLE PAGE alt 2",
        { 0xf6, 0x7f, 0x01, 0x00, 0xbe },
        4,
        0,
        after_TITLE_PAGE
    },

    {
        Game::Journey,
        "after INTRO",
        {},
        0,
        0,
        after_INTRO
    },

    {
        Game::Journey,
        "REFRESH-SCREEN",
        { 0xff, 0x7f, 0x01, 0xc5, 0x0d, 0x01, 0x01, 0xa0 },
        0,
        0,
        REFRESH_SCREEN
    },

    {
        Game::Journey,
        "INIT-SCREEN",
        { 0x41, 0x8f, 0x06, 0x51 },
        0,
        0,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "INIT-SCREEN alt",
        { 0x0f, 0x00, 0x11, 0x00, 0x77, 0x00, 0x9f },
        0,
        0,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "INIT-SCREEN alt 2",
        { 0x03, 0x4b, 0x0d},
        -2,
        0,
        INIT_SCREEN
    },

    {
        Game::Journey,
        "DIVIDER",
        { 0xBB, 0xBB, 0xF0, 0x3F},
        -10,
        0,
        DIVIDER
    },

    {
        Game::Journey,
        "BOLD-CURSOR",
        { 0xda, 0x1f, 0x01, 0x17, 0x01, 0x55},
        0,
        0,
        BOLD_CURSOR
    },

    {
        Game::Journey,
        "BOLD-PARTY-CURSOR",
        { 0xda, 0x1f, 0x01, 0x17, 0x01, 0x2d},
        0,
        0,
        BOLD_PARTY_CURSOR
    },

#pragma mark Shogun

    {
        Game::Shogun,
        "V-VERSION",
        { 0x0d, 0x02, 0x12, 0xa0, 0x01 },
        0,
        0,
        V_VERSION
    },
    {
        Game::Shogun,
        "after V-VERSION",
        { 0xBF, 0x00, 0xA0, 0x01, 0xC5, 0x8F },
        9,
        0,
        after_V_VERSION
    },

    //    {
    //        Game::Shogun,
    //        "SETUP-TEXT-AND-STATUS",
    //        { 0xff, 0x7f, 0x01, 0xc5 , 0x0d, 0x01, 0x02, 0x0f },
    //        0,
    //        0,
    //        after_SETUP_TEXT_AND_STATUS
    //    },

    {
        Game::Shogun,
        "After SETUP-TEXT-AND-STATUS",
        { 0x05, 0x00, 0x04, 0x00, 0xb8 },
        4,
        0,
        after_SETUP_TEXT_AND_STATUS
    },

    {
        Game::Shogun,
        "SCENE-SELECT",
        { 0x06, 0xBE, 0x13, 0x5F, 0x07, 0x02 },
        1,
        0,
        SCENE_SELECT
    },

    {
        Game::Shogun,
        "V-CREDITS",
        { 0xF1, 0x7F, 0x02, 0xDA, 0x0F },
        0,
        0,
        V_CREDITS
    },

    {
        Game::Shogun,
        "after V-CREDITS",
        { 0xB0, 0x00, 0xC1, 0x8F },
        0,
        0,
        after_V_CREDITS
    },

    {
        Game::Shogun,
        "GOTO-SCENE",
        { 0xFF, 0x7F, 0x02, 0xC5, 0x2D, 0x02 },
        0,
        0,
        GOTO_SCENE
    },


    {
        Game::Shogun,
        "INIT-HINT-SCREEN",
        { 0xEB, 0x7F, 0x07, 0xED, 0x7F},
        0,
        0,
        INIT_HINT_SCREEN
    },

    {
        Game::Shogun,
        "after V-HINT",
        { 0x11, 0x86, 0x21, 0xE0 , 0x04, 0x61},
        -1,
        0,
        after_V_HINT
    },

    {
        Game::Shogun,
        "DISPLAY-HINT",
        { 0x4F, 0x01, 0x00, 0x02, 0xF3 },
        0,
        0,
        DISPLAY_HINT
    },

    {
        Game::Shogun,
        "after DISPLAY-HINT",
        { 0xF5, 0x7F, 0x01, 0x8C, 0xFE, 0x8B, 0x00},
        -5,
        0,
        after_DISPLAY_HINT
    },

    {
        Game::Shogun,
        "after DISPLAY-HINT alt ",
        { 0xB1, 0xF5, 0x7F, 0x01, 0x8C, 0xFE },
        -4,
        0,
        after_DISPLAY_HINT
    },

    {
        Game::Shogun,
        "V-DEFINE",
        { 0xED, 0x3F, 0xFF, 0xFF, 0x36 },
        0,
        0,
        V_DEFINE
    },

    {
        Game::Shogun,
        "after V-DEFINE",
        { 0x04, 0x7F, 0x01, 0x00, 0xEB, 0x7F, 0x00, 0xED, 0x7F, 0x00 },
        -1,
        0,
        after_V_DEFINE
    },


    {
        Game::Shogun,
        "V-COLOR",
        { 0x98, 0x05, 0xCF, 0x2F },
        2,
        0,
        V_COLOR
    },

    {
        Game::Shogun,
        "after V-COLOR",
        {},
        0,
        0,
        after_V_COLOR
    },

    {
        Game::Shogun,
        "MAC-II?",
        { 0xbe, 0x13, 0x5f, 0x07, 0x03, 0x00 },
        0,
        0,
        MAC_II
    },

    {
        Game::Shogun,
        "after MAC-II?",
        {},
        0,
        0,
        after_MAC_II
    },
};

void find_color_globals(void) {
    int start = 0;
    int color_return_address = 0;
    int mac_ii_return_address = 0;
    int intro_return_address = 0;
    for (auto &entrypoint : entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            fg_global_idx = memory[entrypoint.found_at_address - 0xd] - 0x10;
            bg_global_idx = memory[entrypoint.found_at_address + 0x4] - 0x10;
            start = entrypoint.found_at_address + 12;
            fprintf(stderr, "Global index of fg: 0x%x Global index of bg: 0x%x\n", fg_global_idx, bg_global_idx);
            for (int i = start; i < memory_size - 12; i++) {
                if (memory[i] == 0xb8) {
                    color_return_address = i;
                    break;
                }
            }
        } else if (entrypoint.fn == after_V_COLOR && color_return_address != 0) {
            entrypoint.found_at_address = color_return_address;
            fprintf(stderr, "after_V_COLOR at: 0x%x\n", entrypoint.found_at_address);
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
            fprintf(stderr, "after_MAC_II at: 0x%x\n", entrypoint.found_at_address);
        } else if (entrypoint.fn == UPDATE_STATUS_LINE && entrypoint.found_at_address != 0) {
            update_status_bar_address = entrypoint.found_at_address - 1;
            update_global_idx = memory[update_status_bar_address + 0xd] - 0x10;
        } else if (entrypoint.fn == RT_UPDATE_PICT_WINDOW && entrypoint.found_at_address != 0) {
            update_picture_address = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_UPDATE_INVT_WINDOW && entrypoint.found_at_address != 0) {
            update_inventory_address = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_UPDATE_STAT_WINDOW && entrypoint.found_at_address != 0) {
            update_stats_address = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == RT_UPDATE_MAP_WINDOW && entrypoint.found_at_address != 0) {
            update_map_address = entrypoint.found_at_address - 1;
            global_map_grid_y_idx = memory[update_map_address + 2] - 0x10;
        } else if (entrypoint.fn == INIT_SCREEN && entrypoint.found_at_address != 0) {
            init_screen_address = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == REFRESH_SCREEN && entrypoint.found_at_address != 0) {
            refresh_screen_address = entrypoint.found_at_address - 1;
        } else if (entrypoint.fn == DIVIDER && entrypoint.found_at_address != 0) {
            global_text_window_left_idx = memory[entrypoint.found_at_address + 31] - 0x10;
        } else if (entrypoint.fn == INTRO && entrypoint.found_at_address != 0) {
            start = entrypoint.found_at_address;
            for (int i = start; i < memory_size - 12; i++) {
                if (memory[i] == 0xb0) {
                    intro_return_address = i;
                    break;
                }
            }
        } else if (entrypoint.fn == after_INTRO && intro_return_address != 0) {
            entrypoint.found_at_address = intro_return_address;
            fprintf(stderr, "after_INTRO at: 0x%x\n", entrypoint.found_at_address);
        }
        
    }
}

extern uint8_t *memory;

void find_entrypoints(int start, int end) {
    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game)) {
            for (int i = start; i < end; i++) {
                if (entrypoint.pattern.size() && std::memcmp(&memory[i], entrypoint.pattern.data(), entrypoint.pattern.size()) == 0) {
                    fprintf(stderr, "Found routine %s at offset 0x%04x\n", entrypoint.title.c_str(), i + entrypoint.offset);
                    entrypoint.found_at_address = i + entrypoint.offset;
                    break;
                }
            }
        }
    }

    find_color_globals();
//
//    uint16_t address = find_string_in_dictionary({'c', 'o', 'l', 'o', 'r'});
//    fprintf(stderr, "Dictionary address of \"color\" is %d (%x)\n", address, address);
}


void check_entrypoints(uint32_t pc) {
    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game) && pc == entrypoint.found_at_address) {
            fprintf(stderr, "Found entrypoint %s at pc 0x%04x. Calling function.\n", entrypoint.title.c_str(), entrypoint.found_at_address);
            (entrypoint.fn)();
        }
    }
}

