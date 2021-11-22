/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscsnd.cpp - TADS Compressed Sound base class
Function
  This is a base class for compressed audio datatypes, such as MPEG and
  Ogg Vorbis.  This class provides the basic framework for playing back
  compressed audio by streaming it from disk, through the decoder, and
  into a DirectX buffer.
Notes
  
Modified
  04/26/02 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include <Windows.h>
#include <dsound.h>

#include "tadshtml.h"
#include "tadssnd.h"
#include "tadscsnd.h"


/* ------------------------------------------------------------------------ */
/*
 *   direct sound constants not defined in all dsound.h headers (arg...)
 */
#ifndef DSBCAPS_CTRLDEFAULT
#define DSBCAPS_CTRLDEFAULT \
    ((DSBCAPS_CTRLVOLUME) | (DSBCAPS_CTRLPAN) | (DSBCAPS_CTRLFREQUENCY))
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Implementation 
 */


/*
 *   create 
 */
CTadsCompressedAudio::CTadsCompressedAudio(IDirectSound *ds,
                                           class CTadsAudioControl *ctl)
{
    /* remember and reference our IDirectSound object */
    ds_ = ds;
    ds_->AddRef();

    /* no IDirectSound or IDirectSoundBuffer yet */
    dsbuf_ = 0;

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();

    /* no notification window yet */
    done_hwnd_ = 0;
    done_msg_ = 0;
    done_msg_lpar_ = 0;

    /* no playback thread yet */
    thread_hdl_ = 0;
    thread_id_ = 0;

    /* no URL yet */
    url_ = 0;
}

/*
 *   destroy 
 */
CTadsCompressedAudio::~CTadsCompressedAudio()
{
    /* wait for the playback thread to terminate */
    wait_until_done();
    
    /* make sure we've closed our buffer */
    close_playback_buffer();

    /* release our audio controller interface */
    audio_control_->audioctl_release();
}

/* 
 *   open an audio buffer 
 */
int CTadsCompressedAudio::open_playback_buffer(int freq, int bits_per_sample,
                                               int num_channels)
{
    DSBUFFERDESC bufdesc;

    /* 
     *   Determine the buffer size based on the sampling frequency and number
     *   of channels.  This is the "chunk size" - we create a buffer for
     *   three chunks, and cycle through them as we fill up the buffer so
     *   that playback is always trailing decoding by a comfortable margin.
     *   
     *   Use a chunk size of half a second; this seems experimentally to be a
     *   good compromise between latency (which we want short - the smaller
     *   the chunk size the better for this, because it means we have less to
     *   decode before we start playing) and risk of underruns (which we want
     *   to avoid - the larger the chunk size the better for this, because it
     *   gives the decoder a bigger lead time ahead of the playback cursor).
     *   
     *   'freq' gives us the number of samples per second; we have one sample
     *   per channel, and 'word-size' bytes per sample, where 'word-size' is
     *   the number of bits per sample over 8.  
     */
    data_size_ = (num_channels * freq * (bits_per_sample/8)) / 2;

    /* we haven't sent anything to the sound card yet */
    flush_count_ = 0;

    /* 
     *   Remember the value to write into a buffer for silence.  For 8-bit
     *   samples, the silence level is 128; for other sample sizes, it's
     *   zero. 
     */
    silence_ = (bits_per_sample == 8 ? 128 : 0);

    /* set up our WAVEFORMATEX */
    wf_.wBitsPerSample  = (WORD)bits_per_sample;
    wf_.wFormatTag = WAVE_FORMAT_PCM;
    wf_.nChannels = (WORD)num_channels;
    wf_.nSamplesPerSec = (DWORD)freq;
    wf_.nAvgBytesPerSec = (DWORD)num_channels * freq;
    wf_.nBlockAlign = (WORD)num_channels;
    wf_.cbSize = 0;

    /* 
     *   adjust the bytes per second oand block alignment according to the
     *   size of each sample in bytes 
     */
    if (bits_per_sample > 8)
    {
        /* scale the byte sizes according to the bytes per sample */
        wf_.nAvgBytesPerSec *= (bits_per_sample / 8);
        wf_.nBlockAlign *= (bits_per_sample / 8);
    }

    /* set up the direct sound buffer descriptor */
    memset(&bufdesc, 0, sizeof(bufdesc));
    bufdesc.dwSize = sizeof(bufdesc);
    bufdesc.dwFlags = DSBCAPS_CTRLDEFAULT
                      | DSBCAPS_GETCURRENTPOSITION2
                      | DSBCAPS_GLOBALFOCUS;
    bufdesc.dwBufferBytes = data_size_ * 3;
    bufdesc.dwReserved = 0;
    bufdesc.lpwfxFormat = &wf_;

    /* create our output buffer */
    if (ds_->CreateSoundBuffer(&bufdesc, &dsbuf_, 0) != DS_OK)
        return 2;

    /* we're now playing, so register with the audio controller */
    audio_control_->register_active_sound(this);

    /* 
     *   set the buffer's initial volume according to the application-wide
     *   muting setting 
     */
    dsbuf_->SetVolume(audio_control_->get_mute_sound()
                      ? DSBVOLUME_MIN : DSBVOLUME_MAX);

    /* set up to play from the beginning */
    dsbuf_->SetCurrentPosition(0);

    /* lock the initial buffer */
    dsbuf_->Lock(0, data_size_, &lock_ptr1_, &lock_len1_,
                 &lock_ptr2_, &lock_len2_, 0);
    stage_buf_cur_ = (char *)lock_ptr1_;
    stage_buf_end_ = stage_buf_cur_ + lock_len1_;

    /* the first write cursor is at the start of the buffer */
    cur_write_csr_ = 0;

    /* move the write cursor past the first buffer */
    next_write_csr_ = data_size_;

    /* the direct sound buffer isn't playing yet */
    playing_ = FALSE;
    started_playing_ = FALSE;

    /* success */
    return 0;
}

