/* Audit tool: does any action line of a game name an item that is not in the
   Items[] array?

   ConditionHolds() (scott_actions.c) range-checks its argument for
   COND_IN_PLAY only; the other item conditions index Items[arg] as they find
   it, and so do the opcodes that move items. This walks every condition and
   every opcode parameter of a loaded game without running it, and prints a line
   for each argument that is out of range:

     - standard games: Actions[0..NumActions], five conditions each, and the
       parameters the four opcodes of the line consume, tracked the way
       PerformLine() does;
     - TI-99/4A games: the implicit action list and every verb's chain of
       explicit action blocks, decoded as PerformTI99Line() does.

   Items are the subject; room and bit flag arguments are checked on the way
   because they come for free. "Out of range" for an item is an index above
   GameHeader.NumItems. AllocateGameData() always allocates through
   LIGHT_SOURCE, so an index between NumItems and LIGHT_SOURCE is reported as
   "unused" and not as "OUT OF BOUNDS".

   Reuses every engine object except scott.o's glk_main (renamed via
   -Dglk_main=scott_real_glk_main), same trick as death_test.c.

     usage: [SCOTT_AUDIT_DUMP=1] scott_cond_audit <game-file>
   with the answer to a compilation's menu, if any, on stdin. A line with a
   finding is printed as stored; SCOTT_AUDIT_DUMP prints every line. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

#include "detect_game.h"
#include "load_ti99_4a.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_defines.h"

typedef enum { ARG_NONE, ARG_ITEM, ARG_ROOM, ARG_FLAG, ARG_OTHER } ArgKind;

/* Number of bits in BitFlags (a uint64_t). */
#define NUM_BIT_FLAGS 64

static int checked[ARG_OTHER + 1];
static int highest[ARG_OTHER + 1];
static int findings = 0;
static int dump_all = 0;

static const char *const kind_names[] = { "", "item", "room", "flag", "" };

static const char *const cond_names[] = {
    "PARAMETER", "CARRIED", "IN_ROOM", "PRESENT", "AT_LOC", "NOT_IN_ROOM",
    "NOT_CARRIED", "NOT_AT_LOC", "FLAG_SET", "FLAG_CLEAR", "CARRYING_ANY",
    "CARRYING_NONE", "NOT_PRESENT", "IN_PLAY", "NOT_IN_PLAY", "COUNTER_LE",
    "COUNTER_GT", "NOT_MOVED", "MOVED", "COUNTER_EQ"
};

static ArgKind ConditionArgKind(int condition)
{
    switch (condition) {
    case COND_CARRIED:
    case COND_IN_ROOM:
    case COND_PRESENT:
    case COND_NOT_IN_ROOM:
    case COND_NOT_CARRIED:
    case COND_NOT_PRESENT:
    case COND_IN_PLAY:
    case COND_NOT_IN_PLAY:
    case COND_NOT_MOVED:
    case COND_MOVED:
        return ARG_ITEM;
    case COND_AT_LOC:
    case COND_NOT_AT_LOC:
        return ARG_ROOM;
    case COND_FLAG_SET:
    case COND_FLAG_CLEAR:
        return ARG_FLAG;
    default:
        return ARG_NONE;
    }
}

/* where: "action 12", "implicit +0x34", "verb 7 +0x1a2"; what: the condition
   or opcode the argument belongs to. */
static void Check(ArgKind kind, int arg, const char *where, const char *what)
{
    if (kind == ARG_NONE || kind == ARG_OTHER)
        return;

    checked[kind]++;
    if (arg > highest[kind])
        highest[kind] = arg;

    const char *verdict = NULL;
    switch (kind) {
    case ARG_ITEM:
        if (arg < 0 || arg > MAX(GameHeader.NumItems, LIGHT_SOURCE))
            verdict = "OUT OF BOUNDS";
        else if (arg > GameHeader.NumItems)
            verdict = "unused";
        break;
    case ARG_ROOM:
        if (arg < 0 || arg > GameHeader.NumRooms)
            verdict = "OUT OF BOUNDS";
        break;
    case ARG_FLAG:
        if (arg < 0 || arg >= NUM_BIT_FLAGS)
            verdict = "OUT OF BOUNDS";
        break;
    default:
        break;
    }
    if (verdict == NULL)
        return;

    findings++;
    printf("  %s: %s %s %d: %s\n", where, what, kind_names[kind], arg, verdict);
}

