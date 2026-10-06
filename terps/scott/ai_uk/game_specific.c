//
//  game_specific.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-27.
//

#include <string.h>

#include "sagagraphics.h"
#include "apple2draw.h"
#include "game_specific.h"
#include "parser.h"
#include "saga.h"
#include "sagadraw.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_display.h"

void UpdateSecretAnimations(void)
{
    if (MyLoc != 13 || AnimationFlag == 0) {
        glk_request_timer_events(0);
        AnimationFlag = 0;
        return;
    }
    /* Roll down */
    if (AnimationFlag > 0 && AnimationFlag < 48) {
        RectFill(113, 0, 102, AnimationFlag, white_colour);
        AnimationFlag++;
    }
    /* Flicker */
    if (AnimationFlag >= 48 && AnimationFlag < 100) {
        if (Items[21].Location == 0) {
            /* Image 30 is a white square */
            DrawImage(30);
            glk_request_timer_events(0);
            AnimationFlag = 0;
        } else if (AnimationFlag > 69) {
            if (AnimationFlag == 70)
                DrawImage(30);
            else
                DrawBlack();
            AnimationFlag = 70 + (AnimationFlag == 70);
        } else {
            AnimationFlag++;
        }
    }
    /* Roll up */
    if (AnimationFlag >= 100 && AnimationFlag < 200) {
        int height = AnimationFlag - 100;
        if (height)
            RectFill(112, 48 - height, 104, height, 0);
        AnimationFlag++;
    }
}

void SecretAction(int p)
{
    int oldimage = 0;
    switch (p) {
    case 1: /* Bomb explodes */
        if (split_screen) {
            glk_window_close(Top, NULL);
            Top = NULL;
            MyLoc = 1;
            Rooms[1].Image = 25; /* BOOM */
            DrawImage(25);
            Delay(2);
            DrawImage(26);
            Rooms[1].Image = 26; /* Mushroom cloud */
        }
        break;
    case 2: /* ID picture */
        DrawImage(24);
        if (IsPresent(1)) {
            DrawImage(27); // "Security" label
        } else if (IsPresent(7) && !IsPresent(8) && !IsPresent(41)) {
            DrawImage(28); /* "Visitor" label */
        }
        showing_closeup = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
        break;
    case 3: /* Screen rolls down */
    case 4:
        oldimage = Rooms[13].Image;
        /* Image 30 is a white square */
        Rooms[13].Image = 30;
        DrawBlack();
        AnimationFlag = 1;
        glk_request_timer_events(10);
        Output(sys[HIT_ENTER]);
        Output("\n");
        HitEnter();
        DrawImage(30);
        AnimationFlag = 100;
        glk_request_timer_events(5);
        event_t ev;
        do {
            glk_select(&ev);
            Updates(ev);
        } while (AnimationFlag < 200);
        Rooms[13].Image = oldimage;
        AnimationFlag = 0;
        break;
    default:
        fprintf(stderr, "Secret Mission: Unsupported action parameter %d!\n", p);
        return;
    }
}

void AdventurelandDarkness(void)
{
    if ((Rooms[MyLoc].Image & 128) == 128)
        SetDark();
    else
        SetLight();
}

void AdventurelandAction(int p)
{
    int image = 0;
    switch (p) {
    case 1:
        /* BOOM */
        image = 36;
        break;
    case 2:
        /* Genie */
        image = 34;
        break;
    case 3:
        /* Dragon */
        image = 33;
        break;
    case 4:
        /* White Ox */
        image = 35;
        break;
    default:
        fprintf(stderr, "Adventureland: Unsupported action parameter %d!\n", p);
        return;
    }
    DrawImage(image);
    Output("\n");
    Output(sys[HIT_ENTER]);
    showing_closeup = 1;
    HitEnter();
    return;
}

/* Map system_messages[src_offset + i] to sys[keys[i]] for each of the
   n keys. Each port stores its system messages in its own order, so most
   of them need a key table. */
void MapSysMessages(const SysMessageType *keys, size_t n, int src_offset)
{
    for (size_t i = 0; i < n; i++)
        sys[keys[i]] = system_messages[src_offset + i];
}

/* Map the consecutive sys[] slots first to last to the consecutive
   system_messages[] starting at index src. */
void MapSysRange(SysMessageType first, SysMessageType last, int src)
{
    for (int i = first; i <= last; i++)
        sys[i] = system_messages[src - first + i];
}

static void CopyWordList(const char **dest, const char **src, int count)
{
    if (src == NULL)
        return;
    for (int i = 0; i < count; i++)
        dest[i] = src[i];
}

/* Switch the parser's word lists to those of another language.
   Lists passed as NULL are left as they are. */
