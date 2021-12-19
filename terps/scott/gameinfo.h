//
//  gameinfo.h
//  Spatterlight
//
//  Created by Administrator on 2022-01-09.
//

#ifndef gameinfo_h
#define gameinfo_h

#include "definitions.h"

#define NUMGAMES 13

struct GameInfo games[NUMGAMES] =
{
    {   "Seas of Blood",
        SEAS_OF_BLOOD,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_COMPRESSED, // dictionary type

        125,  // Number of items
        344, // Number of actions
        134, // Number of words
        83,  // Number of rooms
        10,  // Max carried items
        4,   // Word length
        99,  // Number of messages

        69, // number_of_verbs
        134, // number_of_nouns;

        0x494d, // header
        LATE,   // header style

        0,      // no room images
        0x4961, // item flags
        FOLLOWS, // item images

        FOLLOWS, // actions
        COMPRESSED,
        0x591b, // dictionary
        0x67cb, // start_of_room_descriptions;
        0x5c4b, // start_of_room_connections;
        0x5ebb, // start_of_messages;
        0x6ce0, // start_of_item_descriptions;
        0x5e3d, // start_of_item_locations;

        0x24ed, // start_of_system_messages
        FOLLOWS, // start of directions

        0x7389, // start_of_characters;
        0x7a89, // start_of_image_data;
        0x7b9f, // image_address_offset
        139,    // number_of_pictures;
        ZXOPT,   // palette
        4,      // picture_format_version;
    },

    {   "Robin of Sherwood",
        ROBIN_OF_SHERWOOD,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_COMPRESSED, // dictionary type

        87,  // Number of items
        295, // Number of actions
        114, // Number of words
        93,  // Number of rooms
        10,  // Max carried items
        4,   // Word length
        98,  // Number of messages

        115, // number_of_verbs
        109, // number_of_nouns;

        0x3b5a, // header
        LATE,   // header style

        0, //0x3d99 room images, zero because it needs custom handling
        0x3db8, // item flags
        FOLLOWS, // item images

        0x409b, // actions
        COMPRESSED,
        0x4dc3, // dictionary
        0, //0x9b53 start_of_room_descriptions, zero because of custom handling
        0x3e67, // start_of_room_connections
        0x5147, // start_of_messages
        0x5d65, // start_of_item_descriptions
        0x4d6b, // start_of_item_locations

        0x250b,  // start_of_system_messages
        0x26b5, // start of directions

        0x614f, // start_of_characters
        0x66bf, // start_of_image_data
        0x6765, // image_address_offset
        83,     // number_of_pictures
        ZXOPT,   // palette
        4,      // picture_format_version
    },

    {   "Gremlins (German)",
        GREMLINS_GERMAN,
        GREMLINS_VARIANT, // type
        LOCALIZED, // subtype
        GERMAN, // dictionary type

        99,  // Number of items
        236, // Number of actions
        126, // Number of words
        42,  // Number of rooms
        6,  // Max carried items
        5,   // Word length
        98,  // Number of messages

        115, // number_of_verbs
        126, // number_of_nouns

        0x237d, // header
        LATE,   // header style

        0x3a07,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images

        0x3afc, // actions
        COMPRESSED,
        0x45d9, // dictionary
        FOLLOWS, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x23de, // start_of_system_messages
        0x2623, // start of directions

        0x643e, // start_of_characters
        0x6c3e, // start_of_image_data
        0x6d3e,  //image_address_offset
        72,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    },

    {   "Gremlins (Spanish)",
        GREMLINS_SPANISH,
        GREMLINS_VARIANT, // type
        LOCALIZED, // subtype
        SPANISH, // dictionary type

        99,  // Number of items
        236, // Number of actions
        126, // Number of words
        42,  // Number of rooms
        6,  // Max carried items
        4,   // Word length
        98,  // Number of messages

        115, // number_of_verbs
        126, // number_of_nouns

        0x237d, // header
        LATE,   // header style

        0x3993,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images

        0x3a88, // actions
        COMPRESSED,
        0x455f, // dictionary
        FOLLOWS, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2426, // start_of_system_messages
        0x25c4, // start of directions

        0x6171, // start_of_characters
        0x6971, // start_of_image_data
        0x6A71,  //image_address_offset
        74,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    },

