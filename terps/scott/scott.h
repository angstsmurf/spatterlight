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
  
#define LIGHT_SOURCE	9		/* Always 9 how odd */
#define CARRIED		255		/* Carried */
#define DESTROYED	0		/* Destroyed */
#define DARKBIT		15
#define LIGHTOUTBIT	16		/* Light gone out */


#include <stdio.h>
#include <stdint.h>

#include "definitions.h"

typedef struct {
 	short Unknown;
 	short NumItems;
 	short NumActions;
 	short NumWords;		/* Smaller of verb/noun is padded to same size */
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
	unsigned short Action[2];
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


#define YOUARE		1	/* You are not I am */
#define SCOTTLIGHT	2	/* Authentic Scott Adams light messages */
#define DEBUGGING	4	/* Info from database load */
#define TRS80_STYLE	8	/* Display in style used on TRS-80 */
#define PREHISTORIC_LAMP 16	/* Destroy the lamp (very old databases) */
#define SPECTRUM_STYLE 32    /* Display in style used on ZX Spectrum */

#define MAX_GAMEFILE_SIZE 200000

/* Anything used by the other source files goes here */

#include "glk.h"

#define MyLoc    (GameHeader.PlayerRoom)

#define CurrentGame    (GameInfo->gameID)

void Output(const char *a);
void OutputNumber(int a);
void Display(winid_t w, const char *fmt, ...);
void HitEnter(void);
void Look(void);
void DrawRoomImage(void);
void ListInventory(void);
void Delay(float seconds);
void DrawImage(int image);
void OpenGraphicsWindow(void);
size_t GetFileLength(FILE *in);
void *MemAlloc(int size);
int LoadDatabase(FILE *f, int loud);
void CloseGraphicsWindow(void);
void Updates(event_t ev);
int PerformExtraCommand(void);
const char *MapSynonym(int noun);
void Fatal(const char *x);
void DrawBlack(void);
uint8_t *seek_to_pos(uint8_t *buf, int offset);

extern struct GameInfo *GameInfo;
extern Header GameHeader;
extern Room *Rooms;
extern Item *Items;
extern Action *Actions;
extern const char **Verbs, **Nouns, **Messages;
extern winid_t Bottom, Top, Graphics;
extern char *sys[];
extern char *system_messages[];
extern uint8_t *entire_file;
extern size_t file_length;
extern int file_baseline_offset;
extern int dead;
extern int split_screen;
extern long BitFlags;
extern int LightRefill;
extern int Counters[];
extern int AnimationFlag;
extern int SavedRoom;


#endif /* scott_h */
