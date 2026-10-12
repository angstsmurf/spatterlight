//
//  dumpactions.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Debug dumps of a loaded database. Everything here prints through
//  debug_print(), which compiles to nothing unless DEBUG is defined.
//

#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "dumpactions.h"

/* Human-readable names for condition opcodes, indexed by opcode number.
   Used by DumpActions() when printing a debug dump of the action table.
   debug_print() compiles to nothing unless DEBUG is defined. */
#ifdef DEBUG
static const char * const ConditionList[] = {
    "<ERROR>",
    "CARRIED",
    "HERE",
    "PRESENT",
    "AT",
    "ROOMDESC",
    "NOP6",
    "NOP7",
    "NOTZERO",
    "ISZERO",
    "SOMETHING",
    "UNK11",
    "UNK12",
    "NODESTROYED",
    "DESTROYED",
    "CURLE",
    "CURGT",
    "CNTEQCNT",
    "CNTGTCNT", // 18
    "CUREQ", // 19
    "CNTEQ",
    "CNTGTE",
    "CNTZERO",
    "INROOMCNT",
    "NOP24",
    "NOP25",
    "OBJIN",
    "UNK27",
    "NOUNIS",
    "DICTWORD",
    "OBJFLAG",
    "MODE",
    "OUT OF RANGE",
};
#endif

// clang-format off

/* Human-readable names, opcodes, and argument counts for command opcodes.
   Opcodes 1–51 are implicit MESSAGE commands (print the message at that index).
   Opcodes 52+ are explicit commands listed here. Used by DumpActions(). */
typedef struct
{
    const char *name;
    int        opcode;
    int        count;
} Keyword;

static const Keyword CommandList[] =
{
    {"NOP",           0, 0 },
    {"GET",          52, 1 },
    {"DROP",         53, 1 },
    {"GOTO",         54, 1 },
    {"MEANS",        55, 2 },
    {"LIGHTSOUT",    56, 0 },
    {"LIGHTSON",     57, 0 },
    {"SET",          58, 1 },
    {"DESTROY",      59, 1 },
    {"CLEAR",        60, 1 },
    {"DEATH",        61, 0 },
    {"DROPIN",       62, 2 },
    {"GAMEOVER",     63, 0 },
    {"SETCNT",       64, 2 },
    {"MULTIPLY?",    65, 2 },
    {"INVENTORY",    66, 0 },
    {"SET0FLAG",     67, 0 },
    {"CLEAR0FLAG",   68, 0 },
    {"DONE",         69, 0 },
    {"CLS",          70, 0 },
    {"SAVE",         71, 0 },
    {"SWAP",         72, 2 },
    {"CONTINUE",     73, 0 },
    {"STEAL",        74, 1 },
    {"SAME",         75, 2 },
    {"LOOK",         76, 0 },
    {"DECCUR",       77, 0 },
    {"PRINTCUR",     78, 0 },
    {"SETCUR",       79, 1 },
    {"CNT2LOC",      80, 2 },
    {"SWAPCUR",      81, 1 },
    {"ADD",          82, 1 },
    {"SUB",          83, 1 },
    {"ECHONOUN",     84, 0 },
    {"PRINTOBJ",     85, 1 },
    {"ECHODICT",     86, 1 },
    {"PAUSE",        88, 0 },
    {"REDRAW?",      89, 0 },
    {"SHOWIMG",      90, 1 },
    {"DIVMOD",       91, 3 },
    {"NODOMORE",     92, 0 },
    {"DELAYQS",      93, 1 },
    {"GETEXIT",      94, 3 },
    {"SETEXIT",      95, 3 },
    {"SWAPCNT",      97, 2 },
    {"SETTOC",       98, 2 },
    {"SETTOA",       99, 2 },
    {"ADDCNT",      100, 2 },
    {"DECCNT",      101, 2 },
    {"PRNCNT",      102, 1 },
    {"ABS",         103, 1 },
    {"NEG",         104, 1 },
    {"AND",         105, 2 },
    {"OR",          106, 2 },
    {"XOR",         107, 2 },
    {"RAND",        108, 1 },
    {"CLRCNT",      109, 1 },
    {"SYSCMD",      110, 2 },
    {"GOTO0",       111, 0 },
    {"BEGIN",       112, 0 },
    {"LOOPBACK",    113, 0 },
    {"SKIPLP",      114, 0 },
    {"SKIPLPNXT",   115, 0 },
    {"DOMORE",      116, 0 },
    {"FMATCH",      117, 0 },
    {"DONE",        118, 0 },
    {"SETPROMPT",   119, 1 },
    {"GETWORDS",    120, 0 },
    {"SETWORDS",    121, 0 },
    {"PRNOBJART",   122, 1 },
    {"ADDCNT",      123, 2 },
    {"BREAK",       124, 0 },
    {"CPYFLAG",     125, 2 },
    {"INDTOCNT",    126, 2 },
    {"CNTTOIND",    127, 2 },
    {"PRINTMESS",   128, 1 },
    {"",             -1, 0 }
};

