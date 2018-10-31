/* $Header: d:/cvsroot/tads/html/win32/tadsmidi.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsmidi.h - TADS MIDI support
Function
  
Notes
  
Modified
  01/17/98 MJRoberts  - Creation
*/

#ifndef TADSMIDI_H
#define TADSMIDI_H

#include <stdlib.h>

/* TADS OS interfaces - for file I/O */
#include <os.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif


/*
 *   MIDI file player buffer 
 */
const size_t midi_file_buf_size = 16*1024;
class CTadsMidiFilePlayerBuffer
{
public:
    CTadsMidiFilePlayerBuffer();
    ~CTadsMidiFilePlayerBuffer();

    /* get the buffer pointer and size */
    unsigned char *get_buf() const { return buf_; }
    size_t get_buf_size() const { return midi_file_buf_size; }

    /* prepare the header; returns zero on success, non-zero on error */
    int prepare_header(HMIDISTRM hmidistrm,
                       class CTadsMidiFilePlayer *player);

    /* play the buffer */
    int play(HMIDISTRM hmidistrm, DWORD buf_used);

    /* unprepare the header */
    void unprepare_header(HMIDISTRM hmidistrm);

private:
    /* MIDI header */
    MIDIHDR midihdr_;

    /* data buffer */
    unsigned char *buf_;

    /* flag indicating whether the header has been prepared */
    int prepared_ : 1;
};

/*
 *   MIDI file player 
 */
class CTadsMidiFilePlayer: public CTadsAudioPlayer
{
public:
    CTadsMidiFilePlayer(class CTadsAudioControl *ctl);
    virtual ~CTadsMidiFilePlayer();

    /* 
     *   Begin playing a MIDI file.  Before calling, the client must seek
     *   the file to the start of the MIDI header (for a .MID file, this
     *   is simply the first byte in the file).
     *   
     *   Returns immediately after starting playback (playback continues
     *   in the background).  Returns zero on success, non-zero on failure.
     *   
     *   If the given callback function is non-null, we'll invoke it with
     *   the given context argument when playback is finished.
     *   
     *   The given window handle will receive MM_MOM_OPEN, MM_MOM_CLOSE,
     *   and MM_MOM_DONE messages.  It is responsible for calling our
     *   static handle_mm_message() function to handle these messages.
     *   The CTadsWin object does this by default, so any CTadsWin
     *   subclass can be used to coordinate MIDI messages without any
     *   additional coding.
     */
    int play(HWND hwnd, osfildef *fp, void (*done_callback)(void *, int),
             void *done_ctx, int repeat);

    /*
     *   Stop playback.  If 'sync' is true, it means that we must not
     *   return until the sound has actually stopped and the callback has
     *   been invoked; otherwise, we may return before the callback is
     *   invoked.  
     */
    void stop(int sync);

    /*
     *   MM_MOM_xxx message handler - our host window must call this
     *   routine whenever it receives an MM_MOM_OPEN, MM_MOM_DONE, or
     *   MM_MOM_CLOSE message.  By default, the CTadsWin message handler
     *   dispatches these messages to us, so in most cases any CTadsWin
     *   subclass should be able to coordinate MIDI events without any
     *   addition coding in the subclass.  
     */
    static int handle_mm_message(UINT msg, WPARAM wpar, LPARAM lpar)
    {
        /* we only need to handle MM_MOM_DONE messages */
        if (msg == MM_MOM_DONE)
        {
            /* 
             *   For MM_MOM_DONE messages, our 'this' object is in the
             *   dwUser member of the MIDIHDR structure, which is in the
             *   lparam.  So, get the MIDIHDR, extract 'this' from the
             *   header, and call our regular mesage handler.
             *   
             *   WARNING - the other events don't pass the MIDIHDR in the
             *   lparam, so we can't get our 'this' pointer this way for
             *   those messages.  Fortunately, we don't need to deal with
             *   those messages, so at the moment the problem is academic
             *   only.  
             */
            ((CTadsMidiFilePlayer *)((MIDIHDR *)lpar)->dwUser)->
                do_midi_cb(MOM_DONE);
        }

        /* handled */
        return 1;
    }

    /*
     *   CTadsAudioPlayer implementation 
     */
    
    /* mute playback */
    void on_mute_change(int mute);

private:
    /* 
     *   Close the MIDI stream, freeing the system MIDI device to play
     *   other MIDI files.  We automatically call this when playback is
     *   completed.  
     */
    void close_stream();

    /* member function reached from callback after parameter translation */
    void do_midi_cb(UINT msg);
    
    /* MIDI stream handle */
    HMIDISTRM hmidistrm_;

    /* MIDI device ID */
    UINT midi_dev_id_;

    /* MIDI file reader */
    class CTadsMidiFileReader *reader_;

    /* flag indicating that we've exhausted the input file */
    int end_of_stream_ : 1;

    /* 
     *   Stream buffers.  We allocate two buffers, so that we can always
     *   have one playing while we're working on filling the other one.
     *   This allows for continuous uninterrupted playback without needing
     *   to load the entire file into memory at once. 
     */
    CTadsMidiFilePlayerBuffer buffers_[2];

