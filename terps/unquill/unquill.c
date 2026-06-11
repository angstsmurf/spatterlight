/* Pull Quill data from a .SNA file.  *
* Output to stdout.                  */

/* Format of the Quill database:

Somewhere in memory (6D04h for vanilla Quill with no Illustrator/Patch/Press)
there is a colour definition table:

DEFB	10h,ink,11h,paper,12h,flash,13h,bright,14h,inverse,15h,over
DEFB	border

We use this as a magic number to find the Quill database in memory. After
the colour table is a table of values:

DEFB	no. of objects player can carry at once
DEFB	no. of objects
DEFB	no. of locations
DEFB	no. of messages

then a table of pointers:

DEFW	rsptab	;Response table
DEFW	protab	;Process table
DEFW	objtop	;Top of object descriptions
DEFW	loctop	;Top of locations.
DEFW	msgtop	;Top of messages.
**
DEFW	conntab	;Table of connections.
DEFW	voctab	;Table of words.
DEFW	oloctab	;Table of object inital locations.
DEFW	free	;[Early] Start of free memory
;[Late]  Word/object mapping
DEFW	freeend	;End of free memory
DEFS	3	;?
DEFB	80h,dict ;Expansion dictionary (for compressed games).
;Dictionary is stored in ASCII, high bit set on last
;letter of each word.
rsptab:	...		     ;response	- both stored as: DB verb, noun
protab: ...          ;process                     DW address-of-handler
;                               terminated by verb 0.
Handler format is:
DB conditions...0FFh
DB actions......0FFh
objtab:	...		;objects	- all are stored with all bytes
objtop:	DEFW	objtab        complemented to deter casual hackers.
DEFW    object1
DEFW    object2 etc.
loctab:	...		;locations       Texts are terminated with 0E0h
loctop:	DEFW    loctab           (ie 1Fh after decoding).
DEFW    locno1
DEFW    locno2 etc.
msgtab:	...		;messages
msgtop:	DEFW    msgtab
DEFW    mess1
DEFW    mess2 etc.
conntab: ...    ;connections    - stored as DB WORD,ROOM,WORD,ROOM...
each entry is terminated with 0FFh.

voctab: ...		;vocabulary     - stored as DB 'word',number - the
word is complemented to deter hackers.
Table terminated with a word entry all
five bytes of which are 0 before
decoding.
oloctab: ...    ;initial locations of objects. 0FCh => not created
0FDh => worn
0FEh => carried
0FFh => end of table

In later games (those made with Illustrator?), there is an extra byte 
(the number of system messages) after the number of messages, and at 
the label ** the following word is inserted:

DEFW    systab  ;Points to word pointing to table of system
;messages.
systab: DEFW    sysm0
DEFW    sysm1 etc.

The address of the user-defined graphics is stored at 23675. In "Early" games,
the system messages are at UDGs+168.

CPC differences:

* I don't know where the UDGs are.
* Strings are terminated with 0xFF (0 after decoding) rather than 0xE0 
(0x1F).
* No Spectrum colour codes in strings. Instead, the 
code 0x14 means "newline" and 0x09 means "toggle reverse video".
There are other CPC colour codes in the range 0x01-0x0A, but I don't
know their meaning.
* I have assumed that the database always starts at 0x1BD1, which it does 
in the snapshots I have examined.

Commodore 64 differences:

* I don't know where the UDGs are.
* Strings are terminated with 0xFF (0 after decoding) rather than 0xE0 
(0x1F).
* No Spectrum colour codes in strings. 
* I have assumed that the database always starts at 0x0804, which it does 
in the snapshots I have examined.


*/


#define MAINMODULE
#include "unquill.h"    /* Function prototypes */

#include <setjmp.h>
#include <getopt.h>
#include <strings.h>

#include "decompressz80.h"

/* unp64 (terps/unp64) C interface: decompress a crunched C64 program image.
 * Used to unpack the self-extracting program inside a .T64 tape image. */
extern int unp64(uchar *compressed, size_t length, uchar *destinationBuffer,
		 size_t *finalLength, const char *settings);

extern glui32 gli_determinism;
#ifdef SPATTERLIGHT
extern int gli_enable_graphics;
extern int gli_slowdraw;
extern int gli_sa_delays;
#endif

static ushort ucptr;

/* A ZX Spectrum loading screen (SCREEN$) is a 6912-byte dump of the display
 * file: 6144 bytes of bitmap followed by 768 bytes of colour attributes (one
 * byte per 8x8 cell). When we load a .z80 snapshot we keep a copy here so it
 * can be shown as a title image before the game starts. NULL when absent.
 * The format geometry (ZX_SCREEN_SIZE, ZX_BITMAP_SIZE, ZX_SCREEN_WIDTH/HEIGHT,
 * ZX_SCREEN_COLS) and the decode helpers come from decompressz80.h. */
static uchar *zxloadscreen = NULL;

void die(char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    glk_put_string("\n\n*** Fatal Error ***\n\n");
    glk_put_string(buf);
    glk_exit();
}

void glk_printf(char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    glk_put_string(buf);    
}

/* The only command-line argument is the filename. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { "-r", glkunix_arg_ValueFollows, "-r: Restore a save file." },
    { "-l", glkunix_arg_NoValue, "-l: Attempt to interpret as a 'later' type Quill file." },
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

void usage(char *title)
{
    fprintf(stderr,"UnQuill v" VERSION ". Play .SNA snapshots of Quill adventure games.\n");
    fprintf(stderr,"    -l           -- Attempt to interpret as a 'later' type Quill file.\n");
    fprintf(stderr,"    -r file.sav  -- restore a saved game\n");
    exit(1);
}

int glkunix_startup_code(glkunix_startup_t *data)
{
//    char *restorefile;
    int c;
    
    while ((c = getopt(data->argc, data->argv, "lr:")) != -1)
    {
	switch (c)
	{
	    case 'l': dbver = 10; break;
	    case 'r':
//            restorefile = optarg;
            break;
	    default: usage(data->argv[0]); break;
	}
    }
    
    if (data->argc == optind)
	usage(data->argv[0]);
    
    inname = data->argv[optind];
    
    if ((infile = fopen(inname,"rb")) == NULL)
    {
	fprintf(stderr, "Cannot open file '%s'\n", inname);
	return FALSE;
    }
    
    return TRUE;
}

static jmp_buf myenv;

/* Load a .z80 snapshot (the common Spectrum emulator format, which the
 * plain .SNA reader below cannot handle because it is usually compressed).
 * We decompress the whole 48K image with the shared decompressz80 library
 * and copy the part the Quill engine cares about (0x5C00 upwards) into
 * zxmemory, exactly as the .SNA path does. */
static void load_z80(void)
{
    long filelen;
    uchar *raw;
    uchar *uncompressed;
    size_t len;

    if (fseek(infile, 0, SEEK_END) || (filelen = ftell(infile)) < 0)
    {
	die("Cannot read .z80 file '%s'.", inname);
    }
    rewind(infile);

    raw = malloc(filelen);
    if (!raw || fread(raw, filelen, 1, infile) != 1)
    {
	die("Cannot read .z80 file '%s'.", inname);
    }

    len = (size_t)filelen;
    uncompressed = DecompressZ80(raw, &len);
    free(raw);

    /* DecompressZ80 returns the RAM image starting at 0x4000 (0xC000 bytes
     * for a 48K snapshot, more for 128K). We need everything from 0x5C00. */
    if (!uncompressed || len < (size_t)((mem_base - 0x4000) + mem_size))
    {
	free(uncompressed);
	die("'%s' is not a usable Spectrum .z80 snapshot.", inname);
    }

    memcpy(zxmemory, uncompressed + (mem_base - 0x4000), mem_size);

    /* Keep the SCREEN$ (display file at 0x4000, i.e. the first 6912 bytes of
     * the decompressed image) so it can be shown as a loading-screen title. */
    zxloadscreen = malloc(ZX_SCREEN_SIZE);
    if (zxloadscreen)
	memcpy(zxloadscreen, uncompressed, ZX_SCREEN_SIZE);

    free(uncompressed);

    glk_put_string(".z80 snapshot loaded.\n");
}

/* --- ZX Spectrum loading-screen (SCREEN$) display ----------------------- *
 * Ported from the scott interpreter's title_image.c. scott draws into its
 * saga graphics buffer with PutPixel(); here we render straight into a Glk
 * graphics window with glk_window_fill_rect() instead. */

static winid_t zx_gw = NULL;            /* The title graphics window */
static int zx_scale = 1, zx_xoff = 0, zx_yoff = 0;

/* Recompute scaling and centring for the current graphics window size. */
static void zx_layout(void)
{
    glui32 w, h;
    int sx, sy;
    glk_window_get_size(zx_gw, &w, &h);
    sx = (int)w / ZX_SCREEN_WIDTH;
    sy = (int)h / ZX_SCREEN_HEIGHT;
    zx_scale = (sx < sy) ? sx : sy;
    if (zx_scale < 1)
	zx_scale = 1;
    zx_xoff = ((int)w - ZX_SCREEN_WIDTH  * zx_scale) / 2;
    zx_yoff = ((int)h - ZX_SCREEN_HEIGHT * zx_scale) / 3;
    if (zx_xoff < 0) zx_xoff = 0;
    if (zx_yoff < 0) zx_yoff = 0;
    glk_window_clear(zx_gw);
}

/* Draw the 8 pixels of one bitmap byte (display-file address bmaddr) using the
 * attribute currently in scr for that cell. The display file is non-linear, so
 * the pixel row is recovered from the address bits exactly as on hardware. */
