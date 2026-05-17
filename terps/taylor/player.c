//
//  player.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "c64decrunch.h"
#include "decompressz80.h"
#include "decrypttotloader.h"
#include "extracommands.h"
#include "extracttape.h"
#include "glkstart.h"
#include "parseinput.h"
#include "restorestate.h"
#include "graphics.h"
#include "irmak.h"
#include "utility.h"
#include "common_file_utils.h"
#include "randomness.h"

#include "taylor.h"

uint8_t Flag[128];      /* Game state flags (conditions, counters, etc.) */
uint8_t ObjectLoc[256]; /* Location of each object (room number or special values) */

uint8_t *FileImage = NULL;  /* Raw game file loaded into memory */
uint8_t *EndOfData = NULL;  /* One past the last byte of FileImage */
size_t FileImageLen = 0;
size_t VerbBase;            /* Offset of the verb/noun dictionary in FileImage */
static size_t TokenBase;    /* Offset of the compressed token (text fragment) table */
static size_t MessageBase;  /* Offset of the primary message text table */
static size_t Message2Base; /* Offset of the secondary message text table */
static size_t RoomBase;     /* Offset of room description text */
static size_t ObjectBase;   /* Offset of object description text */
static size_t ExitBase;     /* Offset of room exit/connection table */
static size_t ObjLocBase;   /* Offset of initial object location table */
static size_t StatusBase;   /* Offset of automatic (status) action table */
static size_t ActionBase;   /* Offset of player command action table */
static size_t FlagBase;     /* Offset of initial flag values */
size_t AnimationData = 0;   /* Offset of Kayleth animation data (0 if none) */

/* Objects below this index are "low objects" — printed as part of the
   room description rather than after the "You can see:" prompt. */
static int NumLowObjects;

static int ActionsExecuted; /* Whether any action matched this turn */
int PrintedOK;             /* Whether "OK" has been printed this turn */
int Redraw = 0;            /* Room description needs refreshing */

int FirstAfterInput = 0;   /* First output character after player input */

int StopTime = 0;          /* Suppress status table runs for this many turns */
int JustStarted = 1;       /* True until the first player command */
int ShouldRestart = 0;     /* Set to trigger a full game restart */

int NoGraphics = 0;        /* Disable graphics display */

/* Offset added to all table addresses to account for the file's load
   position differing from the original Z80 memory layout. */
long FileBaselineOffset = 0;

char DelimiterChar = '_';  /* Word separator in dictionary entries */

/* Token table conventions: each byte's low 7 bits carry the character,
   and the high bit marks the final byte of a token's expansion. */
#define TOKEN_LAST_BYTE 0x80
#define TOKEN_BYTE_MASK 0x7F

/* First byte value treated as a token in QP3 text streams; raw bytes
   below this are emitted as literal characters. */
#define QP3_TOKEN_BASE 0x7b

/* QP3 text-table framing: each entry ends with QP3_ENTRY_DELIM, and the
   table itself ends with QP3_TABLE_END. */
#define QP3_ENTRY_DELIM 0x1f
#define QP3_TABLE_END   0x18

/* QP3 object-location codes: an object can be in a room (room number) or
   in one of these special locations. */
#define LOC_DESTROYED   0xfc /* Object is removed from play */
#define LOC_INV_TORCH   253  /* In Human Torch's inventory */
#define LOC_INV_THING   254  /* In Thing's inventory */

/* Message text terminators in non-QP3 versions: MSG_END ends an entry,
 MSG_END_SPACE ends it and indicates a trailing space is pending. */
#define MSG_END         0x7e
#define MSG_END_SPACE   0x5e

/* Action-table opcode byte layout: high bit marks an action byte (vs. a
   condition byte); bit 6 on an action byte stops further table scanning;
   the low 6 bits hold the opcode number. */
#define ACTION_BIT      0x80
#define ACTION_DONE_BIT 0x40
#define ACTION_OP_MASK  0x3F

/* Terminator byte that marks the end of a status or command action table. */
#define ACTION_TABLE_END 0x7F

/* Word code used as a wildcard in command-table verb/noun entries — matches
   any input word. */
#define WORD_WILDCARD    126

static char *Filename;
static uint8_t *CompanionFile; /* Second file for Temple of Terror hybrid mode */

char LastChar = 0;       /* Buffered output character (for punctuation lookahead) */
static int Upper = 0;    /* Capitalize the next alphabetic character */
int PendSpace = 0;       /* A space is pending before the next character */

static int LastNoun = 0; /* Last noun for "IT" pronoun substitution */
int LastVerb = 0;        /* Last verb for implicit verb carry-forward */
static int GoVerb = 0;   /* Dictionary code for "GO" */

static int FoundVerb = 0; /* Current input matched a verb in the action table */
static int FoundNoun = 0; /* Current input matched a noun in the action table */

GameInfo *Game = NULL;
extern GameInfo games[];

strid_t room_description_stream = NULL;

extern int AnimationRunning;

static int DeferredGoto = 0;
int InKaylethPreview = 0;

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

#endif

/* Questprobe 3 uses different opcode numbering for conditions and actions.
   These tables map QP3 opcodes to the standard enum values. */
