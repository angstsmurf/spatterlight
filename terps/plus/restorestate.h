//
//  restorestate.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

typedef struct SavedState {
    uint16_t Counters[64];
    uint8_t ObjectLocations[256];
    uint64_t BitFlags;
    int ProtagonistString;
    ImgType LastImgType;
    int LastImgIndex;
    struct SavedState *previousState;
    struct SavedState *nextState;
} SavedState;

extern SavedState *InitialState;

extern ImgType SavedImgType;
extern int SavedImgIndex;
extern int JustRestored;

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(int game);
void RamLoad(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
void RecoverFromBadRestore(SavedState *state);
int LoadGame(void);
void SaveGame(void);
void RestartGame(void);

#endif /* restorestate_h */
