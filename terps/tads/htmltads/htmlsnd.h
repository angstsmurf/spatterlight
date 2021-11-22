/* $Header: d:/cvsroot/tads/html/htmlsnd.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlsnd.h - sound support
Function
  
Notes
  
Modified
  01/10/98 MJRoberts  - Creation
*/

#ifndef HTMLSND_H
#define HTMLSND_H

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLATTR_H
#include "htmlattr.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Sound queue entry.  Each entry in a sound queue is represented by one
 *   of these objects.  
 */
class CHtmlSoundQueueEntry
{
    friend class CHtmlSoundQueue;
    
public:
    CHtmlSoundQueueEntry(class CHtmlResCacheObject *res,
                         int repeat_count, int random_start,
                         HTML_Attrib_id_t seq);
    ~CHtmlSoundQueueEntry();

    /* get the sound resource */
    class CHtmlResCacheObject *get_res() const { return res_; }

    /* get the sequence type */
    HTML_Attrib_id_t get_sequence() const { return seq_; }

    /* get the repeat count remaining - zero indicates looping forever */
    int get_repeat_count() const { return repeat_count_; }

    /*
     *   Decrement the repeat count.  This should be called when we finish
     *   playing the sound to determine how many more times the sound
     *   should be played.  This routine returns true if we've exhausted
     *   the repeat count. 
     */
    int dec_repeat_count(int count)
    {
        /* if the repeat count is zero, we play forever */
        if (repeat_count_ == 0)
            return FALSE;

        /* decrement the counter; return true if it's reached zero */
        repeat_count_ -= count;
        return (repeat_count_ == 0);
    }

    /*
     *   Exhaust the repeat count in preparation for repeated playback.
     *   This should be called when the sound is scheduled to be played
     *   for its full number of repetitions.  We'll set the repeat count
     *   to 1 if it's non-zero, in which case when the sound has finished
     *   playing it will be evident that it doesn't need to be played
     *   again, or leave it at zero if it's zero. 
     */
    void exhaust_repeat_count()
    {
        /* if the repeat count is zero, we play forever */
        if (repeat_count_ == 0)
            return;

        /* set the counter to 1 to indicate that it has no more plays left */
        repeat_count_ = 1;
    }

    /* get the random start probability */
    int get_random_start() const { return random_start_; }

    /*
     *   Mark the sound for cancellation.  When the end-of-sound callback
     *   fires, we'll remove the sound from its queue. 
     */
    void cancel() { cancel_ = TRUE; }

    /* check if the sound has been cancelled */
    int is_cancelled() const { return cancel_; }

protected:
    /* this item's sound resource */
    class CHtmlResCacheObject *res_;

    /* sequencing type */
    HTML_Attrib_id_t seq_;

    /* repeat count */
    int repeat_count_;

    /* random start probability */
    int random_start_;

    /* flag indicating that the sound is to be cancelled */
    int cancel_ : 1;
    
    /* next and previous items in my queue */
    class CHtmlSoundQueueEntry *next_;
    class CHtmlSoundQueueEntry *prev_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Sound queue.  
 */

class CHtmlSoundQueue
{
public:
    /*
     *   Create a sound queue.  The queue is associated with the given
     *   system window. 'bgqueue' is the next queue further in the
     *   background; whenever we play a sound, we will tell each queue
     *   behind me about it so that they have a chance to suspend their
     *   playback if the sound device requires exclusive access.  At the
     *   end of each sound, we'll inform each background queue so that
     *   they can resume playback if they suspended a sound for us.  
     */
    CHtmlSoundQueue(class CHtmlSysWin *win, CHtmlSoundQueue *bgqueue);

    ~CHtmlSoundQueue();

    /* delete all entries in the queue */
    void clear();

