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
#include "utility.h"
#include "sagadraw.h"

#include "taylor.h"

uint8_t Flag[128];
uint8_t ObjectLoc[256];
static uint8_t Word[5];

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

static int ActionsDone;
static int ActionsExecuted;
static int Redraw;

//#define Redraw (Flag[52])

static int DarkFlag = 43;

static int FirstAfterInput = 0;

extern struct SavedState *initial_state;

int stop_time = 0;
int just_started = 1;
int should_restart = 0;

long FileBaselineOffset = 0;

struct GameInfo *Game = NULL;

extern struct GameInfo games[];

#ifdef DEBUG

/*
 *	Debugging
 */
static unsigned char WordMap[256][5];

static char *Condition[]={
    "<ERROR>", //0
    "AT", //1
    "NOTAT", //2
    "ATGT", //3
    "ATLT", //4
    "PRESENT", //5
    "HERE", //6
    "ABSENT", //7
    "NOTHERE", //8
    "CARRIED", //9
    "NOTCARRIED", //10
    "WORN", //11
    "NOTWORN", //12
    "NODESTROYED", //13
    "DESTROYED", //14
    "ZERO", //15
    "NOTZERO", //16
    "WORD1", //17
    "WORD2", //18
    "WORD3", //19
    "CHANCE", //20
    "LT", //21
    "GT", //22
    "EQ", //23
    "NE", //24
    "OBJECTAT", //25
    "COND26",
    "COND27",
    "COND28",
    "COND29",
    "COND30",
    "COND31",
};

