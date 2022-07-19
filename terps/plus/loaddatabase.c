//
//  loaddatabase.c
//  Plus
//
//  Created by Administrator on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "loaddatabase.h"

Synonym *Substitutions;
uint8_t *MysteryValues;


static const char *ConditionList[]={
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

struct Keyword
{
    char     *name;
    int      opcode;
    int      count;
};

static const struct Keyword CommandList[] =
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
//  {"SWAPLOC?",     87, 0 },
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
    {"CPY2CNT",     126, 2 },
    {"CNT2CPY",     127, 2 },
    {"PRINTMESS",   128, 1 },
    {"",             -1, 0 }
};


static const char *ReadWholeLine(FILE *f)
{
    char tmp[1024];
    char *t;
    int c;
    int ct = 0;
    do {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
        if (c == '\"') {
            c = fgetc(f);
            if (c == '\"')
                return "\0";
            else
                break;
        }
    } while (c != EOF && (isspace((unsigned char)c) || c == '\"')); // Strip first hyphen
    do {
        if (c == '\n' || c == 10 || c == 13 || c == EOF)
            break;
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        else if ((c >= 32 && c <= 126))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
        c = fgetc(f);
    } while (1);
    
    for (int i = ct - 1; i > 0; i--) // Look for last hyphen
        if (tmp[i] == '\"') {
            ct = i + 1;
            break;
        }
    if (ct > 1)
        ct--;
    else
        return "\0";
    tmp[ct] = 0;
    t = MemAlloc(ct + 1);
    memcpy(t, tmp, ct + 1);
    return (t);
}


static char *ReadString(FILE *f, size_t *length)
{
    char tmp[1024];
    char *t;
    int c, nc;
    int ct = 0;
lookdeeper:
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"') {
        goto lookdeeper;
    }
    do {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"'; /* pdd */
        
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
        else if ((c >= 32 && c <= 126))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    } while (1);
    tmp[ct] = 0;
    *length = ct + 1;
    t = MemAlloc((int)*length);
    memcpy(t, tmp, *length);
    return (t);
}

static DictWord *ReadDictWordsPC(FILE *f, int numstrings, int loud) {
    DictWord dictionary[1024];
    DictWord *dw = &dictionary[0];
    char *str = NULL;
    int group = 0;
    int index = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t length;
        str = ReadString(f, &length);
        if (str == NULL || str[0] == '\0')
            continue;
        int lastcomma = 0;
        int commapos = 0;
        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while(str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                int length = commapos - lastcomma;
                if (length > 0) {
                    dw->Word = MemAlloc(length);
                    memcpy(dw->Word, &str[lastcomma + 1], length);
                    dw->Group = group;
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
    if (loud)
        for (int j = 0; dictionary[j].Word != NULL; j++)
            debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);
    return finaldict;
}

static Synonym *ReadSubstitutions(FILE *f, int numstrings, int loud) {
    Synonym syn[1024];
    Synonym *s = &syn[0];
    
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
            if (str[j] == ',') {
                while(str[j] == ',') {
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
                int length = commapos - lastcomma - foundrep;
                if (length > 0) {
                    if (foundrep) {
                        if (replace) {
                            free(replace);
                        }
                        replace = MemAlloc(length);
                        memcpy(replace, &str[lastcomma + 2], length);
                        if (loud)
                            debug_print("Found new replacement string \"%s\"\n", replace);
                    } else {
                        s->SynonymString = MemAlloc(length);
                        memcpy(s->SynonymString, &str[lastcomma + 1], length);
                        if (loud)
                            debug_print("Found new synonym string \"%s\"\n", s->SynonymString);
                        s = &syn[++index];
                    }
                }
                if (foundrep) {
                    for (int k = firstsyn; k < index; k++) {
                        int len = (int)strlen(replace) + 1;
                        syn[k].ReplacementString = MemAlloc(len);
                        memcpy(syn[k].ReplacementString, replace, len);
                        if (loud)
                            debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", syn[k].SynonymString, k, syn[k].ReplacementString);
                    }
                    firstsyn = index;
                    foundrep = 0;
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
    }
    if (replace)
        free(replace);
    syn[index].SynonymString = NULL;
    int synsize = (index + 1) * sizeof(Synonym);
    Synonym *finalsyns = (Synonym *)MemAlloc(synsize);
    
    memcpy(finalsyns, syn, synsize);
    if (loud)
        for (int j = 0; syn[j].SynonymString != NULL; j++)
            debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);
    return finalsyns;
}

char **Comments;

static void ReadComments(FILE *f, int loud) {
    const char *strings[1024];
    const char *comment;
    int index = 0;
    do {
        comment = ReadWholeLine(f);
        strings[index++] = comment;
    } while (comment != NULL);
    if (index <= 1)
        return;
    else
        index--;
    Comments = MemAlloc((index + 1) * sizeof(char *));
    for (int i = 0; i < index; i++) {
        int len = (int)strlen(strings[i]);
        Comments[i] = MemAlloc(len + 1);
        memcpy(Comments[i], strings[i], len + 1);
        if (loud)
            debug_print("Comment %d:\"%s\"\n", i, Comments[i]);
    }
    Comments[index] = NULL;
}

static int NumberOfArguments(int opcode) {
    int i = 0;
    while (CommandList[i].opcode != -1) {
        if (CommandList[i].opcode == opcode)
            return CommandList[i].count;
        i++;
    }
    return 0;
}

void PrintDictWord(int idx, DictWord *dict) {
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == idx) {
            debug_print("%s", dict->Word);
            return;
        }
        dict++;
    }
    debug_print("%d", idx);
}