    {   "Gremlins",
        GREMLINS,
        GREMLINS_VARIANT, // type
        ENGLISH, // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        99,  // Number of items
        236, // Number of actions
        126, // Number of words
        42,  // Number of rooms
        6,  // Max carried items
        4,   // Word length
        98,  // Number of messages

        115, // number_of_verbs
        126, // number_of_nouns;

        0x2370, // header
        LATE,   // header style

        0x3a09,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images

        0x3afe, // actions
        COMPRESSED, // actions_style;
        0x45e5, // dictionary
        FOLLOWS, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;
        0x2426, // start_of_system_messages
        0x25f6, // start of directions

        0x5be1, // start_of_characters;
        0x63e1, // start_of_image_data
        0x64e1,      //image_address_offset
        78,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    },

    {
        "Questprobe 1: The Hulk",
        HULK,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        54,  // Number of items
        261, // Number of actions
        128, // Number of words
        20,  // Number of rooms
        10,  // Max carried items
        4,   // Word length
        99,  // Number of messages

        128, // number_of_verbs
        129, // number_of_nouns;

        0x4bf4, // header
        HULK_HEADER,   // header style

        0x270c,  // room images
        0x813c, // item flags
        0x26c8, // item images

        0x6087, // actions
        HULK_ACTIONS,
        0x4cc4, // dictionary
        0x51cd, // start_of_room_descriptions;
        0x7111, // start_of_room_connections;
        0x575e, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x5f3d, // start_of_item_locations;

        0x2553, // start_of_system_messages
        0x28f7, // start of directions

        0x281b,  // start_of_characters;
        0x741b, // start_of_image_data
        0,  // image_address_offset
        43,    // number_of_pictures;
        ZXOPT,   // palette
        0,      // picture_format_version;
    },

    {   "Questprobe 2: Spiderman",
        SPIDERMAN,
        NO_TYPE, // type
        ENGLISH, // subtype
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
        EARLY,   // header style

        0x3dd1,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images

        FOLLOWS, // actions
        UNCOMPRESSED,
        0x5036, // dictionary
        0x5518, // start_of_room_descriptions;
        0x4eac, // start_of_room_connections;
        0x575e, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4fa2, // start_of_item_locations;

        0x2553, // start_of_system_messages
        0x28f7, // start of directions

        0x6296,  // start_of_characters;
        0x6a96, // start_of_image_data
        -0x3fe5,  // image_address_offset
        41,    // number_of_pictures;
        ZXOPT,   // palette
        2,      // picture_format_version;
    },

    {   "The Sorcerer of Claymorgue Castle",
        CLAYMORGUE,
        NO_TYPE, // type
        ENGLISH, // subtype
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

        0x246b, // header
        EARLY,   // header style

        0x3bf6,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images
        FOLLOWS, // actions
        UNCOMPRESSED,
        0x4ecf, // dictionary
        0x53F7, // start_of_room_descriptions;
        0x4d6f, // start_of_room_connections;
        0x5605, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4e35, // start_of_item_locations;

        0x24e2, // start_of_system_messages
        0x2877, // start of directions

        0x6007,  // start_of_characters;
        0x6807, // start_of_image_data
        -0x3fe5,  // image_address_offset
        37,    // number_of_pictures;
        ZXOPT,   // palette
        2,      // picture_format_version;
    },

