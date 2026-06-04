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

#include "alkatraz_screen.h"
#include "loading_screen.h"

uint8_t *ZXLoadingScreen = NULL;

/* For the slow "draw as the loader did" reveal: the screen addresses in the
   exact order the Alkatraz loader wrote them. Empty (length 0) for plain
   loaders/snapshots, which fall back to a linear top-to-bottom reveal. */
static uint16_t AlkatrazDrawOrder[6912];
static int AlkatrazDrawOrderLen = 0;

#define ZX_BITMAP_SIZE 6144
#define ZX_SCREEN_WIDTH 256
#define ZX_SCREEN_HEIGHT 192

/* Reveal timing: ~REVEAL_TICKS timer steps to paint the whole screen. */
#define ZX_REVEAL_TICK_MS 30
#define ZX_REVEAL_TICKS   120

void ZXSetDrawOrder(const uint16_t *order, int n)
{
    if (n > ZX_SCREEN_SIZE)
        n = ZX_SCREEN_SIZE;
    if (n < 0 || order == NULL)
        n = 0;
    if (n > 0)
        memcpy(AlkatrazDrawOrder, order, n * sizeof AlkatrazDrawOrder[0]);
    AlkatrazDrawOrderLen = n;
}

/* Draw the 6912-byte SCREEN$ into the (already open) Graphics window,
   scaled by the largest integer multiplier that fits and centred both
   ways. We fill rectangles directly rather than going through PutPixel,
   which has no vertical offset and shares globals the game still needs. */
/* Largest integer scale that fits 256x192 in the window, and the centring
   offsets. */
static void zx_geometry(int *mult, int *ox, int *oy)
{
    glui32 gw, gh;
    glk_window_get_size(Graphics, &gw, &gh);

    int m = gh / ZX_SCREEN_HEIGHT;
    if ((glui32)(ZX_SCREEN_WIDTH * m) > gw)
        m = gw / ZX_SCREEN_WIDTH;
    if (m < 1)
        m = 1;

    *mult = m;
    *ox = ((int)gw - ZX_SCREEN_WIDTH * m) / 2;
    *oy = ((int)gh - ZX_SCREEN_HEIGHT * m) / 2;
}

/* Draw the 8 pixels of one bitmap byte (display-file address bmaddr) using the
   attribute currently in scr for that cell. */
static void DrawBitmapByte(const uint8_t *scr, uint16_t bmaddr,
    int mult, int ox, int oy)
{
    int offset = bmaddr - 0x4000;
    int col = offset & 0x1f;
    int y = ((offset & 0x0700) >> 8) | ((offset & 0x00e0) >> 2) | ((offset & 0x1800) >> 5);

    uint8_t bits = scr[offset];
    uint8_t attr = scr[ZX_BITMAP_SIZE + (y >> 3) * 32 + col];
    int bright = (attr & 0x40) ? 8 : 0;
    int ink = (attr & 0x07) + bright;
    int paper = ((attr >> 3) & 0x07) + bright;
    /* The flash bit (0x80) is ignored for a static title image. */

    for (int b = 0; b < 8; b++) {
        int set = (bits >> (7 - b)) & 1;
        glk_window_fill_rect(Graphics, pal[set ? ink : paper],
            ox + (col * 8 + b) * mult, oy + y * mult, mult, mult);
    }
}

/* Redraw the cell at a screen address: one bitmap byte, or (for an attribute
   address) the whole 8x8 character it colours. */
static void RedrawCell(const uint8_t *scr, uint16_t addr, int mult, int ox, int oy)
{
    if (addr < 0x5800) {
        DrawBitmapByte(scr, addr, mult, ox, oy);
        return;
    }
    int cell = addr - 0x5800;
    int crow = cell >> 5, ccol = cell & 0x1f;
    for (int py = 0; py < 8; py++) {
        int y = crow * 8 + py;
        int offset = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8) | ccol;
        DrawBitmapByte(scr, (uint16_t)(0x4000 + offset), mult, ox, oy);
    }
}

