//
//  definitions.h
//  Spatterlight
//
//  Created by Administrator on 2022-01-10.
//

#ifndef definitions_h
#define definitions_h

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define FOLLOWS 0xffff

#define GO 1
#define TAKE 10
#define DROP 18

#define LASTALL 127

typedef enum {
    UNKNOWN_GAME,
    SCOTTFREE,
    PIRATE,
    VOODOO,
    STRANGE,
    BANZAI,
    BATON,
    TIME_MACHINE,
    ARROW1,
    ARROW2,
    PULSAR7,
    CIRCUS,
    FEASABILITY,
    AKYRZ,
    PERSEUS,
    INDIANS,
    WAXWORKS,
    HULK,
    ADVENTURELAND,
    SECRET_MISSION,
    CLAYMORGUE,
    SPIDERMAN,
    SAVAGE_ISLAND,
    SAVAGE_ISLAND2,
    GREMLINS,
    GREMLINS_GERMAN,
    GREMLINS_SPANISH,
    SUPERGRAN,
    ROBIN_OF_SHERWOOD,
    SEAS_OF_BLOOD,
    NUMGAMES
} GameIDType;

typedef enum
{
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
    SAVED,
    CANT_USE_ALL,
    TRANSCRIPT_OFF,
    TRANSCRIPT_ON,
    NO_TRANSCRIPT,
    TRANSCRIPT_ALREADY,
    FAILED_TRANSCRIPT,
    TRANSCRIPT_START,
    TRANSCRIPT_END,
    LAST_SYSTEM_MESSAGE
} SysMessageType;

#define MAX_SYSMESS LAST_SYSTEM_MESSAGE

typedef enum
{
    NOT_A_GAME,
    FOUR_LETTER_UNCOMPRESSED,
    THREE_LETTER_UNCOMPRESSED,
    FIVE_LETTER_UNCOMPRESSED,
    FOUR_LETTER_COMPRESSED,
    GERMAN,
    SPANISH
} dictionary_type;

typedef enum
{
    NO_TYPE,
    GREMLINS_VARIANT,
    TEXT_ONLY,
} game_type;

typedef enum
{
    ENGLISH,
    MYSTERIOUS,
    LOCALIZED
} subtype;


typedef enum
{
    NO_PALETTE,
    ZX,
    ZXOPT,
    C64A,
    C64B,
    VGA
} palette_type;

typedef enum
{
    UNKNOWN_HEADER_TYPE,
    EARLY,
    LATE,
    HULK_HEADER,
} header_type;

typedef enum
{
    UNKNOWN_ACTIONS_TYPE,
    COMPRESSED,
    UNCOMPRESSED,
    HULK_ACTIONS
} action_table_type;

struct GameInfo
{
    const char *Title;

    GameIDType gameID;
    game_type type;
    subtype subtype;
    dictionary_type dictionary;

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
    header_type header_style;

    int start_of_room_image_list;
    int start_of_item_flags;
    int start_of_item_image_list;

    int start_of_actions;
    action_table_type actions_style;
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
    int image_address_offset; /* This is the difference between the value given by the image data lookup table and a usable file offset */
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
};

#endif /* definitions_h */