    /* next buffer that needs refilling at the 'done' callback */
    int next_to_refill_;

    /* number of buffers still playing */
    int num_playing_;

    /* 
     *   Starting tick position - this is the starting position in the MIDI
     *   stream, expressed as the tick count, from the last time we started
     *   the underlying system sequencer running.  We need this to calculate
     *   where we are globally in the entire stream if we need to interrupt
     *   playback and later restart it, because the underlying system
     *   sequencer can only tell us the elapsed ticks since the last playback
     *   start. 
     */
    unsigned long start_tick_pos_;

    /* flag indicating that we have encountered an error */
    int err_ : 1;

    /* flag indicating that we're waiting for playback to stop */
    int stopping_ : 1;

    /* flag: our output is muted */
    int muted_ : 1;

    /* client callback to be invoked when playback is finished */
    void (*client_done_cb_)(void *, int);
    void *client_done_ctx_;

    /* event handle - we use this to synchronize cancellation */
    HANDLE eventhdl_;

    /* our system audio controller */
    class CTadsAudioControl *audio_control_;
};


/*
 *   MIDI constants
 */

/* non-channel messages */
const unsigned char MIDI_SYSEX = 0xF0;
const unsigned char MIDI_SYSEXEND = 0xF7;
const unsigned char MIDI_META = 0xFF;
const unsigned char MIDI_META_TEMPO = 0x51;
const unsigned char MIDI_META_EOT = 0x2F;

/* channel messages */
const unsigned char MIDI_NOTEOFF = 0x80;
const unsigned char MIDI_NOTEON = 0x90;
const unsigned char MIDI_POLYPRESS = 0xA0;
const unsigned char MIDI_CTRLCHANGE = 0xB0;
const unsigned char MIDI_PRGMCHANGE = 0xC0;
const unsigned char MIDI_CHANPRESS = 0xD0;
const unsigned char MIDI_PITCHBEND = 0xE0;

/* control messages */
const unsigned char MIDICTRL_VOLUME = 0x07;
const unsigned char MIDICTRL_VOLUME_LSB = 0x27;
const unsigned char MIDICTRL_PAN = 0x0A;


/*
 *   MIDI event time type
 */
typedef unsigned long midi_time_t;
const midi_time_t MIDI_TIME_INFINITY = 0xffffffffL;

/*
 *   Event structure.  This is a private structure we use to describe an
 *   event that we've read from the input file.  
 */
class midi_event_t
{
public:
    midi_event_t();
    ~midi_event_t();

    /* 
     *   allocate space for long data; returns zero on success, non-zero
     *   on failure 
     */
    int alloc_long_data(unsigned long siz);
    
    /* time of the event */
    midi_time_t time_;

    /* length of the event */
    unsigned short event_len_;

    /* event type and parameters, for a normal channel message */
    unsigned char short_data_[4];

    /* length of long data */
    unsigned long long_data_len_;

    /* pointer to long data */
    OS_HUGEPTR(unsigned char) long_data_;

    /* allocated size of long data area */
    unsigned long long_data_alo_;
};

/*
 *   Status codes returned by CTadsMidiFileReader::fill_stream() 
 */
enum midi_status_t
{
    /* no error */
    MIDI_STATUS_OK = 0,

    /* reached end of stream */
    MIDI_STATUS_END_STREAM,

    /* output buffer is full */
    MIDI_STATUS_BUFFER_FULL,

    /* output buffer has reached its time limit */
    MIDI_STATUS_TIME_LIMIT,

    /* 
     *   event skipped writing output buffer (this is a success
     *   indication; it simply means that the requested event is of a type
     *   that is not meant to be written to the output buffer, so it was
     *   ignored) 
     */
    MIDI_STATUS_EVENT_SKIPPED,

    /* error */
    MIDI_STATUS_ERROR
};


/*
 *   MIDI file reader.  This object reads and parses a MIDI file.  
 */
class CTadsMidiFileReader
{
public:
    CTadsMidiFileReader();
    ~CTadsMidiFileReader();

    /* 
     *   Read the file header.  Returns zero on success, non-zero if an
     *   error occurred reading the header. 
     */
    int read_header(osfildef *fp);

    /* 
     *   get the time division information (this isn't valid until after
     *   we've read the header) 
     */
    unsigned int get_time_division() const { return time_div_; }

    /* 
     *   Rewind to the start of the file.  Returns zero on success,
     *   non-zero on failure. 
     */
    int rewind();

    /* rewind to a specific time */
    void rewind_to_time(unsigned long tm);

    /* 
     *   Fill a Windows MIDI stream from the file.  *outbufused returns
     *   with the number of bytes of the buffer filled.  We won't put
     *   events occupying more than max_delta_time into the buffer; if we
     *   reach the time limit, we'll return MIDI_STATUS_TIME_LIMIT.  
     */
    midi_status_t fill_stream(unsigned char *outbuf, size_t outbuflen,
                              size_t *outbufused,
                              midi_time_t max_delta_time, int muted);
    
