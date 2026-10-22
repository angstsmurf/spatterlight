/* ossound.c -- HTML TADS <SOUND>, on Glk sound channels
 *
 * HTML TADS plays sounds in four layers, each with a queue of its own
 * (htmlsnd.cpp in the HTML TADS sources, which this follows):
 *
 *   - A sound is added at the end of its layer's queue, and starts when
 *     nothing ahead of it is playing.  A finished sound leaves the queue
 *     once it has played all its REPEATs; a LOOP never leaves.
 *   - CANCEL marks everything in the queue as cancelled and stops what is
 *     playing, fading it out first if FADEOUT says so; when it has stopped,
 *     the cancelled sounds go and the next one starts.  INTERRUPT is a
 *     CANCEL of the sound's own layer before it is added.
 *   - A sound with RANDOM=n plays once at a time, each time with an n%
 *     chance, tried when it comes up in the queue and then once a second
 *     while nothing in the layer plays.
 *
 * Each layer has two Glk channels.  The queue plays on one; the other is
 * for a sound that fades out in the background (FADEOUT=CROSSFADE,n) while
 * the queue goes on.  The end of a sound, and of a fade, comes back as a
 * notify event with the number given to glk_schannel_play_ext or
 * glk_schannel_set_volume_ext, which tells whose it is.
 *
 * What Glk can't do: a fade-out at the natural end of a sound, and the
 * cross-fade into a queued sound (FADEIN=CROSSFADE without INTERRUPT), both
 * need the sound's length.  Those sounds end without a fade.
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "os.h"
#include "glk.h"
#include "gi_blorb.h"
#include "osres.h"
#include "ossound.h"

typedef struct sndentry
{
    struct sndentry *prev, *next;
    glui32 snd;
    int repeat;                 /* plays left, 0 for ever */
    int played;                 /* plays done */
    int random;
    int sequence;
    glui32 vol;                 /* Glk volume */
    unsigned long fadein;
    int cancelled;
} sndentry;

typedef struct
{
    sndentry *first, *last;
    sndentry *playing, *last_played;
    int seq_mode;
    schanid_t chan[2];
    int cur;                    /* chan[cur] plays the queue */
    int scheduled;              /* the plays given to the channel */
    glui32 play_notify;         /* the sound playing on chan[cur] */
    glui32 fade_notify;         /* a CANCEL fading chan[cur] out */
    glui32 bgfade_notify;       /* chan[!cur] fading out in the background */
} sndlayer;

static sndlayer layers[OSSND_LAYERS];
static glui32 last_notify;

static glui32 next_notify(void)
{
    if (++last_notify == 0)
        ++last_notify;
    return last_notify;
}

/*
 *   The RANDOM chances have a generator of their own, so as not to move
 *   the game's on; under the determinism testing theme it always starts
 *   the same
 */
static glui32 rng_state;

static int chance(void)
{
    glui32 x;

    if (rng_state == 0)
        rng_state = osglk_vclock() ? 0x2545F491 : (glui32)os_time(NULL) | 1;
    x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return (int)(x % 100);
}

static int sound_on(void)
{
    return glk_gestalt(gestalt_Sound2, 0) != 0;
}

static schanid_t layer_chan(sndlayer *l, int which)
{
    if (!l->chan[which])
        l->chan[which] = glk_schannel_create_ext((glui32)(l - layers) + 1,
                                                 0x10000);
    return l->chan[which];
}

static void delete_entry(sndlayer *l, sndentry *e)
{
    if (e->prev)
        e->prev->next = e->next;
    else
        l->first = e->next;
    if (e->next)
        e->next->prev = e->prev;
    else
        l->last = e->prev;
    if (l->playing == e)
        l->playing = NULL;
    if (l->last_played == e)
        l->last_played = e->prev;
    free(e);
}

static void sound_done(sndlayer *l, int count);

/*
 *   Start the next sound in the layer, if nothing plays and there is one
 *   (CHtmlSoundQueue::maybe_start_sound)
 */