// MARK: Standard action table

/* The kinds of the parameters an opcode takes off the parameter list, in
   order. Returns how many. */
static int OpcodeArgs(int op, ArgKind kinds[2])
{
    kinds[0] = kinds[1] = ARG_NONE;
    switch (op) {
    case OP_GET_ITEM:
    case OP_DROP_ITEM:
    case OP_DESTROY_ITEM:
    case OP_DESTROY_ITEM_2:
    case OP_FORCE_CARRY:
        kinds[0] = ARG_ITEM;
        return 1;
    case OP_GOTO_ROOM:
        kinds[0] = ARG_ROOM;
        return 1;
    case OP_SET_FLAG:
    case OP_CLEAR_FLAG:
        kinds[0] = ARG_FLAG;
        return 1;
    case OP_MOVE_ITEM:
        kinds[0] = ARG_ITEM;
        kinds[1] = ARG_ROOM;
        return 2;
    case OP_SWAP_ITEMS:
    case OP_MOVE_TO_LOC_OF:
        kinds[0] = kinds[1] = ARG_ITEM;
        return 2;
    case OP_SET_COUNTER:
    case OP_SWAP_COUNTER:
    case OP_ADD_COUNTER:
    case OP_SUB_COUNTER:
    case OP_SWAP_ROOM:
    case OP_DRAW_IMAGE:
        kinds[0] = ARG_OTHER;
        return 1;
    case OP_GAME_SPECIFIC:
        /* Takes no argument in the US releases; see PerformLine(). */
        if (Game->type == US_VARIANT)
            return 0;
        kinds[0] = ARG_OTHER;
        return 1;
    default:
        return 0;
    }
}

/* The line as it is stored: verb and noun, the five condition/argument
   pairs, the four opcodes. */
static void DumpAction(int ct)
{
    const Action *a = &Actions[ct];

    printf("    %3d: verb %d noun %d |", ct, a->Vocab / VOCAB_MULTIPLIER,
        a->Vocab % VOCAB_MULTIPLIER);
    for (int cc = 0; cc < NUM_CONDITIONS; cc++)
        printf(" %s(%d)", cond_names[a->Condition[cc] % CONDITION_MULTIPLIER],
            a->Condition[cc] / CONDITION_MULTIPLIER);
    printf(" | %d %d %d %d\n", a->Opcode[0] / VOCAB_MULTIPLIER,
        a->Opcode[0] % VOCAB_MULTIPLIER, a->Opcode[1] / VOCAB_MULTIPLIER,
        a->Opcode[1] % VOCAB_MULTIPLIER);
}

