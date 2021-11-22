#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsmidi.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsmidi.cpp - TADS MIDI support
Function
  
Notes
  
Modified
  01/17/98 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <string.h>

#include <Windows.h>

/* include the TADS OS layer for I/O routines */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   MIDI file player implementation 
 */

CTadsMidiFilePlayer::CTadsMidiFilePlayer(CTadsAudioControl *ctl)
{
    /* clear out members */
    hmidistrm_ = 0;
    midi_dev_id_ = 0;
    reader_ = 0;
    end_of_stream_ = FALSE;
    num_playing_ = 0;
    err_ = FALSE;

    /* create an event handle, for synchronization when stopping playback */
    eventhdl_ = CreateEvent(0, FALSE, FALSE, "MIDI playback");

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();

    /* note the initial mute status */
    muted_ = ctl->get_mute_sound();
}

CTadsMidiFilePlayer::~CTadsMidiFilePlayer()
{
    /* close the MIDI stream  */
    close_stream();
    
    /* delete the reader */
    if (reader_ != 0)
        delete reader_;

    /* done with the event handle */
    CloseHandle(eventhdl_);

    /* release our audio controller interface */
    audio_control_->audioctl_release();
}

/*
 *   Begin playing a MIDI file.  Returns zero on success, non-zero on
 *   failure.  
 */
int CTadsMidiFilePlayer::play(HWND hwnd, osfildef *fp,
                              void (*done_callback)(void *, int),
                              void *done_ctx, int /*repeat*/)
{
    MIDIPROPTIMEDIV mptd;
    CTadsMidiFilePlayerBuffer *buf;
    int i;
    int num_started;

    /* haven't yet reached the end */
    end_of_stream_ = FALSE;
    stopping_ = FALSE;

    /* no errors yet */
    err_ = FALSE;

    /* no buffers playing yet */
    num_playing_ = 0;
    num_started = 0;

    /* remember the completion callback */
    client_done_cb_ = done_callback;
    client_done_ctx_ = done_ctx;

    /* if we already have a stream, close it */
    close_stream();
    
    /* reset the event we use to wait for the end of the stream */
    ResetEvent(eventhdl_);

    /* always start at tick offset zero in the midi stream */
    start_tick_pos_ = 0;

    /* open a MIDI stream for playing the file */
    midi_dev_id_ = MIDI_MAPPER;
    if (midiStreamOpen(&hmidistrm_, &midi_dev_id_, (DWORD)1,
                       (DWORD)hwnd, (DWORD)this, CALLBACK_WINDOW)
        != MMSYSERR_NOERROR)
        return 1;

    /* delete any existing reader */
    if (reader_ != 0)
        delete reader_;

    /* read the header */
    reader_ = new CTadsMidiFileReader();
    if (reader_->read_header(fp))
        return 2;

    /* set stream properties */
    mptd.cbStruct = sizeof(mptd);
    mptd.dwTimeDiv = reader_->get_time_division();
    if (midiStreamProperty(hmidistrm_, (LPBYTE)&mptd,
                           MIDIPROP_SET | MIDIPROP_TIMEDIV)
        != MMSYSERR_NOERROR)
        return 3;

    /* restart the stream */
    if (midiStreamRestart(hmidistrm_) != MMSYSERR_NOERROR)
        return 4;

    /* now that we're playing, register with the audio controller */
    audio_control_->register_active_sound(this);

    /*
     *   Start the process going.  For each of our two buffers, load up
     *   the buffer with as much data as will fit, and start it playing.  
     */
    for (i = 0, buf = buffers_ ; i < 2 && !end_of_stream_ ; ++i, ++buf)
    {
        size_t buf_used;
        
        /* fill up this buffer */
        switch(reader_->fill_stream(buf->get_buf(), buf->get_buf_size(),
                                    &buf_used, MIDI_TIME_INFINITY, muted_))
        {
        case MIDI_STATUS_END_STREAM:
            /* 
             *   This is the end of the stream - note that we have nothing
             *   more to write, but play back what we loaded into this
             *   buffer 
             */
            end_of_stream_ = TRUE;
            break;

        case MIDI_STATUS_BUFFER_FULL:
        case MIDI_STATUS_TIME_LIMIT:
            /*
             *   we've filled up this buffer, and we have more left to
             *   write; play back this buffer and keep going 
             */
            break;

        default:
            /* error - can't proceed; abort */
            err_ = TRUE;
            break;
        }

        /* if there's an error, stop */
        if (err_)
            break;

        /* prepare this buffer header */
        if (buf->prepare_header(hmidistrm_, this))
        {
            err_ = TRUE;
            break;
        }

        /*
         *   Add the current buffer to the playback list in the MIDI
         *   stream driver 
         */
        if (buf->play(hmidistrm_, buf_used))
        {
            /* note the error internally, and return failure */
            err_ = TRUE;
            break;
        }

        /* count the playing buffer */
        ++num_playing_;
        ++num_started;
    }

    /* check to see if we encountered an error before starting any buffers */
    if (err_ && num_started == 0 && done_callback != 0)
    {
#if 0
        // $$$ this isn't actually necessary - the caller will know to
        //     invoke the callback because of the play-start error
        /*  
         *   Call the done callback immediately so that the caller knows
         *   the callback is finished.
         */
        (*done_callback)(done_ctx, 0);
#endif

        /* return an error */
        return 5;
    }

    /*
     *   The first buffer will finish before the second buffer starts
     *   playing, so the first completion callback we'll get will be for
     *   the first buffer.  So, this is the next buffer we will re-fill. 
     */
    next_to_refill_ = 0;

    /* success */
    return 0;
}

