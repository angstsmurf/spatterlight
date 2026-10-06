//
//  saga.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//

#ifndef saga_h
#define saga_h

#include "scott_defines.h"

int DrawUSRoom(int room);
int DrawFuzzyRoom(int room);
void DrawRoomObjectImages(void);
void DrawUSRoomObject(int item);
void LookUS(void);

typedef struct {
    const char *filename;
    uint32_t stringlength;
} pairrec;

int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2);

GameIDType FreeGameResources(void);

char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t stringlen, size_t *stringlength2);

char *LookInDatabase(const pairrec list[][2], const char *game_path, size_t pathlen);

/* Swaps the game file with its companion file, when the database turned out
   to be on the companion disk */
void SwapWithCompanionFile(uint8_t **sf, size_t *extent, uint8_t **companion, size_t *companionsize);

/* Frees USImages if it holds nothing but a single empty image */
void FreeUSImagesIfEmpty(void);

GameIDType LoadBinaryDatabase(uint8_t *data, size_t length, GameInfo info, int dict_start);

uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms);

uint8_t *ParseRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms, int stride);

uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items, int stride);

uint8_t *ParseMessages(uint8_t *ptr, uint8_t *endptr, int number_of_messages);

uint8_t *ParseActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions, int big_endian);

uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof);
#endif /* saga_h */
