//
//  loaddatabase.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "common_utils.h"
#include "gameinfo.h"
#include "graphics.h"
#include "read_le16.h"
#include "loaddatabase.h"

Synonym *Substitutions;
uint8_t *MysteryValues;

/* Human-readable names for condition opcodes, indexed by opcode number.
   Used by DumpActions() when printing a debug dump of the action table. */
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


/* Reads a complete line from the plaintext database, stripping surrounding
   quote marks. Used to read comment/annotation lines that follow the main
   game data. Returns NULL on EOF, or an empty string for empty quoted lines.
   Non-ASCII characters are replaced with '?'. */
static const char *ReadWholeLine(FILE *f)
{
    char tmp[1024];
    int c;
    int ct = 0;

    /* Skip whitespace and find the opening quote mark */
    do {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
        if (c == '"') {
            c = fgetc(f);
            if (c == '"')
                return "\0";
            break;
        }
    } while (isspace((unsigned char)c));

    /* Read characters until end of line or EOF */
    while (c != '\n' && c != '\r' && c != EOF) {
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        if (c >= 32 && c <= 126)
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
        c = fgetc(f);
    }

    /* Find the closing quote and truncate there */
    int closequote = 0;
    for (int i = ct - 1; i > 0; i--) {
        if (tmp[i] == '"') {
            closequote = i;
            break;
        }
    }

    if (closequote)
        ct = closequote;
    else if (ct > 1)
        ct--;
    else
        return "\0";

    tmp[ct] = 0;
    char *t = MemAlloc(ct + 1);
    memcpy(t, tmp, ct + 1);
    return t;
}

/* Reads a quoted string from the plaintext database file. Strings are
   delimited by double quotes; a pair of consecutive quotes ("") inside a
   string represents a literal quote character. Backticks are converted to
   double quotes. Non-ASCII characters are replaced with '?', and bare CR
   characters are stripped (assumed to be part of CRLF line endings).
   Returns the allocated string and sets *length to its size including the
   null terminator. */
static char *ReadString(FILE *f, size_t *length)
{
    char tmp[1024];
    int c;
    int ct = 0;

    /* Skip everything until the opening quote */
    do {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF before string");
    } while (c != '"');

    /* Read string contents until the closing quote */
    do {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        /* A pair of consecutive quotes is an escaped literal quote;
           a single quote followed by something else ends the string */
        if (c == '"') {
            int nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"';

        /* Ensure a valid Glk newline is sent. */
        if (c == '\n')
            tmp[ct++] = 10;
        /* Special case: assume CR is part of CRLF in a
         * DOS-formatted file, and ignore it.
         */
        else if (c == 13)
            ;
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        else if (c >= 32 && c <= 126)
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    } while (ct < 1000);

    tmp[ct] = 0;
    *length = ct + 1;
    char *t = MemAlloc(*length);
    memcpy(t, tmp, *length);
    return t;
}

/* Reads dictionary words from the plaintext database. Each string read from
   the file contains comma-separated words that belong to the same synonym
   group (e.g. "GET,TAKE,GRAB,"). Words within a group are interchangeable.
   The group number increments with each string read from the file.
   Builds the dictionary in a stack buffer, then copies to a heap-allocated
   array terminated by a NULL Word sentinel. */
static DictWord *ReadDictWordsPC(FILE *f, int numstrings, int loud)
{
    DictWord dictionary[1024];
    char *str = NULL;
    int group = 0;
    int index = 0;

    for (int i = 0; i < numstrings; i++) {
        size_t length;
        str = ReadString(f, &length);
        if (str == NULL || str[0] == '\0')
            continue;

        /* Split the comma-separated string into individual dictionary words */
        int lastcomma = 0;
        int commapos = 0;
        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] != ',')
                continue;

            while (str[j] == ',') {
                str[j] = '\0';
                commapos = j;
                j++;
            }

            int seglen = commapos - lastcomma;
            if (seglen > 0) {
                dictionary[index].Word = MemAlloc(seglen);
                memcpy(dictionary[index].Word, &str[lastcomma + 1], seglen);
                dictionary[index].Group = group;
                index++;
            }
            lastcomma = commapos;
        }
        free(str);
        str = NULL;
        group++;
    }

    /* Copy the stack-built dictionary to a right-sized heap allocation */
    dictionary[index].Word = NULL;
    size_t dictsize = (index + 1) * sizeof(DictWord);
    DictWord *finaldict = MemAlloc(dictsize);
    memcpy(finaldict, dictionary, dictsize);

    if (loud)
        for (int j = 0; finaldict[j].Word != NULL; j++)
            debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);

    return finaldict;
}

/* Reads substitution (synonym) entries from the plaintext database.
   Each string contains comma-separated tokens. An '=' prefix marks the next
   token as a replacement string; all preceding synonym strings in the current
   group are mapped to that replacement. For example, the string
   ",HELLO,HI,=,GREET," means both "HELLO" and "HI" expand to "GREET".
   Builds entries in a stack buffer, then copies to the heap. */
