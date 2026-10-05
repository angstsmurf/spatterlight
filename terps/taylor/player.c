//
//  player.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  Game state, room display, the per-turn input loop and glk_main.
//  Text output lives in textoutput.c, file loading and table lookup in
//  gameload.c, and the condition/action interpreter in actions.c.
//

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "actions.h"
#include "animations.h"
#include "c64decrunch.h"
#include "extracommands.h"
#include "gameload.h"
#include "graphics.h"
#include "irmak.h"
#include "loading_screen.h"
#include "parseinput.h"
#include "randomness.h"
#include "restorestate.h"
#include "textoutput.h"
#include "utility.h"

#include "taylor.h"

uint8_t Flag[128];      /* Game state flags (conditions, counters, etc.) */
uint8_t ObjectLoc[256]; /* Location of each object (room number or special values) */

/* Objects below this index are "low objects" — printed as part of the
   room description rather than after the "You can see:" prompt. */
static int NumLowObjects;

int PrintedOK;             /* Whether "OK" has been printed this turn */
int Redraw = 0;            /* Room description needs refreshing */

int StopTime = 0;          /* Suppress status table runs for this many turns */
int JustStarted = 1;       /* True until the first player command */
int ShouldRestart = 0;     /* Set to trigger a full game restart */

int NoGraphics = 0;        /* Disable graphics display */

char DelimiterChar = '_';  /* Word separator in dictionary entries */

static int LastNoun = 0; /* Last noun for "IT" pronoun substitution */
int LastVerb = 0;        /* Last verb for implicit verb carry-forward */
static int GoVerb = 0;   /* Dictionary code for "GO" */

GameInfo *Game = NULL;
int InKaylethPreview = 0;

unsigned char Destroyed(void)
{
    return LOC_DESTROYED;
}

unsigned char Carried(void)
{
    return Flag[2];
}

unsigned char Worn(void)
{
    return Version == QUESTPROBE3_TYPE ? 0 : Flag[3];
}

unsigned char NumObjects(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 49;

    /* This removes one weird empty "You notice." in Kayleth */
    if (BaseGame == KAYLETH)
        return 120;

    return Flag[6];
}

static int WaitFlag(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 5;
    else if (BaseGame != REBEL_PLANET && BaseGame != KAYLETH)
        return -1;
    else
        return 7;
}

int CarryItem(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 1;
    /* Flag 5: Items carried. Flag 4: Max carried */
    if (ItemsCarried == MaxCarried && CurrentGame != BLIZZARD_PASS)
        return 0;
    if (ItemsCarried < 255)
        ItemsCarried++;
    return 1;
}

int DarkFlag(void)
{
    return Version == QUESTPROBE3_TYPE ? 43 : 1;
}

void DropItem(void)
{
    if (Version != QUESTPROBE3_TYPE && ItemsCarried > 0)
        ItemsCarried--;
}

void Put(unsigned char obj, unsigned char loc)
{
    /* Putting stuff in a room might change the picture, so redraw */
    if (ObjectLoc[obj] == MyLoc || loc == MyLoc)
        Redraw = 1;
    ObjectLoc[obj] = loc;
}

int Present(unsigned char obj)
{
    if (obj >= NumObjects())
        return 0;
    unsigned char v = ObjectLoc[obj];
    if (v == MyLoc || v == Worn() || v == Carried() ||
        (Version == QUESTPROBE3_TYPE && v == OtherGuyInv && OtherGuyLoc == MyLoc))
        return 1;
    return 0;
}

/* Reset all game state to initial values and display the starting room. */
static void NewGame(void)
{
    Redraw = 1;
    /* Zero every flag, then copy the seven initial flag values stored in
       the file header. The remaining 121 flags stay at zero. */
    memset(Flag, 0, sizeof(Flag));
    memcpy(Flag, FileImage + FlagBase, 7);
    if (Version == QUESTPROBE3_TYPE) {
        /* QP3 keeps only Flag[0..3] from the file; force the rest to
           zero (this overrides the file's values for Flag[4..6]) and
           seed the inventory and dark flags. */
        for (int i = 0; i < 124; i++) {
            Flag[4 + i] = 0;
        }
        Flag[42] = 0;                /* dark flag for current character */
        Flag[43] = 0;                /* dark flag for other character */
        Flag[2] = LOC_INV_THING;     /* Carried() — default is Thing, before randomization */
        Flag[3] = LOC_INV_TORCH;     /* Worn() slot used for other character's inventory */
    }
    /* Start in room 0 (the intro/title pseudo-room). */
    MyLoc = 0;
    /* Restore each object's initial location from the file's location table. */
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
    /* Reset the per-game wait counter, if this game uses one. */
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
    Look();
    PrintedOK = 1;
}

