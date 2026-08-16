/*----------------------------------------------------------------------*\

  glkmedia.c

  Map the DOS helper commands Alan 2 games run through the SYSTEM
  instruction onto Glk images and sound channels. The reference game is
  The Hollywood Murders (Michael Zerbo, 1996), which shells out to

    viewer <name>.pcx    Viewer 2.2, a VESA SVGA image viewer: flip the
                         whole screen to the picture, wait for a key,
                         restore text mode
    showjpg <name>.jpg   Jan Patera's ShowJPG, the same thing for JPEGs.
                         A Matter of Time 1.2 (1995, re-released 2003)
                         re-rendered its pictures as JPEG and switched to
                         it; everything else about the idiom is unchanged
    sbplay <name>.wav /s SBPlay 2.11, a Sound Blaster sample player that
                         blocks until the sample has finished
    pause                COMMAND.COM's "Press any key to continue . . .",
                         used to hold the text before the flip to graphics
                         and so ignored here -- see glkmedia_system()

  Media files live next to the game. On-disk case does not match the
  command strings (DOS did not care), and two of the shareware files lie
  about their content: title.pcx is IFF, nuthin.wav is plain text. So
  lookup is case-insensitive and format detection goes by file magic,
  exactly as the DOS helpers did. Files absent from the shareware build
  must degrade to silence, with no error text.

\*----------------------------------------------------------------------*/

#ifdef SPATTERLIGHT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>

#include "glkimp.h"             /* win_loadimage(), win_loadsound(),
                                   gli_enable_graphics, gli_enable_sound */

#include "types.h"
#include "main.h"               /* advnam, para(), newline() */
#include "glkio.h"              /* glkMainWin */
#include "glkmedia.h"

#define MAX_ENTRIES 128
#define MAX_NAME 64
#define NUM_SOUND_CHANNELS 3

typedef enum { MEDIA_PICTURE, MEDIA_SOUND } MediaKind;

typedef struct {
    MediaKind kind;
    char name[MAX_NAME];        /* name from the command string, lowercased */
    char *path;                 /* resolved on-disk path, NULL if absent */
    int resno;                  /* Glk resource number, 0 = not registered */
    Boolean broken;             /* file exists but is not usable */
} MediaEntry;

static MediaEntry entries[MAX_ENTRIES];
static int numentries = 0;
static int nextpicres = 1;
static int nextsndres = 1;

/* SBPLAY.EXE blocked, so consecutive sbplay calls played sequentially in
   DOS. Glk playback is asynchronous and a second play on the same channel
   cancels the first, which would collapse the game's repeated effects
   (clicker.wav three times in a row) into one audible click. Rotate over a
   few channels so short repeats overlap instead of cancelling. */
static schanid_t soundchan[NUM_SOUND_CHANNELS];
static int nextchan = 0;


/*----------------------------------------------------------------------
  Case-insensitive lookup of a media file in the game directory
\*----------------------------------------------------------------------*/

static const char *gamedir(void)
{
    static char dir[1024];
    static Boolean inited = FALSE;

    if (!inited) {
        char *slash = strrchr(advnam, '/');
        if (slash != NULL && (size_t)(slash - advnam) < sizeof(dir) - 1) {
            memcpy(dir, advnam, slash - advnam);
            dir[slash - advnam] = '\0';
        } else
            strcpy(dir, ".");
        inited = TRUE;
    }
    return dir;
}

static char *findFile(const char *name)
{
    DIR *dp;
    struct dirent *de;
    char *result = NULL;

    dp = opendir(gamedir());
    if (dp == NULL)
        return NULL;
    while ((de = readdir(dp)) != NULL)
        if (strcasecmp(de->d_name, name) == 0) {
            size_t n = strlen(gamedir()) + strlen(de->d_name) + 2;
            result = malloc(n);
            if (result != NULL)
                snprintf(result, n, "%s/%s", gamedir(), de->d_name);
            break;
        }
    closedir(dp);
    return result;
}

