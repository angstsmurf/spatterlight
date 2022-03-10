#ifndef taylor_h
#define taylor_h

#include "glk.h"

unsigned char WaitCharacter(void);
void LineInput(char *buf, int len);
void DisplayInit(void);
void TopWindow(void);
void BottomWindow(void);
void PrintCharacter(unsigned char c);
void DrawRoomImage(void);
size_t FindCode(const char *x, size_t base, size_t len);
void Updates(event_t ev);
void DrawBlack(void);
void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
;
void CloseGraphicsWindow(void);
void OpenGraphicsWindow(void);
void OpenTopWindow(void);

#define FOLLOWS 0xffff

#define MyLoc (Flag[0])

#define CurrentGame (Game->gameID)
#define Version (Game->type)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum {
    QUESTPROBE3,
    REBEL_PLANET,
    BLIZZARD_PASS,
    HEMAN,
    TEMPLE_OF_TERROR,
    KAYLETH,
    UNKNOWN_GAME,
    NUMGAMES
} GameIDType;

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
    CONTINUE,
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

typedef enum { NO_PALETTE, ZX, ZXOPT, C64A, C64B, VGA } palette_type;

typedef enum {
    REBEL_PLANET_TYPE,
    BLIZZARD_PASS_TYPE,
    HEMAN_TYPE,
    QUESTPROBE3_TYPE
} GameVersion;

struct GameInfo {
    const char *Title;

    GameIDType gameID;
    GameVersion type;

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

    int start_of_room_image_list;
    int start_of_item_flags;
    int start_of_item_image_list;

    int start_of_actions;
    int start_of_dictionary;
    int start_of_tokens;
    int start_of_room_descriptions;
    int start_of_room_connections;
    int start_of_messages;
    int start_of_item_descriptions;
    int start_of_item_locations;

    int start_of_system_messages;
    int start_of_directions;

    int start_of_characters;
    int start_of_image_blocks;
    int image_patterns_lookup;
    int number_of_patterns;
    int pattern_end_marker;
    int start_of_image_instructions;
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
};

extern unsigned char ObjectLoc[];
extern unsigned char Flag[];

extern winid_t Bottom, Top, Graphics, CurrentWindow;
extern long FileBaselineOffset;

extern int NumLowObjects;

extern struct GameInfo *Game;

extern int Resizing;

extern char DelimiterChar;

#endif /* taylor_h */