static const char Q3Condition[] = {
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

static const char Q3Action[] = {
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

/* Search FileImage for a byte pattern starting at base. Returns the
   offset if found, or (size_t)-1 if not found. */
size_t FindCode(const char *code, size_t base, size_t len)
{
    unsigned char *p = FileImage + base;
    while (p < FileImage + FileImageLen - len) {
        if (memcmp(p, code, len) == 0)
            return p - FileImage;
        p++;
    }
    return (size_t)-1;
}

/* Locate the initial flag data in the file image by searching for known
   byte patterns. Falls back to the Game struct's recorded offset. */
static size_t FindFlags(void)
{
    size_t pos;

    /* Questprobe */
    pos = FindCode("\xE7\x97\x51\x95\x5B\x7E\x5D\x7E\x76\x93", 0, 10);
    if (pos == -1) {
        /* Look for the flag initial block copy */
        pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
        if (pos == -1) {
            if (Game)
                return Game->start_of_flags + FileBaselineOffset;
            else {
                fprintf(stderr, "Cannot find initial flag data.\n");
                glk_exit();
            }
        } else
            return pos + 5;
    }
    return pos + 11;
}

/* Heuristic: check if the data at pos looks like a token table by
   counting lowercase ASCII characters in a fixed-size sample window. */
static int LooksLikeTokens(size_t pos)
{
    enum {
        TOKEN_SAMPLE_SIZE      = 512, /* bytes to inspect */
        TOKEN_SAMPLE_THRESHOLD = 300, /* >this many lowercase letters = likely tokens */
    };

    if (pos > FileImageLen || FileImageLen - pos < TOKEN_SAMPLE_SIZE)
        return 0;

    unsigned char *sample = FileImage + pos;
    int lowercase_count = 0;
    for (int i = 0; i < TOKEN_SAMPLE_SIZE; i++) {
        unsigned char c = sample[i] & TOKEN_BYTE_MASK;
        if (c >= 'a' && c <= 'z')
            lowercase_count++;
    }
    return lowercase_count > TOKEN_SAMPLE_THRESHOLD;
}

/* Detect version 0 format by checking for in-stream end markers
   (MSG_END_SPACE or MSG_END) in the token table. */
static void TokenClassify(size_t pos)
{
    unsigned char *ptr = FileImage + pos;
    int n = 0;
    while (n++ < 256) {
        do {
            if (*ptr == MSG_END_SPACE || *ptr == MSG_END)
                Version = 0;
        } while (!(*ptr++ & TOKEN_LAST_BYTE));
    }
}

/* Locate the token (text fragment) table by searching for known byte
   patterns. Tries game-specific signatures first, then falls back to
   generic Z80 instruction patterns and the "You are in " string. */
static size_t FindTokens(void)
{
    size_t addr;
    size_t pos = 0;

    if (Game)
        switch (CurrentGame) {
        case TOT_TEXT_ONLY_64:
        case TOT_HYBRID_64:
            if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                return pos;
            break;
        case TEMPLE_OF_TERROR_64:
            if ((pos = FindCode("\x80\x20\x54\x68\x65\x72\x65\x20\x69\x73", 0, 10)) != -1)
                return pos;
            break;
        case REBEL_PLANET_64:
            if ((pos = FindCode("\xa7\x2e\xfe\x20\xfe\x2c\xfe\x21\xfe\x3f", 0, 10)) != -1)
                return pos;
            break;
        case QUESTPROBE3_64:
            if ((pos = FindCode("\x61\xa0\x64\xa0\x65\xa0\x67\xa0\x69\xa0", 0, 10)) != -1)
                return pos;
            break;
        case HEMAN_64:
        case KAYLETH_64:
            if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                return pos;
            break;
        default:
            break;
        }

    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if (pos == -1) {
            /* Questprobe */
            pos = FindCode("\x58\x58\x58\x58\xFF", 0, 5);
            if (pos == -1) {
                /* Last resort */
                addr = FindCode("You are in ", 0, 11) - 1;
                if (addr == -1) {
                    if (Game)
                        return Game->start_of_tokens + FileBaselineOffset;
                    fprintf(stderr, "Unable to find token table.\n");
                    return 0;
                }
                return addr;
            } else
                return pos + 6;
        }
        addr = READ_LE_UINT16(FileImage + pos - 2) - ZX_RAM_BASE + FileBaselineOffset;
    } while (LooksLikeTokens(addr) == 0);
    TokenClassify(addr);
    return addr;
}

/* Write formatted text to the room description stream (used by the
   graphics system to capture room text for layout purposes). */
void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[2048];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* Emit a single character, applying auto-capitalization if set. */
static void OutWrite(char c)
{
    if (isalpha(c) && Upper) {
        c = toupper(c);
        Upper = 0;
    }
    PrintCharacter(c);
}

/* Flush the buffered output character and any pending space. */
void OutFlush(void)
{
    if (LastChar)
        OutWrite(LastChar);
    if (PendSpace && LastChar != '\n' && !FirstAfterInput)
        OutWrite(' ');
    LastChar = 0;
    PendSpace = 0;
}

static void OutReset(void)
{
    OutFlush();
}

static int Q3Upper = 0;

/* Flush any buffered character and set capitalize-next flag. */
void OutCaps(void)
{
    if (LastChar) {
        OutWrite(LastChar);
        LastChar = 0;
    }
    Upper = 1;
    Q3Upper = 1;
}

static int periods = 0;
int JustWrotePeriod = 0;

/* Buffer a character for output with smart punctuation handling:
   - ']' is treated as newline
   - Periods are accumulated and deferred (for ellipsis detection)
   - Spaces are deferred to allow punctuation to suppress them
   - Auto-capitalization after sentence-ending punctuation */
void OutChar(char c)
{
    /* Taylor's games use ']' as an in-game newline marker. */
    if (c == ']')
        c = '\n';

    if (c == '.') {
        /* Accumulate periods so we can collapse runs into ellipses below.
           But if this period follows punctuation/whitespace, demote it
           to a space — it's not part of a sentence-ending mark. */
        periods++;
        if (LastChar == '?' || FirstAfterInput || isspace(LastChar) || LastChar == '.') {
            c = ' ';
            if (LastChar == ' ')
                LastChar = 0;
        }
        PendSpace = 0;
    } else {
        /* Non-period: flush any accumulated periods now (unless the next
           char is itself terminal punctuation, in which case the periods
           are part of an abbreviation and stay associated with it). */
        if (periods && !JustWrotePeriod && c != ',' && c != '?' && c != '!') {
            /* Normalize a double-period to a single period; three or more
               periods (an ellipsis) print verbatim. */
            if (periods == 2)
                periods = 1;
            for (int i = 0; i < periods; i++) {
                JustWrotePeriod = 0;
                PrintCharacter('.');
            }
            JustWrotePeriod = 1;
            LastChar = 0;
            PendSpace = 0;
            Upper = 0;
        }
        periods = 0;
    }

    /* Spaces are deferred via PendSpace so the next non-space character
       can decide whether to actually emit one (or suppress it, e.g.
       before punctuation). */
    if (c == ' ') {
        PendSpace = 1;
        return;
    }
    /* First character after a player command: eat leading whitespace and
       capitalize the first real character. */
    if (FirstAfterInput) {
        if (isspace(LastChar)) {
            LastChar = 0;
            Upper = 1;
        } else if (!isspace(c)) {
            FirstAfterInput = 0;
        }
        PendSpace = 0;
    }
    /* Flush the one-character lookahead buffer. Suppress it if it was a
       period we just wrote ourselves above (ellipsis flush). */
    if (LastChar) {
        if (isspace(LastChar))
            PendSpace = 0;
        if (LastChar == '.' && JustWrotePeriod) {
            LastChar = 0;
        } else {
            OutWrite(LastChar);
        }
    }
    /* Emit the deferred space, capitalizing afterwards if it sits between
       a period and the next sentence (skipped for Kayleth, which has its
       own casing rules). */
    if (PendSpace) {
        if (JustWrotePeriod && BaseGame != KAYLETH)
            Upper = 1;
        OutWrite(' ');
        PendSpace = 0;
    }
    /* Buffer this character; the next call will flush it. Newlines reset
       capitalization (both base and QP3-specific). */
    LastChar = c;
    if (LastChar == '\n' || LastChar == '\r') {
        Upper = 1;
        Q3Upper = 1;
    }
}

static void OutReplace(char c)
{
    LastChar = c;
}

static void OutKillSpace(void)
{
    PendSpace = 0;
}

void OutString(char *p)
{
    while (*p)
        OutChar(*p++);
}

/* Return a pointer to the start of token n in the token table.
   Tokens are variable-length with the high bit marking the last byte. */
static unsigned char *TokenText(unsigned char n)
{
    unsigned char *p = FileImage + TokenBase;
    if (Version == QUESTPROBE3_TYPE)
        n -= QP3_TOKEN_BASE;

    while (n > 0 && p < EndOfData) {
        while ((*p & TOKEN_LAST_BYTE) == 0)
            p++;
        n--;
        p++;
    }
    return p;
}

/* Print a character using Questprobe 3's capitalization rules:
   auto-capitalize after sentence-ending punctuation. */
void Q3PrintChar(uint8_t c)
{
    /* Drop control characters except newline — QP3 token data sometimes
       contains stray low bytes we don't want to emit. */
    if (c < ' ' && c != '\n')
        return;

    /* Treat the first character after a player command as sentence-start. */
    if (FirstAfterInput)
        Q3Upper = 1;

    /* If a capital is pending and this is a lowercase letter, uppercase it. */
    if (Q3Upper && c >= 'a') {
        c -= 'a' - 'A';
    }
    OutChar(c);
    /* Any "real" printable character (past space and '!') consumes the
       pending-capital flag; space and '!' deliberately preserve it so a
       sequence like "Hello! World" still capitalizes the W. */
    if (c > '!') {
        Q3Upper = 0;
        Upper = 0;
    }
    /* Sentence-ending punctuation arms the flag for the next letter. */
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        Q3Upper = 1;
    }
}

