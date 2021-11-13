#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/hos_w32.cpp,v 1.3 1999/05/29 15:51:04 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  hos_w32.cpp - HTML OS functions for Win32
Function
  
Notes
  
Modified
  01/24/98 MJRoberts  - Creation
*/

#include <Windows.h>

#include <os.h>
#include <osifcext.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Code page identifier 
 */

/* initialize, given the xxx_CHARSET ID and the code page number */
oshtml_charset_id_t::oshtml_charset_id_t(unsigned int cs, unsigned int cp)
{
    charset = cs;
    codepage = cp;
}

/* initialize to default settings */
oshtml_charset_id_t::oshtml_charset_id_t()
{
    charset = DEFAULT_CHARSET;
    codepage = CP_ACP;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the next character in a string.
 */
textchar_t *os_next_char(oshtml_charset_id_t id, const textchar_t *p)
{
    return CharNextExA((WORD)id.codepage, p, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   get the current system time
 */
os_timer_t os_get_time()
{
    return GetTickCount();
}


/* ------------------------------------------------------------------------ */
/*
 *   Get system information 
 */
int os_get_sysinfo(int code, void * /*param*/, long *result)
{
    CHtmlSys_mainwin *win;
    CHtmlPreferences *prefs;
    
    /* get the preferences object, in case we need it */
    if ((win = CHtmlSys_mainwin::get_main_win()) != 0)
        prefs = win->get_prefs();
    else
        prefs = 0;

    /* find out what kind of information they want */
    switch(code)
    {
    case SYSINFO_INTERP_CLASS:
        /* indicate that we're a full HTML interpreter */
        *result = SYSINFO_ICLASS_HTML;
        return TRUE;
        
    case SYSINFO_HTML:
    case SYSINFO_JPEG:
    case SYSINFO_PNG:
    case SYSINFO_MNG:
    case SYSINFO_MIDI:
    case SYSINFO_PNG_TRANS:
    case SYSINFO_MNG_TRANS:
        /* we support HTML, JPEG, PNG, MNG, MIDI, and PNG/MNG transparency */
        *result = TRUE;
        return TRUE;

    case SYSINFO_TEXT_COLORS:
        /* we support full RGB test colors */
        *result = SYSINFO_TXC_RGB;
        return TRUE;

    case SYSINFO_BANNERS:
        /* we support the full banner API */
        *result = TRUE;
        return TRUE;

    case SYSINFO_TEXT_HILITE:
        /* text highlighting is supported (and so much more) */
        *result = TRUE;
        return TRUE;

    case SYSINFO_PNG_ALPHA:
    case SYSINFO_MNG_ALPHA:
        /* ask the image class if it supports alpha blending */
        *result = CTadsImage::is_alpha_supported();
        return TRUE;

    case SYSINFO_WAV:
    case SYSINFO_WAV_MIDI_OVL:
    case SYSINFO_WAV_OVL:
    case SYSINFO_MPEG:
    case SYSINFO_MPEG2:
    case SYSINFO_MPEG3:
    case SYSINFO_OGG:
        /*
         *   We may support WAV or not, depending on DirectX availability.
         *   If WAV isn't supported, then the overlap features aren't
         *   meaningful, but return false for them anyway in that case.  We
         *   support WAV and the various overlapped playback features if and
         *   only if DirectSound is available.
         *   
         *   We use an integrated software player to play MPEG layer II and
         *   III sounds.  However, we essentially decode MPEG sounds to WAV
         *   in memory and play the resulting WAV through DirectX, so we
         *   require DirectX to play MPEG's, just as we do for WAV's.
         *   
         *   Ogg Vorbis is similar to MPEG; we support it if we have
         *   DirectSound capabilities.  
         */
        *result = (win != 0 && win->get_directsound() != 0);
        return TRUE;

    case SYSINFO_PREF_IMAGES:
        *result = (prefs != 0 && prefs->get_graphics_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_SOUNDS:
        *result = (prefs != 0 && prefs->get_sounds_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_MUSIC:
        *result = (prefs != 0 && prefs->get_music_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_LINKS:
        *result = (prefs != 0
                   ? (prefs->get_links_on()
                      ? 1
                      : prefs->get_links_ctrl() ? 2 : 0)
                   : 0);
        return TRUE;

    case SYSINFO_LINKS_HTTP:
    case SYSINFO_LINKS_FTP:
    case SYSINFO_LINKS_NEWS:
    case SYSINFO_LINKS_MAILTO:
    case SYSINFO_LINKS_TELNET:
        *result = TRUE;
        return TRUE;

    default:
        /* we don't recognize other codes */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Receive notification that a character mapping file has been loaded.
 */
void os_advise_load_charmap(char *id, char *ldesc, char *sysinfo)
{
    CHtmlSys_mainwin *mainwin;
    
    /* get the main window, and tell it about the change */
    mainwin = (CHtmlSys_mainwin *)CHtmlSysFrame::get_frame_obj();
    if (mainwin != 0)
        mainwin->advise_load_charmap(ldesc, sysinfo);
}


/* ------------------------------------------------------------------------ */
/*
 *   Terminate the program 
 */
void os_term(int exit_code)
{
    /* post a quit message */
    PostMessage(0, WM_QUIT, exit_code, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Extended OS interface: command events.
 */

/* command event status */
static S_cmd_evt_stat[OS_CMD_LAST];

/* enable/disable a command event */
void os_enable_cmd_event(int id, unsigned int status)
{
    /* if the ID is off our map, ignore it */
    if (id < 1 || id > OS_CMD_LAST)
        return;

    /* adjust the ID to an array index */
    --id;

    /* if the EVT select bit is present, set the EVT bit */
    if ((status & OSCS_EVT_SEL) != 0)
    {
        /* clear out the old status and set the new status */
        S_cmd_evt_stat[id] &= ~OSCS_EVT_ON;
        S_cmd_evt_stat[id] |= status;
    }

    /* if the GETS select bit is present, set the GETS bit */
    if ((status & OSCS_GTS_SEL) != 0)
    {
        /* clear out the old status and set the new status */
        S_cmd_evt_stat[id] &= ~OSCS_GTS_ON;
        S_cmd_evt_stat[id] |= status;
    }
}

/* get the current status of a command event */
int oss_is_cmd_event_enabled(int id, unsigned int typ)
{
    /* 
     *   if the ID is off our map, then it's enabled by default for os_gets()
     *   and disabled for os_get_event() 
     */
    if (id < 1 || id > OS_CMD_LAST)
        return (typ & (OSCS_GTS_ON));

    /* adjust the ID to an array index */
    --id;

    /* if this ID has never been set before, apply the default settings */
    if (S_cmd_evt_stat[id] == 0)
    {
        /* 
         *   it's never been set - the default is ON for os_gets(), OFF for
         *   os_get_event() 
         */
        S_cmd_evt_stat[id] = OSCS_EVT_SEL | OSCS_GTS_SEL | OSCS_GTS_ON;
    }

    /* return the status for the selected ON bit */
    return S_cmd_evt_stat[id] & typ;
}