static void maybe_start(sndlayer *l)
{
    sndentry *nxt = NULL, *orig;
    int repeat;
    schanid_t chan;
    glui32 notify;

    if (l->playing)
        return;

    switch (l->seq_mode)
    {
        case OSSND_SEQ_CYCLE:
            /*
             *   round the queue; HTML TADS meant this for CYCLE, but tests
             *   for a LOOP that no tag sets, and plays CYCLE in order.  No
             *   game has a SEQUENCE.
             */
            nxt = l->last_played && l->last_played->next
                ? l->last_played->next : l->first;
            break;

        case OSSND_SEQ_RANDOM:
            if (l->first)
            {
                int cnt = 0, idx;
                for (nxt = l->first ; nxt ; nxt = nxt->next)
                    ++cnt;
                idx = chance() % cnt;
                for (nxt = l->first ; nxt && idx ; nxt = nxt->next)
                    --idx;
            }
            break;

        default:
            /* the last one played is still there if it repeats */
            nxt = l->last_played ? l->last_played : l->first;
            break;
    }

    /* nothing next in line: one waiting for its chance, then */
    if (!nxt)
    {
        for (nxt = l->first ; nxt ; nxt = nxt->next)
            if (nxt->random)
                break;
        if (!nxt)
            return;
    }

    /* a RANDOM sound that misses its chance passes to the next */
    for (orig = nxt ; nxt->random && chance() > nxt->random ; )
    {
        nxt = nxt->next ? nxt->next : l->first;
        if (nxt == orig)
            return;
    }

    if (nxt->sequence == OSSND_SEQ_REPLACE)
        while (l->first && l->first != nxt)
            delete_entry(l, l->first);
    l->seq_mode = nxt->sequence;

    /*
     *   the channel repeats a sound only when nothing else can come
     *   between its plays
     */
    repeat = (l->seq_mode == OSSND_SEQ_NONE
              || l->seq_mode == OSSND_SEQ_REPLACE) && !nxt->random
        ? nxt->repeat : 1;

    l->playing = nxt;
    l->scheduled = repeat;
    chan = layer_chan(l, l->cur);
    notify = next_notify();
    if (chan)
        glk_schannel_set_volume(chan, nxt->played == 0 && nxt->fadein
                                ? 0 : nxt->vol);
    if (!chan || !glk_schannel_play_ext(chan, nxt->snd,
                                        repeat ? (glui32)repeat : 0xFFFFFFFF,
                                        notify))
    {
        /* it can't play: as if it had stopped at once */
        nxt->cancelled = 1;
        sound_done(l, 0);
        return;
    }
    l->play_notify = notify;
    if (nxt->played == 0 && nxt->fadein)
        glk_schannel_set_volume_ext(chan, nxt->vol, nxt->fadein, 0);
}

/* the sound playing has ended (CHtmlSoundQueue::on_sound_done) */
static void sound_done(sndlayer *l, int count)
{
    sndentry *e, *nxt;

    l->seq_mode = l->playing ? l->playing->sequence : OSSND_SEQ_NONE;
    l->last_played = l->playing;
    l->playing = NULL;
    l->play_notify = l->fade_notify = 0;

    e = l->last_played;
    if (e && e->repeat != 0)
    {
        e->repeat -= count;
        e->played += count;
        if (e->repeat <= 0)
            delete_entry(l, e);
    }
    else if (e)
        e->played += count;

    for (e = l->first ; e ; e = nxt)
    {
        nxt = e->next;
        if (e->cancelled)
            delete_entry(l, e);
    }

    maybe_start(l);
}

/* CANCEL a layer (CHtmlSoundQueue::stop_current_sound) */
static void cancel_layer(sndlayer *l, unsigned long fadeout, int background)
{
    sndentry *e;
    schanid_t chan;

    if (!l->playing)
    {
        while (l->first)
            delete_entry(l, l->first);
        return;
    }

    for (e = l->first ; e ; e = e->next)
        e->cancelled = 1;

    chan = l->chan[l->cur];
    if (fadeout && background)
    {
        /* it fades on its own channel, and the queue goes on */
        if (l->bgfade_notify)
            glk_schannel_stop(l->chan[!l->cur]);
        l->bgfade_notify = next_notify();
        glk_schannel_set_volume_ext(chan, 0, fadeout, l->bgfade_notify);
        l->cur = !l->cur;
        sound_done(l, 0);
    }
    else if (fadeout)
    {
        /* the queue goes on when the fade is done */
        l->fade_notify = next_notify();
        glk_schannel_set_volume_ext(chan, 0, fadeout, l->fade_notify);
    }
    else
    {
        glk_schannel_stop(chan);
        sound_done(l, 0);
    }
}

void ossound_tag_init(ossound_tag *tag)
{
    memset(tag, 0, sizeof(*tag));
    tag->layer = -1;
    tag->repeat = 1;
    tag->volume = 100;
}