void SetParserWordLists(const char **directions, const char **skip_list,
    const char **delimiters, const char **extra_commands, const char **extra_nouns)
{
    CopyWordList(Directions, directions, NUMBER_OF_DIRECTIONS);
    CopyWordList(SkipList, skip_list, NUMBER_OF_SKIPPABLE_WORDS);
    CopyWordList(DelimiterList, delimiters, NUMBER_OF_DELIMITERS);
    CopyWordList(ExtraCommands, extra_commands, NUMBER_OF_EXTRA_COMMANDS);
    CopyWordList(ExtraNouns, extra_nouns, NUMBER_OF_EXTRA_NOUNS);
}

/* The system message order shared by the C64 ports of Adventureland,
   Secret Mission and the Mysterious Adventures. */
static const SysMessageType c64_sysmess_keys[] = {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    EXITS,
    YOU_SEE,
    YOU_ARE,
    TOO_DARK_TO_SEE,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    YOU_CANT_GO_THAT_WAY,
    OK,
    WHAT_NOW,
    HUH,
    YOU_HAVENT_GOT_IT,
    INVENTORY,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    DIRECTION,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    PLAY_AGAIN,
    RESUME_A_SAVED_GAME,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    YOU_CANT_DO_THAT_YET,
    I_DONT_UNDERSTAND,
    NOTHING,
    YOUVE_SOLVED_IT
};

void Spiderman64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        DROPPED,
        TAKEN,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

/* The text-only Atari 8-bit 16K port. Its interpreter says "OK." where the
   others say "Dropped." and "Taken.", and it has no "<HIT ENTER>". */
void SpidermanAtari8Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        NONE, /* "TAPE ERROR" */
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        INVENTORY,
        I_DONT_UNDERSTAND,
        NOTHING,
        PLAY_AGAIN,
        YOUVE_SOLVED_IT
    };

    const char *none = sys[NONE];

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[NONE] = none;
    /* This string follows the unterminated direction letters "NSEWUD" */
    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    /* A carriage return ends the room description in the original */
    sys[YOU_SEE] = "\nI can see:";
    sys[DROPPED] = sys[OK];
    sys[TAKEN] = sys[OK];
}

void Adventureland64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Claymorgue64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Mysterious64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);
    /* Here YOUVE_SOLVED_IT is the 40th message, not the 39th */
    sys[YOUVE_SOLVED_IT] = system_messages[39];

    sys[ITEM_DELIMITER] = " - ";
    sys[MESSAGE_DELIMITER] = "\n";

    sys[YOU_SEE] = "\nThings I can see:\n";

    sys[I_DONT_KNOW_HOW_TO] = "\"";
    sys[PLAY_AGAIN] = "The game is over, thanks for playing\nWant to play again ? ";

    char *dictword = NULL;
    size_t len = GameHeader.WordLength;
    for (int i = 1; i <= 6; i++) {
        dictword = MemAlloc(len + 1);
        strncpy(dictword, sys[i - 1], GameHeader.WordLength);
        dictword[len] = 0;
        Nouns[i] = dictword;
    }

    Nouns[0] = "ANY";

    switch (CurrentGame) {
    case BATON_C64:
        Nouns[79] = "CAST";
        Verbs[79] = ".";
        GameHeader.NumWords = 79;
        break;
    case TIME_MACHINE_C64:
        Verbs[86] = ".";
        break;
    case ARROW1_C64:
    case PERSEUS_C64:
        Nouns[82] = ".";
        break;
    case ARROW2_C64:
        Verbs[80] = ".";
        break;
    case PULSAR7_C64:
        Nouns[102] = ".";
        break;
    case CIRCUS_C64:
        Nouns[96] = ".";
        break;
    case FEASIBILITY_C64:
        Nouns[80] = ".";
        break;
    default:
        break;
    }
}

void PerseusItalianSysmess(void)
{
    sys[YOU_ARE] = "Sono in ";
    sys[YOU_SEE] = "\nQui posso vedere:\n";
    sys[INVENTORY] = "Ho raccolto: ";
}

void Supergran64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        TAKEN,
        DROPPED,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DIRECTION,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_WHAT_A] = "\"";
    sys[IS] = "\" is a word I don't know. ";
}

void SecretMission64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
}

/* Draw a close-up image and wait for a key press. The room image is
   redrawn after that. */
void ShowCloseup(int image)
{
    DrawImage(image);
    showing_closeup = 1;
    Output(sys[HIT_ENTER]);
    HitEnter();
}

void ShowUSCloseup(int image, int offset) {
    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        if (DrawUSRoom(offset + image)) {
            showing_closeup = 1;
            DrawImageOrVector();
            Output(sys[HIT_ENTER]);
            HitEnter();
        }
    }
}