// clang-format on


/* Looks up the number of arguments a command opcode takes by searching
   the CommandList table. Returns 0 for unknown opcodes. */
static int NumberOfArguments(int opcode)
{
    int i = 0;
    while (CommandList[i].opcode != -1) {
        if (CommandList[i].opcode == opcode)
            return CommandList[i].count;
        i++;
    }
    return 0;
}

/* Prints the first dictionary word whose group matches the given index.
   Falls back to printing the raw numeric index if no match is found. */
void PrintDictWord(int idx, const DictWord *dict)
{
    for (; dict->Word != NULL; dict++) {
        if (dict->Group == idx) {
            debug_print("%s", dict->Word);
            return;
        }
    }
    debug_print("%d", idx);
}

/* Prints a single condition opcode and argument for debug output.
   Condition 0 is a bare argument (extra parameter for the previous condition).
   Condition 31 is a modifier: OR, NOT, parentheses, depending on its argument.
   Tracks negation and OR state across calls via the output parameters. */
static void PrintCondition(uint8_t condition, uint16_t argument, int *negate, int *mult, int *or)
{
    if (condition == 0) {
        debug_print(" %d", argument);
    } else if (condition == 31) {
        if (argument == 1) {
            debug_print(" OR(31) %d ", argument); // Arg 1 == OR
            * or = 1;
        } else if (argument == 3 || argument == 4) {
            debug_print(" NOT(31) %d ", argument);
            *negate = 1;
            *mult = (argument != 3);
        } else if (argument == 6) {
            debug_print(" ( ");
        } else if (argument == 7) {
            debug_print(" ) ");
        } else {
            debug_print(" MOD(31) %d ", argument);
            if (argument == 0)
                * or = 0;
        }
    } else if (condition > 40) {
        debug_print(" Condition %d out of range! Arg:%d\n", condition, argument);
    } else {
        debug_print(" ");
        if (*negate) {
            debug_print("!");
            if (!*mult)
                *negate = 0;
        }
        debug_print("%s(%d) %d", ConditionList[condition], condition, argument);
    }
}

/* Prints the text of a message by index, if it exists and is non-empty.
   Used by PrintCommand to show the actual message text alongside opcodes. */
static void PrintDebugMess(int index)
{
    if (index > GameHeader.NumMessages)
        return;
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        debug_print("\"%s\" ", message);
    }
}

/* Prints a single command opcode and its arguments for debug output.
   Opcodes 1–51 are implicit message-print commands. Opcode 128 (PRINTMESS)
   subtracts 76 from its argument to get the actual message index. Other
   opcodes are looked up in the CommandList table. */
static void PrintCommand(uint8_t opcode, uint8_t arg, uint8_t arg2, uint8_t arg3, int numargs)
{
    /* Opcodes 1–51 are shorthand for "print message N" */
    if (opcode > 0 && opcode <= 51) {
        debug_print(" MESSAGE %d ", opcode);
        PrintDebugMess(opcode);
        debug_print("\n");
        return;
    }

    int i = 0;
    int found = 0;
    while (CommandList[i].opcode != -1) {
        if (CommandList[i].opcode == opcode) {
            found = 1;
            debug_print(" %s(%d) ", CommandList[i].name, opcode);

            if (opcode == 128) {
                arg -= 76;
                debug_print("%d ", arg);
                PrintDebugMess(arg);
            } else if (numargs >= 1) {
                debug_print("%d ", arg);
                if (numargs >= 2) {
                    debug_print("%d ", arg2);
                    if (numargs >= 3) {
                        debug_print("%d ", arg3);
                    }
                }
            }

            break;
        }
        i++;
    }
    if (found)
        debug_print("\n");
    else {
        debug_print(" UNKNOWN(%d)\n", opcode);
    }
}

/* Prints a complete human-readable dump of all loaded actions, including
   verb/noun pairs, extra word parameters, condition chains, and command
   sequences. Annotates entries with comment strings if available. This is
   the main diagnostic tool for verifying that a database was loaded correctly. */
