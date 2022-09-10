//
//  detectgame.h
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#ifndef detectgame_h
#define detectgame_h

#include "definitions.h"
#include <stdint.h>
#include <stdio.h>

GameIDType DetectGame(const char *file_name);
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr);
int TryLoading(struct GameInfo info, int dict_start, int loud);
DictionaryType GetId(size_t *offset);
int FindCode(const char *x, int base);

#endif /* detectgame_h */