/*
 *   Stop playback.  The client done callback will still be invoked in the
 *   usual manner.  This may return before playback has actually
 *   completed, unless 'sync' is set to true, in which case we must wait
 *   until the sound has been cancelled and the callback has been invoked
 *   before we can return.  
 */
void CTadsMidiFilePlayer::stop(int sync)
{
    /* set the stop flag so we know not to start another buffer */
    stopping_ = TRUE;

    /* tell the stream to stop playing */
    midiStreamStop(hmidistrm_);

    /* 
     *   if the 'sync' flag is set, we must wait here until the sound
     *   actually stops 
     */
    while (sync)
    {
        DWORD id;
        MSG msg;
        
        /* 
         *   Wait for our event handle, allowing a 5-second timeout.  Stop
         *   waiting if there's a posted message in the queue, since we want
         *   to process any MM_MOM_DONE messages that the midi stream posts
         *   when it's finished canceling the playback. 
         */
        id = MsgWaitForMultipleObjects(1, &eventhdl_, FALSE, 5000,
                                       QS_ALLPOSTMESSAGE);

        /* check what happened */
        switch (id)
        {
        case WAIT_TIMEOUT:
        case WAIT_OBJECT_0:
        default:
            /* 
             *   We either timed out or our event was signalled - in either
             *   case, we're done synchronizing, so stop waiting.  (We also
             *   stop waiting if we get any event we don't otherwise process,
             *   to ensure we don't park here forever if something unexpected
             *   happens.)  
             */
            sync = FALSE;
            break;

        case WAIT_OBJECT_0 + 1:
            /* a posted event is in the queue - process any we find */
            while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            {
                /* translate and dispatch the message */
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            /* 
             *   we've emptied out the queue; go back and check to see if the
             *   playback has finished yet 
             */
            break;
        }
    }
}

/*
 *   Set the muting status 
 */
void CTadsMidiFilePlayer::on_mute_change(int mute)
{
    MMTIME tm;
    
    /* note the new status */
    muted_ = mute;

    /* get the current playback time */
    tm.wType = TIME_TICKS;
    midiStreamPosition(hmidistrm_, &tm, sizeof(tm));

    /* 
     *   The current playback time only tells us how many ticks we've been
     *   playing since we last started, so we need to add this to the base
     *   tick position from the last time we started play to get the rewind
     *   position.  This is also the new base position for the next time we
     *   need to rewind and restart. 
     */
    start_tick_pos_ += tm.u.ticks;

    /* stop the stream */
    midiStreamStop(hmidistrm_);

    /* rewind the file */
    reader_->rewind_to_time(start_tick_pos_);

    /* restart the stream from the current position */
    midiStreamRestart(hmidistrm_);
}

/*
 *   Close the stream
 */
void CTadsMidiFilePlayer::close_stream()
{
    int i;
    CTadsMidiFilePlayerBuffer *buf;

    /* if we don't have a stream open, there's nothing to do */
    if (hmidistrm_ == 0)
        return;

    /* reset the midi output device */
    midiOutReset((HMIDIOUT)hmidistrm_);

    /* unprepare all of the headers */
    for (i = 0, buf = buffers_ ; i < 2 ; ++i, ++buf)
        buf->unprepare_header(hmidistrm_);

    /* close the stream */
    midiStreamClose(hmidistrm_);

    /* we're no longer playing, so unregister with the controller */
    audio_control_->unregister_active_sound(this);

    /* forget the stream */
    hmidistrm_ = 0;
}

/*
 *   MIDI callback - this is a member function reached from the static
 *   callback 
 */
void CTadsMidiFilePlayer::do_midi_cb(UINT msg)
{
    /* check the message */
    switch(msg)
    {
    case MOM_DONE:
        /*
         *   We're finished playing back a stream.  If we're not at the
         *   end of the input yet, read the next buffer and start it
         *   playing; otherwise, simply note that the buffer is done.  
         */

        /* that's one less buffer playing */
        --num_playing_;

        /* 
         *   if we're not at the end of the stream, and there hasn't been
         *   an error, and we're not stopping playback, load up the next
         *   buffer and start it playing 
         */
        if (!end_of_stream_ && !err_ && !stopping_)
        {
            size_t buf_used;
            CTadsMidiFilePlayerBuffer *buf;

            /* fill up the next buffer */
            buf = &buffers_[next_to_refill_];
            switch(reader_->fill_stream(buf->get_buf(), buf->get_buf_size(),
                                        &buf_used, MIDI_TIME_INFINITY,
                                        muted_))
            {
            case MIDI_STATUS_END_STREAM:
                /* 
                 *   This is the end of the stream - note that we have
                 *   nothing more to write, but play back what we loaded
                 *   into this buffer 
                 */
                end_of_stream_ = TRUE;
                break;

            case MIDI_STATUS_BUFFER_FULL:
            case MIDI_STATUS_TIME_LIMIT:
                /*
                 *   we've filled up this buffer, and we have more left to
                 *   write; play back this buffer and keep going 
                 */
                break;

            default:
                /* error - can't proceed - note the error and continue */
                err_ = TRUE;
                break;
            }

            /*
             *   If we didn't encounter an error, add the current buffer
             *   to the playback list in the MIDI stream driver 
             */
            if (!err_ && buf->play(hmidistrm_, buf_used))
            {
                /* note the error internally */
                err_ = TRUE;
            }

            /* if we started a new buffer playing, count it */
            if (!err_)
            {
                /* that's another buffer playing */
                ++num_playing_;

                /* advance to the next buffer to fill */
                ++next_to_refill_;
                if (next_to_refill_ > 1)
                    next_to_refill_ = 0;
            }
        }

        /* 
         *   check to see if that's the last buffer - if so, we've
         *   finished playback 
         */
        if (num_playing_ == 0)
        {
            /* 
             *   Close our stream.  Do this *before* invoking the client
             *   callback, since we could just start playing the same
             *   sound over again. 
             */
            close_stream();

            /* set our event flag to indicate that we're finished */
            SetEvent(eventhdl_);

            /* 
             *   We're done with playback -- if there's a client callback
             *   for notification of the end of playback, invoke it now.
             *   Note that we ignore the caller's repeat count request, so
             *   we'll always indicate that we've played back once.  
             */
            if (client_done_cb_ != 0)
                (*client_done_cb_)(client_done_ctx_, 1);
        }

        /* done */
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   File player buffer implementation
 */

CTadsMidiFilePlayerBuffer::CTadsMidiFilePlayerBuffer()
{
    /* allocate the data buffer */
    buf_ = new unsigned char[midi_file_buf_size];

    /* note yet prepared */
    prepared_ = FALSE;
}

CTadsMidiFilePlayerBuffer::~CTadsMidiFilePlayerBuffer()
{
    /* free the data buffer */
    delete buf_;
}

/*
 *   prepare the MIDI header 
 */
int CTadsMidiFilePlayerBuffer::prepare_header(HMIDISTRM hmidistrm,
                                              CTadsMidiFilePlayer *player)
{
    /* point the header to the data buffer */
    midihdr_.lpData = (LPSTR)buf_;
    midihdr_.dwBufferLength = midi_file_buf_size;
    midihdr_.dwBytesRecorded = 0;

    /* 
     *   store the file player object in the user data in the header -
     *   this allows the message handler, which receives the MIDIHDR as
     *   its parameter, to locate the CTadsMidiFilePlayer object to
     *   dispatch the event to the player event callback 
     */
    midihdr_.dwUser = (DWORD)player;

    /* initialize the rest of the header */
    midihdr_.dwFlags = 0;
    midihdr_.lpNext = 0;
    midihdr_.reserved = 0;
    midihdr_.dwOffset = 0;
    memset(midihdr_.dwReserved, 0, sizeof(midihdr_.dwReserved));

    /* ask the MIDI system to prepare the header */
    if (midiOutPrepareHeader((HMIDIOUT)hmidistrm, &midihdr_, sizeof(midihdr_))
        != MMSYSERR_NOERROR)
        return 1;

    /* note that we're prepared now */
    prepared_ = TRUE;

    /* success */
    return 0;
}

/*
 *   play the buffer 
 */
int CTadsMidiFilePlayerBuffer::play(HMIDISTRM hmidistrm, DWORD buf_used)
{
    /* set the amount of data we're using in the buffer */
    midihdr_.dwBytesRecorded = buf_used;

    /* write the buffer to the output */
    if (midiStreamOut(hmidistrm, &midihdr_, sizeof(midihdr_))
        != MMSYSERR_NOERROR)
        return 1;

    /* success */
    return 0;
}

/*
 *   unprepare the MIDI header 
 */
void CTadsMidiFilePlayerBuffer::unprepare_header(HMIDISTRM hmidistrm)
{
    /* if we're not prepared, ignore the call */
    if (!prepared_)
        return;
    
    /* tell MIDI we're done with the header */
    midiOutUnprepareHeader((HMIDIOUT)hmidistrm, &midihdr_, sizeof(midihdr_));
    prepared_ = FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   MIDI File Reader implementation 
 */

CTadsMidiFileReader::CTadsMidiFileReader()
{
    /* we don't have any tracks initially */
    track_cnt_ = 0;
    tracks_ = 0;
}

CTadsMidiFileReader::~CTadsMidiFileReader()
{
    /* if we have any tracks, delete them */
    if (tracks_ != 0)
        delete [] tracks_;
}

/*
 *   Read a MIDI file header and set up our track list.  This must be used
 *   before we can start playing sounds from a MIDI file.  Before calling
 *   this, seek to the location of the file header.  
 */
int CTadsMidiFileReader::read_header(osfildef *fp)
{
    unsigned char hdrbuf[14];
    unsigned int midi_fmt;
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* start out at time zero */
    cur_time_ = 0;

    /* we don't have an event yet */
    have_event_ = FALSE;

    /* not at end of stream yet */
    end_of_stream_ = FALSE;

    /* no error yet */
    err_ = FALSE;

    /* 
     *   read the MThd header: "MThd", followed by big-endian 32-bit
     *   header size, followed by the file header.
     */
    if (osfrb(fp, hdrbuf, sizeof(hdrbuf)))
        return 1;

    /* 
     *   make sure the MThd is present, and make sure the header size
     *   matches what we expect 
     */
    if (memcmp(hdrbuf, "MThd", 4) != 0 || conv_midi32(&hdrbuf[4]) != 6)
        return 2;

    /* decode the header fields */
    midi_fmt = conv_midi16(&hdrbuf[8]);
    track_cnt_ = conv_midi16(&hdrbuf[10]);
    time_div_ = conv_midi16(&hdrbuf[12]);

    /* allocate space for the tracks, now that we know how many there are */
    tracks_ = new CTadsMidiTrackReader[track_cnt_];

    /* read each track header */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
    {
        unsigned long track_len;
        unsigned long track_ofs;
        
        /* read the track header */
        if (osfrb(fp, hdrbuf, 8))
            return 3;

        /* make sure the header has the "MTrk" signature */
        if (memcmp(hdrbuf, "MTrk", 4) != 0)
            return 4;

        /* get the length of the track */
        track_len = conv_midi32(&hdrbuf[4]);

        /* note the start of the track */
        track_ofs = osfpos(fp);

        /* initialize the track */
        if (track->init(fp, track_ofs, track_len))
            return 5;

        /* skip the rest of the track data */
        osfseek(fp, track_ofs + track_len, OSFSK_SET);
    }

    /* success */
    return 0;
}

/*
 *   Rewind to the start of the MIDI file 
 */
int CTadsMidiFileReader::rewind()
{
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* rewind each track */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
    {
        /* rewind this track; return an error if the track does */
        if (track->rewind())
            return 1;
    }

    /* we don't have an event buffered now */
    have_event_ = FALSE;

    /* not at end of stream yet */
    end_of_stream_ = FALSE;

    /* no error yet */
    err_ = FALSE;

    /* success */
    return 0;
}

/*
 *   rewind to a specific time 
 */
void CTadsMidiFileReader::rewind_to_time(unsigned long tm)
{
    unsigned int i;
    CTadsMidiTrackReader *track;

    /* rewind completely */
    rewind();

    /* set our time to the requested time */
    cur_time_ = tm;

    /* skip ahead on each track to the requested time */
    for (i = 0, track = tracks_ ; i < track_cnt_ ; ++i, ++track)
        track->skip_to_time(tm);
}

/*
 *   output stream descriptor - this is a private structure that we use to
 *   keep track of our position in an output stream as we fill it 
 */
struct midi_stream_info_t
{
    /* starting time for the current block of data in the stream */
    midi_time_t start_time_;

    /* maximum delta time allowed for the stream */
    midi_time_t max_delta_time_;

    /* current write pointer */
    unsigned char *bufptr_;

    /* space remaining in the buffer */
    size_t buflen_;
};

/*
 *   Read data from the MIDI file into a Windows MIDI stream.  This reads
 *   the file and converts information in the file into MIDI stream data.  
 */
midi_status_t CTadsMidiFileReader::fill_stream(unsigned char *outbuf,
                                               size_t outbuflen,
                                               size_t *outbufused,
                                               midi_time_t max_delta_time,
                                               int muted)
{
    midi_status_t stat;
    midi_stream_info_t info;
    int done;

    /* if we have an error, we can't proceed */
    if (err_)
        return MIDI_STATUS_ERROR;

    /* set up our stream descriptor structure */
    info.start_time_ = cur_time_;
    info.max_delta_time_ = max_delta_time;
    info.bufptr_ = outbuf;
    info.buflen_ = outbuflen;

    /* keep going until we encounter an error or fill up the buffer */
    for (done = FALSE ; !done ; )
    {
        /* get the next event */
        stat = read_next_event();
        switch(stat)
        {
        case MIDI_STATUS_END_STREAM:
            /* reached end of stream without an error */
            done = TRUE;
            break;

        case MIDI_STATUS_OK:
            /* no problem; keep going */
            break;

        default:
            /* note that we've encountered an error */
            err_ = TRUE;
            
            /* return the error */
            done = TRUE;
            break;
        }

        /* exit the loop if we're done */
        if (done)
            break;
        
        /* add this event to the stream buffer */
        stat = add_to_stream(&event_, &info, muted);
        switch(stat)
        {
        case MIDI_STATUS_OK:
            /* success - keep going */
            break;

        case MIDI_STATUS_EVENT_SKIPPED:
            /* this event was skipped - go on to the next */
            break;

        case MIDI_STATUS_BUFFER_FULL:
        case MIDI_STATUS_TIME_LIMIT:
            /* 
             *   we've filled up the buffer, either in space or time;
             *   there's no error, so we can keep going on a subsequent
             *   call, but it's time to stop for now 
             */
            done = TRUE;
            break;

        default:
            /* 
             *   other conditions are errors - we can't proceed now or on
             *   a subsequent call 
             */
            err_ = TRUE;
            done = TRUE;
            break;
        }

        /* exit the loop if we're done */
        if (done)
            break;
        
        /* we've consumed the current event */
        have_event_ = FALSE;
    }

    /* indicate how many bytes we've consumed */
    *outbufused = outbuflen - info.buflen_;

    /* return the status code that made us stop looping */
    return stat;
}

/*
 *   Read an event into our event buffer 
 */
midi_status_t CTadsMidiFileReader::read_next_event()
{
    unsigned int i;
    CTadsMidiTrackReader *track;
    CTadsMidiTrackReader *next_track;
    midi_time_t min_time;
    
    /* if we already have an event, there's nothing more to do now */
    if (have_event_)
        return MIDI_STATUS_OK;

    /* keep going until we get an event or encounter an error */
    for (;;)
    {
        /* 
         *   scan each track and find the one with the event nearest to
         *   the current time 
         */
        for (i = 0, track = tracks_,
             min_time = MIDI_TIME_INFINITY, next_track = 0 ;
             i < track_cnt_ ; ++i, ++track)
        {
            /* 
             *   Check to see if this track is the soonest so far
             *   (ignoring any tracks that have reached their ends) 
             */
            if (!track->end_of_track()
                && track->get_next_event_time() < min_time)
            {
                /* this is the best one so far */
                min_time = track->get_next_event_time();
                next_track = track;
            }
        }
        
        /* if we didn't find anything, the whole stream is at an end */
        if (next_track == 0)
        {
            end_of_stream_ = TRUE;
            return MIDI_STATUS_END_STREAM;
        }
        
        /* get the event from the track */
        if (next_track->read_event(&event_))
        {
            /* 
             *   couldn't read the event - set our error flag and return
             *   failure 
             */
            err_ = TRUE;
            return MIDI_STATUS_ERROR;
        }

        /*
         *   If this is an end-of-track event, go back for another event,
         *   since we may have another track with more data.  
         */
        if (event_.short_data_[0] == MIDI_META
            && event_.short_data_[1] == MIDI_META_EOT)
            continue;
        
        /* we now have an event */
        have_event_ = TRUE;
        
        /* success */
        return MIDI_STATUS_OK;
    }
}

/*
 *   Add an event to a stream buffer 
 */
midi_status_t CTadsMidiFileReader::add_to_stream(
    midi_event_t *event, midi_stream_info_t *info, int muted)
{
    midi_time_t now;
    midi_time_t delta;
    MIDIEVENT *outevt;
    size_t outlen;
    
    /* if this event would exceed the time limit, we're done */
    if (cur_time_ - info->start_time_ > info->max_delta_time_)
        return MIDI_STATUS_TIME_LIMIT;

    /* note the current time */
    now = cur_time_;

    /* compute the delta time for the track so far */
    delta = event->time_ - cur_time_;

    /* the current time is this event's time */
    cur_time_ = event->time_;

    /* set up a pointer to the event in the output stream */
    outevt = (MIDIEVENT *)info->bufptr_;

    /* presume we won't write anything */
    outlen = 0;

    /* check the event type */
    if (event->short_data_[0] < MIDI_SYSEX)
    {
        /* 
         *   it's a channel message - copy it directly 
         */

        /* make sure we have room */
        outlen = 3 * sizeof(DWORD);
        if (info->buflen_ < outlen)
            return MIDI_STATUS_BUFFER_FULL;

        /* 
         *   if it's a NOTE ON message, and we're muted, change it to a NOTE
         *   OFF message instead, so that we consume the required time but
         *   don't actually play any music 
         */
        if (muted && (event->short_data_[0] & MIDI_NOTEON) == MIDI_NOTEON)
            event->short_data_[0] = (event->short_data_[0] & 0x0F)
                                    | MIDI_NOTEOFF;

        /* write the data, encoding it with a SHORT flag */
        outevt->dwDeltaTime = delta;
        outevt->dwStreamID = 0;
        outevt->dwEvent = ((DWORD)event->short_data_[0])
                          + (((DWORD)event->short_data_[1]) << 8)
                          + (((DWORD)event->short_data_[2]) << 16)
                          + MEVT_F_SHORT;
    }
    else if (event->short_data_[0] == MIDI_SYSEX
             || event->short_data_[0] == MIDI_SYSEXEND)
    {
        /*
         *   System Exclusive event 
         */
        
        /* ignore system exclusive events */
        return MIDI_STATUS_EVENT_SKIPPED;
    }
    else
    {
        /*
         *   Meta events 
         */

        /* make sure it's what we were expecting */
        if (event->short_data_[0] != MIDI_META)
            return MIDI_STATUS_ERROR;

        /* check what type of meta event it is */
        switch(event->short_data_[1])
        {
        case MIDI_META_TEMPO:
            /* 
             *   tempo-change event 
             */

            /* make sure we have our three bytes of parameters */
            if (event->long_data_len_ != 3)
                return MIDI_STATUS_ERROR;

            /* we need space for 3 DWORD's in the output stream */
            outlen = 3 * sizeof(DWORD);
            if (info->buflen_ < outlen)
                return MIDI_STATUS_BUFFER_FULL;

            /* write out the event */
            outevt->dwDeltaTime = delta;
            outevt->dwStreamID = 0;
            outevt->dwEvent = ((DWORD)event->long_data_[2])
                              + (((DWORD)event->long_data_[1]) << 8)
                              + (((DWORD)event->long_data_[0]) << 16)
                              + (((DWORD)MEVT_TEMPO) << 24)
                              + MEVT_F_SHORT;

            /* done */
            break;

        default:
            /* ignore all other meta events */
            return MIDI_STATUS_EVENT_SKIPPED;
        }
    }


    /* advance pointers */
    info->buflen_ -= outlen;
    info->bufptr_ += outlen;

    /* success */
    return MIDI_STATUS_OK;
}


/* ------------------------------------------------------------------------ */
/*
 *   Track Reader implementation
 */

CTadsMidiTrackReader::CTadsMidiTrackReader()
{
    /* no information yet */
    fp_ = 0;
    file_start_pos_ = 0;
    track_len_ = 0;

    /* no error yet */
    err_ = FALSE;
}

CTadsMidiTrackReader::~CTadsMidiTrackReader()
{
}

/*
 *   Initialize the track 
 */
int CTadsMidiTrackReader::init(osfildef *fp, unsigned long file_start_ofs,
                               unsigned long track_len)
{
    /* remember our file information */
    fp_ = fp;
    file_start_pos_ = file_start_ofs;
    track_len_ = track_len;

    /* start at the beginning of the file */
    return rewind();
}

/*
 *   Rewind to the beginning of the track 
 */
int CTadsMidiTrackReader::rewind()
{
    unsigned long val;

    /* dump our buffer, and go to the beginning of the track */
    buf_rem_ = 0;
    track_ofs_ = 0;

    /* reset error indication */
    err_ = FALSE;

    /* reset running status byte */
    last_status_ = 0;

    /* if the track length is zero, there's nothing more to do */
    if (track_len_ == 0)
        return 0;

    /* read the time of the first event */
    if (read_varlen(&val))
    {
        /* read failed - set error flag and return failure */
        err_ = TRUE;
        return 1;
    }

    /* remember the time */
    next_event_time_ = (midi_time_t)val;

    /* success */
    return 0;
}

/*
 *   skip ahead to the requested event time 
 */
void CTadsMidiTrackReader::skip_to_time(unsigned long tm)
{
    midi_event_t event;

    /* read and discard events until we get to the target time */
    while (!end_of_track() && next_event_time_ < tm)
        read_event(&event);
}

/*
 *   Read the next event
 */
int CTadsMidiTrackReader::read_event(midi_event_t *event)
{
    unsigned char evttype;
    
    /* if we're at the end of the track, we can't read anything */
    if (end_of_track())
        return 1;

    /* get the first byte of the event - this specifies the event type */
    if (read_byte(&evttype))
        return 2;

    /* clear out the short data area */
    memset(event->short_data_, 0, sizeof(event->short_data_));

    /* determine what type of message we have */
    if ((evttype & 0x80) == 0)
    {
        /* 
         *   The high bit is not set, so this is a channel message that
         *   keeps the status byte from the previous message 
         */

        /* it's an error if there's no running status */
        if (last_status_ == 0)
        {
            err_ = TRUE;
            return 3;
        }

        /* set the running status and event type data */
        event->short_data_[0] = last_status_;
        event->short_data_[1] = evttype;

        /* get rid of the channel info - get event type */
        evttype = last_status_ & 0xF0;

        /* presume the event length will be 2 bytes */
        event->event_len_ = 2;

        /* if this event has an extra parameter byte, read it */
        if (has_extra_param(evttype))
        {
            /* read the extra byte */
            if (read_byte(&event->short_data_[2]))
                return 4;

            /* note the extra byte in the event length */
            event->event_len_++;
        }
    }
    else if ((evttype & 0xF0) != MIDI_SYSEX)
    {
        /*
         *   The high bit is set, so this message includes a new status
         *   byte, and the message is not a system exclusive message, so
         *   this must be a normal channel message. 
         */

        /* store the status byte, and remember it as the new running status */
        event->short_data_[0] = evttype;
        last_status_ = evttype;

        /* get rid of channel part - get event type */
        evttype &= 0xF0;

        /* presume a two-byte event */
        event->event_len_ = 2;

        /* read the second byte */
        if (read_byte(&event->short_data_[1]))
            return 5;

        /* read the third byte if present */
        if (has_extra_param(evttype))
        {
            /* read the extra byte */
            if (read_byte(&event->short_data_[2]))
                return 6;

            /* count it */
            event->event_len_++;
        }
    }
    else if (evttype == MIDI_SYSEX || evttype == MIDI_SYSEXEND)
    {
        /* 
         *   This is a system exclusive message.  Following the type byte
         *   is a variable-length value specifying the number of bytes of
         *   data that follows, then comes the message data.
         */

        /* remember the event type */
        event->short_data_[0] = evttype;

        /* get the length of the data block */
        if (read_varlen(&event->long_data_len_))
            return 7;

        /* allocate space in the event structure for the long data */
        if (event->alloc_long_data(event->long_data_len_))
        {
            /* note the error and return failure */
            err_ = TRUE;
            return 8;
        }

        /* read the data */
        if (read_bytes(event->long_data_, event->long_data_len_))
            return 9;
    }
    else if (evttype == MIDI_META)
    {
        /*
         *   Meta event.  The next byte specifies the event class, then
         *   comes a varying-length value specifying the size of the
         *   parameters, and finally comes the bytes of the parameter
         *   block. 
         */
        event->short_data_[0] = evttype;

        /* read the class byte */
        if (read_byte(&event->short_data_[1]))
            return 10;

        /* read the length of the long data */
        if (read_varlen(&event->long_data_len_))
            return 11;

        /* if we have any data, read it */
        if (event->long_data_len_ != 0)
        {
            /* make sure there's room for the long data */
            if (event->alloc_long_data(event->long_data_len_))
            {
                /* note the error and return failure */
                err_ = TRUE;
                return 12;
            }
            
            /* read the long data */
            if (read_bytes(event->long_data_, event->long_data_len_))
                return 13;
        }

        /* 
         *   if this is an end-of-track message, note that we're at the
         *   end 
         */
        if (event->short_data_[1] == MIDI_META_EOT)
        {
            /* 
             *   set our read pointer to the end of the data, so that we
             *   know not to read anything more 
             */
            track_ofs_ = track_len_;
        }
    }
    else
    {
        /* invalid message */
        err_ = TRUE;
        return 14;
    }

    /* use the pre-fetched event time */
    event->time_ = next_event_time_;

    /* 
     *   If we're not at the end of the track, read the time of the next
     *   event.  Note that the timestamp is a delta to the start of the
     *   next event, so we must add the delta to the last event's time. 
     */
    if (!end_of_track())
    {
        unsigned long longval;

        /* read the delta */
        if (read_varlen(&longval))
            return 15;

        /* 
         *   add it to the time of the last event to get the time of the
         *   next event 
         */
        next_event_time_ += (midi_time_t)longval;
    }

    /* success */
    return 0;
}

/*
 *   Read the next byte 
 */
int CTadsMidiTrackReader::read_byte(unsigned char *byteptr)
{
    /* if the buffer is empty, refill it */
    if (buf_rem_ == 0)
    {
        size_t readlen;
        
        /* if there's an error, we can't proceed */
        if (err_)
            return 1;

        /* fill the buffer if possible, or read the rest of the track */
        readlen = get_bytes_remaining();
        if (readlen > sizeof(buf_))
            readlen = sizeof(buf_);

        /* seek to the proper position in the file and read the bytes */
        osfseek(fp_, file_start_pos_ + track_ofs_, OSFSK_SET);
        if (readlen == 0 || osfrb(fp_, buf_, readlen))
        {
            /* read failed - go to eof and note the error */
            track_ofs_ = track_len_;
            err_ = TRUE;

            /* return an error */
            return 2;
        }
        
        /* the next byte to be read is the first byte in the buffer */
        buf_ofs_ = 0;
        buf_rem_ = readlen;
    }

    /* return the next byte in the buffer */
    *byteptr = buf_[buf_ofs_];

    /* bump our buffer and file offset */
    ++buf_ofs_;
    ++track_ofs_;

    /* decrement remaining buffer length */
    --buf_rem_;

    /* success */
    return 0;
}

/*
 *   Read a given number of bytes from the track 
 */
int CTadsMidiTrackReader::read_bytes(unsigned char *buf, unsigned long len)
{
    /* if we don't have enough left in the file, fail */
    if (!bytes_avail(len))
    {
        err_ = TRUE;
        return 1;
    }

    /* first, read out of our buffer */
    if (buf_rem_ != 0)
    {
        unsigned long amt;

        /* 
         *   read the lesser of the amount requested and the space left in
         *   the buffer 
         */
        amt = len;
        if (buf_rem_ < amt)
            amt = buf_rem_;

        /* copy bytes out of the buffer */
        memcpy(buf, buf_ + buf_ofs_, (size_t)amt);

        /* advance the destination pointer past the copied bytes */
        buf += amt;
        len -= amt;

        /* adjust counters */
        buf_ofs_ += (size_t)amt;
        track_ofs_ += amt;
        buf_rem_ -= (size_t)amt;
    }

    /* 
     *   if we couldn't satisfy the entire request from the buffer, read
     *   any remainder directly from the file 
     */
    while (len != 0)
    {
        size_t amt;
        
        /* in deference to 16-bit machines, read only up to 16k at a time */
        amt = 16*1024;
        if (len < amt)
            amt = len;

        /* read the data */
        osfseek(fp_, file_start_pos_ + track_ofs_, OSFSK_SET);
        if (osfrb(fp_, buf, amt))
        {
            err_ = TRUE;
            return 2;
        }

        /* advance the destination pointer by the amount read */
        buf += amt;
        len -= amt;

        /* adjust our file position counter */
        track_ofs_ += amt;
    }

    /* success */
    return 0;
}

/*
 *   Read a variable-length value from the stream 
 */
int CTadsMidiTrackReader::read_varlen(unsigned long *valptr)
{
    unsigned long acc;

    /* start off with the accumulator at zero */
    acc = 0;
    
    /* read bytes as long as we're not at EOF and there's more to read */
    for (;;)
    {
        unsigned char cur;
        
        /* if at EOF, abort with an error */
        if (end_of_track())
        {
            /* set error flag and return failure */
            err_ = TRUE;
            return 1;
        }

        /* read the next byte */
        if (read_byte(&cur))
            return 2;

        /* 
         *   Add it to the accumulator.  Each byte in the data stream
         *   encodes 7 bits of the value, starting with the highest-order
         *   byte, plus an extra bit (the high-order bit) indicating
         *   whether another byte follows. 
         */
        acc <<= 7;
        acc += (cur & 0x7f);

        /* if the 'continue' bit is not set, we're done */
        if ((cur & 0x80) == 0)
            break;
    }

    /* return the result */
    *valptr = acc;
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   MIDI event structure 
 */

midi_event_t::midi_event_t()
{
    /* no long data yet */
    long_data_ = 0;
    long_data_alo_ = 0;
}

midi_event_t::~midi_event_t()
{
    /* delete any long data we have */
    if (long_data_ != 0)
        os_free_huge(long_data_);
}

/*
 *   Allocate space for a given amount of long data, if we don't have
 *   space for this much already. 
 */
int midi_event_t::alloc_long_data(unsigned long siz)
{
    if (long_data_alo_ < siz)
    {
        /* free any existing long data area */
        if (long_data_ != 0)
            os_free_huge(long_data_);

        /* allocate space for the given amount of data */
        long_data_ = (OS_HUGEPTR(unsigned char))os_alloc_huge(siz);
        if (long_data_ == 0)
            return 1;

        /* note the new size of the long data */
        long_data_alo_ = siz;
    }

    /* success */
    return 0;
}

