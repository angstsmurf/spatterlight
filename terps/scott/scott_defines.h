//
//  scottdefines.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef scottdefines_h
#define scottdefines_h

#include <stdint.h>

#include "palette.h"
#include "minmax.h"
#include "common_defines.h"

#define FOLLOWS 0xffff

#define GO 1
#define TAKE 10
#define DROP 18

#define LASTALL 128

/* Condition codes (0-19) — used in action table condition fields */
#define COND_PARAMETER       0
#define COND_CARRIED         1
#define COND_IN_ROOM         2
#define COND_PRESENT         3
#define COND_AT_LOC          4
#define COND_NOT_IN_ROOM     5
#define COND_NOT_CARRIED     6
#define COND_NOT_AT_LOC      7
#define COND_FLAG_SET        8
#define COND_FLAG_CLEAR      9
#define COND_CARRYING_ANY   10
#define COND_CARRYING_NONE  11
#define COND_NOT_PRESENT    12
#define COND_IN_PLAY        13
#define COND_NOT_IN_PLAY    14
#define COND_COUNTER_LE     15
#define COND_COUNTER_GT     16
#define COND_NOT_MOVED      17
#define COND_MOVED          18
#define COND_COUNTER_EQ     19

/* Opcode message ranges: codes 1-51 print that message directly,
   codes 102+ print message (code - EXTENDED_MSG_OFFSET) */
#define FIRST_MESSAGE        1
#define LAST_DIRECT_MESSAGE 51
#define EXTENDED_MSG_BASE  102
#define EXTENDED_MSG_OFFSET 50

/* Action opcodes (52-90) — game state operations.
   Equivalent to corresponding TI99OP_ definitions in ti99_4a_terp.c. */
#define OP_GET_ITEM         52
#define OP_DROP_ITEM        53
#define OP_GOTO_ROOM        54
#define OP_DESTROY_ITEM     55
#define OP_SET_DARK         56
#define OP_SET_LIGHT        57
#define OP_SET_FLAG         58
#define OP_DESTROY_ITEM_2   59
#define OP_CLEAR_FLAG       60
#define OP_DIE              61
#define OP_MOVE_ITEM        62
#define OP_GAME_OVER        63
#define OP_LOOK             64
#define OP_PRINT_SCORE      65
#define OP_LIST_INVENTORY   66
#define OP_SET_FLAG_0       67
#define OP_CLEAR_FLAG_0     68
#define OP_REFILL_LIGHT     69
#define OP_CLEAR_SCREEN     70
#define OP_SAVE             71
#define OP_SWAP_ITEMS       72
#define OP_CONTINUE         73
#define OP_FORCE_CARRY      74
#define OP_MOVE_TO_LOC_OF   75
#define OP_LOOK_2           76
#define OP_DEC_COUNTER      77
#define OP_PRINT_COUNTER    78
#define OP_SET_COUNTER      79
#define OP_GOTO_STORED      80
#define OP_SWAP_COUNTER     81
#define OP_ADD_COUNTER      82
#define OP_SUB_COUNTER      83
#define OP_PRINT_NOUN       84
#define OP_PRINTLN_NOUN     85
#define OP_NEWLINE          86
#define OP_SWAP_ROOM        87
#define OP_DELAY            88
#define OP_GAME_SPECIFIC    89
#define OP_DRAW_IMAGE       90

/* Game-specific EXAMINE verb indices */
#define HULK_EXAMINE_VERB    39
#define COUNT_EXAMINE_VERB    8
#define VOODOO_EXAMINE_VERB  42
#define ADVLAND_EXAMINE_VERB 29
#define PIRATE_EXAMINE_VERB  27
#define MISSION_EXAMINE_VERB 40
#define STRANGE_EXAMINE_VERB 37

/* US-variant game-specific image */
#define US_CLOSEUP_IMAGE 80

#define RTPI_WIN_IMAGE 26

/* --- TI-99/4A bytecode opcode definitions ---
   Conditions (183–201): each takes one parameter byte.
   Most are equivalent to the COND_XXX conditions in
   scott_actions.c (but with different numbers). */
#define TI99CND_CARRIED        183
#define TI99CND_IN_ROOM        184
#define TI99CND_PRESENT        185
#define TI99CND_NOT_IN_ROOM    186
#define TI99CND_NOT_CARRIED    187
#define TI99CND_NOT_PRESENT    188
#define TI99CND_IN_PLAY        189
#define TI99CND_NOT_IN_PLAY    190
#define TI99CND_AT_LOC         191
#define TI99CND_NOT_AT_LOC     192
#define TI99CND_FLAG_SET       193
#define TI99CND_FLAG_CLEAR     194
#define TI99CND_CARRYING_ANY   195
#define TI99CND_CARRYING_NONE  196
#define TI99CND_COUNTER_LE     197
#define TI99CND_COUNTER_GT     198
#define TI99CND_COUNTER_EQ     199
#define TI99CND_NOT_MOVED      200
#define TI99CND_MOVED          201

