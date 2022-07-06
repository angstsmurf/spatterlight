//
//  gameinfo.c
//  taylor
//
//  Created by Administrator on 2022-03-23.
//

#include <stdio.h>

#include "taylor.h"

struct GameInfo games[NUMGAMES] = {
    {
        "Questprobe 3",
        QUESTPROBE3,
        QUESTPROBE3_TYPE,                 // type
        QUESTPROBE3,

        66,  // Number of items
        177, // Number of actions
        79,  // Number of words
        93,  // Number of rooms
        133,   // Max carried items
        4,   // Word length
        171,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x3a50 - 0x1B, // actions
        0x45F6,  // dictionary
        0x4c8c, // tokens
        0x28c0 - 0x1B, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        0x2f38 - 0x1B, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2539, // start_of_system_messages
        0x28de, // start of directions

        0x6100, // start_of_characters;
        0x6916, // start_of_image_data;
        0x381c, // image patterns lookup table;
        0x1c, // number of patterns
        0x9f, // patterns end marker
        0x87a6 - 0x1B, // start of room image instructions
        58, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },
    {
        "Questprobe 3 C64",
        QUESTPROBE3_64,
        QUESTPROBE3_TYPE,                 // type
        QUESTPROBE3,

        66,  // Number of items
        177, // Number of actions
        79,  // Number of words
        93,  // Number of rooms
        133,   // Max carried items
        4,   // Word length
        171,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x3a50 - 0x1B, // actions
        0x1187,  // dictionary
        0x4c8c, // tokens
        0x28c0 - 0x1B, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        0x2f38 - 0x1B, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2539, // start_of_system_messages
        0x28de, // start of directions

        0xbb02, // start_of_characters;
        0x6058, // start_of_image_data;
        0x381c, // image patterns lookup table;
        0x1c, // number of patterns
        0x9f, // patterns end marker
        0x87a6 - 0x1B, // start of room image instructions
        58, // number_of_image blocks;
        C64B, // palette
        4, // picture_format_version;
    },

    {
        "Rebel Planet",
        REBEL_PLANET,
        REBEL_PLANET_TYPE,                 // type
        REBEL_PLANET,

        133,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        250,  // Number of rooms
        133,   // Max carried items •
        4,   // Word length
        254,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x83E0 - 0x4000, // actions
        0x7559 - 0x4000,  // dictionary
        0xBBDA - 0x4000, // tokens
        0x9615 - 0x4000, // start_of_room_descriptions;
        0x73A3 - 0x4000, // start_of_room_connections;
        0x9E05 - 0x4000, // start_of_messages;
        0xB321 - 0x4000, // start_of_item_descriptions;
        0x731E - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x810e, // start_of_characters;
        0x9139, // start_of_image_data;
        0, // image patterns lookup table; •
        0, // number of patterns •
        0, // patterns end marker •
        0x87a6, // start of room image instructions
        167, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },

    {
        "Rebel Planet C64",
        REBEL_PLANET_64,
        REBEL_PLANET_TYPE,                 // type
        REBEL_PLANET,

        133,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        250,  // Number of rooms
        133,   // Max carried items •
        4,   // Word length
        254,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x83E0 - 0x4000, // actions
        0x7559 - 0x4000,  // dictionary
        0xBBDA - 0x4000, // tokens
        0x9615 - 0x4000, // start_of_room_descriptions;
        0x73A3 - 0x4000, // start_of_room_connections;
        0x9E05 - 0x4000, // start_of_messages;
        0xB321 - 0x4000, // start_of_item_descriptions;
        0x731E - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x810e, // start_of_characters;
        0x9139, // start_of_image_data;
        0, // image patterns lookup table; •
        0, // number of patterns •
        0, // patterns end marker •
        0x87a6, // start of room image instructions
        167, // number_of_image blocks;
//        0, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },

    {
        "Blizzard Pass",
        BLIZZARD_PASS,
        BLIZZARD_PASS_TYPE,                 // type
        BLIZZARD_PASS,

        150,  // Number of items
        177, // Number of actions
        79,  // Number of words
        107,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        171,  // Number of messages

        80, // number_of_verbs
        79, // number_of_nouns;

        0, // header

        0, // no room images
        0, // no item flags
        0, // no item images

        0x9A54 - 0x4000, // actions
        0xA720 - 0x4000,  // dictionary
        0xB1BC - 0x4000, // tokens
        0x75CE - 0x4000, // start_of_room_descriptions;
        0xAB67 - 0x4000, // start_of_room_connections;
        0x7E26 - 0x4000, // start_of_messages;
        0x8CE6 - 0x4000, // start_of_item_descriptions;
        0xB6A8 - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x8350, // start_of_characters;
        0x8708, // start_of_image_data;
        0, // image patterns lookup table
        0, // number of patterns
        0, // patterns end marker
        0x7798, // start of room image instructions
        114, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },
    {
        "Heman",
        HEMAN,
        HEMAN_TYPE,                 // type
        HEMAN,

        91,  // Number of items
        177, // Number of actions
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        210,  // Number of messages

        80, // number_of_verbs
        79, // number_of_nouns;

        0, // header

        0, // no room images
        0, // no item flags
        0, // no item images

        0x77D8 - 0x4000, // actions
        0x883A - 0x4000,  // dictionary
        0x8CE0 - 0x4000, // tokens
        0x9B36 - 0x4000, // start_of_room_descriptions;
        0x842F - 0x4000, // start_of_room_connections;
        0xA665 - 0x4000, // start_of_messages;
        0x96A2 - 0x4000, // start_of_item_descriptions;
        0x83CB - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x8603, // start_of_characters;
        0x8d13, // start_of_image_data;
        0x3703, // image patterns lookup table;
        0x1c, // number of patterns
        0x9f, // patterns end marker
        0x7adf, // start of room image instructions
        139, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },
    {
        "Heman C64",
        HEMAN_64,
        HEMAN_TYPE,                 // type
        HEMAN,

        91,  // Number of items
        177, // Number of actions
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        210,  // Number of messages

        80, // number_of_verbs
        79, // number_of_nouns;

        0, // header

        0, // no room images
        0, // no item flags
        0, // no item images

        0x77D8 - 0x4000, // actions
        0x883A - 0x4000,  // dictionary
        0x8CE0 - 0x4000, // tokens
        0x9B36 - 0x4000, // start_of_room_descriptions;
        0x842F - 0x4000, // start_of_room_connections;
        0xA665 - 0x4000, // start_of_messages;
        0x96A2 - 0x4000, // start_of_item_descriptions;
        0x83CB - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x8603, // start_of_characters;
        0x8d13, // start_of_image_data;
        0, // image patterns lookup table;
        0, // number of patterns
        0, // patterns end marker
        0x7adf, // start of room image instructions
        139, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },

    {
        "Temple of Terror",
        TEMPLE_OF_TERROR,
        HEMAN_TYPE,                 // type
        TEMPLE_OF_TERROR,

        191,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        210,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x78a4 - 0x4000, // actions
        0x8b9d - 0x4000, // dictionary
        0x90D4 - 0x4000, // tokens
        0x93B3 - 0x4000, // start_of_room_descriptions;
        0x865C - 0x4000, // start_of_room_connections;
        0xA67D - 0x4000, // start_of_messages;
        0x9C4F - 0x4000, // start_of_item_descriptions;
        0x8594 - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0xc3cb - 0x4000, // start_of_characters;
        0xca33 - 0x4000, // start_of_image_blocks;
        0x7837 - 0x4000, // image patterns lookup table;
        0x12, // number of patterns
        0xaa, // patterns end marker
        0xbb75 - 0x4000, // start of room image instructions
        143, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },

    {
        "Temple of Terror text only version",
        TOT_TEXT_ONLY,
        HEMAN_TYPE,                 // type
        TEMPLE_OF_TERROR,

        191,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        210,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x78a4 - 0x4000, // actions
        0x8BA2 - 0x4000, // dictionary
        0x90D9 - 0x4000, // tokens
        0xBA2A - 0x4000, // start_of_room_descriptions;
        0x8661 - 0x4000, // start_of_room_connections;
        0x9FD9 - 0x4000, // start_of_messages;
        0x9419 - 0x4000, // start_of_item_descriptions;
        0x8599 - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0, // start_of_characters;
        0, // start_of_image_blocks;
        0, // image patterns lookup table;
        0, // number of patterns
        0, // patterns end marker
        0, // start of room image instructions
        0, // number_of_image blocks;
        0, // palette
        0, // picture_format_version;
    },

    {
        "Temple of Terror C64",
        TEMPLE_OF_TERROR_64,
        HEMAN_TYPE,                 // type
        TEMPLE_OF_TERROR,

        191,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        210,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x78a4 - 0x4000, // actions
        0x8b9d - 0x4000, // dictionary
        0x90D4 - 0x4000, // tokens
        0x93B3 - 0x4000, // start_of_room_descriptions;
        0x865C - 0x4000, // start_of_room_connections;
        0xA67D - 0x4000, // start_of_messages;
        0x9C4F - 0x4000, // start_of_item_descriptions;
        0x8594 - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0xc3cb - 0x4000, // start_of_characters;
        0xca33 - 0x4000, // start_of_image_blocks;
        0, // image patterns lookup table;
        0, // number of patterns
        0, // patterns end marker
        0xbb75 - 0x4000, // start of room image instructions
        143, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },


    {
        "Kayleth",
        KAYLETH,
        HEMAN_TYPE,                 // type
        KAYLETH,

        123,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        254,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x7A48 - 0x4000, // actions
        0x9323 - 0x4000,  // dictionary
        0xBFEE - 0x4000,  // tokens
        0xAEB3 - 0x4000, // start_of_room_descriptions;
        0x8DE0 - 0x4000, // start_of_room_connections;
        0x9A99 - 0x4000, // start_of_messages;
        0xB870 - 0x4000, // start_of_item_descriptions;
        0x8D5F - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0x95f0, // start_of_characters;
        0x9ce0, // start_of_image_blocks;
        0x38B6, // image patterns lookup table;
        0x1f, // number of patterns
        0x8e, // patterns end marker
        0x8279, // start of room image instructions
        210, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },
    {
        "Kayleth C64",
        KAYLETH_64,
        HEMAN_TYPE,                 // type
        KAYLETH,

        123,  // Number of items
        177, // Number of actions •
        79,  // Number of words •
        127,  // Number of rooms
        0,   // Max carried items •
        4,   // Word length
        254,  // Number of messages

        80, // number_of_verbs •
        79, // number_of_nouns; •

        0, // header •

        0, // no room images •
        0, // no item flags •
        0, // no item images •

        0x7A48 - 0x4000, // actions
        0x9323 - 0x4000,  // dictionary
        0xBFEE - 0x4000,  // tokens
        0xAEB3 - 0x4000, // start_of_room_descriptions;
        0x8DE0 - 0x4000, // start_of_room_connections;
        0x9A99 - 0x4000, // start_of_messages;
        0xB870 - 0x4000, // start_of_item_descriptions;
        0x8D5F - 0x4000, // start_of_item_locations;

        0x2539, // start_of_system_messages •
        0x28de, // start of directions •

        0xc028, // start_of_characters;
        0x5b28, // start_of_image_blocks;
        0, // image patterns lookup table;
        0, // number of patterns
        0, // patterns end marker
        0x8279, // start of room image instructions
        210, // number_of_image blocks;
        ZXOPT, // palette
        4, // picture_format_version;
    },
    {
        "Unknown game",
        UNKNOWN_GAME,
        NO_TYPE,
        UNKNOWN_GAME,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    }
};