/* Expand and print token n from the token table. Each byte's low 7 bits
   are the character; the high bit marks the end of the token. */
static void PrintToken(unsigned char n)
{
    /* Blizzard Pass: token 0x2d in room 49 should render as a single hyphen
       instead of its usual expansion. */
    if (CurrentGame == BLIZZARD_PASS && MyLoc == 49 && n == 0x2d) {
        OutChar('-');
        return;
    }
    unsigned char *p = TokenText(n);
    unsigned char c;
    do {
        c = *p++;
        if (Version == QUESTPROBE3_TYPE)
            Q3PrintChar(c & TOKEN_BYTE_MASK);
        else
            OutChar(c & TOKEN_BYTE_MASK);
    } while (p < EndOfData && !(c & TOKEN_LAST_BYTE));
}

/* Print the nth text entry from a QP3 text table. Entries are delimited
   by QP3_ENTRY_DELIM (0x1f), with QP3_TABLE_END (0x18) marking end-of-table. Bytes >=
   QP3_TOKEN_BASE (0x7B) are tokens. */
static void Q3PrintText(unsigned char *stream, int text_index)
{
    /* Skip phase: walk past `text_index` entries by jumping to the next
       delimiter each time. The inner loop stops at QP3_ENTRY_DELIM,
       QP3_TABLE_END, or end of buffer. */
    while (text_index > 0 && stream < EndOfData) {
        while (stream < EndOfData && *stream != QP3_ENTRY_DELIM && *stream != QP3_TABLE_END)
            stream++;
        text_index--;
        stream++;
    }
    /* Print phase: emit every byte of the target entry. Bytes below
       QP3_TOKEN_BASE are literal characters; bytes at or above are token
       indices that expand to multi-byte text. Stops at QP3_ENTRY_DELIM
       (post-test below), QP3_TABLE_END, or end of buffer. */
    do {
        if (stream >= EndOfData || *stream == QP3_TABLE_END)
            return;
        if (*stream >= QP3_TOKEN_BASE)
            PrintToken(*stream);
        else
            Q3PrintChar(*stream);
    } while (*stream++ != QP3_ENTRY_DELIM);
}

/* Print the nth text entry (version 1+). Entries are delimited by MSG_END
   (0x7e) or MSG_END_SPACE (0x5E, trailing space). Each byte is a token index. */
static void PrintText1(unsigned char *stream, int text_index)
{
    /* Skip phase: walk past `text_index` entries. Each entry is a run of
       token-index bytes ending in MSG_END or MSG_END_SPACE. Bail out if
       the stream runs out before we've skipped enough — the table is
       shorter than the caller thinks. */
    while (text_index > 0 && stream < EndOfData) {
        while (stream < EndOfData && *stream != MSG_END && *stream != MSG_END_SPACE)
            stream++;
        if (stream >= EndOfData)
            return;
        text_index--;
        stream++;
    }
    /* Print phase: expand each token index until we hit the terminator. */
    while (stream < EndOfData && *stream != MSG_END && *stream != MSG_END_SPACE)
        PrintToken(*stream++);
    /* MSG_END_SPACE marks an entry whose printed form should be followed
       by a space — defer it via PendSpace so the next OutChar emits it. */
    if (stream < EndOfData && *stream == MSG_END_SPACE) {
        PendSpace = 1;
    }
}

/*
 *	Version 0 is different
 */

static int InventoryLower = 0;

/* Print the nth text entry (version 0 / Rebel Planet). End markers
   MSG_END_SPACE (0x5e) and MSG_END (0x7e) are embedded within the token stream rather
   than between token indices, so we must fully expand each token to find
   them. */
static void PrintTextRebelPlanet(unsigned char *stream, int text_index)
{
    if (stream > EndOfData)
        return;
    /* `token` walks through one token's expansion at a time. When NULL,
       we fetch the next token by consuming a byte from `stream`. */
    unsigned char *token = NULL;
    while (stream < EndOfData) {
        if (token == NULL)
            token = TokenText(*stream++);
        /* Mask off the high "last-byte" marker to get the character. */
        unsigned char c = *token & TOKEN_BYTE_MASK;
        if (c == MSG_END_SPACE || c == MSG_END) {
            /* Terminator: either we're done with the target entry, or
               this was a divider between earlier entries to skip past. */
            if (text_index == 0) {
                if (c == MSG_END_SPACE)
                    PendSpace = 1;
                return;
            }
            text_index--;
        } else if (text_index == 0) {
            /* Inside the target entry — emit the character.
               Rebel Planet's inventory header is followed by lowercase
               items; InventoryLower flips the first letter and clears. */
            if (InventoryLower) {
                c = tolower(c);
                InventoryLower = 0;
            }
            OutChar(c);
        }
        /* Advance within the current token; the high bit on this byte
           marks the token's last byte, so reset `token` to fetch the
           next one on the following iteration. */
        if (token >= EndOfData || (*token++ & TOKEN_LAST_BYTE))
            token = NULL;
    }
}

/* Dispatch to the correct text printer based on game version. */
static void PrintText(unsigned char *stream, int text_index)
{
    switch (Version) {
    case REBEL_PLANET_TYPE: /* In-stream end markers */
        PrintTextRebelPlanet(stream, text_index);
        break;
    case QUESTPROBE3_TYPE:
        Q3PrintText(stream, text_index);
        break;
    default: /* Out-of-stream end markers (faster) */
        PrintText1(stream, text_index);
        break;
    }
}

/* Print message number m from the primary message table. */
static void Message(unsigned char message_index)
{
    unsigned char *stream = FileImage + MessageBase;
    PrintText(stream, message_index);
    /* Most games want a separator space after each message; Blizzard
       Pass handles its own spacing so we skip it there. */
    if (CurrentGame != BLIZZARD_PASS)
        OutChar(' ');
    /* Rebel Planet's message 156 is the "You are carrying" header — the
       inventory list that follows should be lowercased. Reset the flag
       on any other message so a stray 156 doesn't leak into later output. */
    if (BaseGame == REBEL_PLANET && message_index == 156)
        InventoryLower = 1;
    else
        InventoryLower = 0;
}

/* Print message number m from the secondary message table. */
static void Message2(unsigned int message_index)
{
    unsigned char *stream = FileImage + Message2Base;
    PrintText(stream, message_index);
    OutChar(' ');
}

/* Print a system message (inventory prompt, error text, etc.).
   QP3 maps system message IDs to offsets in the main message table. */
void SysMessage(unsigned char message_index)
{
    if (Version == QUESTPROBE3_TYPE) {
        if (message_index == EXITS)
            message_index = 217;
        else
            message_index = 210 + message_index;
    }

    Message(message_index);
}

static void PrintObject(unsigned char object_index)
{
    /* Temple of Terror has an object described as "locked door",
       but there is no way to unlock it and you can just walk
       through it anyway. This seems a bit unfair, so we change its
       description to just "door" here. */
    if (BaseGame == TEMPLE_OF_TERROR && object_index == 41) {
        OutString("door.");
        return;
    }
    unsigned char *stream = FileImage + ObjectBase;
    if (Version == QUESTPROBE3_TYPE)
        stream--;
    PrintText(stream, object_index);
}