/* the kind of sound, from the name's suffix, as HTML TADS tells it */
enum { SND_NONE, SND_WAV, SND_MIDI, SND_MPEG, SND_OGG };

static int sound_type(const char *src, size_t len)
{
    static const struct { const char *ext; int type; } exts[] =
    {
        { "wav", SND_WAV }, { "mid", SND_MIDI }, { "midi", SND_MIDI },
        { "mpg", SND_MPEG }, { "mp2", SND_MPEG }, { "mp3", SND_MPEG },
        { "ogg", SND_OGG }
    };
    size_t i = len, k;

    while (i > 0 && src[i - 1] != '.' && src[i - 1] != '/')
        --i;
    if (i == 0 || src[i - 1] != '.')
        return SND_NONE;
    for (k = 0 ; k < sizeof(exts) / sizeof(exts[0]) ; ++k)
        if (strlen(exts[k].ext) == len - i
            && strncasecmp(exts[k].ext, src + i, len - i) == 0)
            return exts[k].type;
    return SND_NONE;
}

int ossound_do_tag(const ossound_tag *tag)
{
    sndentry *e;
    sndlayer *l;
    int layer = tag->layer, type, i;
    glui32 snd;

    if (!sound_on())
        return 0;

    if (tag->cancel)
    {
        for (i = 0 ; i < OSSND_LAYERS ; ++i)
            if (layer < 0 || layer == OSSND_ALL || layer == i)
                cancel_layer(&layers[i], tag->fadeout, tag->crossfade_out);
        return 1;
    }

    /* MIDI is music, always at the back; a WAV with no layer is a guess */
    type = sound_type(tag->src, tag->srclen);
    if (type == SND_NONE)
        return 0;
    if (type == SND_MIDI)
        layer = OSSND_BACKGROUND;
    else if (type == SND_WAV && layer < 0)
        layer = tag->random ? OSSND_AMBIENT : OSSND_FOREGROUND;
    if (layer < 0 || layer >= OSSND_LAYERS)
        return 0;

    snd = osres_find(tag->src, tag->srclen, giblorb_ID_Snd);
    if (snd == 0)
        return 0;

    l = &layers[layer];
    if (tag->interrupt)
        cancel_layer(l, tag->crossfade_in ? tag->fadein : 0,
                     tag->crossfade_in);

    e = calloc(1, sizeof(*e));
    if (!e)
        return 0;
    e->snd = snd;
    e->repeat = tag->repeat < 0 ? 1 : tag->repeat;
    if (layer == OSSND_AMBIENT && tag->random && !tag->has_repeat)
        e->repeat = 0;
    e->random = tag->random;
    e->sequence = tag->sequence;
    e->vol = (glui32)tag->volume * 0x10000 / 100;
    e->fadein = tag->fadein;

    e->prev = l->last;
    if (l->last)
        l->last->next = e;
    else
        l->first = e;
    l->last = e;

    if (!l->playing)
        maybe_start(l);
    return 1;
}

int ossound_event(const event_t *ev)
{
    int i;

    if (ev->type != evtype_SoundNotify && ev->type != evtype_VolumeNotify)
        return 0;
    if (ev->val2 == 0)
        return 1;

    for (i = 0 ; i < OSSND_LAYERS ; ++i)
    {
        sndlayer *l = &layers[i];

        if (ev->type == evtype_SoundNotify && ev->val2 == l->play_notify)
            sound_done(l, l->scheduled);
        else if (ev->type == evtype_VolumeNotify
                 && ev->val2 == l->fade_notify)
        {
            glk_schannel_stop(l->chan[l->cur]);
            sound_done(l, 0);
        }
        else if (ev->type == evtype_VolumeNotify
                 && ev->val2 == l->bgfade_notify)
        {
            glk_schannel_stop(l->chan[!l->cur]);
            l->bgfade_notify = 0;
        }
        else
            continue;
        break;
    }
    return 1;
}

int ossound_wants_timer(void)
{
    int i;
    sndentry *e;

    for (i = 0 ; i < OSSND_LAYERS ; ++i)
        if (!layers[i].playing)
            for (e = layers[i].first ; e ; e = e->next)
                if (e->random)
                    return 1;
    return 0;
}

void ossound_timer(void)
{
    int i;

    for (i = 0 ; i < OSSND_LAYERS ; ++i)
        maybe_start(&layers[i]);
}
