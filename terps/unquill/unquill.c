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
#include "zx_title.h"   /* shared ZX loading-screen title renderer */

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

__attribute__((noreturn)) void die(char *fmt, ...)
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

/* --- Transcript (scripting) --------------------------------------------- *
 * The "#transcript" meta-command toggles a running transcript of the session.
 * We use mainwin's echo stream: while it is set, the Glk library copies
 * everything printed to the window — and the echo of every command the player
 * types — to the stream, which is all a transcript needs. mainwin is opened
 * once (before the play loop) and never replaced during play, so the echo
 * stream survives "play again" and undo-after-death without re-attaching. */
static strid_t scriptstr = NULL;

void script_toggle(void)
{
    frefid_t fref;

    if (scriptstr)		/* currently scripting: stop */
    {
	/* Printed before detaching so it lands in the transcript itself. */
	glk_put_string("\n[Transcript stopped.]\n");
	glk_window_set_echo_stream(mainwin, NULL);
	glk_stream_close(scriptstr, NULL);
	scriptstr = NULL;
	return;
    }

    fref = glk_fileref_create_by_prompt(fileusage_Transcript | fileusage_TextMode,
					filemode_Write, 0);
    if (!fref)			/* user cancelled the file prompt */
	return;
    scriptstr = glk_stream_open_file(fref, filemode_Write, 0);
    glk_fileref_destroy(fref);
    if (!scriptstr)
    {
	glk_put_string("\n[Could not start a transcript.]\n");
	return;
    }
    glk_window_set_echo_stream(mainwin, scriptstr);
    /* Printed after attaching so the transcript opens with this marker. */
    glk_put_string("\n[Transcript started.]\n");
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

    if (fseek(infile, 0, SEEK_END) || (filelen = ftell(infile)) < 0 ||
        fseek(infile, 0, SEEK_SET))
        die("Cannot read .z80 file '%s'.", inname);

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
 * The scaled/centred drawing and the slow reveal live in the shared ZXTitle
 * renderer (zx_title.h); here we just set up the window, hand it the captured
 * SCREEN$ with unquill's palette, and tear the window down afterward. */

/* If a usable loading screen was captured from the snapshot, show it in a
 * graphics window and wait for a keypress before starting the game. Replaces
 * mainwin for the duration and opens a fresh text window afterwards. */
static void show_zx_loadscreen(void)
{
    winid_t zx_gw, keywin;
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
	glk_window_clear(zx_gw);

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
	/* unquill uses the authentic ZX RGB palette and sits the picture a
	 * little above centre (vcentre divisor 3); a NULL order reveals it
	 * linearly, top to bottom, as a real Spectrum loaded from tape. */
	ZXTitle title = { zx_gw, ZXPalette, 3, 0, 0, 0 };
	ZXTitleShow(&title, zxloadscreen, keywin, slow, NULL, 0);

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

/* The Illustrator location graphics (vector picture decoding and the Glk
 * graphics-window plumbing) live in illustrator.c. draw_location_graphic() and
 * pic_relayout() are declared in unquill.h; statuswin (defined with the status
 * window code below) is shared with that file. */

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
	undo_reset();	/* a brand-new game has no undo history */
	initgame(zxptr); /* Initialise the game */
resume:
	playgame(zxptr); /* Play it */
	if (estop)
	{
	    estop=0;	/* Emergency stop operation, game restarts */
	    continue;   /* automatically */
	}
	sysmess(13);
	opch32('\n');
	/* The game's "would you like to play again?" message is the first prompt
	 * after death; besides yes/no it also accepts "#undo", which rolls back
	 * to the move before the game ended and resumes the same game without
	 * re-initialising (and so without replaying any intro). */
	switch (end_game_prompt())
	{
	case END_UNDO:
	    goto resume;
	case END_QUIT:
	    running = 0;
	    sysmess(14);
	    break;
	case END_AGAIN:
	default:
	    break;	/* loop round and start a fresh game */
	}
    }

    /* Flush and close any open transcript before we leave. */
    if (scriptstr)
    {
	glk_window_set_echo_stream(mainwin, NULL);
	glk_stream_close(scriptstr, NULL);
	scriptstr = NULL;
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

/* The Scott-style description grid. Shared with illustrator.c, which splits this
 * window's parent to place the picture window above the whole text stack. */
winid_t statuswin = NULL;

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