    {   "Adventureland",
        ADVENTURELAND,
        NO_TYPE, // type
        ENGLISH, // subtype
        THREE_LETTER_UNCOMPRESSED, // dictionary type

        65,  // Number of items
        181, // Number of actions
        69, // Number of words
        33,  // Number of rooms
        6,   // Max carried items
        3,   // Word length
        75,  // Number of messages

        70, // number_of_verbs
        69, // number_of_nouns;

        0x246b, // header
        EARLY,   // header style

        0x3ebe,  // room images
        FOLLOWS, // item flags
        0x3f1f, // item images
        0x3f5e, // actions
        UNCOMPRESSED,
        0x4c10, // dictionary
        0x4e40, // start_of_room_descriptions;
        0x4abe, // start_of_room_connections;
        0x52c3, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4b8a, // start_of_item_locations;

        0x24eb, // start_of_system_messages
        0x285e, // start of directions

        0x631e,  // start_of_characters;
        FOLLOWS, // start_of_image_data
        -0x3fe5,  // image_address_offset
        41,    // number_of_pictures;
        ZXOPT,   // palette
        1,      // picture_format_version;
    },
    {   "Secret Mission",
        SECRET_MISSION,
        NO_TYPE, // type
        ENGLISH, // subtype
        THREE_LETTER_UNCOMPRESSED, // dictionary type

        53,  // Number of items
        164, // Number of actions
        64, // Number of words
        23,  // Number of rooms
        7,   // Max carried items
        3,   // Word length
        81,  // Number of messages

        65, // number_of_verbs
        65, // number_of_nouns;

        0x246b, // header
        EARLY,   // header style
        0x3f26,  // room images
        FOLLOWS, // item flags
        0x2e28, // item images
        0x3fa2, // actions
        UNCOMPRESSED,
        0x4af0, // dictionary
        0x4cf8, // start_of_room_descriptions;
        0x49f2, // start_of_room_connections;
        0x4edf, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        0x4a82, // start_of_item_locations;

        0x24eb, // start_of_system_messages
        0x285e, // start of directions

        0x625d,  // start_of_characters;
        FOLLOWS, // start_of_image_data
        -0x3fe5,  // image_address_offset
        44,    // number_of_pictures;
        ZXOPT,   // palette
        1,      // picture_format_version;
    },
    
    {   "Savage Island part I",
        SAVAGE_ISLAND,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        58,  // Number of items
        259, // Number of actions
        84, // Number of words
        34,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        99,  // Number of messages

        82, // number_of_verbs
        84, // number_of_nouns;

        0x236d, // header
        LATE,   // header style
        0x390c,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images
        0x39a6, // actions
        COMPRESSED,
        0x4484, // dictionary
        0x47c7, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        0x4b91, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2423, // start_of_system_messages
        0x25f3, // start of directions

        0x570f,  // start_of_characters;
        FOLLOWS, // start_of_image_data
        0x600f,  // image_address_offset
        37,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    },

    {   "Savage Island part II",
        SAVAGE_ISLAND2,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        48,  // Number of items
        241, // Number of actions
        79, // Number of words
        30,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        95,  // Number of messages

        74, // number_of_verbs
        79, // number_of_nouns;

        0x236d, // header
        LATE,   // header style
        0x390c,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images
        0x398e, // actions
        COMPRESSED,
        0x43f0, // dictionary
        0x46ed, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
//        0x5379,
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2423, // start_of_system_messages
        0x25f3, // start of directions

        0x57d0,  // start_of_characters;
        FOLLOWS, // start_of_image_data
        0x60d0,  // image_address_offset
        21,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    },

    {   "Supergran",
        SUPERGRAN,
        NO_TYPE, // type
        ENGLISH, // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        85,  // Number of items
        204, // Number of actions
        105, // Number of words
        39,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        99,  // Number of messages

        101, // number_of_verbs
        106, // number_of_nouns;

        0x236d, // header
        LATE,   // header style
        0x38c8,  // room images
        FOLLOWS, // item flags
        FOLLOWS, // item images
        0x399e, // actions
        COMPRESSED,
        0x42fd, // dictionary
        0x4708, // start_of_room_descriptions;
        FOLLOWS, // start_of_room_connections;
        FOLLOWS, // start_of_messages;
        FOLLOWS, // start_of_item_descriptions;
        FOLLOWS, // start_of_item_locations;

        0x2423, // start_of_system_messages
        0x25f3, // start of directions

        0x5a4e,  // start_of_characters;
        FOLLOWS, // start_of_image_data
        0x634e,  // image_address_offset
        47,    // number_of_pictures;
        ZXOPT,   // palette
        3,      // picture_format_version;
    }
};



/* This is supposed to be the original ScottFree system
 messages in second person, as far as possible */
