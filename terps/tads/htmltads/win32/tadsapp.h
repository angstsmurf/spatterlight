/* $Header: d:/cvsroot/tads/html/win32/tadsapp.h,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsapp.h - TADS Windows application object
Function
  This object provides a repository for data and methods associated
  with the application.
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/

#ifndef TADSAPP_H
#define TADSAPP_H

#include <windows.h>

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Application object 
 */
class CTadsApp
{
public:
    /*
     *   Run the main event loop.  Returns either when *flag becomes
     *   non-zero, or when the application is terminated by some external
     *   or user action.  If the application is terminated, we'll return
     *   false; if the application should keep running, we'll return true.
     */
    int event_loop(int *flag);

    /*
     *   Set the current accelerator.  If accel is not null, we'll use the
     *   handle to translate accelerators and send resulting commands to
     *   the indicated window handle.  
     */
    void set_accel(HACCEL accel, HWND accel_win)
    {
        /* remember the new accelerator settings */
        accel_ = accel;
        accel_win_ = accel_win;
    }
    
    /* do idle-time processing */
    void do_idle();

    /* create the application object */
    static void create_app(HINSTANCE inst, HINSTANCE pinst,
                           LPSTR cmdline, int cmdshow);

    /* get the main application object */
    static CTadsApp *get_app() { return app_; }

    /* delete the application object */
    static void delete_app();

    /* get the application instance handle */
    HINSTANCE get_instance() { return instance_; }

    /* 
     *   Get a font matching a given description.  If we already have a
     *   font matching the description, we'll return it and set *created
     *   to false; otherwise, we'll invoke the callback function (if
     *   provided; if not, we'll use a default creation function) to
     *   create the font, and set *created to true. 
     */
    class CTadsFont *get_font(const CTadsLOGFONT *font,
                              class CTadsFont *
                              (*create_func)(const CTadsLOGFONT *),
                              int *created);

    /* 
     *   Get a font, given a brief description of the font.  The font will
     *   be a proportional roman font in normal window-text color.  This
     *   does the same thing as the version of get_font() based on a
     *   CTadsLOGFONT structure, but is somewhat simpler to call when the
     *   standard defaults can be used in the CTadsLOGFONT structure.  
     */
    class CTadsFont *get_font(const char *font_name, int point_size)
        { return get_font(font_name, point_size, 0, 0,
                          GetSysColor(COLOR_WINDOWTEXT),
                          FF_ROMAN | VARIABLE_PITCH); }

    /* 
     *   Get a font, given a brief description.  'attributes' contains the
     *   Windows attribute flags (a combination of FF_xxx and xxx_PITCH
     *   flags: FF_SWISS | VARIABLE_PITCH for a proportional sans serif
     *   font) to use if the given font name cannot be found.  
     */
    class CTadsFont *get_font(const char *font_name, int point_size,
                              int bold, int italic, COLORREF color,
                              int attributes);

    /*
     *   Get/set the directory for the standard file dialogs.  If the
     *   application wants to keep track of the directory last used for
     *   these dialogs without actually changing the directory, it can use
     *   these operations.  Simply set the lpstrInitialDir member of the
     *   dialog control structure to get_openfile_dir() before calling the
     *   open file function, and then call set_openfile_dir() with the
     *   resulting file to save the directory for next time. 
     */
    char *get_openfile_dir() const { return open_file_dir_; }
    void set_openfile_dir(const char *fname)
        { set_openfile_dir_or_fname(fname, TRUE); }

    /* 
     *   set the openfile directory to a path - this assumes that the
     *   caller already has a directory path without an attached filename 
     */
    void set_openfile_path(const char *fname)
      { set_openfile_dir_or_fname(fname, FALSE); }

    /*
     *   Set the message capture filter.  If this is set, all input will be
     *   sent here before it's processed through the normal Windows
     *   translation and dispatching routines (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).
     *   
     *   We only maintain one capture filter; callers are responsible for
     *   properly chaining and restoring filters as needed.  For the caller's
     *   convenience in chaining and restoring the older filter, we return
     *   the old filter object.  
     */
    class CTadsMsgFilter *set_msg_filter(class CTadsMsgFilter *filter)
    {
        class CTadsMsgFilter *old_filter;
        
        /* remember the outgoing filter for a moment */
        old_filter = msg_filter_;

        /* set the new filter */
        msg_filter_ = filter;

        /* return the old filter */
        return old_filter;
    }

    /*
     *   Add a modeless dialog to our list.  Each modeless dialog that's
     *   running must be added to this list in order for keyboard events
     *   to be properly distributed to it.
     */
    void add_modeless(HWND win);