static void DrawZXScreen(const uint8_t *scr)
{
    if (!scr || !Graphics)
        return;

    int mult, ox, oy;
    zx_geometry(&mult, &ox, &oy);

    for (int y = 0; y < ZX_SCREEN_HEIGHT; y++) {
        int bitmap_row = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8);
        for (int col = 0; col < 32; col++)
            DrawBitmapByte(scr, (uint16_t)(0x4000 + bitmap_row + col), mult, ox, oy);
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

/* Some Alkatraz-protected games (Kayleth, Terraquake) hide the loading screen
   behind the turbo loader, which draws the picture pixel-by-pixel in a
   scrambled order rather than storing a plain SCREEN$ block. We expand the
   loader's window descriptors into that draw order (AlkatrazScreenOrder), pull
   the pixel stream from the given tape block, and decrypt each byte with the
   same rolling-key XOR the loader uses, writing it to the screen address it
   draws next. Background bitmap is black and the attributes are a uniform
   fill. The per-game descriptors and parameters live in the game's
   *_loadscreen_data.h header.

   Returns a malloc'd 6912-byte SCREEN$ (caller frees) or NULL. */
uint8_t *DecodeAlkatrazLoadingScreen(uint8_t *image, size_t length,
    const uint8_t *descriptors, int nwin, int block_index, int offset,
    uint8_t loacon0, uint8_t add2, uint8_t attrfill)
{
    uint16_t order[ZX_SCREEN_SIZE];
    int n = AlkatrazScreenOrder(descriptors, nwin, order, ZX_SCREEN_SIZE);
    if (n <= 0)
        return NULL;

    /* Remember the draw order so the title can be revealed slowly, in the
       same scrambled sequence the loader used. */
    ZXSetDrawOrder(order, n);

    size_t blocklen = length;
    uint8_t *block = GetTZXBlock(block_index, image, &blocklen);
    if (block == NULL)
        return NULL;
    if (blocklen < (size_t)(offset + n)) {
        free(block);
        return NULL;
    }

    uint8_t *screen = MemAlloc(ZX_SCREEN_SIZE);
    memset(screen, 0x00, ZX_BITMAP_SIZE);                 /* black bitmap   */
    memset(screen + ZX_BITMAP_SIZE, attrfill,
        ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);                 /* attribute fill */

    uint8_t loacon = loacon0;
    for (int k = 0; k < n; k++) {
        uint16_t addr = order[k];
        uint8_t enc = block[offset + k];
        /* addr is a Spectrum screen address (0x4000-0x5AFF); offset 0x4000
           maps to the start of our SCREEN$ buffer. */
        screen[addr - 0x4000] =
            enc ^ loacon ^ 1 ^ ((addr >> 8) & 0xff) ^ (addr & 0xff);
        loacon += add2;
    }

    free(block);
    return screen;
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

/* Reveal the title progressively, in the order the loader drew it (the
   captured Alkatraz order, or a linear top-to-bottom sweep for plain loaders),
   on a timer. A keypress dismisses it (skipping any remaining reveal). */
static void ZXSlowReveal(winid_t keywin)
{
    if (keywin == NULL || !ZXLoadingScreen)
        return;

    const uint16_t *order;
    int n;
    static uint16_t linear[ZX_SCREEN_SIZE];
    if (AlkatrazDrawOrderLen > 0) {
        order = AlkatrazDrawOrder;
        n = AlkatrazDrawOrderLen;
    } else {
        for (int i = 0; i < ZX_SCREEN_SIZE; i++)
            linear[i] = (uint16_t)(0x4000 + i);
        order = linear;
        n = ZX_SCREEN_SIZE;
    }

    /* Start from the unrevealed background: copy the finished screen, then
       blank the cells we are about to draw (bitmap to black, attributes to the
       screen's uniform fill = its most common attribute byte). */
    static uint8_t work[ZX_SCREEN_SIZE];
    memcpy(work, ZXLoadingScreen, ZX_SCREEN_SIZE);
    int counts[256] = { 0 };
    for (int i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
        counts[ZXLoadingScreen[i]]++;
    int fill = 0;
    for (int v = 1; v < 256; v++)
        if (counts[v] > counts[fill])
            fill = v;
    for (int k = 0; k < n; k++) {
        uint16_t a = order[k];
        work[a - 0x4000] = (a < 0x5800) ? 0x00 : (uint8_t)fill;
    }

    int mult, ox, oy;
    zx_geometry(&mult, &ox, &oy);
    glk_window_clear(Graphics);
    DrawZXScreen(work);

    int per_tick = n / ZX_REVEAL_TICKS;
    if (per_tick < 1)
        per_tick = 1;
    int pos = 0;

    glk_request_timer_events(ZX_REVEAL_TICK_MS);
    glk_request_char_event(keywin);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            int end = pos + per_tick;
            if (end > n)
                end = n;
            for (; pos < end; pos++) {
                uint16_t a = order[pos];
                work[a - 0x4000] = ZXLoadingScreen[a - 0x4000];
                RedrawCell(work, a, mult, ox, oy);
            }
            if (pos >= n)
                glk_request_timer_events(0);
        } else if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            zx_geometry(&mult, &ox, &oy);
            glk_window_clear(Graphics);
            DrawZXScreen(work);
        }
    } while (ev.type != evtype_CharInput);

    glk_request_timer_events(0);
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

    /* Reveal the picture slowly (in the loader's draw order) when the
       slow-draw setting is on; otherwise paint it at once. */
    int slow = 1;
#ifdef SPATTERLIGHT
    slow = gli_slowdraw && !gli_determinism;
#endif
    if (slow) {
        ZXSlowReveal(keywin);
    } else {
        DrawZXScreen(ZXLoadingScreen);
        WaitForKeyZX(keywin);
    }

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
    AlkatrazDrawOrderLen = 0;
}
