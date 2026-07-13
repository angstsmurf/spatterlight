/*
 *  scott_display.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Room description display, image rendering, inventory listing,
 *  and related UI functions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "layouttext.h"
#include "line_drawing.h"
#include "irmak.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "apple2draw.h"
#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "parser.h"
#include "saga.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Game-specific constants for display workarounds */
#define SAVAGE_BEAR_ITEM 20
#define SAVAGE_BEACH_ROOM 8
#define SAVAGE_BEAR_IMAGE 9

static strid_t room_description_stream = NULL;
int print_look_to_transcript = 0;
static int pause_next_room_description = 0;

/* Draw a decorative separator line at the bottom of the status window.
   Spectrum style uses asterisks; TRS-80 style uses <---...---> */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    if (Options & SPECTRUM_STYLE)
        for (unsigned i = 0; i < TopWidth; i++)
            glk_put_char('*');
    else {
        glk_put_char('<');
        for (unsigned i = 0; i < TopWidth - 2; i++)
            glk_put_char('-');
        glk_put_char('>');
    }
}

/* Close the room description memory stream and render its contents.
   In split-screen mode, the text is line-broken to fit the top window;
   otherwise it's printed inline in the main window. Also handles
   transcript output, window delimiter drawing, and the inter-room pause. */
static void FlushRoomDescription(char *buf)
{
    glk_stream_close(room_description_stream, 0);

    if (Transcript && print_look_to_transcript) {
        glui32 *unistring = ToUnicode(buf);
        glk_put_string_stream_uni(Transcript, unistring);
        free(unistring);
    }

    int print_delimiter = (Options & (TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE));

    if (split_screen) {
        glk_window_clear(Top);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        int rows, length;
        char *text_with_breaks = LineBreakText(buf, TopWidth, &rows, &length);

        glui32 bottomheight;
        glk_window_get_size(Bottom, NULL, &bottomheight);
        winid_t o2 = glk_window_get_parent(Top);
        if (!(bottomheight < 3 && TopHeight < rows)) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
                Top);
        } else {
            print_delimiter = 0;
        }

        int line = 0;
        int index = 0;
        int i;
        char string[DISPLAY_BUFFER_SIZE];
        if (TopWidth > DISPLAY_BUFFER_SIZE - 1)
            TopWidth = DISPLAY_BUFFER_SIZE - 1;
        for (line = 0; line < rows && index < length; line++) {
            for (i = 0; i < TopWidth; i++) {
                string[i] = text_with_breaks[index++];
                if (string[i] == '\n' || string[i] == '\r' || index >= length)
                    break;
            }
            if (i < TopWidth + 1) {
                string[i++] = '\n';
            }
            string[i] = 0;
            if (strlen(string) == 0)
                break;
            glk_window_move_cursor(Top, 0, line);
            Display(Top, "%s", string);
        }

        if (line < rows - 1) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
                MIN(rows - 1, TopHeight - 1), Top);
        }

        free(text_with_breaks);
    } else {
        Display(Bottom, "%s", buf);
    }

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    if (pause_next_room_description) {
        Delay(0.8);
        pause_next_room_description = 0;
    }

    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
}

/* Render inventory in the graphics window for US-variant games */
void UpdateUSInventory(void)
{
    char *buf = MemAlloc(ROOM_DESC_BUFFER_SIZE);
    buf = memset(buf, 0, ROOM_DESC_BUFFER_SIZE);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESC_BUFFER_SIZE, filemode_Write, 0);
    ListInventory(1);
    FlushRoomDescription(buf);
    InventoryUS();
}

int ItIsDark(void)
{
    return ((BitFlags & (1 << DARKBIT)) && Items[LIGHT_SOURCE].Location != CARRIED && Items[LIGHT_SOURCE].Location != MyLoc);
}

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * (glui32)pixel_size,
        12 * 8 * (glui32)pixel_size);
}

/* Open or find the status text-grid window above the main window */
void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        if (split_screen) {
            Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
                TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
            if (Top == NULL) {
                split_screen = 0;
                Top = Bottom;
            } else {
                glk_window_get_size(Top, &TopWidth, NULL);
            }
        } else {
            Top = Bottom;
        }
    }
}

/* Compute the largest integer pixel multiplier that fits the game's
   native image dimensions within the available graphics area. */
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight)
{
    int multiplier = 1;
    multiplier = graphheight / ImageHeight;
    if (ImageWidth * multiplier > graphwidth)
        multiplier = graphwidth / ImageWidth;

    if (multiplier == 0)
        multiplier = 1;

    *outwidth = ImageWidth * multiplier;
    *outheight = ImageHeight * multiplier;

    return multiplier;
}

