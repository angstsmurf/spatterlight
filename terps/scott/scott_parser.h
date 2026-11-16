/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scott_parser.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-19.
 */

#ifndef scott_parser_h
#define scott_parser_h

#include "glk.h"

typedef struct Command {
    int verb;
    int noun;
    int item;
    int verbwordindex;
    int nounwordindex;
    int allflag;
    struct Command *previous;
    struct Command *next;
} Command;

typedef enum {
    NO_COMMAND,
    RESTART,
    SAVE,
    RESTORE,
    SCRIPT,
    ON,
    OFF,
    UNDO,
    RAM,
    RAMSAVE,
    RAMLOAD,
    GAME,
    COMMAND,
    ALL,
    IT,
    EXCEPT,
    FLICKER
} extra_command;

int GetInput(int *vb, int *no);
void FreeCommands(void);
glui32 *ToUnicode(const char *string);
int RecheckForExtraCommand(void);
int FindExtraNoun(int wordindex);
int WhichWord(const char *word, const char **list, int word_length,
    int list_length);

extern glui32 **UnicodeWords;

typedef enum {
    PARSER_ENGLISH,
    PARSER_GERMAN,
    PARSER_SPANISH,
    PARSER_ITALIAN
} ParserLanguage;

/* Switch the parser's word lists to those of a language */
void SetParserLanguage(ParserLanguage language);

#endif /* scott_parser_h */
