/*
 *  scott_actions.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Action engine: condition evaluation, opcode dispatch, and
 *  main action table.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robinofsherwood.h"
#include "seasofblood.h"

#include "parser.h"
#include "ti99_4a_terp.h"
#include "saga.h"

#include "randomness.h"

#include "scott.h"
#include "scott_actions.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

//#define DEBUG_ACTIONS

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


/* Game state operations — basically helper functions
   for PerformLine() and PerformTI99Line().
   Some of them are general enough to be used by other
   parts of the code as well. */

int CountCarried(void)
{
    int ct = 0;
    int n = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == CARRIED)
            n++;
        ct++;
    }
    return (n);
}

void GameOver(void)
{
    if (split_screen && Top)
        Look();
    Output("\n\n");
    Output(sys[PLAY_AGAIN]);
    Output("\n");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

/* Pause for a given duration using Glk timer events.
 If vector drawing is in progress, waits for it to finish first. */
void Delay(float seconds)
{
    if (Options & NO_DELAYS)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    if (DrawingVector()) {
        do {
            glk_select(&ev);
            Updates(ev);
        } while (DrawingVector());
#ifdef SPATTERLIGHT
        if (gli_slowdraw)
            seconds = 0.5;
#endif
    }

    glk_request_timer_events(1000 * seconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(0);
}

void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    Items[itemA].Location = Items[itemB].Location;
    if (Items[itemB].Location == MyLoc) {
        should_look_in_transcript = 1;
        DrawUSRoomObject(itemB);
        DrawImageOrVector();
    }
}

void GoTo(int loc)
{
#ifdef DEBUG_ACTIONS
    debug_print("player location is now room %d (%s).\n", loc,
                Rooms[loc].Text);
#endif
    int oldloc = MyLoc;
    MyLoc = loc;
    should_look_in_transcript = should_draw_image = 1;
    Look();
    if (oldloc != MyLoc && (Options & FLICKER_ON))
        Delay(0.2);
}

void GoToStoredLoc(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("switch location to stored location (%d) (%s).\n",
        SavedRoom, Rooms[SavedRoom].Text);
#endif
    int t = MyLoc;
    MyLoc = SavedRoom;
    SavedRoom = t;
    should_look_in_transcript = should_draw_image = 1;
}

void SetBitFlag(int bit) {
#ifdef DEBUG_ACTIONS
    debug_print("Bitflag %d is set\n", bit);
#endif
    BitFlags |= (uint64_t)1 << bit;
}

void ClearBitFlag(int bit) {
#ifdef DEBUG_ACTIONS
    debug_print("Bitflag %d is cleared\n", bit);
#endif
    BitFlags &= ~((uint64_t)1 << bit);
}

static void ChangeDarkness(int dark) {
    int was_dark = ItIsDark();
    if (dark) {
        SetBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 0);
    } else {
        ClearBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 1);
    }
    if (should_draw_image == 0) {
        should_draw_image = should_look_in_transcript;
    }
}

void SetDark(void) {
    ChangeDarkness(1);
}

void SetLight(void) {
    ChangeDarkness(0);
}

void SwapLocAndRoomflag(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("swap location<->roomflag[%d]\n", index);
#endif
    int temp = MyLoc;
    MyLoc = RoomSaved[index];
    RoomSaved[index] = temp;
    should_look_in_transcript = should_draw_image = 1;
    Look();
}

void SwapItemLocations(int itemA, int itemB)
{
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
    if (Items[itemA].Location == MyLoc || Items[itemB].Location == MyLoc)
        should_look_in_transcript = should_draw_image = 1;
}

void PutItemAInRoomB(int itemA, int roomB)
{
#ifdef DEBUG_ACTIONS
    debug_print("Item %d (%s) is put in room %d (%s). MyLoc: %d (%s)\n",
        itemA, Items[itemA].Text, roomB, Rooms[roomB].Text, MyLoc,
        Rooms[MyLoc].Text);
#endif
    if (Items[itemA].Location == MyLoc)
        LookWithPause();
    Items[itemA].Location = roomB;
}

