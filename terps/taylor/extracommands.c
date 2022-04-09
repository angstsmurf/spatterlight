//
//  extracommands.c
//  taylor
//
//  Created by Administrator on 2022-04-05.
//
#include <string.h>
#include <ctype.h>

#include "restorestate.h"
#include "utility.h"
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
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "bom",
    "oops",
    "undo",
    "#undo",
    "ram",
    "ramload",
    "qload",
    "quickload",
    "ramrestore",
    "#quickload",
    "quicksave",
    "qsave",
    "ramsave",
    "#quicksave",
    "game",
    "story",
    "move",
    "command",
    "turn",
    NULL
};

extra_command ExtraCommandsKey[] = {
    RESTART,
    RESTART,
    SAVE_EXTR,
    SAVE_EXTR,
    RESTORE,
    RESTORE,
    RESTORE,
    UNDO,
    UNDO,
    UNDO,
    UNDO,
    RAM,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMSAVE_EXTR,
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

extern winid_t Bottom;

int YesOrNo(void);
void SaveGame(void);
int LoadGame(void);

static int ParseExtraCommand(char *p)
{
    if (p == NULL)
        return NO_COMMAND;
    size_t len = strlen(p);
    if (len == 0)
        return NO_COMMAND;
    int j = 0;
    int found = 0;
    while(ExtraCommands[j] != NULL) {
        size_t commandlen = strlen(ExtraCommands[j]);
        if (commandlen == len) {
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
                Display(Bottom, "Restart? (Y/N) ");
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