char *sysdict[MAX_SYSMESS] = {
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
    "Nothing.",
    "Tell me what to do ? ",
    "<HIT ENTER>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "You are carrying too much. \n",
    "You're dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "You carry nothing. ",
    "Your light is growing dim. ",
    ", ",
    "\n",
    " - ",
    "What ?",
    "yes",
    "no",
    "Answer yes or no.\n",
    "Are you sure? ",
    "Move undone. ",
    "Saved. ",
    "You can't use ALL with that verb. ",
    "Transcript is now off.\n",
    "Transcript is now on.\n",
    "No transcript is currently running.\n",
    "A transcript is already running.\n",
    "Failed to create transcript file. ",
    "Start of transcript\n\n",
    "\n\nEnd of transcript\n",
};

/* These are supposed to be the original ScottFree system
 messages in first person, as far as possible */
const char *sysdict_i_am[MAX_SYSMESS] = {
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
    "\nI can also see: ",
    "Obvious exits: ",
    "I'm carrying: \n",
    "Nothing.",
    "Tell me what to do ? ",
    "<HIT ENTER>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I've too much to carry. \n",
    "I'm dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "I carry nothing. ",
    "My light is growing dim. ",
    NULL,
};

const char *sysdict_zx[MAX_SYSMESS] = {
    "NORTH",
    "SOUTH",
    "EAST",
    "WEST",
    "UP",
    "DOWN",
    "The Adventure is over. Want to try this Adventure again? ",
    "I've stored",
    "Treasures. ",
    "On a scale of 0 to 100 that rates",
    "Dropped.",
    "Taken.",
    "O.K. ",
    "FANTASTIC! You've solved it ALL! \n",
    "I must be stupid, but I just don't understand what you mean ",
    "I can't do that...yet! ",
    "Huh? ",
    "I need a direction too. ",
    "I'm not carrying it. ",
    "I already have it. ",
    "I don't see it here. ",
    "It's beyond my Power to do that. ",
    "It's dangerous to move in the dark! ",
    "\nI fell and broke my neck! I'm DEAD! ",
    "I can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "It's too dark to see!\n",
    "I am in a ",
    ". Visible items:\n",
    "Exits: ",
    "I'm carrying the following: ",
    "Nothing at all. ",
    "---TELL ME WHAT TO DO ? ",
    "<HIT ENTER> ",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I'm carrying too much! Try: TAKE INVENTORY. ",
    "I'm DEAD!! ",
    "Restore a previously saved game ? ",
    "None",
    "There's nothing here to take. ",
    "I carry nothing. ",
    "My light is growing dim. ",
    " ",
    " ",
    ". ",
    "What ? ",
    NULL
};

//
// static char *ConditionDescriptions[]={
// "unused",
// "Carried",
// "In room with player",
// "Carried or in room with player",
// "In room",
// "Not in room with player",
// "Not carried",
// "Not in room",
// "Bitflag set",
// "Bitflag cleared",
// "Player carries anything",
// "Player carries nothing",
// "Not carried or in room with player",
// "Item is in play",
// "Item is not in play",
// "CurrentCounter <= ",
// "CurrentCounter >",
// "Object is in its initial room",
// "Object not in its initial room",
// "CurrentCounter = ",
// };
//
// static char *ActionDescriptions[]={
// "unused",
// "print message",
// "get item with check",
// "drop item",
// "move to room",
// "item is removed from play",
// "darkness set",
// "darkness cleared",
// "bitflag <arg> is set"
// "item is removed from play",
// "BitFlag <arg> is cleared",
// "Death. Dark flag cleared, player moved to last room",
// "Item <arg> put in room <arg>",
// "Game over.",
// "Describe room",
// "Score",
// "Inventory",
// "BitFlag 0 is set",
// "BitFlag 0 is cleared",
// "Refill lamp",
// "Screen is cleared",
// "Saves the game",
// "Swap item and item locations",
// "Continue with next line ",
// "Take item <arg> without check",
// "Put item with item",
// "Look - same as 64",
// "Decrement current counter.",
// "Print current counter value",
// "Set current counter value",
// "Swap location with current location-swap flag",
// "Swap counter"
// "Add <arg> to current counter",
// "Subtract <arg> from current counter",
// "Echo noun player typed without CR",
// "Echo the noun the player typed",
// "CR",
// "Swap current location value with backup location-swap value <arg>",
// "Wait 2 seconds",
// "Game-specific special actions",
// "Hulk draw picture",
// "Print message"
// };

#endif /* gameinfo_h */
