/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscsnd.h - TADS Compressed Sound base class
Function
  This is a base class for compressed audio datatypes, such as MPEG and
  Ogg Vorbis.  This class provides the basic framework for playing back
  compressed audio by streaming it from disk, through the decoder, and
  into a DirectX buffer.
Notes
  
Modified
  04/26/02 MJRoberts  - Creation
*/

#ifndef TADSCSND_H
#define TADSCSND_H

#include <stdio.h>
#include <string.h>

#include <Windows.h>
#include <dsound.h>

#include "tadshtml.h"
#include "tadssnd.h"


/* ------------------------------------------------------------------------ */
/*
 *   TADS Compressed Audio class.  This is designed as a mix-in class that
 *   can be multiply inherited into a class implementing a specific decoder.
 */
class CTadsCompressedAudio: public CTadsAudioPlayer
{
public:
    /* -------------------------------------------------------------------- */
    /*
     *   HTML TADS interface.  The HTML TADS compressed audio objects use
     *   this interface to control sound playback.  
     */

    /* destroy */
    virtual ~CTadsCompressedAudio();

    /* play back a file */
    int play(const textchar_t *url, HANDLE hfile, DWORD file_size,
             HWND hwnd, UINT done_msg, LPARAM done_msg_lpar);

    /* wait until playback is finished */
    void wait_until_done();

    /* determine if we're still playing */
    int is_playing();

    /* halt playback */
    void stop(int sync);

    /*
     *   CTadsAudioPlayer implementation
     */
    
    /* turn muting on or off */
    void on_mute_change(int mute);

protected:
    /* -------------------------------------------------------------------- */
    /*
     *   Decoder overrides.  This is the interface the decoder subclass must
     *   implement. 
     */

    /* 
     *   Decode the file.  This should simply decode the entire file; it
     *   should open the audio buffer once it knows the playback format,
     *   then decode the data and write it to the playback buffer as it
     *   does.  Data should be written in small chunks to ensure that
     *   decoding stays ahead of playback. 
     */
    virtual void do_decoding(HANDLE hfile, DWORD file_size) = 0;

    /*
     *   Get/set the decoder 'stop' flag.  This flag tells us we should stop
     *   playback as soon as possible. 
     */
    virtual int get_decoder_stopping() = 0;
    virtual void set_decoder_stopping(int f) = 0;

    /* -------------------------------------------------------------------- */
    /*
     *   Decoder interface - this is for the decoder subclass's use 
     */
    
    /* initialize */
    CTadsCompressedAudio(IDirectSound *ds, class CTadsAudioControl *ctl);

    /* open the output audio buffer */
    int open_playback_buffer(int freq, int bits_per_sample,
                             int number_of_channels);

    /* close the output audio buffer */
    void close_playback_buffer();

    /* write decoded PCM data to the playback buffer */
    void write_playback_buffer(char *buf, int bytes);

    /* halt the playback of the DirectX stream */
    void halt_playback_buffer();

private:
    /* -------------------------------------------------------------------- */
    /*
     *   private internal operations 
     */
    
    /* begin DirectX playback */
    void directx_begin_play();

    /* flush the staging buffer to DirtcX */
    void flush_to_directx(DWORD valid_len);

    /* wait for our last buffer to finish playing back */
    void wait_for_last_buf(DWORD padding);

    /* playback thread main routine - static entrypoint */
    static void play_thread_main(void *ctx)
    {
        /* call the member function version */
        ((CTadsCompressedAudio *)ctx)->do_play_thread();
    }

    /* main playback thread entrypoint */
    void do_play_thread();

protected:
    /* -------------------------------------------------------------------- */
    /*
     *   member variables
     */

    /* the file we're to play back, and its size in bytes */
    HANDLE in_file_;
    DWORD in_file_size_;

    /* our DirectSound object */
    IDirectSound *ds_;

    /* our application-wide audio controller */
    class CTadsAudioControl *audio_control_;

    /* our DirectSound buffer object */
    IDirectSoundBuffer *dsbuf_;

    /* wave format descriptor */
    WAVEFORMATEX wf_;

    /* 
     *   'silence' value - for 8-bit samples, this has the value 128, and for
     *   other sizes it's 0 
     */
    int silence_;

    /* 
     *   Size of each block in our direct sound buffer.  The underlying
     *   buffer consists of three blocks of this size.  
     */
    DWORD data_size_;

    /* offset in direct sound buffer of the chunk currently being written */
    DWORD cur_write_csr_;

    /* offset in the direct sound buffer of next write position */
    DWORD next_write_csr_;

    /*
     *   Number of valid bytes in the three buffer chunks.  We'll use this at
     *   the end of playback to determine how much longer we have to wait for
     *   playback to end. 
     */
    DWORD valid_chunk_len_[3];

    /* current locked DirectSound buffer block(s) */
    void *lock_ptr1_, *lock_ptr2_;
    DWORD lock_len1_, lock_len2_;

    /* the next available byte in the locked buffer block */
    char *stage_buf_cur_;

    /* the end of the locked buffer block */
    char *stage_buf_end_;

    /* flag indicating whether the underlying directx buffer is playing */
    int playing_;

    /* 
     *   flag indicating whether or not we ever started playback - this
     *   allows us to distinguish the case that we reach the end of a short
     *   file without every having started buffered playback from the case
     *   where we cancel playback at some point after starting it 
     */
    int started_playing_;

    /* number of buffers we've filled and flushed to directx */
    int flush_count_;

    /* 
     *   done notification window -- when we finish playback, we'll notify
     *   the given window via the given message and lparam 
     */
    HWND done_hwnd_;
    UINT done_msg_;
    LPARAM done_msg_lpar_;

    /* the thread that's handling playback, if applicable */
    HANDLE thread_hdl_;
    DWORD thread_id_;

    /* the URL of the sound resource (used only for displaying errors) */
    const char *url_;
};

#endif /* TADSCSND_H */