static int GetGlkFileLength(strid_t stream)
{
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

int YesOrNo(void)
{
    while (1) {
        uint8_t c = WaitCharacter();
        if (c == 250)
            c = 0;
        OutChar(c);
        OutChar('\n');
        OutFlush();
        if (c == 'n' || c == 'N')
            return 0;
        if (c == 'y' || c == 'Y')
            return 1;
        OutString("Please answer Y or N.\n");
        OutFlush();
    }
}

int LoadGame(void)
{
    frefid_t fileref = glk_fileref_create_by_prompt(fileusage_SavedGame,
        filemode_Read, 0);
    if (!fileref) {
        OutFlush();
        return 0;
    }

    /*
     * Reject the file reference if we're expecting to read from it, and the
     * referenced file doesn't exist.
     */
    if (!glk_fileref_does_file_exist(fileref)) {
        OutString("Unable to open file.\n");
        glk_fileref_destroy(fileref);
        OutFlush();
        return 0;
    }

    strid_t stream = glk_stream_open_file(fileref, filemode_Read, 0);
    if (!stream) {
        OutString("Unable to open file.\n");
        glk_fileref_destroy(fileref);
        OutFlush();
        return 0;
    }

    SavedState *state = SaveCurrentState();

    /* Restore saved game data. */

    if (glk_get_buffer_stream(stream, (char *)Flag, sizeof(Flag)) != sizeof(Flag)
        || glk_get_buffer_stream(stream, (char *)ObjectLoc, sizeof(ObjectLoc)) != sizeof(ObjectLoc)
        || (size_t)GetGlkFileLength(stream) != sizeof(Flag) + sizeof(ObjectLoc)) {
        RecoverFromBadRestore(state);
    } else {
        glk_window_clear(Bottom);
        Look();
        free(state);
        InKaylethPreview = 0;
    }
    glk_stream_close(stream, NULL);
    glk_fileref_destroy(fileref);
    return 1;
}

/* Print the player's inventory: every object whose location matches the
   "carried" or "worn" code. Each worn item is annotated with a "(worn)"
   suffix; if nothing is carried, the "you have nothing" message prints. */
void Inventory(void)
{
    /* Rebel Planet does its own capitalization via PrintTextRebelPlanet's
       InventoryLower flag, so we skip OutCaps() for it. */
    if (BaseGame != REBEL_PLANET)
        OutCaps();
    SysMessage(INVENTORY);

    int printed_any = 0;
    for (int i = 0; i < NumObjects(); i++) {
        if (ObjectLoc[i] == Carried() || ObjectLoc[i] == Worn()) {
            printed_any = 1;
            PrintObject(i);
            if (ObjectLoc[i] == Worn()) {
                /* Drop any pending separator, then append "(worn)". */
                OutReplace(0);
                SysMessage(NOWWORN);
                if (CurrentGame == REBEL_PLANET) {
                    /* Rebel Planet uses a comma between items instead of
                       the default trailing space. */
                    OutKillSpace();
                    OutFlush();
                    OutChar(',');
                }
            }
        }
    }
    if (!printed_any) {
        SysMessage(NOTHING);
    } else {
        /* End the list with ". " and arm capitalization for whatever
           prints next (typically the prompt or the next room header). */
        OutReplace('.');
        OutChar(' ');
        OutCaps();
    }
}

void SaveGame(void)
{

    strid_t file;
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    if (ref == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    /* Write game state. */
    glk_put_buffer_stream(file, (char *)Flag, sizeof(Flag));
    glk_put_buffer_stream(file, (char *)ObjectLoc, sizeof(ObjectLoc));
    glk_stream_close(file, NULL);
    OutString("Saved.\n");
    OutFlush();
}

/* List available exits from the current room by scanning the exit table. */
static void ListExits(int caps)
{
    /* Each room's exit block in the table is preceded by a marker byte:
       the room number with the high bit set. */
    unsigned char loc_marker = 0x80 | MyLoc;
    unsigned char *entry = FileImage + ExitBase;
    int printed_any = 0;

    while (*entry != loc_marker)
        entry++;
    entry++;

    /* Exit entries are (direction, destination) byte pairs; the block ends
       when we see another marker byte (high bit set). */
    while (*entry < 0x80) {
        if (!printed_any) {
            if (CurrentGame == BLIZZARD_PASS && LastChar == ',')
                LastChar = 0;
            OutCaps();
            SysMessage(EXITS);
            printed_any = 1;
        }
        if (caps)
            OutCaps();
        SysMessage(*entry);
        entry += 2;
    }
    if (printed_any) {
        OutReplace('.');
        OutChar('\n');
    }
}

/* Display the current room: description, visible objects (low objects
   inline, high objects after "You see:"), exits, and optional inventory.
   Draws the room image if graphics are enabled. */
void Look(void)
{
    /* Room 0 is the intro/title pseudo-room; Kayleth's room 91 is an
       analogous non-game screen. */
    int is_intro_room = (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91));

    if (is_intro_room || NoGraphics)
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();
    int i;
    int printed_any = 0;

    PendSpace = 0;
    /* Skip the output reset while a line-input event is pending in the
       bottom window — resetting would lose buffered input. */
    if (!(CurrentWindow == Bottom && LineEvent))
        OutFlush();
    TopWindow();

    Redraw = 0;
    if (Transcript)
        glk_put_char_stream(Transcript, '\n');

    OutCaps();

    /* Dark room: print the canned "too dark" message and bail out before
       drawing the room or listing its contents. */
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (BaseGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);

    /* "Low" objects are part of the room description (e.g. "...a sword
       lies here."). They're listed inline rather than after "You see:". */
    for (i = 0; i < NumLowObjects; i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (!printed_any) {
                if (Version == QUESTPROBE3_TYPE) {
                    OutReplace(0);
                    SysMessage(0);
                } else if (BaseGame == HEMAN || BaseGame == REBEL_PLANET) {
                    OutChar(' ');
                }
                printed_any = 1;
            }
            PendSpace = 1;
            PrintObject(i);
        }
    }
    if (printed_any && !isalpha(LastChar))
        OutReplace('.');

    /* QP3 has no "You see:" phase — every visible item is a low object.
       For other versions, continue scanning from the same `i` to list
       higher-numbered objects under a "You see:" heading. */
    if (Version == QUESTPROBE3_TYPE) {
        ListExits(1);
    } else {
        printed_any = 0;
        for (; i < NumObjects(); i++) {
            if (ObjectLoc[i] == MyLoc) {
                if (!printed_any) {
                    /* Only the text-only and hybrid games */
                    if (BaseGame == TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR_64) {
                        OutChar(' ');
                    }
                    SysMessage(YOU_SEE);
                    if (CurrentGame == BLIZZARD_PASS) {
                        PendSpace = 0;
                        OutString(":- ");
                    }
                    if (Version == REBEL_PLANET_TYPE)
                        OutReplace(0);
                    printed_any = 1;
                }
                PrintObject(i);
            }
        }
        if (printed_any)
            OutReplace('.');
        else
            OutChar('.');
        ListExits(BaseGame != TEMPLE_OF_TERROR && BaseGame != HEMAN);
    }

    if (LastChar != '\n')
        OutChar('\n');

    /* Don't show top window inventory in intro rooms */
    if ((Options & FORCE_INVENTORY) && !is_intro_room) {
        OutChar('\n');
        Inventory();
        OutChar('\n');
    }

    OutChar('\n');

    BottomWindow();

    if (MyLoc != 0 && !NoGraphics && TAYLOR_GRAPHICS_ENABLED && Graphics) {
        /* A resize-in-progress: reuse the buffered image rather than
           re-running the room's drawing pipeline. */
        if (Resizing) {
            DrawIrmakPictureFromBuffer();
            return;
        }
        if (Version == QUESTPROBE3_TYPE) {
            /* QP3 draws room images via the status table (which contains
               IMAGE actions). Run it once with DrawImages=255 to draw
               without advancing the turn; stash StopTime so the bypass
               doesn't suppress next turn's real status pass. */
            int tempstop = StopTime;
            StopTime = 0;
            DrawImages = 255;
            RunStatusTable();
            QP3DrawExtraImages();
            StopTime = tempstop;
        } else {
            glk_window_clear(Graphics);
            DrawRoomImage();
        }
    }
}

