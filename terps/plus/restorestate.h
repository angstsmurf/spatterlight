//
//  restorestate.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

#include <stdint.h>

#include "definitions.h"

typedef struct SavedState {
    int16_t Counters[NUM_COUNTERS];
    uint16_t ObjectLocations[256];
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

/* Set after a saved game was loaded from file, until the next turn */
extern int JustRestored;
/* JustRestarted is only used to adjust newlines and room descriptions in the transcript */
extern int JustRestarted;
/* JustStarted is only used for the error message "Can't undo on first move" */
extern int JustStarted;

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(int game);
void RamLoad(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
int LoadGame(void);
void SaveGame(void);
void RestartGame(void);

#endif /* restorestate_h */