static void AuditStandard(void)
{
    char where[32], what[32];

    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        int param[NUM_CONDITIONS], num_params = 0;

        int findings_before = findings;

        snprintf(where, sizeof where, "action %d", ct);

        for (int cc = 0; cc < NUM_CONDITIONS; cc++) {
            int cv = Actions[ct].Condition[cc] % CONDITION_MULTIPLIER;
            int dv = Actions[ct].Condition[cc] / CONDITION_MULTIPLIER;
            if (cv == COND_PARAMETER) {
                param[num_params++] = dv;
                continue;
            }
            snprintf(what, sizeof what, "condition %s", cond_names[cv]);
            Check(ConditionArgKind(cv), dv, where, what);
        }

        int act[NUM_OPCODES];
        act[0] = Actions[ct].Opcode[0] / VOCAB_MULTIPLIER;
        act[1] = Actions[ct].Opcode[0] % VOCAB_MULTIPLIER;
        act[2] = Actions[ct].Opcode[1] / VOCAB_MULTIPLIER;
        act[3] = Actions[ct].Opcode[1] % VOCAB_MULTIPLIER;

        int pptr = 0;
        for (int cc = 0; cc < NUM_OPCODES; cc++) {
            ArgKind kinds[2];
            int count = OpcodeArgs(act[cc], kinds);
            for (int i = 0; i < count; i++, pptr++) {
                snprintf(what, sizeof what, "opcode %d", act[cc]);
                if (pptr >= num_params) {
                    /* PerformLine() reads a parameter slot the line never
                       filled. */
                    if (kinds[i] != ARG_OTHER) {
                        findings++;
                        printf("  %s: %s takes %s parameter %d, line has %d\n",
                            where, what, kind_names[kinds[i]], pptr + 1, num_params);
                    }
                    continue;
                }
                Check(kinds[i], param[pptr], where, what);
            }
        }

        if (findings != findings_before || dump_all)
            DumpAction(ct);
    }
}

// MARK: TI-99/4A bytecode

static const int ti99_conditions[] = {
    COND_CARRIED, COND_IN_ROOM, COND_PRESENT, COND_NOT_IN_ROOM,
    COND_NOT_CARRIED, COND_NOT_PRESENT, COND_IN_PLAY, COND_NOT_IN_PLAY,
    COND_AT_LOC, COND_NOT_AT_LOC, COND_FLAG_SET, COND_FLAG_CLEAR,
    COND_CARRYING_ANY, COND_CARRYING_NONE, COND_COUNTER_LE, COND_COUNTER_GT,
    COND_COUNTER_EQ, COND_NOT_MOVED, COND_MOVED
};

static int TI99OpcodeArgs(int op, ArgKind kinds[2])
{
    kinds[0] = kinds[1] = ARG_NONE;
    switch (op) {
    case TI99OP_GET_ITEM:
    case TI99OP_DROP_ITEM:
    case TI99OP_DESTROY_ITEM:
    case TI99OP_FORCE_CARRY:
        kinds[0] = ARG_ITEM;
        return 1;
    case TI99OP_GOTO_ROOM:
        kinds[0] = ARG_ROOM;
        return 1;
    case TI99OP_SET_FLAG:
    case TI99OP_CLEAR_FLAG:
        kinds[0] = ARG_FLAG;
        return 1;
    case TI99OP_MOVE_ITEM:
        /* The room comes first here. */
        kinds[0] = ARG_ROOM;
        kinds[1] = ARG_ITEM;
        return 2;
    case TI99OP_SWAP_ITEMS:
    case TI99OP_MOVE_TO_LOC_OF:
        kinds[0] = kinds[1] = ARG_ITEM;
        return 2;
    case TI99OP_TRY:
    case TI99OP_SET_COUNTER:
    case TI99OP_ADD_COUNTER:
    case TI99OP_SUB_COUNTER:
    case TI99OP_SWAP_ROOM:
    case TI99OP_SWAP_COUNTER:
        kinds[0] = ARG_OTHER;
        return 1;
    default:
        return 0;
    }
}

/* Decode one action line, from the byte after the block's length byte to its
   TI99OP_SUCCESS. The fallback code of a try block lies inside the line, so
   reading straight through covers it. */