/*
 *   Change the muting status 
 */
void CTadsCompressedAudio::on_mute_change(int mute)
{
    /* if we have a buffer, set its volume according to the muting status */
    if (dsbuf_ != 0)
        dsbuf_->SetVolume(mute ? DSBVOLUME_MIN : DSBVOLUME_MAX);
}

/*
 *   begin playback if we haven't already done so 
 */
void CTadsCompressedAudio::directx_begin_play()
{
    if (!playing_)
    {
        /* start playing at the beginning of our buffer */
        dsbuf_->SetCurrentPosition(0);
        dsbuf_->Play(0, 0, DSBPLAY_LOOPING);

        /* note that we're playing, and that we ever started playback */
        playing_ = TRUE;
        started_playing_ = TRUE;
    }
}

/*
 *   Flush the staging buffer to directx 
 */
void CTadsCompressedAudio::flush_to_directx(DWORD valid_len)
{
    HRESULT stat;

    /* unlock the current buffer */
    dsbuf_->Unlock(lock_ptr1_, lock_len1_, lock_ptr2_, lock_len2_);

    /* 
     *   if we're playing, wait for the playback pointer to move out of the
     *   buffer region we're going to attempt to write into next 
     */
    if (playing_)
    {
        /* wait until playback moves out of our next write area */
        for (;;)
        {
            DWORD playpos;
            DWORD writepos;

            /* get the current playback position */
            dsbuf_->GetCurrentPosition(&playpos, &writepos);

            /* if it's in the region we want to write, wait a bit */
            if (playpos >= next_write_csr_
                && playpos < next_write_csr_ + data_size_)
            {
                /* wait a bit, then check again */
                Sleep(50);
            }
            else
            {
                /* it's not in our playback region; we can proceed */
                break;
            }
        }
    }

    /*
     *   Note the amount of valid data in the chunk we just filled.  The
     *   buffer is divided into three chunks of size data_size_, so we must
     *   merely divide the last write offset by data_size_ to get the chunk
     *   index. 
     */
    valid_chunk_len_[cur_write_csr_ / data_size_] = valid_len;

    /*
     *   Attempt to lock one buffer-full at the current write position.
     *   This should always be the next position after our last write.  
     */
    for (;;)
    {
        /* try locking */
        stat = dsbuf_->Lock(next_write_csr_, data_size_,
                            &lock_ptr1_, &lock_len1_,
                            &lock_ptr2_, &lock_len2_, 0);

        /* check status */
        if (stat == DSERR_BUFFERLOST)
        {
            /* try restoring, and loop back for another try if successful */
            stat = dsbuf_->Restore();

            /* check the status */
            if (stat != DS_OK && stat != DSERR_BUFFERLOST)
                continue;

            /* failed to restore - give up and drop this buffer */
            return;
        }

        /* if we didn't get the buffer, give up and drop this buffer */
        if (stat != DS_OK)
            return;

        /* got it */
        break;
    }

    /* note the start of the buffer we're now starting to write */
    cur_write_csr_ = next_write_csr_;

    /* 
     *   increment our next write cursor, wrapping if we reach the end of the
     *   direct sound buffer 
     */
    next_write_csr_ += data_size_;
    if (next_write_csr_ >= 3 * data_size_)
        next_write_csr_ = 0;

    /*
     *   Start playback after sending the first two buffers to directx.
     *   Wait until we've finished two buffers to help ensure that we keep
     *   a backlog in the pipe, so that the play cursor doesn't move into
     *   the part of the buffer we're working on filling.  
     */
    if (++flush_count_ == 2)
        directx_begin_play();

    /* 
     *   we've flushed out the staging buffer, so we can start filling it
     *   up again from the beginning 
     */
    stage_buf_cur_ = (char *)lock_ptr1_;
    stage_buf_end_ = stage_buf_cur_ + lock_len1_;
}

