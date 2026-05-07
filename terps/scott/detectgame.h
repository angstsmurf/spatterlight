//
//  detectgame.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef detectgame_h
#define detectgame_h

#include "scottdefines.h"
#include <stdint.h>

GameIDType DetectGame(const char *file_name);
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr);
GameIDType TryLoading(const GameInfo *info, int dict_start);
DictionaryType GetId(size_t *offset);
int FindCode(const char *x, int base);
uint8_t *ReadHeader(uint8_t *ptr);
int ParseHeader(int *h, HeaderType type, int *num_items, int *num_actions,
    int *num_words, int *num_rooms, int *max_carry, int *player_room,
    int *treasures, int *word_length, int *light_time, int *num_messages,
    int *treasure_room);

void PrintHeaderInfo(int *h, int num_items, int num_actions, int num_words,
    int num_rooms, int max_carry, int player_room, int treasures,
    int word_length, int light_time, int num_messages, int treasure_room);

void ParseItemSlashAutoGet(int index);
void SetGameHeader(int num_items, int num_actions, int num_words,
                   int num_rooms, int max_carry, int player_room,
                   int treasures, int word_length, int light_time,
                   int num_messages, int treasure_room);
void AllocateGameData(void);

extern int header[];

#endif /* detectgame_h */
