/*
 * hecugel_sound.c: sound and music for the Spatterlight Glk front end for
 * Hugo (see hecugel.c).
 */

#include "heheader.h"
#include "glk.h"
#include "glkimp.h"
#include "fileref.h"

#undef fclose
#undef ferror
#undef fgetc
#undef fgets
#undef fread
#undef fprintf
#undef fputc
#undef fputs
#undef ftell
#undef fseek

#define MAXRES 1024

static schanid_t mchannel = NULL;
static schanid_t schannel = NULL;

static long resids[MAXRES];
static char *resfiles[MAXRES];
static int numres = 0;

static int loadres(HUGO_FILE infile, int reslen)
{
    char *buf;
    long offset, suboffset;
    int index;
    int i;

    /* A resource is identified by its offset *within a given file*: games
       may have more than one resource file, and the same offset in two of
       them is two different sounds. */
    offset = glk_stream_get_position(infile);
    for (i = 0; i < numres; i++)
        if (resids[i] == offset && resfiles[i] && infile->filename
            && !strcmp(resfiles[i], infile->filename)) {
            return i;
        }

    /* Too many resources loaded... */
    if (numres + 1 == MAXRES)
        return -1;

    buf = malloc(reslen);
    if (!buf)
        return -1;

    index = numres++;

    resids[index] = offset;
    resfiles[index] = infile->filename ? strdup(infile->filename) : NULL;

    glk_get_buffer_stream(infile, buf, reslen);

    suboffset = 0;

    // At least Hugo Tetris sends malformed data with extra junk before the RIFF header.
    // We try to fix that here.
    if (reslen > 128)
    {
        for (i = 0; i < 124; i++)
        {
            if (!memcmp(buf + i, "RIFF", 4))
            {
                suboffset = i;
                break;
            }
        }
    }

    /* Note that reslen is still the whole length of the sound: in Hugo Tetris
       the resource file's index is what is off, listing each sound a few bytes
       before its RIFF header, while the length counts from the header. (The
       last sound in TETRIS1 ends exactly at end of file when counted that
       way.) So the junk is skipped by moving the start, not by shortening. */
    win_loadsound(index, infile->filename, offset + suboffset, reslen);

    free(buf);
    return index;
}

/* The music or sample channel, created on first use. NULL if Glk can't
   play that kind of sound. */
static schanid_t heglk_channel(int music)
{
    schanid_t *chan = music ? &mchannel : &schannel;
    if (!*chan && glk_gestalt(gestalt_Sound, 0)
        && (!music || glk_gestalt(gestalt_SoundMusic, 0)))
        *chan = glk_schannel_create(0);
    return *chan;
}

static int heglk_play(schanid_t chan, HUGO_FILE infile, long reslen, char loop_flag)
{
    if (chan)
    {
        int index = loadres(infile, reslen);
        if (index < 0)
        {
            glk_stream_close(infile, NULL);
            return false;
        }
        glk_schannel_play_ext(chan, index, loop_flag ? -1 : 1, 0);
    }

    glk_stream_close(infile, NULL);
    return true;
}

static void heglk_set_volume(schanid_t chan, int vol)
{
    if (chan)
        glk_schannel_set_volume(chan, (vol * 0x10000) / 100);
}

static void heglk_stop(schanid_t chan)
{
    if (chan)
        glk_schannel_stop(chan);
}

int hugo_playmusic(HUGO_FILE infile, long reslen, char loop_flag)
{
    return heglk_play(heglk_channel(true), infile, reslen, loop_flag);
}

void hugo_musicvolume(int vol)
{
    heglk_set_volume(heglk_channel(true), vol);
}

void hugo_stopmusic(void)
{
    heglk_stop(heglk_channel(true));
}

int hugo_playsample(HUGO_FILE infile, long reslen, char loop_flag)
{
    return heglk_play(heglk_channel(false), infile, reslen, loop_flag);
}

void hugo_samplevolume(int vol)
{
    heglk_set_volume(heglk_channel(false), vol);
}

void hugo_stopsample(void)
{
    heglk_stop(heglk_channel(false));
}
