/* Audit tool: can a real game's action table move an item out from under an
   in-progress TAKE ALL / DROP ALL chain?

   The ALL chain is expanded once, when the line is parsed (CreateAllCommands),
   into one node per eligible item. Each node then runs as its own turn, which
   means implicit (vocab 0) actions run between nodes and an explicit action can
   fire for a node's own verb/noun. If any of those relocates an item that is
   still queued in the chain, the chain's skip path is taken.

   This walks Actions[] and reports every action that relocates an ALL-eligible
   item (one with a non-treasure AutoGet word), split by how it can fire:
     - implicit (verb 0): fires on a random roll every turn, so it can fire
       between two nodes of a chain that is already running;
     - TAKE/DROP (verb 10/18): fires for a node of the chain itself, and moves
       an item that is NOT the one it was invoked for.

   Reuses every engine object except scott.o's glk_main (renamed via
   -Dglk_main=scott_real_glk_main), same trick as death_test.c. */

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

#include "detect_game.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_defines.h"

/* Opcodes that change an item's Location, and how many params each pops. */
static int MovesItem(int op)
{
    switch (op) {
    case OP_GET_ITEM:      /* 52: item -> CARRIED */
    case OP_DROP_ITEM:     /* 53: item -> MyLoc */
    case OP_DESTROY_ITEM:  /* 55: item -> room 0 */
    case OP_DESTROY_ITEM_2:/* 59 */
    case OP_MOVE_ITEM:     /* 62: item -> room */
    case OP_SWAP_ITEMS:    /* 72 */
    case OP_FORCE_CARRY:   /* 74 */
    case OP_MOVE_TO_LOC_OF:/* 75 */
        return 1;
    default:
        return 0;
    }
}

static int ParamCount(int op)
{
    return (op == OP_MOVE_ITEM || op == OP_SWAP_ITEMS || op == OP_MOVE_TO_LOC_OF) ? 2 : 1;
}

/* An item is eligible for ALL if it has an AutoGet word that isn't a treasure. */
static int AllEligible(int item)
{
    if (item < 0 || item > GameHeader.NumItems)
        return 0;
    return Items[item].AutoGet != NULL && Items[item].AutoGet[0] != '*';
}

void glk_main(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    glk_set_window(Bottom);
    if (game_file == NULL)
        return;

    extern const char *sysdict_i_am[];
    for (int i = 0; i < MAX_SYSMESS; i++)
        sys[i] = sysdict_i_am[i] ? sysdict_i_am[i] : "";

    if (DetectGame(game_file) == UNKNOWN_GAME) {
        fprintf(stderr, "all_audit: could not load %s\n", game_file);
        return;
    }

    int implicit = 0, explicit_take = 0;

    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        int vocab = Actions[ct].Vocab;
        int verb = vocab / VOCAB_MULTIPLIER;
        int noun = vocab % VOCAB_MULTIPLIER;

        if (verb != 0 && verb != TAKE && verb != DROP)
            continue;

        /* Collect the action's params, exactly as PerformLine does. */
        int param[NUM_CONDITIONS], pptr = 0;
        for (int cc = 0; cc < NUM_CONDITIONS; cc++)
            if (Actions[ct].Condition[cc] % CONDITION_MULTIPLIER == 0)
                param[pptr++] = Actions[ct].Condition[cc] / CONDITION_MULTIPLIER;

        int act[NUM_OPCODES];
        act[0] = Actions[ct].Opcode[0] / VOCAB_MULTIPLIER;
        act[1] = Actions[ct].Opcode[0] % VOCAB_MULTIPLIER;
        act[2] = Actions[ct].Opcode[1] / VOCAB_MULTIPLIER;
        act[3] = Actions[ct].Opcode[1] % VOCAB_MULTIPLIER;

        int p = 0;
        for (int cc = 0; cc < NUM_OPCODES; cc++) {
            int op = act[cc];
            if (op >= FIRST_MESSAGE && op <= LAST_DIRECT_MESSAGE)
                continue;
            if (op >= EXTENDED_MSG_BASE)
                continue;
            if (!MovesItem(op)) {
                /* Not an item op, but it may still pop params. Approximate:
                   only item ops and a few others take params; a mis-tracked
                   pptr just makes us over-report, never under-report. */
                continue;
            }
            int target = (p < pptr) ? param[p] : 0;
            p += ParamCount(op);

            if (!AllEligible(target))
                continue;
            /* An explicit TAKE/DROP action moving its OWN item is the normal
               case (the action does the take); only a different item matters. */
            if (verb != 0 && Items[target].AutoGet != NULL && noun != 0) {
                extern int WhichWord(const char *, const char **, int, int);
                int tnoun = WhichWord(Items[target].AutoGet, (const char **)Nouns,
                                      GameHeader.WordLength, GameHeader.NumWords + 1);
                if (tnoun == noun)
                    continue;
            }

            if (verb == 0) {
                implicit++;
                printf("  implicit  action %3d (chance %d%%): op %d moves ALL-item %d \"%s\"\n",
                       ct, noun, op, target, Items[target].Text);
            } else {
                explicit_take++;
                printf("  %-9s action %3d (noun %d): op %d moves OTHER ALL-item %d \"%s\"\n",
                       verb == TAKE ? "TAKE" : "DROP", ct, noun, op, target, Items[target].Text);
            }
        }
    }

    printf("%s: %d implicit + %d explicit TAKE/DROP actions can relocate an "
           "ALL-eligible item mid-chain\n",
           game_file, implicit, explicit_take);
    glk_exit();
}
