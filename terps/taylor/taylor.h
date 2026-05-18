//
//  taylor.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#ifndef taylor_h
#define taylor_h

#include <stdlib.h>
#include "palette.h"
#include "glk.h"

/* Shortcuts for fields of the currently loaded GameInfo record. */
#define CurrentGame (Game->gameID)
#define Version (Game->type)
#define BaseGame (Game->base_game)

/* Runtime query for whether the Glk host can show graphics. Under
   Spatterlight this is a user preference; elsewhere we ask Glk. */
#ifdef SPATTERLIGHT
#define TAYLOR_GRAPHICS_ENABLED gli_enable_graphics
#else
#define TAYLOR_GRAPHICS_ENABLED glk_gestalt(gestalt_Graphics, 0)
#endif

/* Named aliases for game-state flag slots. The slot numbers are baked
   into the Taylor data format and referenced directly by action
   opcodes, so they can't be renumbered. */
#define MyLoc (Flag[0])
#define OtherGuyLoc (Flag[1])
#define OtherGuyInv (Flag[3])
#define MaxCarried (Flag[4])
#define ItemsCarried (Flag[5])
#define TurnsLow (Flag[26])
#define TurnsHigh (Flag[27])
#define IsThing (Flag[31])
#define ThingAsphyx (Flag[47])
#define TorchAsphyx (Flag[48])
#define DrawImages (Flag[52])
#define Q3SwitchedWatch (Flag[126])

/* All supported game variants. Each title has separate IDs for the ZX
   Spectrum and Commodore 64 releases; Temple of Terror also has a
   text-only/hybrid split. */
typedef enum {
    QUESTPROBE3,
    QUESTPROBE3_64,
    REBEL_PLANET,
    REBEL_PLANET_64,
    BLIZZARD_PASS,
    HEMAN,
    HEMAN_64,
    TEMPLE_OF_TERROR,
    TEMPLE_OF_TERROR_64,
    TOT_TEXT_ONLY,
    TOT_HYBRID,
    TOT_TEXT_ONLY_64,
    TOT_HYBRID_64,
    KAYLETH,
    KAYLETH_64,
    UNKNOWN_GAME,
    NUMGAMES
} GameIDType;

/* Indices into the per-game system-message table (compass directions,
   "Inventory", "OK", "You can't go that way", etc.). Order is fixed
   because the loader populates the table positionally. */
typedef enum {
    YOU_SEE,
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    NORTHEAST,
    NORTHWEST,
    SOUTHEAST,
    SOUTHWEST,
    I_DONT_UNDERSTAND,
    THATS_BEYOND_MY_POWER,
    EXITS,
    WHAT_NOW,
    YOURE_CARRYING_TOO_MUCH,
    INVENTORY,
    NOTHING,
    PLAY_AGAIN,
    OKAY,
    HIT_ENTER,
    YOU_HAVE_IT,
    YOU_DONT_SEE_IT,
    YOU_HAVENT_GOT_IT,
    YOU_CANT_GO_THAT_WAY,
    TOO_DARK_TO_SEE,
    RESUME_A_SAVED_GAME,
    EMPTY_MESSAGE,
    YOU_ARE_NOT_WEARING_IT,
    YOU_ARE_WEARING_IT,
    NOWWORN,
    NO_HELP_AVAILABLE,
    I_DONT_UNDERSTAND_THAT_VERB,
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

/* Condition opcodes from Taylor's action tables. Values match the
   byte codes stored in the original game data and must stay in order. */
typedef enum {
    CONDITIONERROR,
    AT,
    NOTAT,
    ATGT,
    ATLT,
    PRESENT,
    HERE,
    ABSENT,
    NOTHERE,
    CARRIED,
    NOTCARRIED,
    WORN,
    NOTWORN,
    NODESTROYED,
    DESTROYED,
    ZERO,
    NOTZERO,
    WORD1,
    WORD2,
    WORD3,
    CHANCE,
    LT,
    GT,
    EQ,
    NE,
    OBJECTAT,
    COND26,
    COND27,
    COND28,
    COND29,
    COND30,
    COND31,
} ConditionType;

/* Action opcodes from Taylor's action tables. Values match the byte
   codes stored in the original game data and must stay in order. */
typedef enum {
    ACTIONERROR,
    LOADPROMPT,
    QUIT,
    SHOWINVENTORY,
    ANYKEY,
    SAVE,
    DROPALL,
    LOOK,
    PRINTOK,
    GET,
    DROP,
    GOTO,
    GOBY,
    SET,
    CLEAR,
    MESSAGE,
    CREATE,
    DESTROY,
    PRINT,
    DELAY,
    WEAR,
    REMOVE,
    LET,
    ADD,
    SUB,
    PUT,
    SWAP,
    SWAPF,
    MEANS,
    PUTWITH,
    BEEP,
    REFRESH,
    RAMSAVE,
    RAMLOAD,
    CLSLOW,
    OOPS,
    DIAGNOSE,
    SWITCHINVENTORY,
    SWITCHCHARACTER,
    DONE,
    IMAGE,
    ACT41,
    ACT42,
    ACT43,
    ACT44,
    ACT45,
    ACT46,
    ACT47,
    ACT48,
    ACT49,
    ACT50,
} ActionType;

/* Bitfield of runtime options, mostly driven by user preferences. */
typedef enum {
    DEBUGGING = 1, /* Info from database load */
    NO_DELAYS = 2, /* Skip all pauses */
    FORCE_PALETTE_ZX = 4, /* Force ZX Spectrum image palette */
    FORCE_PALETTE_C64 = 8, /* Force CBM 64 image palette */
    FORCE_INVENTORY = 16, /* Inventory in upper window always on */
    FORCE_INVENTORY_OFF = 32 /* Inventory in upper window always off */
} OptionsType;

/* Coarse data-format family. Several game IDs may share one format,
   so loader/interpreter quirks switch on this rather than on gameID. */
typedef enum {
    NO_TYPE,
    REBEL_PLANET_TYPE,
    BLIZZARD_PASS_TYPE,
    HEMAN_TYPE,
    QUESTPROBE3_TYPE
} GameVersion;

/* Per-game metadata: title, identity, and the file offsets the loader
   needs to locate each table inside the raw game data. */
typedef struct {
    const char *Title;

    GameIDType gameID;
    GameVersion type;
    GameIDType base_game;

    int start_of_dictionary;
    int start_of_tokens;
    int start_of_room_descriptions;
    int start_of_item_descriptions;
    int start_of_automatics;
    int start_of_actions;
    int start_of_room_connections;
    int start_of_flags;
    int start_of_item_locations;
    int start_of_messages;
    int start_of_messages_2;

    int start_of_tiles;
    int start_of_image_blocks;
    int image_patterns_lookup;
    int number_of_patterns;
    int pattern_end_marker;
    int start_of_image_instructions;
    int number_of_pictures;
    palette_type palette;
    int start_of_intro_text;
} GameInfo;

/* Registry of all supported games, indexed by GameIDType. */
extern GameInfo games[NUMGAMES];

#include "player.h"
#include "ui.h"

#endif /* taylor_h */
