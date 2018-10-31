#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32snd.cpp,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32snd.cpp - html tads win32 sound implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif
#ifndef TADSWAV_H
#include "tadswav.h"
#endif
#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef W32SND_H
#include "w32snd.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef MPEGAMP_W32_H
#include "mpegamp_w32.h"
#endif
#ifndef TADSVORB_H
#include "tadsvorb.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   System MIDI object implementation 
 */

CHtmlSysSoundMidi_win32::CHtmlSysSoundMidi_win32(CTadsAudioControl *ctl)
{
    /* create a MIDI player object */
    player_ = new CTadsMidiFilePlayer(ctl);

    /* no file yet */
    fp_ = 0;
}

CHtmlSysSoundMidi_win32::~CHtmlSysSoundMidi_win32()
{
    /* done with the MIDI player */
    delete player_;

    /* close our file */
    if (fp_ != 0)
        osfcls(fp_);
}

CHtmlSysResource *CHtmlSysSoundMidi::
   create_midi(const CHtmlUrl *url, const textchar_t *filename,
               unsigned long seekpos, unsigned long filesize,
               CHtmlSysWin *win)
{
    int err;
    CHtmlMidi *midi;
    CHtmlSysSoundMidi_win32 *snd;

    /* create the generic loader object */
    midi = new CHtmlMidi();
    err = midi->read_midi_file(filename, seekpos, filesize);
    if (err != 0)
    {
        oshtml_dbg_printf("midi: error %d reading file %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create the system MIDI object from the
         *   MIDI loader object 
         */
        snd = new CHtmlSysSoundMidi_win32((CHtmlSysWin_win32 *)win);
        if ((err = snd->load_from_res(midi)) != 0)
        {
            /* failure - delete it */
            delete snd;
            snd = 0;
            oshtml_dbg_printf("midi: error %d preparing %s\n",
                              err, url->get_url());
        }
    }
    
    /* we're done with the loader object - delete it */
    delete midi;

    /* return the sound object */
    return snd;
}

/* play the sound */
int CHtmlSysSoundMidi_win32::play_sound(class CHtmlSysWin *win,
                                        void (*done_func)(void *, int),
                                        void *done_func_ctx, int repeat,
                                        const textchar_t *url)
{
    int err;
    
    /* make sure music is enabled - ignore the request if not */
    if (!((CHtmlSysWin_win32 *)win)->get_prefs()->get_music_on())
        return 1;

    /* 
     *   open the file, if it's not open already (we might have already
     *   opened the file on a previous repetition, in which case we don't
     *   need to re-open it now) 
     */
    if (fp_ == 0)
    {
        /* open the file */
        fp_ = osfoprb(fname_.get(), OSFTBIN);

        /* make sure we succeeded */
        if (fp_ == 0)
        {
            oshtml_dbg_printf("midi player: unable to open file: %s\n", url);
            return 2;
        }
    }

    /* seek to the proper point in the file */
    osfseek(fp_, seek_pos_, OSFSK_SET);

    /* begin playback of the file */
    if ((err = player_->play(((CHtmlSysWin_win32 *)win)->get_handle(),
                             fp_, done_func, done_func_ctx, repeat)) != 0)
        oshtml_dbg_printf("midi player: error %d playing file %s\n",
                          err, url);

    /* return the result from the player */
    return err;
}


/* cancel the sound */
void CHtmlSysSoundMidi_win32::cancel_sound(class CHtmlSysWin *, int sync)
{
    /* tell the player to stop playing */
    player_->stop(sync);
}

/* suspend playback if necessary */
int CHtmlSysSoundMidi_win32::maybe_suspend(CHtmlSysSound *new_sound)
{
    /* see what we have */
    switch(new_sound->get_res_type())
    {
    case HTML_res_type_WAV:
    case HTML_res_type_MP123:
    case HTML_res_type_OGG:
        /* 
         *   we can always play a digitized sound (WAV, MPEG, Ogg Vorbis) and
         *   a MIDI at the same time 
         */
        return FALSE;

    default:
        /* further consideration is required */
        break;
    }

    /*
     *   MIDI is exlusive -- we can only play one MIDI sound at a time.
     *   However, the current HTML TADS model only allows MIDI to play in
     *   one layer (the background layer) anyway, so we should never find
     *   ourselves wanting to play a MIDI sound while another MIDI sound
     *   is already active.  So, we'll just ignore this request entirely;
     *   the result will be that the new foreground sound will be unable
     *   to play.
     *   
     *   If at some point in the future the HTML TADS model changes to
     *   allow multiple layers of MIDI sounds, this part of this routine
     *   will have to have a real implementation.  (The bulk of the code
     *   would be in CTadsMidiFilePlayer, since that's what handles the
     *   details of the Windows MIDI API.)  
     */
    // $$$ ignore for now
    return FALSE;
}

/* resume playback of suspended sound */
void CHtmlSysSoundMidi_win32::resume()
{
    /* 
     *   We never suspend MIDI, so there's nothing to do.  (See the notes
     *   in maybe_suspend() - if the model changes to require that MIDI
     *   suspension be implemented, we would have to provide a real
     *   implementation here.) 
     */
    // $$$ ignore for now
}

/* ------------------------------------------------------------------------ */
/*
 *   Win32 Compressed Audio implementation 
 */

/*
 *   create 
 */
CHtmlSysSoundDigitized_win32::CHtmlSysSoundDigitized_win32()
{
    /* no player yet */
    player_ = 0;

    /* not playing yet */
    playing_ = FALSE;

    /* no file yet */
    hfile_ = INVALID_HANDLE_VALUE;
}

/*
 *   destroy
 */
CHtmlSysSoundDigitized_win32::~CHtmlSysSoundDigitized_win32()
{
    /* delete our player */
    if (player_ != 0)
        delete player_;

    /* close our file, if we have one */
    if (hfile_ != INVALID_HANDLE_VALUE)
        CloseHandle(hfile_);
}

/* play the sound */
int CHtmlSysSoundDigitized_win32::play_sound(class CHtmlSysWin *win0,
                                             void (*done_func)(void *, int),
                                             void *done_func_ctx, int repeat,
                                             const textchar_t *url)
{
    int err;
    CHtmlSysWin_win32 *win = (CHtmlSysWin_win32 *)win0;

    /* make sure sounds are enabled - ignore the request if not */
    if (!win->get_prefs()->get_sounds_on())
        return 1;

    /* if we're already playing, we can't play again */
    if (playing_)
    {
        oshtml_dbg_printf("compressed audio player: "
                          "file %s already playing\n", url);
        return 1;
    }

    /* if we already have a player, delete it and start over */
    if (player_ != 0)
    {
        delete player_;
        player_ = 0;
    }

    /* create the player */
    player_ = create_player(win->get_directsound(), win);

    /* if the file isn't already open, open it */
    if (hfile_ == INVALID_HANDLE_VALUE)
    {
        /* open the file */
        hfile_ = CreateFile(fname_.get(), GENERIC_READ, FILE_SHARE_READ,
                            0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (hfile_ == INVALID_HANDLE_VALUE)
        {
            oshtml_dbg_printf("compressed audio player: "
                              "unable to open file %s\n", url);
            return 1;
        }
    }

    /* remember our callback function */
    done_func_ = done_func;
    done_ctx_ = done_func_ctx;

    /* seek to the proper point in the file */
    SetFilePointer(hfile_, seek_pos_, 0, FILE_BEGIN);

    /* presume we'll play */
    playing_ = TRUE;

    /* begin playback of the file */
    err = player_->play(url, hfile_, data_size_, win->get_handle(),
                        HTMLM_WAV_DONE, (LPARAM)this);

    /* if that didn't work, we're not playing after all */
    if (err != 0)
    {
        oshtml_dbg_printf("compressed audio player: "
                          "error %d playing file %s\n", err, url);
        playing_ = FALSE;
    }

    /* return the result from the player */
    return err;
}

/* cancel the sound */
void CHtmlSysSoundDigitized_win32::cancel_sound(CHtmlSysWin *, int sync)
{
    /* tell the player to stop the sound */
    player_->stop(sync);
}

/* suspend playback if necessary */
int CHtmlSysSoundDigitized_win32::maybe_suspend(CHtmlSysSound *new_sound)
{
    /* see what we have */
    switch(new_sound->get_res_type())
    {
    case HTML_res_type_MIDI:
        /* we can always play a WAV and a MIDI sound at the same time */
        return FALSE;

    default:
        /* further consideration is required */
        break;
    }

    /* 
     *   With DirectX, we can play back digitized sounds along with anything
     *   else, so there's no need to suspend for any other sound.  
     */
    return FALSE;

    /*
     *   If we want to allow non-DirectX playback, we'll only be able to play
     *   one digitized audio type at a time.  If the other sound is a WAV,
     *   MPEG, or OGG file, we'll need to suspend this sound to allow
     *   playback of the other one.  
     */
    // $$$ allow non-directx playback? if so, we'll have to suspend
}

/* resume playback of suspended sound */
void CHtmlSysSoundDigitized_win32::resume()
{
    /*
     *   With DirectX, we never have to suspend a WAV, MPEG, or OGG, so we'll
     *   never have to resume one.  
     */
    // $$$ allow non-directx playback? if so, we'll need to resume here
}

/* 
 *   receive notification that the sound has stopped - this is called by
 *   my window when it recieves the sound-stop notification from the MPEG
 *   object (the MPEG object uses a window message, rather than calling
 *   the callback directly, to avoid any synchronization problem that may
 *   result from the MPEG playback being handled by a separate thread) 
 */
void CHtmlSysSoundDigitized_win32::on_sound_done(int repeat_count)
{
    /* note that we're no longer playing */
    playing_ = FALSE;

    /* if we have a callback, invoke it */
    if (done_func_ != 0)
        (*done_func_)(done_ctx_, repeat_count);
}

/* ------------------------------------------------------------------------ */
/*
 *   WAV audio datatype 
 */

/*
 *   create from the WAV resource 
 */
CHtmlSysResource *CHtmlSysSoundWav::
   create_wav(const CHtmlUrl *url, const textchar_t *filename,
              unsigned long seekpos, unsigned long filesize,
              CHtmlSysWin *win)
{
    int err;
    CHtmlWav *wav;
    CHtmlSysSoundWav_win32 *snd;

    /* create the generic loader and load the file */
    wav = new CHtmlWav();
    err = wav->read_wav_file(filename, seekpos, filesize);
    if (err != 0)
    {
        oshtml_dbg_printf("WAV audio: error %d loading %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create the system WAV player object from
         *   the loader object 
         */
        snd = new CHtmlSysSoundWav_win32();
        if ((err = snd->load_from_res(wav)) != 0)
        {
            /* failure - delete it */
            delete snd;
            snd = 0;
            oshtml_dbg_printf("WAV audio: error %d preparing %s\n",
                              err, url->get_url());
        }
    }

    /* we're done with the loader object - delete it */
    delete wav;

    /* return the sound object */
    return snd;
}

/*
 *   Create the player 
 */
CTadsCompressedAudio *CHtmlSysSoundWav_win32::create_player(
    IDirectSound *ds, class CTadsAudioControl *ctl)
{
    /* return a new CWaveW32 object */
    return new CWavW32(ds, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   MPEG audio datatype 
 */

/*
 *   create from the MPEG resource 
 */
CHtmlSysResource *CHtmlSysSoundMpeg::
   create_mpeg(const CHtmlUrl *url, const textchar_t *filename,
               unsigned long seekpos, unsigned long filesize,
               CHtmlSysWin *win)
{
    int err;
    CHtmlMpeg *mpeg;
    CHtmlSysSoundMpeg_win32 *snd;

    /* create the generic loader and load the file */
    mpeg = new CHtmlMpeg();
    err = mpeg->read_mpeg_file(filename, seekpos, filesize);
    if (err != 0)
    {
        oshtml_dbg_printf("mpeg audio: error %d loading %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create the system MPEG player object from
         *   the loader object 
         */
        snd = new CHtmlSysSoundMpeg_win32();
        if ((err = snd->load_from_res(mpeg)) != 0)
        {
            /* failure - delete it */
            delete snd;
            snd = 0;
            oshtml_dbg_printf("mpeg audio: error %d preparing %s\n",
                              err, url->get_url());
        }
    }

    /* we're done with the loader object - delete it */
    delete mpeg;

    /* return the sound object */
    return snd;
}

/*
 *   Create the player 
 */
CTadsCompressedAudio *CHtmlSysSoundMpeg_win32::create_player(
    IDirectSound *ds, class CTadsAudioControl *ctl)
{
    /* return a new CMpegAmpW32 object */
    return new CMpegAmpW32(ds, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Ogg Vorbis audio datatype 
 */

/*
 *   create from the OGG resource 
 */
CHtmlSysResource *CHtmlSysSoundOgg::
   create_ogg(const CHtmlUrl *url, const textchar_t *filename,
              unsigned long seekpos, unsigned long filesize,
              CHtmlSysWin *win)
{
    int err;
    CHtmlOgg *ogg;
    CHtmlSysSoundOgg_win32 *snd;

    /* create the generic loader and load the file */
    ogg = new CHtmlOgg();
    err = ogg->read_ogg_file(filename, seekpos, filesize);
    if (err != 0)
    {
        oshtml_dbg_printf("Ogg-Vorbis: error %d loading %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create the system Ogg Vorbis player object
         *   from the loader object 
         */
        snd = new CHtmlSysSoundOgg_win32();
        if ((err = snd->load_from_res(ogg)) != 0)
        {
            /* failure - delete it */
            delete snd;
            snd = 0;
            oshtml_dbg_printf("Ogg-Vorbis: error %d preparing %s\n",
                              err, url->get_url());
        }
    }

    /* we're done with the loader object - delete it */
    delete ogg;

    /* return the sound object */
    return snd;
}

/*
 *   Create the player 
 */
CTadsCompressedAudio *CHtmlSysSoundOgg_win32::create_player(
    IDirectSound *ds, class CTadsAudioControl *ctl)
{
    /* return a new CVorbisW32 object */
    return new CVorbisW32(ds, ctl);
}