static void draw_zx_byte(const uchar *scr, int bmaddr)
{
    int offset = bmaddr - 0x4000;
    int col = offset & 0x1f;
    int y = ZXBitmapRow(offset);
    uchar bits = scr[offset];
    uchar attr = scr[ZX_BITMAP_SIZE + (y >> 3) * ZX_SCREEN_COLS + col];
    int ink_index, paper_index;
    ZXDecodeAttr(attr, &ink_index, &paper_index);
    glui32 ink   = ZXPalette[ink_index];
    glui32 paper = ZXPalette[paper_index];
    /* The flash bit (0x80) is ignored for a static title image. */
    int b = 0;
    while (b < 8)
    {
	int set = (bits >> (7 - b)) & 1;
	int start = b;
	while (b < 8 && (((bits >> (7 - b)) & 1) == set))
	    b++;
	glk_window_fill_rect(zx_gw, set ? ink : paper,
	    zx_xoff + (col * 8 + start) * zx_scale, zx_yoff + y * zx_scale,
	    (b - start) * zx_scale, zx_scale);
    }
}

/* Redraw the cell at a screen address: one bitmap byte, or (for an attribute
 * address) the whole 8x8 character it colours. */
static void redraw_zx_cell(const uchar *scr, int addr)
{
    if (addr < 0x5800)
    {
	draw_zx_byte(scr, addr);
	return;
    }
    int cell = addr - 0x5800;
    int crow = cell >> 5, ccol = cell & 0x1f;
    int py;
    for (py = 0; py < 8; py++)
    {
	int y = crow * 8 + py;
	draw_zx_byte(scr, 0x4000 + ZXBitmapOffset(y, ccol));
    }
}

static void draw_zx_screen(const uchar *scr)
{
    int y, col;
    if (!scr || !zx_gw)
	return;
    for (y = 0; y < ZX_SCREEN_HEIGHT; y++)
	for (col = 0; col < ZX_SCREEN_COLS; col++)
	    draw_zx_byte(scr, 0x4000 + ZXBitmapOffset(y, col));
}

/* Reveal the screen progressively, in the linear order a real Spectrum loaded
 * it from tape (bitmap top-to-bottom, then the attributes), on a timer. A
 * keypress dismisses it. */
#define ZX_REVEAL_TICK_MS 30
#define ZX_REVEAL_TICKS   120

