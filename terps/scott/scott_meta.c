/*
 *  scott_meta.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Save/load, restart, transcript, undo, and meta-command handling.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "restore_state.h"

#include "parser.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

extern SavedState *InitialState;

/* Save the current game state to a file via Glk file prompts.
   Writes counters, room flags, bit flags, location, and item positions. */
void SaveGame(void)
{
    strid_t file;
    frefid_t ref;
    int ct;
    char buf[128];

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        snprintf(buf, sizeof buf, "%d %d\n", Counters[ct], RoomSaved[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%ld %d %hd %d %d %hd %d\n", BitFlags,
        (BitFlags & (1 << DARKBIT)) ? 1 : 0, MyLoc, CurrentCounter,
        SavedRoom, GameHeader.LightTime, AutoInventory);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)Items[ct].Location);
        glk_put_string_stream(file, buf);
    }

    glk_stream_close(file, NULL);
    Output(sys[SAVED]);
}

/* Load a saved game state from a file via Glk file prompts.
   Validates all loaded values against the current database limits
   and rolls back to a snapshot if any value is out of range. */
void LoadGame(void)
{
    strid_t file;
    frefid_t ref;
    char buf[128];
    int ct = 0;
    short lo;
    short DarkFlag;

    int PreviousAutoInventory = AutoInventory;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Read, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    SavedState *state = SaveCurrentState();

    int result;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%d %d", &Counters[ct], &RoomSaved[ct]);
        if (result != 2 || RoomSaved[ct] > GameHeader.NumRooms) {
            RecoverFromBadRestore(state);
            return;
        }
    }
    glk_get_line_stream(file, buf, sizeof buf);
    result = sscanf(buf, "%ld %hd %hd %d %d %hd %d\n", &BitFlags, &DarkFlag,
        &MyLoc, &CurrentCounter, &SavedRoom, &GameHeader.LightTime,
        &AutoInventory);
    if (result == 6)
        AutoInventory = PreviousAutoInventory;
    if ((result != 7 && result != 6) || MyLoc > GameHeader.NumRooms || MyLoc < 1 || SavedRoom > GameHeader.NumRooms) {
        RecoverFromBadRestore(state);
        return;
    }

    /* Backward compatibility */
    if (DarkFlag)
        SetBitFlag(DARKBIT);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%hd\n", &lo);
        Items[ct].Location = (unsigned char)lo;
        if (result != 1 || (Items[ct].Location > GameHeader.NumRooms && Items[ct].Location != CARRIED)) {
            RecoverFromBadRestore(state);
            return;
        }
    }

    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    glui32 end = glk_stream_get_position(file);
    if (end != position) {
        RecoverFromBadRestore(state);
        return;
    }

    SaveUndo();
    JustStarted = 0;
    StopTime = 1;
    should_look_in_transcript = should_draw_image =  1;
}

void RestartGame(void)
{
    if (CurrentCommand)
        FreeCommands();
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    Display(Bottom, "\n\n");
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        Output(sys[TRANSCRIPT_ALREADY]);
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);

    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        Output(sys[FAILED_TRANSCRIPT]);
        return;
    }

    glui32 *start_of_transcript = ToUnicode(sys[TRANSCRIPT_START]);
    glk_put_string_stream_uni(Transcript, start_of_transcript);
    free(start_of_transcript);

    print_look_to_transcript = 1;
    Look();

    glk_put_string_stream(glk_window_get_stream(Bottom),
        (char *)sys[TRANSCRIPT_ON]);
    glk_window_set_echo_stream(Bottom, Transcript);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Output(sys[NO_TRANSCRIPT]);
        return;
    }

    glk_window_set_echo_stream(Bottom, NULL);

    glui32 *end_of_transcript = ToUnicode(sys[TRANSCRIPT_END]);
    glk_put_string_stream_uni(Transcript, end_of_transcript);
    free(end_of_transcript);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    Output(sys[TRANSCRIPT_OFF]);
}

static void FlickerOn(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is already on");
    } else {
        Output("Flicker is now on");
    }
    if (Options & NO_DELAYS)
        Output(" (but delays are off, so it won't have any effect.)");
    else Output(".");
    Output("\n");
    Options |= FLICKER_ON;
}

static void FlickerOff(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is now off.");
    } else {
        Output("Flicker is already off.");
    }
    Options &= ~FLICKER_ON;
}

/* Handle meta-commands that aren't part of the game's action table:
   save, restore, restart, undo, RAM save/load, transcript, and flicker.
   Returns 1 if the command was handled, 0 otherwise. */
