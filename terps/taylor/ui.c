/*
 *	Simple curses UI implementation. You probably want to replace
 *	this with something more useful
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "glk.h"
#include "glkstart.h"

#include "taylor.h"

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

winid_t Bottom, Top;
winid_t Graphics;
static int OutputPos;
static int OutLine;
static int OutC;
static char OutWord[128];
static int Window;
static int SavedPos;

glui32 Width; /* Terminal width */
glui32 TopHeight; /* Height of top window */

winid_t FindGlkWindowWithRock(glui32 rock)
{
    winid_t win;
    glui32 rockptr;
    for (win = glk_window_iterate(NULL, &rockptr); win;
         win = glk_window_iterate(win, &rockptr)) {
        if (rockptr == rock)
            return win;
    }
    return 0;
}

static void WriteChar(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
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
        }
    } while (result == 0);

    return;
}



void PrintCharacter(unsigned char c)
{
    WriteChar(Bottom, "%c", c);
//	if(OutC == 0 &&  c ==' ')
//		return;
//	if(Window == 1) {
//		if(isspace(c)) {
//            WriteChar(Bottom, " ");
//			return;
//		}
//		OutWord[OutC] = c;
//		OutC++;
//		if(OutC > 79)
//			WordFlush(Bottom);
//		else if(OutC + OutputPos > 78)
//			Scroll(Bottom);
//		return;
//	} else {
//		if(isspace(c)) {
//			WordFlush(Top);
//			WriteChar(Top, ' ');
//			if(c == '\n') {
//				OutLine++;
//				OutputPos = 0;
//				wmove(Top, OutLine, OutputPos);
//			}
//			return;
//		}
//		OutWord[OutC] = c;
//		OutC++;
//		if(OutC == 78)
//			WordFlush(Top);
//		else if(OutC + OutputPos > 78) {
//			OutLine++;
//			OutputPos = 0;
//		}
//		return;
//	}
}

unsigned char WaitCharacter(void)
{
    glk_request_char_event(Bottom);
    
    event_t ev;    
    do {
        glk_select(&ev);
    } while (ev.type != evtype_CharInput);
    return ev.val1;
}

void Look(void);

void LineInput(char *buf, int len)
{
    event_t ev;

    glk_request_line_event(Bottom, buf, len - 1, 0);

    while(1)
    {
        glk_select(&ev);

        if(ev.type == evtype_LineInput)
            break;
        else if(ev.type == evtype_Arrange)
            Look();
    }

    buf[ev.val1] = 0;
}

void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
                              TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
        if (Top == NULL) {
            Top = Bottom;
        } else {
            glk_window_get_size(Top, &Width, NULL);
        }
    }
}

void DisplayInit(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
}


//void glk_main(void)
//{
//    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
//    if (Bottom == NULL)
//        glk_exit();
//    glk_set_window(Bottom);
//
//    if (game_file == NULL)
//        Fatal("No game provided");
//
//    for (int i = 0; i < MAX_SYSMESS; i++) {
//        sys[i] = sysdict[i];
//    }
//
//    const char **dictpointer;
//
//    if (Options & YOUARE)
//        dictpointer = sysdict;
//    else
//        dictpointer = sysdict_i_am;
//
//    for (int i = 0; i < MAX_SYSMESS && dictpointer[i] != NULL; i++) {
//        sys[i] = dictpointer[i];
//    }
//
//    GameIDType game_type = DetectGame(game_file);
//
//    if (!game_type)
//        Fatal("Unsupported game!");
//
//    if (game_type != SCOTTFREE && game_type != TI994A) {
//        Options |= SPECTRUM_STYLE;
//        split_screen = 1;
//    } else {
//        if (game_type != TI994A)
//            Options |= TRS80_STYLE;
//        split_screen = 1;
//    }
//
//    if (title_screen != NULL) {
//        if (split_screen)
//            PrintTitleScreenGrid();
//        else
//            PrintTitleScreenBuffer();
//    }
//
//    if (Options & TRS80_STYLE) {
//        Width = 80;
//        TopHeight = 10;
//    }
//
//    OpenTopWindow();
//
//    if (game_type == SCOTTFREE || CurrentGame == TI994A)
//        Output("\
//Scott Free, A Scott Adams game driver in C.\n\
//Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
//Distributed under the GNU software license\n\n");
//
//    if (CurrentGame == TI994A) {
//        Display(Bottom, "In this adventure, you may abbreviate any word \
//by typing its first %d letters, and directions by typing \
//one letter.\n\nDo you want to restore previously saved game?\n",
//                GameHeader.WordLength);
//        if (YesOrNo())
//            LoadGame();
//        ClearScreen();
//    }
//
//#ifdef SPATTERLIGHT
//    UpdateSettings();
//    if (gli_determinism)
//        srand(1234);
//    else
//#endif
//        srand((unsigned int)time(NULL));
//
//    initial_state = SaveCurrentState();
//
//    while (1) {
//        glk_tick();
//
//        if (should_restart)
//            RestartGame();
//
//        if (!stop_time)
//            PerformActions(0, 0);
//        if (!(CurrentCommand && CurrentCommand->allflag && !(CurrentCommand->allflag & LASTALL))) {
//            print_look_to_transcript = should_look_in_transcript;
//            Look();
//            print_look_to_transcript = should_look_in_transcript = 0;
//            if (!stop_time && !should_restart)
//                SaveUndo();
//        }
//
//        if (should_restart)
//            continue;
//
//        if (GetInput(&vb, &no) == 1)
//            continue;
//
//        switch (PerformActions(vb, no)) {
//            case ER_RAN_ALL_LINES_NO_MATCH:
//                if (!RecheckForExtraCommand()) {
//                    Output(sys[I_DONT_UNDERSTAND]);
//                    FreeCommands();
//                }
//                break;
//            case ER_RAN_ALL_LINES:
//                Output(sys[YOU_CANT_DO_THAT_YET]);
//                FreeCommands();
//                break;
//            default:
//                just_started = 0;
//        }
//
//        /* Brian Howarth games seem to use -1 for forever */
//        if (Items[LIGHT_SOURCE].Location != DESTROYED && GameHeader.LightTime != -1 && !stop_time) {
//            GameHeader.LightTime--;
//            if (GameHeader.LightTime < 1) {
//                BitFlags |= (1 << LIGHTOUTBIT);
//                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
//                    Output(sys[LIGHT_HAS_RUN_OUT]);
//                }
//                if ((Options & PREHISTORIC_LAMP) || (GameInfo->subtype & MYSTERIOUS) || CurrentGame == TI994A)
//                    Items[LIGHT_SOURCE].Location = DESTROYED;
//            } else if (GameHeader.LightTime < 25) {
//                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
//                    if ((Options & SCOTTLIGHT) || (GameInfo->subtype & MYSTERIOUS)) {
//                        Display(Bottom, "%s %d %s\n",sys[LIGHT_RUNS_OUT_IN], GameHeader.LightTime, sys[TURNS]);
//                    } else {
//                        if (GameHeader.LightTime % 5 == 0)
//                            Output(sys[LIGHT_GROWING_DIM]);
//                    }
//                }
//            }
//        }
//    }
//}