static MediaEntry *mediaEntry(const char *name, MediaKind kind)
{
    int i;
    MediaEntry *e;

    for (i = 0; i < numentries; i++)
        if (entries[i].kind == kind && strcmp(entries[i].name, name) == 0)
            return &entries[i];

    if (numentries == MAX_ENTRIES || strlen(name) >= MAX_NAME)
        return NULL;

    e = &entries[numentries++];
    e->kind = kind;
    strcpy(e->name, name);
    e->path = findFile(name);
    e->resno = 0;
    e->broken = FALSE;
    return e;
}

static unsigned char *readFile(const char *path, long *lenp)
{
    FILE *f;
    long len;
    unsigned char *data;

    f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    data = malloc(len);
    if (data == NULL || fread(data, 1, len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *lenp = len;
    return data;
}


/*----------------------------------------------------------------------
  Image decoding: 8-bpp and EGA PCX, and IFF ILBM/PBM, as Viewer 2.2
  read them. Everything decodes to one byte per pixel plus a 768-byte
  RGB palette, then goes out as a palettised BMP for the app to read.
\*----------------------------------------------------------------------*/

#define MAX_DIMENSION 4096

static unsigned char *decodePcx(const unsigned char *data, long len,
                                int *wp, int *hp, unsigned char *pal)
{
    int bpp, planes, bpl, w, h, x, y, pl;
    const unsigned char *p, *end;
    unsigned char *pixels, *linebuf;

    if (len < 129 || data[0] != 0x0A || data[2] != 1)
        return NULL;
    bpp = data[3];
    w = (data[8] | data[9] << 8) - (data[4] | data[5] << 8) + 1;
    h = (data[10] | data[11] << 8) - (data[6] | data[7] << 8) + 1;
    planes = data[65];
    bpl = data[66] | data[67] << 8;
    if (w <= 0 || h <= 0 || w > MAX_DIMENSION || h > MAX_DIMENSION)
        return NULL;
    if (!((bpp == 8 && planes == 1) || (bpp == 1 && planes >= 1 && planes <= 4)))
        return NULL;
    if (bpl < (bpp == 8 ? w : (w + 7) / 8))
        return NULL;

    pixels = calloc((size_t)w * h, 1);
    linebuf = malloc((size_t)bpl * planes);
    if (pixels == NULL || linebuf == NULL) {
        free(pixels);
        free(linebuf);
        return NULL;
    }

    p = data + 128;
    end = data + len;
    for (y = 0; y < h; y++) {
        int need = bpl * planes, got = 0;
        while (got < need && p < end) {
            int c = *p++;
            if ((c & 0xC0) == 0xC0) {
                int count = c & 0x3F;
                if (p == end)
                    break;
                c = *p++;
                while (count-- > 0 && got < need)
                    linebuf[got++] = c;
            } else
                linebuf[got++] = c;
        }
        if (bpp == 8)
            memcpy(pixels + (size_t)y * w, linebuf, w);
        else
            for (x = 0; x < w; x++)
                for (pl = 0; pl < planes; pl++)
                    if (linebuf[pl * bpl + (x >> 3)] & (0x80 >> (x & 7)))
                        pixels[(size_t)y * w + x] |= 1 << pl;
    }

    if (bpp == 8 && len >= 769 && data[len - 769] == 0x0C)
        memcpy(pal, data + len - 768, 768);
    else {
        /* EGA images carry their palette in the header; give a headerless
           8-bpp file a grayscale ramp rather than showing nothing */
        memset(pal, 0, 768);
        if (bpp == 1)
            memcpy(pal, data + 16, 48);
        else
            for (x = 0; x < 256; x++)
                pal[x * 3] = pal[x * 3 + 1] = pal[x * 3 + 2] = x;
    }

    free(linebuf);
    *wp = w;
    *hp = h;
    return pixels;
}

static unsigned char *decodeIff(const unsigned char *data, long len,
                                int *wp, int *hp, unsigned char *pal)
{
    int w = 0, h = 0, nplanes = 0, masking = 0, compression = 0;
    Boolean chunky;
    const unsigned char *body = NULL, *p, *end;
    long bodylen = 0, pos;
    int rowbytes, totplanes, x, y, pl;
    unsigned char *pixels, *rowbuf;

    if (len < 12 || memcmp(data, "FORM", 4) != 0)
        return NULL;
    if (memcmp(data + 8, "ILBM", 4) == 0)
        chunky = FALSE;
    else if (memcmp(data + 8, "PBM ", 4) == 0)
        chunky = TRUE;              /* DPaint for DOS's one-byte-per-pixel variant */
    else
        return NULL;

    memset(pal, 0, 768);
    pos = 12;
    while (pos + 8 <= len) {
        long chunklen = (long)data[pos + 4] << 24 | (long)data[pos + 5] << 16
                      | (long)data[pos + 6] << 8 | data[pos + 7];
        const unsigned char *payload = data + pos + 8;
        if (chunklen < 0 || pos + 8 + chunklen > len)
            break;
        if (memcmp(data + pos, "BMHD", 4) == 0 && chunklen >= 11) {
            w = payload[0] << 8 | payload[1];
            h = payload[2] << 8 | payload[3];
            nplanes = payload[8];
            masking = payload[9];
            compression = payload[10];
        } else if (memcmp(data + pos, "CMAP", 4) == 0) {
            long ncolors = chunklen / 3;
            if (ncolors > 256)
                ncolors = 256;
            memcpy(pal, payload, ncolors * 3);
        } else if (memcmp(data + pos, "BODY", 4) == 0) {
            body = payload;
            bodylen = chunklen;
        }
        pos += 8 + chunklen + (chunklen & 1);
    }

    if (body == NULL || w <= 0 || h <= 0 || w > MAX_DIMENSION || h > MAX_DIMENSION
        || nplanes < 1 || nplanes > 8 || compression > 1)
        return NULL;

    if (chunky) {
        rowbytes = (w + 1) & ~1;
        totplanes = 1;
    } else {
        rowbytes = ((w + 15) / 16) * 2;
        totplanes = nplanes + (masking == 1);
    }

    pixels = calloc((size_t)w * h, 1);
    rowbuf = malloc(rowbytes);
    if (pixels == NULL || rowbuf == NULL) {
        free(pixels);
        free(rowbuf);
        return NULL;
    }

    p = body;
    end = body + bodylen;
    for (y = 0; y < h; y++)
        for (pl = 0; pl < totplanes; pl++) {
            if (compression == 1) {
                int got = 0;
                while (got < rowbytes && p < end) {
                    signed char n = (signed char)*p++;
                    if (n >= 0) {
                        int count = n + 1;
                        while (count-- > 0 && p < end && got < rowbytes)
                            rowbuf[got++] = *p++;
                    } else if (n != -128) {
                        int count = -n + 1;
                        if (p == end)
                            break;
                        x = *p++;
                        while (count-- > 0 && got < rowbytes)
                            rowbuf[got++] = x;
                    }
                }
            } else {
                if (end - p < rowbytes)
                    memset(rowbuf, 0, rowbytes);
                else
                    memcpy(rowbuf, p, rowbytes);
                p += rowbytes;
            }
            if (chunky)
                memcpy(pixels + (size_t)y * w, rowbuf, w);
            else if (pl < nplanes)
                for (x = 0; x < w; x++)
                    if (rowbuf[x >> 3] & (0x80 >> (x & 7)))
                        pixels[(size_t)y * w + x] |= 1 << pl;
        }

    free(rowbuf);
    *wp = w;
    *hp = h;
    return pixels;
}

/* Emit an 8-bpp palettised BMP, the closest ImageIO-readable relative of
   the decoded data: header, 256-entry palette, bottom-up rows. */
static long writeBmp(int fd, int w, int h,
                     const unsigned char *pal, const unsigned char *pixels)
{
    FILE *f;
    unsigned char hdr[54];
    unsigned char quad[4];
    int rowbytes = (w + 3) & ~3;
    long imagesize = (long)rowbytes * h;
    long filesize = 54 + 1024 + imagesize;
    int i, y;

    f = fdopen(fd, "wb");
    if (f == NULL) {
        close(fd);
        return 0;
    }

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = filesize; hdr[3] = filesize >> 8;
    hdr[4] = filesize >> 16; hdr[5] = filesize >> 24;
    hdr[10] = (54 + 1024) & 0xFF; hdr[11] = (54 + 1024) >> 8;
    hdr[14] = 40;
    hdr[18] = w; hdr[19] = w >> 8;
    hdr[22] = h; hdr[23] = h >> 8;
    hdr[26] = 1;                                /* planes */
    hdr[28] = 8;                                /* bits per pixel */
    hdr[34] = imagesize; hdr[35] = imagesize >> 8;
    hdr[36] = imagesize >> 16; hdr[37] = imagesize >> 24;
    hdr[46] = 0; hdr[47] = 1;                   /* 256 palette entries */
    fwrite(hdr, 1, sizeof(hdr), f);

    quad[3] = 0;
    for (i = 0; i < 256; i++) {
        quad[0] = pal[i * 3 + 2];
        quad[1] = pal[i * 3 + 1];
        quad[2] = pal[i * 3];
        fwrite(quad, 1, 4, f);
    }

    memset(quad, 0, 4);
    for (y = h - 1; y >= 0; y--) {
        fwrite(pixels + (size_t)y * w, 1, w, f);
        if (rowbytes > w)
            fwrite(quad, 1, rowbytes - w, f);
    }

    if (fclose(f) != 0)
        return 0;
    return filesize;
}

/* Decode the picture, convert it to a temp BMP kept for the session, and
   register it with the app under a fresh resource number, so the standard
   glk_image_* calls find it and the app-side cache handles repeats. */
static Boolean registerPicture(MediaEntry *e)
{
    long len, bmplen;
    unsigned char *data, *pixels;
    unsigned char pal[768];
    int w, h, fd;
    const char *tmpdir;
    char bmppath[1024];

    data = readFile(e->path, &len);
    if (data == NULL)
        return FALSE;

    /* A format the app reads by itself needs no help from us: hand the
       file over as it lies. This is the showjpg case (A Matter of Time
       1.2), and it also covers a PCX or IFF game whose art someone has
       since converted. */
    if (len > 3
        && ((data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
            || memcmp(data, "\x89PNG", 4) == 0
            || memcmp(data, "GIF8", 4) == 0)) {
        free(data);
        e->resno = nextpicres++;
        win_loadimage(e->resno, e->path, 0, (int)len);
        return TRUE;
    }

    if (data[0] == 0x0A)
        pixels = decodePcx(data, len, &w, &h, pal);
    else
        pixels = decodeIff(data, len, &w, &h, pal);
    free(data);
    if (pixels == NULL)
        return FALSE;

    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || *tmpdir == '\0')
        tmpdir = "/tmp";
    snprintf(bmppath, sizeof(bmppath), "%s%salan2imgXXXXXX.bmp", tmpdir,
             tmpdir[strlen(tmpdir) - 1] == '/' ? "" : "/");
    fd = mkstemps(bmppath, 4);
    if (fd == -1) {
        free(pixels);
        return FALSE;
    }
    bmplen = writeBmp(fd, w, h, pal, pixels);
    free(pixels);
    if (bmplen == 0)
        return FALSE;

    e->resno = nextpicres++;
    win_loadimage(e->resno, bmppath, 0, (int)bmplen);
    return TRUE;
}


/*----------------------------------------------------------------------
  Holding back the text a game repeats across a picture

  VIEWER.EXE wiped the screen twice over: once flipping to graphics and
  once restoring text mode. So the games print the text around a picture
  twice -- the acode idiom is describe, pause, viewer, describe again, the
  second copy being what the player reads once the graphics screen is
  gone. Drawn inline the picture wipes nothing, so the second copy is pure
  repetition, and far more glaring than it ever was in DOS.

  What gets repeated is not tidy. In The Hollywood Murders "examine julie"
  repeats one sentence, and the two copies differ by a trailing space. In
  A Matter of Time every room repeats its description AND its object list,
  and on the way in the first copy is glued to the end of a much longer
  paragraph ("You decide that it is too dangerous for Clarisse ... There
  appears to be a trail leading to the north"). Paragraphs are therefore
  the wrong unit. What holds in both games is that the words printed after
  the picture are the last words printed before it, so that is the test:
  compare word by word, ignoring white space entirely, and look for the
  repeat as a suffix of what came before.

  Every character the interpreter prints funnels through glkio_printf(),
  which offers it here first. Normally it goes straight out and all we do
  is keep a copy of the turn so far. From the moment a picture is drawn
  the rest of the turn is held back instead, and whatever part of it
  repeats the run-up to the picture is dropped; the rest is printed.

  Text may never be lost: whatever is held is resolved and released at the
  next prompt, at the next picture, at the end of the game, and released
  untouched if it outgrows the buffer.
\*----------------------------------------------------------------------*/

#define TEXT_MAX 4096           /* per buffer; only the tail of the turn
                                   can ever match, so older text is
                                   dropped off the front rather than
                                   stopping us from tracking */
#define MAX_WORDS (TEXT_MAX / 2)

/* Below this a match is more likely to be a coincidence -- a stray "Yes."
   or the room name -- than a repeated description */
#define MIN_REPEAT_WORDS 4
#define MIN_REPEAT_CHARS 24

static char before[TEXT_MAX];   /* what the main window got this turn */
static int beforelen = 0;
static char held[TEXT_MAX];     /* printed since the picture, not yet out */
static int heldlen = 0;
static Boolean holding = FALSE;

/* statusline() prints through the same funnel with the status window
   current, and none of that is transcript */
static Boolean toMainWindow(void)
{
    return (Boolean)(glkMainWin != NULL
                     && glk_stream_get_current() == glk_window_get_stream(glkMainWin));
}

static void rememberText(const char *s, int len)
{
    if (len >= TEXT_MAX) {      /* keep the tail, drop the rest */
        s += len - TEXT_MAX;
        len = TEXT_MAX;
    }
    if (beforelen + len > TEXT_MAX) {
        int drop = beforelen + len - TEXT_MAX;
        memmove(before, before + drop, beforelen - drop);
        beforelen -= drop;
    }
    memcpy(before + beforelen, s, len);
    beforelen += len;
}

static void emitText(const char *s, int len)
{
    glk_put_buffer((char *)s, len);
    rememberText(s, len);
}

typedef struct { int off, len; } Word;

static int splitWords(const char *s, int slen, Word *w)
{
    int i = 0, n = 0;

    while (i < slen && n < MAX_WORDS) {
        while (i < slen && isspace((unsigned char)s[i])) i++;
        if (i == slen)
            break;
        w[n].off = i;
        while (i < slen && !isspace((unsigned char)s[i])) i++;
        w[n].len = i - w[n].off;
        n++;
    }
    return n;
}

/* How much of the front of `b` repeats the end of `a`, in bytes of `b`,
   0 for no repeat worth dropping. Longest run of words wins, so a repeat
   that the game followed with something new is still recognised. */
static int repeatedRun(const char *a, int alen, const char *b, int blen)
{
    static Word aw[MAX_WORDS], bw[MAX_WORDS];
    int na = splitWords(a, alen, aw);
    int nb = splitWords(b, blen, bw);
    int k, i, chars;

    for (k = (na < nb ? na : nb); k > 0; k--) {
        chars = 0;
        for (i = 0; i < k; i++) {
            Word *x = &aw[na - k + i], *y = &bw[i];
            if (x->len != y->len || memcmp(a + x->off, b + y->off, x->len) != 0)
                break;
            chars += x->len;
        }
        if (i == k && k >= MIN_REPEAT_WORDS && chars >= MIN_REPEAT_CHARS)
            return bw[k - 1].off + bw[k - 1].len;
    }
    return 0;
}

/* Safety valve: out it all goes, unjudged */
static void releaseHeld(void)
{
    int n = heldlen;

    heldlen = 0;
    holding = FALSE;
    if (n > 0)
        emitText(held, n);
}

/* Drop the repeat, print the rest */
static void resolveHeld(void)
{
    int n, run;

    if (!holding)
        return;
    holding = FALSE;

    run = repeatedRun(before, beforelen, held, heldlen);
    n = heldlen - run;
    heldlen = 0;
    if (n > 0)
        emitText(held + run, n);
}

/*======================================================================
  glkmedia_filter_output()

  Offered every string on its way to the main window. Returns TRUE when
  the string has been dealt with here and the caller must not print it.
 */
int glkmedia_filter_output(char *str)
{
    int len;

    if (str == NULL || str[0] == '\0' || !toMainWindow())
        return FALSE;

    len = (int)strlen(str);

    if (!holding) {
        rememberText(str, len);
        return FALSE;               /* the caller prints it, as always */
    }

    if (heldlen + len > TEXT_MAX) { /* far more than a repeat: give up */
        releaseHeld();
        return FALSE;
    }
    memcpy(held + heldlen, str, len);
    heldlen += len;
    return TRUE;
}

/*======================================================================
  glkmedia_flush_output()

  Called where the text stops and something else takes over: before input
  is asked for, and at the end of the game. This is where a turn ends, so
  it is where the held text is judged and released, and where the record
  of the turn starts afresh.

  The turn has to start afresh, or the record would begin with the "> "
  prompt agetline() prints before asking for input, followed by nothing at
  all for the line the player types -- the library echoes that, it does
  not come through here.
 */
void glkmedia_flush_output(void)
{
    resolveHeld();
    beforelen = 0;
}


/*----------------------------------------------------------------------
  viewer: put the picture in the transcript
\*----------------------------------------------------------------------*/

static void showPicture(MediaEntry *e)
{
    glui32 iw, ih;

    if (e->resno == 0 && !registerPicture(e)) {
        e->broken = TRUE;
        return;
    }

    /* Nothing to say to a host that cannot draw into a buffer window --
       and say it before touching the transcript, so that a text-only host
       (CheapGlk) still gets the byte-identical output it got before */
    if (!glk_gestalt(gestalt_Graphics, 0)
        || !glk_gestalt(gestalt_DrawImage, wintype_TextBuffer))
        return;
    if (!glk_image_get_info(e->resno, &iw, &ih) || iw == 0 || ih == 0) {
        e->broken = TRUE;
        return;
    }

    /* DOS flipped the whole screen to graphics and back on a keypress.
       Draw into the buffer window instead: the picture stays where the
       game put it, so there is nothing to flip back from and no keypress
       to wait for. Original size with the aspect ratio kept, but never
       wider than the window and re-resolved on every re-layout, so a
       640x480 screenshot fits and follows a resize (Glk 0.7.6
       glk_image_draw_scaled_ext; 0x10000 is 1.0 in 16.16 fixed point). */
    resolveHeld();              /* two pictures in a turn, as at startup */

    para();
    glk_image_draw_scaled_ext(glkMainWin, e->resno, imagealign_InlineUp, 0,
                              0, 0x10000,
                              imagerule_WidthOrig | imagerule_AspectRatio,
                              0x10000);
    newline();

    /* Whatever the game says from here to the end of the turn is held
       back, and judged against the run-up to this picture. The paragraph
       break just printed is part of that run-up, but only as white space,
       which the comparison ignores. */
    heldlen = 0;
    holding = (Boolean)(beforelen > 0);
}


/*----------------------------------------------------------------------
  sbplay: register the sample and start it on a rotating channel
\*----------------------------------------------------------------------*/

static void playSound(MediaEntry *e)
{
    schanid_t chan;

    if (e->resno == 0) {
        long len;
        unsigned char magic[4];
        FILE *f = fopen(e->path, "rb");

        /* Take only formats the app's sniffer will recognise (WAVE, and
           FORM for AIFF/8SVX): this is what catches nuthin.wav, a text
           file wearing a .wav name */
        if (f == NULL)
            return;
        if (fread(magic, 1, 4, f) != 4
            || (memcmp(magic, "RIFF", 4) != 0 && memcmp(magic, "FORM", 4) != 0)
            || fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) <= 0) {
            fclose(f);
            e->broken = TRUE;
            return;
        }
        fclose(f);
        e->resno = nextsndres++;
        win_loadsound(e->resno, e->path, 0, (int)len);
    }

    chan = soundchan[nextchan];
    if (chan == NULL)
        chan = soundchan[nextchan] = glk_schannel_create(0);
    nextchan = (nextchan + 1) % NUM_SOUND_CHANNELS;

    /* glk_schannel_play() is what keeps the app's one-play-slot bookkeeping
       in step: its loadsound() issues the FINDSOUND that selects which
       sample a PLAYSOUND starts. Calling win_playsound() directly here
       would play whatever sound was loaded last. */
    if (chan != NULL)
        glk_schannel_play(chan, e->resno);
}


/*----------------------------------------------------------------------
  The SYSTEM instruction
\*----------------------------------------------------------------------*/

void glkmedia_system(char *command)
{
    char verb[MAX_NAME], arg[MAX_NAME];
    int i, n, skip;

    if (getenv("ALAN2_MEDIA_DEBUG") != NULL)
        fprintf(stderr, "SYSTEM: %s\n", command);

    /* Peel off the verb and the first following word that is not a switch:
       switches can come before the file name ("viewer -f3 title.pcx" in
       A Matter of Time) or after it ("sbplay <name>.wav /s"). Anything
       else is junk (the one "pause $n $n" call site, where getstr() does
       not expand Alan's $n escapes) */
    n = 1;
    if (sscanf(command, "%63s%n", verb, &skip) < 1)
        return;
    command += skip;
    while (sscanf(command, "%63s%n", arg, &skip) >= 1) {
        command += skip;
        if (arg[0] != '-' && arg[0] != '/') {
            n = 2;
            break;
        }
    }
    for (i = 0; verb[i] != '\0'; i++)
        verb[i] = tolower(verb[i]);
    for (i = 0; n > 1 && arg[i] != '\0'; i++)
        arg[i] = tolower(arg[i]);

    if ((strcmp(verb, "viewer") == 0 || strcmp(verb, "showjpg") == 0)
        && n > 1) {
        /* The game's own "graphics off" verb gates the call sites in
           acode; this covers the Spatterlight preference on top */
        if (gli_enable_graphics) {
            MediaEntry *e = mediaEntry(arg, MEDIA_PICTURE);
            if (e != NULL && e->path != NULL && !e->broken)
                showPicture(e);
        }
    } else if (strcmp(verb, "sbplay") == 0 && n > 1) {
        if (gli_enable_sound) {
            MediaEntry *e = mediaEntry(arg, MEDIA_SOUND);
            if (e != NULL && e->path != NULL && !e->broken)
                playSound(e);
        }
    }
    /* "pause" -- COMMAND.COM's "Press any key to continue . . ." -- is
       ignored. The games use it to hold the text on screen before
       VIEWER.EXE flips the whole screen to graphics, so it sits right in
       front of a viewer call: the acode idiom is describe, pause, viewer,
       describe again on the way back. Drawing the picture inline wipes
       nothing, so there is nothing to hold, and the prompt would only ask
       the player for a keypress to reveal a picture that is already on its
       way into the transcript.

       Anything else: this is not DOS, do not try to run it */
}

#endif /* SPATTERLIGHT */