static void zx_slow_reveal(winid_t keywin)
{
    static uchar work[ZX_SCREEN_SIZE];
    int counts[256] = { 0 };
    int i, fill = 0, per_tick, pos = 0;
    event_t ev;

    /* Start from the unrevealed background: black bitmap, attributes set to
     * the screen's most common fill byte. */
    for (i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
	counts[zxloadscreen[i]]++;
    for (i = 1; i < 256; i++)
	if (counts[i] > counts[fill])
	    fill = i;
    memset(work, 0x00, ZX_BITMAP_SIZE);
    memset(work + ZX_BITMAP_SIZE, (uchar)fill, ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);

    draw_zx_screen(work);

    per_tick = ZX_SCREEN_SIZE / ZX_REVEAL_TICKS;
    if (per_tick < 1)
	per_tick = 1;

    glk_request_char_event(keywin);
    glk_request_timer_events(ZX_REVEAL_TICK_MS);

    do {
	glk_select(&ev);
	if (ev.type == evtype_Timer)
	{
	    int end = pos + per_tick;
	    if (end > ZX_SCREEN_SIZE)
		end = ZX_SCREEN_SIZE;
	    for (; pos < end; pos++)
	    {
		work[pos] = zxloadscreen[pos];
		redraw_zx_cell(work, 0x4000 + pos);
	    }
	    if (pos >= ZX_SCREEN_SIZE)
		glk_request_timer_events(0);
	}
	else if (ev.type == evtype_Arrange)
	{
	    zx_layout();
	    draw_zx_screen(work);
	}
    } while (ev.type != evtype_CharInput);

    glk_request_timer_events(0);
}

/* If a usable loading screen was captured from the snapshot, show it in a
 * graphics window and wait for a keypress before starting the game. Replaces
 * mainwin for the duration and opens a fresh text window afterwards. */
static void show_zx_loadscreen(void)
{
    winid_t keywin;
    event_t ev;
    int slow = 0;

    if (!zxloadscreen)
	return;
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
    {
	free(zxloadscreen);
	zxloadscreen = NULL;
	return;
    }
#endif
    if (!glk_gestalt(gestalt_Graphics, 0) || ZXScreenIsBlackOnWhite(zxloadscreen))
    {
	free(zxloadscreen);
	zxloadscreen = NULL;
	return;
    }

    if (mainwin)
    {
	glk_window_close(mainwin, NULL);
	mainwin = NULL;
    }

    zx_gw = glk_window_open(0, 0, 0, wintype_Graphics, 0);
    if (zx_gw)
    {
	glk_window_set_background_color(zx_gw, 0x000000);
	zx_layout();

	/* Graphics windows can take key input directly on some libraries;
	 * otherwise borrow a one-line text window below for the keypress. */
	if (glk_gestalt(gestalt_GraphicsCharInput, 0))
	{
	    keywin = zx_gw;
	}
	else
	{
	    keywin = glk_window_open(zx_gw, winmethod_Below | winmethod_Fixed, 1,
				     wintype_TextBuffer, 0);
	    if (!keywin)
		keywin = zx_gw;
	}

#ifdef SPATTERLIGHT
	slow = gli_slowdraw && !gli_determinism;
#endif
	if (slow)
	{
	    zx_slow_reveal(keywin);
	}
	else
	{
	    draw_zx_screen(zxloadscreen);
	    glk_request_char_event(keywin);
	    do {
		glk_select(&ev);
		if (ev.type == evtype_Arrange)
		{
		    zx_layout();
		    draw_zx_screen(zxloadscreen);
		}
	    } while (ev.type != evtype_CharInput);
	}

	if (keywin != zx_gw)
	    glk_window_close(keywin, NULL);
	glk_window_close(zx_gw, NULL);
	zx_gw = NULL;
    }

    /* Back to a fresh text window for the game itself. */
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (!mainwin)
    {
	fprintf(stderr, "could not create glk window!\n");
	exit(1);
    }
    glk_set_window(mainwin);

    free(zxloadscreen);
    zxloadscreen = NULL;
}

/* --- The Illustrator location graphics ---------------------------------- *
 * Quill games drawn with Gilsoft's "The Illustrator" carry a vector picture
 * per location: a list of drawing commands (plot, line, fill, ...) that the
 * runtime walks to paint the screen. We decode that command list (the same
 * format UnQuill's listgfx() prints) into an offscreen framebuffer and blit it
 * to a graphics window above the text. The command set and the 0xFAB8 pointer
 * block were confirmed against Bored of the Rings by disassembling its renderer
 * (an 8-way jump table at 0xfbef, low 3 bits of each command = opcode). */

#define ILL_W    256
#define ILL_H    176	/* ZX plot area; y is measured up from the bottom */
#define ILL_CX   (ILL_W / 8)	/* attribute cells across (32) */
#define ILL_CY   (ILL_H / 8)	/* attribute cells down (22) */
#define ILL_BASE 0xFAB8	/* Spectrum Illustrator graphics pointer block */

/* We render the picture into a genuine Spectrum display file - a 6912-byte
 * buffer laid out exactly as the hardware's screen memory: a 6144-byte 1bpp
 * bitmap in the non-linear display order, followed by a 768-byte grid of 8x8
 * attribute cells (ink, paper, bright). Drawing this way (rather than a private
 * colour-per-pixel buffer) lets the picture share the loading-screen blit and,
 * crucially, lets the fill be a faithful port of the ROM routine that walks the
 * same memory. The blit composes bitmap and attributes like the hardware: a set
 * bit shows ink, a clear bit shows paper. */
static winid_t picwin = NULL;
static winid_t statuswin = NULL;	/* the Scott-style description grid */
static uchar  *picscr = NULL;	/* ZX_SCREEN_SIZE display file (bitmap + attrs) */
static int     pic_scale = 1, pic_xoff = 0, pic_yoff = 0;

/* Split-screen detection. Fullscreen Illustrator games paint the whole 176-pixel
 * plot area; later "split screen" games (e.g. Bugsy) only draw into a fixed band
 * at the top and leave the rest for text, which would otherwise render as a black
 * lower half. We measure how deep each picture actually draws (ill_bot_sy, the
 * lowest touched screen row of the current picture) and keep the session's
 * running maximum, rounded up to a character cell, as the window height
 * (ill_draw_h). It starts at 0 ("unknown" -> treat as full height) and only ever
 * grows, so the graphics window settles at the game's true picture-area height
 * without flicking shorter for sparsely drawn rooms. */
static int     ill_bot_sy = -1;	/* deepest screen row drawn by current picture */
static int     ill_draw_h = 0;	/* picture-area height in pixels (0 = unknown) */

static void ill_track(int y)	/* y is a plot row (0 = bottom) */
{
    int sy = ILL_H - 1 - y;	/* convert to a top-down screen row */
    if (sy >= 0 && sy < ILL_H && sy > ill_bot_sy)
	ill_bot_sy = sy;
}

/* The Illustrator plots in a 256x176 area with y measured up from the bottom;
 * the Spectrum screen is addressed top-down. These map a plot coordinate to a
 * display-file bitmap byte/bit and to its attribute byte. */
static int ill_bmoff(int x, int y)	/* y is a top-down screen row 0..191 */
{
    return ZXBitmapOffset(y, x >> 3);
}
static int ill_getpix(int x, int y)	/* y is a plot row (0 = bottom) */
{
    int sy = ILL_H - 1 - y;
    return (picscr[ill_bmoff(x, sy)] >> (7 - (x & 7))) & 1;
}
static void ill_putpix(int x, int y, int set)
{
    int sy = ILL_H - 1 - y, off = ill_bmoff(x, sy), bit = 0x80 >> (x & 7);
    if (set) picscr[off] |= (uchar)bit;
    else     picscr[off] &= (uchar)~bit;
    if (set) ill_track(y);
}
static uchar *ill_attrp(int x, int y)
{
    int sy = ILL_H - 1 - y;
    return &picscr[ZX_BITMAP_SIZE + (sy >> 3) * ZX_SCREEN_COLS + (x >> 3)];
}

/* Pen / attribute state while drawing one picture. */
static int pen_x, pen_y, pen_ink, pen_paper, pen_bright;

/* Per-component "transparent" flags, mirroring the Spectrum's MASK_T: INK/PAPER/
 * BRIGHT 8 means "leave this component of the cell unchanged". When a flag is
 * set, drawing into a cell keeps that component of the cell's existing
 * attribute (e.g. the Fag End recess fill, drawn after PAPER 8, keeps the white
 * wall paper and only lays down black ink). */
static int pen_ink_t, pen_paper_t, pen_bright_t;


/* GOSUB scale factor (the command's sc field, 0 = unscaled). When non-zero the
 * relative deltas of the sub-picture's commands are multiplied by sc/8, exactly
 * as the Spectrum's coordinate routine (0xff5c -> 0xff7c, which multiplies by
 * sc then shifts right three times) does, so scaled-down repeated sub-pictures
 * (e.g. the trees) come out the right size. */
static int ill_sc = 0;
static int ill_scaled(int v) { return ill_sc ? (v * ill_sc) >> 3 : v; }

/* Unit moves for RPLOT, indexed by bits 5-7 of the command byte. */
static const int ill_rmoves[8][2] = {
    {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
};

/* Stamp the current pen attribute onto the cell containing pixel (x,y), leaving
 * any component flagged transparent (INK/PAPER/BRIGHT 8) at its existing value -
 * exactly as the ROM's PO-ATTR combines ATTR_T through MASK_T. */
static void ill_setcell(int x, int y)
{
    uchar *a, v;
    if (x < 0 || x >= ILL_W || y < 0 || y >= ILL_H)
	return;
    a = ill_attrp(x, y);
    v = *a;
    if (!pen_ink_t)    v = (uchar)((v & ~0x07) | (pen_ink & 7));
    if (!pen_paper_t)  v = (uchar)((v & ~0x38) | ((pen_paper & 7) << 3));
    if (!pen_bright_t) v = (uchar)((v & ~0x40) | (pen_bright << 6));
    *a = v;
    ill_track(y);
}

/* Set (ink) or clear (paper) one pixel and stamp its cell's attribute. */
static void ill_plot(int x, int y, int set)
{
    if (x < 0 || x >= ILL_W || y < 0 || y >= ILL_H)
	return;
    ill_putpix(x, y, set);
    ill_setcell(x, y);
}

/* Draw a line exactly as the ZX ROM's DRAW-LINE routine (0x24BA) does, since
 * that is what The Illustrator calls: step the major axis every pixel, step the
 * minor axis when an error accumulator (initialised to major/2) overflows. This
 * reproduces the ROM's pixel choices so flood fills bound identically to the
 * original. The start point is assumed already plotted (as on the Spectrum). */
static void ill_line(int x0, int y0, int x1, int y1)
{
    int adx = abs(x1 - x0), ady = abs(y1 - y0);
    int ix = (x1 > x0) ? 1 : -1, iy = (y1 > y0) ? 1 : -1;
    int major, minor, err, k, x = x0, y = y0;

    if (adx == 0 && ady == 0)
	return;
    if (adx >= ady)		/* x is the major axis */
    {
	major = adx; minor = ady; err = major >> 1;
	for (k = 0; k < major; k++)
	{
	    err += minor;
	    if (err >= major) { err -= major; y += iy; }
	    x += ix;
	    ill_plot(x, y, 1);
	}
    }
    else			/* y is the major axis */
    {
	major = ady; minor = adx; err = major >> 1;
	for (k = 0; k < major; k++)
	{
	    err += minor;
	    if (err >= major) { err -= major; x += ix; }
	    y += iy;
	    ill_plot(x, y, 1);
	}
    }
}

/* Flood the connected region of clear pixels reachable from the seed, collecting
 * it into ill_region. Bounded only by set (ink) pixels - exactly as the ROM fill
 * (0xfc6c), which scans the bitmap and stops at set bits. Returns 0 (nothing to
 * do) if the seed itself is on a set pixel, as the ROM does. */
static uchar ill_region[ILL_W * ILL_H];
static int ill_flood(int x, int y)
{
    static int stack[ILL_W * ILL_H];
    int sp = 0;

    if (x < 0 || x >= ILL_W || y < 0 || y >= ILL_H)
	return 0;
    if (ill_getpix(x, y))		/* seed on a set pixel: nothing to do */
	return 0;

    memset(ill_region, 0, ILL_W * ILL_H);
    stack[sp++] = y * ILL_W + x;
    while (sp)
    {
	int p = stack[--sp];
	int px = p % ILL_W, py = p / ILL_W;
	if (ill_region[p] || ill_getpix(px, py))
	    continue;
	ill_region[p] = 1;
	if (px > 0         && !ill_region[p-1]     && !ill_getpix(px-1, py)   && sp < ILL_W*ILL_H-4) stack[sp++] = p-1;
	if (px < ILL_W - 1 && !ill_region[p+1]     && !ill_getpix(px+1, py)   && sp < ILL_W*ILL_H-4) stack[sp++] = p+1;
	if (py > 0         && !ill_region[p-ILL_W] && !ill_getpix(px,   py-1) && sp < ILL_W*ILL_H-4) stack[sp++] = p-ILL_W;
	if (py < ILL_H - 1 && !ill_region[p+ILL_W] && !ill_getpix(px,   py+1) && sp < ILL_W*ILL_H-4) stack[sp++] = p+ILL_W;
    }
    return 1;
}

/* FILL: flood the connected clear region and, exactly as the ROM does, set each
 * of its pixels (ink) while stamping the current attribute onto every cell it
 * touches. The pixel-level boundary gives smooth edges; the door interior stays
 * clear because its planks (set pixels drawn first) wall it off from the fill. */
static void ill_fill(int x, int y)
{
    int i;
    if (!ill_flood(x, y))
	return;
    for (i = 0; i < ILL_W * ILL_H; i++)
	if (ill_region[i])
	{
	    ill_setcell(i % ILL_W, i / ILL_W);
	    ill_putpix(i % ILL_W, i / ILL_W, 1);
	}
}

/* SHADE: flood the clear region, stamp the attribute, then set the pixels whose
 * dither bit is on. Each pixel picks bit ((x&1)<<2 | ((x>>1)&1)<<1 | (y&1)) of
 * the 8-bit pattern (the SHADE handler at 0xfe92), so set=ink shows through the
 * paper as a two-tone dither. */
static void ill_shade(int x, int y, int pattern)
{
    int i;
    if (!ill_flood(x, y))
	return;
    for (i = 0; i < ILL_W * ILL_H; i++)
	if (ill_region[i])
	{
	    int px = i % ILL_W, py = i / ILL_W;
	    int idx = ((px & 1) << 2) | (((px >> 1) & 1) << 1) | (py & 1);
	    ill_setcell(px, py);
	    if ((pattern >> idx) & 1)
		ill_putpix(px, py, 1);
	}
}

/* Decode and render one picture (a list of commands at gptr) into the bitmap.
 * `depth` guards against runaway GOSUB recursion. */
static void ill_render(ushort gfx_ptrs, ushort gptr, int depth)
{
    uchar gflag;
    int op, value, nargs, a0, a1;

    if (depth > 8)
	return;

    do
    {
	gflag = zmem(gptr);
	op    = gflag & 7;
	value = gflag >> 3;
	nargs = 0;

	switch (op)
	{
	case 0:	/* PLOT x,y  (over+inverse => AMOVE, absolute move only) */
	    nargs = 2;
	    pen_x = zmem(gptr + 1);
	    pen_y = zmem(gptr + 2);
	    if (!((gflag & 0x08) && (gflag & 0x10)))
		ill_plot(pen_x, pen_y, 1);
	    break;

	case 1:	/* LINE dx,dy  (over+inverse => MOVE only) */
	    nargs = 2;
	    a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
	    a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
	    if ((gflag & 0x08) && (gflag & 0x10))
	    {
		pen_x += a0; pen_y += a1;	/* MOVE */
	    }
	    else
	    {
		ill_line(pen_x, pen_y, pen_x + a0, pen_y + a1);
		pen_x += a0; pen_y += a1;
	    }
	    break;

	case 2:	/* FILL / BLOCK / SHADE / BSHADE */
	    switch (gflag & 0x30)
	    {
	    case 0x00:	/* FILL: flood at pen + (dx,dy). Does NOT move the pen -
			 * the ROM computes COORDS +/- args but never stores it back. */
		nargs = 2;
		a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
		a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
		ill_fill(pen_x + a0, pen_y + a1);
		break;
	    case 0x10:	/* BLOCK: recolour a rectangle of attribute cells. The ROM's
			 * handler (0xfd1a) writes only the attribute file, never the
			 * bitmap, so ink already plotted survives - e.g. the death
			 * screen washes the picture red over the ringwraith, which
			 * stays black on the new red paper. */
	    {
		/* args: height, width, start col, start row (all in 8x8 cells,
		 * rows numbered top-down as on the Spectrum's attribute file). */
		int bh = zmem(gptr + 1), bw = zmem(gptr + 2);
		int bcol = zmem(gptr + 3), brow = zmem(gptr + 4);
		int row, col;
		nargs = 4;
		for (row = brow; row <= brow + bh; row++)
		    for (col = bcol; col <= bcol + bw; col++)
			ill_setcell(col * 8, 175 - row * 8);
		break;
	    }
	    default:	/* SHADE / BSHADE: dither at pen + (dx,dy), again without
			 * moving the pen (so a run of shades stays anchored to the
			 * preceding MOVE rather than drifting). */
		nargs = 3;
		a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
		a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
		ill_shade(pen_x + a0, pen_y + a1, zmem(gptr + 3));
		break;
	    }
	    break;

	case 3:	/* GOSUB: draw sub-picture at scale sc. The pen is NOT restored
		 * afterwards - as on the Spectrum, the sub-picture leaves it at
		 * its last point, and the parent's next MOVE is relative to that. */
	{
	    int sub = zmem(gptr + 1), old_sc = ill_sc;
	    nargs = 1;
	    ill_sc = value & 7;
	    ill_render(gfx_ptrs, zword(gfx_ptrs + 2 * sub), depth + 1);
	    ill_sc = old_sc;
	    break;
	}

	case 4:	/* RPLOT: plot one step in a direction */
	    nargs = 0;
	    pen_x += ill_rmoves[(value >> 2) & 7][0];
	    pen_y += ill_rmoves[(value >> 2) & 7][1];
	    if (!((gflag & 0x08) && (gflag & 0x10)))
		ill_plot(pen_x, pen_y, 1);
	    break;

	case 5:	/* PAPER / BRIGHT. The colour value is 0-7 to set, 8 = transparent
		 * (leave unchanged), 9 = contrast; the engine forms it the same way
		 * the ZX PRINT control code does, so honour 8 instead of masking it
		 * down to 0 (PAPER 8 is how a fill keeps the paper it inherited). */
	    nargs = 0;
	    {
		int v = value & 0x0F;
		if (gflag & 0x80) { pen_bright_t = (v == 8); if (v < 8) pen_bright = v & 1; }
		else              { pen_paper_t  = (v == 8); if (v < 8) pen_paper  = v;     }
	    }
	    break;

	case 6:	/* INK / FLASH (flash ignored); INK 8 is transparent, as above. */
	    nargs = 0;
	    if (!(gflag & 0x80))
	    {
		int v = value & 0x0F;
		pen_ink_t = (v == 8);
		if (v < 8) pen_ink = v;
	    }
	    break;

	case 7:	/* END */
	    nargs = 0;
	    break;
	}

	gptr += nargs + 1;
    }
    while (op != 7);
}

/* Recompute scaling/centring of the picture window, and crop the window's
 * height so the image fills it exactly (no empty vertical band above or below
 * the picture). */
static void pic_layout(void)
{
    glui32 w = 0, h = 0;
    int sx, sy, target_h;
    int dh = (ill_draw_h > 0) ? ill_draw_h : ILL_H;	/* drawn picture height */
    winid_t parent;

    glk_window_get_size(picwin, &w, &h);
    sx = (int)w / ILL_W;
    sy = (int)h / dh;
    pic_scale = (sx < sy) ? sx : sy;
    if (pic_scale < 1)
	pic_scale = 1;

    /* Resize the window's height to the picture's exact rendered height (which,
     * for a split-screen game, is only the top band it actually draws into). */
    target_h = dh * pic_scale;
    parent = glk_window_get_parent(picwin);
    if (parent && target_h > 0 && (int)h != target_h)
    {
	glk_window_set_arrangement(parent,
	    winmethod_Above | winmethod_Fixed, (glui32)target_h, picwin);
	glk_window_get_size(picwin, &w, &h);
	/* The arrangement may have been clamped; honour the actual size. */
	sx = (int)w / ILL_W;
	sy = (int)h / dh;
	pic_scale = (sx < sy) ? sx : sy;
	if (pic_scale < 1)
	    pic_scale = 1;
    }

    pic_xoff = ((int)w - ILL_W * pic_scale) / 2;
    pic_yoff = ((int)h - dh * pic_scale) / 2;
    if (pic_xoff < 0) pic_xoff = 0;
    if (pic_yoff < 0) pic_yoff = 0;
}

/* Repaint the picture after a window resize (evtype_Arrange) or a redraw
 * request (evtype_Redraw). The Fixed cropping we leave behind on pic_layout
 * makes the window stop growing when the parent does, so we first reset the
 * arrangement to the proportional default to let the layout pick up the
 * parent's new available space, then re-layout (which crops down) and re-blit
 * from the stored bitmap. Mirrors the Updates() / Look() flow used by the
 * Scott, Taylor and Plus interpreters. */
static void pic_blit(void);

static void pic_relayout(void)
{
    winid_t parent;
    if (!picwin || !picscr)
	return;
    parent = glk_window_get_parent(picwin);
    if (parent)
	glk_window_set_arrangement(parent,
	    winmethod_Above | winmethod_Proportional, 55, picwin);
    pic_layout();
    pic_blit();
}

/* Resolve one pixel to a palette index by combining its bit with its cell's
 * attribute: a set bit shows ink, a clear bit shows paper, both shifted into
 * the bright half of the palette when the cell's bright flag is set. */
static int pic_pixel(int x, int y)
{
    uchar a = *ill_attrp(x, y);
    int bright = (a & 0x40) ? 8 : 0;
    return ill_getpix(x, y)
	 ? ((a & 7) + bright)		/* ink */
	 : (((a >> 3) & 7) + bright);	/* paper */
}

/* Blit the bitmap+attributes to the picture window (run-length per row; ZX y
 * is flipped). */
static void pic_blit(void)
{
    int sy, x;
    int dh = (ill_draw_h > 0) ? ill_draw_h : ILL_H;	/* drawn picture height */
    if (!picwin || !picscr)
	return;
    glk_window_clear(picwin);
    for (sy = 0; sy < dh; sy++)	/* only the band the picture draws into */
    {
	int y = ILL_H - 1 - sy;
	x = 0;
	while (x < ILL_W)
	{
	    int c = pic_pixel(x, y);
	    int start = x;
	    while (x < ILL_W && pic_pixel(x, y) == c)
		x++;
	    glk_window_fill_rect(picwin, ZXPalette[c],
		pic_xoff + start * pic_scale, pic_yoff + sy * pic_scale,
		(x - start) * pic_scale, pic_scale);
	}
    }
}

/* Draw the picture for location `loc`, if it has one, into a graphics window
 * above the text. Returns silently when graphics are unavailable or the game
 * has no Illustrator data. */
void draw_location_graphic(uchar loc)
{
    ushort gfx_ptrs, gfx_flags, gfx_count, gptr;
    uchar gflag;

    if (arch != ARCH_SPECTRUM)
	return;
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
	return;
#endif
    if (!glk_gestalt(gestalt_Graphics, 0))
	return;

    gfx_ptrs  = zword(ILL_BASE);
    gfx_flags = zword(ILL_BASE + 2);
    gfx_count = zmem(ILL_BASE + 6);
    if (gfx_count == 0 || loc >= gfx_count)
	return;
    /* Sanity-check the pointer block before trusting it. */
    if (gfx_ptrs < 0x5C00 || gfx_flags < 0x5C00)
	return;

    gptr = zword(gfx_ptrs + 2 * loc);

    /* A location with no picture has a record that is just END; tear down any
     * window left from a previous location so the text reclaims that space and
     * a subsequent resize cannot redraw the stale bitmap. */
    if ((zmem(gptr) & 7) == 7)
    {
	if (picwin)
	{
	    glk_window_close(picwin, NULL);
	    picwin = NULL;
	}
	return;
    }

    if (!picscr)
    {
	picscr = malloc(ZX_SCREEN_SIZE);
	if (!picscr)
	    return;
    }
    if (!picwin)
    {
	/* Put the picture at the very top, above the description grid (Scott /
	 * TaylorMade style): split the status window's parent so the picture
	 * sits over the whole [description grid + text buffer] stack rather
	 * than between them. */
	winid_t split = (statuswin && glk_window_get_parent(statuswin))
		      ? glk_window_get_parent(statuswin) : mainwin;
	picwin = glk_window_open(split,
		winmethod_Above | winmethod_Proportional, 55,
		wintype_Graphics, 0);
	if (!picwin)
	    return;
    }

    /* The picture starts from the location's flag attribute: ink and paper come
     * from it, but not bright (pictures render without bright unless a BRIGHT
     * command turns it on). INK/PAPER/BRIGHT commands change these as it draws. */
    gflag = zmem(gfx_flags + loc);
    pen_ink    = gflag & 7;
    pen_paper  = (gflag >> 3) & 7;
    pen_bright = 0;
    pen_ink_t = pen_paper_t = pen_bright_t = 0;
    pen_x = 0; pen_y = 0;
    ill_sc = 0;

    /* Clear the bitmap and set every cell to the location's initial attribute,
     * so the picture starts as a blank field of its paper colour. */
    memset(picscr, 0, ZX_BITMAP_SIZE);
    memset(picscr + ZX_BITMAP_SIZE,
	   (uchar)(pen_ink | (pen_paper << 3) | (pen_bright << 6)),
	   ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);

    ill_bot_sy = -1;			/* measure how deep this picture draws */
    ill_render(gfx_ptrs, gptr, 0);
    if (ill_bot_sy >= 0)
    {
	/* Grow the picture-area height to the deepest row drawn so far, rounded
	 * up to a character cell. Monotonic, so a split-screen game settles on
	 * its fixed top band and a fullscreen game expands to the full height. */
	int h = ((ill_bot_sy >> 3) + 1) << 3;
	if (h > ILL_H) h = ILL_H;
	if (h > ill_draw_h) ill_draw_h = h;
    }

    pic_layout();
    pic_blit();
}

/* --- C64 .T64 (crunched Quill tape image) loading ---------------------- *
 * A .T64 holds a crunched, self-extracting C64 program. We extract the first
 * file, decompress it with the shared unp64 library, then locate the Quill
 * database and shift the image so its header sits at 0x804 (where the running
 * C64 Quill engine keeps it, and where our C64 reader expects it). The
 * decompressed image's pointer tables already contain final runtime
 * addresses, so the game's own startup copy-up is reproduced by the shift. */

static long safe_word(uchar *img, long size, long idx)
{
    if (idx < 0 || idx + 1 >= size)
	return -1;
    return img[idx] | (img[idx + 1] << 8);
}

/* Decode the C64 Quill text record at image offset idx (bytes complemented,
 * terminated by the byte that decodes to zero) and return the percentage of
 * printable characters in [0,100], or -1 if it never terminates / is out of
 * range. */
static int c64_text_clean(uchar *img, long size, long idx)
{
    int len = 0, good = 0;
    if (idx < 0)
	return -1;
    while (idx < size && len < 1000)
    {
	uchar cch = (0xFF - img[idx++]) & 0xFF;
	if (cch == 0)
	    return len ? (good * 100 / len) : 100;
	cch &= 0x7F;
	if (cch == ' ' || (cch >= 9 && cch <= 13) || (cch >= 32 && cch < 127))
	    good++;
	len++;
    }
    return -1;
}

/* Scan a decompressed C64 image for the Quill database header. The header is
 * not necessarily at 0x804 in the freshly-decompressed image (the game copies
 * itself up at runtime), so we find the offset H whose header decodes valid
 * system messages and locations, and return the shift S = 0x804 - H that puts
 * the database at 0x804. Returns 1 and sets *shift on success. */
static int c64_find_db_shift(uchar *img, long size, long *shift)
{
    long H;
    for (H = 0; H < 0x2000; H++)
    {
	uchar car = img[H], nloc = img[H + 2], nmsg = img[H + 3], nsys = img[H + 4];
	long S = 0x804 - H;
	long loctop, msgtop, sysbase;
	int i, total, ok;

	if (!(car >= 1 && car <= 40 && nloc >= 8 && nmsg >= 8 &&
	      nsys >= 8 && nsys <= 64))
	    continue;
	loctop  = safe_word(img, size, H + 11);
	msgtop  = safe_word(img, size, H + 13);
	sysbase = safe_word(img, size, H + 15);
	if (loctop < 0x801 || msgtop < 0x801 || sysbase < 0x801)
	    continue;

	/* System messages 1..4 are fixed Quill strings; they must decode. */
	ok = 1;
	for (i = 1; i <= 4 && ok; i++)
	{
	    long ptr = safe_word(img, size, (sysbase - S) + 2 * i);
	    if (ptr < 0 || c64_text_clean(img, size, ptr - S) < 90)
		ok = 0;
	}
	if (!ok)
	    continue;

	/* Nearly all locations must decode cleanly (allow a few oddities). */
	total = 0;
	for (i = 0; i < nloc; i++)
	{
	    long ptr = safe_word(img, size, (loctop - S) + 2 * i);
	    if (ptr >= 0 && c64_text_clean(img, size, ptr - S) >= 90)
		total++;
	}
	if (total < nloc - nloc / 20)
	    continue;

	*shift = S;
	return 1;
    }
    return 0;
}

static void load_t64(void)
{
    long filelen, dataoff, datalen, addr, shift;
    ushort start;
    uchar *raw, *prg, *img;
    size_t outlen = 0;

    if (fseek(infile, 0, SEEK_END) || (filelen = ftell(infile)) < 0)
	die("Cannot read .t64 file '%s'.", inname);
    rewind(infile);

    raw = malloc(filelen);
    if (!raw || fread(raw, filelen, 1, infile) != 1)
	die("Cannot read .t64 file '%s'.", inname);
    if (filelen < 96 || memcmp(raw, "C64", 3) != 0)
	die("'%s' is not a C64 .t64 tape image.", inname);

    /* First directory entry is at offset 64: load address at +2, file data
     * offset (32-bit) at +8. */
    start   = raw[64 + 2] | (raw[64 + 3] << 8);
    dataoff = raw[64 + 8] | (raw[64 + 9] << 8) |
	      ((long)raw[64 + 10] << 16) | ((long)raw[64 + 11] << 24);
    if (dataoff < 96 || dataoff >= filelen)
	die("Bad file record in '%s'.", inname);
    datalen = filelen - dataoff;

    /* Rebuild a .PRG (2-byte load address + data) for unp64. */
    prg = malloc(datalen + 2);
    prg[0] = start & 0xFF;
    prg[1] = (start >> 8) & 0xFF;
    memcpy(prg + 2, raw + dataoff, datalen);
    free(raw);

    img = malloc(0x10000);
    memset(img, 0, 0x10000);
    if (!unp64(prg, datalen + 2, img, &outlen, NULL))
    {
	free(prg);
	free(img);
	die("Could not decompress the C64 program in '%s'.", inname);
    }
    free(prg);

    if (!c64_find_db_shift(img, 0x10000, &shift))
    {
	free(img);
	die("No C64 Quill database found in '%s'.", inname);
    }

    /* Lay the image down as C64 memory, shifted so the database is at 0x804. */
    arch     = ARCH_C64;
    mem_base = 0;
    mem_size = 0xFFFF;   /* mem_size is 16-bit; covers all RAM we touch (<0xD000) */
    dbver    = 5;
    memset(zxmemory, 0, sizeof(zxmemory));
    for (addr = 0x800; addr < 0x10000; addr++)
    {
	long si = addr - shift;
	if (si >= 0 && si < 0x10000)
	    zxmemory[addr] = img[si];
    }
    free(img);

    fprintf(stderr, ".t64 C64 Quill game loaded.\n");
}

/* Scan an absolute 64K Spectrum image for the Quill database signature (the
 * byte pairs 0x10,0x11,0x12,0x13,0x14,0x15 at even offsets). Returns its
 * address, or 0 if absent. Matches the search range used by the main loader. */
static ushort zx_img_signature(const uchar *img)
{
    ushort n;
    for (n = 0x5C00; n < 0xFFF5; n++)
        if (img[n  ] == 0x10 && img[n+2] == 0x11 &&
            img[n+4] == 0x12 && img[n+6] == 0x13 &&
            img[n+8] == 0x14 && img[n+10] == 0x15)
            return n;
    return 0;
}

/* Test whether a Quill database is self-consistent when the buffer 'buf'
 * (length 'len', signature at byte offset 'sigoff') is loaded at absolute
 * address 'base', interpreted with the early (late=0) or later (late=1) header
 * layout. The header stores absolute addresses for its tables, so only the
 * correct base makes every table pointer -- and every word-sized entry within
 * the location, object, message and connection tables -- fall inside the loaded
 * image. Returns 1 when all those pointers are in range. */
static int zx_quill_consistent(const uchar *buf, size_t len, size_t sigoff,
                               unsigned base, int late)
{
    unsigned end = base + (unsigned)len;
    size_t   zp  = sigoff + 13 + (late ? 1 : 0);   /* buffer offset of zxptr */
    unsigned nobj, nloc, nmsg, i;
    unsigned resptab, proctab, objtab, loctab, msgtab, conntab, vocab;

    #define W16(o)   ((unsigned)buf[(o)] | ((unsigned)buf[(o)+1] << 8))
    #define INIMG(a) ((unsigned)(a) >= base && (unsigned)(a) < end)

    if (sigoff + 34 >= len) return 0;              /* header must fit in buffer */

    nobj = buf[sigoff + 14];
    nloc = buf[sigoff + 15];
    nmsg = buf[sigoff + 16];
    if (!nobj || !nloc || !nmsg) return 0;

    resptab = W16(zp + 4);
    proctab = W16(zp + 6);
    objtab  = W16(zp + 8);
    loctab  = W16(zp + 10);
    msgtab  = W16(zp + 12);
    conntab = W16(zp + 14 + (late ? 2 : 0));
    vocab   = late ? W16(zp + 18) : W16(zp + 16);

    /* The header pointers, and the full extent of each word-pointer table,
     * must lie within the loaded image. */
    if (!INIMG(resptab) || !INIMG(proctab) || !INIMG(vocab))   return 0;
    if (!INIMG(objtab)  || !INIMG(objtab  + 2*nobj))           return 0;
    if (!INIMG(loctab)  || !INIMG(loctab  + 2*nloc))           return 0;
    if (!INIMG(msgtab)  || !INIMG(msgtab  + 2*nmsg))           return 0;
    if (!INIMG(conntab) || !INIMG(conntab + 2*nloc))           return 0;

    /* Each entry of those tables is itself a pointer into the image. */
    for (i = 0; i < nloc; i++) if (!INIMG(W16(loctab  - base + 2*i))) return 0;
    for (i = 0; i < nobj; i++) if (!INIMG(W16(objtab  - base + 2*i))) return 0;
    for (i = 0; i < nmsg; i++) if (!INIMG(W16(msgtab  - base + 2*i))) return 0;
    for (i = 0; i < nloc; i++) if (!INIMG(W16(conntab - base + 2*i))) return 0;

    return 1;

    #undef W16
    #undef INIMG
}

/* Reconstruct a Spectrum RAM image from the code blocks of a custom-loader tape
 * (one whose game data rides in headerless blocks, so the standard-header pass
 * placed nothing). The blocks are concatenated in tape order, then located by
 * trying each base from the top of RAM downward and accepting the first at
 * which the Quill database's own table pointers are self-consistent
 * (zx_quill_consistent). Returns 1 and copies the blocks into the absolute 64K
 * image 'img' on success. */
static int tzx_rebuild_custom(uchar *img, uchar *codebuf, size_t codelen)
{
    size_t   sigoff;
    unsigned base, top;
    int      late;

    if (codelen < 16 || codelen > 0xC000) return 0;  /* must fit RAM at 0x4000+ */

    /* Find the Quill signature within the concatenated code image. */
    for (sigoff = 0; sigoff + 11 < codelen; sigoff++)
        if (codebuf[sigoff  ] == 0x10 && codebuf[sigoff+2] == 0x11 &&
            codebuf[sigoff+4] == 0x12 && codebuf[sigoff+6] == 0x13 &&
            codebuf[sigoff+8] == 0x14 && codebuf[sigoff+10] == 0x15)
            break;
    if (sigoff + 11 >= codelen) return 0;            /* no database here */

    /* Highest base that still keeps the whole image inside the 64K address
     * space; walk downward and take the first self-consistent placement. */
    top = (unsigned)(0x10000 - codelen);
    for (base = top; base >= 0x4000; base--)
        for (late = 1; late >= 0; late--)
            if (zx_quill_consistent(codebuf, codelen, sigoff, base, late))
            {
                memcpy(img + base, codebuf, codelen);
                return 1;
            }
    return 0;
}

/* Load a ZX Spectrum .tzx tape image. Standard-loader games carry each CODE
 * block behind a tape header giving its load address; those are copied straight
 * into place, reconstructing the RAM image a real machine would hold after all
 * LOAD "" CODE calls. Custom turbo-loader games (e.g. Bugsy) instead store the
 * game as headerless blocks with a non-standard flag byte and no load address;
 * when the standard pass yields no Quill database, we fall back to
 * tzx_rebuild_custom, which self-locates the concatenated code blocks. A
 * loading screen (a ~6912-byte block) is captured into zxloadscreen. */
static void load_tzx(void)
{
    long filelen;
    uchar *raw;
    size_t pos;
    int pending_code = 0;     /* preceded by a standard CODE header        */
    int pending_basic = 0;    /* next data block is the BASIC loader        */
    ushort pending_addr = 0;
    uchar *img;               /* absolute 64K Spectrum image we assemble   */
    uchar *codebuf;
    size_t codelen = 0;

    if (fseek(infile, 0, SEEK_END) || (filelen = ftell(infile)) < 0)
        die("Cannot read .tzx file '%s'.", inname);
    rewind(infile);

    raw = malloc(filelen);
    if (!raw || fread(raw, filelen, 1, infile) != 1)
        die("Cannot read .tzx file '%s'.", inname);

    if (filelen < 10 || memcmp(raw, "ZXTape!\x1a", 8) != 0)
        die("'%s' is not a ZX Spectrum TZX tape image.", inname);

    arch = ARCH_SPECTRUM;

    img = calloc(0x10000, 1);
    /* Worst case the whole file is one code image; +2 guards the W16 reads. */
    codebuf = malloc((size_t)filelen + 2);
    if (!img || !codebuf) die("Out of memory loading '%s'.", inname);

    pos = 10; /* skip 10-byte TZX header */

    while (pos < (size_t)filelen)
    {
        uchar id = raw[pos++];
        size_t dlen = 0;
        uchar *data = NULL;

        switch (id)
        {
        case 0x10: /* Standard Speed Data Block: pause(2) + len(2) + data */
            if (pos + 4 > (size_t)filelen) goto tzx_done;
            dlen = raw[pos+2] | ((size_t)raw[pos+3] << 8);
            data = raw + pos + 4;
            pos += 4 + dlen;
            if (pos > (size_t)filelen) goto tzx_done;
            break;
        case 0x11: /* Turbo Speed Data Block: 15 params + len(3) + data */
            if (pos + 18 > (size_t)filelen) goto tzx_done;
            dlen = raw[pos+15] | ((size_t)raw[pos+16] << 8) | ((size_t)raw[pos+17] << 16);
            data = raw + pos + 18;
            pos += 18 + dlen;
            if (pos > (size_t)filelen) goto tzx_done;
            break;
        case 0x20: pos += 2;                           continue; /* Pause */
        case 0x21: if (pos < (size_t)filelen) pos += 1 + raw[pos]; continue; /* Group Start */
        case 0x22:                                     continue; /* Group End */
        case 0x24: pos += 2;                           continue; /* Loop Start */
        case 0x25:                                     continue; /* Loop End */
        case 0x2A: pos += 4;                           continue; /* Stop if 48K mode */
        case 0x2B: pos += 5;                           continue; /* Set Signal Level */
        case 0x30: if (pos < (size_t)filelen) pos += 1 + raw[pos]; continue; /* Text Description */
        case 0x31: if (pos + 1 < (size_t)filelen) pos += 2 + raw[pos+1]; continue; /* Message */
        case 0x32: /* Archive Info: len(2) + data */
            if (pos + 1 < (size_t)filelen) { size_t l = raw[pos] | ((size_t)raw[pos+1] << 8); pos += 2 + l; }
            continue;
        case 0x33: /* Hardware Type: count + count*3 bytes */
            if (pos < (size_t)filelen) pos += 1 + (size_t)raw[pos] * 3;
            continue;
        case 0x35: /* Custom Info Block: id(16) + len(4) + data */
            if (pos + 20 <= (size_t)filelen) {
                size_t l = raw[pos+16] | ((size_t)raw[pos+17] << 8) |
                           ((size_t)raw[pos+18] << 16) | ((size_t)raw[pos+19] << 24);
                pos += 20 + l;
            }
            continue;
        default:
            goto tzx_done; /* unknown block type: stop walking */
        }

        if (dlen < 2 || !data) { pending_code = 0; pending_basic = 0; continue; }

        if (data[0] == 0x00) /* flag 0x00 = standard header block */
        {
            /* Standard Spectrum tape header: flag(1) + type(1) + name(10) +
             * datalen(2) + param1/load-addr(2) + param2(2) + checksum(1) = 19 bytes */
            pending_code = pending_basic = 0;
            if (dlen >= 19 && data[1] == 0x03)        /* type 3 = CODE    */
            {
                pending_addr = data[14] | ((ushort)data[15] << 8);
                pending_code = 1;
            }
            else if (dlen >= 19 && data[1] == 0x00)   /* type 0 = PROGRAM */
                pending_basic = 1;                    /* its data is the loader */
        }
        else
        {
            uchar *payload = data + 1;   /* strip leading flag byte */
            size_t paylen  = dlen - 2;   /* strip trailing checksum */

            /* Capture the ZX loading screen (a ~6912-byte block; custom loaders
             * pad it slightly, so accept a small surplus). */
            if (!zxloadscreen && paylen >= ZX_SCREEN_SIZE &&
                paylen <= ZX_SCREEN_SIZE + 0x40)
            {
                zxloadscreen = malloc(ZX_SCREEN_SIZE);
                if (zxloadscreen)
                    memcpy(zxloadscreen, payload, ZX_SCREEN_SIZE);
            }

            if (data[0] == 0xFF && pending_code)      /* standard CODE data */
            {
                size_t n = paylen;
                if (n > (size_t)(0x10000 - pending_addr))
                    n = (size_t)(0x10000 - pending_addr);
                memcpy(img + pending_addr, payload, n);
            }
            else if (!pending_basic &&                /* skip the BASIC loader */
                     !(paylen >= ZX_SCREEN_SIZE && paylen <= ZX_SCREEN_SIZE + 0x40))
            {
                /* A headerless / non-standard data block: stash it as a
                 * candidate piece of a custom-loader code image. */
                memcpy(codebuf + codelen, payload, paylen);
                codelen += paylen;
            }
            pending_code = pending_basic = 0;
        }
    }

tzx_done:
    /* Standard-loader game? The CODE blocks are already in place. Otherwise
     * rebuild from the gathered custom-loader code blocks. */
    if (!zx_img_signature(img) && !tzx_rebuild_custom(img, codebuf, codelen))
    {
        free(img);
        free(codebuf);
        free(raw);
        die("No Quill database found in '%s' (unsupported tape loader?).",
            inname);
    }

    /* Hand the running RAM window (0x5C00..0xFFFF) to the interpreter exactly
     * as the .SNA path does, so the early/later database autodetection (which
     * relies on out-of-range reads below mem_base triggering a retry) works. */
    mem_base = 0x5C00;
    mem_size = 0xA400;
    memset(zxmemory, 0, sizeof(zxmemory));
    memcpy(zxmemory, img + mem_base, mem_size);

    free(img);
    free(codebuf);
    free(raw);
    glk_put_string(".tzx tape image loaded.\n");
}

void glk_main(void)
{
    ushort n, seekpos;
    ushort zxptr = 0;
    
    /* Set up GLK window. */
    
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (!mainwin)
    {
	fprintf(stderr, "could not create glk window!\n");
	exit(1);
    }
    glk_set_window(mainwin);
    
    /* Load the snapshot. To save space, we ignore the printer
    buffer and the screen, which can contain nothing of value. */

    arch       = ARCH_SPECTRUM;
    seekpos    = 0x1C1B;	/* Number of bytes to skip in the file */
    mem_base   = 0x5C00;	/* First address loaded */
    mem_size   = 0xA400;	/* Number of bytes to load from it */
    mem_offset = 0x3FE5;	/* Load address of snapshot in host system memory */

    /* .z80 snapshots are a different (and usually compressed) format, so
    they get their own loader. Everything else is a raw .SNA-style dump. */
    {
	size_t namelen = strlen(inname);

	if (namelen >= 4 && !strcasecmp(inname + namelen - 4, ".z80"))
	{
	    load_z80();
	}
	else if (namelen >= 4 && !strcasecmp(inname + namelen - 4, ".t64"))
	{
	    load_t64();
	}
	else if (namelen >= 4 && !strcasecmp(inname + namelen - 4, ".tzx"))
	{
	    load_tzx();
	}
	else
	{
	    /* << v0.7: Check for CPC6128 format */

	    if (fread(snapid, sizeof(snapid), 1, infile) != 1)
	    {
		die("Not in Spectrum, CPC or C64 snapshot format.");
	    }

	    /* CPCEMU snapshot */
	    if (!memcmp(snapid, "MV - SNA", 9))
	    {
		arch = ARCH_CPC;

		seekpos    = 0x1C00;
		mem_base   = 0x1B00;
		mem_size   = 0xA500;
		mem_offset = -0x100;
		dbver = 10;	/* CPC engine is equivalent to the "later" Spectrum one. */

		glk_put_string("CPC snapshot signature found.\n");
	    }

	    /* VICE snapshot */
	    if (!memcmp(snapid, "VICE Snapshot File\032", 19))
	    {
		arch = ARCH_C64;

		seekpos    =  0x874;
		mem_base   =  0x800;
		mem_size   = 0xA500;
		mem_offset =  -0x74;
		dbver = 5;	/* C64 engine is between the two Spectrum ones. */

		glk_put_string("C64 snapshot signature found.\n");
	    }

	    /* >> v0.7 */

	    /* Skip screen/printer buffer/registers and load the rest */

	    if (fseek(infile,seekpos,SEEK_SET))
	    {
		perror(inname);
		exit(1);
	    }

	    /* Load file */

	    if (fread(zxmemory, mem_size, 1, infile) != 1)
	    {
		perror (inname);
		exit(1);
	    }
	}
    }
    
    /* .SNA read ok. Find a Quill signature */
    
    switch(arch)
    {
	case ARCH_SPECTRUM:
	    
	    /* I could _probably_ assume that the Spectrum database is 
	    * always at one location for "early" and another for "late"
	    * games. But this method is safer. */
	    
	    for (n = 0x5C00; n < 0xFFF5; n++)
	    {
		if ((zmem(n  ) == 0x10) && (zmem(n+2) == 0x11) && 
		    (zmem(n+4) == 0x12) && (zmem(n+6) == 0x13) && 
		    (zmem(n+8) == 0x14) && (zmem(n+10) == 0x15))
		{
		    fprintf(stderr, "Quill signature found.\n");
		    found = 1;
		    zxptr = n + 13;
		    break;
		}
	    }
	    break;
	    
	case ARCH_CPC: 
	    found = 1;
	    zxptr = 0x1BD1;	/* From guesswork: CPC Quill files always seem to start at 0x1BD1 */
	    break;
	    
	case ARCH_C64: 
	    found = 1;
	    zxptr = 0x804;	/* From guesswork: C64 Quill files always seem to start at 0x804 */
	    break;
    }
    
    if (!found)
    {
	die("%s does not seem to be a valid Quill .SNA file."
	    "If you know it is, you have found a bug.", inname);
    }
  
    
    /* If we later bomb on an invalid address, come back here and try with the -l option */
    if (setjmp(myenv))
    {
	if (dbver == 10)
	{
	    die("Invalid address requested.");
	}
	else
	{
	    fprintf(stderr, "Retrying as a later version game.\n");
	    dbver = 10;
	}
    }
        
    /* Setup memory and tables */
    
    ucptr   = zxptr;
    maxcar1 = maxcar = zmem (zxptr);	/* Player's carrying capacity */
    nobj             = zmem (zxptr +  1);	/* No. of objects */
    nloc             = zmem (zxptr +  2);	/* No. of locations */
    nmsg             = zmem (zxptr +  3);	/* No. of messages */
    if (dbver)
    {
	++zxptr;
	nsys     = zmem (zxptr +  3);	/* No. of system messages */
	vocab    = zword(zxptr + 18);	/* Words list */
	dict     = zxptr + 29;		/* Expansion dictionary */
    }
    else vocab = zword(zxptr + 16);
    
    resptab            = zword(zxptr +  4);
    proctab            = zword(zxptr +  6);
    objtab             = zword(zxptr +  8);
    loctab             = zword(zxptr + 10);
    msgtab             = zword(zxptr + 12);
    if (dbver) sysbase = zword(zxptr + 14);
    else       sysbase = zword(23675) + 168; /* Just after the UDGs */ 
    conntab            = zword(zxptr + 14 + ( dbver ? 2 : 0));
    if(dbver) objmap   = zword(zxptr + 22);
    postab             = zword(zxptr + 18 + ( dbver ? 2 : 0 ));

    /* Show the loading-screen picture (from a .z80 snapshot), if any, before
     * the game text starts. */
    show_zx_loadscreen();

    /* Run the game */

    glk_put_string("\n");
    
    outfile = stderr;
    running = 1;
   
    ramsave[0x100] = 0; /* No position RAMsaved */
    
    while (running)
    {
	estop = 0;
    if (!gli_determinism)
        srand(time(NULL));
    else
        srand(1234);
	oopt  = 'T';        /* Outputs in plain text */
	initgame(zxptr); /* Initialise the game */
	playgame(zxptr); /* Play it */ 
	if (estop)
	{
	    estop=0;	/* Emergency stop operation, game restarts */
	    continue;   /* automatically */
	}
	sysmess(13);
	opch32('\n');
	if (yesno()=='N')
	{
	    running=0;
	    sysmess(14);
	}
    }
    
    glk_put_string("\n");
}



/* All Spectrum memory accesses are routed through this procedure.
* If TINY is defined, this accesses the .sna file directly.
*/
uchar zmem(ushort addr)
{
    if (addr < mem_base || (arch != ARCH_SPECTRUM &&
			    (addr >= (mem_base + mem_size))))
    {
	/* On the Spectrum this is normally just the early-format (dbver 0)
	 * database probe overrunning; we longjmp back and retry as a later
	 * game, so stay silent here. The alarming diagnostic is only useful
	 * for the non-recoverable cases (other architectures, or a Spectrum
	 * game that has already failed the retry, handled at the setjmp). */
	if (arch != ARCH_SPECTRUM)
	{
	    glk_printf("\n\n*** Invalid address %4.4x requested ***\n\n", addr);
	    glk_put_string("Probably not a Quilled game.\n");
	    glk_exit();
	}
	longjmp(myenv, 1);
    }
    
    return zxmemory[addr - mem_base];
}



ushort zword(ushort addr)
{
    return (ushort)(zmem(addr) + (256 * zmem(addr + 1)));
}



void dec32(ushort v)
{
    char decbuf[6];
    int i;
    
    sprintf(decbuf,"%d",v);
    i=0;
    while (decbuf[i]) opch32(decbuf[i++]);
}


/* --- Scott-style status window ------------------------------------------- *
 * The location description and the list of objects present are drawn into a
 * text-grid window above the main buffer, ScottFree-style. Quill location
 * texts are full prose paragraphs of varying length, so the grid cannot be a
 * fixed height: each turn we capture the description text, word-wrap it to the
 * grid width to learn how many rows it needs, resize the grid to fit, and then
 * print the wrapped lines into it. Command input and responses keep scrolling
 * in the main buffer below. */


/* While capturing, description output produced through put_ch()/put_str() (and
 * therefore through opch32(), oneitem() and listat()) is collected into capbuf
 * instead of being sent to a window, ready to be laid out in the grid. */
static int    status_capture = 0;
/* Set during status_begin/status_end while at the title screen (CURLOC==0).
 * In this mode put_ch / opch32 bypass the line-reformatter (no leading-space
 * collapse, no auto-wrap at column 31), and expch maps 0x06 to '\n' instead
 * of padding spaces, so capbuf preserves the original line layout. */
int           title_capture = 0;
static char  *capbuf = NULL;
static size_t caplen = 0, capcap = 0;

static void cap_putc(char c)
{
    if (caplen + 1 >= capcap)
    {
	size_t ncap = capcap ? capcap * 2 : 256;
	char *nb = realloc(capbuf, ncap);
	if (!nb)
	    return;
	capbuf = nb;
	capcap = ncap;
    }
    capbuf[caplen++] = c;
}

/* Routed output primitives: when capturing, divert to capbuf; otherwise behave
 * exactly like the underlying Glk calls. opch32() and listat() use these so
 * that normal (non-description) output is unaffected. */
void put_ch(char c)
{
    if (status_capture)
	cap_putc(c);
    else
	glk_put_char(c);
}

void put_str(char *s)
{
    if (status_capture)
	while (*s)
	    cap_putc(*s++);
    else
	glk_put_string(s);
}

void status_begin(void)
{
    caplen = 0;
    status_capture = 1;
    /* Location 0 holds the title screen on most Quill games; capture it with
     * line breaks and indentation preserved so status_render can centre each
     * line rather than reflowing it as a paragraph. */
    title_capture = (CURLOC == 0);
}

/* Greedy word-wrap of text[0..len) to `width` columns, returning a malloc'd
 * array of malloc'd line strings (count in *out_n). Explicit newlines force a
 * break; leading spaces of a logical line are preserved so the indented object
 * list keeps its indentation. */
static char **push_line(char **lines, int *n, int *cap, char *cur, int col)
{
    char *copy = malloc((size_t)col + 1);
    if (copy)
    {
	memcpy(copy, cur, (size_t)col);
	copy[col] = '\0';
    }
    if (*n >= *cap)
    {
	*cap = *cap ? *cap * 2 : 8;
	lines = realloc(lines, (size_t)*cap * sizeof(char *));
    }
    lines[(*n)++] = copy;
    return lines;
}

#define PUSH() do { lines = push_line(lines, &nlines, &caplines, cur, col); \
		    col = 0; } while (0)

static char **wrap_text(const char *text, size_t len, int width, int *out_n)
{
    char **lines = NULL;
    int    nlines = 0, caplines = 0;
    char  *cur = malloc((size_t)width + 1);
    int    col = 0;
    size_t i = 0, j;
    int    wl;

    if (!cur)
    {
	*out_n = 0;
	return NULL;
    }

    while (i < len)
    {
	char c = text[i];

	if (c == '\n')
	{
	    PUSH();
	    i++;
	    continue;
	}

	if (c == ' ')
	{
	    /* Leading spaces (indentation) are emitted directly; spaces inside
	     * a line become candidate break points handled with the next word. */
	    if (col == 0)
	    {
		if (col < width)
		    cur[col++] = ' ';
		i++;
		continue;
	    }
	    /* collapse runs of spaces to a single pending separator */
	    while (i < len && text[i] == ' ')
		i++;
	    /* measure the following word */
	    j = i;
	    wl = 0;
	    while (j < len && text[j] != ' ' && text[j] != '\n')
	    {
		wl++;
		j++;
	    }
	    if (wl == 0)
		continue;
	    if (col + 1 + wl > width)
		PUSH();		/* word won't fit after a space: wrap */
	    else if (col < width)
		cur[col++] = ' ';
	    continue;
	}

	/* a word character: copy the whole word, hard-splitting if it alone is
	 * wider than the grid */
	j = i;
	wl = 0;
	while (j < len && text[j] != ' ' && text[j] != '\n')
	{
	    wl++;
	    j++;
	}
	if (wl > width && col == 0)
	{
	    while (i < j)
	    {
		if (col == width)
		    PUSH();
		cur[col++] = text[i++];
	    }
	    continue;
	}
	if (col + wl > width && col > 0)
	    PUSH();
	while (i < j && col < width)
	    cur[col++] = text[i++];
	i = j;		/* in case the word was clipped at width */
    }
    if (col > 0)
	PUSH();

    free(cur);
    *out_n = nlines;
    return lines;
}

#undef PUSH

/* Word-wrap the currently-captured description in capbuf to the status grid's
 * current width and print it. Factored out of status_end so the same code can
 * reflow the description on a window resize (status_relayout). Returns 0 if
 * the grid is unusable (no width); the caller can then decide whether to fall
 * back to inline printing. */
/* Split capbuf on '\n' into one malloc'd string per line, preserving leading
 * and trailing whitespace. Used by status_render's title-screen path so the
 * original line layout survives into rendering without word-wrap. */
static char **split_capbuf_lines(int *out_n)
{
    int    nlines = 1;
    size_t i, start;
    char **lines;
    int    li;

    for (i = 0; i < caplen; i++)
	if (capbuf[i] == '\n')
	    nlines++;

    lines = malloc((size_t)nlines * sizeof(char *));
    if (!lines)
    {
	*out_n = 0;
	return NULL;
    }
    li = 0;
    start = 0;
    for (i = 0; i <= caplen; i++)
    {
	if (i == caplen || capbuf[i] == '\n')
	{
	    size_t len = i - start;
	    char  *ln  = malloc(len + 1);
	    if (ln)
	    {
		memcpy(ln, capbuf + start, len);
		ln[len] = '\0';
	    }
	    lines[li++] = ln;
	    start = i + 1;
	}
    }
    *out_n = nlines;
    return lines;
}

static int status_render(void)
{
    glui32 width = 0, height = 0;
    char **lines;
    int    nlines = 0, row;
    int    title_mode = (CURLOC == 0);

    if (!statuswin || !capbuf || caplen == 0)
	return 0;

    glk_window_get_size(statuswin, &width, &height);
    if (width == 0)
	return 0;

    if (title_mode)
	lines = split_capbuf_lines(&nlines);
    else
	lines = wrap_text(capbuf, caplen, (int)width, &nlines);
    if (nlines < 1)
	nlines = 1;

    /* The captured description ends with paragraph-break newlines, which
     * wrap into trailing blank lines; drop them so the delimiter sits flush
     * against the actual text rather than a row below it. */
    while (nlines > 1)
    {
	char *ln = lines[nlines - 1];
	int   blank = 1;
	if (ln)
	    for (char *p = ln; *p; p++)
		if (*p != ' ')
		{
		    blank = 0;
		    break;
		}
	if (!blank)
	    break;
	free(lines[nlines - 1]);
	nlines--;
    }

    /* Reserve one extra row at the bottom for a TaylorMade-style underline
     * separating the room description from the scrolling buffer below. */
    glk_window_set_arrangement(glk_window_get_parent(statuswin),
			       winmethod_Above | winmethod_Fixed,
			       (glui32)(nlines + 1), statuswin);

    glk_window_clear(statuswin);
    glk_set_window(statuswin);
    /* The arrangement change may have nudged the grid's width; query again so
     * the title-screen centring (and the underline below) uses the actual
     * width of the grid we ended up with. */
    glk_window_get_size(statuswin, &width, &height);
    for (row = 0; row < nlines; row++)
    {
	char *ln = lines[row];
	if (title_mode && ln)
	{
	    /* Centre the line by stripping its existing leading/trailing
	     * spaces and computing a left margin against the current grid
	     * width. Preserves the original line breaks (no word-wrap). */
	    size_t ln_len = strlen(ln);
	    size_t lead   = 0;
	    while (lead < ln_len && ln[lead] == ' ')
		lead++;
	    while (ln_len > lead && ln[ln_len - 1] == ' ')
		ln_len--;
	    size_t content = ln_len - lead;
	    int col = ((int)width - (int)content) / 2;
	    if (col < 0)
		col = 0;
	    glk_window_move_cursor(statuswin, (glui32)col, (glui32)row);
	    for (size_t j = 0; j < content; j++)
		glk_put_char(ln[lead + j]);
	}
	else
	{
	    glk_window_move_cursor(statuswin, 0, (glui32)row);
	    if (ln)
		glk_put_string(ln);
	}
	free(ln);
    }
    free(lines);

    glk_window_move_cursor(statuswin, 0, (glui32)nlines);
    for (row = 0; row < (int)width; row++)
	glk_put_char('_');

    glk_set_window(mainwin);
    return 1;
}

void status_end(void)
{
    glui32 width = 0, height = 0;

    status_capture = 0;
    xpos = 0;		/* buffer output that follows starts at column 0 */

    /* Open the grid lazily, split above the main buffer. */
    if (!statuswin && mainwin)
	statuswin = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed,
				    1, wintype_TextGrid, 0);

    if (statuswin)
	glk_window_get_size(statuswin, &width, &height);

    /* No usable grid (e.g. the stdio CLI harness reports width 0): fall back to
     * printing the description inline in the buffer, as before. */
    if (!statuswin || width == 0)
    {
	if (capbuf && caplen)
	{
	    cap_putc('\0');
	    glk_put_string(capbuf);
	    glk_put_char('\n');
	}
	return;
    }

    status_render();
}

