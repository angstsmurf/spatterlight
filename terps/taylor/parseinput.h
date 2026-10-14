//
//  parseinput.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef parseinput_h
#define parseinput_h

void FreeInputWords(void);
void Parser(void);
int ParseWord(const char *word);
int EqualsIgnoringCase(const char *a, const char *b);

extern char **InputWordStrings;
extern int WordPositions[5];
extern int WordIndex;
extern int WordsInInput;
extern uint8_t Word[5];
extern int FoundExtraCommand;

#endif /* parseinput_h */