static void PrintRoom(unsigned char room_index)
{
    unsigned char *stream = FileImage + RoomBase;
    /* Blizzard Pass allows switching between graphics and short room descriptions,
       or text-only and long descriptions. We always use the long ones.*/
    if (CurrentGame == BLIZZARD_PASS && room_index < 102)
        stream = FileImage + 0x18000 + FileBaselineOffset;
    PrintText(stream, room_index);
}

/* Print a small integer (used by the PRINT action). Zero is printed as
   "00" to match the original game's two-digit display for that case. */
static void PrintNumber(unsigned char number)
{
    char buf[4];
    if (number == 0)
        snprintf(buf, sizeof buf, "00");
    else
        snprintf(buf, sizeof buf, "%d", (int)number);
    OutString(buf);
}

inline static unsigned char Destroyed(void)
{
    return 252;
}

inline static unsigned char Carried(void)
{
    return Flag[2];
}

inline static unsigned char Worn(void)
{
    return Version == QUESTPROBE3_TYPE ? 0 : Flag[3];
}

static unsigned char NumObjects(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 49;

    /* This removes one weird empty "You notice." in Kayleth */
    if (BaseGame == KAYLETH)
        return 120;

    return Flag[6];
}

static int WaitFlag(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 5;
    else if (BaseGame != REBEL_PLANET && BaseGame != KAYLETH)
        return -1;
    else
        return 7;
}

static int CarryItem(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 1;
    /* Flag 5: Items carried. Flag 4: Max carried */
    if (ItemsCarried == MaxCarried && CurrentGame != BLIZZARD_PASS)
        return 0;
    if (ItemsCarried < 255)
        ItemsCarried++;
    return 1;
}

static inline int DarkFlag(void)
{
    return Version == QUESTPROBE3_TYPE ? 43 : 1;
}

static void DropItem(void)
{
    if (Version != QUESTPROBE3_TYPE && ItemsCarried > 0)
        ItemsCarried--;
}

static void Put(unsigned char obj, unsigned char loc)
{
    /* Putting stuff in a room might change the picture, so redraw */
    if (ObjectLoc[obj] == MyLoc || loc == MyLoc)
        Redraw = 1;
    ObjectLoc[obj] = loc;
}

static int Present(unsigned char obj)
{
    if (obj >= NumObjects())
        return 0;
    unsigned char v = ObjectLoc[obj];
    if (v == MyLoc || v == Worn() || v == Carried() ||
        (Version == QUESTPROBE3_TYPE && v == OtherGuyInv && OtherGuyLoc == MyLoc))
        return 1;
    return 0;
}

static int Chance(int n)
{
    return (erkyrath_random() % 100 <= n);
}

void Look(void);

/* Reset all game state to initial values and display the starting room. */
static void NewGame(void)
{
    Redraw = 1;
    /* Zero every flag, then copy the seven initial flag values stored in
       the file header. The remaining 121 flags stay at zero. */
    memset(Flag, 0, sizeof(Flag));
    memcpy(Flag, FileImage + FlagBase, 7);
    if (Version == QUESTPROBE3_TYPE) {
        /* QP3 keeps only Flag[0..3] from the file; force the rest to
           zero (this overrides the file's values for Flag[4..6]) and
           seed the inventory and dark flags. */
        for (int i = 0; i < 124; i++) {
            Flag[4 + i] = 0;
        }
        Flag[42] = 0;                /* dark flag for current character */
        Flag[43] = 0;                /* dark flag for other character */
        Flag[2] = LOC_INV_THING;     /* Carried() — default is Thing, before randomization */
        Flag[3] = LOC_INV_TORCH;     /* Worn() slot used for other character's inventory */
    }
    /* Start in room 0 (the intro/title pseudo-room). */
    MyLoc = 0;
    /* Restore each object's initial location from the file's location table. */
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
    /* Reset the per-game wait counter, if this game uses one. */
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
    Look();
    PrintedOK = 1;
}