static Synonym *ReadSubstitutions(FILE *f, int numstrings, int loud)
{
    Synonym syn[1024];
    char *str = NULL;
    char *replace = NULL;
    int index = 0;
    int firstsyn = 0;

    for (int i = 0; i < numstrings; i++) {
        size_t length;
        str = ReadString(f, &length);
        if (loud)
            debug_print("Read synonym string \"%s\"\n", str);
        if (str == NULL || str[0] == '\0')
            continue;

        int lastcomma = 0;
        int commapos = 0;
        int foundrep = 0;
        int nextisrep = 0;

        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] != ',')
                continue;

            while (str[j] == ',') {
                str[j] = '\0';
                commapos = j;
                j++;
            }

            if (nextisrep) {
                foundrep = 1;
                nextisrep = 0;
            } else if (str[j] == '=') {
                nextisrep = 1;
            }

            if (foundrep) {
                int seglen = commapos - lastcomma - 1;
                if (seglen > 0) {
                    free(replace);
                    replace = MemAlloc(seglen);
                    memcpy(replace, &str[lastcomma + 2], seglen);
                    if (loud)
                        debug_print("Found new replacement string \"%s\"\n", replace);
                }
            } else {
                int seglen = commapos - lastcomma;
                if (seglen > 0) {
                    syn[index].SynonymString = MemAlloc(seglen);
                    memcpy(syn[index].SynonymString, &str[lastcomma + 1], seglen);
                    if (loud)
                        debug_print("Found new synonym string \"%s\"\n", syn[index].SynonymString);
                    index++;
                }
            }

            if (foundrep) {
                size_t replen = strlen(replace) + 1;
                for (int k = firstsyn; k < index; k++) {
                    syn[k].ReplacementString = MemAlloc(replen);
                    memcpy(syn[k].ReplacementString, replace, replen);
                    if (loud)
                        debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", syn[k].SynonymString, k, syn[k].ReplacementString);
                }
                firstsyn = index;
                foundrep = 0;
            }

            lastcomma = commapos;
        }
        free(str);
        str = NULL;
    }

    free(replace);
    syn[index].SynonymString = NULL;

    size_t synsize = (index + 1) * sizeof(Synonym);
    Synonym *finalsyns = MemAlloc(synsize);
    memcpy(finalsyns, syn, synsize);

    if (loud)
        for (int j = 0; finalsyns[j].SynonymString != NULL; j++)
            debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);

    return finalsyns;
}

static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *length);

/* Binary-format variant of ReadSubstitutions. Reads substitution entries from
   an in-memory binary database using length-prefixed strings (via ReadPlusString)
   instead of file I/O. Same comma-delimited format and '=' replacement logic
   as the plaintext version. Advances *startpointer past the consumed data. */
static Synonym *ReadSubstitutionsBinary(uint8_t **startpointer, int numstrings, int loud)
{
    Synonym syn[1024];
    uint8_t *ptr = *startpointer;

    char *str = NULL;
    char *replace = NULL;
    int index = 0;
    int firstsyn = 0;

    for (int i = 0; i < numstrings; i++) {
        size_t length;
        ptr = ReadPlusString(ptr, &str, &length);
        while (length == 0 && i == 0)
            ptr = ReadPlusString(ptr, &str, &length);
        if (loud)
            debug_print("Read synonym string %d, \"%s\"\n", i, str);
        if (str == NULL || str[0] == 0)
            continue;

        int lastcomma = 0;
        int commapos = 0;
        int foundrep = 0;
        int nextisrep = 0;

        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] != ',')
                continue;

            while (str[j] == ',') {
                str[j] = '\0';
                commapos = j;
                j++;
            }

            if (nextisrep) {
                foundrep = 1;
                nextisrep = 0;
            } else if (str[j] == '=') {
                nextisrep = 1;
            }

            if (foundrep) {
                int seglen = commapos - lastcomma - 1;
                if (seglen > 0) {
                    free(replace);
                    replace = MemAlloc(seglen);
                    memcpy(replace, &str[lastcomma + 2], seglen);
                    if (loud)
                        debug_print("Found new replacement string \"%s\"\n", replace);
                }
            } else {
                int seglen = commapos - lastcomma;
                if (seglen > 0) {
                    syn[index].SynonymString = MemAlloc(seglen);
                    memcpy(syn[index].SynonymString, &str[lastcomma + 1], seglen);
                    if (loud)
                        debug_print("Found new synonym string \"%s\"\n", syn[index].SynonymString);
                    index++;
                }
            }

            if (foundrep) {
                size_t replen = strlen(replace) + 1;
                for (int k = firstsyn; k < index; k++) {
                    syn[k].ReplacementString = MemAlloc(replen);
                    memcpy(syn[k].ReplacementString, replace, replen);
                    if (loud)
                        debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", syn[k].SynonymString, k, syn[k].ReplacementString);
                }
                firstsyn = index;
                foundrep = 0;
            }

            lastcomma = commapos;
        }
        free(str);
        str = NULL;
    }

    free(replace);
    syn[index].SynonymString = NULL;

    size_t synsize = (index + 1) * sizeof(Synonym);
    Synonym *finalsyns = MemAlloc(synsize);
    memcpy(finalsyns, syn, synsize);

    if (loud)
        for (int j = 0; finalsyns[j].SynonymString != NULL; j++)
            debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);

    *startpointer = ptr;
    return finalsyns;
}

char **Comments;

/* Reads the comment/annotation section that follows the main game data in
   the plaintext database. These are human-readable descriptions of each action,
   used by DumpActions() to annotate the debug output. The resulting NULL-
   terminated array is stored in the global Comments pointer. */