/*
 *   Wait until we've played back the last buffer written.
 *   
 *   'padding' indicates the amount of silence written after the last valid
 *   data from the last buffer.  
 */
void CTadsCompressedAudio::wait_for_last_buf(DWORD padding)
{
    /* 
     *   if the underlying DirectSound buffer isn't playing, there's
     *   nothing to wait for 
     */
    if (!playing_)
        return;

    /* 
     *   write out a buffer of silence, to ensure that we have a little
     *   time to catch it when the playback cursor moves out of our last
     *   buffer -- since we probably won't catch it at the exact instant
     *   it leaves, this will at least make it quiet until we manage to
     *   stop playback 
     */
    if (lock_ptr1_ != 0)
    {
        /* write a buffer of silence */
        memset(lock_ptr1_, silence_, stage_buf_end_ - (char *)lock_ptr1_);
        
        /* flush it - this is all silence, so none of it is real data */
        flush_to_directx(0);
    }

    /* if anything is locked, unlock it */
    if (lock_ptr1_ != 0)
    {
        dsbuf_->Unlock(lock_ptr1_, lock_len1_, lock_ptr2_, lock_len2_);
        lock_ptr1_ = lock_ptr2_ = 0;
    }

    /*
     *   keep waiting until the playback pointer moves into the next
     *   buffer after our last write buffer 
     */
    for (;;)
    {
        DWORD playpos;
        DWORD playpos_chunk_ofs;
        DWORD writepos;
        DWORD cur_valid;
        DWORD rem;
        int chunk_idx;
        int i;
        
        /* get the current playback position */
        dsbuf_->GetCurrentPosition(&playpos, &writepos);

        /* 
         *   the buffer is divided into three chunks of size data_size_;
         *   figure out how many valid bytes are left in the chunk we're
         *   currently in 
         */
        chunk_idx = playpos / data_size_;
        cur_valid = valid_chunk_len_[chunk_idx];

        /* get the offset of the playback cursor in the current chunk */
        playpos_chunk_ofs = playpos - chunk_idx * data_size_;

        /*
         *   If the play position has moved past the end of the valid data in
         *   this chunk, then we're done - we've moved into the silent
         *   padding after the last valid data, so we've already played back
         *   all of the actual sound data, thus we're done.
         *   
         *   Note that if this is not the last chunk, it will be completely
         *   full, so it's not possible for the play pointer to have moved
         *   past the end of the chunk and still look like it's in this chunk
         *   - once it's past the end of the chunk, it'll be in the next
         *   chunk, so we wouldn't have thought it was in this chunk in the
         *   first place.  
         */
        if (playpos_chunk_ofs > cur_valid)
            break;

        /* 
         *   We have more valid data in the current chunk.  Calculate how
         *   much we have left in this chunk.  Then, add in the amount
         *   remaining in subsequent chunks until we find the first fully
         *   silent chunk - we always write at least one silent chunk as
         *   padding after the last valid chunk, so we merely need to look
         *   for the first silent chunk.  
         */
        rem = cur_valid - playpos_chunk_ofs;
        for (i = chunk_idx + 1 ; ; ++i)
        {
            /* the buffer is three chunks long - wrap at the end */
            if (i > 2)
                i = 0;

            /* if we've looped, something's wrong - bail */
            if (i == chunk_idx)
                break;

            /* 
             *   if this one's empty, it's the first padding chunk, so we're
             *   done - the previous chunk had the last actual sound data
             */
            if (valid_chunk_len_[i] == 0)
                break;

            /* add this chunk's length to the remaining length */
            rem += valid_chunk_len_[i];
        }

        /*
         *   We know how much play time we have left.  Sleep for this amount
         *   of time.  Note that we know the time in bytes; to calculate the
         *   time in seconds, divide the bytes by the number of channels, the
         *   bytes per sample, and the samples per second.  Multiply by 1000
         *   to get milliseconds, of course.  
         */
        Sleep(rem * 1000
              / (wf_.wBitsPerSample/8) / wf_.nChannels / wf_.nSamplesPerSec);

        /* that should be enough */
        break;
    }
    
    /* stop playback */
    dsbuf_->Stop();
    playing_ = FALSE;
}

