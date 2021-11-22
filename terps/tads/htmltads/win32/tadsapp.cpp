#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsapp.cpp,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsapp.cpp - TADS Windows application object
Function
  
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <Ole2.h>
#include <Windows.h>
#include <shlobj.h>

#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Application object 
 */

/* static containing the app object singleton */
CTadsApp *CTadsApp::app_ = 0;


/*
 *   initialize 
 */
CTadsApp::CTadsApp(HINSTANCE inst, HINSTANCE pinst,
                   LPSTR /*cmdline*/, int /*cmdshow*/)
{
    OSVERSIONINFO osver;
    
    /* remember the application instance handle */
    instance_ = inst;

    /* we don't have an MDI frame window yet */
    mdi_win_ = 0;

    /* no fonts allocated yet */
    fonts_allocated_ = 0;
    fontlist_ = 0;
    fontcount_ = 0;

    /* initialize OLE */
    switch(OleInitialize(0))
    {
    case S_OK:
    case S_FALSE:
        /* 
         *   note that OLE was initialized, so we'll need to uninitialize
         *   it before we shut down 
         */
        ole_inited_ = TRUE;
        break;

    default:
        /* OLE didn't initialize properly */
        ole_inited_ = FALSE;
        break;
    }

    /* no file-dialog directory yet */
    open_file_dir_ = 0;

    /* we don't have a message filter installed yet */
    msg_filter_ = 0;

    /* allocate some space for modeless dialogs */
    modeless_dlg_cnt_ = 0;
    modeless_dlg_alloc_ = 10;
    modeless_dlgs_ = (HWND *)th_malloc(modeless_dlg_alloc_ * sizeof(HWND));

    /* get OS version information */
    memset(&osver, 0, sizeof(osver));
    osver.dwOSVersionInfoSize = sizeof(osver);
    GetVersionEx(&osver);

    /* note the windows version identifiers */
    win_sys_id_ = osver.dwPlatformId;
    win_ver_major_ = osver.dwMajorVersion;
    win_ver_minor_ = osver.dwMinorVersion;

    /* register our window classes if necessary */
    if (pinst == 0)
    {
        /* register the basic window class */
        CTadsWin::register_win_class(this);

        /* perform static class initialization */
        CTadsWin::class_init(this);
        CTadsWinScroll::class_init(this);
    }
}

CTadsApp::~CTadsApp()
{
    int i;
    CTadsFont **p;
    
    /* perform static termination */
    CTadsWin::class_terminate();
    CTadsWinScroll::class_terminate();

    /* delete any fonts we've allocated */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
        delete *p;

    /* delete the font list */
    if (fontlist_)
    {
        th_free(fontlist_);
        fontlist_ = 0;
        fontcount_ = 0;
        fonts_allocated_ = 0;
    }

    /* delete the open-file buffer, if we have one */
    if (open_file_dir_ != 0)
        th_free(open_file_dir_);

    /* delete our modeless dialog list */
    th_free(modeless_dlgs_);

    /* deinitialize OLE if necessary */
    if (ole_inited_)
        OleUninitialize();
}

/*
 *   create the application object 
 */
void CTadsApp::create_app(HINSTANCE inst, HINSTANCE pinst,
                          LPSTR cmdline, int cmdshow)
{
    /* if there's not already an app, create one */
    if (app_ == 0)
        app_ = new CTadsApp(inst, pinst, cmdline, cmdshow);
}

/*
 *   delete the application object 
 */
void CTadsApp::delete_app()
{
    if (app_ != 0)
    {
        delete app_;
        app_ = 0;
    }
}

/*
 *   Add a modeless dialog to our list of modeless dialogs 
 */
void CTadsApp::add_modeless(HWND win)
{
    /* allocate more space in our list if necessary */
    if (modeless_dlg_cnt_ == modeless_dlg_alloc_)
    {
        modeless_dlg_alloc_ += 10;
        modeless_dlgs_ = (HWND *)
                         th_realloc(modeless_dlgs_,
                                    modeless_dlg_alloc_ * sizeof(HWND));
    }

    /* add this to the end of our list */
    modeless_dlgs_[modeless_dlg_cnt_++] = win;
}