static int GetGlkFileLength(strid_t stream)
{
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

int YesOrNo(void)
{
    while (1) {
        uint8_t c = WaitCharacter();
        if (c == 250)
            c = 0;
        OutChar(c);
        OutChar('\n');
        OutFlush();
        if (c == 'n' || c == 'N')
            return 0;
        if (c == 'y' || c == 'Y')
            return 1;
        OutString("Please answer Y or N.\n");
        OutFlush();
    }
}

int LoadGame(void)
{
    frefid_t fileref = glk_fileref_create_by_prompt(fileusage_SavedGame,
        filemode_Read, 0);
    if (!fileref) {
        OutFlush();
        return 0;
    }

    /*
     * Reject the file reference if we're expecting to read from it, and the
     * referenced file doesn't exist.
     */
    if (!glk_fileref_does_file_exist(fileref)) {
        OutString("Unable to open file.\n");
        glk_fileref_destroy(fileref);
        OutFlush();
        return 0;
    }

    strid_t stream = glk_stream_open_file(fileref, filemode_Read, 0);
    if (!stream) {
        OutString("Unable to open file.\n");
        glk_fileref_destroy(fileref);
        OutFlush();
        return 0;
    }

    SavedState *state = SaveCurrentState();

    /* Restore saved game data. */

    if (glk_get_buffer_stream(stream, (char *)Flag, sizeof(Flag)) != sizeof(Flag)
        || glk_get_buffer_stream(stream, (char *)ObjectLoc, sizeof(ObjectLoc)) != sizeof(ObjectLoc)
        || (size_t)GetGlkFileLength(stream) != sizeof(Flag) + sizeof(ObjectLoc)) {
        RecoverFromBadRestore(state);
    } else {
        glk_window_clear(Bottom);
        Look();
        free(state);
        InKaylethPreview = 0;
    }
    glk_stream_close(stream, NULL);
    glk_fileref_destroy(fileref);
    return 1;
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

static int RecursionGuard = 0;

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

/* Print the player's inventory: every object whose location matches the
   "carried" or "worn" code. Each worn item is annotated with a "(worn)"
   suffix; if nothing is carried, the "you have nothing" message prints. */
static void Inventory(void)
{
    /* Rebel Planet does its own capitalization via PrintTextRebelPlanet's
       InventoryLower flag, so we skip OutCaps() for it. */
    if (BaseGame != REBEL_PLANET)
        OutCaps();
    SysMessage(INVENTORY);

    int printed_any = 0;
    for (int i = 0; i < NumObjects(); i++) {
        if (ObjectLoc[i] == Carried() || ObjectLoc[i] == Worn()) {
            printed_any = 1;
            PrintObject(i);
            if (ObjectLoc[i] == Worn()) {
                /* Drop any pending separator, then append "(worn)". */
                OutReplace(0);
                SysMessage(NOWWORN);
                if (CurrentGame == REBEL_PLANET) {
                    /* Rebel Planet uses a comma between items instead of
                       the default trailing space. */
                    OutKillSpace();
                    OutFlush();
                    OutChar(',');
                }
            }
        }
    }
    if (!printed_any) {
        SysMessage(NOTHING);
    } else {
        /* End the list with ". " and arm capitalization for whatever
           prints next (typically the prompt or the next room header). */
        OutReplace('.');
        OutChar(' ');
        OutCaps();
    }
}

static void AnyKey(void)
{
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

static void Okay(void)
{
    SysMessage(OKAY);
    OutChar(' ');
    OutCaps();
    PrintedOK = 1;
}

void SaveGame(void)
{

    strid_t file;
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    if (ref == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    /* Write game state. */
    glk_put_buffer_stream(file, (char *)Flag, sizeof(Flag));
    glk_put_buffer_stream(file, (char *)ObjectLoc, sizeof(ObjectLoc));
    glk_stream_close(file, NULL);
    OutString("Saved.\n");
    OutFlush();
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

/* List available exits from the current room by scanning the exit table. */
static void ListExits(int caps)
{
    /* Each room's exit block in the table is preceded by a marker byte:
       the room number with the high bit set. */
    unsigned char loc_marker = 0x80 | MyLoc;
    unsigned char *entry = FileImage + ExitBase;
    int printed_any = 0;

    while (*entry != loc_marker)
        entry++;
    entry++;

    /* Exit entries are (direction, destination) byte pairs; the block ends
       when we see another marker byte (high bit set). */
    while (*entry < 0x80) {
        if (!printed_any) {
            if (CurrentGame == BLIZZARD_PASS && LastChar == ',')
                LastChar = 0;
            OutCaps();
            SysMessage(EXITS);
            printed_any = 1;
        }
        if (caps)
            OutCaps();
        SysMessage(*entry);
        entry += 2;
    }
    if (printed_any) {
        OutReplace('.');
        OutChar('\n');
    }
}

static void RunStatusTable(void);
static void QP3DrawExtraImages(void);

/* Display the current room: description, visible objects (low objects
   inline, high objects after "You see:"), exits, and optional inventory.
   Draws the room image if graphics are enabled. */
void Look(void)
{
    /* Room 0 is the intro/title pseudo-room; Kayleth's room 91 is an
       analogous non-game screen. */
    int is_intro_room = (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91));

    if (is_intro_room || NoGraphics)
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();
    int i;
    int printed_any = 0;

    PendSpace = 0;
    /* Skip the output reset while a line-input event is pending in the
       bottom window — resetting would lose buffered input. */
    if (!(CurrentWindow == Bottom && LineEvent))
        OutReset();
    TopWindow();

    Redraw = 0;
    if (Transcript)
        glk_put_char_stream(Transcript, '\n');

    OutCaps();

    /* Dark room: print the canned "too dark" message and bail out before
       drawing the room or listing its contents. */
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (BaseGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);

    /* "Low" objects are part of the room description (e.g. "...a sword
       lies here."). They're listed inline rather than after "You see:". */
    for (i = 0; i < NumLowObjects; i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (!printed_any) {
                if (Version == QUESTPROBE3_TYPE) {
                    OutReplace(0);
                    SysMessage(0);
                } else if (BaseGame == HEMAN || BaseGame == REBEL_PLANET) {
                    OutChar(' ');
                }
                printed_any = 1;
            }
            PendSpace = 1;
            PrintObject(i);
        }
    }
    if (printed_any && !isalpha(LastChar))
        OutReplace('.');

    /* QP3 has no "You see:" phase — every visible item is a low object.
       For other versions, continue scanning from the same `i` to list
       higher-numbered objects under a "You see:" heading. */
    if (Version == QUESTPROBE3_TYPE) {
        ListExits(1);
    } else {
        printed_any = 0;
        for (; i < NumObjects(); i++) {
            if (ObjectLoc[i] == MyLoc) {
                if (!printed_any) {
                    /* Only the text-only and hybrid games */
                    if (BaseGame == TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR_64) {
                        OutChar(' ');
                    }
                    SysMessage(YOU_SEE);
                    if (CurrentGame == BLIZZARD_PASS) {
                        PendSpace = 0;
                        OutString(":- ");
                    }
                    if (Version == REBEL_PLANET_TYPE)
                        OutReplace(0);
                    printed_any = 1;
                }
                PrintObject(i);
            }
        }
        if (printed_any)
            OutReplace('.');
        else
            OutChar('.');
        ListExits(BaseGame != TEMPLE_OF_TERROR && BaseGame != HEMAN);
    }

    if (LastChar != '\n')
        OutChar('\n');

    /* Don't show top window inventory in intro rooms */
    if ((Options & FORCE_INVENTORY) && !is_intro_room) {
        OutChar('\n');
        Inventory();
        OutChar('\n');
    }

    OutChar('\n');

    BottomWindow();

    if (MyLoc != 0 && !NoGraphics && TAYLOR_GRAPHICS_ENABLED && Graphics) {
        /* A resize-in-progress: reuse the buffered image rather than
           re-running the room's drawing pipeline. */
        if (Resizing) {
            DrawIrmakPictureFromBuffer();
            return;
        }
        if (Version == QUESTPROBE3_TYPE) {
            /* QP3 draws room images via the status table (which contains
               IMAGE actions). Run it once with DrawImages=255 to draw
               without advancing the turn; stash StopTime so the bypass
               doesn't suppress next turn's real status pass. */
            int tempstop = StopTime;
            StopTime = 0;
            DrawImages = 255;
            RunStatusTable();
            QP3DrawExtraImages();
            StopTime = tempstop;
        } else {
            glk_window_clear(Graphics);
            DrawRoomImage();
        }
    }
}

static void Goto(unsigned char destination)
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

/* Execute one action line: evaluate conditions (bytes with bit 7 clear),
   and if all pass, run the action commands (bytes with bit 7 set).
   Bit 6 on an action byte sets *done to stop further table scanning.

   Convention inside the condition switch below: 'continue' means the
   condition passed — go on to the next condition byte. 'break' falls out
   of the switch and returns from the function, abandoning this line. */
static void ExecuteLineCode(unsigned char *code, int *done)
{
    unsigned char arg1 = 0, arg2 = 0;
    int tmp;

    /* Phase 1: read condition bytes until we see one with the action bit
       set (which signals the start of the action stream for this line). */
    do {
        unsigned char op = *code;

        if (op & ACTION_BIT)
            break;
        code++;
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

        switch (op) {
        case AT:
            if (MyLoc == arg1)
                continue;
            break;
        case NOTAT:
            if (MyLoc != arg1)
                continue;
            break;
        case ATGT:
            if (MyLoc > arg1)
                continue;
            break;
        case ATLT:
            if (MyLoc < arg1)
                continue;
            break;
        case PRESENT:
            if (Present(arg1))
                continue;
            break;
        case HERE:
            if (ObjectLoc[arg1] == MyLoc)
                continue;
            break;
        case ABSENT:
            if (!Present(arg1))
                continue;
            break;
        case NOTHERE:
            if (ObjectLoc[arg1] != MyLoc)
                continue;
            break;
        case CARRIED:
            if (ObjectLoc[arg1] == Carried() || ObjectLoc[arg1] == Worn())
                continue;
            break;
        case NOTCARRIED:
            if (ObjectLoc[arg1] != Carried() && ObjectLoc[arg1] != Worn())
                continue;
            break;
        case WORN:
            if (ObjectLoc[arg1] == Worn())
                continue;
            break;
        case NOTWORN:
            if (ObjectLoc[arg1] != Worn())
                continue;
            break;
        case NODESTROYED:
            if (ObjectLoc[arg1] != Destroyed())
                continue;
            break;
        case DESTROYED:
            if (ObjectLoc[arg1] == Destroyed())
                continue;
            break;
        case ZERO:
            if (BaseGame == TEMPLE_OF_TERROR) {
                /* Unless we have kicked sand in the eyes of the guard, tracked by flag 63,
                 make sure they kill us if we try to pass, by setting flag 28 to zero */
                /* This fixes a bug in the original game, which lets us just walk
                   past the guard */
                if (arg1 == 28 && Flag[63] == 0 && Word[0] == 20 && Word[1] == 162)
                    Flag[28] = 0;
            }
            if (Flag[arg1] == 0)
                continue;
            break;
        case NOTZERO:
            if (Flag[arg1] != 0)
                continue;
            break;
        /* WORD1/2/3 match against the third/fourth/fifth parsed input
           words (Word[0] and Word[1] are the verb and primary noun). */
        case WORD1:
            if (Word[2] == arg1)
                continue;
            break;
        case WORD2:
            if (Word[3] == arg1)
                continue;
            break;
        case WORD3:
            if (Word[4] == arg1)
                continue;
            break;
        case CHANCE:
            if (Chance(arg1))
                continue;
            break;
        case LT:
            if (Flag[arg1] < arg2)
                continue;
            break;
        case GT:
            if (Flag[arg1] > arg2)
                continue;
            break;
        case EQ:
            /* Fix final puzzle (Flag 12 conflict) */
            if (BaseGame == TEMPLE_OF_TERROR) {
                if (arg1 == 12 && arg2 == 4)
                    arg1 = 60;
            }
            if (Flag[arg1] == arg2) {
                continue;
            }
            break;
        case NE:
            if (Flag[arg1] != arg2)
                continue;
            break;
        case OBJECTAT:
            if (ObjectLoc[arg1] == arg2)
                continue;
            break;
        default:
            fprintf(stderr, "Unknown condition %d.\n",
                op);
            break;
        }
#ifdef DEBUG
        fprintf(stderr, "\n");
#endif
        /* Condition failed (or unknown): abandon this line and let the
           caller advance to the next one. */
        return;
    } while (1);

    /* Phase 2: all conditions passed — record that we did something this
       turn and execute the action bytes until we see one without the
       action bit (i.e. the next line's conditions) or run off the end. */
    ActionsExecuted = 1;

    do {
        unsigned char op = *code;
        if (!(op & ACTION_BIT))
            break;

#ifdef DEBUG
        if (op & ACTION_DONE_BIT)
            fprintf(stderr, "DONE:");
        if (Version == QUESTPROBE3_TYPE)
            fprintf(stderr, "%s(%d) ", Action[Q3Action[op & ACTION_OP_MASK]], op & ACTION_OP_MASK);
        else
            fprintf(stderr, "%s(%d) ", Action[op & ACTION_OP_MASK], op & ACTION_OP_MASK);
#endif

        code++;
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

        switch (op) {
        case LOADPROMPT:
            if (LoadPrompt()) {
                *done = 1;
                return;
            }
            break;
        case QUIT:
            if (!RecursionGuard) {
                RecursionGuard = 1;
                QuitGame();
            }
            *done = 1;
            return;
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
                return;
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
#if defined(GLK_MODULE_GARGLKBLEEP)
            garglk_zbleep(1 + (arg1 == 250));
#elif defined(SPATTERLIGHT)
            fprintf(stderr, "BEEP: arg1: %d arg2: %d\n", arg1, arg2);
            win_beep(1 + (arg1 == 250));
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
            if (MyLoc == 3 || Flag[DarkFlag()]) {
                DrawBlack();
                break;
            }
            if (arg1 == 0) {
                ClearGraphMem();
                DrawPictureNumber(MyLoc - 1, 1);
            } else if (arg1 == 45 && ObjectLoc[48] != MyLoc) {
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
        if (WasDark != Flag[DarkFlag()])
            Redraw = 1;
    } while (1);
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
static void QP3DrawExtraImages(void)
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
static void RunStatusTable(void)
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
static void RunCommandTable(void)
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

/* Check if direction word v has a matching exit from the current room.
   If so, move the player there and return 1. */
static int AutoExit(unsigned char v)
{
    unsigned char *ptr = FileImage + ExitBase;
    unsigned char want = MyLoc | 0x80;
    while (*ptr != want) {
        if (*ptr == 0xfe)
            return 0;
        ptr++;
    }
    ptr++;
    while (*ptr < 0x80) {
        if (*ptr == v) {
            Goto(ptr[1]);
            return 1;
        }
        ptr += 2;
    }
    return 0;
}

/* Check if a word code is a direction (N/S/E/W/U/D etc.). */
static int IsDir(unsigned char word)
{
    if (word == 0)
        return 0;
    if (Version == QUESTPROBE3_TYPE) {
        return (word <= 4 || word == 57 || word == 60);
    } else
        return (word <= 10);
}

extern int FoundExtraCommand;

/* Process one player command: try extra commands, direction auto-exits,
   the command action table, and implicit verb carry-forward. Then run
   the status table and handle any pending room redraw. */
static void RunOneInput(void)
{
    PrintedOK = 0;

    if (FoundExtraCommand) {
        if (TryExtraCommand()) {
            if (Redraw)
                Look();
            return;
        }
    }
    if (Word[0] == 0 && Word[1] == 0) {
        if (TryExtraCommand() == 0) {
            OutCaps();
            SysMessage(I_DONT_UNDERSTAND);
            StopTime = 2;
        } else {
            if (Redraw)
                Look();
        }
        return;
    }
    if (IsDir(Word[0]) || (Word[0] == GoVerb && IsDir(Word[1]))) {
        if (AutoExit(Word[0]) || AutoExit(Word[1])) {
            StopTime = 0;
            RunStatusTable();
            if (Redraw)
                Look();
            return;
        }
    }

    /* Handle IT */
    if (Word[1] == 128)
        Word[1] = LastNoun;
    if (Word[1] != 0)
        LastNoun = Word[1];

    OutCaps();
    RunCommandTable();

    if (ActionsExecuted == 0) {
        int OriginalVerb = Word[0];
        if (TryExtraCommand() == 0) {
            if (LastVerb) {
                Word[4] = Word[3];
                Word[3] = Word[2];
                Word[2] = Word[1];
                Word[1] = Word[0];
                Word[0] = LastVerb;
                RunCommandTable();
            }
            if (ActionsExecuted == 0) {
                if (IsDir(OriginalVerb) || (Word[0] == GoVerb && IsDir(Word[1]))) {
                    SysMessage(YOU_CANT_GO_THAT_WAY);
                } else if (FoundVerb) {
                    SysMessage(THATS_BEYOND_MY_POWER);
                } else if (FoundNoun == 1)  {
                    SysMessage(I_DONT_UNDERSTAND_THAT_VERB);
                } else {
                    SysMessage(I_DONT_UNDERSTAND);
                }
                OutFlush();
                StopTime = 1;
                return;
            }
        } else {
            if (Redraw)
                Look();
            return;
        }
    }

    if (Word[0] != 0)
        LastVerb = Word[0];

    if (Redraw && !((BaseGame == REBEL_PLANET && MyLoc == 250) || (BaseGame == KAYLETH && MyLoc == 15))) {
        Look();
    }

    Redraw = 0;

    int waitflag = WaitFlag();

    if (waitflag != -1 && Flag[waitflag] > 1)
        Flag[waitflag]++;

    do {
        if (waitflag != -1 && Flag[waitflag]) {
            Flag[waitflag]--;
            if (LastChar != '\n')
                OutChar('\n');
        }

        if (Version == QUESTPROBE3_TYPE) {
            DrawImages = 0;
            RunStatusTable();
            DrawImages = 255;
            int tempstop = StopTime;
            StopTime = 0;
            RunStatusTable();
            QP3DrawExtraImages();
            StopTime = tempstop;
        } else {
            RunStatusTable();
        }

        if (Redraw) {
            Look();
        }
        Redraw = 0;

    } while (waitflag != -1 && Flag[waitflag] > 0);
    if (AnimationRunning)
        glk_request_timer_events(AnimationRunning);
    if (waitflag != -1)
        Flag[waitflag] = 0;
}

/* Resolve all data table offsets from the Game struct, applying
   FileBaselineOffset to convert from original Z80 addresses. */
static void FindTables(void)
{
    TokenBase = FindTokens();
    RoomBase = Game->start_of_room_descriptions + FileBaselineOffset;
    ObjectBase = Game->start_of_item_descriptions + FileBaselineOffset;
    StatusBase = Game->start_of_automatics + FileBaselineOffset;
    ActionBase = Game->start_of_actions + FileBaselineOffset;
    ExitBase = Game->start_of_room_connections + FileBaselineOffset;
    FlagBase = FindFlags();
    ObjLocBase = Game->start_of_item_locations + FileBaselineOffset;
    MessageBase = Game->start_of_messages + FileBaselineOffset;
    Message2Base = Game->start_of_messages_2 + FileBaselineOffset;

    if (BaseGame == KAYLETH) {
        AnimationData = FindCode("\xff\x00\x00\x00\x0f\x00\x5d\x0f\x00\x61", 0, 10);
        if (AnimationData == -1)
            AnimationData = FindCode("\xff\x00\x00\x15\x0f\x00\x5d\x0f\x00\x61", 0, 10);
    }
}


/* Guess where "low" (room-description) objects end for Rebel Planet.
   Scans object descriptions looking for a comma at the end of an entry,
   which signals the start of "high" (standalone) objects. */
static int GuessLowObjectEndRebelPlanet(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *t = NULL;
    unsigned char c = 0, lc;
    int n = 0;

    while (p < EndOfData) {
        if (t == NULL)
            t = TokenText(*p++);
        lc = c;
        c = *t & TOKEN_BYTE_MASK;
        if (c == MSG_END_SPACE || c == MSG_END) {
            if (lc == ',' && n > 20)
                return n;
            n++;
        }
        if (*t++ & TOKEN_LAST_BYTE)
            t = NULL;
    }
    return -1;
}

/* Guess the boundary between low objects (embedded in room descriptions)
   and high objects (listed separately after "You see:"). Low object
   descriptions end with a comma in the last token. */
static int GuessLowObjectEnd(void)
{
    /* Can't automatically guess in this case */
    if (CurrentGame == BLIZZARD_PASS)
        return 70;
    else if (Version == QUESTPROBE3_TYPE)
        return 49;

    if (Version == REBEL_PLANET_TYPE)
        return GuessLowObjectEndRebelPlanet();

    unsigned char *obj_text = FileImage + ObjectBase;
    int obj_index = 0;

    while (obj_index < NumObjects()) {
        while (*obj_text != MSG_END && *obj_text != MSG_END_SPACE) {
            obj_text++;
        }
        /* obj_text[-1] is the last token index of the description; find
           that token's last byte to check whether it ends in a comma. */
        unsigned char *last_byte = TokenText(obj_text[-1]);
        while (!(*last_byte & TOKEN_LAST_BYTE)) {
            last_byte++;
        }
        if ((*last_byte & TOKEN_BYTE_MASK) == ',')
            return obj_index;
        obj_index++;
        obj_text++;
    }
    fprintf(stderr, "Unable to guess the last description object.\n");
    return 0;
}

static void RestartGame(void)
{
    RecursionGuard = 0;
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    OutFlush();
    glk_window_clear(Bottom);
    Look();
    RunStatusTable();
    ShouldRestart = 0;
    Look();
}

int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    if (argc > 1)
        while (argv[1]) {
            if (*argv[1] != '-')
                break;
            switch (argv[1][1]) {
            case 'n':
                Options |= NO_DELAYS;
                break;
            }
            argv++;
            argc--;
        }

    if (argv[1] == NULL) {
        fprintf(stderr, "%s: <file>.\n", argv[0]);
        glk_exit();
    }

    size_t namelen = strlen(argv[1]);
    Filename = MemAlloc(namelen + 1);
    strncpy(Filename, argv[1], namelen);
    Filename[namelen] = '\0';

    FileImage = ReadFileIfExists(Filename, &FileImageLen);
    if (FileImage == NULL) {
        perror(Filename);
        CleanupAndExit();
    }

    FileImage = ProcessFile(FileImage, &FileImageLen);
    EndOfData = FileImage + FileImageLen;

#ifdef GARGLK
    garglk_set_program_name("TaylorMade 0.4");
    garglk_set_program_info("TaylorMade 0.4 by Alan Cox\n"
                            "Glk port, graphics and Questprobe 3 support by Petter Sjölund\n");
    const char *s;
    if ((s = strrchr(Filename, '/')) != NULL || (s = strrchr(Filename, '\\')) != NULL) {
        garglk_set_story_name(s + 1);
    } else {
        garglk_set_story_name(Filename);
    }
#endif

    return 1;
}

//#ifdef DEBUG
//
//static void PrintConditionAddresses(void)
//{
//    fprintf(stderr, "Memory addresses of conditions\n\n");
//    uint16_t conditionsOffsets = 0x56A8 + FileBaselineOffset;
//    uint8_t *conditions;
//    conditions = &FileImage[conditionsOffsets];
//    for (int i = 1; i < 20; i++) {
//        uint16_t address = READ_LE_UINT16_AND_ADVANCE(&conditions);
//        fprintf(stderr, "Condition %02d: 0x%04x (%s)\n", i, address, Condition[Q3Condition[i]]);
//    }
//    fprintf(stderr, "\n");
//}
//
//static void PrintActionAddresses(void)
//{
//    fprintf(stderr, "Memory addresses of actions\n\n");
//    uint16_t actionOffsets = 0x591C + FileBaselineOffset;
//    uint8_t *actions;
//    actions = &FileImage[actionOffsets];
//    for (int i = 1; i < 24; i++) {
//        uint16_t address = READ_LE_UINT16_AND_ADVANCE(&actions);
//        fprintf(stderr, "   Action %02d: 0x%04x (%s)\n", i, address, Action[Q3Action[i]]);
//    }
//    fprintf(stderr, "\n");
//}
//
//#endif

/* Identify the game by trying each entry in the games[] table. For each
   candidate, compute the baseline offset from the verb table and check
   whether the token table falls at the expected relative position. */
static GameInfo *DetectGame(size_t LocalVerbBase)
{
    GameInfo *LocalGame;

    for (int i = 0; i < NUMGAMES; i++) {
        LocalGame = &games[i];
        FileBaselineOffset = (long)LocalVerbBase - (long)LocalGame->start_of_dictionary;
        long diff = FindTokens() - LocalVerbBase;
        if ((LocalGame->start_of_tokens - LocalGame->start_of_dictionary) == diff) {
#ifdef DEBUG
            fprintf(stderr, "This is %s\n", LocalGame->Title);
#endif
            return LocalGame;
        } else {
#ifdef DEBUG
            fprintf(stderr, "Diff for game %s: %d. Looking for %ld\n", LocalGame->Title, LocalGame->start_of_tokens - LocalGame->start_of_dictionary, diff);
#endif
        }
    }
    return NULL;
}

/* Restore the FileImage globals to a previously saved state. Used when
   switching back from a companion file. */
static void UnparkFileImage(uint8_t *ParkedFile, size_t ParkedLength, long ParkedOffset, int FreeCompanion)
{
    FileImage = ParkedFile;
    FileImageLen = ParkedLength;
    FileBaselineOffset = ParkedOffset;
    if (FreeCompanion)
        free(CompanionFile);
}

/* Temple of Terror has separate graphics and text-only versions. If we
   loaded one, look for the other by swapping the last character before
   the extension ('a' <-> 'b'). If found, offer to create a hybrid that
   uses the text-only version's longer prose with the graphics version's
   images. */
static void LookForSecondTOTGame(void)
{
    size_t namelen = strlen(Filename);
    char *secondfile = MemAlloc(namelen + 1);
    strncpy(secondfile, Filename, namelen);
    secondfile[namelen] = '\0';

    char *period = strrchr(secondfile, '.');
    if (period == NULL)
        period = &secondfile[namelen - 1];
    else
        period--;

    if (CurrentGame == TEMPLE_OF_TERROR)
        *period = 'b';
    else
        *period = 'a';

    size_t filelength;

    CompanionFile = ReadFileIfExists(secondfile, &filelength);

    if (CompanionFile == NULL) {
        return;
    }

    uint8_t *ParkedFile = FileImage;
    size_t ParkedLength = FileImageLen;
    size_t ParkedOffset = FileBaselineOffset;

    CompanionFile = ProcessFile(CompanionFile, &filelength);

    FileImage = CompanionFile;
    FileImageLen = filelength;

    size_t AltVerbBase = FindCode("NORT\001N", 0, 6);

    if (AltVerbBase == -1) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    GameInfo *AltGame = DetectGame(AltVerbBase);

    if ((CurrentGame == TOT_TEXT_ONLY && AltGame->gameID != TEMPLE_OF_TERROR) || (CurrentGame == TEMPLE_OF_TERROR && AltGame->gameID != TOT_TEXT_ONLY)) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    Display(Bottom, "Found files for both the text-only version and the graphics version of Temple of Terror.\n"
                    "Would you like to use the longer texts from the text-only version along with the graphics from the other file? (Y/N) ");
    if (!YesOrNo()) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    int index = 0;

    if (CurrentGame == TOT_TEXT_ONLY) {
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        InitGraphics();
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
    } else {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
        InitGraphics();
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        FileImage = CompanionFile;
        FileImageLen = filelength;
        VerbBase = AltVerbBase;
    }

    EndOfData = FileImage + FileImageLen;
}

void LoadKaylethAnimationData(void);

/* Main entry point. Loads and identifies the game file, locates all data
   tables, initializes graphics, and enters the main input/action loop. */
void glk_main(void)
{
    /* Step 1: load the game file. DetectC64 handles both raw image files
       and C64 disk-image containers, decrunching as needed; the file is
       installed into FileImage/FileImageLen on success. */
    if (DetectC64(&FileImage, &FileImageLen) != UNKNOWN_GAME) {
        EndOfData = FileImage + FileImageLen;
    } else {
        fprintf(stderr, "DetectC64 did not recognize the game\n");
    }

#ifdef DEBUG
    fprintf(stderr, "Loaded %zu bytes.\n", FileImageLen);
#endif

    /* Step 2: locate the verb table by scanning for its signature — the
       dictionary always starts with "NORT" + code 1 + "N" (the first
       direction word, "NORTH", with code 1 followed by its alias "N"). */
    VerbBase = FindCode("NORT\001N", 0, 6);
    if (VerbBase == -1) {
        fprintf(stderr, "No verb table!\n");
        glk_exit();
    }

    /* Step 3: identify which game we're playing. The verb table's offset
       relative to its expected (in-game) address tells us how far the
       loaded file's addressing differs from the original Z80 layout. */
    if (!Game)
        Game = DetectGame(VerbBase);
    if (Game == NULL) {
        fprintf(stderr, "Did not recognize game!\n");
        glk_exit();
    } else {
        FileBaselineOffset = VerbBase - Game->start_of_dictionary;
    }
#ifdef DEBUG
    fprintf(stderr, "FileBaselineOffset: %ld\n", FileBaselineOffset);
#endif

    /* Seed the RNG. In determinism mode (used by automated tests and
       transcript playback) we use a fixed seed; otherwise time-based. */
#ifdef SPATTERLIGHT
    if (gli_determinism) {
        set_erkyrath_random(1234);
    } else
#endif
    set_erkyrath_random(0);

    DisplayInit();

    /* Temple of Terror ships as a paired text+graphics duo; check whether
       a companion file is present alongside the loaded one. */
    if (CurrentGame == TEMPLE_OF_TERROR || CurrentGame == TOT_TEXT_ONLY) {
        LookForSecondTOTGame();
    }

    /* Cache the dictionary code for "GO" — used by the implicit-verb
       fallback when the player types just a direction. */
    GoVerb = ParseWord("GO");

    InitGraphics();

    /* QP3 and Rebel Planet separate top and lower text windows by
       a row of '=' rather than the default '_'. */
    if (CurrentGame == QUESTPROBE3 || CurrentGame == REBEL_PLANET)
        DelimiterChar = '=';

    /* Step 4: locate all the remaining tables (tokens, messages, rooms,
       actions, etc.) using game-specific offsets or signature scans. */
    FindTables();
#ifdef DEBUG
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
#endif

    /* Step 5: initialize state and present the opening room. NewGame()
       resets flags and objects; GuessLowObjectEnd() determines which
       objects render inline vs. in the "You see:" list; the initial
       state is snapshotted for the RESTART command. */
    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    InitialState = SaveCurrentState();

    /* Run the status table once on entry so any per-turn setup actions
       (e.g. drawing the first room image) execute before the prompt. */
    RunStatusTable();
    if (Redraw) {
        OutFlush();
        Look();
    }

    /* Kayleth begins with a non-interactive "preview" sequence before
       handing control to the player; load its animation data and arm
       the flag so that undo is suppressed until MyLoc reaches 1. */
    if (BaseGame == KAYLETH) {
        LoadKaylethAnimationData();
        InKaylethPreview = 1;
    }

    /* Main turn loop: snapshot undo state, read a command, execute it,
       and repeat forever. Exits are handled by glk_exit() from within
       QUIT/restart paths. */
    while (1) {
        if (ShouldRestart) {
            RestartGame();
            /* Kayleth's restart sequence is itself non-interactive, so
               don't capture undo until it's done. */
            if (BaseGame != KAYLETH) {
                SaveUndo();
            }
        } else if (!StopTime && !InKaylethPreview) {
            /* Take an undo snapshot before each real player turn, but
               skip while time is paused or the preview is still running. */
            SaveUndo();
        }
        Parser();
        FirstAfterInput = 1;
        RunOneInput();
        if (StopTime) {
            StopTime--;
        } else if (!InKaylethPreview) {
            JustStarted = 0;
        }
        /* Kayleth's preview ends when the player is put in room 1. */
        if (MyLoc == 1) {
            InKaylethPreview = 0;
        }
    }
}