void SwapCounters(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print(
        "Select a counter. Current counter is swapped with backup "
        "counter %d\n",
        index);
#endif
    if (index > NUM_COUNTERS - 1) {
        debug_print("ERROR! parameter out of range. Max %d, got %d\n", NUM_COUNTERS - 1, index);
        index = NUM_COUNTERS - 1;
    }
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
#ifdef DEBUG_ACTIONS
    debug_print("Value of new selected counter is %d\n",
        CurrentCounter);
#endif
}

void PlayerIsDead(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("Player is dead\n");
#endif
    Output(sys[IM_DEAD]);
    SetLight();
    should_look_in_transcript = should_draw_image = 1;
    MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
}

static void PrintTakenOrDropped(int index)
{
    Output(sys[index]);
    int length = strlen(sys[index]);
    char last = sys[index][length - 1];
    if (last == '\n' || last == '\r')
        return;
    Output(" ");
    if ((!(CurrentCommand->allflag & LASTALL))
        || split_screen == 0) {
        Output("\n");
    }
}

void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

int RandomPercent(int n)
{
    return erkyrath_random() % 100 < n;
}

void OutputNumber(int a) { Display(Bottom, "%d", a); }

#define RTPI_WIN_IMAGE 26

int PrintScore(void)
{
    int i = 0;
    int n = 0;
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == GameHeader.TreasureRoom && *Items[i].Text == '*')
            n++;
        i++;
    }
    Display(Bottom, "%s %d %s%s %d.\n", sys[IVE_STORED], n, sys[TREASURES],
        sys[ON_A_SCALE_THAT_RATES], (n * 100) / GameHeader.Treasures);
    if (n == GameHeader.Treasures) {
        Output(sys[YOUVE_SOLVED_IT]);
        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            ShowUSCloseup(RTPI_WIN_IMAGE, 0);
        }
        GameOver();
        return 1;
    }
    return 0;
}

void PrintNoun(void)
{
    if (CurrentCommand)
        glk_put_string_stream_uni(glk_window_get_stream(Bottom),
            UnicodeWords[CurrentCommand->nounwordindex]);
}

void PrintMessage(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("Print message %d: \"%s\"\n", index,
        Messages[index]);
#endif
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        Output(message);
        const char lastchar = message[strlen(message) - 1];
        if (lastchar != '\r' && lastchar != '\n')
            Output(sys[MESSAGE_DELIMITER]);
    }
}


/* Evaluate one action line: check up to 5 conditions, and if all pass,
 execute the 4 opcodes encoded in the action's two Opcode words.

 Conditions are encoded as (value * 20 + condition_code). Condition
 code 0 stores a parameter for later use by action opcodes. Opcodes
 1-51 and 102+ print messages; 52-90 are game state operations.

 Returns ACT_FAILURE if a condition fails, ACT_CONTINUE for action 73
 (continue to next line), ACT_GAMEOVER on game-ending actions, or
 ACT_SUCCESS if all opcodes completed normally. */
