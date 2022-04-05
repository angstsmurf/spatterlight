//
//  extracommands.c
//  taylor
//
//  Created by Administrator on 2022-04-05.
//
#include <string.h>
#include <ctype.h>

#include "restorestate.h"
#include "extracommands.h"

typedef enum {
    NO_COMMAND,
    RESTART,
    RESTORE,
    SAVE_EXTR,
    RAMSAVE_EXTR,
    RAMLOAD_EXTR,
    UNDO,
    RAM,
    GAME,
    COMMAND,
    ALL,
    IT,
} extra_command;

const char *ExtraCommands[] = {
    "restart",
    "save",
    "restore",
    "load",
    "bom",
    "oops",
    "undo",
    "ram",
    "ramload",
    "qload",
    "quickload",
    "ramrestore",
    "quicksave",
    "qsave",
    "ramsave",
    "game",
    "story",
    "move",
    "command",
    "turn",
    NULL
};

extra_command ExtraCommandsKey[] = {
    RESTART,
    SAVE_EXTR,
    RESTORE,
    RESTORE,
    UNDO,
    UNDO,
    UNDO,
    RAM,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMSAVE_EXTR,
    RAMSAVE_EXTR,
    RAMSAVE_EXTR,
    GAME,
    GAME,
    COMMAND,
    COMMAND,
    COMMAND,
    NO_COMMAND
};

extern char wb[5][17];
extern int should_restart;
extern int stop_time;
extern int Redraw;

int YesOrNo(void);
void SaveGame(void);
int LoadGame(void);
void OutString(char *p);

static int ParseExtraCommand(char *p)
{
    if (p == NULL)
        return NO_COMMAND;
    size_t len = strlen(p);
    if (len == 0)
        return NO_COMMAND;
    int j = 0;
    int found;
    while(ExtraCommands[j] != NULL) {
        char *c = p;
        found = 1;
        for(int i = 0; i < len; i++) {
            if (tolower(*c++) != ExtraCommands[j][i]) {
                found = 0;
                break;
            }
        }
        if (found) {
            return ExtraCommandsKey[j];
        }
        j++;
    }
    return NO_COMMAND;
}

int TryExtraCommand(void)
{
    int verb = ParseExtraCommand(wb[0]);
    int noun = ParseExtraCommand(wb[1]);

    stop_time = 1;
    Redraw = 1;
    switch (verb) {
        case RESTORE:
            if (noun == NO_COMMAND || noun == GAME) {
                LoadGame();
                return 1;
            }
            break;
        case RESTART:
            if (noun == NO_COMMAND || noun == GAME) {
                OutString("Restart? (Y/N) ");
                if (YesOrNo()) {
                    should_restart = 1;
                }
                return 1;
            }
            break;
        case SAVE_EXTR:
            if (noun == NO_COMMAND || noun == GAME) {
                SaveGame();
                return 1;
            }
            break;
        case UNDO:
            if (noun == NO_COMMAND || noun == COMMAND) {
                RestoreUndo(1);
                return 1;
            }
            break;
        case RAM:
            if (noun == RESTORE) {
                RamLoad();
                return 1;
            } else if (noun == SAVE_EXTR) {
                RamSave(1);
                return 1;
            }
            break;
        case RAMSAVE_EXTR:
            if (noun == NO_COMMAND) {
                RamSave(1);
                return 1;
            }
            break;
        case RAMLOAD_EXTR:
            if (noun == NO_COMMAND) {
                RamLoad();
                return 1;
            }
            break;
            //        case SCRIPT:
            //            if (noun == ON || noun == 0) {
            //                TranscriptOn();
            //                return 1;
            //            } else if (noun == OFF) {
            //                TranscriptOff();
            //                return 1;
            //            }
            //            break;
        default:
            break;
    }

    stop_time = 0;
    Redraw = 0;
    return 0;
}