/*
 *   close the audio buffer 
 */
void CTadsCompressedAudio::close_playback_buffer()
{
    /* if we never initialized, there's nothing to do */
    if (dsbuf_ == 0)
        return;

    /* check to see if we cancelled playback prematurely */
    if (get_decoder_stopping())
    {
        /* make sure playback is terminated */
        if (playing_)
        {
            dsbuf_->Stop();
            playing_ = FALSE;
        }
    }
    else
    {
        DWORD padding;

        /* presume there's no padding needed in the last buffer */
        padding = 0;

        /* if we have a partially-filled buffer, write it out */
        if (stage_buf_cur_ != (char *)lock_ptr1_)
        {
            DWORD valid_len;

            /* 
             *   figure out how much valid data we have in the buffer, and
             *   how much extra blank padding we need to add to fill out the
             *   remainder of the staging buffer 
             */
            valid_len = stage_buf_cur_ - (char *)lock_ptr1_;
            padding = stage_buf_end_ - stage_buf_cur_;
            
            /* fill in the balance of the buffer with silence */
            memset(stage_buf_cur_, silence_, padding);
            stage_buf_cur_ = stage_buf_end_;
            
            /* write it out */
            flush_to_directx(valid_len);
        }

        /* 
         *   if we never started playing at all, it means that we never
         *   pre-filled enough buffer space to start the player - in this
         *   case, keep flushing empty buffers until we do start playback 
         */
        while (!started_playing_)
        {
            /* fill in the whole stage buffer with silence */
            memset(stage_buf_cur_, silence_, stage_buf_end_ - stage_buf_cur_);
            stage_buf_cur_ = stage_buf_end_;

            /* write it out - it's all silence, so there's no actual data */
            flush_to_directx(0);
        }

        /* 
         *   Wait for playback to finish, noting the amount of silence we had
         *   to pad into the last buffer. 
         */
        wait_for_last_buf(padding);
    }

    /* release the DirectSound objects */
    if (dsbuf_ != 0)
        dsbuf_->Release();
    if (ds_ != 0)
        ds_->Release();

    /* remember that we've forgotten these */
    ds_ = 0;
    dsbuf_ = 0;

    /* we're no longer playing, so unregister with the controller */
    audio_control_->unregister_active_sound(this);
}

/*
 *   Write to the audio buffer.  This puts data into the buffer, blocking
 *   until enough space is available to store the data.  The decoder should
 *   call this frequently to ensure that we keep the buffer prepared with
 *   data to play back while we're decoding.  
 */
void CTadsCompressedAudio::write_playback_buffer(char *buf, int bytes)
{
    /* if the decoder is stopping, don't bother */
    if (get_decoder_stopping())
        return;

    /* keep going until we run out of bytes to copy */
    while (bytes != 0)
    {
        DWORD copylen;
        DWORD max_copylen;
        
        /* copy as much as we can into the staging buffer */
        max_copylen = (DWORD)(stage_buf_end_ - stage_buf_cur_);
        copylen = (DWORD)bytes;
        if (copylen > max_copylen)
            copylen = max_copylen;

        /* copy these bytes */
        CopyMemory(stage_buf_cur_, buf, copylen);

        /* advance the pointers */
        stage_buf_cur_ += copylen;
        buf += copylen;

        /* deduct the amount copied from the remaining balance */
        bytes -= (int)copylen;

        /* we've filled up our staging buffer - flush it to directx */
        if (stage_buf_cur_ == stage_buf_end_)
            flush_to_directx(stage_buf_cur_ - (char *)lock_ptr1_);
    }
}


