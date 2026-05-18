//
//  player.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2026-05-18.
//

#ifndef player_h
#define player_h

#include "taylor.h"

size_t FindCode(const char *code, size_t base, size_t len);
int LoadGame(void);
void Look(void);
void OutCaps(void);
void OutChar(char c);
void OutFlush(void);
void OutString(char *p);
void SaveGame(void);
void SysMessage(unsigned char m);
int YesOrNo(void);

void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
;

extern uint8_t Flag[];
extern uint8_t ObjectLoc[];
extern uint8_t *FileImage;
extern uint8_t *EndOfData;
extern size_t FileImageLen;
extern size_t VerbBase;
extern size_t AnimationData;
extern int PrintedOK;
extern int Redraw;
extern int FirstAfterInput;
extern int StopTime;
extern int JustStarted;
extern int ShouldRestart;
extern int NoGraphics;
extern long FileBaselineOffset;
extern char DelimiterChar;
extern char LastChar;
extern int PendSpace;
extern int LastVerb;
extern GameInfo *Game;
extern strid_t room_description_stream;
extern int InKaylethPreview;
extern int JustWrotePeriod;

#endif /* player_h */