static void ReadComments(FILE *f, int loud)
{
    const char *strings[1024];
    int count = 0;

    const char *comment;
    while ((comment = ReadWholeLine(f)) != NULL && count < 1024)
        strings[count++] = comment;

    if (count == 0)
        return;

    Comments = MemAlloc((count + 1) * sizeof(char *));
    for (int i = 0; i < count; i++) {
        Comments[i] = (char *)strings[i];
        if (loud)
            debug_print("Comment %d:\"%s\"\n", i, Comments[i]);
    }
    Comments[count] = NULL;
}

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
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == idx) {
            debug_print("%s", dict->Word);
            return;
        }
        dict++;
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
static void DumpActions(void)
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

        if (Comments && Comments[ct][0] != 0)
            debug_print("\nComment: \"%s\"\n", Comments[ct]);
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

/* Reads a single numeric value from the plaintext database.
   Each value occupies its own line. Returns 0 on parse failure. */
static uint8_t ReadNum(FILE *f)
{
    int num;
    if (fscanf(f, "%d\n", &num) != 1)
        return 0;
    return num;
}

/* Reads a single action entry from the plaintext database.
   The first byte encodes two 4-bit fields: the high nibble is the number of
   extra word parameters, and the low nibble is the command byte count.
   Next come verb and noun (or auto-trigger chance), then extra words,
   then a chain of 16-bit condition entries (high bit set on the last one),
   and finally the command bytes. */
static void ReadAction(FILE *f, Action *ap)
{
    /* High nibble = number of extra words, low nibble = command length */
    uint8_t length = ReadNum(f);
    ap->NumWords = length >> 4;
    ap->CommandLength = length & 0xf;
    ap->Verb = ReadNum(f);
    if (ap->Verb & 128) {
        Fatal("2-byte verbs unimplemented");
    }
    ap->NounOrChance = ReadNum(f);
    if (ap->NounOrChance & 128) {
        Fatal("2-byte nouns unimplemented");
    }
    ap->Words = MemAlloc(ap->NumWords);

    for (int i = 0; i < ap->NumWords; i++) {
        ap->Words[i] = ReadNum(f);
    }

    /* Read conditions: each is a big-endian 16-bit value where the high bit
       signals the last condition in the chain. The lower 15 bits encode
       a 5-bit condition opcode and a 10-bit argument. Stored as alternating
       condition/argument pairs, terminated by a 255 sentinel. */
    uint16_t conditions[1024];
    int condargs = 0;
    for (;;) {
        uint16_t raw = ReadNum(f) * 256 + ReadNum(f);
        int last = raw & 0x8000;
        if (condargs + 2 > 1024)
            Fatal("Broken database!");
        conditions[condargs++] = raw & 0x1f;
        conditions[condargs++] = (raw >> 5) & 0x3ff;
        if (last)
            break;
    }
    ap->Conditions = MemAlloc((condargs + 1) * sizeof(uint16_t));
    memcpy(ap->Conditions, conditions, condargs * sizeof(uint16_t));
    ap->Conditions[condargs] = 255;

    /* Read command bytes directly into a heap allocation */
    int cmdlen = ap->CommandLength + 1;
    ap->Commands = MemAlloc(cmdlen);
    for (int i = 0; i < cmdlen; i++) {
        ap->Commands[i] = ReadNum(f);
    }
}

/* Prints all parsed header fields for diagnostic purposes. */
static void PrintHeaderInfo(const Header h)
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
    debug_print("Unknown3 =\t%d\n", h.Unknown2);
}

/* Matches the game's title/ID string against the known games database.
   Sets the global Game pointer on success. Returns 1 if a match was found,
   0 if the game is unrecognized. */
static int SetGame(const char *id_string, size_t length)
{
    for (int i = 0; games[i].title != NULL; i++)
        if (strncmp(id_string, games[i].ID_string, length) == 0) {
            Game = &games[i];
            return 1;
        }
    return 0;
}

/* Attempts to load a .PAK image file by constructing its full path from the
   game directory and a short name (e.g. "R00", "B03", "S01"). On success,
   populates the imgrec with the file data, size, and filename. Returns 1 if
   the file was found and loaded, 0 otherwise. */
int FindAndAddImageFile(const char *shortname, imgrec *rec)
{
    int result = 0;
    size_t pathlen = DirPathLength + 9;
    char *filename = MemAlloc(pathlen);
    int n = snprintf(filename, pathlen, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        rec->Data = ReadFileIfExists(filename, &rec->Size);
        if (rec->Data == NULL) {
            fprintf(stderr, "Could not find or read image file %s\n", filename);
        } else {
            size_t namelen = strlen(shortname) + 1;
            rec->Filename = MemAlloc(namelen);
            memcpy(rec->Filename, shortname, namelen);
            result = 1;
            debug_print("Found and read image file %s\n", filename);
        }
    }
    free(filename);
    return result;
}

/* Loads a game database from a plaintext (DOS/PC) format file. The plaintext
   format starts with a quoted title string (used to identify the game),
   followed by comma-separated header values, then sections for actions,
   dictionary words, rooms, messages, items, adverbs, prepositions,
   substitutions, object images, mystery values, and comments.
   On success, closes the file and returns the game ID. On failure (unrecognized
   game or malformed data), returns UNKNOWN_GAME with the file left open so
   the caller can retry with the binary loader. */
