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

/* ZX Spectrum display-file addressing and 8x8 character cells. The format
   geometry (ZX_SCREEN_SIZE, ZX_BITMAP_SIZE, ZX_SCREEN_WIDTH/HEIGHT,
   ZX_SCREEN_COLS) and the bitmap/attribute decode come from decompressz80.h. */
#define ZX_SCREEN_BASE      0x4000  /* display file (bitmap) start address  */
#define ZX_ATTR_BASE        0x5800  /* colour attribute area start address  */
#define ZX_CELL             8       /* pixels per byte / character cell side */
#define ZX_COL_MASK         0x1f    /* column index within a row            */

/* Reveal timing: ~REVEAL_TICKS timer steps to paint the whole screen. */
#define ZX_REVEAL_TICK_MS 30
#define ZX_REVEAL_TICKS   120

void ZXSetDrawOrder(const uint16_t *order, int count)
{
    if (count > ZX_SCREEN_SIZE)
        count = ZX_SCREEN_SIZE;
    if (count < 0 || order == NULL)
        count = 0;
    if (count > 0)
        memcpy(AlkatrazDrawOrder, order, count * sizeof AlkatrazDrawOrder[0]);
    AlkatrazDrawOrderLen = count;
}

/* Draw the 6912-byte SCREEN$ into the (already open) Graphics window,
   scaled by the largest integer multiplier that fits and centred both
   ways. We fill rectangles directly rather than going through PutPixel,
   which has no vertical offset and shares globals the game still needs. */
/* Largest integer scale that fits 256x192 in the window, and the centring
   offsets. */
static void zx_geometry(int *scale, int *origin_x, int *origin_y)
{
    glui32 win_w, win_h;
    glk_window_get_size(Graphics, &win_w, &win_h);

    int s = win_h / ZX_SCREEN_HEIGHT;
    if ((glui32)(ZX_SCREEN_WIDTH * s) > win_w)
        s = win_w / ZX_SCREEN_WIDTH;
    if (s < 1)
        s = 1;

    *scale = s;
    *origin_x = ((int)win_w - ZX_SCREEN_WIDTH * s) / 2;
    *origin_y = ((int)win_h - ZX_SCREEN_HEIGHT * s) / 2;
}

/* Draw the 8 pixels of one bitmap byte (display-file address bmaddr) using the
   attribute currently in scr for that cell. */
static void DrawBitmapByte(const uint8_t *screen, uint16_t bitmap_addr,
    int scale, int origin_x, int origin_y)
{
    int byte_offset = bitmap_addr - ZX_SCREEN_BASE;
    int char_col = byte_offset & ZX_COL_MASK;
    int pixel_y = ZXBitmapRow(byte_offset);

    uint8_t row_bits = screen[byte_offset];
    uint8_t attr = screen[ZX_BITMAP_SIZE + (pixel_y / ZX_CELL) * ZX_SCREEN_COLS + char_col];
    int ink, paper;
    ZXDecodeAttr(attr, &ink, &paper);
    /* The flash bit is ignored for a static title image. */

    for (int bit = 0; bit < ZX_CELL; bit++) {
        int pixel_on = (row_bits >> (ZX_CELL - 1 - bit)) & 1;
        glk_window_fill_rect(Graphics, pal[pixel_on ? ink : paper],
            origin_x + (char_col * ZX_CELL + bit) * scale,
            origin_y + pixel_y * scale, scale, scale);
    }
}

/* Redraw the cell at a screen address: one bitmap byte, or (for an attribute
   address) the whole 8x8 character it colours. */
static void RedrawCell(const uint8_t *screen, uint16_t screen_addr,
    int scale, int origin_x, int origin_y)
{
    if (screen_addr < ZX_ATTR_BASE) {
        DrawBitmapByte(screen, screen_addr, scale, origin_x, origin_y);
        return;
    }
    int attr_index = screen_addr - ZX_ATTR_BASE;
    int cell_row = attr_index / ZX_SCREEN_COLS, cell_col = attr_index & ZX_COL_MASK;
    for (int pixel_row = 0; pixel_row < ZX_CELL; pixel_row++) {
        int pixel_y = cell_row * ZX_CELL + pixel_row;
        DrawBitmapByte(screen, (uint16_t)(ZX_SCREEN_BASE + ZXBitmapOffset(pixel_y, cell_col)),
            scale, origin_x, origin_y);
    }
}

static void DrawZXScreen(const uint8_t *screen)
{
    if (!screen || !Graphics)
        return;

    int scale, origin_x, origin_y;
    zx_geometry(&scale, &origin_x, &origin_y);

    for (int pixel_y = 0; pixel_y < ZX_SCREEN_HEIGHT; pixel_y++)
        for (int char_col = 0; char_col < ZX_SCREEN_COLS; char_col++)
            DrawBitmapByte(screen, (uint16_t)(ZX_SCREEN_BASE + ZXBitmapOffset(pixel_y, char_col)),
                scale, origin_x, origin_y);
}

