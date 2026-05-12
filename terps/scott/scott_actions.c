/*
 *  scott_actions.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Action engine: condition evaluation, subcommand dispatch, and
 *  main action table scanning extracted from scott.c.
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

#include "scott.h"

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
#define COND_AT_INITIAL_LOC 17
#define COND_MOVED          18
#define COND_COUNTER_EQ     19

/* Subcommand message ranges: codes 1-51 print that message directly,
   codes 102+ print message (code - EXTENDED_MSG_OFFSET) */
#define FIRST_MESSAGE 1
#define LAST_DIRECT_MESSAGE 51
#define EXTENDED_MSG_BASE 102
#define EXTENDED_MSG_OFFSET 50

/* Subcommand opcodes (52-90) — game state operations */
#define CMD_GET             52
#define CMD_DROP_HERE       53
#define CMD_GOTO            54
#define CMD_DESTROY         55
#define CMD_SET_DARK        56
#define CMD_SET_LIGHT       57
#define CMD_SET_FLAG        58
#define CMD_DESTROY_2       59
#define CMD_CLEAR_FLAG      60
#define CMD_DIE             61
#define CMD_PUT_IN_ROOM     62
#define CMD_GAME_OVER       63
#define CMD_LOOK            64
#define CMD_SCORE           65
#define CMD_INVENTORY       66
#define CMD_SET_FLAG0       67
#define CMD_CLEAR_FLAG0     68
#define CMD_REFILL_LAMP     69
#define CMD_CLEAR_SCREEN    70
#define CMD_SAVE_GAME       71
#define CMD_SWAP_ITEMS      72
#define CMD_CONTINUE        73
#define CMD_FORCED_GET      74
#define CMD_MOVE_TO_LOC_OF  75
#define CMD_LOOK2           76
#define CMD_DEC_COUNTER     77
#define CMD_PRINT_COUNTER   78
#define CMD_SET_COUNTER     79
#define CMD_SWAP_LOC        80
#define CMD_SWAP_COUNTERS   81
#define CMD_ADD_COUNTER     82
#define CMD_SUB_COUNTER     83
#define CMD_PRINT_NOUN      84
#define CMD_PRINTLN_NOUN    85
#define CMD_NEWLINE         86
#define CMD_SWAP_ROOMFLAG   87
#define CMD_DELAY           88
#define CMD_GAME_SPECIFIC   89
#define CMD_DRAW_IMAGE      90

/* Game-specific EXAMINE verb indices */
#define HULK_EXAMINE_VERB 39
#define COUNT_EXAMINE_VERB 8
#define VOODOO_EXAMINE_VERB 42
#define ADVENTURELAND_EXAMINE_VERB 29
#define PIRATE_EXAMINE_VERB 27
#define MISSION_EXAMINE_VERB 40
#define STRANGE_EXAMINE_VERB 37

/* US-variant game-specific image */
#define US_GAME_SPECIFIC_IMAGE 80

/* Evaluate one action line: check up to 5 conditions, and if all pass,
   execute the 4 subcommands encoded in the action's two Subcommand words.

   Conditions are encoded as (value * 20 + condition_code). Condition
   code 0 stores a parameter for later use by subcommands. Subcommands
   1-51 and 102+ print messages; 52-90 are game state operations.

   Returns ACT_FAILURE if a condition fails, ACT_CONTINUE for action 73
   (continue to next line), ACT_GAMEOVER on game-ending actions, or
   ACT_SUCCESS if all subcommands completed normally. */