int LoadDatabasePlaintext(FILE *f, int loud)
{
    int num_items, action_sum, num_actions, num_verbs, num_nouns, num_rooms;
    int max_carry, player_room, num_treasures, light_time, num_messages;
    int treasure_room, num_adverbs, num_preps, num_substr;
    int unknown1, num_obj_img, unknown2;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;

    /* The title string comes first and identifies which game this is */
    size_t length;
    char *title = ReadString(f, &length);

    if (!SetGame(title, length)) {
        free(title);
        return UNKNOWN_GAME;
    }

    /* Parse the 18 comma-separated header values that follow the title */
    if (fscanf(f, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            &num_items, &action_sum, &num_nouns, &num_verbs, &num_rooms,
            &max_carry, &player_room, &num_messages, &treasure_room,
            &light_time, &num_preps, &num_adverbs, &num_actions,
            &num_treasures, &num_substr, &unknown1, &num_obj_img, &unknown2)
        < 10) {
        if (loud)
            debug_print("Invalid database(bad header)\n");
        return UNKNOWN_GAME;
    }
    GameHeader.NumItems = num_items;
    Counters[43] = num_items;
    Items = (Item *)MemAlloc(sizeof(Item) * (num_items + 1));
    GameHeader.NumActions = num_actions;
    GameHeader.ActionSum = action_sum;
    Actions = (Action *)MemAlloc(sizeof(Action) * (num_actions + 1));
    GameHeader.NumVerbs = num_verbs;
    GameHeader.NumNouns = num_nouns;
    GameHeader.NumRooms = num_rooms;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (num_rooms + 1));
    GameHeader.MaxCarry = max_carry;
    GameHeader.PlayerRoom = player_room;
    MyLoc = player_room;
    GameHeader.Treasures = num_treasures;
    GameHeader.LightTime = light_time;
    GameHeader.NumMessages = num_messages;
    Messages = MemAlloc(sizeof(char *) * (num_messages + 1));
    GameHeader.TreasureRoom = treasure_room;
    GameHeader.NumAdverbs = num_adverbs;
    GameHeader.NumPreps = num_preps;
    GameHeader.Unknown1 = unknown1;
    GameHeader.NumSubStr = num_substr;
    GameHeader.Unknown2 = unknown2;
    GameHeader.NumObjImg = num_obj_img;
    ObjectImages = (ObjectImage *)MemAlloc(sizeof(ObjectImage) * (num_obj_img + 1));
    MysteryValues = MemAlloc(unknown2 + 1);

    Counters[35] = num_rooms;
    Counters[34] = treasure_room;
    Counters[42] = max_carry;
    SetBit(35); // Graphics on

    if (loud) {
        PrintHeaderInfo(GameHeader);
    }

    /* Load actions — each action has a verb/noun trigger, optional extra
       words, a condition chain, and a command sequence */
    ct = 0;
    ap = Actions;
    if (loud)
        debug_print("Reading %d actions.\n", num_actions);
    while (ct < num_actions + 1) {
        ReadAction(f, ap);
        ap++;
        ct++;
    }

    /* Load verb and noun dictionaries */
    if (loud)
        debug_print("Reading %d verbs.\n", num_verbs);
    Verbs = ReadDictWordsPC(f, GameHeader.NumVerbs + 1, loud);
    if (loud)
        debug_print("Reading %d nouns.\n", num_nouns);
    Nouns = ReadDictWordsPC(f, GameHeader.NumNouns + 1, loud);

    /* Load room exit tables: 6 compass directions + Exits[6] (message index
       for the room description, offset by 76) + Image (room image index) */
    rp = Rooms;
    if (loud)
        debug_print("Reading %d rooms.\n", num_rooms);
    for (ct = 0; ct < num_rooms + 1; ct++) {
        if (fscanf(f, "%d,%d,%d,%d,%d,%d,%d,%d\n", &rp->Exits[0], &rp->Exits[1],
                &rp->Exits[2], &rp->Exits[3], &rp->Exits[4],
                &rp->Exits[5], &rp->Exits[6], &rp->Image)
            != 8) {
            debug_print("Bad room line (%d)\n", ct);
            return UNKNOWN_GAME;
        }
        rp++;
    }

    /* Load message strings */
    if (loud)
        debug_print("Reading %d messages.\n", num_messages);
    for (ct = 0; ct < num_messages + 1; ct++) {
        Messages[ct] = ReadString(f, &length);
        if (loud)
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }

    /* Resolve room descriptions: Exits[6] holds a message index offset by 76 */
    for (ct = 0; ct < num_rooms + 1; ct++) {
        if (Rooms[ct].Exits[6] == 0)
            Rooms[ct].Text = "";
        else
            Rooms[ct].Text = Messages[Rooms[ct].Exits[6] - 76];
        if (loud)
            debug_print("Room description of room %d: \"%s\"\n", ct, Rooms[ct].Text);
    }

    /* Load items: each has a quoted text string followed by comma-separated
       location, dictionary word index, and flag values */
    ct = 0;
    if (loud)
        debug_print("Reading %d items.\n", num_items);
    int loc, dictword, flag;
    ip = Items;
    while (ct < num_items + 1) {
        ip->Text = ReadString(f, &length);
        if (loud)
            debug_print("Item %d: \"%s\"\n", ct, ip->Text);
        if (fscanf(f, ",%d,%d,%d\n", &loc, &dictword, &flag) != 3) {
            debug_print("Bad item line (%d)\n", ct);
            return UNKNOWN_GAME;
        }
        ip->Location = (uint8_t)loc;
        ip->Dictword = (uint8_t)dictword;
        ip->Flag = (uint8_t)flag;
        if (loud && (ip->Location == CARRIED || ip->Location <= GameHeader.NumRooms))
            debug_print("Location of item %d: %d, \"%s\"\n", ct, ip->Location,
                ip->Location == CARRIED ? "CARRIED" : Rooms[ip->Location].Text);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }

    /* Load additional dictionaries: adverbs and prepositions */
    if (loud)
        debug_print("Reading %d adverbs.\n", GameHeader.NumAdverbs);
    Adverbs = ReadDictWordsPC(f, GameHeader.NumAdverbs + 1, loud);
    if (loud)
        debug_print("Reading %d prepositions.\n", GameHeader.NumPreps);
    Prepositions = ReadDictWordsPC(f, GameHeader.NumPreps + 1, loud);

    /* Load text substitutions (synonym expansions) */
    Substitutions = ReadSubstitutions(f, GameHeader.NumSubStr + 1, loud);

    /* Load object image mappings: each entry maps a room and object to an
       image index, controlling which graphics are shown for item sprites */
    ObjectImage *objimg = ObjectImages;
    if (loud)
        debug_print("Reading %d object image values.\n", num_obj_img + 1);
    for (ct = 0; ct <= num_obj_img; ct++) {
        if (fscanf(f, "%d,%d,%d\n", &objimg->Room, &objimg->Object, &objimg->Image)
            != 3) {
            debug_print("Bad object image line (%d)\n", ct);
            return UNKNOWN_GAME;
        }
        if (loud) {
            debug_print("Object image %d ", ct);
            debug_print("room: %d object: %d image: %d\n", objimg->Room, objimg->Object, objimg->Image);
        }
        objimg++;
    }

    /* Load unknown/mystery values — their purpose is not yet understood */
    if (loud)
        debug_print("Reading %d unknown values.\n", unknown2 + 1);
    for (ct = 0; ct < unknown2 + 1; ct++) {
        MysteryValues[ct] = ReadNum(f);
    }

    /* Load action comments (human-readable annotations) */
    ReadComments(f, loud);

    if (loud) {
        debug_print("\nLoad Complete.\n\n");
        DumpActions();
    }
    fclose(f);

    /* Build the image file table by looking for .PAK files on disk.
       Images are organized in three groups: room images ('R'), item/object
       images ('B'), and special images ('S'), each numbered from 0. */
    int numimages = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;

    Images = MemAlloc((numimages + 1) * sizeof(imgrec));
    debug_print("\nTotal number of images:%d\n", numimages);

    int i, recidx = 0, imgidx = 0;
    char type = 'R';
    for (i = 0; i < numimages; i++) {
        if (i == Game->no_of_room_images) {
            type = 'B';
            imgidx = 0;
        }
        if (i == Game->no_of_room_images + Game->no_of_item_images) {
            type = 'S';
            imgidx = 0;
        }
        char *shortname = ShortNameFromType(type, imgidx);
        if (FindAndAddImageFile(shortname, &Images[recidx]))
            recidx++;
        free(shortname);
        imgidx++;
    }
    Images[recidx].Filename = NULL;

    CurrentSys = SYS_MSDOS;

    return CurrentGame;
}