static ActionResultType PerformLine(int ct)
{
#ifdef DEBUG_ACTIONS
    debug_print("Performing line %d: ", ct);
#endif
    int continuation = 0, dead = 0;
    int param[NUM_CONDITIONS], pptr = 0;
    int p = 0;
    int act[NUM_OPCODES];
    int cc = 0;
    while (cc < NUM_CONDITIONS) {
        int cv, dv;
        cv = Actions[ct].Condition[cc];
        dv = cv / CONDITION_MULTIPLIER;
        cv %= CONDITION_MULTIPLIER;
#ifdef DEBUG_ACTIONS
        debug_print("Testing condition %d: ", cv);
#endif
        switch (cv) {
            case COND_PARAMETER:
                param[pptr++] = dv;
                break;
            case COND_CARRIED:
#ifdef DEBUG_ACTIONS
                debug_print("Does the player carry %s?\n", Items[dv].Text);
#endif
                if (Items[dv].Location != CARRIED)
                    return ACT_FAILURE;
                break;
            case COND_IN_ROOM:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s in location?\n", Items[dv].Text);
#endif
                if (Items[dv].Location != MyLoc)
                    return ACT_FAILURE;
                break;
            case COND_PRESENT:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s held or in location?\n", Items[dv].Text);
#endif
                if (Items[dv].Location != CARRIED && Items[dv].Location != MyLoc)
                    return ACT_FAILURE;
                break;
            case COND_AT_LOC:
#ifdef DEBUG_ACTIONS
                debug_print("Is location %s?\n", Rooms[dv].Text);
#endif
                if (MyLoc != dv)
                    return ACT_FAILURE;
                break;
            case COND_NOT_IN_ROOM:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s NOT in location?\n", Items[dv].Text);
#endif
                if (Items[dv].Location == MyLoc)
                    return ACT_FAILURE;
                break;
            case COND_NOT_CARRIED:
#ifdef DEBUG_ACTIONS
                debug_print("Does the player NOT carry %s?\n", Items[dv].Text);
#endif
                if (Items[dv].Location == CARRIED)
                    return ACT_FAILURE;
                break;
            case COND_NOT_AT_LOC:
#ifdef DEBUG_ACTIONS
                debug_print("Is location NOT %s?\n", Rooms[dv].Text);
#endif
                if (MyLoc == dv)
                    return ACT_FAILURE;
                break;
            case COND_FLAG_SET:
#ifdef DEBUG_ACTIONS
                debug_print("Is bitflag %d set?\n", dv);
#endif
                if ((BitFlags & ((uint64_t)1 << dv)) == 0)
                    return ACT_FAILURE;
                break;
            case COND_FLAG_CLEAR:
#ifdef DEBUG_ACTIONS
                debug_print("Is bitflag %d NOT set?\n", dv);
#endif
                if (BitFlags & ((uint64_t)1 << dv))
                    return ACT_FAILURE;
                break;
            case COND_CARRYING_ANY:
#ifdef DEBUG_ACTIONS
                debug_print("Does the player carry anything?\n");
#endif
                if (CountCarried() == 0)
                    return ACT_FAILURE;
                break;
            case COND_CARRYING_NONE:
#ifdef DEBUG_ACTIONS
                debug_print("Does the player carry nothing?\n");
#endif
                if (CountCarried())
                    return ACT_FAILURE;
                break;
            case COND_NOT_PRESENT:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s neither carried nor in room?\n", Items[dv].Text);
#endif
                if (Items[dv].Location == CARRIED || Items[dv].Location == MyLoc)
                    return ACT_FAILURE;
                break;
            case COND_IN_PLAY:
                if (dv > GameHeader.NumItems + 1)
                    Fatal("Broken database!");
#ifdef DEBUG_ACTIONS
                debug_print("Is %s (%d) in play?\n", Items[dv].Text, dv);
#endif
                if (Items[dv].Location == 0)
                    return ACT_FAILURE;
                break;
            case COND_NOT_IN_PLAY:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s NOT in play?\n", Items[dv].Text);
#endif
                if (Items[dv].Location)
                    return ACT_FAILURE;
                break;
            case COND_COUNTER_LE:
#ifdef DEBUG_ACTIONS
                debug_print("Is CurrentCounter <= %d?\n", dv);
#endif
                if (CurrentCounter > dv)
                    return ACT_FAILURE;
                break;
            case COND_COUNTER_GT:
#ifdef DEBUG_ACTIONS
                debug_print("Is CurrentCounter > %d?\n", dv);
#endif
                if (CurrentCounter <= dv)
                    return ACT_FAILURE;
                break;
            case COND_NOT_MOVED:
#ifdef DEBUG_ACTIONS
                debug_print("Is %s still in initial room?\n", Items[dv].Text);
#endif
                if (Items[dv].Location != Items[dv].InitialLoc)
                    return ACT_FAILURE;
                break;
            case COND_MOVED:
#ifdef DEBUG_ACTIONS
                debug_print("Has %s been moved?\n", Items[dv].Text);
#endif
                if (Items[dv].Location == Items[dv].InitialLoc)
                    return ACT_FAILURE;
                break;
            case COND_COUNTER_EQ:
#ifdef DEBUG_ACTIONS
                debug_print("Is current counter == %d?\n", dv);
                if (CurrentCounter != dv)
                    debug_print("Nope, current counter is %d\n", CurrentCounter);
#endif
                if (CurrentCounter != dv)
                    return ACT_FAILURE;
                break;
        }
#ifdef DEBUG_ACTIONS
        debug_print("YES\n");
#endif
        cc++;
    }
#if defined(__clang__)
#pragma mark Opcodes
#endif

    /* Decode the 4 opcodes from two 16-bit words.
     Each word encodes two actions: high = word / VOCAB_MULTIPLIER,
     low = word % VOCAB_MULTIPLIER. */
    act[0] = Actions[ct].Opcode[0];
    act[2] = Actions[ct].Opcode[1];
    act[1] = act[0] % VOCAB_MULTIPLIER;
    act[3] = act[2] % VOCAB_MULTIPLIER;
    act[0] /= VOCAB_MULTIPLIER;
    act[2] /= VOCAB_MULTIPLIER;
    cc = 0;
    pptr = 0;
    while (cc < NUM_OPCODES) {
#ifdef DEBUG_ACTIONS
        debug_print("Performing action %d: ", act[cc]);
#endif
        if (act[cc] >= FIRST_MESSAGE && act[cc] <= LAST_DIRECT_MESSAGE) {
            PrintMessage(act[cc]);
        } else if (act[cc] >= EXTENDED_MSG_BASE) {
            PrintMessage(act[cc] - EXTENDED_MSG_OFFSET);
        } else
            switch (act[cc]) {
                case 0: /* NOP */
                    break;
                case OP_GET_ITEM:
                    if (CountCarried() >= GameHeader.MaxCarry) {
                        Output(sys[YOURE_CARRYING_TOO_MUCH]);
                        return ACT_SUCCESS;
                    } else if (Items[param[pptr]].Location == MyLoc) {
                        should_draw_image = 1;
                    }
                    Items[param[pptr++]].Location = CARRIED;
                    break;
                case OP_DROP_ITEM:
#ifdef DEBUG_ACTIONS
                    debug_print("item %d (\"%s\") is now in location.\n", param[pptr],
                                Items[param[pptr]].Text);
#endif
                    DrawUSRoomObject(param[pptr]);
                    DrawImageOrVector();
                    Items[param[pptr++]].Location = MyLoc;
                    should_look_in_transcript = 1;
                    break;
                case OP_GOTO_ROOM:
                    GoTo(param[pptr++]);
                    break;
                case OP_DESTROY_ITEM:
                case OP_DESTROY_ITEM_2:
#ifdef DEBUG_ACTIONS
                    debug_print("Item %d (%s) is removed from the game (put in room 0).\n",
                                param[pptr], Items[param[pptr]].Text);
#endif
                    if (Items[param[pptr]].Location == MyLoc) {
                        should_look_in_transcript = should_draw_image = 1;
                    }
                    Items[param[pptr++]].Location = 0;
                    break;
                case OP_SET_DARK:
                    SetDark();
                    break;
                case OP_SET_LIGHT:
                    SetLight();
                    break;
                case OP_SET_FLAG:
                    SetBitFlag(param[pptr++]);
                    break;
                case OP_CLEAR_FLAG:
                    ClearBitFlag(param[pptr++]);
                    break;
                case OP_DIE:
                    PlayerIsDead();
                    break;
                case OP_MOVE_ITEM:
                    p = param[pptr++];
                    PutItemAInRoomB(p, param[pptr++]);
                    break;
                case OP_GAME_OVER:
#ifdef DEBUG_ACTIONS
                    debug_print("Game over.\n");
#endif
                    GameOver();
                    dead = 1;
                    break;
                case OP_LOOK:
                    break;
                case OP_PRINT_SCORE:
                    dead = PrintScore();
                    StopTime = 2;
                    break;
                case OP_LIST_INVENTORY:
                    if (Game->type == SEAS_OF_BLOOD_VARIANT)
                        AdventureSheet();
                    else
                        ListInventory(0);
                    if (Game->type == US_VARIANT && HasGraphics()) {
                        UpdateUSInventory();
                    }
                    StopTime = 2;
                    break;
                case OP_SET_FLAG_0:
#ifdef DEBUG_ACTIONS
                    debug_print("Set flag 0\n");
#endif
                    SetBitFlag(0);
                    break;
                case OP_CLEAR_FLAG_0:
#ifdef DEBUG_ACTIONS
                    debug_print("Clear flag 0\n");
#endif
                    ClearBitFlag(0);
                    break;
                case OP_REFILL_LIGHT:
#ifdef DEBUG_ACTIONS
                    debug_print("Refill lamp\n");
#endif
                    GameHeader.LightTime = LightRefill;
                    Items[LIGHT_SOURCE].Location = CARRIED;
                    ClearBitFlag(LIGHTOUTBIT);
                    break;
                case OP_CLEAR_SCREEN:
#ifdef DEBUG_ACTIONS
                    debug_print("Clear screen\n");
#endif
                    ClearScreen(); /* pdd. */
                    break;
                case OP_SAVE:
#ifdef DEBUG_ACTIONS
                    debug_print("Save game\n");
#endif
                    SaveGame();
                    StopTime = 2;
                    break;
                case OP_SWAP_ITEMS:
                    p = param[pptr++];
#ifdef DEBUG_ACTIONS
                    debug_print("Swap locations of item %d (%s) (currently at %d) and item %d (%s) (currently at %d).\n", p, Items[p].Text, Items[p].Location, param[pptr], Items[param[pptr]].Text, Items[param[pptr]].Location);
#endif
                    SwapItemLocations(p, param[pptr++]);
                    break;
                case OP_CONTINUE:
#ifdef DEBUG_ACTIONS
                    debug_print("Continue with next line\n");
#endif
                    continuation = 1;
                    break;
                case OP_FORCE_CARRY:
                    if (Items[param[pptr]].Location == MyLoc) {
                        should_look_in_transcript = should_draw_image = 1;
                    }
                    Items[param[pptr++]].Location = CARRIED;
                    break;
                case OP_MOVE_TO_LOC_OF:
                    p = param[pptr++];
                    MoveItemAToLocOfItemB(p, param[pptr++]);
                    break;
                case OP_LOOK_2:
#ifdef DEBUG_ACTIONS
                    debug_print("LOOK\n");
#endif
                    print_look_to_transcript = should_draw_image = 1;
                    if (split_screen)
                        Look();
                    print_look_to_transcript =
                    should_look_in_transcript = 0;
                    break;
                case OP_DEC_COUNTER:
                    if (CurrentCounter >= 1)
                        CurrentCounter--;
#ifdef DEBUG_ACTIONS
                    debug_print(
                                "decrementing current counter. Current counter is now %d.\n",
                                CurrentCounter);
#endif
                    break;
                case OP_PRINT_COUNTER:
                    OutputNumber(CurrentCounter);
                    Output(" ");
                    break;
                case OP_SET_COUNTER:
#ifdef DEBUG_ACTIONS
                    debug_print("CurrentCounter is set to %d.\n", param[pptr]);
#endif
                    CurrentCounter = param[pptr++];
                    break;
                case OP_GOTO_STORED:
                    GoToStoredLoc();
                    break;
                case OP_SWAP_COUNTER:
                    SwapCounters(param[pptr++]);
                    break;
                case OP_ADD_COUNTER:
                    CurrentCounter += param[pptr++];
                    break;
                case OP_SUB_COUNTER:
                    CurrentCounter -= param[pptr++];
                    if (CurrentCounter < -1)
                        CurrentCounter = -1;
                    /* Note: This seems to be needed. I don't yet
                     know if there is a maximum value to limit too */
                    break;
                case OP_PRINT_NOUN:
                    PrintNoun();
                    break;
                case OP_PRINTLN_NOUN:
                    PrintNoun();
                    Output("\n");
                    break;
                case OP_NEWLINE:
                    if (!(Options & SPECTRUM_STYLE))
                        Output("\n");
                    break;
                case OP_SWAP_ROOM:
                    SwapLocAndRoomflag(param[pptr++]);
                    break;
                case OP_DELAY:
#ifdef DEBUG_ACTIONS
                    debug_print("Delay\n");
#endif
                    Delay(1);
                    break;
                case OP_GAME_SPECIFIC:
#ifdef DEBUG_ACTIONS
                    debug_print("Action OP_GAME_SPECIFIC, parameter %d\n", param[pptr]);
#endif
                    fprintf(stderr, "Action OP_GAME_SPECIFIC, parameter %d\n", param[pptr]);
                    /* Some SAGA releases use this opcode (89) to show room image 80:
                       - Close-up of the genie in Adventureland
                       - Map sailing animation in Pirate Adventure
                       - Bomb blast ending animation in Mission Impossible */

                    /*  Unlike the Adventure Internation UK interpreters, in these games
                        opcode 89 does NOT take an argument, so we must watch out for this. */
                    if (Game->type == US_VARIANT) {
                        fprintf(stderr, "Opcode 89 called in US game, do NOT read parameter!\n");
                        ShowUSCloseup(US_CLOSEUP_IMAGE, 0);
                    } else {
                        p = param[pptr++];
                    }
                    switch (CurrentGame) {
                        case SPIDERMAN:
                        case SPIDERMAN_C64:
                            DrawBlack();
                            break;
                        case SECRET_MISSION:
                        case SECRET_MISSION_C64:
                            SecretAction(p);
                            break;
                        case ADVENTURELAND:
                        case ADVENTURELAND_C64:
                            AdventurelandAction(p);
                            break;
                        case SEAS_OF_BLOOD:
                        case SEAS_OF_BLOOD_C64:
                            BloodAction(p);
                            break;
                        case ROBIN_OF_SHERWOOD:
                        case ROBIN_OF_SHERWOOD_C64:
                            SherwoodAction(p);
                            break;
                        case GREMLINS:
                        case GREMLINS_SPANISH:
                        case GREMLINS_SPANISH_C64:
                        case GREMLINS_GERMAN:
                        case GREMLINS_GERMAN_C64:
                            GremlinsAction();
                            break;
                        default:
                            break;
                    }
                    break;
                case OP_DRAW_IMAGE:
#ifdef DEBUG_ACTIONS
                    debug_print("Draw Hulk image, parameter %d\n", param[pptr]);
#endif
                    p = param[pptr++];
                    if (!ItIsDark() && (CurrentGame == HULK || CurrentGame == HULK_C64)) {
                        DrawHulkImage(p);
                    } else if (Game->type == US_VARIANT) {
                        ShowUSCloseup(p, 0);
                    }
                    break;
                default:
                    debug_print("Unknown action %d [Param begins %d %d]\n", act[cc],
                                param[pptr], param[pptr + 1]);
                    break;
            }
        cc++;
    }

    if (dead) {
        return ACT_GAMEOVER;
    } else if (continuation) {
        return ACT_CONTINUE;
    } else {
        return ACT_SUCCESS;
    }
}


