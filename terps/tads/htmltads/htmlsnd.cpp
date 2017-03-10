#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlsnd.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlsnd.cpp - sound support
Function
  
Notes
  
Modified
  01/10/98 MJRoberts  - Creation
*/

#include <stdlib.h>

/* include TADS OS layer */
#include <os.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLATTR_H
#include "htmlattr.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Sound queue entry implementation 
 */

CHtmlSoundQueueEntry::CHtmlSoundQueueEntry(CHtmlResCacheObject *res,
                                           int repeat_count, int random_start,
                                           HTML_Attrib_id_t seq)
{
    /* remember the cache object, and add a reference to it */
    res_ = res;
    res_->add_ref();

    /* not in any list yet */
    next_ = prev_ = 0;

    /* remember the attributes */
    seq_ = seq;
    random_start_ = random_start;
    repeat_count_ = repeat_count;

    /* this sound hasn't been cancelled */
    cancel_ = FALSE;
}

CHtmlSoundQueueEntry::~CHtmlSoundQueueEntry()
{
    /* remove our reference from our cache object */
    res_->remove_ref();
}

/* ------------------------------------------------------------------------ */
/*
 *   Sound queue implementation 
 */

CHtmlSoundQueue::CHtmlSoundQueue(CHtmlSysWin *win, CHtmlSoundQueue *bgqueue)
{
    /* remember our window and the next queue in the background */
    win_ = win;
    next_bg_queue_ = bgqueue;

    /* we haven't had any timer events yet */
    last_timer_event_time_ = os_get_time();

    /* register with the window to receive timer callbacks */
    win->register_timer_func(&timer_func_cb, (void *)this);

    /* nothing in the list yet */
    first_entry_ = last_entry_ = 0;

    /* nothing is playing yet */
    playing_ = 0;
    last_played_ = 0;

    /* start off in default sequence mode */
    seq_mode_ = HTML_Attrib_invalid;

    /* we're not suspended yet */
    fg_suspender_ = 0;
}

CHtmlSoundQueue::~CHtmlSoundQueue()
{
    /* make sure the current sound is stopped */
    stop_current_sound(TRUE);

    /* remove our timer function */
    win_->unregister_timer_func(&timer_func_cb, (void *)this);

    /* delete all of the entries in the queue */
    clear();
}

/*
 *   Clear the queue.  Deletes all entries in the queue. 
 */
void CHtmlSoundQueue::clear()
{
    CHtmlSoundQueueEntry *cur;
    CHtmlSoundQueueEntry *nxt;

    /* delete all of our list entries */
    for (cur = first_entry_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next item, since we're about to delete this one */
        nxt = cur->next_;

        /* delete this item */
        delete cur;
    }

    /* clear all of our references to queue entries, since they're all gone */
    first_entry_ = last_entry_ = 0;
    playing_ = 0;
    last_played_ = 0;
}

/*
 *   Add a sound to the queue 
 */
void CHtmlSoundQueue::add_sound(CHtmlResCacheObject *res, int repeat_count,
                                int random_start, HTML_Attrib_id_t seq)
{
    /* if it's not a sound, ignore the request */
    if (res->get_sound() == 0)
        return;
    
    /* add the entry to the end of the queue */
    add_queue_entry(res, repeat_count, random_start, seq);

    /* 
     *   if there's nothing currently playing, we may want to start the
     *   new sound immediately 
     */
    if (playing_ == 0)
        maybe_start_sound();
}

/*
 *   Immediately stop the sound currently playing.  Do not start a new
 *   sound.  If 'sync' is true, this call must not return until the sound
 *   has actually stopped.  
 */
void CHtmlSoundQueue::stop_current_sound(int sync)
{
    CHtmlSoundQueueEntry *entry;

    if (playing_ != 0)
    {
        /* 
         *   We have a sound playing, so we must end it and then clear out
         *   the queue.  First, mark all of the objects currently in the
         *   queue as cancelled 
         */
        for (entry = first_entry_ ; entry != 0 ; entry = entry->next_)
            entry->cancel();
    
        /* 
         *   Next, tell the current sound to stop playing.  Ending the sound
         *   will invoke our callback as normal; at that point we'll clear
         *   all of the canceled sound objects out of the queue.  
         */
        playing_->get_res()->get_sound()->cancel_sound(win_, sync);
    }
    else
    {
        /* nothing is playing - delete everything in the queue */
        while (first_entry_ != 0)
            delete_queue_entry(first_entry_);
    }
}