/* Reflow the previously-captured room description into the status grid after a
 * window resize. capbuf/caplen still hold the last description (status_begin
 * only resets caplen at the start of a turn), so a fresh word-wrap at the new
 * grid width reproduces the layout. */
static void status_relayout(void)
{
    status_render();
}

/* Output a character, assuming 32-column screen */
void opch32(char ch)
{
    /* Title screen (location 0): pass characters through to capbuf unchanged
     * so the original line layout and indentation are preserved. On Spectrum
     * expch maps 0x06 to '\n', so each source paragraph already becomes one
     * line in capbuf. C64 titles have no newline codes at all (the text is
     * padded out to fill each 40-column row), so we emit a '\n' once xpos
     * reaches 40 to reproduce the original C64 screen's row breaks. */
    if (title_capture && status_capture)
    {
	if (ch == '\n')
	{
	    put_ch('\n');
	    xpos = 0;
	}
	else if (ch == 8)
	{
	    if (xpos > 0) xpos--;
	}
	else
	{
	    put_ch(ch);
	    xpos++;
	}
	if (arch == ARCH_C64 && xpos >= 40)
	{
	    put_ch('\n');
	    xpos = 0;
	}
	return;
    }

#if 0 // original 32/40 column screen exact replica output
    glk_put_char(ch);
    if (ch == '\n') xpos = 0;
    else if (ch == 8) xpos--;
    else if (arch == ARCH_SPECTRUM && xpos == 31) opch32('\n');
    else if (arch != ARCH_SPECTRUM && xpos == 39) opch32('\n');
    else xpos++;
    
#else // reformatted ...
    
    static int space = 0;
    static int linefeed = 0;
    
    /* inhibit multiple spaces and linefeeds and spaces at the beginning of a line */
    if (!(space && ch == ' ') && !(linefeed && ch == '\n') && !(xpos == 0 && ch == ' '))
	put_ch(ch);
    
    /* count spaces */
    if (ch == ' ')
	space = 1;
    else
	space = 0;
    
    if (ch == '\n')
    {
	if (linefeed == 0)
	    put_ch('\n'); // an extra paragraph breaking space
	if (linefeed == 5)
	    put_ch('\n'); // a clear screen really ... just add some more space
	linefeed ++;
	xpos = 0;
    }
     
    else if (ch == 8)
    {
	xpos--;
    }
    
    /* autowrap ... in the original output  */
    else if ((arch == ARCH_SPECTRUM && xpos == 31) ||
	     (arch != ARCH_SPECTRUM && xpos == 39))
    {
	/* ended line directly after a word, so we need an extra space */
	if (!space)
	    put_ch(' ');
	xpos = 0;
    }
    
    else
    {
	linefeed = 0;
	xpos++;
    }
    
#endif
}

