//
//  actions.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  The condition/action bytecode interpreter: running the automatic
//  (status) table and the player command table, and the implementations
//  of the individual action opcodes.
//

#include <stdio.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "animations.h"
#include "gameload.h"
#include "graphics.h"
#include "irmak.h"
#include "parseinput.h"
#include "player.h"
#include "randomness.h"
#include "restorestate.h"
#include "textoutput.h"
#include "ui.h"

#include "actions.h"

int ActionsExecuted;      /* Whether any action matched this turn */
int FoundVerb = 0;        /* Current input matched a verb in the action table */
int FoundNoun = 0;        /* Current input matched a noun in the action table */
int RecursionGuard = 0;   /* Set while QUIT is running, to avoid re-entry */
static int DeferredGoto = 0;

#ifdef DEBUG

/*
 *	Debugging
 */
static unsigned char WordMap[256][5];

static const char *Condition[] = {
    "<ERROR>",
    "AT",
    "NOTAT",
    "ATGT",
    "ATLT",
    "PRESENT",
    "HERE",
    "ABSENT",
    "NOTHERE",
    "CARRIED",
    "NOTCARRIED",
    "WORN",
    "NOTWORN",
    "NODESTROYED",
    "DESTROYED",
    "ZERO",
    "NOTZERO",
    "WORD1",
    "WORD2",
    "WORD3",
    "CHANCE",
    "LT",
    "GT",
    "EQ",
    "NE",
    "OBJECTAT",
    "COND26",
    "COND27",
    "COND28",
    "COND29",
    "COND30",
    "COND31",
};

static const char *Action[] = {
    "<ERROR>",
    "LOAD?",
    "QUIT",
    "INVENTORY",
    "ANYKEY",
    "SAVE",
    "DROPALL",
    "LOOK",
    "OK", /* Guess */
    "GET",
    "DROP",
    "GOTO",
    "GOBY",
    "SET",
    "CLEAR",
    "MESSAGE",
    "CREATE",
    "DESTROY",
    "PRINT",
    "DELAY",
    "WEAR",
    "REMOVE",
    "LET",
    "ADD",
    "SUB",
    "PUT", /* ?? */
    "SWAP",
    "SWAPF",
    "MEANS",
    "PUTWITH",
    "BEEP", /* Rebel Planet at least */
    "REFRESH?",
    "RAMSAVE",
    "RAMLOAD",
    "CLSLOW?",
    "OOPS",
    "DIAGNOSE",
    "SWITCHINVENTORY",
    "SWITCHCHARACTER",
    "CONTINUE",
    "IMAGE",
    "ACT41",
    "ACT42",
    "ACT43",
    "ACT44",
    "ACT45",
    "ACT46",
    "ACT47",
    "ACT48",
    "ACT49",
    "ACT50",
};

static void LoadWordTable(void)
{
    unsigned char *p = FileImage + VerbBase;

    while (1) {
        if (p[4] == 255)
            break;
        if (WordMap[p[4]][0] == 0)
            memcpy(WordMap[p[4]], p, 4);
        p += 5;
    }
}

static void PrintWord(unsigned char word)
{
    if (word == WORD_WILDCARD)
        fprintf(stderr, "*	  ");
    else if (word == 0 || WordMap[word][0] == 0)
        fprintf(stderr, "%-4d ", word);
    else {
        fprintf(stderr, "%c%c%c%c ",
            WordMap[word][0],
            WordMap[word][1],
            WordMap[word][2],
            WordMap[word][3]);
    }
}

/* Set up the debug-trace tables once the game has been identified. */
void InitActionDebugging(void)
{
    /* GOBY prints from the secondary message table except in Blizzard Pass
       and Questprobe 3. */
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
}

#endif

/* Questprobe 3 uses different opcode numbering for conditions and actions.
   These tables map QP3 opcodes to the standard enum values. */
