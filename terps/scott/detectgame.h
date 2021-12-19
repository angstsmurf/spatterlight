//
//  detectgame.h
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#ifndef detectgame_h
#define detectgame_h

#include <stdio.h>
#include "definitions.h"

GameIDType detect_game(FILE *f);
int seek_if_needed(int expected_start, int *offset, uint8_t **ptr);

#endif /* detectgame_h */