/* Open the graphics window between the status window and the main text
   window, sized to fit the game's native image resolution at the best
   integer scale. Re-opens the status window above it if needed. */
void OpenGraphicsWindow(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
            60, wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(graphwidth, graphheight, &optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                    optimal_height, NULL);
        }

    /* Set the graphics window background to match
       the main window background, best as we can,
       and clear the window. */
        glui32 background_color;
        if (Bottom && glk_style_measure(Bottom, style_Normal, stylehint_BackColor, &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
            wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(graphwidth, graphheight, &optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent)
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
    }
    right_margin = optimal_width + x_offset;
}

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

/* Draw a non-US game image using the appropriate rendering method:
   Howarth vector graphics (format 99) or bitmap picture number. */
void DrawImage(int image)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        debug_print("DrawImage: Graphic window NULL?\n");
        return;
    }
    if (Game->picture_format_version == HOWARTH_FORMAT)
        DrawHowarthVectorPicture(image);
    else
        DrawPictureNumber(image, (Game->type == SEAS_OF_BLOOD_VARIANT));
}

/* Draw the image for the current room, with special handling for
   darkness (black screen or fuzzy image), US-variant layout, and
   game-specific overrides (Seas of Blood, Robin of Sherwood, Hulk,
   Gremlins, Savage Island). Also draws visible item overlays. */
void DrawRoomImage(void)
{
    if (CurrentGame == ADVENTURELAND || CurrentGame == ADVENTURELAND_C64) {
        AdventurelandDarkness();
    }

    int dark = ItIsDark();

    if (dark && Graphics != NULL && (Rooms[MyLoc].Image != NO_IMAGE || Game->type == US_VARIANT)) {
        vector_image_shown = -1;
        VectorState = NO_VECTOR_IMAGE;
        glk_request_timer_events(0);
        if (Game->type == US_VARIANT) {
            glk_window_clear(Graphics);
            if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
                if (!DrawFuzzyRoom(MyLoc)) {
                    ClearVDP();
                }
            } else {
                DrawUSRoom(0);
            }
            DrawImageOrVector();
        } else {
            DrawBlack();
        }
        return;
    }

    if (Game->type == US_VARIANT) {
        LookUS();
        return;
    }

    switch (CurrentGame) {
    case SEAS_OF_BLOOD:
    case SEAS_OF_BLOOD_C64:
        SeasOfBloodRoomImage();
        return;
    case ROBIN_OF_SHERWOOD:
    case ROBIN_OF_SHERWOOD_C64:
        RobinOfSherwoodLook();
        return;
    case HULK:
    case HULK_C64:
        HulkLook();
        return;
    default:
        break;
    }

    if (Rooms[MyLoc].Image == NO_IMAGE) {
        CloseGraphicsWindow();
        return;
    }

    if (dark)
        return;

    if (Game->picture_format_version == HOWARTH_FORMAT) {
        DrawImage(MyLoc - 1);
        return;
    }

    if (Game->type == GREMLINS_VARIANT) {
        GremlinsLook();
    } else {
        DrawImage(Rooms[MyLoc].Image & IMAGE_INDEX_MASK);
    }
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Image && Items[ct].Location == MyLoc) {
            if ((Items[ct].Flag & IMAGE_INDEX_MASK) == MyLoc) {
                DrawImage(Items[ct].Image);
                /* Draw the correct image of the bear on the beach */
            } else if (Game->type == SAVAGE_ISLAND_VARIANT && ct == SAVAGE_BEAR_ITEM && MyLoc == SAVAGE_BEACH_ROOM) {
                DrawImage(SAVAGE_BEAR_IMAGE);
            }
        }
}

static void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 1, 2)))
#endif
    ;

