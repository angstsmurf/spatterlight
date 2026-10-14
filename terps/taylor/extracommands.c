//
//  extracommands.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "extracommands.h"
#include "parseinput.h"
#include "player.h"
#include "restorestate.h"
#include "utility.h"

strid_t Transcript = NULL;

/* Every interpreter provides this for the shared common_utils.c, whose
   Fatal() calls it after printing its message. */
GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

typedef enum {
    NO_COMMAND,
    RESTART,
    RESTORE,
    SAVE_EXTR,
    RAMSAVE_EXTR,
    RAMLOAD_EXTR,
    UNDO,
    RAM,
    SCRIPT,
    GAME,
    COMMAND,
    ALL,
    IT,
    ON,
    OFF,
    OTHER_WORD /* A word of the game's dictionary */
} extra_command;

/* Words understood by the interpreter itself, on top of the game's
   dictionary. */
static const struct {
    const char *word;
    extra_command command;
} ExtraCommands[] = {
    { "restart", RESTART },
    { "#restart", RESTART },
    { "save", SAVE_EXTR },
    { "#save", SAVE_EXTR },
    { "restore", RESTORE },
    { "load", RESTORE },
    { "#restore", RESTORE },
    { "bom", UNDO },
    { "oops", UNDO },
    { "undo", UNDO },
    { "#undo", UNDO },
    { "ram", RAM },
    { "ramload", RAMLOAD_EXTR },
    { "qload", RAMLOAD_EXTR },
    { "quickload", RAMLOAD_EXTR },
    { "ramrestore", RAMLOAD_EXTR },
    { "#quickload", RAMLOAD_EXTR },
    { "#qload", RAMLOAD_EXTR },
    { "quicksave", RAMSAVE_EXTR },
    { "qsave", RAMSAVE_EXTR },
    { "ramsave", RAMSAVE_EXTR },
    { "#quicksave", RAMSAVE_EXTR },
    { "#qsave", RAMSAVE_EXTR },
    { "game", GAME },
    { "story", GAME },
    { "move", COMMAND },
    { "command", COMMAND },
    { "turn", COMMAND },
    { "#script", SCRIPT },
    { "script", SCRIPT },
    { "#transcript", SCRIPT },
    { "transcript", SCRIPT },
    { "on", ON },
    { "off", OFF },
    { NULL, NO_COMMAND }
};

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        Display(Bottom, "A transcript is already active. \n");
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        Display(Bottom, "Failed to create transcript file. ");
        return;
    }

    glk_put_string_stream(Transcript, "Start of transcript\n\n");
    Display(Bottom, "Transcript is now on.\n");
    glk_window_set_echo_stream(Bottom, Transcript);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Display(Bottom, "No transcript is currently running.\n");
        return;
    }

    glk_put_string_stream(Transcript, "\n\nEnd of transcript\n");

    glk_stream_close(Transcript, NULL);
    glk_window_set_echo_stream(Bottom, NULL);
    Transcript = NULL;
    Display(Bottom, "Transcript is now off.\n");
}

static int ParseExtraCommand(const char *word)
{
    if (word == NULL)
        return NO_COMMAND;
    for (int i = 0; ExtraCommands[i].word != NULL; i++) {
        if (EqualsIgnoringCase(word, ExtraCommands[i].word))
            return ExtraCommands[i].command;
    }
    return NO_COMMAND;
}

int IsExtraCommand(const char *word)
{
    return ParseExtraCommand(word) != NO_COMMAND;
}

int TryExtraCommand(void)
{
    if (InKaylethPreview)
        return 0;
    int verb_position = WordPositions[0];
    int verb = ParseExtraCommand(InputWordStrings[verb_position]);
    int noun = NO_COMMAND;

    /* The first word the game knows may be the second word of the command:
       GAME in RESTART GAME, or, in a game that has a SAVE of its own but no
       RAM, SAVE in RAM SAVE. */
    if (verb_position > 0
        && (verb > SCRIPT || ParseExtraCommand(InputWordStrings[verb_position - 1]) == RAM)) {
        verb_position--;
        verb = ParseExtraCommand(InputWordStrings[verb_position]);
    }
    if (verb_position + 1 < WordsInInput)
        noun = ParseExtraCommand(InputWordStrings[verb_position + 1]);
    /* A dictionary word is not an extra command, whatever its code */
    if (noun == NO_COMMAND && Word[1] != 0)
        noun = OTHER_WORD;

    StopTime = 1;
    PrintedOK = 1;
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
                ShouldRestart = 1;
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
            RamSave();
            return 1;
        }
        break;
    case RAMSAVE_EXTR:
        if (noun == NO_COMMAND) {
            RamSave();
            return 1;
        }
        break;
    case RAMLOAD_EXTR:
        if (noun == NO_COMMAND) {
            RamLoad();
            return 1;
        }
        break;
    case SCRIPT:
        if (noun == ON || noun == NO_COMMAND) {
            TranscriptOn();
            return 1;
        } else if (noun == OFF) {
            TranscriptOff();
            return 1;
        }
        break;
    default:
        break;
    }

    StopTime = 0;
    Redraw = 0;
    return 0;
}