/*
 *   Create a new queue entry and add it at the end of the queue 
 */
void CHtmlSoundQueue::add_queue_entry(CHtmlResCacheObject *res,
                                      int repeat_count, int random_start,
                                      HTML_Attrib_id_t seq)
{
    CHtmlSoundQueueEntry *entry;
    
    /* create the new entry */
    entry = new CHtmlSoundQueueEntry(res, repeat_count, random_start, seq);

    /* link it into the end of our list */
    entry->prev_ = last_entry_;
    if (last_entry_ != 0)
        last_entry_->next_ = entry;
    else
        first_entry_ = entry;
    last_entry_ = entry;
}

/*
 *   Remove an entry from the queue and delete the entry
 */
void CHtmlSoundQueue::delete_queue_entry(CHtmlSoundQueueEntry *entry)
{
    /* unlink it from the list */
    if (entry->prev_ != 0)
        entry->prev_->next_ = entry->next_;
    else
        first_entry_ = entry->next_;

    if (entry->next_ != 0)
        entry->next_->prev_ = entry->prev_;
    else
        last_entry_ = entry->prev_;

    /* 
     *   forget any pointers to this sound, since it won't be valid after
     *   we return 
     */
    if (playing_ == entry)
        playing_ = 0;

    /* 
     *   if this was the last-played entry, advance the last-played to the
     *   next entry 
     */
    if (last_played_ == entry)
        last_played_ = entry->next_;

    /* delete the entry */
    entry->next_ = entry->prev_ = 0;
    delete entry;
}

/*
 *   Process timer notification 
 */
void CHtmlSoundQueue::on_timer()
{
    /* if we have a sound currently playing, there's nothing to do here */
    if (playing_ != 0)
        return;

    /*
     *   Check the interval from the last timer call.  If less than a
     *   second has elapsed, ignore the call - the system is delivering
     *   timer events faster than we can use them.  
     */
    if (os_get_time() < last_timer_event_time_ + OS_TIMER_UNITS_PER_SECOND)
        return;

    /* note the current time as the time of the last timer event */
    last_timer_event_time_ = os_get_time();

    /* check to see if we should start a new sound */
    maybe_start_sound();
}

/*
 *   Check to see if we should start a new sound.  This routine is invoked
 *   whenever we finish playing a sound, or when we add a new sound and
 *   nothing else is playing, or at timer intervals when nothing is
 *   playing.  
 */