static const unsigned char Q3Condition[] = {
    CONDITIONERROR,
    AT,
    NOTAT,
    ATGT,
    ATLT,
    PRESENT,
    ABSENT,
    CARRIED,
    NOTCARRIED,
    NODESTROYED,
    DESTROYED,
    ZERO,
    NOTZERO,
    WORD1,
    WORD2,
    CHANCE,
    LT,
    GT,
    EQ,
    OBJECTAT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const unsigned char Q3Action[] = {
    ACTIONERROR,
    SWITCHINVENTORY, /* Swap inventory and dark flag */
    DIAGNOSE, /* Print Reed Richards' watch status message */
    LOADPROMPT,
    QUIT,
    SHOWINVENTORY,
    ANYKEY,
    SAVE,
    DONE, /* Set "condition failed" flag, Flag[118], to 1 */
    GET,
    DROP,
    GOTO,
    SWITCHCHARACTER, /* Go to the location of the other guy */
    SET,
    CLEAR,
    MESSAGE,
    CREATE,
    DESTROY,
    LET,
    ADD,
    SUB,
    PUT,
    SWAP,
    IMAGE, /* Draw image on top of room image */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static int Chance(int n)
{
    return (erkyrath_random() % 100 <= n);
}

static int LoadPrompt(void)
{
    OutCaps();
    glk_window_clear(Bottom);
    SysMessage(RESUME_A_SAVED_GAME);
    OutFlush();

    if (!YesOrNo()) {
        glk_window_clear(Bottom);
        if (DeferredGoto == 1) {
            MyLoc = 1;
            DeferredGoto = 0;
        }
        return 0;
    } else {
        return LoadGame();
    }
}

static void QuitGame(void)
{
    if (LastChar == '\n')
        OutReplace(' ');
    OutFlush();
    Look();
    OutCaps();
    SysMessage(PLAY_AGAIN);
    OutFlush();
    if (YesOrNo()) {
        ShouldRestart = 1;
        StopTime = 2;
        return;
    } else {
        glk_exit();
    }
}

static void AnyKey(void)
{
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

void Okay(void)
{
    SysMessage(OKAY);
    OutChar(' ');
    OutCaps();
    PrintedOK = 1;
}

/* Pick up all objects in the current room starting from object index start. */
static void TakeAll(int start)
{
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        return;
    }
    int found = 0;
    for (int i = start; i < NumObjects(); i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (found)
                OutChar('\n');
            found = 1;
            PrintObject(i);
            OutReplace(0);
            OutString("......");
            if (CarryItem() == 0) {
                SysMessage(YOURE_CARRYING_TOO_MUCH);
                return;
            }
            OutKillSpace();
            OutString("Taken");
            OutFlush();
            Put(i, Carried());
        }
    }
    if (!found) {
        Message(31);
    }
}

static void DropAll(int loud)
{
    int i;
    int found = 0;
    for (i = 0; i < NumObjects(); i++) {
        if (ObjectLoc[i] == Carried() && ObjectLoc[i] != Worn()) {
            if (loud) {
                if (found)
                    OutChar('\n');
                found = 1;
                PrintObject(i);
                OutReplace(0);
                OutString("......");
                OutKillSpace();
                OutString("Dropped");
                OutFlush();
            }
            Put(i, MyLoc);
        }
    }
    if (loud & !found) {
        OutString("You have nothing to drop. ");
    }
    ItemsCarried = 0;
}

static int GetObject(unsigned char obj)
{
    if (ObjectLoc[obj] == Carried() || ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_HAVE_IT);
        return 0;
    }
    if (!(Version == QUESTPROBE3_TYPE && Flag[1] == MyLoc && ObjectLoc[obj] == Flag[3])) {
        if (ObjectLoc[obj] != MyLoc) {
            SysMessage(YOU_DONT_SEE_IT);
            return 0;
        }
    }
    if (CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return 0;
    }

    Put(obj, Carried());
    return 1;
}

static int DropObject(unsigned char obj)
{
    /* FIXME: check if this is how the real game behaves */
    if (ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return 0;
    }
    if (ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return 0;
    }

    DropItem();
    Put(obj, MyLoc);
    return 1;
}

