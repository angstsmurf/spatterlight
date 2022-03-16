#ifndef taylor_h
#define taylor_h

extern unsigned char WaitCharacter(void);
void LineInput(char *buf, int len);
void DisplayInit(void);
void TopWindow(void);
void BottomWindow(void);
void PrintCharacter(unsigned char c);

#define FOLLOWS 0xffff

#define MyLoc (Flag[0])

#define CurrentGame (Game->gameID)


typedef enum {
    UNKNOWN_GAME,
    BLIZZARD_PASS,
    REBEL_PLANET,
    TEMPLE_OF_TERROR,
    HEMAN,
    KAYLETH,
    HUMAN_TORCH,
    NUMGAMES
} GameIDType;

typedef struct {
    char *Text;
    /* PORTABILITY WARNING: THESE TWO MUST BE 8 BIT VALUES. */
    uint8_t Location;
    uint8_t InitialLoc;
    uint8_t Flag;
    uint8_t Image;
} Item;

typedef enum {
    YOU_SEE,
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    NORTHWEST,
    UP,
    DOWN,
    I_DONT_UNDERSTAND,
    THATS_BEYOND_MY_POWER,
    EXITS,
    WHAT_NOW,
    YOURE_CARRYING_TOO_MUCH,
    INVENTORY,
    NOTHING,
    PLAY_AGAIN,
    OK,
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
    WORN,
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

typedef enum { NO_PALETTE, ZX, ZXOPT, C64A, C64B, VGA } palette_type;

typedef enum {
    NO_TYPE,
    OLD_STYLE,
} GameType;

struct GameInfo {
    const char *Title;

    GameIDType gameID;
    GameType type;

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
    int start_of_room_descriptions;
    int start_of_room_connections;
    int start_of_messages;
    int start_of_item_descriptions;
    int start_of_item_locations;

    int start_of_system_messages;
    int start_of_directions;

    int start_of_characters;
    int start_of_image_blocks;
    int start_of_image_instructions;
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
};

extern unsigned char ObjectLoc[];
extern unsigned char Flag[];

extern winid_t Bottom, Top, Graphics;
extern int FileBaselineOffset;

extern int NumLowObjects;

extern struct GameInfo *Game;
extern Item *Items;

size_t FindCode(const char *x, size_t base, size_t len);


#endif /* taylor_h */