static int Q3Condition[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    7,
    9,
    10,
    13,
    14,
    15,
    16,
    17,
    18,
    20,
    21,
    22,
    23,
    25,
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
    "SWITCH",
    "DONE",
    "ACT40",
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

static int Q3Action[]={
    0,
    37, // swap TORCH <-> THING
    36, // report status
    1,
    2,
    3,
    4,
    5,
    39, // set flag 118 to 1?
    9,
    10,
    11,
    38, // swap TORCH <-> THING
    13,
    14,
    15,
    16,
    17,
    22,
    23,
    24,
    25,
    26,
    31, // Redraw room image
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
        fprintf(stderr, "*    ");
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

size_t FindCode(const char *x, size_t base, size_t len)
{
    unsigned char *p = FileImage + base;
    while(p < FileImage + FileImageLen - len) {
        if(memcmp(p, x, len) == 0)
            return p - FileImage;
        p++;
    }
    return -1;
}

static size_t FindFlags(void)
{
    /* Look for the flag initial block copy */
    size_t pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
    if(pos == -1) {
        fprintf(stderr, "Cannot find initial flag data.\n");
        return 0x5b71 - 0x3fe5;
//        glk_exit();
    }
    return pos + 6;
}

static size_t FindObjectLocations(void)
{
    size_t pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
    if(pos == -1) {
        fprintf(stderr, "Cannot find initial object data.\n");
        return 0x50be;
//        glk_exit();
    }
    pos = FileImage[pos - 16] + (FileImage[pos - 15] << 8);
    return pos - 0x4000 + FileBaselineOffset;
}

static size_t FindExits(void)
{
    size_t pos = 0;

    return 0x90d4 - 0x3fe5;

    while((pos = FindCode("\x1A\xBE\x28\x0B\x13", pos+1, 5)) != -1)
    {
        pos = FileImage[pos - 5] + (FileImage[pos - 4] << 8);
        pos -= 0x4000 + FileBaselineOffset;
        fprintf(stderr, "found exits at 0x%04zx\n", pos);
        return pos;
    }
    fprintf(stderr, "Cannot find initial flag data.\n");
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

//static void TokenClassify(size_t pos)
//{
//    unsigned char *p = FileImage + pos;
//    int n = 0;
//    while(n++ < 256) {
//        do {
//            if(*p == 0x5E || *p == 0x7E)
//                Version = 0;
//        } while(!(*p++ & 0x80));
//    }
//}

static size_t FindTokens(void)
{

    fprintf(stderr, "Found tokens at 0x4ca6.\n");
    //                glk_exit();
    print_memory2(0x4ca6, 16);
    return 0x4ca6;


    size_t addr;
    size_t pos = 0;
    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if(pos == -1) {
            /* Last resort */
            addr = FindCode("You are in ", 0, 11) - 1;
            if(addr == -1) {
                fprintf(stderr, "Unable to find token table.\n");
//                glk_exit();
                return 0x8c8b - 0x3fe5;
            }
            return addr;
        }
        addr = (FileImage[pos-1] <<8 | FileImage[pos-2]) - 0x4000 + FileBaselineOffset;
    } while(LooksLikeTokens(addr) == 0);
//    TokenClassify(addr);
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

static void OutChar(char c)
{
    fprintf(stderr, "%c", c);
    if(c == ']')
        c = '\n';

    int SetUpper = 0;

    if (c == '.') {
        periods++;
        if (periods == 3) {
            PrintCharacter('.');
            PrintCharacter('.');
            LastChar = 0;
            PendSpace = 0;
            periods = 0;
            Upper = 0;
        } else if (LastChar == '?' || FirstAfterInput || isspace(LastChar) || LastChar == '.') {
            c = ' ';
            if (LastChar == ' ')
                LastChar = 0;
        }
        PendSpace = 0;
    } else {
        periods = 0;
    }

    if(c == ' ') {
        PendSpace = 1;
        if (LastChar == '.')
            SetUpper = 1;
        return;
    }
    if (FirstAfterInput) {
        FirstAfterInput = 0;
        PendSpace = 0;
    }
    if(LastChar) {
        if (isspace(LastChar))
            PendSpace = 0;
        OutWrite(LastChar);
    }
    if(PendSpace) {
        OutWrite(' ');
        PendSpace = 0;
    }
    LastChar = c;
    if (LastChar == '\n')
        SetUpper = 1;
    if (SetUpper)
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

void QPrintChar(uint8_t c) { // Print character
    if (c == 0x0d)
        return;
    if (Upper && c >= 'a') {
        c -= 0x20; // token is made uppercase
    }
    OutChar(c);
    if (c > '!') {
        Upper = 0;
    }
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        Upper = 1;
    }
}

static void PrintToken(unsigned char n)
{
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
    fprintf(stderr, "Unable to locate messages.\n");
    return 0x6000 - 0x3fe5;
//    glk_exit();
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
    if (CurrentGame == QUESTPROBE3)
        OutChar(' ');
}

static void Message2(unsigned int m)
{
    unsigned char *p = FileImage + Message2Base;
    PrintText(p, m);
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
    fprintf(stderr, "Unable to locate objects.\n");
    return 0x6c7a - 0x3fe5;
//    glk_exit();
}

static void PrintObject(unsigned char obj)
{
    unsigned char *p = FileImage + ObjectBase - 1;
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
    fprintf(stderr, "Unable to locate rooms.\n");
    return 0x68a5 - 0x3fe5;
//    glk_exit();
}


static void PrintRoom(unsigned char room)
{
    unsigned char *p = FileImage + RoomBase;
    if (CurrentGame == BLIZZARD_PASS && room < 102)
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

static unsigned char MaxCarry()
{
    if (CurrentGame == QUESTPROBE3)
        return 5;
    return Flag[4];
}

static int CarryItem(void)
{
    if(Flag[5] == MaxCarry())
        return 0;
    if(Flag[5] < 255)
        Flag[5]++;
    return 1;
}

static void DropItem(void)
{
    if(Flag[5] > 0)
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
    if(v == MyLoc|| v == Worn() || v == Carried() || (CurrentGame == QUESTPROBE3 && v == Flag[3] && Flag[1] == MyLoc))
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

static void NewGame(void)
{
    Redraw = 1;
    memset(Flag, 0, 128);
    memcpy(Flag + 1, FileImage + FlagBase, 127);
    for (int i = 0; i < 128; i++) {
        fprintf(stderr, "Flag %d is initially set to %d\n", i, Flag[i]);
    }
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
//    fprintf(stderr, "NewGame: NumObjects: %d\n", NumObjects());
}

void Look(void);

static int GetFileLength(strid_t stream) {
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

static int LoadGame(void)
{
    char c;
    OutCaps();
    glk_window_clear(Bottom);
    SysMessage(RESUME_A_SAVED_GAME);
    OutChar(' ');
    OutFlush();

    do {
        c = WaitCharacter();
        if(c == 'n' || c == 'N') {
            glk_window_clear(Bottom);
            return 0;
        }
        if(c == 'y' || c == 'Y') {
            OutString("Y\n");
            OutFlush();

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
                free(state);
            }
            glk_stream_close (stream, NULL);
            glk_fileref_destroy (fileref);
            Redraw = 1;
            return 1;
        }
    }
    while(1);
}

static void QuitGame(void)
{
    char c;
    OutCaps();
    SysMessage(PLAY_AGAIN);
    OutChar(' ');
    OutFlush();
    do {
        c = WaitCharacter();
        if(c == 'n' || c == 'N') {
            OutString("N \n");
            exit(0);
        }
        if(c == 'y' || c == 'Y') {
            OutString("Y \n");
            NewGame();
            return;
        }
    }
    while(1);
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
                SysMessage(WORN);
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
            OutChar('.');
        } else {
            OutReplace('.');
        }
    }
    OutFlush();
}

static void  AnyKey(void) {
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

static void SaveGame(void) {

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

static void DropAll(void) {
    int i;
    for(i = 0; i < NumObjects(); i++) {
        if(ObjectLoc[i] == Carried() || ObjectLoc[i] == Worn())
            Put(i, MyLoc);
    }
    Flag[5] = 0;
}

static void GetObject(unsigned char obj) {
    if(ObjectLoc[obj] == Carried() || ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_HAVE_IT);
        return;
    }
    if (!(CurrentGame == QUESTPROBE3 && Flag[1] == MyLoc && ObjectLoc[obj] == Flag[3])) {
        if(ObjectLoc[obj] != MyLoc) {
            SysMessage(YOU_DONT_SEE_IT);
            return;
        }
    }
    if(CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return;
    }
    SysMessage(OK);
    OutChar(' ');
    OutFlush();
    Put(obj, Carried());
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
    SysMessage(OK);
    OutChar(' ');
    OutFlush();
    DropItem();
    Put(obj, MyLoc);
}

void Look(void) {
    if (MyLoc == 0 || (CurrentGame == KAYLETH && MyLoc == 91))
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();
    int i;
    int f = 0;
    unsigned char locw = 0x80 | MyLoc;
    unsigned char *p;

    PendSpace = 0;
    LastChar = 0;
    OutReset();
    TopWindow();

    Redraw = 0;
    OutCaps();

    if(Flag[DarkFlag]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (CurrentGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);
    if (CurrentGame == QUESTPROBE3) {
        for(i = 0; i < NumObjects(); i++) {
            if(ObjectLoc[i] == MyLoc) {
                if(f == 0) {
                    OutReplace(0);
                    SysMessage(0);
                }
                f = 1;
                PrintObject(i);
            }
        }
        if(f == 1)
            OutReplace('.');
    }

    p = FileImage + ExitBase;

    f = 0;
    
    while(*p != locw)
        p++;
    p++;
    while(*p < 0x80) {
        if(f == 0) {
            OutCaps();
            SysMessage(EXITS);
        }
        f = 1;
        OutCaps();
        SysMessage(*p);
        p += 2;
    }
    if(f == 1)
    {
        OutReplace('.');
    }
    f = 0;

    if (CurrentGame != QUESTPROBE3) {

        for(i = 0; i < NumObjects(); i++) {
            if(ObjectLoc[i] == MyLoc) {
                if(f == 0) {
                    SysMessage(YOU_SEE);
                    if( Version == REBEL_PLANET_TYPE)
                        OutReplace(0);
                }
                f = 1;
                PrintObject(i);
            }
        }
        if(f == 1)
            OutReplace('.');
    }

    if (LastChar != '\n')
        OutChar('\n');
    OutChar('\n');

    if (MyLoc != 0) {
        glk_window_clear(Graphics);
        DrawRoomImage();
    }
    BottomWindow();
}


static void Goto(unsigned char loc) {
    Flag[0] = loc;
    Look();
}

static void Delay(unsigned char seconds) {
    OutChar(' ');
    OutFlush();
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


static void switchQ3flags1(void) {
    fprintf(stderr, "switchQ3flags1\n");
    if (Flag[31] != 0) {
        if (ObjectLoc[2] != 0xfc) {
            Flag[1] = MyLoc;
        } else {
            Flag[1] = ObjectLoc[18]; // Location of Torch
        }
    } else {
        if (ObjectLoc[1] == 0xfc) {
            Flag[1] = ObjectLoc[17]; // Location of Thing
        } else {
            Flag[1] = MyLoc;
        }
    }
}

static void switchQ3flags2(void) {
    if (Flag[52] != 0) {
        switchQ3flags1();
        return;
    }
    Flag[26]++; // Turns played % 100
    if (Flag[26] == 100) {
        Flag[27]++; // Turns / 100
        Flag[26] = 0;
    }
    Flag[47]++;
    if (Flag[47] == 0)
        Flag[47] = 0xff;
    Flag[48]++;
    if (Flag[48] == 0)
        Flag[48] = 0xff;
    switchQ3flags1();
}

static void ExecuteLineCode(unsigned char *p)
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
        fprintf(stderr, "%s %d ", Condition[Q3Condition[op]], arg1);
#endif
        if(op > 15)
        {
            arg2 = *p++;
#ifdef DEBUG
            fprintf(stderr, "%d ", arg2);
#endif
        }

        if (CurrentGame == QUESTPROBE3) {
            op = Q3Condition[op];
        }

        switch(op) {
            case 1:
                if(MyLoc == arg1)
                    continue;
                break;
            case 2:
                if(MyLoc != arg1)
                    continue;
                break;
            case 3:
                if(MyLoc > arg1)
                    continue;
                break;
            case 4:
                if(MyLoc < arg1)
                    continue;
                break;
            case 5:
                if(Present(arg1))
                    continue;
                break;
            case 6:
                if(ObjectLoc[arg1] == MyLoc)
                    continue;
                break;
            case 7:
                if(!Present(arg1))
                    continue;
                break;
            case 8:
                if(ObjectLoc[arg1] != MyLoc)
                    continue;
                break;
            case 9:
                /*FIXME : or worn ?? */
                if(ObjectLoc[arg1] == Carried() || ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case 10:
                /*FIXME : or worn ?? */
                if(ObjectLoc[arg1] != Carried() && ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case 11:
                if(ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case 12:
                if(ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case 13:
                if(ObjectLoc[arg1] != Destroyed())
                    continue;
                break;
            case 14:
                if(ObjectLoc[arg1] == Destroyed())
                    continue;
                break;
            case 15:
                if(Flag[arg1 + 4] == 0)
                    continue;
                break;
            case 16:
                if(Flag[arg1 + 4] != 0)
                    continue;
                break;
            case 17:
                if(Word[2] == arg1)
                    continue;
                break;
            case 18:
                if(Word[3] == arg1)
                    continue;
                break;
            case 19:
                if(Word[4] == arg1)
                    continue;
                break;
            case 20:
                if(Chance(arg1))
                    continue;
                break;
            case 21:
                if(Flag[arg1 + 4] < arg2)
                    continue;
                break;
            case 22:
                if(Flag[arg1 + 4] > arg2)
                    continue;
                break;
            case 23:
                if(Flag[arg1 + 4] == arg2)
                    continue;
                break;
            case 24:
                if(Flag[arg1 + 4] != arg2)
                    continue;
                break;
            case 25:
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
        fprintf(stderr,"%s(%d) ", Action[Q3Action[op & 0x3F]], op & 0x3F);
#endif

        p++;
        if(op & 0x40)
            ActionsDone = 1;
        op &= 0x3F;


        if(op > 8) {
            arg1 = *p++;
#ifdef DEBUG
            fprintf(stderr, "%d ", arg1);
#endif
        }
        if(op > 17) {
            arg2 = *p++;
#ifdef DEBUG
            fprintf(stderr, "%d ", arg2);
#endif
        }

        if (CurrentGame == QUESTPROBE3)
            op = Q3Action[op];

        switch(op) {
            case 1:
                if (LoadGame())
                    return;
                break;
            case 2:
                QuitGame();
                break;
            case 3:
                Inventory();
                break;
            case 4:
                AnyKey();
                break;
            case 5:
                SaveGame();
                break;
            case 6:
                DropAll();
                break;
            case 7:
                Look();
                break;
            case 8:
                /* Guess */
                SysMessage(OK);
                OutFlush();
                break;
            case 9:
                GetObject(arg1);
                break;
            case 10:
                DropObject(arg1);
                break;
            case 11:
                Goto(arg1);
                Redraw = 1;
                break;
            case 12:
                /* Blizzard pass era */
                if(Version == BLIZZARD_PASS_TYPE)
                    Goto(ObjectLoc[arg1]);
                else
                    Message2(arg1);
                break;
            case 13:
                Flag[arg1 + 4] = 255;
                if (arg1 + 4 == DarkFlag)
                    Look();
                break;
            case 14:
                Flag[arg1 + 4] = 0;
                if (arg1 + 4 == DarkFlag)
                    Look();
                break;
            case 15:
                Message(arg1);
                break;
            case 16:
                Put(arg1, MyLoc);
                break;
            case 17:
                Put(arg1, Destroyed());
                break;
            case 18:
                PrintNumber(Flag[arg1]);
                break;
            case 19:
                Delay(arg1);
                break;
            case 20:
                Wear(arg1);
                break;
            case 21:
                Remove(arg1);
                break;
            case 22:
                Flag[arg1 + 4] = arg2;
                break;
            case 23:
                n = Flag[arg1 + 4] + arg2;
                if(n > 255)
                    n = 255;
                Flag[arg1 + 4] = n;
                break;
            case 24:
                n = Flag[arg1 + 4] - arg2;
                if(n < 0)
                    n = 0;
                Flag[arg1 + 4] = n;
                break;
            case 25:
                Put(arg1, arg2);
                break;
            case 26:
                n = ObjectLoc[arg1];
                Put(arg1, ObjectLoc[arg2]);
                Put(arg2, n);
                break;
            case 27:
                n = Flag[arg1 + 4];
                Flag[arg1 + 4] = Flag[arg2 + 4];
                Flag[arg2 + 4] = n;
                break;
            case 28:
                Means(arg1, arg2);
                break;
            case 29:
                Put(arg1, ObjectLoc[arg2]);
                break;
            case 30:
                /* Beep */
#ifdef SPATTERLIGHT
                win_beep(1);
#else
                putchar('\007');
                fflush(stdout);
#endif
                break;
            case 31:
                DrawRoomImage();
                if (arg1 > 0) {
                    arg1--;
                    DrawSagaPictureNumber(arg1);
                    DrawSagaPictureFromBuffer();
                }
                break;
            case 32:
                RamSave(1);
                break;
            case 33:
                RamLoad();
                break;
            case 34:
                OutFlush();
                glk_window_clear(Bottom);
                break;
            case 35:
                RestoreUndo(0);
                Redraw = 1;
                break;
            case 36: // DIAGNOSE
                Message(223);
                char buf[5];
                char *p = buf;
                snprintf(buf, 5, "%04d", Flag[26] + Flag[27] * 100);
                while(*p)
                    OutChar(*p++);
                SysMessage(14);
                if (Flag[31])
                    OutString("100");
                else {
                   p = buf;
                    snprintf(buf, 4, "%d", (Flag[7] >> 2) + Flag[7]);
                    while(*p)
                        OutChar(*p++);
                }
                SysMessage(15);
                break;
            case 37: // SWITCHINVENTORY
            {
                uint8_t temp = Flag[2];
                Flag[2] = Flag[3];
                Flag[3] = temp;
                temp = Flag[42];
                Flag[42] = Flag[43];
                Flag[43] = temp;
                Redraw = 1;
                break;
            }
            case 38: // SWITCHCHARACTER
                MyLoc = ObjectLoc[arg1];
                GetObject(arg1);
                break;
            case 39: // DONE
                ActionsDone = 0;
                break;
            default:
                fprintf(stderr, "Unknown command %d.\n",
                        op);
                break;
        }
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
        if(op > 15)
            p++;
    }
    while(((op = *p) & 0x80)) {
        op &= 0x3F;
        p++;
        if (op > 8)
            p++;
        if (op > 17) {
            p++;
        }

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
    fprintf(stderr, "Unable to find automatics.\n");
    return 0x7e10 - 0x3fe5;
//    glk_exit();
}

uint8_t *skip_opcodes(uint8_t *p) {
    while ((*p & 0x80) != 0) {
        uint8_t val = *p;
        p += 2;
        if (val >= 0x10)
            p++;
    }
    do {
        uint8_t val = *p & 0x3f;
        p++;
        if (val >= 0x09) {
            if (val >= 0x12) {
                p++;
            }
            p++;
        }
    } while((*p & 0x80) == 0);
    if (*p == 0x7f) {
        return NULL;
    }
    if (*p != 0x7e)
        p++;
    p++;
    if (*p != 0x7e) {
        p++;
        return skip_opcodes(p);
    }
    return p++;
}

static void RunStatusTable(void)
{
    unsigned char *p = FileImage + StatusBase;

    ActionsDone = 0;
    ActionsExecuted = 0;

    Flag[52] = 1;

    switchQ3flags2();

    while(*p != 0x7F) {
        while (*p == 0x7e) {
            p++;
        }
        ExecuteLineCode(p);
        if(ActionsDone) {
            Flag[52] = 0;
            return;
        }
        p = NextLine(p);
    }
    Flag[52] = 0;
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
    fprintf(stderr, "Unable to find commands.\n");
    return 0x7045 - 0x3fe5;
//    glk_exit();
}

static void RunCommandTable(void)
{
    unsigned char *p = FileImage + ActionBase;

    ActionsDone = 0;
    ActionsExecuted = 0;

    while(*p != 0x7F) {
        if((*p == 126 || *p == Word[0]) &&
           (p[1] == 126 || p[1] == Word[1])) {
#ifdef DEBUG
            PrintWord(p[0]);
            PrintWord(p[1]);
#endif
            ExecuteLineCode(p + 2);
            if(ActionsDone)
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

static void RunOneInput(void)
{
    if(Word[0] == 0 && Word[1] == 0) {
        OutCaps();
        SysMessage(I_DONT_UNDERSTAND);
        stop_time = 2;
        return;
    }
    if(Word[0] < 11) {
        if(AutoExit(Word[0])) {
            if(Redraw)
                Look();
            RunStatusTable();
            return;
        }
    }

    /* Handle IT (At least in Temple of Terror, check other games!)*/
    if (Word[1] == 128)
        Word[1] = LastNoun;
    if (Word[1] != 0)
        LastNoun = Word[1];

    OutCaps();
    RunCommandTable();

    if(ActionsExecuted == 0) {
        if(Word[0] < 11)
            SysMessage(YOU_CANT_GO_THAT_WAY);
        else
            SysMessage(THATS_BEYOND_MY_POWER);
        stop_time = 2;
        return;
    }
    if(Redraw)
        Look();
    RunStatusTable();
    if(Redraw)
        Look();
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

    i = 0;
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

static void  SimpleParser(void)
{
    int nw;
    int i;
    int wn = 0;
    char wb[5][17];
    char buf[256];
    if (LastChar != '\n')
        OutChar('\n');
    OutFlush();
    if(CurrentGame == QUESTPROBE3) {
        if (MyLoc != 6) {
            if (Flag[31] == 0)
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

    for(i = 0; i < nw ; i++)
    {
        Word[wn] = ParseWord(wb[i]);
        if(Word[wn])
            wn++;
        //        else if (wn == 0) {
        //
        //        }
    }
    for(i = wn; i < 5; i++)
        Word[i] = 0;
}

static void FindTables(void)
{
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
        return 69;

    if (CurrentGame == QUESTPROBE3)
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
    //    if (CurrentCommand)
    //        FreeCommands();
    RestoreState(initial_state);
    just_started = 0;
    stop_time = 0;
    OutFlush();
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "w");

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
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


    //    if (argc == 2) {
    //        game_file = argv[1];
    //    }

    FILE *f;

    argv[1] = "/Users/administrator/Desktop/human.sna";

    if(argv[1] == NULL)
    {
        fprintf(stderr, "%s: <file>.\n", argv[0]);
        glk_exit();
    }

    f = fopen(argv[1], "r");
    if(f == NULL)
    {
        perror(argv[1]);
        glk_exit();
    }

    fseek(f, 0, SEEK_END);
    FileImageLen = ftell(f);
    if (FileImageLen == -1) {
        glk_exit();
    }

    FileImage = MemAlloc((int)FileImageLen);

    fseek(f, 0, SEEK_SET);
    if (fread(FileImage, 1, FileImageLen, f) != FileImageLen) {
        fprintf(stderr, "File read error!\n");
    }

//    size_t length = FileImageLen;

//    uint8_t *uncompressed = DecompressZ80(FileImage, &length);
//    if (uncompressed != NULL) {
//        free(FileImage);
//        FileImage = uncompressed;
//        FileImageLen = length;
//    }

    EndOfData = FileImage + FileImageLen;

    return 1;
}

void PrintConditionAddresses(void) {
    uint16_t conditionsOffsets = 0x56c1;
    print_memory2(conditionsOffsets, 16);
    uint8_t *conditions;
jumpHere:
    conditions = &FileImage[conditionsOffsets];
    for (int i = 0; i < 20; i++) {
        uint16_t address = *conditions++;
        address += *conditions * 256;
        conditions++;
        if (i == 1 && address != 0x95ed) {
            conditionsOffsets--;
            goto jumpHere;
        }
        fprintf(stderr, "Address of condition %d, %s: 0x%04x\n", i, Condition[Q3Condition[i]], address);
    }
    fprintf(stderr, "conditionsOffsets: 0x%04x\n", conditionsOffsets);

}

void PrintActionAddresses(void) {
    uint16_t actionOffsets = 0x991a - 0x3fe5;
    print_memory2(actionOffsets, 16);
    uint8_t *actions;
jumpHere:
    actions = &FileImage[actionOffsets];
    for (int i = 0; i < 24; i++) {
        uint16_t address = *actions++;
        address += *actions * 256;
        actions++;
        fprintf(stderr, "Address of action %d, %s: 0x%04x\n", i, Action[Q3Action[i]], address);
    }
    fprintf(stderr, "conditionsOffsets: 0x%04x\n", actionOffsets);

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

    Game = &games[0];

    if (CurrentGame == QUESTPROBE3)
        DarkFlag = 43;

    DisplayInit();

    FileBaselineOffset = (long)VerbBase - (long)Game->start_of_dictionary;

//    fprintf(stderr, "\n");
//
//    int found = 0;
//    for (int i = 0; i < FileImageLen; i++) {
//
//        uint8_t *p = FileImage + i;
//        uint8_t c = *p & 0x7F;
//            if(c >= ' ' && c <= 'z')
//                fprintf(stderr, "%c", c);
//
//
//        if (LooksLikeTokens(i)) {
//            fprintf(stderr, "0x%04x (%d) looks like tokens.\n", i, i);
//            found = 1;
//        }
//    }
//
//    fprintf(stderr, "\n");

//    if (!found)
//        fprintf(stderr, "Found nothing that looks like tokens.\n");

    TokenBase = FindTokens();

    fprintf(stderr, "Found tokens at %zx (%zu)\n", TokenBase, TokenBase);

//    if (CurrentGame == UNKNOWN_GAME) {
//        fprintf(stderr, "Unrecognized game!\n");
//        glk_exit();
//    }

    fprintf(stderr, "FileBaselineOffset: %ld\n", FileBaselineOffset);

    FindTables();
#ifdef DEBUG
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
#endif


//    for (int i = 0; i < 100; i++) {
//        fprintf(stderr, "\nRoom %d: ", i);
//        PrintRoom(i);
//        OutChar('\n');
//        OutFlush();
//        WaitCharacter();
//    }
//    for (int i = 0; i < 76; i++) {
//        fprintf(stderr, "\nObject %d: ", i);
//        PrintObject(i);
//        OutFlush();
//    }
//    for (int i = 0; i < 100; i++) {
//        fprintf(stderr, "\nMessage %d: ", i);
//        Message(i);
//        OutFlush();
//        WaitCharacter();
//    }

    PrintConditionAddresses();
    PrintActionAddresses();

    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    initial_state = SaveCurrentState();

    RamSave(0);
    Look();
    RunStatusTable();
    if(Redraw) {
        OutFlush();
        Look();
    }
    while(1) {
        if (should_restart)
            RestartGame();
        else if (!stop_time)
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
