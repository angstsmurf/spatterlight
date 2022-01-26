//
//  layouttext.c
//  scott
//
//  Created by Administrator on 2022-01-11.
//

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "layouttext.h"

int FindBreak(const char *buf, int pos, int columns) {

    int diff = 0;

    while (diff < columns && !isspace(buf[pos])) {
        pos--;
        diff++;
    }
    
    if (diff >= columns || diff < 1) /* Found no space */ {
        return -1;
    }

    return diff;
}

/* Breaks a null-terminated string up by inserting newlines, moving words
 down to the next line when reaching the end of the line */
char *LineBreakText(char *source, int columns, int *rows, int *length) {
    columns-=1;

    char *result = NULL;
    char buf[768];
    int col = 0;
    int row = 0;
    int sourcepos = 0;
    int destpos = 0;
    int diff = 0;
    *rows = 0;
    while (source[sourcepos] != '\0') {
        while (col < columns && source[sourcepos] != '\0') {
            if (source[sourcepos] == 10 || source[sourcepos] == 13) {
                /* Found a line break. */
                /* Any spaces before a line break may cause trouble, */
                /* so we delete them */
                while (destpos && buf[destpos - 1] == ' ') {
                    destpos--;
                }
                col = 0;
                row++;
            } else {
                col++;
            }

            buf[destpos++] = source[sourcepos++];

            if (source[sourcepos] == 10 || source[sourcepos] == 13)
                col--;
        }

        /* We have reached the end of a line */
        row++;
        col = 0;

        if (source[sourcepos] == '\0') {
            break;
        }

        diff = FindBreak(source, sourcepos, columns);
        if (diff > -1) { /* We found a suitable break */
            sourcepos = sourcepos - diff;
            destpos = destpos - diff;
            buf[destpos++] = '\n';

            if (isspace(source[sourcepos])) {
                sourcepos++;
            }
        }
    }
    *rows = row;
    *length = 0;
    result = malloc(destpos + 1);
    if (result == NULL)
        return NULL;
    memcpy(result, buf, destpos);
    result[destpos] = '\0';
    *length = destpos;
    return result;
}