    /* 
     *   Convert a 32-bit value from the MIDI file format to an unsigned
     *   long.  Values in the MIDI file are stored in big-endian format,
     *   so we need to swap the bytes.  
     */
    static unsigned long conv_midi32(const unsigned char *buf)
    {
        return (((unsigned long)buf[0]) << 24)
            + (((unsigned long)buf[1]) << 16)
            + (((unsigned long)buf[2]) << 8)
            + ((unsigned long)buf[3]);
    }

    /*
     *   Convert a 16-bit value from the MIDI file format to an unsigned
     *   int.  Values in the MIDI file are stored in big-endian format, so
     *   we need to swap the bytes. 
     */
    static unsigned int conv_midi16(const unsigned char *buf)
    {
        return (((unsigned int)buf[0]) << 8)
            + ((unsigned long)buf[1]);
    }

private:
    /*
     *   Read the next event into our event buffer.  Returns a stream
     *   status code.  
     */
    midi_status_t read_next_event();

    /*
     *   Add an event to a MIDI stream
     */
    midi_status_t add_to_stream(class midi_event_t *event,
                                struct midi_stream_info_t *stream_info,
                                int muted);
    
    /* number of tracks in the file */
    unsigned int track_cnt_;

    /* time division */
    unsigned int time_div_;

    /* array of tracks - we allocate one per track */
    class CTadsMidiTrackReader *tracks_;

    /* current MIDI stream time */
    midi_time_t cur_time_;

    /* the current event */
    midi_event_t event_;

    /* flag indicating whether we have an event buffered in event_ */
    int have_event_ : 1;

    /* flag indicating that we've reached the end of the MIDI data */
    int end_of_stream_ : 1;

    /* flag indicating that we've encountered an error reading the file */
    int err_ : 1;
};

/* track reader input buffer size */
const size_t midi_track_buf_size = 1024;

/*
 *   MIDI file track reader.  This object provides buffered read access to
 *   an individual track in a MIDI file.  The MIDI file reader object
 *   creates one of these objects per track.
 */
class CTadsMidiTrackReader
{
public:
    CTadsMidiTrackReader();
    ~CTadsMidiTrackReader();

    /* 
     *   Initialize the track.  Returns zero on success, non-zero on
     *   failure. 
     */
    int init(osfildef *fp, unsigned long file_start_ofs,
             unsigned long track_len);

    /* 
     *   rewind to the start of the track; returns zero on success,
     *   non-zero on failure 
     */
    int rewind();

    /* skip to the requested time */
    void skip_to_time(unsigned long tm);

    /*
     *   Read the next event from the track.  Returns zero on success,
     *   non-zero on failure.  
     */
    int read_event(class midi_event_t *event);

    /* 
     *   get the next byte from the track; returns zero on success,
     *   non-zero on error 
     */
    int read_byte(unsigned char *byteptr);

    /*
     *   get the next n bytes from the track 
     */
    int read_bytes(unsigned char *byteptr, unsigned long bytes_to_read);

    /*
     *   read a varying-length value from the track; returns zero on
     *   success, non-zero on error 
     */
    int read_varlen(unsigned long *valptr);

    /* get the number of bytes remaining in the track */
    unsigned long get_bytes_remaining() const
    {
        return track_len_ - track_ofs_;
    }

    /* determine if the given number of bytes are available in the track */
    int bytes_avail(unsigned long cnt) const
    {
        return cnt <= get_bytes_remaining();
    }

    /* are we at the end of the track? */
    int end_of_track() const
    {
        return get_bytes_remaining() == 0;
    }

    /* get the time of the next event in the track */
    midi_time_t get_next_event_time() const { return next_event_time_; }

    /*
     *   Determine if a given channel message has an extra parameter byte.
     *   The "program change" and "channel pressure" events have two
     *   bytes; all other messages have three bytes. 
     */
    static int has_extra_param(unsigned char evttype)
    {
        if (evttype == MIDI_PRGMCHANGE || evttype == MIDI_CHANPRESS)
        {
            /* these have only two bytes - there's not an extra parameter */
            return FALSE;
        }
        else
        {
            /* all others have three bytes, so there is another parameter */
            return TRUE;
        }
    }

private:
    /* bytes remaining in the buffer */
    size_t buf_rem_;

    /* offset of next byte to be read from the buffer */
    size_t buf_ofs_;

    /* current offset in the track */
    unsigned long track_ofs_;

    /* input buffer */
    unsigned char buf_[midi_track_buf_size];

    /* file */
    osfildef *fp_;

    /* starting location in the file */
    unsigned long file_start_pos_;

    /* total number of bytes in track */
    unsigned long track_len_;

    /* time of the next event in the track */
    midi_time_t next_event_time_;

    /* error flag - when set, the track has an error */
    int err_ : 1;

    /* last message status - used for running status */
    unsigned char last_status_;
};


#endif /* TADSMIDI_H */