/* Check if direction word v has a matching exit from the current room.
   If so, move the player there and return 1. */
static int AutoExit(unsigned char v)
{
    unsigned char *ptr = FileImage + ExitBase;
    unsigned char want = MyLoc | 0x80;
    while (*ptr != want) {
        if (*ptr == 0xfe)
            return 0;
        ptr++;
    }
    ptr++;
    while (*ptr < 0x80) {
        if (*ptr == v) {
            Goto(ptr[1]);
            return 1;
        }
        ptr += 2;
    }
    return 0;
}

/* Check if a word code is a direction (N/S/E/W/U/D etc.). */
static int IsDir(unsigned char word)
{
    if (word == 0)
        return 0;
    if (Version == QUESTPROBE3_TYPE) {
        return (word <= 4 || word == 57 || word == 60);
    } else
        return (word <= 10);
}

/* Process one player command: try extra commands, direction auto-exits,
   the command action table, and implicit verb carry-forward. Then run
   the status table and handle any pending room redraw. */
static void RunOneInput(void)
{
    PrintedOK = 0;

    if (FoundExtraCommand) {
        if (TryExtraCommand()) {
            if (Redraw)
                Look();
            return;
        }
    }
    if (Word[0] == 0 && Word[1] == 0) {
        if (TryExtraCommand() == 0) {
            OutCaps();
            SysMessage(I_DONT_UNDERSTAND);
            StopTime = 2;
        } else {
            if (Redraw)
                Look();
        }
        return;
    }
    if (IsDir(Word[0]) || (Word[0] == GoVerb && IsDir(Word[1]))) {
        if (AutoExit(Word[0]) || AutoExit(Word[1])) {
            StopTime = 0;
            RunStatusTable();
            if (Redraw)
                Look();
            return;
        }
    }

    /* Handle IT */
    if (Word[1] == 128)
        Word[1] = LastNoun;
    if (Word[1] != 0)
        LastNoun = Word[1];

    OutCaps();
    RunCommandTable();

    if (ActionsExecuted == 0) {
        int OriginalVerb = Word[0];
        if (TryExtraCommand() == 0) {
            if (LastVerb) {
                Word[4] = Word[3];
                Word[3] = Word[2];
                Word[2] = Word[1];
                Word[1] = Word[0];
                Word[0] = LastVerb;
                RunCommandTable();
            }
            if (ActionsExecuted == 0) {
                if (IsDir(OriginalVerb) || (Word[0] == GoVerb && IsDir(Word[1]))) {
                    SysMessage(YOU_CANT_GO_THAT_WAY);
                } else if (FoundVerb) {
                    SysMessage(THATS_BEYOND_MY_POWER);
                } else if (FoundNoun == 1)  {
                    SysMessage(I_DONT_UNDERSTAND_THAT_VERB);
                } else {
                    SysMessage(I_DONT_UNDERSTAND);
                }
                OutFlush();
                StopTime = 1;
                return;
            }
        } else {
            if (Redraw)
                Look();
            return;
        }
    }

    if (Word[0] != 0)
        LastVerb = Word[0];

    if (Redraw && !((BaseGame == REBEL_PLANET && MyLoc == 250) || (BaseGame == KAYLETH && MyLoc == 15))) {
        Look();
    }

    Redraw = 0;

    int waitflag = WaitFlag();

    if (waitflag != -1 && Flag[waitflag] > 1)
        Flag[waitflag]++;

    do {
        if (waitflag != -1 && Flag[waitflag]) {
            Flag[waitflag]--;
            if (LastChar != '\n')
                OutChar('\n');
        }

        if (Version == QUESTPROBE3_TYPE) {
            DrawImages = 0;
            RunStatusTable();
            DrawImages = 255;
            int tempstop = StopTime;
            StopTime = 0;
            RunStatusTable();
            QP3DrawExtraImages();
            StopTime = tempstop;
        } else {
            RunStatusTable();
        }

        if (Redraw) {
            Look();
        }
        Redraw = 0;

    } while (waitflag != -1 && Flag[waitflag] > 0);
    if (AnimationRunning)
        glk_request_timer_events(AnimationRunning);
    if (waitflag != -1)
        Flag[waitflag] = 0;
}

