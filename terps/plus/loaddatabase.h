//
//  loaddatabase.h
//  Plus
//
//  Created by Administrator on 2022-06-04.
//

#ifndef loaddatabase_h
#define loaddatabase_h

#include <stdio.h>

#include "definitions.h"

int LoadDatabase(FILE *f, int loud);
void PrintDictWord(int idx, DictWord *dict);

#endif /* loaddatabase_h */
