#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include <stdarg.h>
#include <strings.h>

#include "glk.h"
#include "glkimp.h"
#include "glkstart.h"
#include "restorestate.h"
#include "decompressz80.h"
#include "extracttape.h"
#include "decrypttotloader.h"
#include "utility.h"
#include "sagadraw.h"
#include "extracommands.h"

#include "taylor.h"

uint8_t Flag[128];
uint8_t ObjectLoc[256];
static uint8_t Word[5];
char wb[5][17];

uint8_t *FileImage = NULL;
uint8_t *EndOfData = NULL;
size_t FileImageLen = 0;
static size_t VerbBase;
static size_t TokenBase;
static size_t MessageBase;
static size_t Message2Base;
static size_t RoomBase;
static size_t ObjectBase;
static size_t ExitBase;
static size_t ObjLocBase;
static size_t StatusBase;
static size_t ActionBase;
static size_t FlagBase;

int NumLowObjects;

static int ActionsExecuted;
int Redraw = 0;

#define OtherGuyLoc (Flag[1])
#define OtherGuyInv (Flag[3])
#define TurnsLow (Flag[26])
#define TurnsHigh (Flag[27])
#define IsThing (Flag[31])
#define ThingAsphyx (Flag[47])
#define TorchAsphyx (Flag[48])
#define DrawImages (Flag[52])

static int FirstAfterInput = 0;

int stop_time = 0;
int just_started = 1;
int should_restart = 0;

int NoGraphics = 0;

long FileBaselineOffset = 0;

char DelimiterChar = '_';

struct GameInfo *Game = NULL;
extern struct GameInfo games[];

static char *Filename;
static uint8_t *CompanionFile;

extern struct SavedState *initial_state;

extern int AnimationRunning;

#ifdef DEBUG

/*
 *	Debugging
 */
static unsigned char WordMap[256][5];