static void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* List room exits in ZX Spectrum style (only shown if exits exist) */
static void ListExitsSpectrumStyle(void)
{
    int ct = 0;
    int f = 0;

    while (ct < NUM_EXITS) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f == 0) {
                if (!(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n\n");
                WriteToRoomDescriptionStream("%s", sys[EXITS]);
            } else {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    WriteToRoomDescriptionStream("\n");
    return;
}

/* List room exits in standard format (always shows header, "None" if empty) */
static void ListExits(void)
{
    int ct = 0;
    int f = 0;

    WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);

    while (ct < NUM_EXITS) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f) {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    if (f == 0)
        WriteToRoomDescriptionStream("%s", sys[NONE]);
    return;
}

static int ItemEndsWithPeriod(int item)
{
    if (item < 0 || item > GameHeader.NumItems)
        return 0;
    const char *desc = Items[item].Text;
    if (desc != NULL && desc[0] != 0) {
        const char lastchar = desc[strlen(desc) - 1];
        if (lastchar == '.' || lastchar == '!') {
            return 1;
        }
    }
    return 0;
}

/* Build and display the full room description: room image, room text,
   exits, visible items, and optionally the inventory. The description
   is assembled in a memory stream, then flushed to the appropriate window. */
void Look(void)
{
    showing_inventory = 0;
    DrawRoomImage();

    if (split_screen && Top == NULL)
        return;

    char *buf = MemAlloc(ROOM_DESC_BUFFER_SIZE);
    buf = memset(buf, 0, ROOM_DESC_BUFFER_SIZE);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESC_BUFFER_SIZE, filemode_Write, 0);

    Room *r;
    int ct, f;

    if (!split_screen) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript && print_look_to_transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }

    if (ItIsDark()) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf);
        return;
    }

    r = &Rooms[MyLoc];

    if (!r->Text) {
        /* Flush rather than bare-return: buf and room_description_stream are
           already allocated/open, and only FlushRoomDescription frees and
           closes them. A bare return leaks both every time it is hit. */
        FlushRoomDescription(buf);
        return;
    }
    /* An initial asterisk means the room description should not */
    /* start with "You are" or equivalent */
    if (*r->Text == '*') {
        WriteToRoomDescriptionStream("%s", r->Text + 1);
    } else {
        WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
    }

    if (!(Options & SPECTRUM_STYLE)) {
        ListExits();
        WriteToRoomDescriptionStream(".\n");
    }

    ct = 0;
    f = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == MyLoc) {
            if (Items[ct].Text[0] == 0) {
                fprintf(stderr, "Invisible item in room: %d\n", ct);
                ct++;
                continue;
            }
            if (f == 0) {
                WriteToRoomDescriptionStream("%s", sys[YOU_SEE]);
                f++;
                if (Options & SPECTRUM_STYLE && !(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n");
            } else if (!(Options & (TRS80_STYLE | SPECTRUM_STYLE))) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
            WriteToRoomDescriptionStream("%s", Items[ct].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
        }
        ct++;
    }

    if ((Options & TI994A_STYLE) && f) {
        WriteToRoomDescriptionStream(".");
    } else if (Options & PC_STYLE && !f)
        WriteToRoomDescriptionStream(". ");

    if (Options & SPECTRUM_STYLE) {
        ListExitsSpectrumStyle();
    } else if (f) {
        WriteToRoomDescriptionStream("\n");
    }

    if ((AutoInventory || (Options & FORCE_INVENTORY)) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventory(1);
    }

    FlushRoomDescription(buf);
}

static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glui32 *unistring = ToUnicode(msg);
    glk_put_string_stream_uni(glk_window_get_stream(Bottom), unistring);
    free(unistring);
}

/* List the player's carried items. When upper=1, writes to the room
   description stream (status window); otherwise writes to the main window. */
void ListInventory(int upper)
{
    void (*print_function)(const char *fmt, ...);
    if (upper) {
        print_function = WriteToRoomDescriptionStream;
    } else {
        print_function = WriteToLowerWindow;
    }

    int i = 0;
    int lastitem = -1;
    print_function("%s", sys[INVENTORY]);
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == CARRIED) {
            if (Items[i].Text[0] == 0) {
                fprintf(stderr, "Invisible item in inventory: %d\n", i);
                i++;
                continue;
            }
            if (lastitem > -1 && (Options & (TRS80_STYLE | SPECTRUM_STYLE)) == 0) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
            lastitem = i;
            print_function("%s", Items[i].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
        }
        i++;
    }
    if (lastitem == -1)
        print_function("%s", sys[NOTHING]);
    else if (Options & TI994A_STYLE) {
        if (!ItemEndsWithPeriod(lastitem))
            print_function(".");
        print_function(" ");
    }
    if (upper) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }
}

/* Perform a Look with a brief pause, used during multi-command
   sequences so the player can see intermediate room descriptions. */
void LookWithPause(void)
{
    if (Rooms[MyLoc].Text == NULL || MyLoc == 0)
        return;
    char fc = Rooms[MyLoc].Text[0];
    if (fc == 0 || fc == '.' || fc == ' ')
        return;
    should_look_in_transcript = should_draw_image = 1;
    pause_next_room_description = 1;
    Look();
}