/* The loading screen is, in practice, always the first standard-length
   (6912-byte) data block on the tape. ExtractTapePayloads() returns only
   the 0xFF data blocks (the 0x00 header blocks are skipped), with the
   offset and length of each, so we just return the first that is exactly
   one SCREEN$ in size. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx)
{
#define MAX_TAPE_BLOCKS 64
    size_t payloads_len = 0;
    size_t block_offset[MAX_TAPE_BLOCKS], block_len[MAX_TAPE_BLOCKS];
    int num_blocks = 0;

    uint8_t *payloads = ExtractTapePayloads(raw, raw_len, is_tzx, &payloads_len,
        block_offset, block_len, &num_blocks, MAX_TAPE_BLOCKS);
    if (!payloads)
        return NULL;

    uint8_t *screen = NULL;
    for (int i = 0; i < num_blocks; i++) {
        if (block_len[i] == ZX_SCREEN_SIZE) {
            screen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(screen, payloads + block_offset[i], ZX_SCREEN_SIZE);
            break;
        }
    }
    free(payloads);
    return screen;
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
    const uint8_t *descriptors, int num_windows, int block_index, int offset,
    uint8_t key_seed, uint8_t key_step, uint8_t attr_fill)
{
    uint16_t order[ZX_SCREEN_SIZE];
    int order_len = AlkatrazScreenOrder(descriptors, num_windows, order, ZX_SCREEN_SIZE);
    if (order_len <= 0)
        return NULL;

    /* Remember the draw order so the title can be revealed slowly, in the
       same scrambled sequence the loader used. */
    ZXSetDrawOrder(order, order_len);

    size_t block_len = length;
    uint8_t *block = GetTZXBlock(block_index, image, &block_len);
    if (block == NULL)
        return NULL;
    if (block_len < (size_t)(offset + order_len)) {
        free(block);
        return NULL;
    }

    uint8_t *screen = MemAlloc(ZX_SCREEN_SIZE);
    memset(screen, 0x00, ZX_BITMAP_SIZE);                 /* black bitmap   */
    memset(screen + ZX_BITMAP_SIZE, attr_fill,
        ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);                 /* attribute fill */

    /* key is the loader's rolling XOR key (its LOACON), advancing by
       key_step (the loader's ADD2) after each decoded byte. */
    uint8_t key = key_seed;
    for (int i = 0; i < order_len; i++) {
        uint16_t addr = order[i];
        uint8_t encrypted = block[offset + i];
        /* addr is a Spectrum screen address (0x4000-0x5AFF); ZX_SCREEN_BASE
           maps to the start of our SCREEN$ buffer. */
        screen[addr - ZX_SCREEN_BASE] =
            encrypted ^ key ^ 1 ^ ((addr >> 8) & 0xff) ^ (addr & 0xff);
        key += key_step;
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
    int order_len;
    static uint16_t linear[ZX_SCREEN_SIZE];
    if (AlkatrazDrawOrderLen > 0) {
        order = AlkatrazDrawOrder;
        order_len = AlkatrazDrawOrderLen;
    } else {
        for (int i = 0; i < ZX_SCREEN_SIZE; i++)
            linear[i] = (uint16_t)(ZX_SCREEN_BASE + i);
        order = linear;
        order_len = ZX_SCREEN_SIZE;
    }

    /* Start from the unrevealed background: copy the finished screen, then
       blank the cells we are about to draw (bitmap to black, attributes to the
       screen's uniform fill = its most common attribute byte). */
    static uint8_t work_screen[ZX_SCREEN_SIZE];
    memcpy(work_screen, ZXLoadingScreen, ZX_SCREEN_SIZE);
    int attr_counts[256] = { 0 };
    for (int i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
        attr_counts[ZXLoadingScreen[i]]++;
    int fill_attr = 0;
    for (int value = 1; value < 256; value++)
        if (attr_counts[value] > attr_counts[fill_attr])
            fill_attr = value;
    for (int i = 0; i < order_len; i++) {
        uint16_t addr = order[i];
        work_screen[addr - ZX_SCREEN_BASE] =
            (addr < ZX_ATTR_BASE) ? 0x00 : (uint8_t)fill_attr;
    }

    int scale, origin_x, origin_y;
    zx_geometry(&scale, &origin_x, &origin_y);
    glk_window_clear(Graphics);
    DrawZXScreen(work_screen);

    int cells_per_tick = order_len / ZX_REVEAL_TICKS;
    if (cells_per_tick < 1)
        cells_per_tick = 1;
    int reveal_pos = 0;

    glk_request_timer_events(ZX_REVEAL_TICK_MS);
    glk_request_char_event(keywin);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            int batch_end = reveal_pos + cells_per_tick;
            if (batch_end > order_len)
                batch_end = order_len;
            for (; reveal_pos < batch_end; reveal_pos++) {
                uint16_t addr = order[reveal_pos];
                work_screen[addr - ZX_SCREEN_BASE] = ZXLoadingScreen[addr - ZX_SCREEN_BASE];
                RedrawCell(work_screen, addr, scale, origin_x, origin_y);
            }
            if (reveal_pos >= order_len)
                glk_request_timer_events(0);
        } else if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            zx_geometry(&scale, &origin_x, &origin_y);
            glk_window_clear(Graphics);
            DrawZXScreen(work_screen);
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
