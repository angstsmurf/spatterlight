/* $Header: d:/cvsroot/tads/html/win32/tadswav.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadswav.h - TADS WAV (waveform audio) support
Function
  
Notes
  
Modified
  01/19/98 MJRoberts  - Creation
*/

#ifndef TADSWAV_H
#define TADSWAV_H

#include <windows.h>
#include <dsound.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/*
 *   Wave file decoder and playback engine
 */
class CWavW32: public CTadsCompressedAudio
{
public:
    CWavW32(struct IDirectSound *ds, class CTadsAudioControl *ctl);

    /* decode the file */
    virtual void do_decoding(HANDLE hfile, DWORD file_size);

    /* get/set our 'stop' flag */
    virtual int get_decoder_stopping() { return stop_flag_; }
    void set_decoder_stopping(int f) { stop_flag_ = f; }

protected:
    /*
     *   Read a WAV file header.  This parses the file header and sets up
     *   the WAVEFORMATEX structure.  Returns zero on success, non-zero on
     *   failure.  
     */
    int read_header(HANDLE hfile);

    /* 
     *   get a pointer to the WAVEFORMATEX structure (the contents of this
     *   structure aren't valid until the header has been read in) 
     */
    WAVEFORMATEX *get_wavefmtex() const { return wavefmt_; }

    /* seek to the start of the wave data stream */
    void seek_data_start() { data_read_ofs_ = 0; }

    /*
     *   Read from the wave data stream.  Returns zero on success, nonzero
     *   on error.  *bytes_read returns with the actual number of bytes
     *   that we read; if we reach the end of the file before the full
     *   request is satisfied, we'll return success with *bytes_read
     *   indicating that the buffer is only partially filled. 
     */
    int read_data(HANDLE hfile, char *buf, unsigned long bytes_to_read,
                  unsigned long *bytes_read, int repeat, int *repeats_done);

    /* get the total length of the wave data byte stream */
    unsigned long get_wave_len() const { return data_len_; }

private:
    /* decoder stop flag */
    volatile int stop_flag_;

    /* decoding work buffer */
    char pcmbuf_[4096];

    /* format data */
    WAVEFORMATEX *wavefmt_;

    /* 
     *   seek position in file of start of byte stream containing the
     *   actual digitized sound data 
     */
    unsigned long data_fpos_;

    /* flags: we've found the header and data chunks in the file */
    int found_header_ : 1;
    int found_data_ : 1;

    /*
     *   Data read offset - this is the byte offset in the data stream
     *   from which the next data read routine will start reading. 
     */
    unsigned long data_read_ofs_;

    /* size of the data chunk */
    unsigned long data_len_;
};

#endif /* TADSWAV_H */