/*
 *   Remove a modeless dialog from our list of modeless dialogs 
 */
void CTadsApp::remove_modeless(HWND win)
{
    HWND *winp;
    int i;
    
    /* find the window in our list */
    for (i = 0, winp = modeless_dlgs_ ; i < modeless_dlg_cnt_ ; ++i, ++winp)
    {
        /* see if this is the one */
        if (*winp == win)
        {
            /* move everything following down a slot */
            if (i + 1 < modeless_dlg_cnt_)
                memmove(winp, winp + 1,
                        (modeless_dlg_cnt_ - i - 1) * sizeof(HWND *));

            /* that's one less dialog in our list */
            --modeless_dlg_cnt_;

            /* done */
            return;
        }
    }
}

/*
 *   main event loop 
 */
int CTadsApp::event_loop(int *flag)
{
    BOOL stat;
    
    /* if there's a flag, initialize it to false */
    if (flag != 0)
        *flag = 0;
    
    /* get and dispatch messages */
    for (;;)
    {
        MSG msg;

        /*
         *   if a flag variable was provided, and the flag variable has
         *   become true, we're done -- return true to indicate that the
         *   application is to continue running 
         */
        if (flag != 0 && *flag != 0)
            return TRUE;
        
#if 0
        /* check for an empty message queue */
        while (!PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
            do_idle();
#endif

        /* process the next message */
        stat = GetMessage(&msg, 0, 0, 0);
        if (stat == -1)
        {
            /* error reading a message - terminate */
            return FALSE;
        }
        else if (stat != 0)
        {
            int done;
            HWND *winp;
            int i;

            /* 
             *   If there's a message filter, give it the first shot at the
             *   message.  If the message filter marks the message as
             *   handled, then skip all remaining processing on it.  
             */
            if (msg_filter_ != 0 && msg_filter_->filter_system_message(&msg))
                done = TRUE;

            /* 
             *   scan the modeless dialog list, and see if any of the
             *   dialogs want to process the message as a dialog
             *   navigation event 
             */
            for (i = 0, winp = modeless_dlgs_, done = FALSE ;
                 i < modeless_dlg_cnt_ ; ++i, ++winp)
            {
                /* see if this is a message for the given dialog */
                if (IsDialogMessage(*winp, &msg))
                {
                    /* it is - don't process the message any further */
                    done = TRUE;
                    break;
                }
            }

            /* 
             *   try treating it as an MDI message, if we have an MDI main
             *   frame window 
             */
            if (!done
                && mdi_win_ != 0
                && TranslateMDISysAccel(mdi_win_->get_client_handle(), &msg))
                done = TRUE;
            
            /* 
             *   If we didn't already process the message, check for
             *   accelerators 
             */
            if (!done
                && (accel_ == 0
                    || !TranslateAccelerator(accel_win_, accel_, &msg)))
            {
                /* translate and dispatch the message */
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            /* the application is quitting */
            return FALSE;
        }
    }
}


/*
 *   idle-time processing 
 */
void CTadsApp::do_idle()
{
}


/*
 *   Get a font given the font description.
 */
class CTadsFont *CTadsApp::get_font(const char *font_name, int point_size,
                                    int bold, int italic, COLORREF color,
                                    int attributes)
{
    CTadsLOGFONT tlf;

    /* set up the font description structure */
    tlf.lf.lfWidth = 0;
    tlf.lf.lfEscapement = 0;
    tlf.lf.lfOrientation = 0;
    tlf.lf.lfCharSet = DEFAULT_CHARSET;
    tlf.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    tlf.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    tlf.lf.lfQuality = DEFAULT_QUALITY;
    tlf.lf.lfHeight = CTadsFont::calc_lfHeight(point_size);
    tlf.lf.lfItalic = (BYTE)italic;
    tlf.lf.lfUnderline = 0;
    tlf.lf.lfStrikeOut = 0;
    tlf.superscript = 0;
    tlf.subscript = 0;
    tlf.color = color;
    tlf.color_set_explicitly = TRUE;
    tlf.bgcolor = 0;
    tlf.bgcolor_set = FALSE;
    tlf.lf.lfWeight = (bold ? FW_BOLD : FW_NORMAL);
    tlf.lf.lfPitchAndFamily = (BYTE)attributes;

    /* copy the font name if provided */
    if (font_name == 0)
    {
        tlf.face_set_explicitly = FALSE;
        tlf.lf.lfFaceName[0] = '\0';
    }
    else
    {
        tlf.face_set_explicitly = TRUE;
        strcpy(tlf.lf.lfFaceName, font_name);
    }
    
    /* get the font */
    return get_font(&tlf, 0, 0);
}


/*
 *   Get a font given a description.  We'll try to find a matching font
 *   in our cache, and return that object; if we don't already have a font
 *   matching this description, we'll create a new one and add it to our
 *   list.  
 */
class CTadsFont *CTadsApp::get_font(const CTadsLOGFONT *logfont,
                                    CTadsFont *(*create_func)
                                    (const CTadsLOGFONT *),
                                    int *created)
{
    int i;
    CTadsFont **p;
    CTadsFont *newfont;
    
    /* run through my list of fonts and see if we can find an exact match */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
    {
        /* if this font matches the description, return it */
        if ((*p)->matches(logfont))
        {
            if (created != 0)
                *created = FALSE;
            return *p;
        }
    }

    /* we'll need to create a new font - ensure the table is big enough */
    if (fonts_allocated_ == fontcount_)
    {
        /* expand the table */
        fonts_allocated_ += 100;
        if (fontlist_ == 0)
            fontlist_ = (CTadsFont **)th_malloc(fonts_allocated_
                                                * sizeof(CTadsFont *));
        else
            fontlist_ = (CTadsFont **)th_realloc(fontlist_,
                fonts_allocated_ * sizeof(CTadsFont *));
    }

    /* create a new font */
    if (created != 0)
        *created = TRUE;
    newfont = (create_func != 0 ? (*create_func)(logfont)
                           : new CTadsFont(logfont));

    /* add it to the table */
    fontlist_[fontcount_++] = newfont;

    /* return it */
    return newfont;
}

/*
 *   Set the open-file directory to the directory of the given file
 */
void CTadsApp::set_openfile_dir_or_fname(const char *fname, int is_filename)
{
    size_t fname_len;

    /* if we were given a filename, find the path prefix */
    if (is_filename)
    {
        const char *last_sep;
        const char *p;

        /* find the last path separator in the filename */
        for (p = fname, last_sep = 0 ; *p != 0 ; ++p)
        {
            /* if this is a path separator, remember it */
            if (*p == ':' || *p == '/' || *p == '\\')
                last_sep = p;
        }
        
        /* see if we found a separator */
        if (last_sep == 0)
        {
            /* 
             *   no separators at all - the file is in the current
             *   directory, so there's nothing we need to do 
             */
            return;
        }

        /* the length we want is that of the path prefix portion only */
        fname_len = last_sep - fname;
    }
    else
    {
        /* we were given a directory name - use the entire string */
        fname_len = strlen(fname);
    }

    /* if we don't have a buffer yet, allocate one */
    if (open_file_dir_ == 0)
        open_file_dir_ = (char *)th_malloc(256);

    /* save everything up to the last separator */
    memcpy(open_file_dir_, fname, fname_len);
    open_file_dir_[fname_len] = '\0';

    /* tell the OS layer to use this as the open file directory */
    oss_set_open_file_dir(open_file_dir_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the "My Documents" folder path 
 */
int CTadsApp::get_my_docs_path(char *fname, size_t fname_len)
{
    IMalloc *imal;
    ITEMIDLIST *idlist;

    /* get a windows shell ITEMIDLIST for the "My Documents" folder */
    if (!SUCCEEDED(SHGetSpecialFolderLocation(0, CSIDL_PERSONAL, &idlist)))
    {
        /* couldn't get the folder location - return an empty path */
        fname[0] = '\0';
        return FALSE;
    }

    /* convert the ID list to a path */
    SHGetPathFromIDList(idlist, fname);

    /* done with the ITEMIDLIST - free it using the shell's allocator */
    SHGetMalloc(&imal);
    imal->Free(idlist);
    imal->Release();

    /* indicate success */
    return TRUE;
}