/* Set once the player has pressed a key during a pause, to skip the rest of a
 * pause sequence (such as a long scripted intro) up to the next command
 * prompt. Reset by myreadline() at the start of each turn. */
static int skip_pauses = 0;

/* Pause for the given number of milliseconds, yielding to the Glk event loop
 * so the display keeps updating, instead of busy-waiting on the system clock.
 * Like the Delay() helper used by the Scott and Taylor interpreters it honours
 * the user's "delays" preference and libraries without timer support (such as
 * the stdio CLI harness) return at once; in addition a keypress cuts the pause
 * short and skips any remaining pauses until the next command is entered. */
void do_pause(glui32 millisecs)
{
    event_t ev;

    if (millisecs == 0 || skip_pauses)
	return;
#ifdef SPATTERLIGHT
    if (!gli_sa_delays)
	return;
#endif
    if (!glk_gestalt(gestalt_Timer, 0))
	return;

    glk_request_timer_events(millisecs);
    glk_request_char_event(mainwin);
    do {
	glk_select(&ev);
	if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
	{
	    pic_relayout();
	    status_relayout();
	}
    } while (ev.type != evtype_Timer && ev.type != evtype_CharInput);
    glk_request_timer_events(0);
    if (ev.type == evtype_CharInput)
	skip_pauses = 1;	/* skip the rest of this pause sequence */
    else
	glk_cancel_char_event(mainwin);
}

int getch(void)
{
    event_t event;

    glk_request_char_event(mainwin);

    while (1)
    {
	glk_select(&event);
	if (event.type == evtype_Arrange || event.type == evtype_Redraw)
	{
	    pic_relayout();
	    status_relayout();
	}
	if (event.type == evtype_CharInput)
	    break;
    }

    return event.val1;
}

void myreadline(char *buf, int cap)
{
    event_t ev;

    skip_pauses = 0;	/* a new turn re-enables scripted pauses */

    if (xpos == 0)
	glk_put_string("> ");

    glk_request_line_event(mainwin, buf, cap, 0);

    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
	{
	    pic_relayout();
	    status_relayout();
	}
	if (ev.type == evtype_LineInput)
	    break;
    }

    /* The line we have received in commandbuf is not null-terminated */
    buf[ev.val1] = '\0';	/* i.e., the length */
}