/* Reads a length-prefixed string from an in-memory binary database.
   The first byte is the string length; 0 or 255 indicate an empty string.
   Backticks are converted to double quotes (same convention as the plaintext
   format). Returns the advanced pointer past the consumed bytes. */
static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *len)
{
    uint8_t length = *ptr++;
    if (length == 0 || length == 255) {
        *string = MemCalloc(1);
        *len = 0;
        return ptr;
    }

    char *str = MemAlloc(length + 1);
    for (int i = 0; i < length; i++) {
        str[i] = *ptr++;
        if (str[i] == '`')
            str[i] = '"';
    }
    str[length] = 0;

    *string = str;
    *len = length;
    return ptr;
}

/* Loads message strings from an in-memory binary database. On Atari ST,
   empty strings (except the first) are skipped — the ST format sometimes
   inserts padding zero bytes between messages. Advances *ptr past all
   consumed data. */
char **LoadMessages(int numstrings, uint8_t **ptr)
{
    if (numstrings == 0)
        numstrings = 0xffff;
    char *str;
    char **target = MemAlloc(numstrings * sizeof(char *));
    int i;
    for (i = 0; i < numstrings; i++) {
        size_t length;
        *ptr = ReadPlusString(*ptr, &str, &length);
        if (CurrentSys == SYS_ST) {
            while (str[0] == 0 && i != 0)
                *ptr = ReadPlusString(*ptr, &str, &length);
        }
        debug_print("Message %d: \"%s\"\n", i, str);
        target[i] = str;
    }
    return target;
}

/* Extracts the 16-word binary header into named fields. The binary format
   has no explicit action count or treasure count (na and tr are set to 0);
   the action count is determined at runtime by parsing until ActionSum bytes
   are consumed. Some header slots (h[14], h[15]) have unknown purposes. */