static void RestartGame(void)
{
    RecursionGuard = 0;
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    OutFlush();
    glk_window_clear(Bottom);
    Look();
    RunStatusTable();
    ShouldRestart = 0;
    Look();
}

/* Main entry point. Loads and identifies the game file, locates all data
   tables, initializes graphics, and enters the main input/action loop. */
void glk_main(void)
{
    /* Step 1: load the game file. DetectC64 handles both raw image files
       and C64 disk-image containers, decrunching as needed; the file is
       installed into FileImage/FileImageLen on success. */
    if (DetectC64(&FileImage, &FileImageLen) != UNKNOWN_GAME) {
        EndOfData = FileImage + FileImageLen;
    } else {
        fprintf(stderr, "DetectC64 did not recognize the game\n");
    }

#ifdef DEBUG
    fprintf(stderr, "Loaded %zu bytes.\n", FileImageLen);
#endif

    /* Step 2: locate the verb table by scanning for its signature — the
       dictionary always starts with "NORT" + code 1 + "N" (the first
       direction word, "NORTH", with code 1 followed by its alias "N"). */
    VerbBase = FindCode("NORT\001N", 0, 6);
    if (VerbBase == -1) {
        fprintf(stderr, "No verb table!\n");
        glk_exit();
    }

    /* Step 3: identify which game we're playing. The verb table's offset
       relative to its expected (in-game) address tells us how far the
       loaded file's addressing differs from the original Z80 layout. */
    if (!Game)
        Game = DetectGame(VerbBase);
    if (Game == NULL) {
        fprintf(stderr, "Did not recognize game!\n");
        glk_exit();
    } else {
        FileBaselineOffset = VerbBase - Game->start_of_dictionary;
    }
#ifdef DEBUG
    fprintf(stderr, "FileBaselineOffset: %ld\n", FileBaselineOffset);
#endif

    /* Seed the RNG. In determinism mode (used by automated tests and
       transcript playback) we use a fixed seed; otherwise time-based. */
#ifdef SPATTERLIGHT
    if (gli_determinism) {
        set_erkyrath_random(1234);
    } else
#endif
    set_erkyrath_random(0);

    DisplayInit();

    /* If the loaded file carried a ZX Spectrum loading screen, show it as
       a title image before the game proper begins. */
    DrawZXTitleImage();

    /* Temple of Terror ships as a paired text+graphics duo; check whether
       a companion file is present alongside the loaded one. */
    if (CurrentGame == TEMPLE_OF_TERROR || CurrentGame == TOT_TEXT_ONLY) {
        LookForSecondTOTGame();
    }

    /* Cache the dictionary code for "GO" — used by the implicit-verb
       fallback when the player types just a direction. */
    GoVerb = ParseWord("GO");

    InitGraphics();

    /* QP3 and Rebel Planet separate top and lower text windows by
       a row of '=' rather than the default '_'. */
    if (CurrentGame == QUESTPROBE3 || CurrentGame == REBEL_PLANET)
        DelimiterChar = '=';

    /* Step 4: locate all the remaining tables (tokens, messages, rooms,
       actions, etc.) using game-specific offsets or signature scans. */
    FindTables();
#ifdef DEBUG
    InitActionDebugging();
#endif

    /* Step 5: initialize state and present the opening room. NewGame()
       resets flags and objects; GuessLowObjectEnd() determines which
       objects render inline vs. in the "You see:" list; the initial
       state is snapshotted for the RESTART command. */
    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    InitialState = SaveCurrentState();

    /* Run the status table once on entry so any per-turn setup actions
       (e.g. drawing the first room image) execute before the prompt. */
    RunStatusTable();
    if (Redraw) {
        OutFlush();
        Look();
    }

    /* Kayleth begins with a non-interactive "preview" sequence before
       handing control to the player; load its animation data and arm
       the flag so that undo is suppressed until MyLoc reaches 1. */
    if (BaseGame == KAYLETH) {
        LoadKaylethAnimationData();
        InKaylethPreview = 1;
    }

    /* Main turn loop: snapshot undo state, read a command, execute it,
       and repeat forever. Exits are handled by glk_exit() from within
       QUIT/restart paths. */
    while (1) {
        if (ShouldRestart) {
            RestartGame();
            /* Kayleth's restart sequence is itself non-interactive, so
               don't capture undo until it's done. */
            if (BaseGame != KAYLETH) {
                SaveUndo();
            }
        } else if (!StopTime && !InKaylethPreview) {
            /* Take an undo snapshot before each real player turn, but
               skip while time is paused or the preview is still running. */
            SaveUndo();
        }
        Parser();
        FirstAfterInput = 1;
        RunOneInput();
        if (StopTime) {
            StopTime--;
        } else if (!InKaylethPreview) {
            JustStarted = 0;
        }
        /* Kayleth's preview ends when the player is put in room 1. */
        if (MyLoc == 1) {
            InKaylethPreview = 0;
        }
    }
}