static void AuditTI99Line(const char *list, const uint8_t *base,
    const uint8_t *line, const uint8_t *end)
{
    char where[48], what[32];
    const uint8_t *ip = line;

    snprintf(where, sizeof where, "%s +0x%tx", list, line - base);

    while (ip < end) {
        int opcode = *(ip++);
        ArgKind kinds[2];
        int count;

        if (opcode == TI99OP_SUCCESS)
            return;

        if (opcode >= TI99CND_CARRIED && opcode <= TI99CND_MOVED) {
            int cond = ti99_conditions[opcode - TI99CND_CARRIED];
            if (cond == COND_CARRYING_ANY || cond == COND_CARRYING_NONE)
                continue;
            if (ip >= end)
                break;
            snprintf(what, sizeof what, "condition %s", cond_names[cond]);
            Check(ConditionArgKind(cond), *(ip++), where, what);
            continue;
        }

        count = TI99OpcodeArgs(opcode, kinds);
        snprintf(what, sizeof what, "opcode %d", opcode);
        for (int i = 0; i < count && ip < end; i++)
            Check(kinds[i], *(ip++), where, what);
    }

    findings++;
    printf("  %s: line runs off the end of the action data\n", where);
}

static int AuditTI99(void)
{
    char list[32];
    int lines = 0;

    if (ti99_implicit_actions != NULL) {
        const uint8_t *end = ti99_implicit_actions + ti99_implicit_extent;
        const uint8_t *block = ti99_implicit_actions;
        while (block + 1 < end && block[0] != 0) {
            AuditTI99Line("implicit", ti99_implicit_actions, block + 2, end);
            lines++;
            if (block[1] == 0)
                break;
            block += 1 + block[1];
        }
    }

    if (ti99_explicit_actions == NULL || VerbActionOffsets == NULL)
        return lines;

    /* VerbActionOffsets has one entry per verb. The loader does not keep the
       verb count, but it pads the shorter word list with "." up to NumWords. */
    int num_verbs = GameHeader.NumWords;
    while (num_verbs > 0 && Verbs[num_verbs] != NULL && strcmp(Verbs[num_verbs], ".") == 0)
        num_verbs--;

    const uint8_t *end = ti99_explicit_actions + ti99_explicit_extent;
    for (int verb = 0; verb <= num_verbs; verb++) {
        const uint8_t *block = VerbActionOffsets[verb];
        snprintf(list, sizeof list, "verb %d", verb);
        while (block != NULL && block + 1 < end) {
            AuditTI99Line(list, ti99_explicit_actions, block + 2, end);
            lines++;
            if (block[1] == 0)
                break;
            block += 1 + block[1];
        }
    }
    return lines;
}

void glk_main(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL) {
        fprintf(stderr, "cond_audit: could not open window\n");
        return;
    }
    glk_set_window(Bottom);

    if (game_file == NULL) {
        fprintf(stderr, "usage: scott_cond_audit <game-file>\n");
        return;
    }

    dump_all = getenv("SCOTT_AUDIT_DUMP") != NULL;

    /* Populate sys[] the way the real glk_main does before DetectGame(). */
    extern const char *sysdict_i_am[];
    for (int i = 0; i < MAX_SYSMESS; i++)
        sys[i] = sysdict_i_am[i] ? sysdict_i_am[i] : "";

    GameIDType id = DetectGame(game_file);

    /* A compilation's "which game" menu has been printed by now; the runner
       reads from this line on. */
    printf("\n== cond_audit\n");
    if (id == UNKNOWN_GAME) {
        printf("unknown game\n");
        glk_exit();
    }

    int lines;
    if (CurrentSys == SYS_TI994A)
        lines = AuditTI99();
    else {
        AuditStandard();
        lines = GameHeader.NumActions + 1;
    }

    printf("game %d \"%s\" sys %d: %d lines; items 0..%d, highest named %d (%d checked); "
           "rooms 0..%d, highest %d (%d); flags highest %d (%d); %d findings\n",
        id, Game && Game->Title ? Game->Title : "", CurrentSys, lines,
        GameHeader.NumItems, highest[ARG_ITEM], checked[ARG_ITEM],
        GameHeader.NumRooms, highest[ARG_ROOM], checked[ARG_ROOM],
        highest[ARG_FLAG], checked[ARG_FLAG], findings);
    glk_exit();
}