static ActionResultType PerformLine(int ct)
{
#ifdef DEBUG_ACTIONS
    debug_print("Performing line %d: ", ct);
#endif
    int continuation = 0, dead = 0;
    int param[NUM_CONDITIONS], pptr = 0;
    int p = 0;
    int act[NUM_SUBCOMMANDS];
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
        case COND_AT_INITIAL_LOC:
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
#pragma mark Subcommands
#endif

    /* Decode the 4 subcommands from two 16-bit words.
       Each word encodes two actions: high = word / VOCAB_MULTIPLIER,
       low = word % VOCAB_MULTIPLIER. */
    act[0] = Actions[ct].Subcommand[0];
    act[2] = Actions[ct].Subcommand[1];
    act[1] = act[0] % VOCAB_MULTIPLIER;
    act[3] = act[2] % VOCAB_MULTIPLIER;
    act[0] /= VOCAB_MULTIPLIER;
    act[2] /= VOCAB_MULTIPLIER;
    cc = 0;
    pptr = 0;
    while (cc < NUM_SUBCOMMANDS) {
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
            case CMD_GET:
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ACT_SUCCESS;
                } else if (Items[param[pptr]].Location == MyLoc) {
                    should_draw_image = 1;
                }
                Items[param[pptr++]].Location = CARRIED;
                break;
            case CMD_DROP_HERE:
#ifdef DEBUG_ACTIONS
                debug_print("item %d (\"%s\") is now in location.\n", param[pptr],
                    Items[param[pptr]].Text);
#endif
                DrawUSRoomObject(param[pptr]);
                DrawImageOrVector();
                Items[param[pptr++]].Location = MyLoc;
                should_look_in_transcript = 1;
                break;
            case CMD_GOTO:
                GoTo(param[pptr++]);
                break;
            case CMD_DESTROY:
            case CMD_DESTROY_2:
#ifdef DEBUG_ACTIONS
                debug_print("Item %d (%s) is removed from the game (put in room 0).\n",
                    param[pptr], Items[param[pptr]].Text);
#endif
                if (Items[param[pptr]].Location == MyLoc) {
                    should_look_in_transcript = should_draw_image = 1;
                }
                Items[param[pptr++]].Location = 0;
                break;
            case CMD_SET_DARK:
                SetDark();
                break;
            case CMD_SET_LIGHT:
                SetLight();
                break;
            case CMD_SET_FLAG:
                SetBitFlag(param[pptr++]);
                break;
            case CMD_CLEAR_FLAG:
                ClearBitFlag(param[pptr++]);
                break;
            case CMD_DIE:
                PlayerIsDead();
                break;
            case CMD_PUT_IN_ROOM:
                p = param[pptr++];
                PutItemAInRoomB(p, param[pptr++]);
                break;
            case CMD_GAME_OVER:
#ifdef DEBUG_ACTIONS
                debug_print("Game over.\n");
#endif
                DoneIt();
                dead = 1;
                break;
            case CMD_LOOK:
                break;
            case CMD_SCORE:
                dead = PrintScore();
                StopTime = 2;
                break;
            case CMD_INVENTORY:
                if (Game->type == SEAS_OF_BLOOD_VARIANT)
                    AdventureSheet();
                else
                    ListInventory(0);
                if (Game->type == US_VARIANT && HasGraphics()) {
                    UpdateUSInventory();
                }
                StopTime = 2;
                break;
            case CMD_SET_FLAG0:
#ifdef DEBUG_ACTIONS
                debug_print("Set flag 0\n");
#endif
                SetBitFlag(0);
                break;
            case CMD_CLEAR_FLAG0:
#ifdef DEBUG_ACTIONS
                debug_print("Clear flag 0\n");
#endif
                ClearBitFlag(0);
                break;
            case CMD_REFILL_LAMP:
#ifdef DEBUG_ACTIONS
                debug_print("Refill lamp\n");
#endif
                GameHeader.LightTime = LightRefill;
                Items[LIGHT_SOURCE].Location = CARRIED;
                ClearBitFlag(LIGHTOUTBIT);
                break;
            case CMD_CLEAR_SCREEN:
#ifdef DEBUG_ACTIONS
                    debug_print("Clear screen\n");
#endif
                ClearScreen(); /* pdd. */
                break;
            case CMD_SAVE_GAME:
#ifdef DEBUG_ACTIONS
                    debug_print("Save game\n");
#endif
                SaveGame();
                StopTime = 2;
                break;
            case CMD_SWAP_ITEMS:
                p = param[pptr++];
#ifdef DEBUG_ACTIONS
                    debug_print("Swap locations of item %d (%s) (currently at %d) and item %d (%s) (currently at %d).\n", p, Items[p].Text, Items[p].Location, param[pptr], Items[param[pptr]].Text, Items[param[pptr]].Location);
#endif
                SwapItemLocations(p, param[pptr++]);
                break;
            case CMD_CONTINUE:
#ifdef DEBUG_ACTIONS
                debug_print("Continue with next line\n");
#endif
                continuation = 1;
                break;
            case CMD_FORCED_GET:
                if (Items[param[pptr]].Location == MyLoc) {
                    should_look_in_transcript = should_draw_image = 1;
                }
                Items[param[pptr++]].Location = CARRIED;
                break;
            case CMD_MOVE_TO_LOC_OF:
                p = param[pptr++];
                MoveItemAToLocOfItemB(p, param[pptr++]);
                break;
            case CMD_LOOK2:
#ifdef DEBUG_ACTIONS
                debug_print("LOOK\n");
#endif
                print_look_to_transcript = should_draw_image = 1;
                if (split_screen)
                    Look();
                print_look_to_transcript =
                should_look_in_transcript = 0;
                break;
            case CMD_DEC_COUNTER:
                if (CurrentCounter >= 1)
                    CurrentCounter--;
#ifdef DEBUG_ACTIONS
                debug_print(
                    "decrementing current counter. Current counter is now %d.\n",
                    CurrentCounter);
#endif
                break;
            case CMD_PRINT_COUNTER:
                OutputNumber(CurrentCounter);
                Output(" ");
                break;
            case CMD_SET_COUNTER:
#ifdef DEBUG_ACTIONS
                debug_print("CurrentCounter is set to %d.\n", param[pptr]);
#endif
                CurrentCounter = param[pptr++];
                break;
            case CMD_SWAP_LOC:
                GoToStoredLoc();
                break;
            case CMD_SWAP_COUNTERS:
                SwapCounters(param[pptr++]);
                break;
            case CMD_ADD_COUNTER:
                CurrentCounter += param[pptr++];
                break;
            case CMD_SUB_COUNTER:
                CurrentCounter -= param[pptr++];
                if (CurrentCounter < -1)
                    CurrentCounter = -1;
                /* Note: This seems to be needed. I don't yet
                         know if there is a maximum value to limit too */
                break;
            case CMD_PRINT_NOUN:
                PrintNoun();
                break;
            case CMD_PRINTLN_NOUN:
                PrintNoun();
                Output("\n");
                break;
            case CMD_NEWLINE:
                if (!(Options & SPECTRUM_STYLE))
                    Output("\n");
                break;
            case CMD_SWAP_ROOMFLAG:
                SwapLocAndRoomflag(param[pptr++]);
                break;
            case CMD_DELAY:
#ifdef DEBUG_ACTIONS
                debug_print("Delay\n");
#endif
                Delay(1);
                break;
            case CMD_GAME_SPECIFIC:
#ifdef DEBUG_ACTIONS
                debug_print("Action CMD_GAME_SPECIFIC, parameter %d\n", param[pptr]);
#endif
                fprintf(stderr, "Action CMD_GAME_SPECIFIC, parameter %d\n", param[pptr]);
                if (Game->type == US_VARIANT) {
                    fprintf(stderr, "CMD_GAME_SPECIFIC called in US game, do not read parameter!\n");
                    ShowUSCloseup(US_GAME_SPECIFIC_IMAGE, 0);
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
            case CMD_DRAW_IMAGE:
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
                if (vb == ADVENTURELAND_EXAMINE_VERB)
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