void CHtmlSoundQueue::maybe_start_sound()
{
    CHtmlSoundQueueEntry *nxt;
    CHtmlSoundQueueEntry *orig_nxt;
    
    /* if anything is playing already, we can't start a new sound */
    if (playing_ != 0)
        return;

    /*
     *   Check the current sequence mode to determine what to do next 
     */
    switch(seq_mode_)
    {
    case HTML_Attrib_invalid:
    case HTML_Attrib_replace:
        /*
         *   Default sequence mode.  Play the next sound in line.  If
         *   last_played_ is not null, it's the next in line, since it's
         *   either the last sound we played (which means the sound is set
         *   to loop), or the next sound after that sound (which means
         *   that the previous sound was deleted).  
         */
        if (last_played_ != 0)
            nxt = last_played_;
        else
            nxt = first_entry_;
        break;

    case HTML_Attrib_loop:
        /*
         *   Looping mode.  If we've reached the last sound in the queue,
         *   start over at the beginning. 
         */
        if (last_played_ != 0 && last_played_->next_ != 0)
            nxt = last_played_->next_;
        else
            nxt = first_entry_;
        break;

    case HTML_Attrib_random:
        /*
         *   Choose a new sound at random.  Count the elements in the queue,
         *   then randomly choose one of them.  If we don't have any
         *   entries, do nothing here.  
         */
        if (first_entry_ != 0)
        {
            int cnt;
            CHtmlSoundQueueEntry *cur;
            int idx;

            /* count the entries */
            for (cur = first_entry_, cnt = 0 ; cur != 0 ;
                 cur = cur->next_, ++cnt) ;

            /* pick a random number from 0 to the number of entries */
            idx = rand() % cnt;

            /* find that entry */
            for (nxt = first_entry_ ; nxt != 0 && idx != 0 ;
                 nxt = nxt->next_, --idx) ;
        }
        else
        {
            /* there are no entries; there's nothing we can randomly start */
            nxt = 0;
        }
        break;

    default:
        /* other attributes should never occur; ignore them if they do */
        break;
    }

    /* make sure we found something */
    if (nxt == 0)
    {
        /*
         *   There's nothing that's next in line for playback.  However,
         *   if we have anything in the queue that can be randomly
         *   started, we can go back and play that now. 
         */
        for (nxt = first_entry_ ; nxt != 0 ; nxt = nxt->next_)
        {
            /* if this can be randomly started, consider starting it */
            if (nxt->get_random_start() != 0)
                break;
        }

        /* if we still didn't find anything, there's nothing to play */
        if (nxt == 0)
            return;
    }

    /*
     *   Check this sound for randomness.  If it has a random start
     *   probability, randomly decide whether to start it; if it fails,
     *   move on to the next sound.
     */
    orig_nxt = nxt;
    for (;;)
    {
        /* if this one isn't random, start it immediately */
        if (nxt->get_random_start() == 0)
            break;

        /* 
         *   choose a random number from 0 to 100 - if it's less than the
         *   start probability, start the sound, otherwise skip it for now 
         */
        if ((rand() % 100) <= nxt->get_random_start())
            break;

        /* 
         *   move on to the next sound, wrapping to the start of the queue
         *   if we reach the end 
         */
        nxt = nxt->next_;
        if (nxt == 0)
            nxt = first_entry_;

        /* if we've looped, we don't want to start anything now */
        if (nxt == orig_nxt)
            return;
    }

    /* remember this as the current sound */
    playing_ = nxt;

    /* start the sound, if we found one */
    if (nxt != 0)
    {
        CHtmlSysSound *snd;
        int repeat;
        
        /*
         *   If the new sound's sequence mode is REPLACE, it means that
         *   everything in the queue before this sound should be removed.  
         */
        if (nxt->get_sequence() == HTML_Attrib_replace)
        {
            /* 
             *   keep removing the first entry until the new sound is the
             *   first entry 
             */
            while (first_entry_ != 0 && first_entry_ != nxt)
                delete_queue_entry(first_entry_);
        }

        /* get the sound to play */
        snd = nxt->get_res()->get_sound();

        /* 
         *   tell next queue in background what we're about to do, so that
         *   it can suspend playback if it has a sound that can't be
         *   played simultaneously with our new sound - since we're in the
         *   foreground, we have priority 
         */
        if (next_bg_queue_ != 0)
            next_bg_queue_->start_fg_sound(snd);

        /* remember its sequence mode */
        seq_mode_ = nxt->get_sequence();

        /*
         *   If we're in REPLACE mode, and this sound is repeated and
         *   non-random, tell the low-level sound player to repeat the
         *   sound; we can allow this because REPLACE-mode non-random
         *   sounds always play back their full number of repetitions
         *   before we can move on to the next sound.  The low-level
         *   player can be more efficient at looping the sound than we
         *   can, so let it control the looping.  
         */
        if ((seq_mode_ == HTML_Attrib_invalid
             || seq_mode_ == HTML_Attrib_replace)
            && nxt->get_random_start() == 0)
        {
            /* get the looping count from the sound */
            repeat = nxt->get_repeat_count();
        }
        else
        {
            /* 
             *   in other sequence modes, repeated sounds don't repeat
             *   until other sounds in the queue have had a chance to
             *   play, so we can't let the low-level player loop these
             *   sounds 
             */
            repeat = 1;
        }

        /* start it playing */
        if (snd->play_sound(win_, sound_done_cb, this, repeat,
                            nxt->get_res()->get_url()))
        {
            /* cancel this sound */
            nxt->cancel();
            
            /* 
             *   playback failed - act as though the current sound has
             *   just stopped with zero repetitions
             */
            on_sound_done(0);
        }
    }
}

