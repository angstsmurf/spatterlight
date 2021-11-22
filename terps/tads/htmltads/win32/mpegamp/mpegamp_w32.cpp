#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/mpegamp/mpegamp_w32.cpp,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $";
#endif

/* Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  mpegamp_w32.c - amp 0.7.6 audio driver for win95/98/nt
Function
  
Notes
  Derived from amp 0.7.6 for use in HTML TADS on Windows 95/98/NT.
Modified
  10/24/98 MJRoberts  - Creation
*/


#include <stdio.h>
#include <string.h>

#include <Windows.h>
#include <dsound.h>

#include "tadshtml.h"
#include "mpegamp_w32.h"

/* ------------------------------------------------------------------------ */
/*
 *   Statics 
 */
const int CMpegAmp::t_b8_l[2][3][22] =
{
    {
        {5,11,17,23,29,35,43,53,65,79,95,115,139,167,199,237,283,335,395,463,521,575},
        {5,11,17,23,29,35,43,53,65,79,95,113,135,161,193,231,277,331,393,463,539,575},
        {5,11,17,23,29,35,43,53,65,79,95,115,139,167,199,237,283,335,395,463,521,575}
    },
    {
        {3,7,11,15,19,23,29,35,43,51,61,73,89,109,133,161,195,237,287,341,417,575},
        {3,7,11,15,19,23,29,35,41,49,59,71,87,105,127,155,189,229,275,329,383,575},
        {3,7,11,15,19,23,29,35,43,53,65,81,101,125,155,193,239,295,363,447,549,575}
    }
};

const int CMpegAmp::t_b8_s[2][3][13] =
{
    {
        {3,7,11,17,23,31,41,55,73,99,131,173,191},
        {3,7,11,17,25,35,47,61,79,103,135,179,191},
        {3,7,11,17,25,35,47,61,79,103,133,173,191}
    },
    {
        {3,7,11,15,21,29,39,51,65,83,105,135,191},
        {3,7,11,15,21,27,37,49,63,79,99,125,191},
        {3,7,11,15,21,29,41,57,77,103,137,179,191}
    }
};

