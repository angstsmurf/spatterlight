/* ossound.h -- HTML TADS <SOUND>, on Glk sound channels */

#ifndef OSSOUND_H
#define OSSOUND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "glk.h"

/* the layers, back to front, as HTML TADS numbers its queues */
enum
{
    OSSND_BACKGROUND,
    OSSND_BGAMBIENT,
    OSSND_AMBIENT,
    OSSND_FOREGROUND,
    OSSND_LAYERS,
    OSSND_ALL = OSSND_LAYERS    /* CANCEL with no layer */
};

/* SEQUENCE */
enum
{
    OSSND_SEQ_NONE,
    OSSND_SEQ_REPLACE,
    OSSND_SEQ_RANDOM,
    OSSND_SEQ_CYCLE
};

/* a <SOUND> tag's attributes, with HTML TADS's defaults */
typedef struct ossound_tag
{
    const char *src;            /* UTF-8, not terminated */
    size_t srclen;
    int layer;                  /* OSSND_*, or -1 for none given */
    int cancel;                 /* CANCEL: 'layer' is the one to cancel */
    int interrupt;
    int repeat;                 /* 0 for LOOP */
    int has_repeat;
    int random;                 /* percent chance a second */
    int sequence;
    int volume;                 /* 0-100 */
    unsigned long fadein, fadeout;  /* milliseconds */
    int crossfade_in, crossfade_out;
} ossound_tag;

void ossound_tag_init(ossound_tag *tag);

/*
 *   Play, queue or cancel.  Returns false if the sound can't be played,
 *   because sound is off or the resource isn't there, and its ALT text is
 *   to be shown instead.
 */
int ossound_do_tag(const ossound_tag *tag);

/*
 *   An event from glk_select: true if it was a sound notify or volume
 *   notify, and the waiting loop should go on waiting.
 */
int ossound_event(const event_t *ev);

/*
 *   Is a sound waiting for its RANDOM chance?  Then a wait with no timer
 *   of its own runs a one-second timer, and calls ossound_timer when it
 *   fires.
 */
int ossound_wants_timer(void);
void ossound_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* OSSOUND_H */
