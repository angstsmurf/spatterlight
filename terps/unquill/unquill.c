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
 * can be shown as a title image before the game starts. NULL when absent. */
#define ZX_SCREEN_SIZE   6912
#define ZX_BITMAP_SIZE   6144
#define ZX_SCREEN_WIDTH  256
#define ZX_SCREEN_HEIGHT 192
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

/* Standard ZX Spectrum palette: 8 normal colours then their 8 bright variants. */
static const glui32 zxpalette[16] = {
    0x000000, 0x0000D7, 0xD70000, 0xD700D7, 0x00D700, 0x00D7D7, 0xD7D700, 0xD7D7D7,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF
};

static winid_t zx_gw = NULL;            /* The title graphics window */
static int zx_scale = 1, zx_xoff = 0, zx_yoff = 0;

/* A snapshot captured at a text prompt has no picture: every attribute cell
 * holds the default black ink on white paper (0x38, ignoring bright/flash).
 * Such a screen isn't worth showing as a title. */
static int zx_screen_blank(const uchar *scr)
{
    int i;
    if (!scr)
	return 1;
    for (i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
	if ((scr[i] & 0x3f) != 0x38)
	    return 0;
    return 1;
}

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
    int y = ((offset & 0x0700) >> 8) | ((offset & 0x00e0) >> 2) | ((offset & 0x1800) >> 5);
    uchar bits = scr[offset];
    uchar attr = scr[ZX_BITMAP_SIZE + (y >> 3) * 32 + col];
    int bright = (attr & 0x40) ? 8 : 0;
    glui32 ink   = zxpalette[(attr & 0x07) + bright];
    glui32 paper = zxpalette[((attr >> 3) & 0x07) + bright];
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
	int offset = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8) | ccol;
	draw_zx_byte(scr, 0x4000 + offset);
    }
}

static void draw_zx_screen(const uchar *scr)
{
    int y, col;
    if (!scr || !zx_gw)
	return;
    for (y = 0; y < ZX_SCREEN_HEIGHT; y++)
    {
	int bitmap_row = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8);
	for (col = 0; col < 32; col++)
	    draw_zx_byte(scr, 0x4000 + bitmap_row + col);
    }
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
    if (!glk_gestalt(gestalt_Graphics, 0) || zx_screen_blank(zxloadscreen))
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

    glk_put_string(".t64 C64 Quill game loaded.\n");
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
		    glk_put_string("Quill signature found.\n");
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
	    glk_put_string("Retrying as a later version game.\n");
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

static winid_t statuswin = NULL;

/* While capturing, description output produced through put_ch()/put_str() (and
 * therefore through opch32(), oneitem() and listat()) is collected into capbuf
 * instead of being sent to a window, ready to be laid out in the grid. */
static int    status_capture = 0;
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

void status_end(void)
{
    glui32 width = 0, height = 0;
    char **lines;
    int    nlines = 0, row;

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
    for (row = 0; row < nlines; row++)
    {
	glk_window_move_cursor(statuswin, 0, (glui32)row);
	glk_put_string(lines[row]);
	free(lines[row]);
    }
    free(lines);

    glk_window_move_cursor(statuswin, 0, (glui32)nlines);
    for (row = 0; row < (int)width; row++)
	glk_put_char('_');

    glk_set_window(mainwin);
}

/* Output a character, assuming 32-column screen */
void opch32(char ch)
{
    /* C64 Quill games lay their text out as hard 40-column lines padded with
     * spaces (there are no newline codes in the text), so the line breaks come
     * from the screen wrapping at column 40. Reproduce that column layout
     * rather than reflowing into paragraphs, which would run every line
     * together. */
    if (arch == ARCH_C64)
    {
	if (ch == 8)
	{
	    if (xpos > 0) xpos--;
	    return;
	}
	put_ch(ch);
	if (ch == '\n')
	    xpos = 0;
	else if (xpos >= 39)
	{
	    put_ch('\n');
	    xpos = 0;
	}
	else
	    xpos++;
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

/* Pause for the given number of milliseconds, yielding to the Glk event loop
 * so the display keeps updating, instead of busy-waiting on the system clock.
 * Mirrors the Delay() helper used by the Scott and Taylor interpreters: it
 * honours the user's "delays" preference, and libraries without timer support
 * (such as the stdio CLI harness) simply return at once. */
void do_pause(glui32 millisecs)
{
    event_t ev;

    if (millisecs == 0)
	return;
#ifdef SPATTERLIGHT
    if (!gli_sa_delays)
	return;
#endif
    if (!glk_gestalt(gestalt_Timer, 0))
	return;

    glk_request_timer_events(millisecs);
    do {
	glk_select(&ev);
    } while (ev.type != evtype_Timer);
    glk_request_timer_events(0);
}

int getch(void)
{
    event_t event;

    glk_request_char_event(mainwin);

    while (1)
    {
	glk_select(&event);
	if (event.type == evtype_CharInput)
	    break;
    }
    
    return event.val1;
}

void myreadline(char *buf, int cap)
{
    event_t ev;
    
    if (xpos == 0)
	glk_put_string("> ");
    
    glk_request_line_event(mainwin, buf, cap, 0);

    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_LineInput)
	    break;
    }
    
    /* The line we have received in commandbuf is not null-terminated */
    buf[ev.val1] = '\0';	/* i.e., the length */
}