void VoodooShowImageOnExamineUS(int noun)
{
    int image = -1;
    switch (noun) {
    case 8: // Count Cristo
        if (Items[27].Location == MyLoc)
            image = 11;
        break;
    case 13: // Broken sword
        if (IsPresent(33))
            image = 0;
        break;
    case 55: // Voodoo doll
        if (IsPresent(44))
            image = 1;
        break;
    case 63: // Voodoo book
        if (IsPresent(52))
            image = 2;
        break;
    case 32: // Ring
        if (IsPresent(25))
            image = 3;
        break;
    case 43: // Ju-ju man statue
        if (IsPresent(53))
            image = 4;
        break;
    case 9: // Glowing idol
        if (IsPresent(9))
            image = 5;
        if (IsPresent(43))
            image = 10;
        break;
    case 42: // Chemicals
        if (IsPresent(38))
            image = 6;
        break;
    case 7: // Bloody knife
        if (IsPresent(0))
            image = 7;

    default:
        break;
    }

    ShowUSCloseup(image, 90);
}

void CountShowImageOnExamineUS(int noun)
{
    int image = -1;
    switch (noun) {
    case 21: // Package
        if (IsPresent(45))
            image = 0;
        break;
    case 50: // Crowd
        if (Items[50].Location == MyLoc)
            image = 1;
        break;
    default:
        break;
    }

    ShowUSCloseup(image, 90);
}

void AdventurelandShowImageOnExamineUS(int noun) {
    int image = -1;
    switch (noun) {
        case 59: /* Firestone */
            if (IsPresent(56) || Items[0].Location == MyLoc)
                image = 1;
            break;
        case 23: /* Bees */
            if (IsPresent(24) || IsPresent(26))
                image = 2;
            break;
        case 26: /* Flint and steel */
            if (IsPresent(28))
                image = 3;
            break;
        case 54: /* Stream of lava */
            if (Items[34].Location == MyLoc)
                image = 4;
            break;
        case 10: /* Magic mirror */
            if (IsPresent(38))
                image = 5;
            break;
        case 46: /* Chiggers */
            if (IsPresent(42))
                image = 6;
            break;
        case 43: /* Jewelled Fruit */
            if (IsPresent(46))
                image = 7;
            break;
        case 44: /* Blue Ox */
            if (IsPresent(47))
                image = 8;
            break;
        case 39: /* Dragon */
            if (Items[27].Location == MyLoc)
                image = 9;
            break;
        default:
            break;
    }

    ShowUSCloseup(image, 80);
}

void PirateShowImageOnExamineUS(int noun) {
    int image = -1;
    switch (noun) {
        case 34: /* Plans */
            if (IsPresent(29))
                image = 1;
            break;
        case 10: /* Book */
            if (IsPresent(3))
                image = 2;
            break;
        case 66: /* Stamps */
            if (IsPresent(50))
                image = 3;
            break;
        case 38: /* Coffin */
            if (IsPresent(13))
                image = 4;
            break;
        case 29: /* DUBLOONS */
            if (IsPresent(22))
                image = 5;
            break;
        case 53: /* Map */
            if (IsPresent(45))
                image = 6;
            break;
        case 39: /* Parrot */
            if (IsPresent(24))
                image = 7;
            break;
        default:
            break;
    }

    ShowUSCloseup(image, 80);
}

void MissionShowImageOnExamineUS(int noun) {
    int image = -1;
    switch (noun) {
        case 14: // "Picture"
            if (IsPresent(1)) {
                image = 1; // Security
            } else if (IsPresent(41)) {
                image = 3; // Maintenance
            } else if (IsPresent(7)) {
                image = 2; // Visitor
            }
            break;
        case 7: // Recorder
            if (IsPresent(3))
                image = 4;
            break;
        case 29: // Bomb
            if (IsPresent(28))
                image = 5;
            break;
        default:
            break;
    }

    ShowUSCloseup(image, 80);
}

void StrangeShowImageOnExamineUS(int noun) {
    int image = -1;
    switch (noun) {
        case 22: // Phaser
            if (IsPresent(10) || IsPresent(11))
                image = 1;
            break;
        case 30: // Control Console
            if (Items[5].Location == MyLoc)
                image = 2;
            break;
        case 41: // Small piece of plastic flush in the wall
            if (Items[25].Location == MyLoc)
                image = 3;
            break;
        case 69: /* Sculpture */
            if (Items[44].Location == 0 && IsPresent(50))
                image = 4;
            break;
        case 75: /* Goggles */
            if (IsPresent(51))
                image = 5;
            break;
        case 58: /* Belt */
            if (Items[44].Location == CARRIED || Items[44].Location == MyLoc)
                image = 6;
            break;
        case 8: /* Hound */
            if (Items[30].Location == MyLoc || IsPresent(31))
                image = 7;
            break;
        case 56: /* Ice Diamond */
            if (IsPresent(43))
                image = 8;
            break;
        default:
            break;
    }

    ShowUSCloseup(image, 80);
}
