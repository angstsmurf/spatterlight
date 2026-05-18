//
//  restore_state.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restore_state_h
#define restore_state_h

#include <stdint.h>

typedef struct SavedState {
    int Counters[16];
    int RoomSaved[16];
    long BitFlags;
    int CurrentLoc;
    int CurrentCounter;
    int SavedRoom;
    int LightTime;
    int AutoInventory;
    uint8_t *ItemLocations;
    struct SavedState *previousState;
    struct SavedState *nextState;
} SavedState;

void SaveUndo(void);
void RestoreUndo(void);
void RamSave(void);
void RamRestore(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
void RecoverFromBadRestore(SavedState *state);

#endif /* restore_state_h */