void DumpActions(char **comments)
{
    int negate_condition = 0;
    int negate_multiple = 0;
    int or_condition = 0;
    Action *ap = Actions;
    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        debug_print("\nAction %d: ", ct);
        if (ap->Verb > 0) {
            PrintDictWord(ap->Verb, Verbs);
            debug_print("(%d) ", ap->Verb);
            PrintDictWord(ap->NounOrChance, Nouns);
            debug_print("(%d)", ap->NounOrChance);
        } else {
            debug_print("(%d)(%d)", ap->Verb, ap->NounOrChance);
        }

        if (comments && comments[ct][0] != 0)
            debug_print("\nComment: \"%s\"\n", comments[ct]);
        else
            debug_print("\n");
        /* Print extra word parameters. The top 2 bits of each byte indicate
           the word type: 0=spare, 1=adverb, 2=participle (followed by an
           object word), 3=preposition. The lower 6 bits are the word index. */
        if (ap->NumWords) {
            debug_print("Extra words: ");
            int isobject = 0;
            for (int i = 0; i < ap->NumWords; i++) {
                debug_print("%d ", ap->Words[i]);
                if (isobject) {
                    debug_print("(object:) ");
                    isobject = 0;
                } else {
                    switch (ap->Words[i] >> 6) {
                    case 3:
                        debug_print("(prep:) ");
                        break;
                    case 2:
                        debug_print("(participle:) ");
                        isobject = 1;
                        break;
                    case 1:
                        debug_print("(adverb:) ");
                        break;
                    case 0:
                        debug_print("(spare:) ");
                        break;
                    }
                }
                if (ap->Words[i] <= GameHeader.NumNouns) {
                    debug_print("(");
                    PrintDictWord(ap->Words[i], Nouns);
                    debug_print(") ");
                } else if ((ap->Words[i] & 0x3f) <= GameHeader.NumPreps) {
                    debug_print("(");
                    PrintDictWord((ap->Words[i] & 0x3f), Prepositions);
                    debug_print(") ");
                } else {
                    debug_print("(%d) ", (ap->Words[i] & 0x3f));
                }
            }
            debug_print("\n");
        }
        negate_condition = 0;
        negate_multiple = 0;
        if (ap->Conditions[0] == 0 && ap->Conditions[1] == 0 && ap->Conditions[2] == 255)
            debug_print("No conditions");
        else {
            debug_print("Conditions:");
            for (int i = 0; ap->Conditions[i] != 255; i += 2) {
                PrintCondition(ap->Conditions[i], ap->Conditions[i + 1], &negate_condition, &negate_multiple, &or_condition);
            }
        }
        negate_condition = 0;
        negate_multiple = 0;
        debug_print("\n");

        /* Print the command sequence. Some commands have variable argument
           counts that depend on the values of their arguments, requiring
           special-case detection here. */
        for (int i = 0; i <= ap->CommandLength; i++) {
            uint8_t command = ap->Commands[i];
            int numargs = NumberOfArguments(command);

            /* Detect extended argument forms for specific commands */
            if (numargs > 0 && i < ap->CommandLength - 2 && ap->Commands[i + 1] == 131 && ap->Commands[i + 2] == 230) {
                numargs++;
            } else if (command == 99 && ap->Commands[i + 2] > 63) {
                numargs = 3;
            } else if (command == 131 && ap->Commands[i + 2] > 31) {
                numargs = 3;
            } else if ((command == 82 || command == 119) && ap->Commands[i + 1] > 127) {
                numargs = 2;
            }

            int arg1 = 0;
            if (numargs >= 1)
                arg1 = ap->Commands[i + 1];
            int arg2 = 0;
            if (numargs >= 2)
                arg2 = ap->Commands[i + 2];
            int arg3 = 0;
            if (numargs >= 3)
                arg3 = ap->Commands[i + 3];
            PrintCommand(command, arg1, arg2, arg3, numargs);
            i += numargs;
        }
        debug_print("\n");
        ap++;
    }
}

/* Prints all parsed header fields for diagnostic purposes. */
void PrintHeaderInfo(const Header h)
{
    debug_print("Number of items =\t%d\n", h.NumItems);
    debug_print("sum of actions =\t%d\n", h.ActionSum);
    debug_print("Number of nouns =\t%d\n", h.NumNouns);
    debug_print("Number of verbs =\t%d\n", h.NumVerbs);
    debug_print("Number of rooms =\t%d\n", h.NumRooms);
    debug_print("Max carried items =\t%d\n", h.MaxCarry);
    debug_print("Player start location =\t%d\n", h.PlayerRoom);
    debug_print("Number of messages =\t%d\n", h.NumMessages);
    debug_print("Treasure room =\t%d\n", h.TreasureRoom);
    debug_print("Light source turns =\t%d\n", h.LightTime);
    debug_print("Number of prepositions =\t%d\n", h.NumPreps);
    debug_print("Number of adverbs =\t%d\n", h.NumAdverbs);
    debug_print("Number of actions =\t%d\n", h.NumActions);
    debug_print("Number of treasures =\t%d\n", h.Treasures);
    debug_print("Number of synonym strings =\t%d\n", h.NumSubStr);
    debug_print("Unknown1 =\t%d\n", h.Unknown1);
    debug_print("Number of object images =\t%d\n", h.NumObjImg);
    debug_print("Unknown2 =\t%d\n", h.Unknown2);
}