/* Main action dispatch: process player input (verb/noun pair) or
 implicit actions (vb=0).

 Handles movement (GO + direction), game-specific examine actions,
 the action table scan, and built-in TAKE/DROP with auto-matching.
 Action 73 (continue) chains consecutive lines with vocab 0,0.

 For TI-99/4A games, delegates to the TI-specific action handler. */
ExplicitResultType PerformActions(int vb, int no)
{
    int dark = ItIsDark();
    int ct = 0;
    ExplicitResultType flag;
    int doagain = 0;
    int found_match = 0;
#if defined(__clang__)
#pragma mark GO
#endif
    if (vb == GO && no == -1) {
        Output(sys[DIRECTION]);
        return ER_SUCCESS;
    }
    if (vb == GO && no >= 1 && no <= NUM_EXITS) {
        int nl;
        if (dark)
            Output(sys[DANGEROUS_TO_MOVE_IN_DARK]);
        nl = Rooms[MyLoc].Exits[no - 1];
        if (nl != 0) {
            /* Seas of Blood needs this to be able to flee back to the last room */
            if (Game->type == SEAS_OF_BLOOD_VARIANT)
                SavedRoom = MyLoc;
            if (Options & (SPECTRUM_STYLE | TI994A_STYLE))
                Output(sys[OK]);
            MyLoc = nl;
            should_look_in_transcript = should_draw_image = 1;
            if (CurrentCommand && CurrentCommand->next) {
                LookWithPause();
            }
            return ER_SUCCESS;
        }
        if (dark) {
            SetLight();
            MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
            Output(sys[YOU_FELL_AND_BROKE_YOUR_NECK]);
            should_look_in_transcript = should_draw_image = 1;
            return ER_SUCCESS;
        }
        Output(sys[YOU_CANT_GO_THAT_WAY]);
        return ER_SUCCESS;
    }

    if (!dark) {
        switch (CurrentGame) {
            case HULK:
            case HULK_C64:
            case HULK_US:
                if (vb == HULK_EXAMINE_VERB)
                    HulkShowImageOnExamine(no);
                break;
            case COUNT_US:
                if (vb == COUNT_EXAMINE_VERB)
                    CountShowImageOnExamineUS(no);
                break;
            case VOODOO_CASTLE_US:
                if (vb == VOODOO_EXAMINE_VERB)
                    VoodooShowImageOnExamineUS(no);
                break;
            case ADVENTURELAND_US:
                if (vb == ADVLAND_EXAMINE_VERB)
                    AdventurelandShowImageOnExamineUS(no);
                break;
            case PIRATE_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == PIRATE_EXAMINE_VERB) {
                    PirateShowImageOnExamineUS(no);
                }
                break;
            case SECRET_MISSION_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == MISSION_EXAMINE_VERB)
                    MissionShowImageOnExamineUS(no);
                break;
            case STRANGE_ODYSSEY_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == STRANGE_EXAMINE_VERB)
                    StrangeShowImageOnExamineUS(no);
                break;
            default:
                break;
        }
    }

    if (CurrentCommand && CurrentCommand->allflag && vb == CurrentCommand->verb && !(dark && vb == TAKE)) {
        if (!lastwasnewline)
            Output("\n");
        Output(Items[CurrentCommand->item].Text);
        Output("....");
    }
    flag = ER_RAN_ALL_LINES_NO_MATCH;
    if (CurrentGame != TI994A) {
        while (ct <= GameHeader.NumActions) {
            int verbvalue, nounvalue;
            verbvalue = Actions[ct].Vocab;
            /* Think this is now right. If a line we run has an action 73
             run all following lines with vocab of 0,0 */
            if (vb != 0 && (doagain && verbvalue != 0))
                break;
            /* Oops.. added this minor cockup fix 1.11 */
            if (vb != 0 && !doagain && flag == 0)
                break;
            nounvalue = verbvalue % VOCAB_MULTIPLIER;
            verbvalue /= VOCAB_MULTIPLIER;
            if ((verbvalue == vb) || (doagain && Actions[ct].Vocab == 0)) {
                if ((verbvalue == 0 && RandomPercent(nounvalue)) || doagain || (verbvalue != 0 && (nounvalue == no || nounvalue == 0))) {
                    if (verbvalue == vb && vb != 0 && nounvalue == no)
                        found_match = 1;
                    ActionResultType flag2;
                    if (flag == ER_RAN_ALL_LINES_NO_MATCH)
                        flag = ER_RAN_ALL_LINES;
                    if ((flag2 = PerformLine(ct)) != ACT_FAILURE) {
                        /* ahah finally figured it out ! */
                        flag = ER_SUCCESS;
                        if (flag2 == ACT_CONTINUE)
                            doagain = 1;
                        else if (flag2 == ACT_GAMEOVER)
                            return ER_SUCCESS;
                        if (vb != 0 && doagain == 0)
                            return ER_SUCCESS;
                    }
                }
            }

            ct++;

            // clang-format off
            /* Previously this did not check ct against
             * GameHeader.NumActions and would read past the end of
             * Actions.  I don't know what should happen on the last
             * action, but doing nothing is better than reading one
             * past the end.
             * --Chris
             */
            // clang-format on

            if (ct <= GameHeader.NumActions && Actions[ct].Vocab != 0)
                doagain = 0;
        }
    } else {
        if (vb == 0) {
            RunImplicitTI99Actions();
            return ER_NO_RESULT;
        } else {
            flag = RunExplicitTI99Actions(vb, no);
        }
    }

    if (found_match)
        return flag;

    if (flag != ER_SUCCESS) {
        int item = 0;
#if defined(__clang__)
#pragma mark TAKE
#endif
        if (vb == TAKE || vb == DROP) {
            if (CurrentCommand && CurrentCommand->allflag) {
                if (vb == TAKE && dark) {
                    Output(sys[TOO_DARK_TO_SEE]);
                    while (!(CurrentCommand->allflag & LASTALL)) {
                        CurrentCommand = CurrentCommand->next;
                    }
                    return ER_SUCCESS;
                }
                item = CurrentCommand->item;
                int location = CARRIED;
                if (vb == TAKE)
                    location = MyLoc;
                while (Items[item].Location != location && !(CurrentCommand->allflag & LASTALL)) {
                    CurrentCommand = CurrentCommand->next;
                }
                if (Items[item].Location != location)
                    return ER_SUCCESS;
            }

            /* Yes they really _are_ hardcoded values */
            if (vb == TAKE) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, MyLoc);
                if (item == -1) {
                    item = MatchUpItem(no, CARRIED);
                    if (item == -1) {
                        item = MatchUpItem(no, 0);
                        if (item == -1) {
                            Output(sys[THATS_BEYOND_MY_POWER]);
                        } else {
                            Output(sys[YOU_DONT_SEE_IT]);
                        }
                    } else {
                        Output(sys[YOU_HAVE_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = CARRIED;
                should_draw_image = 1;
                PrintTakenOrDropped(TAKEN);
                return ER_SUCCESS;
            }
#if defined(__clang__)
#pragma mark DROP
#endif
            if (vb == DROP) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, CARRIED);
                if (item == -1) {
                    item = MatchUpItem(no, 0);
                    if (item == -1) {
                        Output(sys[THATS_BEYOND_MY_POWER]);
                    } else {
                        Output(sys[YOU_HAVENT_GOT_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = MyLoc;
                DrawUSRoomObject(item);
                DrawImageOrVector();
                PrintTakenOrDropped(DROPPED);
                return ER_SUCCESS;
            }
        }
    }
    return flag;
}