    /* 
     *   Remove a modeless dialog from our list.  A dialog that's been
     *   added to our list must be removed before its window is destroyed,
     *   so that we don't refer to it after it's gone.
     */
    void remove_modeless(HWND win);

    /*
     *   Set the MDI frame window.  This should be called when the MDI
     *   frame is first created to set the MDI window.  This should also
     *   be called with a null argument before the MDI window is
     *   destroyed.  
     */
    void set_mdi_win(class CTadsSyswinMdiFrame *mdi_win)
        { mdi_win_ = mdi_win; }

    /* get the Windows OS version information */
    unsigned long get_win_sys_id() const { return win_sys_id_; }
    unsigned long get_win_ver_major() const { return win_ver_major_; }
    unsigned long get_win_ver_minor() const { return win_ver_minor_; }

    /* determine if the OS is win95 or NT4 (and only those - not later) */
    int is_win95_or_nt4() const
    {
        /* 
         *   win95 is major version 4, minor 0, platform WIN32_WINDOWS; NT4
         *   is major 4, minor 0, platform WIN32_NT 
         */
        return ((win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                 && win_ver_major_ == 4 && win_ver_minor_ == 0)
                || (win_sys_id_ == VER_PLATFORM_WIN32_NT
                    && win_ver_major_ == 4 && win_ver_minor_ == 0));
    }

    /* determine if the OS is win98 (and only win98) */
    int is_win98() const
    {
        /* win98 is major version 4, minor version 10 */
        return (win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                && (win_ver_major_ == 4 && win_ver_minor_ == 10));
    }

    /* determine if the OS is win98 or later */
    int is_win98_or_later() const
    {
        /* 
         *   win95 is major version 4, minor version 0; anything with a
         *   higher major version than 4, or a major version of 4 and a minor
         *   version higher than 0, is post-win95, which means win98 or later
         */
        return (win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                && (win_ver_major_ > 4
                    || (win_ver_major_ == 4 && win_ver_minor_ > 0)));
    }

    /* determine if the OS is NT4 or later (this includes 95, 98, and ME) */
    int is_nt4_or_later() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 4);
    }

    /* determine if the OS is Windows 2000 (and ONLY Win2k) */
    int is_win2k() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ == 5
                && win_ver_minor_ == 0);
    }

    /* get the "My Documents" folder path */
    static int get_my_docs_path(char *fname, size_t fname_len);

private:
    CTadsApp();
    CTadsApp(HINSTANCE inst, HINSTANCE pinst, LPSTR cmdline, int cmdshow);

    virtual ~CTadsApp();

    /* 
     *   internal routine to set the openfile directory; takes a flag
     *   indicating whether we're setting the directory from a directory
     *   name or a filename (in the latter case, we'll extract the path
     *   from the filename) 
     */
    void set_openfile_dir_or_fname(const char *fname, int is_filename);

private:
    /* application instance handle */
    HINSTANCE  instance_;

    /* the global application object */
    static CTadsApp *app_;

    /*
     *   font cache - an array of fonts, the number allocated, and the
     *   number actually in use 
     */
    class CTadsFont **fontlist_;
    int fonts_allocated_;
    int fontcount_;

    /* open-file directory */
    char *open_file_dir_;

    /* message filter */
    class CTadsMsgFilter *msg_filter_;

    /* 
     *   our list of modeless dialogs: space for the window handles, the
     *   count of the number of slots allocated, and the number of slots
     *   actually in use 
     */
    HWND *modeless_dlgs_;
    int modeless_dlg_alloc_;
    int modeless_dlg_cnt_;

    /* flag indicating that OLE was initialized successfully */
    int ole_inited_ : 1;

    /* MDI frame window, if this is an MDI application */
    class CTadsSyswinMdiFrame *mdi_win_;

    /* current accelerator and accelerator target window */
    HACCEL accel_;
    HWND accel_win_;

    /* Windows OS system identifier (tells us if we're on 95, NT, etc) */
    unsigned long win_sys_id_;

    /* major and minor Windows OS version identifiers */
    unsigned long win_ver_major_;
    unsigned long win_ver_minor_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Message filter interface - for use with CTadsApp::set_msg_filter().
 */
class CTadsMsgFilter
{
public:
    /* 
     *   Process a system message.  The message filter object always gets
     *   first crack at a message, before we pass it to any modeless dialogs
     *   or to any of the standard system handlers (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).  Return true to indicate that
     *   the message was handled - we'll do nothing further with it.  Return
     *   false to indicate that the standard handling should proceed.  
     */
    virtual int filter_system_message(MSG *msg) = 0;
};


#endif /* TADSAPP_H */

