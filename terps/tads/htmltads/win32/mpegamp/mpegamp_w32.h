/* $Header: d:/cvsroot/tads/html/win32/mpegamp/mpegamp_w32.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  audio_w32.h - win95 audio driver for AMP 0.7.6 MPEG player
Function
  
Notes
  Derived from amp 0.7.6 for use in HTML TADS.
Modified
  10/25/98 MJRoberts  - Creation
*/

#ifndef MPEGAMP_W32_H
#define MPEGAMP_W32_H


#ifndef MPEGAMP_H
#include "mpegamp.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/*
 *   MPEG audio decompressor for Win32 
 */
class CMpegAmpW32: public CMpegAmp, public CTadsCompressedAudio
{
    friend class CMpegAmp;
    
public:
    CMpegAmpW32(struct IDirectSound *ds, class CTadsAudioControl *ctl);
    ~CMpegAmpW32() { }

    /* 
     *   TESTING ONLY - Play a file.  This operates synchronously; it doesn't
     *   return until playback finishes.  If 'stop_on_key' is true, we'll
     *   cancel playback if the user presses a key during playback.  
     */
    void play(const char *inFileStr, int stop_on_key);

protected:
    /* decode a file */
    void do_decoding(HANDLE hfile, DWORD file_size)
    {
        /* remember the file in the decoder's member variables */
        in_file = hfile;
        file_bytes_avail = file_size;
        
        /* we're set up now, so decode the file */
        decodeMPEG();
    }

    /* 
     *   get/set the decoder 'stop' flag - we use the mpeg decoder's internal
     *   stop flag 
     */
    virtual int get_decoder_stopping() { return stop_flag; }
    virtual void set_decoder_stopping(int f) { stop_flag = f; }

private:
    /* set up the decoder */
    int setup_audio(struct AUDIO_HEADER *header);

    /* decode MPEG audio */
    int decodeMPEG(void);
};

#endif /* AUDIO_W95_H */