static char *Condition[]={
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

static char *Action[]={
    "<ERROR>",
    "LOAD?",
    "QUIT",
    "INVENTORY",
    "ANYKEY",
    "SAVE",
    "DROPALL",
    "LOOK",
    "OK",		/* Guess */
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
    "PUT",		/* ?? */
    "SWAP",
    "SWAPF",
    "MEANS",
    "PUTWITH",
    "BEEP",		/* Rebel Planet at least */
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

    while(1) {
        if(p[4] == 255)
            break;
        if(WordMap[p[4]][0] == 0)
            memcpy(WordMap[p[4]], p, 4);
        p+=5;
    }
}

static void PrintWord(unsigned char word)
{
    if(word == 126)
        fprintf(stderr, "*	  ");
    else if(word == 0 || WordMap[word][0] == 0)
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

static char Q3Condition[] = {
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

static char Q3Action[]={
    ACTIONERROR,
    SWITCHINVENTORY, /* Swap inventory and dark flag */
    DIAGNOSE, /* Print Reed Richards' watch status message */
    LOADPROMPT,
    QUIT,
    SHOWINVENTORY,
    ANYKEY,
    SAVE,
    CONTINUE, /* Set "condition failed" flag, Flag[118], to 1 */
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

size_t FindCode(const char *x, size_t base, size_t len)
{
    unsigned char *p = FileImage + base;
    while(p < FileImage + FileImageLen - len) {
        if(memcmp(p, x, len) == 0)
            return p - FileImage;
        p++;
    }
    return (size_t)-1;
}

static size_t FindFlags(void)
{
    if (CurrentGame == QUESTPROBE3)
        return 0x1b70 + FileBaselineOffset;
    /* Questprobe */
    size_t pos = FindCode("\xE7\x97\x51\x95\x5B\x7E\x5D\x7E\x76\x93", 0, 10);
    if(pos == -1) {
        /* Look for the flag initial block copy */
        pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0,8 );
        if(pos == -1) {
            fprintf(stderr, "Cannot find initial flag data.\n");
            glk_exit();
        } else return pos + 5;
    }
    return pos + 11;
}

static size_t FindObjectLocations(void)
{
    if (CurrentGame == QUESTPROBE3)
        return 0x90A3 - 0x4000 + FileBaselineOffset;

    size_t pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
    if(pos == -1) {
        fprintf(stderr, "Cannot find initial object data.\n");
        glk_exit();
    }
    pos = FileImage[pos - 16] + (FileImage[pos - 15] << 8);
    return pos - 0x4000 + FileBaselineOffset;
}

static size_t FindExits(void)
{
    size_t pos = 0;

    while((pos = FindCode("\x1A\xBE\x28\x0B\x13", pos+1, 5)) != -1)
    {
        pos = FileImage[pos - 5] + (FileImage[pos - 4] << 8) - 0x4000;
        return pos + FileBaselineOffset;
    }
    fprintf(stderr, "Cannot find initial exit data.\n");
    glk_exit();
}

static int LooksLikeTokens(size_t pos)
{
    unsigned char *p = FileImage + pos;
    int n = 0;
    int t = 0;
    while(n < 512) {
        unsigned char c = p[n] & 0x7F;
        if(c >= 'a' && c <= 'z')
            t++;
        n++;
    }
    if(t > 300)
        return 1;
    return 0;
}

static void TokenClassify(size_t pos)
{
    unsigned char *p = FileImage + pos;
    int n = 0;
    while(n++ < 256) {
        do {
            if(*p == 0x5E || *p == 0x7E)
                Version = 0;
        } while(!(*p++ & 0x80));
    }
}

static size_t FindTokens(void)
{
    size_t addr;
    size_t pos = 0;
    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if(pos == -1) {
            /* Questprobe */
            pos = FindCode("\x58\x58\x58\x58\xFF", 0, 5);
            if(pos == -1) {
                /* Last resort */
                addr = FindCode("You are in ", 0, 11) - 1;
                if(addr == -1) {
                    fprintf(stderr, "Unable to find token table.\n");
                    return 0;
                }
                return addr;
            } else
                return pos + 6;
        }
        addr = (FileImage[pos-1] <<8 | FileImage[pos-2]) - 0x4000 + FileBaselineOffset;
    }
    while(LooksLikeTokens(addr) == 0);
    TokenClassify(addr);
    return addr;
}

static char LastChar = 0;
static int Upper = 0;
int PendSpace = 0;

strid_t room_description_stream = NULL;

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

static void OutWrite(char c)
{
    if(isalpha(c) && Upper)
    {
        c = toupper(c);
        Upper = 0;
    }
    PrintCharacter(c);
}

static void OutFlush(void)
{
    if(LastChar)
        OutWrite(LastChar);
    if(PendSpace && LastChar != '\n')
        OutWrite(' ');
    LastChar = 0;
    PendSpace = 0;
}

static void OutReset(void)
{
    OutFlush();
}

static void OutCaps(void)
{
    if (LastChar) {
        OutWrite(LastChar);
        LastChar = 0;
    }
    Upper = 1;
}

static int periods = 0;
int JustWrotePeriod = 0;

static void OutChar(char c)
{
    if(c == ']')
        c = '\n';

    if (c == '.') {
        periods++;
        if (LastChar == '?' || FirstAfterInput || isspace(LastChar) || LastChar == '.') {
            c = ' ';
            if (LastChar == ' ')
                LastChar = 0;
        }
        PendSpace = 0;
    } else {
        if (periods && !JustWrotePeriod && c !=',' && c != '?' && c != '!') {
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

    if(c == ' ') {
        PendSpace = 1;
        return;
    }
    if (FirstAfterInput) {
        FirstAfterInput = 0;
        PendSpace = 0;
        Upper = 1;
    }
    if(LastChar) {
        if (isspace(LastChar))
            PendSpace = 0;
        if (LastChar == '.' && JustWrotePeriod) {
            LastChar = 0;
        } else {
            OutWrite(LastChar);
        }
    }
    if(PendSpace) {
        if (JustWrotePeriod && CurrentGame != KAYLETH)
            Upper = 1;
        OutWrite(' ');
        PendSpace = 0;
    }
    LastChar = c;
    if (LastChar == '\n')
        Upper = 1;
}

static void OutReplace(char c)
{
    LastChar = c;
}

static void OutKillSpace()
{
    PendSpace = 0;
}

static void OutString(char *p)
{
    while(*p)
        OutChar(*p++);
}

static unsigned char *TokenText(unsigned char n)
{
    unsigned char *p = FileImage + TokenBase;

    if (CurrentGame == QUESTPROBE3)
        n -= 0x7b;

    while(n > 0 && p < EndOfData) {
        while((*p & 0x80) == 0)
            p++;
        n--;
        p++;
    }
    return p;
}

static int QPUpper = 0;

void QPrintChar(uint8_t c) { // Print character
    if (c == 0x0d)
        return;

    if (FirstAfterInput)
        QPUpper = 1;

    if (QPUpper && c >= 'a') {
        c -= 0x20; // token is made uppercase
    }
    OutChar(c);
    if (c > '!') {
        QPUpper = 0;
        Upper = 0;
    }
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        QPUpper = 1;
    }
}

static void PrintToken(unsigned char n)
{
    if (CurrentGame == BLIZZARD_PASS && MyLoc == 49 && n == 0x2d) {
        OutChar(0x2d);
        return;
    }
    unsigned char *p = TokenText(n);
    unsigned char c;
    do {
        c = *p++;
        if (CurrentGame == QUESTPROBE3)
            QPrintChar(c & 0x7F);
        else
            OutChar(c & 0x7F);
    } while(p < EndOfData && !(c & 0x80));
}

static void PrintTextQ(unsigned char *p, int n)
{
    while (n > 0) {
        while (*p != 0x1f && *p != 0x18) {
            p++;
        }
        n--;
        p++;
    }
    do  {
        if (*p == 0x18)
            return;
        if (*p >= 0x7b) // if c is >= 0x7b it is a token
            PrintToken(*p);
        else {
            QPrintChar(*p);
        }
    } while (*p++ != 0x1f);
}

static void PrintText1(unsigned char *p, int n)
{
    while(n > 0) {
        while(p < EndOfData && *p != 0x7E && *p != 0x5E)
            p++;
        n--;
        p++;
    }
    while(p < EndOfData && *p != 0x7E && *p != 0x5E)
        PrintToken(*p++);
    if(*p == 0x5E) {
        PendSpace = 1;
    }
}

/*
 *	Version 0 is different
 */

static void PrintText0(unsigned char *p, int n)
{
    if (p > EndOfData)
        return;
    unsigned char *t = NULL;
    unsigned char c;
    while(1) {
        if(t == NULL)
            t = TokenText(*p++);
        c = *t & 0x7F;
        if(c == 0x5E || c == 0x7E) {
            if(n == 0) {
                if(c == 0x5E) {
                    PendSpace = 1;
                }
                return;
            }
            n--;
        }
        else if(n == 0)
            OutChar(c);
        if(t >= EndOfData || (*t++ & 0x80))
            t = NULL;
    }
}

static void PrintText(unsigned char *p, int n)
{
    if (Version == REBEL_PLANET_TYPE) {	/* In stream end markers */
        PrintText0(p, n);
    } else if (Version == QUESTPROBE3_TYPE) {
        PrintTextQ(p, n);
    } else {			/* Out of stream end markers (faster) */
        PrintText1(p, n);
    }
}

static size_t FindMessages(void)
{
    size_t pos = 0;
    /* Newer game format */
    while((pos = FindCode("\xF5\xE5\xC5\xD5\x3E\x2E", pos+1, 6)) != -1) {
        if(FileImage[pos + 6] != 0x32)
            continue;
        if(FileImage[pos + 9] != 0x78)
            continue;
        if(FileImage[pos + 10] != 0x32)
            continue;
        if(FileImage[pos + 13] != 0x21)
            continue;
        return (FileImage[pos+14] + (FileImage[pos+15] << 8)) - 0x4000 + FileBaselineOffset;
    }
    /* Try now for older game format */
    while((pos = FindCode("\xF5\xE5\xC5\xD5\x78\x32", pos+1, 6)) != -1) {
        if(FileImage[pos + 8] != 0x21)
            continue;
        if(FileImage[pos + 11] != 0xCD)
            continue;
        /* End markers in compressed blocks */
        //        Version = REBEL_PLANET_TYPE;
        return (FileImage[pos+9] + (FileImage[pos+10] << 8)) - 0x4000 + FileBaselineOffset;
    }

    /* Questprobe */
    pos = FindCode("\x7F\xF8\x64\x86\xDB\x94\x20\xAD\xD2\x2E\x1F\x66\xE5", 0, 13);

    if(pos == -1) {
        fprintf(stderr, "Unable to locate messages.\n");
        glk_exit();
    }
    return pos;
}

static size_t FindMessages2(void)
{
    size_t pos = 0;
    while((pos = FindCode("\xF5\xE5\xC5\xD5\x78\x32", pos+1, 6)) != -1) {
        if(FileImage[pos + 8] != 0x21)
            continue;
        if(FileImage[pos + 11] != 0xC3)
            continue;
        return (FileImage[pos+9] + (FileImage[pos+10] << 8)) - 0x4000 + (int)FileBaselineOffset;
    }
    fprintf(stderr, "No second message block ?\n");
    return 0;
}

static void Message(unsigned char m)
{
    unsigned char *p = FileImage + MessageBase;
    PrintText(p, m);
    if (CurrentGame == QUESTPROBE3 || CurrentGame == TOT_TEXT_ONLY || CurrentGame == HEMAN)
        OutChar(' ');
}

static void Message2(unsigned int m)
{
    unsigned char *p = FileImage + Message2Base;
    PrintText(p, m);
    if (CurrentGame == TOT_TEXT_ONLY || CurrentGame == HEMAN)
        OutChar(' ');
}

static void SysMessage(unsigned char m)
{
    if (CurrentGame != QUESTPROBE3) {
        Message(m);
        return;
    }
    if (m == EXITS)
        m = 217;
    else
        m = 210 + m;
    Message(m);
}

static size_t FindObjects(void)
{
    size_t pos = 0;
    while((pos = FindCode("\xF5\xE5\xC5\xD5\x32", pos+1, 5)) != -1) {
        if(FileImage[pos + 10] != 0xCD)
            continue;
        if(FileImage[pos +7] != 0x21)
            continue;
        return (FileImage[pos+8] + (FileImage[pos+9] << 8)) - 0x4000 + FileBaselineOffset;
    }

    /* Questprobe */
    pos = FindCode("\x20\xFB\x62\x88\xF4\xAC\xBF\x73\x2C\x18\x20\xFF", 0, 12);
    if(pos == -1) {
        fprintf(stderr, "Unable to locate objects.\n");
        glk_exit();
    }
    return pos;
}

static void PrintObject(unsigned char obj)
{
    unsigned char *p = FileImage + ObjectBase;
    if (CurrentGame == QUESTPROBE3)
        p--;
    PrintText(p, obj);
}

/* Standard format */
static size_t FindRooms(void)
{
    size_t pos = 0;
    while((pos = FindCode("\x3E\x19\xCD", pos+1, 3)) != -1) {
        if(FileImage[pos + 5] != 0xC3)
            continue;
        if(FileImage[pos + 8] != 0x21)
            continue;
        return (FileImage[pos+9] + (FileImage[pos+10] << 8)) - 0x4000 + FileBaselineOffset;
    }

    /* Questprobe */
    pos = FindCode("QUESTPROBE 3: FANTASTIC FOUR", 0, 28);
    if(pos == -1) {
        fprintf(stderr, "Unable to locate rooms.\n");
        glk_exit();
    }
    return pos ;
}


static void PrintRoom(unsigned char room)
{
    unsigned char *p = FileImage + RoomBase;
    if (CurrentGame == BLIZZARD_PASS && room < 102 && room != 0)
        p = FileImage + 0x18000 + FileBaselineOffset;
    PrintText(p, room);
}


static void PrintNumber(unsigned char n)
{
    char buf[4];
    char *p = buf;
    snprintf(buf, 3, "%d", (int)n);
    while(*p)
        OutChar(*p++);
}


static unsigned char Destroyed()
{
    return 252;
}

static unsigned char Carried()
{
    return Flag[2];
}

static unsigned char Worn()
{
    if (CurrentGame == QUESTPROBE3)
        return 0;
    return Flag[3];
}

static unsigned char NumObjects()
{
    if (CurrentGame == QUESTPROBE3)
        return 49;
    return Flag[6];
}

static int WaitFlag()
{
    if (CurrentGame == QUESTPROBE3)
        return 5;
    if (CurrentGame != REBEL_PLANET && CurrentGame != KAYLETH)
        return -1;
    return 7;
}

static int CarryItem(void)
{
    if (CurrentGame == QUESTPROBE3)
        return 1;
    /* Flag 5: items carried, flag 4: max carried */
    if(Flag[5] == Flag[4] && CurrentGame != BLIZZARD_PASS)
        return 0;
    if(Flag[5] < 255)
        Flag[5]++;
    return 1;
}

static int DarkFlag(void) {
    if (CurrentGame == QUESTPROBE3)
        return 43;
    return 1;
}

static void DropItem(void)
{
    if(CurrentGame != QUESTPROBE3 && Flag[5] > 0)
        Flag[5]--;
}

static void Put(unsigned char obj, unsigned char loc)
{
    /* Will need refresh logics somewhere, maybe here ? */
    if(ObjectLoc[obj] == MyLoc || loc == MyLoc)
        Redraw = 1;
    ObjectLoc[obj] = loc;
}

static int Present(unsigned char obj)
{
    unsigned char v = ObjectLoc[obj];
    if(v == MyLoc|| v == Worn() || v == Carried() || (CurrentGame == QUESTPROBE3 && v == OtherGuyInv && OtherGuyLoc == MyLoc))
        return 1;
    return 0;
}

static int Chance(int n)
{
    unsigned long v = (rand() >> 12) ^ time(NULL);
    v%=100;
    if(v > n)
        return 0;
    return 1;
}

void Look(void);

static void NewGame(void)
{
    Redraw = 1;
    memset(Flag, 0, 128);
    memcpy(Flag, FileImage + FlagBase, 7);
    if (CurrentGame == QUESTPROBE3) {
        for (int i = 0; i < 128; i++) {
            Flag[4 + i] = 0;
        }
        Flag[42] = 0;
        Flag[43] = 0;
        Flag[2] = 254;
        Flag[3] = 253;
    }
    Flag[0] = 0;
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
    Look();
}

static int GetFileLength(strid_t stream) {
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

int YesOrNo(void)
{
    while(1) {
        uint8_t c = WaitCharacter();
        if (c == 250)
            c = 0;
        OutChar(c);
        OutChar('\n');
        OutFlush();
        if(c == 'n' || c == 'N')
            return 0;
        if(c == 'y' || c == 'Y')
            return 1;
        OutString("Please answer Y or N.\n");
        OutFlush();
    }
}

int LoadGame(void)
{
        frefid_t fileref = glk_fileref_create_by_prompt (fileusage_SavedGame,
                                                         filemode_Read, 0);
        if (!fileref)
        {
            OutFlush();
            return 0;
        }

        /*
         * Reject the file reference if we're expecting to read from it, and the
         * referenced file doesn't exist.
         */
        if (!glk_fileref_does_file_exist (fileref))
        {
            OutString("Unable to open file.\n");
            glk_fileref_destroy (fileref);
            OutFlush();
            return 0;
        }

        strid_t stream = glk_stream_open_file(fileref, filemode_Read, 0);
        if (!stream)
        {
            OutString("Unable to open file.\n");
            glk_fileref_destroy (fileref);
            OutFlush();
            return 0;
        }

        struct SavedState *state = SaveCurrentState();

        /* Restore saved game data. */

        if (glk_get_buffer_stream(stream, (char *)Flag, 128) != 128 || glk_get_buffer_stream(stream, (char *)ObjectLoc, 256) != 256 || GetFileLength(stream) != 384) {
            RecoverFromBadRestore(state);
        } else {
            glk_window_clear(Bottom);
            Look();
            free(state);
        }
        glk_stream_close (stream, NULL);
        glk_fileref_destroy (fileref);
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
        should_restart = 1;
        return;
    } else {
        glk_exit();
    }
}

static void Inventory(void)
{
    int i;
    int f = 0;
    OutCaps();
    SysMessage(INVENTORY);
    for(i = 0; i < NumObjects(); i++) {
        if(ObjectLoc[i] == Carried() || ObjectLoc[i] == Worn()) {
            f = 1;
            PrintObject(i);
            if(ObjectLoc[i] == Worn())
                SysMessage(NOWWORN);
        }
    }
    if(f == 0)
        SysMessage(NOTHING);
    else {
        if (Version == REBEL_PLANET_TYPE) {
            OutKillSpace();
            OutChar('.');
        } else if (Version == QUESTPROBE3_TYPE) {
            OutReplace(0);
            OutString(". ");
            OutCaps();
        } else {
            OutReplace('.');
            OutChar(' ');
        }
    }
    OutFlush();
}

static void  AnyKey(void) {
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

void SaveGame(void) {

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
    glk_put_buffer_stream (file, (char *)Flag, 128);
    glk_put_buffer_stream (file, (char *)ObjectLoc, 256);
    glk_stream_close(file, NULL);
    OutString("Saved.\n");
    OutFlush();
}

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
            if(CarryItem() == 0) {
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

static void DropAll(void) {
    int i;
    int found = 0;
    for(i = 0; i < NumObjects(); i++) {
        if(ObjectLoc[i] == Carried() && ObjectLoc[i] != Worn()) {
            if (found)
                OutChar('\n');
            found = 1;
            PrintObject(i);
            OutReplace(0);
            OutString("......");
            OutKillSpace();
            OutString("Dropped");
            OutFlush();
            Put(i, MyLoc);
        }
    }
    if (!found) {
        OutString("You have nothing to drop. ");
    }
    Flag[5] = 0;
}

static int GetObject(unsigned char obj) {
    if(ObjectLoc[obj] == Carried() || ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_HAVE_IT);
        return 0;
    }
    if (!(CurrentGame == QUESTPROBE3 && Flag[1] == MyLoc && ObjectLoc[obj] == Flag[3])) {
        if(ObjectLoc[obj] != MyLoc) {
            SysMessage(YOU_DONT_SEE_IT);
            return 0;
        }
    }
    if(CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return 0;
    }
    if (!(CurrentGame == HEMAN && obj == 81) && !(CurrentGame == BLIZZARD_PASS)) {
        SysMessage(OKAY);
        OutChar(' ');
        OutFlush();
        Upper = 1;
    }
    Put(obj, Carried());
    return 1;
}

static void DropObject(unsigned char obj) {
    /* FIXME: check if this is how the real game behaves */
    if(ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return;
    }
    if(ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return;
    }
    if (!(CurrentGame == BLIZZARD_PASS)) {
        SysMessage(OKAY);
        OutChar(' ');
        OutFlush();
        Upper = 1;
    }
    DropItem();
    Put(obj, MyLoc);
}

static void ListExits(int caps)
{
    unsigned char locw = 0x80 | MyLoc;
    unsigned char *p;
    int f = 0;
    p = FileImage + ExitBase;

    while(*p != locw)
        p++;
    p++;
    while(*p < 0x80) {
        if(f == 0) {
            if(CurrentGame == BLIZZARD_PASS && LastChar == ',')
                LastChar = 0;
            OutCaps();
            SysMessage(EXITS);
        }
        f = 1;
        if (caps)
            OutCaps();
        SysMessage(*p);
        p += 2;
    }
    if(f == 1)
    {
        OutReplace('.');
        OutChar('\n');
    }
}

static void RunStatusTable(void);
extern uint8_t buffer[768][9];

void Look(void) {
    if (MyLoc == 0 || (CurrentGame == KAYLETH && MyLoc == 91) || NoGraphics)
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();
    int i;
    int f = 0;

    PendSpace = 0;
    OutReset();
    TopWindow();

    Redraw = 0;
    if (Transcript)
        glk_put_char_stream(Transcript, '\n');

    OutCaps();

    if(Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (CurrentGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);

    for(i = 0; i < NumLowObjects; i++) {
        if(ObjectLoc[i] == MyLoc) {
            if(f == 0) {
                if (CurrentGame == QUESTPROBE3) {
                    OutReplace(0);
                    SysMessage(0);
                } else if (CurrentGame == HEMAN) {
                    OutChar(' ');
                }
            }
            f = 1;
            PendSpace = 1;
            PrintObject(i);
        }
    }
    if(f == 1 && CurrentGame != BLIZZARD_PASS)
        OutReplace('.');

    if (CurrentGame == QUESTPROBE3) {
        ListExits(1);
    } else {
        f = 0;
        for(; i < NumObjects(); i++) {
            if(ObjectLoc[i] == MyLoc) {
                if(f == 0) {
                    if (CurrentGame == TOT_TEXT_ONLY) {
                        OutChar(' ');
                    }
                    SysMessage(YOU_SEE);
                    if (CurrentGame == BLIZZARD_PASS) {
                        PendSpace = 0;
                        OutString(":- ");
                    }
                    if (Version == REBEL_PLANET_TYPE)
                        OutReplace(0);
                }
                f = 1;
                PrintObject(i);
            }
        }
        if(f == 1)
            OutReplace('.');
        else
            OutChar('.');
        ListExits((CurrentGame != TEMPLE_OF_TERROR && CurrentGame != TOT_TEXT_ONLY && CurrentGame != HEMAN));
    }

    if (LastChar != '\n')
        OutChar('\n');
    OutChar('\n');

    BottomWindow();

    if (MyLoc != 0 && !NoGraphics) {
        if (Resizing) {
            DrawSagaPictureFromBuffer();
            return;
        }
        if (CurrentGame == QUESTPROBE3) {
            DrawImages = 1;
            RunStatusTable();
        } else {
            glk_window_clear(Graphics);
            DrawRoomImage();
        }
    }
}


static void Goto(unsigned char loc) {
    Flag[0] = loc;
    Redraw = 1;
}

static void Delay(unsigned char seconds) {
    OutChar(' ');
    OutFlush();

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    glk_request_timer_events(1000 * seconds);
    
    event_t ev;

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(0);
}

static void Wear(unsigned char obj) {
    if(ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return;
    }
    if(ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return;
    }
    DropItem();
    Put(obj, Worn());
}

static void Remove(unsigned char obj) {
    if(ObjectLoc[obj] != Worn()) {
        SysMessage(YOU_ARE_NOT_WEARING_IT);
        return;
    }
    if(CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return;
    }
    Put(obj, Carried());
}

static void Means(unsigned char vb, unsigned char no) {
    Word[0] = vb;
    Word[1] = no;
}

static void SwitchInvFlags(unsigned char a, unsigned char b) {
    if (Flag[2] == a) {
        Flag[2] = b;
        Flag[3] = a;
        ObjectLoc[37] = 253 + (ObjectLoc[37] == 253);
    }
}

static void UpdateQ3Flags(void) {
    if (ObjectLoc[7] == 253)
        ObjectLoc[7] = 254;
    if (IsThing) {
        if (ObjectLoc[2] == 0xfc) {
            /* If the "holding HUMAN TORCH by the hands" object is destroyed (i.e. not held) */
            /* the "location of the other guy" flag is set to the location of the Human Torch object */
            OtherGuyLoc = ObjectLoc[18];
        } else {
            OtherGuyLoc = MyLoc;
        }
        SwitchInvFlags(253, 254);
    } else { /* I'm the HUMAN TORCH */
        if (ObjectLoc[1] == 0xfc) {
            /* If the "holding THING by the hands" object is destroyed (i.e. not held) */
            /* The "location of the other guy" flag is set to the location of the Thing object */
            OtherGuyLoc = ObjectLoc[17];
        } else {
            OtherGuyLoc = MyLoc;
        }
        SwitchInvFlags(254, 253);
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
static void AdjustQuestprobeConditions(unsigned char op, unsigned char *arg1)
{
    switch (op) {
        case 15:
        case 16:
        case 21:
        case 22:
        case 23:
        case 24:
            *arg1 += 4;
            break;
        default:
            break;
    }
}

static void AdjustQuestprobeActions(unsigned char op, unsigned char *arg1, unsigned char *arg2)
{
    switch (op) {
        case 13:
        case 14:
        case 22:
        case 23:
        case 24:
            if (arg1 != NULL)
                *arg1 += 4;
            break;
        case 27:
            if (arg1 != NULL)
                *arg1 += 4;
            if (arg2 != NULL)
                *arg2 += 4;
            break;
        default:
            break;
    }
}

static int TwoConditionParameters() {
    if (CurrentGame == QUESTPROBE3)
        return 16;
    else
        return 21;
}

static int TwoActionParameters() {
    if (CurrentGame == QUESTPROBE3)
        return 18;
    else
        return 22;
}

static void ExecuteLineCode(unsigned char *p, int *done)
{
    unsigned char arg1 = 0, arg2 = 0;
    int n;
    do {
        unsigned char op = *p;

        if(op & 0x80)
            break;
        p++;
        arg1 = *p++;

#ifdef DEBUG
        if (CurrentGame == QUESTPROBE3) {
            unsigned char debugarg1 = arg1;
            AdjustQuestprobeConditions(Q3Condition[op], &debugarg1);
            fprintf(stderr, "%s %d ", Condition[Q3Condition[op]], debugarg1);
        } else {
            fprintf(stderr, "%s %d ", Condition[op], arg1);
        }
#endif
        if (op >= TwoConditionParameters())
        {
            arg2 = *p++;
#ifdef DEBUG
            unsigned char debugarg2 = arg2;
            if (CurrentGame == QUESTPROBE3)
                AdjustQuestprobeConditions(Q3Condition[op], &debugarg2);
            fprintf(stderr, "%d ", debugarg2);
#endif
        }

        if (CurrentGame == QUESTPROBE3) {
            op = Q3Condition[op];
            AdjustQuestprobeConditions(op, &arg1);
        }

        switch(op) {
            case AT:
                if(MyLoc == arg1)
                    continue;
                break;
            case NOTAT:
                if(MyLoc != arg1)
                    continue;
                break;
            case ATGT:
                if(MyLoc > arg1)
                    continue;
                break;
            case ATLT:
                if(MyLoc < arg1)
                    continue;
                break;
            case PRESENT:
                if(Present(arg1))
                    continue;
                break;
            case HERE:
                if(ObjectLoc[arg1] == MyLoc)
                    continue;
                break;
            case ABSENT:
                if(!Present(arg1))
                    continue;
                break;
            case NOTHERE:
                if(ObjectLoc[arg1] != MyLoc)
                    continue;
                break;
            case CARRIED:
                /*FIXME : or worn ?? */
                if(ObjectLoc[arg1] == Carried() || ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case NOTCARRIED:
                /*FIXME : or worn ?? */
                if(ObjectLoc[arg1] != Carried() && ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case WORN:
                if(ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case NOTWORN:
                if(ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case NODESTROYED:
                if(ObjectLoc[arg1] != Destroyed())
                    continue;
                break;
            case DESTROYED:
                if(ObjectLoc[arg1] == Destroyed())
                    continue;
                break;
            case ZERO:
                if(Flag[arg1] == 0)
                    continue;
                break;
            case NOTZERO:
                if(Flag[arg1] != 0)
                    continue;
                break;
            case WORD1:
                if(Word[2] == arg1)
                    continue;
                break;
            case WORD2:
                if(Word[3] == arg1)
                    continue;
                break;
            case WORD3:
                if(Word[4] == arg1)
                    continue;
                break;
            case CHANCE:
                if(Chance(arg1))
                    continue;
                break;
            case LT:
                if(Flag[arg1] < arg2)
                    continue;
                break;
            case GT:
                if(Flag[arg1] > arg2)
                    continue;
                break;
            case EQ:
                if(Flag[arg1] == arg2)
                    continue;
                break;
            case NE:
                if(Flag[arg1] != arg2)
                    continue;
                break;
            case OBJECTAT:
                if(ObjectLoc[arg1] == arg2)
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
        return;
    } while(1);

    ActionsExecuted = 1;

    do {
        unsigned char op = *p;
        if(!(op & 0x80))
            break;

#ifdef DEBUG
        if(op & 0x40)
            fprintf(stderr, "DONE:");
        if (CurrentGame == QUESTPROBE3)
            fprintf(stderr,"%s(%d) ", Action[Q3Action[op & 0x3F]], op & 0x3F);
        else
            fprintf(stderr,"%s(%d) ", Action[op & 0x3F], op & 0x3F);
#endif

        p++;
        if(op & 0x40)
            *done = 1;
        op &= 0x3F;

        if(op > 8) {
            arg1 = *p++;
#ifdef DEBUG
            unsigned char debugarg1 = arg1;
            if (CurrentGame == QUESTPROBE3)
                AdjustQuestprobeActions(Q3Action[op], &debugarg1, NULL);
            fprintf(stderr, "%d ", debugarg1);
#endif
        }
        if(op >= TwoActionParameters()) {
            arg2 = *p++;
#ifdef DEBUG
            unsigned char debugarg2 = arg2;
            if (CurrentGame == QUESTPROBE3)
                AdjustQuestprobeActions(Q3Action[op], NULL, &debugarg2);
            fprintf(stderr, "%d ", debugarg2);
#endif
        }

        if (CurrentGame == QUESTPROBE3) {
            op = Q3Action[op];
            AdjustQuestprobeActions(op, &arg1, &arg2);
        }

        int WasDark = Flag[DarkFlag()];

        switch(op) {
            case LOADPROMPT:
                if (LoadPrompt()) {
                    *done = 1;
                    return;
                }
                break;
            case QUIT:
                SaveUndo();
                QuitGame();
                return;
            case SHOWINVENTORY:
                Inventory();
                break;
            case ANYKEY:
                AnyKey();
                break;
            case SAVE:
                stop_time = 1;
                SaveGame();
                break;
            case DROPALL:
                DropAll();
                break;
            case LOOK:
                Look();
                break;
            case PRINTOK:
                /* Guess */
                SysMessage(OKAY);
                OutFlush();
                break;
            case GET:
                if (GetObject(arg1) == 0 && CurrentGame == QUESTPROBE3)
                    *done = 1;
                break;
            case DROP:
                DropObject(arg1);
                break;
            case GOTO:
                /*
                 He-Man moves the the player to a special "By the power of Grayskull" room
                 and then issues an undo to return to the previous room
                 */
                if (CurrentGame == HEMAN && arg1 == 83)
                    SaveUndo();
                Goto(arg1);
                break;
            case GOBY:
                /* Blizzard pass era */
                if(Version == BLIZZARD_PASS_TYPE)
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
                Message(arg1);
                if (CurrentGame == BLIZZARD_PASS)
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
                Flag[arg1] = arg2;
                break;
            case ADD:
                n = Flag[arg1] + arg2;
                if(n > 255)
                    n = 255;
                Flag[arg1] = n;
                break;
            case SUB:
                n = Flag[arg1] - arg2;
                if(n < 0)
                    n = 0;
                Flag[arg1] = n;
                break;
            case PUT:
                Put(arg1, arg2);
                break;
            case SWAP:
                n = ObjectLoc[arg1];
                Put(arg1, ObjectLoc[arg2]);
                Put(arg2, n);
                break;
            case SWAPF:
                n = Flag[arg1];
                Flag[arg1] = Flag[arg2];
                Flag[arg2] = n;
                break;
            case MEANS:
                Means(arg1, arg2);
                break;
            case PUTWITH:
                Put(arg1, ObjectLoc[arg2]);
                break;
            case BEEP:
#ifdef SPATTERLIGHT
                win_beep(1);
#else
                putchar('\007');
                fflush(stdout);
#endif
                break;
            case REFRESH:
                if (CurrentGame == KAYLETH)
                    TakeAll(78);
                if (CurrentGame == HEMAN)
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
                snprintf(buf, 5, "%04d", TurnsLow + TurnsHigh * 100);
                while(*q)
                    OutChar(*q++);
                SysMessage(14);
                if (IsThing)
                /* THING is always 100 percent rested */
                    OutString("100");
                else {
                    /* Calculate "restedness" percentage */
                    /* Flag[7] == 80 means 100 percent rested */
                    q = buf;
                    snprintf(buf, 4, "%d", (Flag[7] >> 2) + Flag[7]);
                    while(*q)
                        OutChar(*q++);
                }
                SysMessage(15);
                break;
            case SWITCHINVENTORY:
            {
                uint8_t temp = Flag[2]; /* Switch inventory */
                Flag[2] = OtherGuyInv;
                OtherGuyInv = temp;
                temp = Flag[42]; /* Switch dark flag */
                Flag[42] = Flag[43];
                Flag[43] = temp;
                Redraw = 1;
                break;
            }
            case SWITCHCHARACTER:
                /* Go to the location of the other guy */
                Flag[0] = ObjectLoc[arg1];
                /* Pick him up, so that you don't see yourself */
                GetObject(arg1);
                Redraw = 1;
                break;
            case CONTINUE:
                *done = 0;
                break;
            case IMAGE:
                if (MyLoc == 3 || Flag[DarkFlag()]) {
                    DrawBlack();
                    break;
                }
                if (arg1 == 0) {
                    ClearGraphMem();
                    DrawSagaPictureNumber(MyLoc - 1);
                } else if (arg1 == 45 && ObjectLoc[48] != MyLoc) {
                    break;
                } else {
                    DrawSagaPictureNumber(arg1 - 1);
                }
                DrawSagaPictureFromBuffer();
                break;
            default:
                fprintf(stderr, "Unknown command %d.\n", op);
                break;
        }
        if (WasDark != Flag[DarkFlag()])
            Redraw = 1;
    }
    while(1);
#ifdef DEBUG
    fprintf(stderr, "\n");
#endif
}

static unsigned char *NextLine(unsigned char *p)
{
    unsigned char op;
    while(!((op = *p) & 0x80)) {
        p+=2;
        if(op >= TwoConditionParameters())
            p++;
    }
    while(((op = *p) & 0x80)) {
        op &= 0x3F;
        p++;
        if (op > 8)
            p++;
        if(op >= TwoActionParameters())
            p++;
    }
    return p;
}

static size_t FindStatusTable(void)
{
    size_t pos = 0;
    while((pos = FindCode("\x3E\xFF\x32", pos+1, 3)) != -1) {
        if(FileImage[pos + 5] != 0x18)
            continue;
        if(FileImage[pos + 6] != 0x07)
            continue;
        if(FileImage[pos + 7] != 0x21)
            continue;
        return (FileImage[pos-2] + (FileImage[pos-1] << 8)) - 0x4000 + FileBaselineOffset;
    }

    /* Questprobe */
    pos = FindCode("\x7E\x7E\x01\x02\x0C\x30\x0B\x17\x10\x16\x07\x05", 0, 12);
    if (pos == -1) {
        fprintf(stderr, "Unable to find automatics.\n");
        glk_exit();
    }
    return pos;
}

static void DrawExtraQP3Images(void) {
    if (MyLoc == 34 && ObjectLoc[29] == 34) {
        DrawSagaPictureNumber(46);
        buffer[32 * 8 + 25][8] &= 191;
        buffer[32 * 9 + 25][8] &= 191;
        buffer[32 * 9 + 26][8] &= 191;
        buffer[32 * 9 + 27][8] &= 191;
        DrawSagaPictureFromBuffer();
    } else if (MyLoc == 2 && ObjectLoc[17] == 2 && Flag[26] > 16 && Flag[26] < 20) {
        DrawSagaPictureNumber(53);
        DrawSagaPictureFromBuffer();
    }
}

static void RunStatusTable(void)
{
    if (stop_time) {
        stop_time--;
        return;
    }
    unsigned char *p = FileImage + StatusBase;

    int done = 0;
    ActionsExecuted = 0;

    if (CurrentGame == QUESTPROBE3) {
        UpdateQ3Flags();
    }

    while(*p != 0x7F) {
        while (CurrentGame == QUESTPROBE3 && *p == 0x7e) {
            p++;
        }
        ExecuteLineCode(p, &done);
        if(done) {
            return;
        }
        p = NextLine(p);
    }
    if (CurrentGame == QUESTPROBE3)
        DrawImages = 0;
}

size_t FindCommandTable(void)
{
    size_t pos = 0;
    while((pos = FindCode("\x3E\xFF\x32", pos+1, 3)) != -1) {
        if(FileImage[pos + 5] != 0x18)
            continue;
        if(FileImage[pos + 6] != 0x07)
            continue;
        if(FileImage[pos + 7] != 0x21)
            continue;
        return (FileImage[pos+8] + (FileImage[pos+9] << 8)) - 0x4000 + FileBaselineOffset;
    }

    /* Questprobe */
    pos = FindCode("\x19\x10\x01\x06\x8B\x02\x8E\x1B\x91\x12\xD0\x11", 0, 12);

    if (pos == -1) {
        fprintf(stderr, "Unable to find commands.\n");
        glk_exit();
    }
    return pos;
}

static void RunCommandTable(void)
{
    unsigned char *p = FileImage + ActionBase;

    int done = 0;
    ActionsExecuted = 0;

    while(*p != 0x7F) {
        if(((*p == 126 || *p == Word[0]) &&
           (p[1] == 126 || p[1] == Word[1])) ||
           ((*p == 126 || *p == Word[1]) &&
           (p[1] == 126 || p[1] == Word[0]))
           ) {
#ifdef DEBUG
            PrintWord(p[0]);
            PrintWord(p[1]);
#endif
            ExecuteLineCode(p + 2, &done);
            if(done)
                return;
        }
        p = NextLine(p + 2);
    }
}

static int AutoExit(unsigned char v)
{
    unsigned char *p = FileImage + ExitBase;
    unsigned char want = MyLoc | 0x80;
    while(*p != want) {
        if(*p == 0xFE)
            return 0;
        p++;
    }
    p++;
    while(*p < 0x80) {
        if(*p == v) {
            Goto(p[1]);
            return 1;
        }
        p+=2;
    }
    return 0;
}

static int LastNoun = 0;

static int IsDir(unsigned char word)
{
    if (word == 0)
        return 0;
    if (CurrentGame == QUESTPROBE3) {
        return (word <= 4 || word == 57 || word == 60);
    }
    else return (word <= 10);
}

static void RunOneInput(void)
{
    if(Word[0] == 0 && Word[1] == 0) {
        if (TryExtraCommand() == 0) {
            OutCaps();
            SysMessage(I_DONT_UNDERSTAND);
            stop_time = 2;
        } else {
            if (Redraw)
                Look();
        }
        return;
    }
    if (IsDir(Word[0])) {
        if(AutoExit(Word[0])) {
            stop_time = 0;
            RunStatusTable();
            if(Redraw)
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

    if(ActionsExecuted == 0) {
        if (TryExtraCommand() == 0) {
            if(IsDir(Word[0]))
                SysMessage(YOU_CANT_GO_THAT_WAY);
            else
                SysMessage(THATS_BEYOND_MY_POWER);
            OutFlush();
            stop_time = 1;
            return;
        } else {
            return;
        }
    }
    if(Redraw) {
        Look();
    }

    do {
        if (CurrentGame == QUESTPROBE3) {
            DrawImages = 0;
            RunStatusTable();
            DrawImages = 255;
        }
        RunStatusTable();

        if (CurrentGame == QUESTPROBE3) {
            DrawExtraQP3Images();
        }

        if(Redraw) {
            Look();
        }
        if (WaitFlag() != -1 && Flag[WaitFlag()]) {
            if (LastChar != '\n')
                OutChar('\n');
        }
    } while (WaitFlag() != -1 && Flag[WaitFlag()]-- > 0);
    if (AnimationRunning)
        glk_request_timer_events(AnimationRunning);
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
}

static const char *Abbreviations[] = { "I   ", "L   ", "X   ", "Z   ", "Q   ", "Y   ", NULL };
static const char *AbbreviationValue[] = { "INVE", "LOOK", "EXAM", "WAIT", "QUIT", "YES ", NULL };

static int ParseWord(char *p)
{
    char buf[5];
    size_t len = strlen(p);
    unsigned char *words = FileImage + VerbBase;
    int i;

    if(len >= 4) {
        memcpy(buf, p, 4);
        buf[4] = 0;
    } else {
        memcpy(buf, p, len);
        memset(buf + len, ' ', 4 - len);
    }
    for(i = 0; i < 4; i++) {
        if(buf[i] == 0)
            break;
        if(islower(buf[i]))
            buf[i] = toupper(buf[i]);
    }
    while(*words != 126) {
        if(memcmp(words, buf, 4) == 0)
            return words[4];
        words+=5;
    }

    words = FileImage + VerbBase;
    for (i = 0; Abbreviations[i] != NULL; i++) {
        if(memcmp(Abbreviations[i], buf, 4) == 0) {
            while(*words != 126) {
                if(memcmp(words, AbbreviationValue[i], 4) == 0)
                    return words[4];
                words+=5;
            }
        }
    }
    return 0;
}

static void SimpleParser(void)
{
    int nw;
    int i;
    int wn = 0;
    char buf[256];
    do {
        for (i = 0; i < 5; i++)
            wb[i][0] = '\0';
        if (LastChar != '\n')
            OutChar('\n');
        OutFlush();
        if (CurrentGame == QUESTPROBE3) {
            if (MyLoc != 6) {
                if (IsThing == 0)
                    SysMessage(8);
                else
                    SysMessage(9);
            }
            OutCaps();
            SysMessage(10);
        } else
            OutString("> ");
        OutFlush();
        PendSpace = 0;
        LastChar = 0;
        do
        {
            LineInput(buf, 255);
            nw = sscanf(buf, "%16s %16s %16s %16s %16s", wb[0], wb[1], wb[2], wb[3], wb[4]);
        } while(nw == 0);

        if (wb[0][0] == 0)
            OutString("Huh?");
    } while (wb[0][0] == 0);

    for(i = 0; i < nw ; i++)
    {
        Word[wn] = ParseWord(wb[i]);
        if(Word[wn])
            wn++;
    }
    for(i = wn; i < 5; i++)
        Word[i] = 0;
}

static void FindTables(void)
{
    TokenBase = FindTokens();
    RoomBase = FindRooms();
    ObjectBase = FindObjects();
    StatusBase = FindStatusTable();
    ActionBase = FindCommandTable();
    ExitBase = FindExits();
    FlagBase = FindFlags();
    ObjLocBase = FindObjectLocations();
    MessageBase = FindMessages();
    Message2Base = FindMessages2();
}

/*
 *	Version 0 is different
 */

static int GuessLowObjectEnd0(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *t = NULL;
    unsigned char c = 0, lc;
    int n = 0;

    while(p < EndOfData) {
        if(t == NULL)
            t = TokenText(*p++);
        lc = c;
        c = *t & 0x7F;
        if(c == 0x5E || c == 0x7E) {
            if(lc == ',' && n > 20)
                return n;
            n++;
        }
        if(*t++ & 0x80)
            t = NULL;
    }
    return -1;
}


static int GuessLowObjectEnd(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *x;
    int n = 0;

    /* Can't automatically guess in this case */
    if (CurrentGame == BLIZZARD_PASS)
        return 70;
    else if (CurrentGame == QUESTPROBE3)
        return 49;

    if (Version == REBEL_PLANET_TYPE)
        return GuessLowObjectEnd0();

    while(n < NumObjects()) {
        while(*p != 0x7E && *p != 0x5E) {
            p++;
        }
        x = TokenText(p[-1]);
        while(!(*x & 0x80)) {
            x++;
        }
        if((*x & 0x7F) == ',')
            return n;
        n++;
        p++;
    }
    fprintf(stderr, "Unable to guess the last description object.\n");
    return 0;
}




static void RestartGame(void)
{
    RestoreState(initial_state);
    just_started = 0;
    stop_time = 0;
    OutFlush();
    glk_window_clear(Bottom);
    Look();
    RunStatusTable();
    should_restart = 0;
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
                    ///                case 'd':
                    //                    Options |= DEBUGGING;
                    //                    break;
                    //                case 'w':
                    //                    split_screen = 0;
                    //                    break;
                    //                case 'n':
                    //                    Options |= NO_DELAYS;
                    //                    break;
            }
            argv++;
            argc--;
        }

//    argv[1] = "/Users/administrator/Desktop/blizzardpass.sna";

    if(argv[1] == NULL)
    {
        fprintf(stderr, "%s: <file>.\n", argv[0]);
        glk_exit();
    }

    size_t namelen = strlen(argv[1]);
    Filename = MemAlloc((int)namelen);
    strncpy(Filename, argv[1], namelen);
    Filename[namelen] = '\0';

    FILE *f = fopen(Filename, "r");
    if(f == NULL)
    {
        perror(Filename);
        glk_exit();
    }

    fseek(f, 0, SEEK_END);
    FileImageLen = ftell(f);
    if (FileImageLen == -1) {
        fclose(f);
        glk_exit();
    }

    FileImage = MemAlloc((int)FileImageLen);

    fseek(f, 0, SEEK_SET);
    if (fread(FileImage, 1, FileImageLen, f) != FileImageLen) {
        fprintf(stderr, "File read error!\n");
    }

    FileImage = ProcessFile(FileImage, &FileImageLen);

    EndOfData = FileImage + FileImageLen;

    fclose(f);
    return 1;
}

void PrintConditionAddresses(void) {
    fprintf(stderr, "Memory adresses of conditions\n\n");
    uint16_t conditionsOffsets = 0x56A8 + FileBaselineOffset;
    uint8_t *conditions;
    conditions = &FileImage[conditionsOffsets];
    for (int i = 1; i < 20; i++) {
        uint16_t address = *conditions++;
        address += *conditions * 256;
        conditions++;
        fprintf(stderr, "Condition %02d: 0x%04x (%s)\n", i, address, Condition[Q3Condition[i]]);
    }
    fprintf(stderr, "\n");
}

void PrintActionAddresses(void) {
    fprintf(stderr, "Memory adresses of actions\n\n");
    uint16_t actionOffsets = 0x591C + FileBaselineOffset;
    uint8_t *actions;
    actions = &FileImage[actionOffsets];
    for (int i = 1; i < 24; i++) {
        uint16_t address = *actions++;
        address += *actions * 256;
        actions++;
        fprintf(stderr, "   Action %02d: 0x%04x (%s)\n", i, address, Action[Q3Action[i]]);
    }
    fprintf(stderr, "\n");
}

struct GameInfo *DetectGame(size_t LocalVerbBase)
{
    struct GameInfo *LocalGame;

    for (int i = 0; i < NUMGAMES; i++) {
        LocalGame = &games[i];
        FileBaselineOffset = (long)LocalVerbBase - (long)LocalGame->start_of_dictionary;
        long diff = FindTokens() - LocalVerbBase;
        if ((LocalGame->start_of_tokens - LocalGame->start_of_dictionary) == diff) {
            fprintf(stderr, "This is %s\n", LocalGame->Title);
            return LocalGame;
        } else {
            fprintf(stderr, "Diff for game %s: %d. Looking for %ld\n", LocalGame->Title, LocalGame->start_of_tokens - LocalGame->start_of_dictionary, diff);
        }
    }
    return NULL;
}

void UnparkFileImage(uint8_t *ParkedFile, size_t ParkedLength, long ParkedOffset, int FreeCompanion)
{
    FileImage = ParkedFile;
    FileImageLen = ParkedLength;
    FileBaselineOffset = ParkedOffset;
    if (FreeCompanion)
        free(CompanionFile);
}

void LookForSecondTOTGame(void)
{
    size_t namelen = strlen(Filename);
    char *secondfile = MemAlloc((int)namelen);
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

    FILE *f = fopen(secondfile, "r");
    if (f == NULL)
        return;

    fseek(f, 0, SEEK_END);
    size_t filelength = ftell(f);
    if (filelength == -1) {
        return;
    }

    CompanionFile = MemAlloc((int)filelength);

    fseek(f, 0, SEEK_SET);
    if (fread(CompanionFile, 1, filelength, f) != filelength) {
        fprintf(stderr, "File read error!\n");
    }

    fclose(f);

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

    struct GameInfo *AltGame = DetectGame(AltVerbBase);

    if ((CurrentGame == TOT_TEXT_ONLY && AltGame->gameID != TEMPLE_OF_TERROR) ||
        (CurrentGame == TEMPLE_OF_TERROR && AltGame->gameID != TOT_TEXT_ONLY)) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    Display(Bottom, "Found files for both the text-only version and the graphics version of Temple of Terror.\n"
            "Would you like to use the longer texts from the text-only version along with the graphics from the other file? (Y/N) ");
    if (!YesOrNo()) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    if (CurrentGame == TOT_TEXT_ONLY) {
        struct GameInfo *tempgame = AltGame;
        AltGame = Game;
        Game = tempgame;
        SagaSetup();
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
    } else {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
        SagaSetup();
        Game = AltGame;
        FileImage = CompanionFile;
        FileImageLen = filelength;
        VerbBase = AltVerbBase;
    }

    EndOfData = FileImage + FileImageLen;
}

void glk_main(void)
{
    /* The message analyser will look for version 0 games */

    fprintf(stderr, "Loaded %zu bytes.\n", FileImageLen);

    VerbBase = FindCode("NORT\001N", 0, 6);
    if(VerbBase == -1) {
        fprintf(stderr, "No verb table!\n");
        glk_exit();
    }

    Game = DetectGame(VerbBase);
    if (Game == NULL) {
        fprintf(stderr, "Did not recognize game!\n");
        glk_exit();
    }

    fprintf(stderr, "FileBaselineOffset: %ld\n", FileBaselineOffset);

    DisplayInit();

    if (CurrentGame == TEMPLE_OF_TERROR || CurrentGame == TOT_TEXT_ONLY) {
        LookForSecondTOTGame();
    }

    SagaSetup();

    if (CurrentGame == QUESTPROBE3)
        DelimiterChar = '=';

    FindTables();
    NumLowObjects = GuessLowObjectEnd();
#ifdef DEBUG
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
#endif
    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    initial_state = SaveCurrentState();

    RunStatusTable();
    if(Redraw) {
        OutFlush();
        Look();
    }
    while(1) {
        if (should_restart) {
            RestartGame();
        } else if (!stop_time)
            SaveUndo();
        SimpleParser();
        FirstAfterInput = 1;
        RunOneInput();
        if (stop_time)
            stop_time--;
        else
            just_started = 0;
    }
}