static void PrintCondition(uint8_t condition, uint8_t argument, int *negate, int *mult, int *or) {
    if (condition == 0) {
        debug_print(" %d", argument);
    } else if (condition == 31) {
        if (argument == 1) {
            debug_print(" OR(31) %d ", argument); // Arg 1 == OR
            *or = 1;
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
                *or = 0;
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


static void PrintDebugMess(int index)
{
    if (index > GameHeader.NumMessages)
        return;
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        debug_print("\"%s\" ", message);
    }
}

static void PrintCommand(uint8_t opcode, uint8_t arg, uint8_t arg2, uint8_t arg3, int numargs) {
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

static void DumpActions(void) {
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
            debug_print("(%d)(%d)",ap->Verb, ap->NounOrChance);
        }

        if (Comments[ct][0] != 0)
            debug_print("\nComment: \"%s\"\n", Comments[ct]);
        else
            debug_print("\n");
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
                PrintCondition(ap->Conditions[i], ap->Conditions[i+1], &negate_condition, &negate_multiple, &or_condition);
            }
        }
        negate_condition = 0;
        negate_multiple = 0;
        debug_print("\n");
        
        for (int i = 0; i <= ap->CommandLength; i++) {
            uint8_t command = ap->Commands[i];
            int numargs = NumberOfArguments(command);
            
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

static uint8_t ReadNum(FILE *f) {
    int num;
    if (fscanf(f, "%d\n", &num) != 1)
        return 0;
    return num;
}

static void ReadAction(FILE *f, Action *ap) {
    uint8_t length = ReadNum(f);
    int i;
    ap->NumWords = length >> 4;
    ap->CommandLength = length & 0xf;
    ap->Verb = ReadNum(f);
    if (ap->Verb & 128) {
        Fatal("2-byte verbs unimplemented\n");
    }
    ap->NounOrChance = ReadNum(f);
    if (ap->NounOrChance & 128) {
        Fatal("2-byte nouns unimplemented\n");
    }
    ap->Words = MemAlloc(ap->NumWords);
    
    for (i = 0; i < ap->NumWords; i++) {
        ap->Words[i] = ReadNum(f);
    }
    
    int reading_conditions = 1;
    int conditions_read = 0;
    uint8_t conditions[1024];
    int condargs = 0;
    while (reading_conditions) {
        uint16_t argcond = ReadNum(f) * 256 + ReadNum(f);
        if (argcond & 0x8000)
            reading_conditions = 0;
        argcond = argcond & 0x1fff;
        uint8_t argument = argcond >> 5;
        uint8_t condition = argcond & 0x1f;
        conditions[condargs++] = condition;
        conditions[condargs++] = argument;
        if (condition != 0)
            conditions_read++;
    }
    ap->Conditions = MemAlloc(condargs + 1);
    for (i = 0; i < condargs; i++)
        ap->Conditions[i] = conditions[i];
    ap->Conditions[condargs] = 255;
    uint8_t commands[1024];
    for (i = 0; i <= ap->CommandLength; i++) {
        commands[i] = ReadNum(f);
    }
    
    ap->Commands = MemAlloc(i);
    memcpy(ap->Commands, commands, i);
}

int LoadDatabase(FILE *f, int loud)
{
    int ni, as, na, nv, nn, nr, mc, pr, tr, lt, mn, trm, adv, prp, ss, unk1, oi, unk3;
    int ct;
    
    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    size_t length;
    char *title = ReadString(f, &length);

    if (strncmp(title, "BUCKAROO", length) == 0)
        CurrentGame = BANZAI;
    else if (strncmp(title, "SPIDER-MAN (tm)", length) == 0)
        CurrentGame = SPIDERMAN;
    else if (strncmp(title, "Sorcerer of Claymorgue Castle. SAGA#13.", length) == 0)
        CurrentGame = CLAYMORGUE;
    else if (strncmp(title, "FF #1 ", length) == 0)
        CurrentGame = FANTASTIC4;
    
    if (fscanf(f, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", &ni, &as, &nn, &nv, &nr, &mc, &pr,
               &mn, &trm, &lt, &prp, &adv, &na, &tr, &ss, &unk1, &oi, &unk3)
        < 10) {
        if (loud)
            debug_print("Invalid database(bad header)\n");
        return 0;
    }
    GameHeader.NumItems = ni;
    Counters[43] = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumVerbs = nv;
    GameHeader.NumNouns = nn;
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    MyLoc = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    //    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;
    GameHeader.NumAdverbs = adv;
    GameHeader.NumPreps = prp;
    GameHeader.NumSubStr = ss;
    GameHeader.NumObjImg = oi;
    ObjectImages = (ObjectImage *)MemAlloc(sizeof(ObjectImage) * (oi + 1));
    MysteryValues = MemAlloc(unk3 + 1);

    Counters[35] = nr;
    Counters[34] = trm;
    Counters[42] = mc;
    SetBit(35); // Graphics on

    if (loud) {
        debug_print("Number of items: %d\n", GameHeader.NumItems);
        debug_print("Number of actions: %d\n", GameHeader.NumActions);
        debug_print("Number of verbs: %d\n", GameHeader.NumVerbs);
        debug_print("Number of nouns: %d\n", GameHeader.NumNouns);
        debug_print("Number of adverbs: %d\n", GameHeader.NumAdverbs);
        debug_print("Number of prepositions: %d\n", GameHeader.NumPreps);
        debug_print("Number of substitution strings: %d\n", GameHeader.NumSubStr);
        debug_print("Number of rooms: %d\n", GameHeader.NumRooms);
        debug_print("Number of messages: %d\n", GameHeader.NumMessages);
        debug_print("Max carried: %d\n", GameHeader.MaxCarry);
        debug_print("Starting location: %d\n", GameHeader.PlayerRoom);
        debug_print("Light time: %d\n", lt);
        debug_print("Number of treasures: %d\n", GameHeader.Treasures);
        debug_print("Treasure room: %d\n", GameHeader.TreasureRoom);
        debug_print("Unknown value 1: %d\n", unk1);
        debug_print("Unknown value 2: %d\n", oi);
        debug_print("Unknown value 3: %d\n", unk3);
    }
    
    /* Load the actions */
    
    ct = 0;
    ap = Actions;
    if (loud)
        debug_print("Reading %d actions.\n", na);
    while (ct < na + 1) {
//        debug_print("Reading action %d.\n", ct);
        ReadAction(f, ap);
        ap++;
        ct++;
    }
    
    if (loud)
        debug_print("Reading %d verbs.\n", nv);
    Verbs = ReadDictWordsPC(f, GameHeader.NumVerbs + 1, loud);
    if (loud)
        debug_print("Reading %d nouns.\n", nn);
    Nouns = ReadDictWordsPC(f, GameHeader.NumNouns + 1, loud);
    
    rp = Rooms;
    if (loud)
        debug_print("Reading %d rooms.\n", nr);
    for (ct = 0; ct < nr + 1; ct++) {
        if (fscanf(f, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd\n", &rp->Exits[0], &rp->Exits[1],
                   &rp->Exits[2], &rp->Exits[3], &rp->Exits[4],
                   &rp->Exits[5], &rp->Exits[6], &rp->Image)
            != 8) {
            debug_print("Bad room line (%d)\n", ct);
            //            FreeDatabase();
            return 0;
        }
        rp++;
    }
    
    
    if (loud)
        debug_print("Reading %d messages.\n", mn);
    for (ct = 0; ct < mn + 1; ct++) {
        Messages[ct] = ReadString(f, &length);
        if (loud)
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
    
    for (ct = 0; ct < nr + 1; ct++) {
        Rooms[ct].Text = Messages[Rooms[ct].Exits[6] - 76];
        if (loud)
            debug_print("Room description of room %d: \"%s\"\n", ct, Rooms[ct].Text);
    }
    
    ct = 0;
    if (loud)
        debug_print("Reading %d items.\n", ni);
    int loc, dictword, flag;
    ip = Items;
    while (ct < ni + 1) {
        ip->Text = ReadString(f, &length);
        if (loud)
            debug_print("Item %d: \"%s\"\n", ct, ip->Text);
        if (fscanf(f, ",%d,%d,%d\n", &loc, &dictword, &flag) != 3) {
            debug_print("Bad item line (%d)\n", ct);
            //            FreeDatabase();
            return 0;
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
    
    if (loud)
        debug_print("Reading %d adverbs.\n", GameHeader.NumAdverbs);
    Adverbs = ReadDictWordsPC(f, GameHeader.NumAdverbs + 1, loud);
    if (loud)
        debug_print("Reading %d prepositions.\n",  GameHeader.NumPreps);
    Prepositions = ReadDictWordsPC(f, GameHeader.NumPreps + 1, loud);
    
    Substitutions = ReadSubstitutions(f, GameHeader.NumSubStr + 1, loud);
    
    ObjectImage *objimg = ObjectImages;
    if (loud)
        debug_print("Reading %d object image values.\n", oi + 1);
    for (ct = 0; ct < oi + 1; ct++) {
        if (fscanf(f, "%d,%d,%d\n", &objimg->room, &objimg->object, &objimg->image)
            != 3) {
            debug_print("Bad object image line (%d)\n", ct);
            //            FreeDatabase();
            return 0;
        }
        objimg++;
    }
    
    if (loud)
        debug_print("Reading %d unknown values.\n", unk3 + 1);
    for (ct = 0; ct < unk3 + 1; ct++) {
        MysteryValues[ct] = ReadNum(f);
        if (MysteryValues[ct] > 255) {
            debug_print("Bad unknown value (%d)\n", ct);
            //            FreeDatabase();
            return 0;
        }
    }
    
    ReadComments(f, loud);
    
    if (loud) {
        debug_print("\nLoad Complete.\n\n");
        DumpActions();
    }
    fclose(f);
    
    return 1;
}
