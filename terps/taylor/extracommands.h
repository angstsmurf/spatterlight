//
//  extracommands.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#ifndef extracommands_h
#define extracommands_h

int TryExtraCommand(void);
int ParseExtraCommand(const char *p);

#define EXTRA_COMMAND 0xffff

#endif /* extracommands_h */