void Goto(unsigned char destination)
{
    if (BaseGame == QUESTPROBE3 && !PrintedOK)
        Okay();
    if (BaseGame == HEMAN && MyLoc == 0 && destination == 1) {
        DeferredGoto = 1;
    } else {
        MyLoc = destination;
        Redraw = 1;
    }
}

static void Delay(unsigned char seconds)
{
    OutChar(' ');
    OutFlush();

    if (Options & NO_DELAYS)
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    glk_request_timer_events(1000 * seconds);

    event_t ev;

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(AnimationRunning);
}

static void Wear(unsigned char obj)
{
    if (ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return;
    }
    if (ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return;
    }
    DropItem();
    Put(obj, Worn());
}

static void Remove(unsigned char obj)
{
    if (ObjectLoc[obj] != Worn()) {
        SysMessage(YOU_ARE_NOT_WEARING_IT);
        return;
    }
    if (CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return;
    }
    Put(obj, Carried());
}

/* Replace the current input words — used by action scripts to redirect
   a command to different verb/noun handling. */
static void Means(unsigned char verb, unsigned char noun)
{
    Word[0] = verb;
    Word[1] = noun;
}

static void Q3SwitchInvFlags(unsigned char a, unsigned char b)
{
    if (Flag[2] == a) {
        Flag[2] = b;
        Flag[3] = a;
    }
}

/* Questprobe 3 per-turn bookkeeping: track the other character's location,
   handle Xandu's presence flag, manage Reed Richards' watch, and
   update turn/asphyxiation counters. */