static void ParseHeader(uint16_t *h,
    int *num_items, int *action_sum, int *num_nouns, int *num_verbs,
    int *num_rooms, int *max_carry, int *player_room, int *num_messages,
    int *treasure_room, int *light_time, int *num_preps, int *num_adverbs,
    int *num_actions, int *num_treasures, int *num_substr,
    int *unknown1, int *num_obj_img, int *unknown2)
{
    *num_items = h[0];
    *action_sum = h[1];
    *num_nouns = h[2];
    *num_verbs = h[3];
    *num_rooms = h[4];
    *max_carry = h[5];
    *player_room = h[6];
    *num_messages = h[7];
    *treasure_room = h[8];
    *light_time = h[9];
    *num_preps = h[10];
    *num_adverbs = h[11];
    *num_actions = 0;
    *num_treasures = 0;
    *num_substr = h[12];
    *unknown1 = h[15];
    *num_obj_img = h[13];
    *unknown2 = 0;
}

/* Reads 16 raw 16-bit header values from the binary database into the global
   header array. Atari ST uses big-endian byte order; all other platforms use
   little-endian. Returns a pointer to the start of the data following the
   header (backs up 2 bytes because the last header word overlaps with the
   action data that follows). */
static uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 16; i++) {
        if (CurrentSys == SYS_ST)
            value = READ_BE_UINT16_AND_ADVANCE(&ptr);
        else
            value = READ_LE_UINT16_AND_ADVANCE(&ptr);
        header[i] = value;
        debug_print("Header value %d: %d\n", i, header[i]);
    }
    return ptr - 2;
}

/* Binary-format variant of ReadDictWordsPC. Reads dictionary words from an
   in-memory binary database using length-prefixed strings (via ReadPlusString).
   Same comma-delimited grouping as the plaintext version. Empty strings
   (padding bytes) are skipped. Advances *pointer past all consumed data. */