/* Commands (212–254). */
/* equivalent to the OP_XXX opcodes in
scott_actions.c (but with different numbers). */
#define TI99OP_CLEAR_SCREEN     212
#define TI99OP_AUTO_INV_ON      214
#define TI99OP_AUTO_INV_OFF     215
#define TI99OP_SUCCESS_OFF      216
#define TI99OP_SUCCESS_ON       217
#define TI99OP_TRY              218
#define TI99OP_GET_ITEM         219
#define TI99OP_DRTI99OP_ITEM        220
#define TI99OP_GOTO_ROOM        221
#define TI99OP_DESTROY_ITEM     222
#define TI99OP_SET_DARK         223
#define TI99OP_SET_LIGHT        224
#define TI99OP_SET_FLAG         225
#define TI99OP_CLEAR_FLAG       226
#define TI99OP_SET_FLAG_0       227
#define TI99OP_CLEAR_FLAG_0     228
#define TI99OP_DIE              229
#define TI99OP_MOVE_ITEM        230
#define TI99OP_GAME_OVER        231
#define TI99OP_PRINT_SCORE      232
#define TI99OP_LIST_INVENTORY   233
#define TI99OP_REFILL_LIGHT     234
#define TI99OP_SAVE             235
#define TI99OP_SWAP_ITEMS       236
#define TI99OP_FORCE_CARRY      237
#define TI99OP_MOVE_TO_LOC_OF   238
#define TI99OP_CLEAR            239
#define TI99OP_LOOK             240
#define TI99OP_LOOK_2           241
#define TI99OP_INC_COUNTER      242
#define TI99OP_DEC_COUNTER      243
#define TI99OP_PRINT_COUNTER    244
#define TI99OP_SET_COUNTER      245
#define TI99OP_ADD_COUNTER      246
#define TI99OP_SUB_COUNTER      247
#define TI99OP_GOTO_STORED      248
#define TI99OP_SWAP_ROOM        249
#define TI99OP_SWAP_COUNTER     250
#define TI99OP_PRINT_NOUN       251
#define TI99OP_PRINTLN_NOUN     252
#define TI99OP_NEWLINE          253
#define TI99OP_DELAY            254
#define TI99OP_SUCCESS          255

#define MAX_MESSAGE_OPCODE  182
#define MAX_TRY_DEPTH        32

typedef enum {
    UNKNOWN_GAME,
    ADVENTURELAND_US = 1,
    PIRATE_US = 2,
    SECRET_MISSION_US = 3,
    VOODOO_CASTLE_US = 4,
    COUNT_US = 5,
    STRANGE_ODYSSEY_US = 6,
    MYSTERY_FUNHOUSE_US = 7,
    PYRAMID_OF_DOOM_US = 8,
    GHOST_TOWN_US = 9,
    SAVAGE_ISLAND_US = 10,
    SAVAGE_ISLAND_2_US = 11,
    GOLDEN_VOYAGE_US = 12,
    CLAYMORGUE_US = 13,
    CLAYMORGUE_US_126,
    RETURN_TO_PIRATES_ISLE,
    ADVENTURELAND,
    PIRATE,
    SECRET_MISSION,
    VOODOO_CASTLE,
    COUNT,
    STRANGE_ODYSSEY,
    MYSTERY_FUNHOUSE,
    PYRAMID_OF_DOOM,
    GHOST_TOWN,
    SAVAGE_ISLAND,
    SAVAGE_ISLAND2,
    GOLDEN_VOYAGE,
    CLAYMORGUE,
    BANZAI,
    BATON,
    BATON_C64,
    TIME_MACHINE,
    TIME_MACHINE_C64,
    ARROW1,
    ARROW1_C64,
    ARROW2,
    ARROW2_C64,
    PULSAR7,
    PULSAR7_C64,
    CIRCUS,
    CIRCUS_C64,
    FEASIBILITY,
    FEASIBILITY_C64,
    AKYRZ,
    AKYRZ_C64,
    PERSEUS,
    PERSEUS_C64,
    PERSEUS_ITALIAN,
    INDIANS,
    INDIANS_C64,
    WAXWORKS,
    WAXWORKS_C64,
    HULK,
    HULK_C64,
    HULK_US,
    HULK_US_PREL,
    ADVENTURELAND_C64,
    SECRET_MISSION_C64,
    CLAYMORGUE_C64,
    SPIDERMAN,
    SPIDERMAN_C64,
    SAVAGE_ISLAND_C64,
    SAVAGE_ISLAND2_C64,
    GREMLINS,
    GREMLINS_C64,
    GREMLINS_GERMAN,
    GREMLINS_GERMAN_C64,
    GREMLINS_SPANISH,
    GREMLINS_SPANISH_C64,
    SUPERGRAN,
    SUPERGRAN_C64,
    ROBIN_OF_SHERWOOD,
    ROBIN_OF_SHERWOOD_C64,
    SEAS_OF_BLOOD,
    SEAS_OF_BLOOD_C64,
    SCOTTFREE,
    TI994A,
    NUMGAMES
} GameIDType;

typedef enum {
    ER_NO_RESULT,
    ER_SUCCESS = 0,
    ER_RAN_ALL_LINES_NO_MATCH = -1,
    ER_RAN_ALL_LINES = -2
} ExplicitResultType;

