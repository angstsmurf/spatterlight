//
//  gameinfo.c
//  Plus
//
//  Created by Administrator on 2022-01-30.
//

#include <stdio.h>

#include "definitions.h"

const struct GameInfo games[] = {
    {
        "Buckaroo Banzai",
        BANZAI,
        OLD_STYLE,                // type
        ENGLISH,                  // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        60,  // Number of items
        266, // Number of actions
        110, // Number of words
        35,  // Number of rooms
        5,   // Max carried items
        4,   // Word length
        95,  // Number of messages

        111, // number_of_verbs
        110, // number_of_nouns;

        0x2451, // header

        0, // no room images
        0, // no item flags
        0, // no item images

        0x3a50, // actions
        0x4c54,  // dictionary
        FOLLOWS, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2539, // start_of_system_messages
        0x28de, // start of directions

        0, // start_of_characters;
        0, // start_of_image_data;
        0, // image_address_offset
        0, // number_of_pictures;
        0, // palette
        0, // picture_format_version;
    },

    {
        "The Sorcerer of Claymorgue Castle",
        CLAYMORGUE,
        NO_VARIANT,                 // type
        ENGLISH,                  // subtype
        FIVE_LETTER_UNCOMPRESSED, // dictionary type

        75,  // Number of items
        267, // Number of actions
        109, // Number of words
        32,  // Number of rooms
        10,  // Max carried items
        5,   // Word length
        79,  // Number of messages

        110, // number_of_verbs
        108, // number_of_nouns;

        0x246c, // header

        0x3bf6,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images
        FOLLOWS, // actions
        0x4ecf,  // dictionary
        0x53f7,  // start_of_room_descriptions;
        0x4d6f,  // start_of_room_connections;
        0x5605,  // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4e35,  // start_of_item_locations;

        0x24e2, // start_of_system_messages
        0x2877, // start of directions

        0x6007,  // start_of_characters;
        0x6807,  // start_of_image_data
        -0x3fe5, // image_address_offset
        37,      // number_of_pictures;
        ZXOPT,   // palette
        1,       // picture_format_version;
    },


    {
        "Questprobe 2: Spider-Man",
        SPIDERMAN,
        NO_VARIANT,               // type
        ENGLISH,                  // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        72,  // Number of items
        257, // Number of actions
        124, // Number of words
        40,  // Number of rooms
        12,  // Max carried items
        4,   // Word length
        99,  // Number of messages

        125, // number_of_verbs
        125, // number_of_nouns;

        0x246b, // header

        0x3dd1,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images

        FOLLOWS, // actions
        0x5036,  // dictionary
        0x5518,  // start_of_room_descriptions;
        0x4eac,  // start_of_room_connections;
        0x575e,  // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4fa2,  // start_of_item_locations;

        0x2553, // start_of_system_messages
        0x28f7, // start of directions

        0x6296,  // start_of_characters;
        0x6a96,  // start_of_image_data
        -0x3fe5, // image_address_offset
        41,      // number_of_pictures;
        ZXOPT,   // palette
        2,       // picture_format_version;
    },

    {
        NULL
    }
};

/* This is supposed to be the original ScottFree system
 messages in second person, as far as possible */
const char *sysdict[MAX_SYSMESS] = {
    "North",
    "South",
    "East",
    "West",
    "Up",
    "Down",
    "The game is now over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "O.K.",
    "O.K.",
    "O.K. ",
    "Well done.\n",
    "I don't understand ",
    "You can't do that yet. ",
    "Huh ? ",
    "Give me a direction too. ",
    "You haven't got it. ",
    "You have it. ",
    "You don't see it. ",
    "It is beyond your power to do that. ",
    "\nDangerous to move in the dark! ",
    "You fell down and broke your neck. ",
    "You can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "You can't see. It is too dark!\n",
    "You are in a ",
    "\nYou can also see: ",
    "Obvious exits: ",
    "You are carrying:\n",
    "Nothing.\n",
    "Tell me what to do ? ",
    "<Hit any key>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "You've too much to carry. Try -TAKE INVENTORY-\n",
    "You're dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "You carry nothing. ",
    "Your light is growing dim. ",
    ", ",
    " ",
    " - ",
    "What ?",
    "yes",
    "no",
    "\nAnswer yes or no. ",
    "Are you sure? ",
    "Move undone.",
    "Can't undo on first turn.",
    "No more undo states stored.",
    "Saved.",
    "You can't use ALL with that verb. ",
    "Transcript is now off.\n",
    "Transcript is now on.\n",
    "No transcript is currently running.\n",
    "A transcript is already running.\n",
    "Failed to create transcript file. ",
    "Start of transcript\n\n",
    "\n\nEnd of transcript\n",
    "BAD DATA! Invalid save file.",
    "State saved.\n",
    "State restored.\n",
    "No saved state exists.\n"
};

/* These are supposed to be the original ScottFree system
 messages in first person, as far as possible */
const char *sysdict_i_am[MAX_SYSMESS] = {
    "north",
    "south",
    "east",
    "west",
    "up",
    "down",
    "The game is now over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "O.K.",
    "O.K.",
    "O.K. ",
    "Well done.\n",
    "I didn't completely understand your command. ",
    "I can't do that yet. ",
    "Huh ? ",
    "Give me a direction too.",
    "I'm not carrying it. ",
    "I already have it. ",
    "I don't see it here. ",
    "It is beyond my power to do that. ",
    "Dangerous to move in the dark! ",
    "\nI fell and broke my neck.",
    "I can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "I can't see. It is too dark!\n",
    "I'm in a ",
    ", and I see here",
    "I see an exit ",
    "I'm carrying",
    " nothing at all.",
    "I WANT YOU TO ",
    "<Hit any key>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I've too much to carry. \n",
    " I'm dead... ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "I have nothing to drop. ",
    "My light is growing dim. ",
    ", ",
    " ",
    ",",
    NULL
};
