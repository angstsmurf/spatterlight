/*
 *	SCOTT FREE
 *
 *	A free Scott Adams style adventure interpreter
 *
 *	Copyright:
 *	    This software is placed under the GNU license.
 *
 *	Statement:
 *	    Everything in this program has been deduced or obtained solely
 *	from published material. No game interpreter code has been
 *	disassembled, only published BASIC sources (PC-SIG, and Byte Dec
 *	1980) have been used.
 */

/*
 *	Controlling block
 */

#ifndef scott_h
#define scott_h

#include <stdint.h>
#include <stdio.h>

#include "debugprint.h"
#include "scottdefines.h"

// clang-format off

#define LIGHT_SOURCE 9         /* Always 9 how odd */
#define CARRIED      255       /* Carried */
#define DESTROYED    0         /* Destroyed */
#define DARKBIT      15
#define LIGHTOUTBIT  16        /* Light gone out */

// clang-format on

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

typedef struct {
    short Unknown;
    short NumItems;
    short NumActions;
    short NumWords; /* Smaller of verb/noun is padded to same size */
    short NumRooms;
    short MaxCarry;
    short PlayerRoom;
    short Treasures;
    short WordLength;
    short LightTime;
    short NumMessages;
    short TreasureRoom;
} Header;

typedef struct {
    unsigned short Vocab;
    unsigned short Condition[5];
    unsigned short Subcommand[2];
} Action;

typedef struct {
    char *Text;
    short Exits[6];
    uint8_t Image;
} Room;

typedef struct {
    char *Text;
    /* PORTABILITY WARNING: THESE TWO MUST BE 8 BIT VALUES. */
    uint8_t Location;
    uint8_t InitialLoc;
    char *AutoGet;
    uint8_t Flag;
    uint8_t Image;
} Item;

typedef struct {
    short Version;
    short AdventureNumber;
    short Unknown;
} Tail;

// clang-format off

#define YOUARE                 0x1     /* You are not I am */
#define SCOTTLIGHT             0x2     /* Authentic Scott Adams light messages */
#define DEBUGGING              0x4     /* Info from database load */
#define TRS80_STYLE            0x8     /* Display in style used on TRS-80 */
#define PREHISTORIC_LAMP      0x10     /* Destroy the lamp (very old databases) */
#define SPECTRUM_STYLE        0x20     /* Display in style used on ZX Spectrum */
#define TI994A_STYLE          0x40     /* Display in style used on TI-99/4A */
#define NO_DELAYS             0x80     /* Skip all pauses */
#define FORCE_PALETTE_ZX     0x100     /* Force ZX Spectrum image palette */
#define FORCE_PALETTE_C64    0x200     /* Force CBM 64 image palette */
#define FORCE_INVENTORY      0x400     /* Inventory in upper window always on */
#define FORCE_INVENTORY_OFF  0x800     /* Inventory in upper window always off */
#define PC_STYLE            0x1000     /* Display in style used on IBM PC (MS-DOS) */
#define FLICKER_ON          0x2000     /* Flicker room description like some original games */

// clang-format on

#define MAX_GAMEFILE_SIZE 250000

/* Anything used by the other source files goes here */

#include "glk.h"

#define MyLoc (GameHeader.PlayerRoom)

#define CurrentGame (Game->gameID)

void Output(const char *a);
void OutputNumber(int a);
void Display(winid_t w, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;
void HitEnter(void);
void Look(void);
void DrawRoomImage(void);
void ListInventory(int upper);
void Delay(float seconds);
void DrawImage(int image);
void OpenGraphicsWindow(void);
size_t GetFileLength(FILE *in);
void *MemAlloc(int size);
GameIDType LoadDatabase(FILE *f, int loud);
void CloseGraphicsWindow(void);
void Updates(event_t ev);
int PerformExtraCommand(int extra_stop_time);
const char *MapSynonym(int noun);
void Fatal(const char *x);
void DrawBlack(void);
uint8_t *SeekToPos(uint8_t *buf, int offset);
int CountCarried(void);
int RandomPercent(int n);
void DoneIt(void);
void SaveGame(void);
void PrintNoun(void);
int PrintScore(void);
void MoveItemAToLocOfItemB(int itemA, int itemB);
void GoTo(int loc);
void GoToStoredLoc(void);
void SwapLocAndRoomflag(int index);
void SwapItemLocations(int itemA, int itemB);
void PutItemAInRoomB(int itemA, int roomB);
void SwapCounters(int index);
void PrintMessage(int index);
void PlayerIsDead(void);
void UpdateSettings(void);
winid_t FindGlkWindowWithRock(glui32 rock);
void OpenTopWindow(void);
void SetDark(void);
void SetLight(void);
void SetBitFlag(int bit);
void ClearBitFlag(int bit);

extern struct GameInfo *Game;
extern Header GameHeader;
extern Room *Rooms;
extern Item *Items;
extern Action *Actions;
extern const char **Verbs, **Nouns, **Messages;
extern const char *title_screen;
extern winid_t Bottom, Top, Graphics;
extern const char *sys[];
extern const char *system_messages[];
extern uint8_t *entire_file;
extern size_t file_length;
extern int file_baseline_offset;
extern int split_screen;
extern long BitFlags;
extern int LightRefill;
extern int Counters[];
extern int AnimationFlag;
extern int SavedRoom;
extern int AutoInventory;
extern int WeAreBigEndian;
extern int CurrentCounter;
extern int RoomSaved[];
extern int Options;
extern int StopTime;
extern int should_look_in_transcript;
extern int ImageWidth;
extern int ImageHeight;
extern const char *game_file;
extern int showing_inventory;
extern MachineType CurrentSys;
extern int lastwasnewline;
extern int showing_closeup;
extern int last_image_index;

#endif /* scott_h */