const short CMpegAmp::t_bitrate[2][3][15] =
{
    {
        {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256},
        {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
        {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160}
    },
    {
        {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
        {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
        {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}
    }
};

const int CMpegAmp::t_sampling_frequency[2][3] =
{
    { 22050 , 24000 , 16000},
    { 44100 , 48000 , 32000}
};

/* ------------------------------------------------------------------------ */
/*
 *   Implementation 
 */

CMpegAmpW32::CMpegAmpW32(IDirectSound *ds, class CTadsAudioControl *ctl)
    : CMpegAmp(), CTadsCompressedAudio(ds, ctl)
{
    /* clear my CMpegAmp object's memory */
    memset((CMpegAmp *)this, 0, sizeof(CMpegAmp));

    /* initialize the decoder */
    initialise_decoder();
}

/*
 *   write data to the output stream 
 */
void CMpegAmp::printout(void)
{
    int j;

    if (nch == 2)
        j=32 * 18 * 2;
    else
        j=32 * 18;

    /* write the data to our output stream */
    ((CMpegAmpW32 *)this)->
        write_playback_buffer((char*)sample_buffer, j * sizeof(short));
}

void CMpegAmp::stop_playback()
{
    ((CMpegAmpW32 *)this)->halt_playback_buffer();
}

/* ------------------------------------------------------------------------ */
/*
 *   The remainder of this file is adapted from the original audio.c, the
 *   main entrypoint for the unix version 
 */

/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* audio.c      main amp source file 
 *
 * Created by: tomislav uzelac  Apr 1996 
 * Karl Anders Oygard added the IRIX code, 10 Mar 1997.
 * Ilkka Karvinen fixed /dev/dsp initialization, 11 Mar 1997.
 * Lutz Vieweg added the HP/UX code, 14 Mar 1997.
 * Dan Nelson added FreeBSD modifications, 23 Mar 1997.
 * Andrew Richards complete reorganisation, new features, 25 Mar 1997
 * Edouard Lafargue added sajber jukebox support, 12 May 1997
 */ 

int CMpegAmpW32::decodeMPEG(void)
{
    struct AUDIO_HEADER header;
    int cnt,g,snd_eof;

    initialise_globals();

    cnt=0;

    if ((g=gethdr(&header))!=0)
    {
        report_header_error(g);
        return -1;
    }
    
    if (header.protection_bit==0)
        getcrc();

    if (setup_audio(&header)!=0)
    {
        warn("Cannot set up audio. Exiting\n");
        return -1;
    }
        
    if (header.layer==1)
    {
        if (layer3_frame(&header,cnt)) {
            warn(" error. blip.\n");
            return -1;
        }
    }
    else if (header.layer==2)
    {
        if (layer2_frame(&header,cnt))
        {
            warn(" error. blip.\n");
            return -1;
        }
    }

    /*
     *   decoder loop ********************************** 
     */
    snd_eof=0;
    while (!snd_eof && !stop_flag)
    {
        while (!snd_eof)
        {
            if ((g=gethdr(&header))!=0)
            {
                report_header_error(g);
                snd_eof=1;
                break;
            }
            
            if (header.protection_bit==0)
                getcrc();
            
            if (header.layer==1)
            {
                if (layer3_frame(&header,cnt))
                {
                    warn(" error. blip.\n");
                    return -1;
                }
            }
            else if (header.layer==2)
            {
                if (layer2_frame(&header,cnt))
                {
                    warn(" error. blip.\n");
                    return -1;
                }
            }
            cnt++;
        }
    }
    return 0;
}

/* 
 *   call this once at the beginning 
 */
void CMpegAmp::initialise_decoder(void)
{
    premultiply();
    imdct_init();
    calculate_t43();
}

/* 
 *   call this before each file is played 
 */
void CMpegAmp::initialise_globals(void)
{
    append=data=nch=0; 
    f_bdirty=TRUE;
    bclean_bytes=0;
    
    memset(s,0,sizeof s);
    memset(res,0,sizeof res);
}

void CMpegAmp::report_header_error(int err)
{
    switch (err)
    {
    case GETHDR_ERR: warn("error reading mpeg bitstream.\n");
                     break;
    case GETHDR_NS : warn("MPEG 2.5 format is not supported -- this format "
                          "is not defined by the ISO/MPEG standard.");
                     break;
    case GETHDR_FL1: warn("ISO/MPEG layer 1 is not supported.\n");
                     break;
    case GETHDR_FF : warn("free format bitstreams are not supported.\n");
                     break;  
    case GETHDR_SYN: warn("error: out of sync.\n");
                     break;
    case GETHDR_EOF: 
    default:                ;   /* some stupid compilers need the semicolon */
    }       
}

/*
 *   set up the audio buffer 
 */
int CMpegAmpW32::setup_audio(struct AUDIO_HEADER *header)
{
    int stereo;
    int freq;

    /* get the sampling frequency from the header */
    freq = t_sampling_frequency[header->ID][header->sampling_frequency];
    
    /* check the header to determine if we're stereo or mono */
    stereo = (header->mode != 3 && !A_DOWNMIX);

    /* remember the number of channels in our 'nch' member variable */
    nch = (stereo ? 2 : 1);

    /* open the PCM output stream */
    open_playback_buffer(freq, 16, nch);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Show a warning 
 */
void CMpegAmp::warn(const char *msg, ...)
{
    va_list ap;

    if (((CMpegAmpW32 *)this)->url_ != 0)
        oshtml_dbg_printf("%s: ", ((CMpegAmpW32 *)this)->url_);
    
    va_start(ap, msg);
    oshtml_dbg_vprintf(msg, ap);
    va_end(ap);
}

/*
 *   dump the buffer 
 */
void CMpegAmp::dump(int *)
{
}


#ifdef TEST_MPEGAMP
#include <conio.h>
/* ------------------------------------------------------------------------ */
/*
 *   Testing 
 */
int main(int argc,char **argv)
{
    CMpegAmpW32 *player;
    int argPos;
    IDirectSound *ds;
    HWND hwnd;

    /* create the IDirectSound interface */
    if (DirectSoundCreate(0, &ds, 0) != DS_OK)
    {
        printf("unable to create IDirectSound object\n");
        exit(2);
    }

    /* we need a window of some kind, so directx knows when we have focus */
    hwnd = CreateWindow("STATIC", "Hello!",
                        WS_POPUP | WS_VISIBLE,
                        100, 100, 200, 200, 0, 0, 0, 0);

    /* set the cooperative level */
    ds->SetCooperativeLevel(hwnd, DSSCL_NORMAL);

    /* create and initialize a player */
    player = new CMpegAmpW32(ds);
    player->initialise_decoder();

    /* check arguments */
    if (argc > 1)
    {
        /* play each file on the command line */
        for(argPos = 1 ; argPos < argc ; argPos++)
            player->play(argv[argPos], TRUE);
    }
    else
    {
        printf("usage: amp mp-file ...\n");
    }

    /* release our IDirectSound object */
    ds->Release();

    /* delete the player */
    delete player;

    /* delete our window */
    DestroyWindow(hwnd);

    exit(0);
    return 0;
}

/* 
 *   play back synchronously 
 */
void CMpegAmpW32::play(const char *inFileStr, int stop_on_key)
{
    HANDLE hfile;
    DWORD file_size;
    int err;

    /* open the file */
    hfile = CreateFile(inFileStr, GENERIC_READ, FILE_SHARE_READ,
                         0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (in_file == INVALID_HANDLE_VALUE)
    {
        warn("Could not open file: %s\n", inFileStr);
        return;
    }

    /* get the file's size */
    file_size = GetFileSize(hfile, 0);

    /* start the playback thread */
    err = play(hfile, file_size, 0, 0, 0);
    if (err != 0)
    {
        printf("error %d from play()\n", err);
        goto done;
    }

    /* wait according to the caller's specifications */
    if (stop_on_key)
    {
        /* wait until playback is finished or the user presses a key */
        for (;;)
        {
            /* if playback is finished, stop */
            if (!is_playing())
                break;
            
            /* if the user has pressed a key, stop */
            if (_kbhit())
            {
                /* stop playback */
                stop(TRUE);
                
                /* no need to wait any longer */
                break;
            }
        }
    }
    else
    {
        /* wait until the thread terminates */
        wait_until_done();
    }

done:
    /* close the file */
    CloseHandle(in_file);
}

#endif