static void Q3UpdateFlags(void)
{
    if (ObjectLoc[7] == LOC_INV_TORCH)
        ObjectLoc[7] = LOC_INV_THING;
    if (IsThing) {
        if (ObjectLoc[2] == LOC_DESTROYED) {
            /* If the "holding HUMAN TORCH by the hands" object is destroyed (i.e. not held) */
            /* the "location of the other guy" flag is set to the location of the Human Torch object */
            OtherGuyLoc = ObjectLoc[18];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(LOC_INV_TORCH, LOC_INV_THING);
    } else { /* I'm the HUMAN TORCH */
        if (ObjectLoc[1] == LOC_DESTROYED) {
            /* If the "holding THING by the hands" object is destroyed (i.e. not held) */
            /* The "location of the other guy" flag is set to the location of the Thing object */
            OtherGuyLoc = ObjectLoc[17];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(LOC_INV_THING, LOC_INV_TORCH);
    }

    /* Reset flag 39 when Xandu is knocked out */
    if (ObjectLoc[33] != 22)
        Flag[39] = 0;
    /* And set it when he is present */
    else if (Present(33))
        Flag[39] = 1;

    /* Make sure that:
     - The watch isn't carried in the "intro"
     - That Thing has it when the game starts,
     no matter who we begin as
     */
    if (!Q3SwitchedWatch) {
        if (MyLoc == 6 || MyLoc == 0) {
            ObjectLoc[37] = LOC_DESTROYED;
        } else {
            ObjectLoc[37] = LOC_INV_THING;
            Q3SwitchedWatch = 1;
        }
    }

    if (DrawImages)
        return;

    TurnsLow++; /* Turns played % 100 */
    if (TurnsLow == 100) {
        TurnsHigh++; /* Turns divided by 100 */
        TurnsLow = 0;
    }

    ThingAsphyx++; // Turns since Thing started holding breath
    if (ThingAsphyx == 0)
        ThingAsphyx = 0xff;
    TorchAsphyx++; // Turns since Torch started holding breath
    if (TorchAsphyx == 0)
        TorchAsphyx = 0xff;
}

/* Questprobe 3 numbers the flags differently, so we have to offset them by 4 */
static void Q3AdjustConditions(unsigned char op, unsigned char *arg1)
{
    switch (op) {
    case ZERO:
    case NOTZERO:
    case LT:
    case GT:
    case EQ:
    case NE:
        *arg1 += 4;
        break;
    default:
        break;
    }
}

static void Q3AdjustActions(unsigned char op, unsigned char *arg1, unsigned char *arg2)
{
    switch (op) {
    case SET:
    case CLEAR:
    case LET:
    case ADD:
    case SUB:
        if (arg1 != NULL)
            *arg1 += 4;
        break;
    case SWAPF:
        if (arg1 != NULL)
            *arg1 += 4;
        if (arg2 != NULL)
            *arg2 += 4;
        break;
    default:
        break;
    }
}

inline static int TwoConditionParameters(void)
{
    return Version == QUESTPROBE3_TYPE ? 16 : 21;
}

inline static int TwoActionParameters(void)
{
    return Version == QUESTPROBE3_TYPE ? 18 : 22;
}

/* Evaluate a single condition opcode (already translated to the canonical
   numbering). Returns nonzero if the condition holds. */
static int ConditionHolds(unsigned char op, unsigned char arg1, unsigned char arg2)
{
    switch (op) {
    case AT:
        return MyLoc == arg1;
    case NOTAT:
        return MyLoc != arg1;
    case ATGT:
        return MyLoc > arg1;
    case ATLT:
        return MyLoc < arg1;
    case PRESENT:
        return Present(arg1);
    case HERE:
        return ObjectLoc[arg1] == MyLoc;
    case ABSENT:
        return !Present(arg1);
    case NOTHERE:
        return ObjectLoc[arg1] != MyLoc;
    case CARRIED:
        return ObjectLoc[arg1] == Carried() || ObjectLoc[arg1] == Worn();
    case NOTCARRIED:
        return ObjectLoc[arg1] != Carried() && ObjectLoc[arg1] != Worn();
    case WORN:
        return ObjectLoc[arg1] == Worn();
    case NOTWORN:
        return ObjectLoc[arg1] != Worn();
    case NODESTROYED:
        return ObjectLoc[arg1] != Destroyed();
    case DESTROYED:
        return ObjectLoc[arg1] == Destroyed();
    case ZERO:
        if (BaseGame == TEMPLE_OF_TERROR) {
            /* Unless we have kicked sand in the eyes of the guard, tracked by flag 63,
             make sure they kill us if we try to pass, by setting flag 28 to zero */
            /* This fixes a bug in the original game, which lets us just walk
               past the guard */
            if (arg1 == 28 && Flag[63] == 0 && Word[0] == 20 && Word[1] == 162)
                Flag[28] = 0;
        }
        return Flag[arg1] == 0;
    case NOTZERO:
        return Flag[arg1] != 0;
    /* WORD1/2/3 match against the third/fourth/fifth parsed input
       words (Word[0] and Word[1] are the verb and primary noun). */
    case WORD1:
        return Word[2] == arg1;
    case WORD2:
        return Word[3] == arg1;
    case WORD3:
        return Word[4] == arg1;
    case CHANCE:
        return Chance(arg1);
    case LT:
        return Flag[arg1] < arg2;
    case GT:
        return Flag[arg1] > arg2;
    case EQ:
        /* Fix final puzzle (Flag 12 conflict) */
        if (BaseGame == TEMPLE_OF_TERROR) {
            if (arg1 == 12 && arg2 == 4)
                arg1 = 60;
        }
        return Flag[arg1] == arg2;
    case NE:
        return Flag[arg1] != arg2;
    case OBJECTAT:
        return ObjectLoc[arg1] == arg2;
    default:
        fprintf(stderr, "Unknown condition %d.\n",
            op);
        return 0;
    }
}

/* Perform a single action opcode (already translated to the canonical
   numbering). May set *done to stop further table scanning. Returns
   nonzero if the rest of the line must be abandoned immediately. */
static int PerformAction(unsigned char op, unsigned char arg1, unsigned char arg2, int *done)
{
    int tmp;

    switch (op) {
    case LOADPROMPT:
        if (LoadPrompt()) {
            *done = 1;
            return 1;
        }
        break;
    case QUIT:
        if (!RecursionGuard) {
            RecursionGuard = 1;
            QuitGame();
        }
        *done = 1;
        return 1;
    case SHOWINVENTORY:
        Inventory();
        break;
    case ANYKEY:
        AnyKey();
        break;
    case SAVE:
        StopTime = 1;
        SaveGame();
        break;
    case DROPALL:
        if ((BaseGame == REBEL_PLANET && (Word[0] != 20 || Word[1] != 141)) ||
            (BaseGame == KAYLETH && (Word[0] != 20 || Word[1] != 254)))
            DropAll(0);
        else
            DropAll(1);
        break;
    case LOOK:
        Look();
        break;
    case PRINTOK:
        /* Guess */
        Okay();
        break;
    case GET:
        if (GetObject(arg1) == 0 && Version == QUESTPROBE3_TYPE)
            *done = 1;
        break;
    case DROP:
        if (DropObject(arg1) == 0 && BaseGame == REBEL_PLANET) {
            *done = 1;
            return 1;
        }
        break;
    case GOTO:
        /*
             He-Man moves the the player to a special "By the power of Grayskull" room
             and then issues an undo to return to the previous room.
        */
        if (BaseGame == HEMAN && arg1 == 83)
            SaveUndo();
        Goto(arg1);
        break;
    case GOBY:
        /* Blizzard pass era */
        if (Version == BLIZZARD_PASS_TYPE)
            Goto(ObjectLoc[arg1]);
        else
            Message2(arg1);
        break;
    case SET:
        Flag[arg1] = 255;
        break;
    case CLEAR:
        Flag[arg1] = 0;
        break;
    case MESSAGE:
        /* Prevent repeated "Blob returns to his post" messages */
        if (CurrentGame == QUESTPROBE3_64 && arg1 == 44)
            Flag[59] = 0;
        Message(arg1);
        if (CurrentGame == BLIZZARD_PASS && arg1 != 160)
            OutChar('\n');
        break;
    case CREATE:
        Put(arg1, MyLoc);
        break;
    case DESTROY:
        Put(arg1, Destroyed());
        break;
    case PRINT:
        PrintNumber(Flag[arg1]);
        break;
    case DELAY:
        Delay(arg1);
        break;
    case WEAR:
        Wear(arg1);
        break;
    case REMOVE:
        Remove(arg1);
        break;
    case LET:
        if (BaseGame == TEMPLE_OF_TERROR) {
            if (arg1 == 28 && arg2 == 2) {
                /* If the serpent guard is present, we have just kicked sand in his eyes. Set flag 63 to track this */
                Flag[63] = (ObjectLoc[48] == MyLoc);
            }
        }
        Flag[arg1] = arg2;
        break;
    case ADD:
        /* Fix final puzzle (Flag 12 conflict) */
        if (BaseGame == TEMPLE_OF_TERROR) {
            if (arg1 == 12 && arg2 == 1)
                arg1 = 60;
        }
        tmp = Flag[arg1] + arg2;
        if (tmp > 255)
            tmp = 255;
        Flag[arg1] = tmp;
        break;
    case SUB:
        tmp = Flag[arg1] - arg2;
        if (tmp < 0)
            tmp = 0;
        Flag[arg1] = tmp;
        break;
    case PUT:
        Put(arg1, arg2);
        break;
    case SWAP:
        tmp = ObjectLoc[arg1];
        Put(arg1, ObjectLoc[arg2]);
        Put(arg2, tmp);
        break;
    case SWAPF:
        tmp = Flag[arg1];
        Flag[arg1] = Flag[arg2];
        Flag[arg2] = tmp;
        break;
    case MEANS:
        Means(arg1, arg2);
        break;
    case PUTWITH:
        Put(arg1, ObjectLoc[arg2]);
        break;
    case BEEP:
        /* The interpreter's Beep routine (Rebel Planet @0x6AD7) builds the
         * ROM beeper (0x03B5) inputs from the two operands, then JP 0x03B5.
         * arg1 is the duration operand, arg2 the pitch operand:
         *   DE (cycles)      = (duration * 4) & 0x1FF
         *   HL (half-period) = pitch * DE   (16x16 multiply at 0x30A9)
         * The & 0x1FF is not a tidy-up: the routine forms duration*4 with
         * two ADD A,A and a single RL D, so only bit 6 of the duration
         * carries into the high byte (it keeps just the low 9 bits). Without
         * it, durations >= 64 (e.g. 0xFF) overshoot badly. f = 437500 /
         * (HL+30.125) Hz for DE cycles. Reverse-engineered in MAME: PRESS IH
         * in Rebel Planet gives arg1=0xFF, arg2=2 -> DE=508, HL=1016
         * (418 Hz) -> ~1.2 s, matching the original. (The old arg1-as-HL
         * code made a ~3 ms click; a naive duration*4 made a ~4.8 s drone.) */
#if defined(GLK_MODULE_GARGLKBLEEP)
        garglk_zbleep(1 + (arg1 == 250));
#elif defined(SPATTERLIGHT)
        {
            int de = (arg1 * 4) & 0x1FF;
            win_beep_zx((arg2 * de) & 0xFFFF, de);
        }
#else
        putchar('\007');
        fflush(stdout);
#endif
        break;
    case REFRESH:
        if (BaseGame == KAYLETH)
            TakeAll(78);
        if (BaseGame == HEMAN)
            TakeAll(45);
        Redraw = 1;
        break;
    case RAMSAVE:
        RamSave(1);
        break;
    case RAMLOAD:
        RamLoad();
        break;
    case CLSLOW:
        OutFlush();
        glk_window_clear(Bottom);
        break;
    case OOPS:
        RestoreUndo(0);
        Redraw = 1;
        break;
    case DIAGNOSE:
        Message(223);
        char buf[5];
        char *q = buf;
        /* TurnsLow = turns % 100, TurnsHigh == turns / 100 */
        snprintf(buf, sizeof buf, "%04d", TurnsLow + TurnsHigh * 100);
        while (*q)
            OutChar(*q++);
        SysMessage(14);
        if (IsThing)
            /* THING is always 100 percent rested */
            OutString("100");
        else {
            /* Calculate "restedness" percentage */
            /* Flag[7] == 80 means 100 percent rested */
            q = buf;
            snprintf(buf, sizeof buf, "%d", (Flag[7] >> 2) + Flag[7]);
            while (*q)
                OutChar(*q++);
        }
        SysMessage(15);
        break;
    case SWITCHINVENTORY: {
        /* Swap "current" and "other character" state: inventory code
           (Flag[2]) plus the two dark flags (Flag[42], Flag[43]). */
        uint8_t temp = Flag[2];
        Flag[2] = OtherGuyInv;
        OtherGuyInv = temp;
        temp = Flag[42];
        Flag[42] = Flag[43];
        Flag[43] = temp;
        Redraw = 1;
        break;
    }
    case SWITCHCHARACTER:
        /* Go to the location of the other guy */
        MyLoc = ObjectLoc[arg1];
        /* Pick him up, so that you don't see yourself */
        GetObject(arg1);
        Redraw = 1;
        break;
    case DONE:
        *done = 1;
        break;
    case IMAGE:
        if (!TAYLOR_GRAPHICS_ENABLED)
            break;
        /* Room 3 is below the tar. The original draws the tar pit as
           seen from above there. */
        if (MyLoc == 3 || Flag[DarkFlag()]) {
            DrawBlack();
            break;
        }
        if (arg1 == 0) {
            ClearGraphMem();
            DrawPictureNumber(MyLoc - 1, 1);
        } else if (arg1 == 45 && ObjectLoc[48] != MyLoc) {
            /* A bug in the original: it draws the toppled statue
               while Xandu still stands. */
            break;
        } else {
            DrawPictureNumber(arg1 - 1, 1);
        }
        DrawIrmakPictureFromBuffer();
        break;
    default:
        fprintf(stderr, "Unknown command %d.\n", op);
        break;
    }
    return 0;
}

/* Execute one action line: evaluate conditions (bytes with bit 7 clear),
   and if all pass, run the action commands (bytes with bit 7 set).
   Bit 6 on an action byte sets *done to stop further table scanning. */
static void ExecuteLineCode(unsigned char *code, int *done)
{
    unsigned char arg1 = 0, arg2 = 0;

    /* Phase 1: read condition bytes until we see one with the action bit
       set (which signals the start of the action stream for this line). */
    while (!(*code & ACTION_BIT)) {
        unsigned char op = *code++;
        arg1 = *code++;

#ifdef DEBUG
        if (Version == QUESTPROBE3_TYPE) {
            unsigned char debugarg1 = arg1;
            Q3AdjustConditions(Q3Condition[op], &debugarg1);
            fprintf(stderr, "%s %d ", Condition[Q3Condition[op]], debugarg1);
        } else {
            fprintf(stderr, "%s %d ", Condition[op], arg1);
        }
#endif
        /* Conditions at or above this opcode index take a second argument. */
        if (op >= TwoConditionParameters()) {
            arg2 = *code++;
#ifdef DEBUG
            fprintf(stderr, "%d ", arg2);
#endif
        }

        /* QP3 uses a different opcode numbering — translate to the
           canonical set and adjust arg1 to match. */
        if (Version == QUESTPROBE3_TYPE) {
            op = Q3Condition[op];
            Q3AdjustConditions(op, &arg1);
        }

        if (!ConditionHolds(op, arg1, arg2)) {
#ifdef DEBUG
            fprintf(stderr, "\n");
#endif
            /* Condition failed (or unknown): abandon this line and let the
               caller advance to the next one. */
            return;
        }
    }

    /* Phase 2: all conditions passed — record that we did something this
       turn and execute the action bytes until we see one without the
       action bit (i.e. the next line's conditions) or run off the end. */
    ActionsExecuted = 1;

    while (*code & ACTION_BIT) {
        unsigned char op = *code++;

#ifdef DEBUG
        if (op & ACTION_DONE_BIT)
            fprintf(stderr, "DONE:");
        if (Version == QUESTPROBE3_TYPE)
            fprintf(stderr, "%s(%d) ", Action[Q3Action[op & ACTION_OP_MASK]], op & ACTION_OP_MASK);
        else
            fprintf(stderr, "%s(%d) ", Action[op & ACTION_OP_MASK], op & ACTION_OP_MASK);
#endif

        /* Bit 6 ("DONE") asks the table scanner to stop after this line. */
        if (op & ACTION_DONE_BIT)
            *done = 1;
        op &= ACTION_OP_MASK;

        /* Opcodes 0-8 take no arguments; 9+ take at least one. */
        if (op > 8) {
            arg1 = *code++;
#ifdef DEBUG
            unsigned char debugarg1 = arg1;
            if (Version == QUESTPROBE3_TYPE)
                Q3AdjustActions(Q3Action[op], &debugarg1, NULL);
            fprintf(stderr, "%d ", debugarg1);
#endif
        }
        /* Actions at or above this opcode index take a second argument. */
        if (op >= TwoActionParameters()) {
            arg2 = *code++;
#ifdef DEBUG
            unsigned char debugarg2 = arg2;
            if (Version == QUESTPROBE3_TYPE)
                Q3AdjustActions(Q3Action[op], NULL, &debugarg2);
            fprintf(stderr, "%d ", debugarg2);
#endif
        }

        /* QP3 uses a different action numbering — translate to the
           canonical set and adjust args to match. */
        if (Version == QUESTPROBE3_TYPE) {
            op = Q3Action[op];
            Q3AdjustActions(op, &arg1, &arg2);

            if (!PrintedOK)
                Okay();
        }

        /* Track lighting before the action so we can redraw if the action
           toggles the dark flag (e.g. lighting a lamp in a dark room). */
        int WasDark = Flag[DarkFlag()];

        if (PerformAction(op, arg1, arg2, done))
            return;

        if (WasDark != Flag[DarkFlag()])
            Redraw = 1;
    }
#ifdef DEBUG
    fprintf(stderr, "\n");
#endif
}

/* Advance past the current action line (conditions + actions) to the
   start of the next one. */
static unsigned char *NextLine(unsigned char *ptr)
{
    unsigned char opcode;
    while (!((opcode = *ptr) & ACTION_BIT)) {
        ptr += 2;
        if (opcode >= TwoConditionParameters())
            ptr++;
    }
    while (((opcode = *ptr) & ACTION_BIT)) {
        opcode &= ACTION_OP_MASK;
        ptr++;
        if (opcode > 8)
            ptr++;
        if (opcode >= TwoActionParameters())
            ptr++;
    }
    return ptr;
}

/* Draw two images that are unused in the original game */
void QP3DrawExtraImages(void)
{
    if (!TAYLOR_GRAPHICS_ENABLED)
        return;
    /* There is an unused image of the cannon
     on the road (room 34) but it has the wrong background
     colour.
     */
    if (MyLoc == 34 && ObjectLoc[29] == 34) {
        PatchAndDrawQP3Cannon();
    } else if (MyLoc == 2 && ObjectLoc[17] == 2 && Flag[26] > 16 && Flag[26] < 20) {
        /* Draw close-up of Thing as he sinks */
        DrawPictureNumber(53, 1);
        DrawIrmakPictureFromBuffer();
    }
}

/* Run the automatic (status) action table — these fire every turn
   regardless of player input. Entries are executed until ACTION_TABLE_END
   or an action sets the done flag. */
void RunStatusTable(void)
{
    if (StopTime) {
        StopTime--;
        return;
    }
    unsigned char *ptr = FileImage + StatusBase;

    int done = 0;
    ActionsExecuted = 0;

    if (Version == QUESTPROBE3_TYPE) {
        Q3UpdateFlags();
    }

    while (*ptr != ACTION_TABLE_END) {
        /* QP3 pads the status table with 0x7e bytes between entries. */
        while (Version == QUESTPROBE3_TYPE && *ptr == 0x7e) {
            ptr++;
        }
        ExecuteLineCode(ptr, &done);
        if (done) {
            return;
        }
        ptr = NextLine(ptr);
    }
    if (Version == QUESTPROBE3_TYPE)
        DrawImages = 0;
}

/* Run the player command action table, matching the parsed input words
   against verb/noun entries. WORD_WILDCARD is a wildcard. Entries can
   match as VERB NOUN or NOUN VERB. */
void RunCommandTable(void)
{
    unsigned char *ptr = FileImage + ActionBase;

    int done = 0;
    ActionsExecuted = 0;
    FoundVerb = 0;
    FoundNoun = 0;

    while (*ptr != ACTION_TABLE_END) {

        if (ptr[0] == Word[0] || ptr[0] == Word[1])
            FoundVerb = 1;
        if (ptr[1] == Word[0] || ptr[1] == Word[1])
            FoundNoun = 1;

        /* Match input to table entry as VERB NOUN or NOUN VERB */
        if (((*ptr == WORD_WILDCARD || *ptr == Word[0]) && (ptr[1] == WORD_WILDCARD || ptr[1] == Word[1])) ||
            ((*ptr == WORD_WILDCARD || *ptr == Word[1]) && (ptr[1] == WORD_WILDCARD || ptr[1] == Word[0]))) {
#ifdef DEBUG
            PrintWord(ptr[0]);
            PrintWord(ptr[1]);
#endif
            /* Work around a Questprobe 3 bug – the original game
               can be broken by The Thing picking something up in
               the great room right after entering it. */
            /* We make sure he is ordered out instead. */
            if (Version == QUESTPROBE3_TYPE) {
                /* In great room, Xandu present */
                if (Present(33)) {
                    Flag[39] = 1;
                    Message(24);
                    Goto(26);
                    ActionsExecuted = 1;
                    return;
                }
            }
            ExecuteLineCode(ptr + 2, &done);
            if (done)
                return;
        }
        ptr = NextLine(ptr + 2);
    }
}