DictWord *ReadDictWords(uint8_t **pointer, int numstrings, int loud)
{
    uint8_t *ptr = *pointer;
    DictWord dictionary[1024];
    DictWord *dw = dictionary;
    char *str = NULL;
    int group = 0;
    int index = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t strlength;
        ptr = ReadPlusString(ptr, &str, &strlength);
        /* Skip padding zero bytes that appear in some binary formats */
        while (str[0] == 0)
            ptr = ReadPlusString(ptr, &str, &strlength);
        debug_print("Read dictionary string \"%s\"\n", str);
        /* Split comma-separated words into individual dictionary entries */
        int lastcomma = 0;
        int commapos = 0;
        for (int j = 0; j <= strlength && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while (str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                int length = commapos - lastcomma;
                if (length > 0) {
                    dw->Word = MemAlloc(length);
                    memcpy(dw->Word, &str[lastcomma + 1], length);
                    dw->Group = group;
                    debug_print("Dictword %d: %s (%d)\n", index, dw->Word, dw->Group);
                    dw = &dictionary[++index];
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
        group++;
    }
    dictionary[index].Word = NULL;
    int dictsize = (index + 1) * sizeof(DictWord);
    DictWord *finaldict = (DictWord *)MemAlloc(dictsize);
    memcpy(finaldict, dictionary, dictsize);
    *pointer = ptr;
    if (loud)
        for (int j = 0; dictionary[j].Word != NULL; j++)
            debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);
    return finaldict;
}

/* Validates that key header fields fall within expected ranges for a
   legitimate Plus game database. Returns 1 if valid, 0 if any field
   is out of range. Used by the binary loader to reject corrupt data. */
int SanityCheckHeader(void)
{
    int16_t v = GameHeader.NumItems;
    if (v < 10 || v > 500)
        return 0;
    v = GameHeader.NumNouns;
    if (v < 50 || v > 190)
        return 0;
    v = GameHeader.NumVerbs;
    if (v < 30 || v > 190)
        return 0;
    v = GameHeader.NumRooms;
    if (v < 10 || v > 100)
        return 0;

    return 1;
}

/* Loads a game database from an in-memory binary image (previously read from
   a platform-specific file: C64, Apple II, Atari 8-bit, or Atari ST).
   The binary format is more compact than the plaintext version: strings are
   length-prefixed, header values are 16-bit words, and the action count is
   not stored explicitly — actions are read until ActionSum bytes are consumed.
   The Atari ST versions of Spider-Man and Fantastic Four require various
   pointer adjustments due to an inconsistent mix of 8-bit and 16-bit values.
   Returns 1 on success, 0 on failure. */
int LoadDatabaseBinary(void)
{
    int num_items, num_actions, num_verbs, num_nouns, num_rooms;
    int max_carry, player_room, num_treasures, num_messages;
    int treasure_room, num_adverbs, num_preps, num_substr, num_obj_img;
    int ct;

    Action *ap;

    /* The binary header starts at offset 0x32 in the raw file image */
    int offset = 0x32;

    if (memlen <= offset)
        return 0;

    int isSTSpiderman = 0, isSTFantastic4 = 0;

#pragma mark header

    uint8_t *ptr = ReadHeader(mem + offset);
    uint8_t *end = mem + memlen;

    int action_sum, light_time, unknown1, unknown2;
    ParseHeader(header, &num_items, &action_sum, &num_nouns, &num_verbs,
        &num_rooms, &max_carry, &player_room, &num_messages, &treasure_room,
        &light_time, &num_preps, &num_adverbs, &num_actions, &num_treasures,
        &num_substr, &unknown1, &num_obj_img, &unknown2);

    /* Identify specific Atari ST games that need special pointer adjustments */
    if (CurrentSys == SYS_ST && action_sum == 5159)
        isSTSpiderman = 1;

    if (CurrentSys == SYS_ST && action_sum == 6991)
        isSTFantastic4 = 1;

    GameHeader.NumItems = num_items;
    Counters[43] = num_items;
    GameHeader.ActionSum = action_sum;
    GameHeader.NumVerbs = num_verbs;
    GameHeader.NumNouns = num_nouns;
    GameHeader.NumRooms = num_rooms;

    GameHeader.MaxCarry = max_carry;
    GameHeader.PlayerRoom = player_room;
    MyLoc = player_room;
    GameHeader.Treasures = num_treasures;
    GameHeader.NumPreps = num_preps;
    GameHeader.NumAdverbs = num_adverbs;
    GameHeader.NumSubStr = num_substr;
    GameHeader.NumObjImg = num_obj_img;
    GameHeader.NumMessages = num_messages;
    GameHeader.TreasureRoom = treasure_room;

    PrintHeaderInfo(GameHeader);

    if (!SanityCheckHeader())
        return 0;

    Items = MemAlloc(sizeof(Item) * (num_items + 1));
    Rooms = MemAlloc(sizeof(Room) * (num_rooms + 1));
    ObjectImages = MemAlloc(sizeof(ObjectImage) * (num_obj_img + 1));

    Counters[35] = num_rooms;
    Counters[34] = treasure_room;
    Counters[42] = max_carry;
    SetBit(35); // Graphics on

#pragma mark actions

    /* The binary format does not store an explicit action count. Instead,
       actions are read sequentially until ActionSum bytes have been consumed.
       The preliminary array grows dynamically if needed. */
    ct = 0;
    int act_capacity = 524;
    Action *PrelActions = MemAlloc(sizeof(Action) * act_capacity);

    ap = PrelActions;

    uint8_t *origptr = ptr;

    while (ptr - origptr <= GameHeader.ActionSum) {
        if (ptr + 3 > end)
            Fatal("Broken database!");
        /* First byte: high nibble = extra word count, low nibble = command length */
        uint8_t length = *ptr++;
        ap->NumWords = length >> 4;
        ap->CommandLength = length & 0xf;
        ap->Verb = *ptr++;
        ap->NounOrChance = *ptr++;

        if (ptr + ap->NumWords > end)
            Fatal("Broken database!");
        ap->Words = MemAlloc(ap->NumWords);

        for (int i = 0; i < ap->NumWords; i++) {
            ap->Words[i] = *ptr++;
        }

        /* Read condition chain: each 16-bit big-endian value encodes a 5-bit
           condition opcode (low bits) and a 10-bit argument (high bits).
           Bit 15 marks the last entry in the chain. Stored as alternating
           condition/argument pairs terminated by a 255 sentinel. */
        uint16_t conditions[1024];
        int condargs = 0;
        for (;;) {
            if (ptr + 2 > end)
                Fatal("Broken database!");
            uint16_t raw = READ_BE_UINT16_AND_ADVANCE(&ptr);
            int last = raw & 0x8000;
            if (condargs + 2 > 1024)
                Fatal("Broken database!");
            conditions[condargs++] = raw & 0x1f;
            conditions[condargs++] = (raw >> 5) & 0x3ff;
            if (last)
                break;
        }
        ap->Conditions = MemAlloc((condargs + 1) * sizeof(uint16_t));
        memcpy(ap->Conditions, conditions, condargs * sizeof(uint16_t));
        ap->Conditions[condargs] = 255;

        int cmdlen = ap->CommandLength + 1;
        if (ptr + cmdlen > end)
            Fatal("Broken database!");
        ap->Commands = MemAlloc(cmdlen);
        memcpy(ap->Commands, ptr, cmdlen);
        ptr += cmdlen;

        ap++;
        ct++;

        if (ct >= act_capacity) {
            act_capacity *= 2;
            PrelActions = MemRealloc(PrelActions, sizeof(Action) * act_capacity);
            ap = PrelActions + ct;
        }
    }

    GameHeader.NumActions = ct - 1;

    /* Copy the preliminary action array into a right-sized final allocation */
    Actions = MemAlloc(sizeof(Action) * ct);
    memcpy(Actions, PrelActions, sizeof(Action) * ct);
    free(PrelActions);

#pragma mark dictionary

    Verbs = ReadDictWords(&ptr, GameHeader.NumVerbs + 1, 1);
    Nouns = ReadDictWords(&ptr, GameHeader.NumNouns + 1, 1);

#pragma mark room connections

    /* The room connections are stored column-major: all rooms' North exits
       come first, then South, East, West, Up, Down, then two extra columns
       (Exits[6] = message index for room description, Exits[7] = room image).
       Directions 6 and 7 are 16-bit on most platforms (read as two bytes).
       On Apple II, direction 7 encodes a memory address for the image. */
    for (int j = 0; j < 8; j++) {
        for (ct = 0; ct <= num_rooms; ct++) {
            Rooms[ct].Exits[j] = *ptr++;

            if (CurrentSys == SYS_APPLE2 && j == 7) {
                /* Apple II stores image addresses as two bytes that combine
                   into a memory address: high * 0x1000 + low * 0x100 */
                int adr = Rooms[ct].Exits[j] * 0x100;
                adr += *ptr++ * 0x10;
                adr *= 0x10;
                debug_print("Room image %d address:%x\n", ct, adr);
                Rooms[ct].Exits[j] = adr;
            } else if (j > 5) {
                /* Directions 6 and 7 use 16-bit values (second byte here) */
                Rooms[ct].Exits[j] |= *ptr++;
            }

            if (j == 7)
                Rooms[ct].Image = Rooms[ct].Exits[j];

            debug_print("Room %d exit %d: %d\n", ct, j, Rooms[ct].Exits[j]);
        }
        /* The database seems to use big-endian 16-bit values
          in the Atari ST versions of Questprobe featuring Spider-Man
          and Questprobe featuring Human Torch and the Thing,
          but because none of the actual values is above 255
          (i.e. all values fit in 8 bits, so every other byte is zero)
          we can simply advance the pointer a single byte and then read
          all the values as if they were little-endian. */

       /* Unfortunatey we also have to nudge the pointer in other ways
          here and there, which can't be explained by endianness.
          It seems that the ST games alternate between 8-bit and 16-bit
          values in an inconsistent way. */

        if (isSTSpiderman || isSTFantastic4)
            ptr++;
    }

    if (isSTSpiderman || isSTFantastic4) {
        ptr -= 2;
    }

#pragma mark messages

    Messages = LoadMessages(GameHeader.NumMessages + 1, &ptr);

    /* Resolve room descriptions: Exits[6] holds a message index offset by 76 */
    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        if (Rooms[i].Exits[6] == 0)
            Rooms[i].Text = "";
        else
            Rooms[i].Text = Messages[Rooms[i].Exits[6] - 76];
        debug_print("Room description of room %d: \"%s\"\n", i, Rooms[i].Text);
    }

#pragma mark items

    /* Item data is split across multiple columnar passes: first all item
       names, then all locations, then all dictionary word indices, then
       all flags — similar to the room exit layout. */
    size_t length;

    if (CurrentSys == SYS_ST && !isSTFantastic4)
        ptr++;

    for (ct = 0; ct <= num_items; ct++)
        ptr = ReadPlusString(ptr, &Items[ct].Text, &length);

#pragma mark item locations
    if (isSTSpiderman || isSTFantastic4) {
        ptr++;
    }

    for (ct = 0; ct <= num_items; ct++) {
        Items[ct].Location = *ptr++;
        Items[ct].InitialLoc = Items[ct].Location;
    }

    if (CurrentSys == SYS_ST)
        ptr += 2;

    for (ct = 0; ct <= num_items; ct++)
        Items[ct].Dictword = READ_LE_UINT16_AND_ADVANCE(&ptr);

    for (ct = 0; ct <= num_items; ct++) {
        Items[ct].Flag = READ_LE_UINT16_AND_ADVANCE(&ptr);
        debug_print("Item %d: \"%s\", %d, %d, %d\n", ct, Items[ct].Text, Items[ct].Location, Items[ct].Dictword, Items[ct].Flag);
    }

    if (CurrentSys == SYS_ST)
        ptr -= 1;

    Adverbs = ReadDictWords(&ptr, GameHeader.NumAdverbs + 1, 1);
    Prepositions = ReadDictWords(&ptr, GameHeader.NumPreps + 1, 1);

#pragma mark substitutions

    Substitutions = ReadSubstitutionsBinary(&ptr, GameHeader.NumSubStr + 1, 1);

    /* In the binary format, the title string comes after the substitutions
       (unlike plaintext where it's at the very beginning) */
    char *title = NULL;
    ptr = ReadPlusString(ptr, &title, &length);
    debug_print("Title: %s\n", title);

    if (!SetGame(title, length)) {
        free(title);
        return 0;
    }

    /* Object image mappings are stored in three separate columnar passes:
       room assignments, object assignments, and image indices. On Apple II,
       image indices are two-byte memory addresses like room images. */
    if (isSTFantastic4)
        ptr++;

    for (ct = 0; ct <= num_obj_img; ct++)
        ObjectImages[ct].Room = *ptr++;

    if (CurrentSys == SYS_ST && !isSTSpiderman)
        ptr++;

    for (ct = 0; ct <= num_obj_img; ct++)
        ObjectImages[ct].Object = *ptr++;

    if (CurrentSys == SYS_ST) {
        ptr++;
        if (!isSTSpiderman)
            ptr++;
    }

    for (ct = 0; ct <= num_obj_img; ct++) {
        ObjectImages[ct].Image = *ptr++;
        if (CurrentSys == SYS_APPLE2) {
            int adr = ObjectImages[ct].Image * 0x100;
            adr += *ptr++ * 0x10;
            adr *= 0x10;
            debug_print("Object image %d address:%x\n", ct, adr);
            ObjectImages[ct].Image = adr;
        } else
            ptr++;
    }

    for (ct = 0; ct <= num_obj_img; ct++)
        debug_print("ObjectImages %d: room:%d object:%d image:%x\n", ct, ObjectImages[ct].Room, ObjectImages[ct].Object, ObjectImages[ct].Image);

    DumpActions();

    return 1;
}
