//
//  loading_screen.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Decode a ZX Spectrum loading screen (the 6912-byte display-file dump:
//  6144 bytes of bitmap in the Spectrum's non-linear layout, followed by
//  768 bytes of 8x8 colour attributes) and show it as a title image.
//

#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkimp.h"
#include "decompressz80.h"      /* ExtractTapePayloads */
#include "palette.h"            /* pal[], palchosen, ZXOPT, DefinePalette */
#include "taylor.h"
#include "ui.h"                 /* Bottom, Top, Graphics, Open*Window */
#include "utility.h"            /* MemAlloc, FindGlkWindowWithRock, rocks */

#include "loading_screen.h"

uint8_t *ZXLoadingScreen = NULL;

#define ZX_BITMAP_SIZE 6144
#define ZX_SCREEN_WIDTH 256
#define ZX_SCREEN_HEIGHT 192

/* Draw the 6912-byte SCREEN$ into the (already open) Graphics window,
   scaled by the largest integer multiplier that fits and centred both
   ways. We fill rectangles directly rather than going through PutPixel,
   which has no vertical offset and shares globals the game still needs. */
static void DrawZXScreen(const uint8_t *scr)
{
    if (!scr || !Graphics)
        return;

    glui32 gw, gh;
    glk_window_get_size(Graphics, &gw, &gh);

    int mult = gh / ZX_SCREEN_HEIGHT;
    if ((glui32)(ZX_SCREEN_WIDTH * mult) > gw)
        mult = gw / ZX_SCREEN_WIDTH;
    if (mult < 1)
        mult = 1;

    int ox = ((int)gw - ZX_SCREEN_WIDTH * mult) / 2;
    int oy = ((int)gh - ZX_SCREEN_HEIGHT * mult) / 2;

    for (int y = 0; y < ZX_SCREEN_HEIGHT; y++) {
        /* Offset of the leftmost bitmap byte of pixel row y. */
        int bitmap_row = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8);
        const uint8_t *attr_row = scr + ZX_BITMAP_SIZE + (y >> 3) * 32;

        for (int col = 0; col < 32; col++) {
            uint8_t bits = scr[bitmap_row + col];
            uint8_t attr = attr_row[col];
            int bright = (attr & 0x40) ? 8 : 0;
            int ink = (attr & 0x07) + bright;
            int paper = ((attr >> 3) & 0x07) + bright;
            /* The flash bit (0x80) is ignored for a static title image. */

            for (int b = 0; b < 8; b++) {
                int set = (bits >> (7 - b)) & 1;
                int x = col * 8 + b;
                glk_window_fill_rect(Graphics, pal[set ? ink : paper],
                    ox + x * mult, oy + y * mult, mult, mult);
            }
        }
    }
}

/* The loading screen is, in practice, always the first standard-length
   (6912-byte) data block on the tape. ExtractTapePayloads() returns only
   the 0xFF data blocks (the 0x00 header blocks are skipped), with the
   offset and length of each, so we just return the first that is exactly
   one SCREEN$ in size. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx)
{
#define MAX_TAPE_BLOCKS 64
    size_t out_len = 0;
    size_t blk_off[MAX_TAPE_BLOCKS], blk_len[MAX_TAPE_BLOCKS];
    int n_blk = 0;

    uint8_t *payloads = ExtractTapePayloads(raw, raw_len, is_tzx, &out_len,
        blk_off, blk_len, &n_blk, MAX_TAPE_BLOCKS);
    if (!payloads)
        return NULL;

    uint8_t *screen = NULL;
    for (int i = 0; i < n_blk; i++) {
        if (blk_len[i] == ZX_SCREEN_SIZE) {
            screen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(screen, payloads + blk_off[i], ZX_SCREEN_SIZE);
            break;
        }
    }
    free(payloads);
    return screen;
}

/* A snapshot captured at a BASIC/text prompt (e.g. "Resume a saved
   game?") has no picture: every attribute cell holds the default black
   ink on white paper (0x38, ignoring the bright and flash bits). */
int ZXScreenIsBlackOnWhite(const uint8_t *scr)
{
    if (!scr)
        return 1;
    for (int i = 0; i < ZX_SCREEN_SIZE - ZX_BITMAP_SIZE; i++)
        if ((scr[ZX_BITMAP_SIZE + i] & 0x3f) != 0x38)
            return 0;
    return 1;
}

/* Wait for a keypress on the title, redrawing the screen on resize. */
static void WaitForKeyZX(winid_t keywin)
{
    if (keywin == NULL)
        return;
    glk_request_char_event(keywin);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            glk_window_clear(Graphics);
            DrawZXScreen(ZXLoadingScreen);
        }
    } while (ev.type != evtype_CharInput);
}

void DrawZXTitleImage(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    if (!ZXLoadingScreen)
        return;

    /* Force the ZX palette regardless of the game's own (a forced-C64
       setting would otherwise mangle the colours), restored afterward. */
    palette_type storedpal = palchosen;
    palchosen = ZXOPT;
    DefinePalette();

    /* Replace the whole window tree with a single full-screen graphics
       window, like ScottFree's title routine. */
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }
    Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
    }
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_window_close(Bottom, NULL);
        Bottom = NULL;
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);
    if (Graphics == NULL) {
        palchosen = storedpal;
        DefinePalette();
        OpenBottomWindow();
        glk_set_window(Bottom);
        OpenTopWindow();
        return;
    }

    /* Black border in the centring margins around the 256x192 screen. */
    glk_window_set_background_color(Graphics, 0);
    glk_window_clear(Graphics);

    /* We need a window that can take a char event. Use the graphics
       window if the implementation supports it, else a thin text buffer. */
    winid_t keywin = Graphics;
    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0) {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        keywin = Bottom;
    }

    DrawZXScreen(ZXLoadingScreen);
    WaitForKeyZX(keywin);

    /* Tear the title down and rebuild the normal window stack. */
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom)
        glk_window_close(Bottom, NULL);
    OpenBottomWindow();
    glk_set_window(Bottom);
    /* DisplayInit() had pointed CurrentWindow at the now-closed Bottom;
       repoint it at the fresh one so output isn't misrouted. */
    CurrentWindow = Bottom;
    OpenTopWindow();

    palchosen = storedpal;
    DefinePalette();

    free(ZXLoadingScreen);
    ZXLoadingScreen = NULL;
}
