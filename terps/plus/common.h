//
//  common.h
//  Spatterlight
//
//  Created by Administrator on 2022-06-04.
//

#ifndef common_h
#define common_h

#include "glk.h"
#include "definitions.h"

void *MemAlloc(int size);
void Fatal(const char *x);
void Output(const char *string);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void PrintDictWord(int idx, DictWord *dict);
void Updates(event_t ev);
void Display(winid_t w, const char *fmt, ...)
#ifdef __GNUC__
__attribute__((__format__(__printf__, 2, 3)))
#endif
;

void SetBit(int bit);
void ResetBit(int bit);
int IsSet(int bit);

int CompareUpToHashSign(char *word1, char *word2);
int GetDictWord(int group);
int YesOrNo(void);
void OpenTopWindow(void);
void Look(int transcript);
void SystemMessage(SysMessageType message);
void AnyKey(int timeout, int message);

extern const char *sys[];

extern Synonym *Substitutions;
extern int16_t Counters[];

extern DictWord *Verbs;
extern DictWord *Nouns;
extern DictWord *Adverbs;
extern DictWord *Prepositions;
extern DictWord ExtraWords[];

extern ObjectImage *ObjectImages;
extern char **Messages;
extern Action *Actions;

extern uint64_t BitFlags;
extern int AutoInventory;

extern winid_t Bottom;

extern char *dir_path;

extern Header GameHeader;

extern Item *Items;
extern Room *Rooms;

extern int ProtagonistString;
extern int lastwasnewline;
extern int should_restart;
extern GameIDType CurrentGame;

extern ImgType LastImgType;
extern int LastImgIndex;

extern strid_t Transcript;

extern int LastVerb, LastNoun, LastPrep, LastPartp, LastNoun2, LastAdverb;

extern int showing_inventory;

#endif /* common_h */
