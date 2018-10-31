/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsvorb.cpp - TADS Ogg-Vorbis decoder
Function
  Defines a compressed audio format decoder and playback engine for the
  Ogg Vorbis format.
Notes

Modified
  04/26/02 MJRoberts  - creation
*/

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <io.h>

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

#include "tadscsnd.h"
#include "tadsvorb.h"

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER -1
#endif

/* ------------------------------------------------------------------------ */
/*
 *   construction 
 */
CVorbisW32::CVorbisW32(struct IDirectSound *ds,
                       class CTadsAudioControl *ctl)
    : CTadsCompressedAudio(ds, ctl)
{
    /* not stopping yet */
    stop_flag_ = FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Callbacks for data source operations.  We want to use a Windows native
 *   file handle rather than a stdio FILE object, so we implement our own
 *   set of data source callbacks.  
 */

/* read from the file */
static size_t cb_read(void *buf, size_t siz, size_t cnt, void *fp)
{
    HANDLE hfile = (HANDLE)fp;
    DWORD actual;

    /* read the data */
    if (ReadFile(hfile, buf, siz*cnt, &actual, 0))
    {
        /* success - return the number of bytes actually read */
        return actual;
    }
    else
    {
        /* failure - return zero */
        return 0;
    }
}

/* seek */
static int cb_seek(void *fp, ogg_int64_t offset, int whence)
{
    HANDLE hfile = (HANDLE)fp;
    LONG pos_lo = (LONG)offset;
    LONG pos_hi = (LONG)(offset >> 32);
    DWORD mode;
    LONG result;

    /* translate the stdio-style mode to the Windows mode */
    switch(whence)
    {
    case SEEK_SET:
        /* relative to start of file */
        mode = FILE_BEGIN;
        break;

    case SEEK_CUR:
        /* relative to current position */
        mode = FILE_CURRENT;
        break;
        
    case SEEK_END:
        /* relative to end of file */
        mode = FILE_END;
        break;

    default:
        /* failure */
        return -1;
    }

    /* set the file position */
    result = SetFilePointer(hfile, pos_lo, pos_hi == 0 ? 0 : &pos_hi, mode);

    /* if there's no high part, decoding the result is easy */
    if (pos_hi == 0)
        return (result == INVALID_SET_FILE_POINTER ? -1 : 0);

    /* if there's a high part, we have to check GetLastError */
    return (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR
            ? -1 : 0);
}

/* close */
static int cb_close(void *fp)
{
    /* 
     *   the file handle is managed externally to the decoder, so we don't
     *   need to do anything here 
     */
    return 0;
}

/* get current position */
static long cb_tell(void *fp)
{
    HANDLE hfile = (HANDLE)fp;

    /* get the current file position by seeking to the current position */
    return SetFilePointer(hfile, 0, 0, FILE_CURRENT);
}


/* ------------------------------------------------------------------------ */
/*
 *   Decode a file 
 */
void CVorbisW32::do_decoding(HANDLE hfile, DWORD file_size)
{
    OggVorbis_File vf;
    vorbis_info *vi;
    int eof = FALSE;
    ov_callbacks cb =
    {
        &cb_read,
        &cb_seek,
        &cb_close,
        &cb_tell
    };
    int active_sect;

    /* final layer: open the vorbisfile descriptor on the stdio FILE */
    if (ov_open_callbacks(hfile, &vf, 0, 0, cb) < 0)
    {
        // $$$"Input does not appear to be an Ogg bitstream."
        return;
    }

    /* get information on the current logical bitstream */
    vi = ov_info(&vf, -1);

    /* open our playback buffer */
    open_playback_buffer(vi->rate, 16, vi->channels);

    /* start with section 0 */
    // $$$ this might not be right
    active_sect = 0;

    /* decode until done */
    for (eof = FALSE ; !eof && !stop_flag_ ; )
    {
        long len;
        int cur_sect;

        /* 
         *   read the next chunk - read 16-bit signed samples in
         *   little-endian mode 
         */
        len = ov_read(&vf, pcmbuf, sizeof(pcmbuf), 0, 2, 1, &cur_sect);

        /* 
         *   if we changed to a new section, we might have to change to a
         *   new sampling rate 
         */
        if (cur_sect != active_sect)
        {
            vorbis_info *new_vi;
            
            /* get the new section's information */
            new_vi = ov_info(&vf, cur_sect);

            /* 
             *   if the sampling rate or channel count has changed, close
             *   and re-open the audio buffer with the new specifications 
             */
            if (new_vi->rate != vi->rate
                || new_vi->channels != vi->channels)
            {
                /* close the playback buffer */
                close_playback_buffer();

                /* open it up again with the new specs */
                open_playback_buffer(new_vi->rate, 16, new_vi->channels);
            }

            /* remember this as the new active section */
            active_sect = cur_sect;
            vi = new_vi;
        }

        /* if we didn't get any data, we've reached the end of the file */
        if (len == 0)
        {
            /* flag that we're at EOF */
            eof = TRUE;
        }
        else if (len < 0)
        {
            /* error in the stream */
            // $$$ might want to report it
        }
        else
        {
            /* write the data to our playback buffer */
            write_playback_buffer(pcmbuf, len);
        }
    }

    /* cleanup */
    ov_clear(&vf);
}

