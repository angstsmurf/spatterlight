//
//  definitions.h
//  Spatterlight
//
//  Created by Administrator on 2022-01-10.
//

#ifndef definitions_h
#define definitions_h

#include <stdint.h>

#define DEBUG_ACTIONS 0

#define debug_print(fmt, ...) \
do { if (DEBUG_ACTIONS) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

#define FOLLOWS 0xffff

#define MAX_GAMEFILE_SIZE 200000

#define MAX_WORDLENGTH 128
#define MAX_WORDS 128

#define MAX_BUFFER 512

#define YOUARE        1    /* You are not I am */
#define DEBUGGING    4    /* Info from database load */
#define NO_DELAYS 128     /* Skip all pauses */
#define FORCE_INVENTORY 1024     /* Inventory in upper window always on */
#define FORCE_INVENTORY_OFF 2048     /* Inventory in upper window always off */

#define NounObject (Counters[30])
#define CurrentCounter (Counters[47])

#define MyLoc     (Counters[32])
#define CurVerb   (Counters[48])
#define CurNoun   (Counters[49])
#define CurPrep   (Counters[50])
#define CurAdverb (Counters[51])
#define CurPartp  (Counters[52])
#define CurNoun2  (Counters[53])

#define CARRIED           255
#define HELD_BY_OTHER_GUY 99
#define DESTROYED         0

#define DARKBIT      15
#define MATCHBIT     33
#define DRAWBIT      34
#define GRAPHICSBIT  35
#define ALWAYSMATCH  60
#define STOPTIMEBIT  63

typedef struct {
    char *Word;
    uint8_t Group;
} DictWord;

typedef enum {
    WORD_NOT_FOUND,
    OTHER_TYPE,
    VERB_TYPE,
    NOUN_TYPE,
    ADVERB_TYPE,
    PREPOSITION_TYPE,
    NUM_WORD_TYPES,
} WordType;

typedef enum {
    NO_IMG,
    IMG_ROOM,
    IMG_OBJECT,
    IMG_SPECIAL,
} ImgType;

typedef struct {
    WordType Type;
    uint8_t Index;
} WordToken;

typedef struct {
    char *SynonymString;
    char *ReplacementString;
} Synonym;

typedef struct {
    uint8_t Verb;
    uint8_t NounOrChance;
    uint8_t *Words;
    uint8_t *Conditions;
    uint8_t *Commands;
    uint8_t NumWords;
    uint8_t CommandLength;
} Action;

typedef struct {
    char *Text;
    short Exits[7];
    short Image;
} Room;

typedef struct {
    char *Text;
    /* PORTABILITY WARNING: THESE TWO MUST BE 8 BIT VALUES. */
    uint8_t Location;
    uint8_t InitialLoc;
    uint8_t Flag;
    uint8_t Dictword;
} Item;

typedef struct {
    char *Title;
    short NumItems;
    short ActionSum;
    short NumNouns;
    short NumVerbs;
    short NumRooms;
    short MaxCarry;
    short PlayerRoom;
    short LightTime;
    short NumMessages;
    short Treasures;
    short NumActions;
    short TreasureRoom;
    short NumAdverbs;
    short NumPreps;
    short NumSubStr;
    short NumObjImg;
    short Unknown2;
} Header;

typedef struct {
    uint32_t room;
    uint32_t object;
    uint32_t image;
} ObjectImage;


typedef enum {
    UNKNOWN_GAME,
    BANZAI,
    BANZAI_C64,
    CLAYMORGUE,
    CLAYMORGUE_C64,
    SPIDERMAN,
    SPIDERMAN_C64,
    FANTASTIC4,
    FANTASTIC4_C64,
    NUMGAMES
} GameIDType;

typedef enum {
    ER_NO_RESULT,
    ER_SUCCESS = 0,
    ER_RAN_ALL_LINES_NO_MATCH = -1,
    ER_RAN_ALL_LINES = -2
} CommandResultType;

typedef enum {
    ACT_SUCCESS = 0,
    ACT_FAILURE = 1,
    ACT_CONTINUE,
    ACT_DONE,
    ACT_LOOP_BEGIN,
    ACT_LOOP,
    ACT_GAMEOVER
} ActionResultType;

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
    HIT_ANY,
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
    FIVE_LETTER_COMPRESSED,
    GERMAN,
    SPANISH,
    ITALIAN
} DictionaryType;

typedef enum {
    NO_VARIANT,
    GREMLINS_VARIANT,
    SHERWOOD_VARIANT,
    SAVAGE_ISLAND_VARIANT,
    SECRET_MISSION_VARIANT,
    SEAS_OF_BLOOD_VARIANT,
    OLD_STYLE,
} GameType;

typedef enum {
    ENGLISH = 0x1,
    MYSTERIOUS = 0x2,
    LOCALIZED = 0x4,
    C64 = 0x8
} Subtype;

typedef enum { NO_PALETTE, ZX, ZXOPT, C64A, C64B, VGA } palette_type;

typedef enum {
    NO_HEADER,
    SPIDERHEADER
} HeaderType;

typedef enum {
    UNKNOWN_ACTIONS_TYPE,
    COMPRESSED,
    UNCOMPRESSED,
    HULK_ACTIONS
} ActionTableType;

struct GameInfo {
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

    int start_of_characters;
    int start_of_image_data;
    int image_address_offset; /* This is the difference between the value given by
                               the image data lookup table and a usable file
                               offset */
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
};

#endif /* definitions_h */