typedef enum {
    ACT_SUCCESS = 0,
    ACT_FAILURE = 1,
    ACT_CONTINUE,
    ACT_GAMEOVER
} ActionResultType;

typedef enum {
    SYS_UNKNOWN,
    SYS_MSDOS,
    SYS_C64,
    SYS_ATARI8,
    SYS_APPLE2,
    SYS_TI994A,
    SYS_APPLE2_LINES,
    SYS_ATARI8_LINES,
    SYS_C64_TINY
} MachineType;

typedef enum {
    IMG_ROOM,
    IMG_ROOM_OBJ,
    IMG_INV_OBJ,
    IMG_INV_AND_ROOM_OBJ
} USImageType;

typedef enum {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    PLAY_AGAIN,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    DROPPED,
    TAKEN,
    OK,
    YOUVE_SOLVED_IT,
    I_DONT_UNDERSTAND,
    YOU_CANT_DO_THAT_YET,
    HUH,
    DIRECTION,
    YOU_HAVENT_GOT_IT,
    YOU_HAVE_IT,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOU_CANT_GO_THAT_WAY,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    TOO_DARK_TO_SEE,
    YOU_ARE,
    YOU_SEE,
    EXITS,
    INVENTORY,
    NOTHING,
    WHAT_NOW,
    HIT_ENTER,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    RESUME_A_SAVED_GAME,
    NONE,
    NOTHING_HERE_TO_TAKE,
    YOU_HAVE_NOTHING,
    LIGHT_GROWING_DIM,
    EXITS_DELIMITER,
    MESSAGE_DELIMITER,
    ITEM_DELIMITER,
    WHAT,
    YES,
    NO,
    ANSWER_YES_OR_NO,
    ARE_YOU_SURE,
    MOVE_UNDONE,
    CANT_UNDO_ON_FIRST_TURN,
    NO_UNDO_STATES,
    SAVED,
    CANT_USE_ALL,
    TRANSCRIPT_OFF,
    TRANSCRIPT_ON,
    NO_TRANSCRIPT,
    TRANSCRIPT_ALREADY,
    FAILED_TRANSCRIPT,
    TRANSCRIPT_START,
    TRANSCRIPT_END,
    BAD_DATA,
    STATE_SAVED,
    STATE_RESTORED,
    NO_SAVED_STATE,
    LAST_SYSTEM_MESSAGE
} SysMessageType;

#define MAX_SYSMESS LAST_SYSTEM_MESSAGE

typedef enum {
    NOT_A_GAME,
    FOUR_LETTER_UNCOMPRESSED,
    THREE_LETTER_UNCOMPRESSED,
    FIVE_LETTER_UNCOMPRESSED,
    FOUR_LETTER_COMPRESSED,
    GERMAN,
    GERMAN_C64,
    SPANISH,
    SPANISH_C64,
    ITALIAN
} DictionaryType;

typedef enum {
    NO_TYPE,
    GREMLINS_VARIANT,
    SHERWOOD_VARIANT,
    SAVAGE_ISLAND_VARIANT,
    SECRET_MISSION_VARIANT,
    SEAS_OF_BLOOD_VARIANT,
    US_VARIANT,
    OLD_STYLE
} GameType;

typedef enum {
    ENGLISH = 0x1,
    MYSTERIOUS = 0x2,
    LOCALIZED = 0x4,
    C64 = 0x8
} Subtype;

typedef enum {
    NO_HEADER,
    EARLY,
    LATE,
    US_HEADER,
    GREMLINS_C64_HEADER,
    ROBIN_C64_HEADER,
    SUPERGRAN_C64_HEADER,
    SEAS_OF_BLOOD_C64_HEADER,
    MYSTERIOUS_C64_HEADER,
    ARROW_OF_DEATH_PT_2_C64_HEADER,
    INDIANS_C64_HEADER
} HeaderType;

typedef enum {
    UNKNOWN_ACTIONS_TYPE,
    COMPRESSED,
    UNCOMPRESSED,
    HULK_ACTIONS
} ActionTableType;

typedef struct {
    const char *Title;

    GameIDType gameID;
    GameType type;
    int subtype;
    DictionaryType dictionary;

    int number_of_items;
    int number_of_actions;
    int number_of_words;
    int number_of_rooms;
    int max_carried;
    int word_length;
    int number_of_messages;

    int number_of_verbs;
    int number_of_nouns;

    int start_of_header;
    HeaderType header_style;

    int start_of_room_image_list;
    int start_of_item_flags;
    int start_of_item_image_list;

    int start_of_actions;
    ActionTableType actions_style;
    int start_of_dictionary;
    int start_of_room_descriptions;
    int start_of_room_connections;
    int start_of_messages;
    int start_of_item_descriptions;
    int start_of_item_locations;

    int start_of_system_messages;
    int start_of_directions;

    int start_of_tiles;
    int start_of_image_data;
    int image_address_offset; /* This is the difference between the value given by
                               the image data lookup table and a usable file
                               offset */
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
} GameInfo;

#endif /* scottdefines_h */