    /* 
     *   Add an entry to the queue.
     *   
     *   'repeat' is the number of times to repeat the sound.  0 indicates
     *   that the sound is to be looped indefinitely.
     *   
     *   'random_start' is the random start probability.  0 indicates that
     *   the sound is to be played without randomization.  A value from 1
     *   to 100 is the probability that the sound will be started at any
     *   given time that the queue is idle.  Each time a sound is played,
     *   if the repeat count is one, we'll remove the sound from the
     *   queue, and if the repeat count is higher than one, we'll
     *   decrement it.
     *   
     *   'seq' is the sequence code.  HTML_Attrib_replace indicates that
     *   any previous sounds in the queue should be removed once this
     *   sound plays.  HTML_Attrib_random indicates that other sounds
     *   should be kept, and a new sound from the queue chosen randomly
     *   after this sound has been played.  HTML_Attrib_cycle indicates
     *   that other sounds should be kept; after this sound is finished
     *   playing, we should move on to the next sound, or back to the
     *   first sound in the queue if nothing's left.  
     */
    void add_sound(class CHtmlResCacheObject *res, int repeat_count,
                   int random_start, HTML_Attrib_id_t seq);

    /*
     *   Immediately stop the sound currently playing.  Do not start a new
     *   sound.  This halts the queue so that a transition can be made,
     *   such as clearing out all current music and starting a new set of
     *   music.
     *   
     *   If sync is true, we must not return until the sound has been
     *   stopped and the callback has been invoked.  Normally, we'll
     *   schedule the sound for cancellation and return; the sound may not
     *   actually be stopped by the time the function returns.  If sync is
     *   true, however, we won't return until after the callback has been
     *   invoked.  
     */
    void stop_current_sound(int sync);

    /*
     *   Receive notification that a queue further in the foreground is
     *   about to start a sound.  If we're playing back a sound that uses
     *   the same device, and playback requires exclusive access to the
     *   device (whether this is true or not is system-dependent), we will
     *   suspend our playback until we hear that the foreground sound is
     *   finished.  The end-of-sound callback is not called here no matter
     *   what happens, because we're only suspending playback rather than
     *   cancelling playback.  
     */
    void start_fg_sound(class CHtmlSysSound *snd);

    /*
     *   Receive notification that a foreground sound is terminating.  If
     *   we suspended a sound on a previous call to start_fg_sound, we
     *   should resume playback now. 
     */
    void end_fg_sound(CHtmlSysSound *snd);
    
protected:
    /* create a new queue entry and add it at the end of the queue */
    void add_queue_entry(class CHtmlResCacheObject *res,
                         int repeat_count, int random_start,
                         HTML_Attrib_id_t seq);

    /* remove a queue entry from the queue and delete it */
    void delete_queue_entry(class CHtmlSoundQueueEntry *entry);

    /* process a timer callback */
    void on_timer();

    /* process an end-of-sound event */
    void on_sound_done(int repeat_count);

    /*
     *   Check to see if we need to start a new sound.  This routine is
     *   invoked whenever we add a new sound and nothing else is playing,
     *   whenever we end a sound, and at timer intervals when nothing else
     *   is playing. 
     */
    void maybe_start_sound();

    /* 
     *   Timer callback function.  When we're created, we register with
     *   our associated window to receive periodic timer callbacks.  This
     *   is the function invoked by the window for these callbacks.  
     */
    static void timer_func_cb(void *ctx);

    /*
     *   End-of-sound callback function.  When we play a sound, we'll tell
     *   the sound to call this function when it's finished playing. 
     */
    static void sound_done_cb(void *ctx, int repeat_count);

    /* 
     *   The window I'm associated with.  A sound queue is always
     *   associated with a window; the window provides access to the
     *   system services that allow us to play sounds. 
     */
    class CHtmlSysWin *win_;

    /* next queue in background behind me */
    CHtmlSoundQueue *next_bg_queue_;

    /* list of sound queue entries */
    class CHtmlSoundQueueEntry *first_entry_;
    class CHtmlSoundQueueEntry *last_entry_;

    /* 
     *   Current queue entry that's playing.  If the queue is currently
     *   idle (i.e., no sound is currently playing), this member is null. 
     */
    class CHtmlSoundQueueEntry *playing_;

