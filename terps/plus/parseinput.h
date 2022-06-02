//
//  parseinput.h
//  plus
//
//  Created by Administrator on 2022-06-04.
//

#ifndef parseinput_h
#define parseinput_h

#include <stdio.h>

int GetInput(int *vb, int *no);
void StopProcessingCommand(void);
int IsNextParticiple(int partp, int noun2);
void FreeCharWords(void);

#endif /* parseinput_h */