/*
 *   Halt playback 
 */
void CTadsCompressedAudio::halt_playback_buffer()
{
    /* if we're playing, stop directx */
    if (playing_)
    {
        dsbuf_->Stop();
        playing_ = FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   HTML TADS Interface 
 */

/*
 *   Play our file asynchronously
 */
int CTadsCompressedAudio::play(const textchar_t *url,
                               HANDLE hfile, DWORD file_size,
                               HWND hwnd, UINT done_msg,
                               LPARAM done_msg_lpar)
{
    /* remember the URL */
    url_ = url;
    
    /* remember the file handle and size */
    in_file_ = hfile;
    in_file_size_ = file_size;

    /* remember the notification information */
    done_hwnd_ = hwnd;
    done_msg_ = done_msg;
    done_msg_lpar_ = done_msg_lpar;

    /* not stopping yet */
    set_decoder_stopping(FALSE);

    /* create the thread that will do the actual work */
    thread_hdl_ = CreateThread(0, 0,
                               (LPTHREAD_START_ROUTINE)play_thread_main,
                               (void *)this, 0, &thread_id_);
    if (thread_hdl_ == 0)
        return 1;

    /* success */
    return 0;
}

/*
 *   Wait until asynchronous playback is finished 
 */
void CTadsCompressedAudio::wait_until_done()
{
    /* if we have a background thread, wait for it */
    if (thread_hdl_ != 0)
    {
        /* wait for the playback thread to exit */
        WaitForSingleObject(thread_hdl_, INFINITE);
        
        /* we're done with the thread handle now */
        CloseHandle(thread_hdl_);
        thread_hdl_ = 0;
    }
}

/*
 *   Determine if playback is still underway.  Returns true if playback is
 *   active, false if playback has finished.  
 */
int CTadsCompressedAudio::is_playing()
{
    /* if we have no background thread, playback is done */
    if (thread_hdl_ == 0)
        return FALSE;

    /* wait for the thread, timing out immediately */
    switch(WaitForSingleObject(thread_hdl_, 0))
    {
    case WAIT_TIMEOUT:
        /* we're still playing */
        return TRUE;

    case WAIT_OBJECT_0:
        /* 
         *   it's finished - close the thread handle and indicate that
         *   playback has ended 
         */
        CloseHandle(thread_hdl_);
        thread_hdl_ = 0;
        return FALSE;

    default:
        /* anything else is an error; indicate that playback has finished */
        return FALSE;
    }
}


/*
 *   Playback thread main entrypoint
 */
void CTadsCompressedAudio::do_play_thread()
{
    /* decode the file */
    do_decoding(in_file_, in_file_size_);

    /* if playback was cancelled, stop directx */
    if (get_decoder_stopping() && playing_)
    {
        if (dsbuf_ != 0)
            dsbuf_->Stop();
        playing_ = FALSE;
    }

    /* close the audio device */
    close_playback_buffer();

    /* 
     *   If we have a notification window, send it the message to let it
     *   know we're done.  Note that we never play back a sound more than
     *   once, so we'll always provide 1 for our repeat count.  
     */
    if (done_hwnd_ != 0)
        PostMessage(done_hwnd_, done_msg_, 1, done_msg_lpar_);
}

/*
 *   Stop playback 
 */
void CTadsCompressedAudio::stop(int sync)
{
    /* set the flag to indicate that we shouldn't do any more decoding */
    set_decoder_stopping(TRUE);

    /* if this is synchronous, wait until the playback thread exits */
    if (sync && thread_hdl_ != 0)
    {
        /* 
         *   wait for the playback thread to exit; give up if it doesn't
         *   exit after a few seconds 
         */
        WaitForSingleObject(thread_hdl_, 5000);

        /* we're done with the thread handle now */
        CloseHandle(thread_hdl_);
        thread_hdl_ = 0;
    }
}