int PerformExtraCommand(int extra_stop_time)
{
    Command command = *CurrentCommand;
    int verb = command.verb;
    if (verb > GameHeader.NumWords)
        verb -= GameHeader.NumWords;
    int noun = command.noun;
    if (noun > GameHeader.NumWords)
        noun -= GameHeader.NumWords;
    else if (noun) {
        const char *NounWord = CharWords[CurrentCommand->nounwordindex];
        int newnoun = WhichWord(NounWord, ExtraNouns, strlen(NounWord),
            NUMBER_OF_EXTRA_NOUNS);
        newnoun = ExtraNounsKey[newnoun];
        if (newnoun)
            noun = newnoun;
    }

    StopTime = 1 + extra_stop_time;

    switch (verb) {
    case RESTORE:
        if (noun == 0 || noun == GAME) {
            LoadGame();
            return 1;
        }
        break;
    case RESTART:
        if (noun == 0 || noun == GAME) {
            Output(sys[ARE_YOU_SURE]);
            if (YesOrNo()) {
                should_restart = 1;
            }
            return 1;
        }
        break;
    case SAVE:
        if (noun == 0 || noun == GAME) {
            SaveGame();
            return 1;
        }
        break;
    case UNDO:
        if (noun == 0 || noun == COMMAND) {
            RestoreUndo();
            return 1;
        }
        break;
    case RAM:
        if (noun == RAMLOAD) {
            RamRestore();
            return 1;
        } else if (noun == RAMSAVE) {
            RamSave();
            return 1;
        }
        break;
    case RAMSAVE:
        if (noun == 0) {
            RamSave();
            return 1;
        }
        break;
    case RAMLOAD:
        if (noun == 0) {
            RamRestore();
            return 1;
        }
        break;
    case SCRIPT:
        if (noun == ON || noun == 0) {
            TranscriptOn();
            return 1;
        } else if (noun == OFF) {
            TranscriptOff();
            return 1;
        }
        break;
    case EXCEPT:
        FreeCommands();
    case FLICKER:
        if (noun == ON || noun == 0) {
            FlickerOn();
            return 1;
        } else if (noun == OFF) {
            FlickerOff();
            return 1;
        }
        break;
    }

    StopTime = 0;
    return 0;
}

/* Prompt for a yes/no response via single-character input.
   Uses the localized first characters of sys[YES] and sys[NO]. */
int YesOrNo(void)
{
    glk_request_char_event_uni(Bottom);

    event_t ev;
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if ((glsi32)ev.val1 > ' ') {
                glk_put_char_stream_uni(glk_window_get_stream(Bottom), ev.val1);
            }
            const char reply = tolower((char)ev.val1);
            if (reply == y) {
                result = 1;
            } else if (reply == n) {
                result = 2;
            } else {
                Output("\n");
                Output(sys[ANSWER_YES_OR_NO]);
                glk_request_char_event_uni(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);

    return (result == 1);
}

/* Present a numbered menu — an intro line followed by the given titles — for a
   compilation (disk or tape) that holds several games, and return the
   zero-based index the player chooses. Entries are keyed 1-9, then A.. for any
   beyond nine. Mirrors YesOrNo's single-character input handling, and clears
   the window afterwards so the game starts on a clean screen. */
int SelectGameFromMenu(const char *intro, const char **titles, int count)
{
    if (count <= 1)
        return 0;
    if (count > 35) /* 1-9 then A-Z */
        count = 35;

    if (intro != NULL) {
        Output(intro);
        Output("\n");
    }
    for (int i = 0; i < count; i++) {
        char label = (i < 9) ? (char)('1' + i) : (char)('A' + (i - 9));
        Display(Bottom, "\n%c. %s", label, titles[i]);
    }

    glk_request_char_event_uni(Bottom);

    event_t ev;
    int choice = -1;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            glui32 c = ev.val1;
            int idx = -1;
            if (c >= '1' && c <= '9')
                idx = (int)(c - '1');
            else if (c >= 'A' && c <= 'Z')
                idx = (int)(c - 'A' + 9);
            else if (c >= 'a' && c <= 'z')
                idx = (int)(c - 'a' + 9);
            if (idx >= 0 && idx < count)
                choice = idx;
            else
                glk_request_char_event_uni(Bottom);
        } else
            Updates(ev);
    } while (choice < 0);

    glk_window_clear(Bottom);
    return choice;
}

void HitEnter(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else {
                glk_request_char_event(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);
    showing_closeup = 0;
    should_draw_image = 1;
    return;
}
