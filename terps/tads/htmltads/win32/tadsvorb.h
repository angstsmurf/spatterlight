/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsvorb.h - TADS Ogg-Vorbis decoder
Function
  Defines a compressed audio format decoder and playback engine for
  the Ogg Vorbis format.
Notes
  
Modified
  04/26/02 MJRoberts  - creation
*/

#ifndef TADSVORB_H
#define TADSVORB_H

#include "tadscsnd.h"

/*
 *   TADS Ogg Vorbis decoder and playback engine
 */
class CVorbisW32: public CTadsCompressedAudio
{
public:
    /* create */
    CVorbisW32(struct IDirectSound *ds, class CTadsAudioControl *ctl);

    /* decode the file */
    virtual void do_decoding(HANDLE hfile, DWORD file_size);

    /* get/set our 'stop' flag */
    virtual int get_decoder_stopping() { return stop_flag_; }
    void set_decoder_stopping(int f) { stop_flag_ = f; }

private:
    /* decoder stop flag */
    volatile int stop_flag_;

    /* decoding work buffer */
    char pcmbuf[4096];
};

#endif /* TADSVORB_H */