/*
 *   Process notification that our sound has finished playing 
 */
void CHtmlSoundQueue::on_sound_done(int repeat_count)
{
    CHtmlSoundQueueEntry *entry;
    CHtmlSoundQueueEntry *nxt;

    /* 
     *   remember this sound's sequence mode -- the sequence mode is taken
     *   from the last sound we played 
     */
    seq_mode_ = (playing_ != 0 ? playing_->get_sequence()
                               : HTML_Attrib_invalid);

    /* remember the sound that played last */
    last_played_ = playing_;

    /* nothing is playing any more */
    playing_ = 0;

    /*
     *   Tell the next background queue that we've just finished this
     *   sound.  If a background queue had to suspend its sound in order
     *   for us to play, the background queue can now resume its sound. 
     */
    if (next_bg_queue_ != 0 && last_played_ != 0)
        next_bg_queue_->end_fg_sound(last_played_->get_res()->get_sound());

    /*
     *   Decrement this sound's repeat count.  If we've exhausted its
     *   repeat count, delete the entry from the queue, since we can't
     *   play it again.  
     */
    if (last_played_ != 0 && last_played_->dec_repeat_count(repeat_count))
        delete_queue_entry(last_played_);

    /*
     *   Check the queue for cancellations, and delete any sounds that
     *   have been cancelled.  
     */
    for (entry = first_entry_ ; entry != 0 ; entry = nxt)
    {
        /* remember the next entry, in case we delete this one */
        nxt = entry->next_;

        /* if this entry has been cancelled, delete it */
        if (entry->cancel_)
            delete_queue_entry(entry);
    }

    /* check to see if there's another sound to play */
    maybe_start_sound();
}

/*
 *   Receive notification that a queue in the foreground is about to start
 *   a new sound.  If the system can't play the new sound and our current
 *   sound at the same time, we must suspend our current sound to make
 *   room for the new sound. 
 */
void CHtmlSoundQueue::start_fg_sound(CHtmlSysSound *fgsnd)
{
    /* 
     *   suspend our sound if necessary - this is system dependent, so we
     *   may not have to do anything 
     */
    if (playing_ != 0
        && playing_->get_res()->get_sound()->maybe_suspend(fgsnd))
    {
        /* 
         *   we had to suspend our sound for this new sound - remember the
         *   foreground sound so that we can resume playback when the
         *   foreground sound ends 
         */
        fg_suspender_ = fgsnd;
    }

    /* tell the next queue in the background about it */
    if (next_bg_queue_ != 0)
        next_bg_queue_->start_fg_sound(fgsnd);
}

/*
 *   Receive notification that a queue in the foreground has just finished
 *   playing a sound.  If we suspended our sound to make room for the
 *   foreground sound, we can now resume playback of our sound. 
 */
void CHtmlSoundQueue::end_fg_sound(CHtmlSysSound *fgsnd)
{
    /*
     *   if we suspended our sound for this sound, and we still have a
     *   sound in our queue, resume our sound now 
     */
    if (fg_suspender_ == fgsnd && playing_ != 0)
        playing_->get_res()->get_sound()->resume();

    /* tell the next queue in the background about it */
    if (next_bg_queue_ != 0)
        next_bg_queue_->end_fg_sound(fgsnd);
}

/*
 *   Timer callback.  This is the static member that we register with the
 *   window; we'll convert the context to an object and invoke the
 *   appropriate member method.  
 */
void CHtmlSoundQueue::timer_func_cb(void *ctx)
{
    /* invoke the timer function in the object */
    ((CHtmlSoundQueue *)ctx)->on_timer();
}

/*
 *   End-of-sound callback 
 */
void CHtmlSoundQueue::sound_done_cb(void *ctx, int repeat_count)
{
    /* invoke the end-of-sound function in the object */
    ((CHtmlSoundQueue *)ctx)->on_sound_done(repeat_count);
}