    /*
     *   Previous sound that we played.  We use this for sequencing to
     *   determine the next sound to play. 
     */
    class CHtmlSoundQueueEntry *last_played_;

    /*
     *   Current sequence mode.  The sequence mode is selected by the last
     *   sound we played back. 
     */
    HTML_Attrib_id_t seq_mode_;

    /*
     *   Foreground sound for which we're suspended.  If the system
     *   requires that a particular type of sound be played exclusively
     *   (i.e, the sound can't play simultaneously with any other sounds
     *   of certain types), we'll suspend our sound whenever a queue
     *   further in the foreground needs to start a sound and we're
     *   playing an incompatible sound.  This member records the
     *   foreground sound that suspended us so that we can resume playback
     *   when the foreground sound ends. 
     */
    class CHtmlSysSound *fg_suspender_;

    /* system time of last timer event */
    os_timer_t last_timer_event_time_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Sound loader.  The sound loaders are very simple; they merely record
 *   the file name, start seek offset of the sound data, and length of the
 *   sound data.  The system sound objects are responsible for reading the
 *   data.
 *   
 *   Note that we don't read the data into memory, because most systems
 *   will want to stream sounds off of disk rather than loading them all
 *   at once.  Sound files tend to be quite large, so it's usually more
 *   efficient to stream a sound off of disk as needed during playback
 *   than to keep the entire file loaded throughout playback.  
 */

class CHtmlSound
{
public:
    CHtmlSound()
    {
        seekpos_ = 0;
        filesize_ = 0;
    }
    
    ~CHtmlSound() { }

    /*
     *   Set file information.  This merely records the file information
     *   for later use. 
     */
    void set_file(const textchar_t *fname, unsigned long seekpos,
                  unsigned long filesize)
    {
        fname_.set(fname);
        seekpos_ = seekpos;
        filesize_ = filesize;
    }

    /* get the file information */
    const textchar_t *get_fname() const { return fname_.get(); }
    unsigned long get_seekpos() const { return seekpos_; }
    unsigned long get_filesize() const { return filesize_; }

protected:
    CStringBuf fname_;
    unsigned long seekpos_;
    unsigned long filesize_;
};

/*
 *   MIDI sound loader - trivial subclass of generic sound loader
 */
class CHtmlMidi: public CHtmlSound
{
public:
    CHtmlMidi() { }

    /* read a MIDI file */
    int read_midi_file(const textchar_t *fname, unsigned long seekpos,
                       unsigned long filesize)
    {
        /* remember the file information */
        set_file(fname, seekpos, filesize);
        return 0;
    }
};

/*
 *   WAV sound loader - trivial subclass of generic sound loader 
 */
class CHtmlWav: public CHtmlSound
{
public:
    CHtmlWav() { }
    
    /* read a WAV file */
    int read_wav_file(const textchar_t *fname, unsigned long seekpos,
                      unsigned long filesize)
    {
        /* remember the file information */
        set_file(fname, seekpos, filesize);
        return 0;
    }
};

/*
 *   MPEG 2.0 audio sound loader - trivial subclass of generic sound
 *   loader 
 */
class CHtmlMpeg: public CHtmlSound
{
public:
    CHtmlMpeg() { }

    /* read an MPEG file */
    int read_mpeg_file(const textchar_t *fname, unsigned long seekpos,
                       unsigned long filesize)
    {
        /* remember the file information */
        set_file(fname, seekpos, filesize);
        return 0;
    }
};

/*
 *   Ogg Vorbis sound loader 
 */
class CHtmlOgg: public CHtmlSound
{
public:
    CHtmlOgg() { }

    /* read an .ogg file */
    int read_ogg_file(const textchar_t *fname, unsigned long seekpos,
                      unsigned long filesize)
    {
        /* remember the file information */
        set_file(fname, seekpos, filesize);
        return 0;
    }
};

#endif /* HTMLSND_H */

