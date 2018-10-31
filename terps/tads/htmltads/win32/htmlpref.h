/* $Header: d:/cvsroot/tads/html/win32/htmlpref.h,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlpref.h - preferences dialog
Function
  
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#ifndef HTMLPREF_H
#define HTMLPREF_H

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLPLST_H
#include "htmlplst.h"
#endif


/*
 *   Preference ID's - these are the ID's of the preference settings in
 *   the preference property list.  
 */
enum HTML_pref_id_t
{
    /* first property must start at zero */
    HTML_PREF_PROP_FONT = 0,
    HTML_PREF_MONO_FONT,
    HTML_PREF_FONT_SCALE,
    HTML_PREF_USE_WIN_COLORS,
    HTML_PREF_TEXT_COLOR,
    HTML_PREF_BG_COLOR,
    HTML_PREF_LINK_COLOR,
    HTML_PREF_VLINK_COLOR,
    HTML_PREF_ALINK_COLOR,
    HTML_PREF_UNDERLINE_LINKS,
    HTML_PREF_EMACS_CTRL_V,
    HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL,
    HTML_PREF_ALT_MORE_STYLE,
    HTML_PREF_WINPOS,
    HTML_PREF_HLINK_COLOR,
    HTML_PREF_HOVER_HILITE,
    HTML_PREF_DBGWINPOS,
    HTML_PREF_MUSIC_ON,
    HTML_PREF_SOUNDS_ON,
    HTML_PREF_DBGMAINPOS,
    HTML_PREF_OVERRIDE_COLORS,
    HTML_PREF_LINKS_ON,
    HTML_PREF_LINKS_CTRL,
    HTML_PREF_DIRECTX_HIDEWARN,
    HTML_PREF_DIRECTX_ERROR_CODE,
    HTML_PREF_GRAPHICS_ON,
    HTML_PREF_FILE_SAFETY_LEVEL,
    HTML_PREF_EMACS_ALT_V,
    HTML_PREF_COLOR_STATUS_BG,
    HTML_PREF_COLOR_STATUS_TEXT,
    HTML_PREF_FONT_SERIF,
    HTML_PREF_FONT_SANS,
    HTML_PREF_FONT_SCRIPT,
    HTML_PREF_FONT_TYPEWRITER,
    HTML_PREF_INPFONT_DEFAULT,
    HTML_PREF_INPFONT_NAME,
    HTML_PREF_INPFONT_COLOR,
    HTML_PREF_INPFONT_BOLD,
    HTML_PREF_INPFONT_ITALIC,
    HTML_PREF_MEM_TEXT_LIMIT,
    HTML_PREF_CLOSE_ACTION,
    HTML_PREF_POSTQUIT_ACTION,
    HTML_PREF_INIT_ASK_GAME,
    HTML_PREF_INIT_OPEN_FOLDER,
    HTML_PREF_RECENT_1,
    HTML_PREF_RECENT_2,
    HTML_PREF_RECENT_3,
    HTML_PREF_RECENT_4,
    HTML_PREF_RECENT_ORDER,
    HTML_PREF_RECENT_DBG_1,
    HTML_PREF_RECENT_DBG_2,
    HTML_PREF_RECENT_DBG_3,
    HTML_PREF_RECENT_DBG_4,
    HTML_PREF_RECENT_DBG_ORDER,
    HTML_PREF_TOOLBAR_VIS,
    HTML_PREF_SHOW_TIMER,
    HTML_PREF_SHOW_TIMER_SECONDS,
    HTML_PREF_PROP_FONTSZ,
    HTML_PREF_MONO_FONTSZ,
    HTML_PREF_SERIF_FONTSZ,
    HTML_PREF_SANS_FONTSZ,
    HTML_PREF_SCRIPT_FONTSZ,
    HTML_PREF_TYPEWRITER_FONTSZ,
    HTML_PREF_INPUT_FONTSZ,
    HTML_PREF_GC_DATABASE,
    HTML_PREF_GC_BKG,
    HTML_PREF_PROFILE_DESC,
    HTML_PREF_MUTE_SOUND,
    HTML_PREF_DEFAULT_PROFILE,

    /* high water mark - this is always the last property ID */
    HTML_PREF_LAST
};

/*
 *   Main Window Closing action definitions 
 */
const int HTML_PREF_CLOSE_CMD = 1;        /* enter "QUIT" command into game */
const int HTML_PREF_CLOSE_PROMPT = 2;       /* prompt then close the window */
const int HTML_PREF_CLOSE_NOW = 3;               /* close without prompting */

/*
 *   Post-Quit Action definitions
 */
const int HTML_PREF_POSTQUIT_EXIT = 1;   /* wait for a keystroke, then exit */
const int HTML_PREF_POSTQUIT_KEEP = 2;                  /* keep window open */


class CHtmlPreferences
{
public:
    CHtmlPreferences();
    ~CHtmlPreferences();

    /* add a reference */
    void add_ref() { ++refcnt_; }

    /* remove a reference, deleting the object if no longer referenced */
    void release_ref()
    {
        if (--refcnt_ == 0)
            delete this;
    }
    
    /* run the dialog */
    void run_preferences_dlg(HWND owner, class CHtmlSysWin_win32 *win);

    /* run the profiles dialog */
    void run_profiles_dlg(HWND owner, class CHtmlSysWin_win32 *win);

    /* run the appearance dialog */
    void run_appearance_dlg(HWND owner, class CHtmlSysWin_win32 *win,
                            int standalone);

    /* get the global system message ID for broadcasting preference updates */
    UINT get_prefs_updated_msg() const { return prefs_updated_msg_; }

    /* 
     *   get the game window - this is valid while we're running one of the
     *   preferences dialogs 
     */
    class CHtmlSysWin_win32 *get_game_win() const { return win_; }

    /* save/restore the preferences in permanent storage */
    void save();
    void save_as(const char *profile);
    void restore(int synced_only);
    void restore_as(const char *profile, int synced_only);

    /* compare to values in permanent storage */
    int equals_saved(int synced_only);
    int equals_saved(const char *profile, int synced_only);

    /* get my property list */
    class CHtmlPropList *get_proplist() const { return proplist_; }

    /* 
     *   Schedule reformatting of the window.  If 'more_mode' is true,
     *   we'll only reformat the window if it's in MORE mode; otherwise,
     *   we'll unconditionally reformat the window. 
     */
    void schedule_reformat(int more_mode);

    /* 
     *   Notify the game window that a change in sound preferences has
     *   occurred.  The game window might need to cancel sound playback if
     *   the preferences now disable a type of sound.  
     */
    void notify_sound_pref_change();

    /* notify the game window of a change to the link enabling status */
    void notify_link_pref_change();

    /* schedule reloading the game chest data */
    void schedule_reload_game_chest();

    /* save the current game chest into the given database file */
    void save_game_chest_db_as(const char *fname);


    /*
     *   Preference settings 
     */

    /*
     *   Recent games.  We refer to recent games by index - the index is a
     *   character from the game order string, so '0' the first item, '1'
     *   is the second item, and so on.  
     */
    const textchar_t *get_recent_game(textchar_t i) const
        { return get_val_str(get_recent_pref_id(i)); }
    void set_recent_game(textchar_t i, const textchar_t *str)
        { set_val_str(get_recent_pref_id(i), str); }

    /* get the pref ID for a recent game index (starting at '0') */
    static HTML_pref_id_t get_recent_pref_id(textchar_t c)
    {
        static const HTML_pref_id_t id[] =
        {
            HTML_PREF_RECENT_1, HTML_PREF_RECENT_2,
            HTML_PREF_RECENT_3, HTML_PREF_RECENT_4
        };
        int i;

        /* get the index */
        i = c - '0';

        /* get the ID code from our array, making sure we're in bounds */
        if (i > 0 && i < sizeof(id)/sizeof(id[0]))
            return id[i];
        else
            return id[0];
    }

    /* 
     *   recent game order - a string like 0123 indicating the order of
     *   the items in the list 
     */
    const textchar_t *get_recent_game_order() const
        { return get_val_str(HTML_PREF_RECENT_ORDER); }
    void set_recent_game_order(const textchar_t *str)
        { set_val_str(HTML_PREF_RECENT_ORDER, str); }

    /*
     *   Recent debug games - these entries track the projects recently
     *   loaded in the debugger 
     */
    const textchar_t *get_recent_dbg_game(textchar_t i) const
        { return get_val_str(get_recent_dbg_pref_id(i)); }
    void set_recent_dbg_game(textchar_t i, const textchar_t *str)
        { set_val_str(get_recent_dbg_pref_id(i), str); }

    /* get the pref ID for a recent game index (starting at '0') */
    static HTML_pref_id_t get_recent_dbg_pref_id(textchar_t c)
    {
        static const HTML_pref_id_t id[] =
        {
            HTML_PREF_RECENT_DBG_1, HTML_PREF_RECENT_DBG_2,
            HTML_PREF_RECENT_DBG_3, HTML_PREF_RECENT_DBG_4
        };
        int i;

        /* get the index */
        i = c - '0';

        /* get the ID code from our array, making sure we're in bounds */
        if (i > 0 && i < sizeof(id)/sizeof(id[0]))
            return id[i];
        else
            return id[0];
    }

    /* 
     *   recent game order - a string like 0123 indicating the order of
     *   the items in the list 
     */
    const textchar_t *get_recent_dbg_game_order() const
        { return get_val_str(HTML_PREF_RECENT_DBG_ORDER); }
    void set_recent_dbg_game_order(const textchar_t *str)
        { set_val_str(HTML_PREF_RECENT_DBG_ORDER, str); }


    /* 
     *   font preferences 
     */
    
    const textchar_t *get_prop_font() const
        { return get_val_str(HTML_PREF_PROP_FONT); }
    void set_prop_font(const textchar_t *newfont)
        { set_val_str(HTML_PREF_PROP_FONT, newfont); }

    const textchar_t *get_mono_font() const
        { return get_val_str(HTML_PREF_MONO_FONT); }
    void set_mono_font(const textchar_t *newfont)
        { set_val_str(HTML_PREF_MONO_FONT, newfont); }
    
    int get_font_scale() const
        { return get_val_longint(HTML_PREF_FONT_SCALE); }
    void set_font_scale(int newscale)
        { set_val_longint(HTML_PREF_FONT_SCALE, newscale); }

    const textchar_t *get_font_serif() const
        { return get_val_str(HTML_PREF_FONT_SERIF); }
    void set_font_serif(const textchar_t *newfont)
        { set_val_str(HTML_PREF_FONT_SERIF, newfont); }

    const textchar_t *get_font_sans() const
        { return get_val_str(HTML_PREF_FONT_SANS); }
    void set_font_sans(const textchar_t *newfont)
        { set_val_str(HTML_PREF_FONT_SANS, newfont); }

    const textchar_t *get_font_script() const
        { return get_val_str(HTML_PREF_FONT_SCRIPT); }
    void set_font_script(const textchar_t *newfont)
        { set_val_str(HTML_PREF_FONT_SCRIPT, newfont); }

    const textchar_t *get_font_typewriter() const
        { return get_val_str(HTML_PREF_FONT_TYPEWRITER); }
    void set_font_typewriter(const textchar_t *newfont)
        { set_val_str(HTML_PREF_FONT_TYPEWRITER, newfont); }

    /*
     *   font size settings 
     */
    int get_prop_fontsz() const
        { return (int)get_val_longint(HTML_PREF_PROP_FONTSZ); }
    void set_prop_fontsz(int sz)
        { set_val_longint(HTML_PREF_PROP_FONTSZ, sz); }

    int get_mono_fontsz() const
        { return (int)get_val_longint(HTML_PREF_MONO_FONTSZ); }
    void set_mono_fontsz(int sz)
        { set_val_longint(HTML_PREF_MONO_FONTSZ, sz); }

    int get_serif_fontsz() const
        { return (int)get_val_longint(HTML_PREF_SERIF_FONTSZ); }
    void set_serif_fontsz(int sz)
        { set_val_longint(HTML_PREF_SERIF_FONTSZ, sz); }

    int get_sans_fontsz() const
        { return (int)get_val_longint(HTML_PREF_SANS_FONTSZ); }
    void set_sans_fontsz(int sz)
        { set_val_longint(HTML_PREF_SANS_FONTSZ, sz); }

    int get_script_fontsz() const
        { return (int)get_val_longint(HTML_PREF_SCRIPT_FONTSZ); }
    void set_script_fontsz(int sz)
        { set_val_longint(HTML_PREF_SCRIPT_FONTSZ, sz); }

    int get_typewriter_fontsz() const
        { return (int)get_val_longint(HTML_PREF_TYPEWRITER_FONTSZ); }
    void set_typewriter_fontsz(int sz)
        { set_val_longint(HTML_PREF_TYPEWRITER_FONTSZ, sz); }


    /*
     *   input font settings 
     */

    const textchar_t *get_inpfont_name() const
        { return get_val_str(HTML_PREF_INPFONT_NAME); }
    void set_inpfont_name(const textchar_t *newfont)
        { set_val_str(HTML_PREF_INPFONT_NAME, newfont); }

    int get_inpfont_size() const
        { return (int)get_val_longint(HTML_PREF_INPUT_FONTSZ); }
    void set_inpfont_size(int sz)
        { set_val_longint(HTML_PREF_INPUT_FONTSZ, sz); }
    
    HTML_color_t get_inpfont_color() const
        { return get_val_color(HTML_PREF_INPFONT_COLOR); }
    void set_inpfont_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_INPFONT_COLOR, newclr); }
    
    int get_inpfont_bold() const
        { return get_val_bool(HTML_PREF_INPFONT_BOLD); }
    void set_inpfont_bold(int dflt)
        { set_val_bool(HTML_PREF_INPFONT_BOLD, dflt); }

    int get_inpfont_italic() const
        { return get_val_bool(HTML_PREF_INPFONT_ITALIC); }
    void set_inpfont_italic(int dflt)
        { set_val_bool(HTML_PREF_INPFONT_ITALIC, dflt); }
    

    /* 
     *   color preferences 
     */
    
    int get_use_win_colors() const
        { return get_val_bool(HTML_PREF_USE_WIN_COLORS); }
    void set_use_win_colors(int flag)
        { set_val_bool(HTML_PREF_USE_WIN_COLORS, flag); }

    int get_override_colors() const
        { return get_val_bool(HTML_PREF_OVERRIDE_COLORS); }
    void set_override_colors(int flag)
        { set_val_bool(HTML_PREF_OVERRIDE_COLORS, flag); }

    HTML_color_t get_text_color() const
        { return get_val_color(HTML_PREF_TEXT_COLOR); }
    void set_text_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_TEXT_COLOR, newclr); }
    
    HTML_color_t get_bg_color() const
        { return get_val_color(HTML_PREF_BG_COLOR); }
    void set_bg_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_BG_COLOR, newclr); }

    /* 
     *   link preferences 
     */
    
    HTML_color_t get_link_color() const
        { return get_val_color(HTML_PREF_LINK_COLOR); }
    void set_link_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_LINK_COLOR, newclr); }

    HTML_color_t get_vlink_color() const
        { return get_val_color(HTML_PREF_VLINK_COLOR); }
    void set_vlink_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_VLINK_COLOR, newclr); }

    HTML_color_t get_hlink_color() const
        { return get_val_color(HTML_PREF_HLINK_COLOR); }
    void set_hlink_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_HLINK_COLOR, newclr); }

    HTML_color_t get_alink_color() const
        { return get_val_color(HTML_PREF_ALINK_COLOR); }
    void set_alink_color(HTML_color_t newclr)
        { set_val_color(HTML_PREF_ALINK_COLOR, newclr); }
    int get_underline_links() const
        { return get_val_bool(HTML_PREF_UNDERLINE_LINKS); }
    void set_underline_links(int flag)
        { set_val_bool(HTML_PREF_UNDERLINE_LINKS, flag); }

    int get_hover_hilite() const
        { return get_val_bool(HTML_PREF_HOVER_HILITE); }
    void set_hover_hilite(int flag)
        { set_val_bool(HTML_PREF_HOVER_HILITE, flag); }

    /* 
     *   parameterized color settings 
     */

    HTML_color_t get_color_status_bg() const
        { return get_val_color(HTML_PREF_COLOR_STATUS_BG); }
    void set_color_status_bg(HTML_color_t newclr)
        { set_val_color(HTML_PREF_COLOR_STATUS_BG, newclr); }

    HTML_color_t get_color_status_text() const
        { return get_val_color(HTML_PREF_COLOR_STATUS_TEXT); }
    void set_color_status_text(HTML_color_t newclr)
        { set_val_color(HTML_PREF_COLOR_STATUS_TEXT, newclr); }

    /* 
     *   keyboard preferences 
     */

    int get_emacs_ctrl_v() const
        { return get_val_bool(HTML_PREF_EMACS_CTRL_V); }
    void set_emacs_ctrl_v(int flag)
        { set_val_bool(HTML_PREF_EMACS_CTRL_V, flag); }
    int get_emacs_alt_v() const
        { return get_val_bool(HTML_PREF_EMACS_ALT_V); }
    void set_emacs_alt_v(int flag)
        { set_val_bool(HTML_PREF_EMACS_ALT_V, flag); }

    int get_arrow_keys_always_scroll() const
        { return get_val_bool(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL); }
    void set_arrow_keys_always_scroll(int flag)
        { set_val_bool(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL, flag); }

    /* 
     *   MORE style 
     */
    
    int get_alt_more_style() const
        { return get_val_bool(HTML_PREF_ALT_MORE_STYLE); }
    void set_alt_more_style(int flag)
        { set_val_bool(HTML_PREF_ALT_MORE_STYLE, flag); }

    /*
     *   Memory text size limit.  A limit of zero indicates that there is
     *   no limit on how large the text buffer can grow; otherwise, the
     *   limit is the maximum number of bytes that we'll allow in the text
     *   array.  
     */
    unsigned long get_mem_text_limit() const
        { return (unsigned long)get_val_longint(HTML_PREF_MEM_TEXT_LIMIT); }
    void set_mem_text_limit(unsigned long val)
        { set_val_longint(HTML_PREF_MEM_TEXT_LIMIT, (long)val); }

    /*
     *   Ask for game at startup.  If this is set, we'll prompt for a game
     *   immediately at startup; otherwise, we'll just stand by until the
     *   user explicitly opens a game.  
     */
    int get_init_ask_game() const
        { return (int)get_val_longint(HTML_PREF_INIT_ASK_GAME); }
    void set_init_ask_game(int val)
        { set_val_longint(HTML_PREF_INIT_ASK_GAME, (long)val); }

    /*
     *   Initial open folder.  This is the folder we'll start in when
     *   asking for a game to open.
     */
    const textchar_t *get_init_open_folder() const
        { return get_val_str(HTML_PREF_INIT_OPEN_FOLDER); }
    void set_init_open_folder(const textchar_t *str)
        { set_val_str(HTML_PREF_INIT_OPEN_FOLDER, str); }
    
    /*
     *   Main Window Close Action - this specifies the response to closing
     *   the main game window.  
     */
    int get_close_action() const
        { return (int)get_val_longint(HTML_PREF_CLOSE_ACTION); }
    void set_close_action(int val)
        { set_val_longint(HTML_PREF_CLOSE_ACTION, (long)val); }

    /*
     *   Post-Quit Action - this specifies what happens after the user
     *   quits the current game
     */
    int get_postquit_action() const
        { return (int)get_val_longint(HTML_PREF_POSTQUIT_ACTION); }
    void set_postquit_action(int val)
        { set_val_longint(HTML_PREF_POSTQUIT_ACTION, (long)val); }

    /* Game Chest database file location */
    const textchar_t *get_gc_database() const
        { return get_val_str(HTML_PREF_GC_DATABASE); }
    void set_gc_database(const textchar_t *str)
        { set_val_str(HTML_PREF_GC_DATABASE, str); }

    /* Game Chest background image */
    const textchar_t *get_gc_bkg() const
        { return get_val_str(HTML_PREF_GC_BKG); }
    void set_gc_bkg(const textchar_t *str)
        { set_val_str(HTML_PREF_GC_BKG, str); }

    /* profile description */
    const textchar_t *get_profile_desc() const
        { return get_val_str(HTML_PREF_PROFILE_DESC); }
    void set_profile_desc(const textchar_t *str)
        { set_val_str(HTML_PREF_PROFILE_DESC, str); }

    /* 
     *   set the profile description to the default description for one of
     *   the standard pre-defined profiles 
     */
    void set_std_profile_desc(const textchar_t *profile_name);

    /* 
     *   window position 
     */
    
    const CHtmlRect *get_win_pos() const
        { return get_val_rect(HTML_PREF_WINPOS); }
    void set_win_pos(const CHtmlRect *rect)
        { set_val_rect(HTML_PREF_WINPOS, rect); }

    /*
     *   interpreter toolbar visibility 
     */
    int get_toolbar_vis() const
        { return (int)get_val_longint(HTML_PREF_TOOLBAR_VIS); }
    void set_toolbar_vis(int val)
        { set_val_longint(HTML_PREF_TOOLBAR_VIS, (long)val); }

    /*
     *   timer display in status line
     */
    int get_show_timer() const
        { return (int)get_val_longint(HTML_PREF_SHOW_TIMER); }
    void set_show_timer(int val)
        { set_val_longint(HTML_PREF_SHOW_TIMER, (long)val); }

    /* show seconds in timer */
    int get_show_timer_seconds() const
        { return (int)get_val_longint(HTML_PREF_SHOW_TIMER_SECONDS); }
    void set_show_timer_seconds(int val)
        { set_val_longint(HTML_PREF_SHOW_TIMER_SECONDS, (long)val); }

    /* 
     *   debug log window position 
     */
    
    const CHtmlRect *get_dbgwin_pos() const
        { return get_val_rect(HTML_PREF_DBGWINPOS); }
    void set_dbgwin_pos(const CHtmlRect *rect)
        { set_val_rect(HTML_PREF_DBGWINPOS, rect); }

    /* 
     *   debug main control window position 
     */
    
    const CHtmlRect *get_dbgmain_pos() const
        { return get_val_rect(HTML_PREF_DBGMAINPOS); }
    void set_dbgmain_pos(const CHtmlRect *rect)
        { set_val_rect(HTML_PREF_DBGMAINPOS, rect); }

    /*
     *   master sound MUTE control 
     */

    int get_mute_sound() const
        { return get_val_bool(HTML_PREF_MUTE_SOUND); }
    void set_mute_sound(int f)
        { set_val_bool(HTML_PREF_MUTE_SOUND, f); }
    

    /* 
     *   music setting 
     */
    
    int get_music_on() const
        { return get_val_bool(HTML_PREF_MUSIC_ON); }
    void set_music_on(int flag)
        { set_val_bool(HTML_PREF_MUSIC_ON, flag); }

    /* 
     *   sound effects setting 
     */
    
    int get_sounds_on() const
        { return get_val_bool(HTML_PREF_SOUNDS_ON); }
    void set_sounds_on(int flag)
        { set_val_bool(HTML_PREF_SOUNDS_ON, flag); }

    /* 
     *   graphics visibility setting 
     */
    
    int get_graphics_on() const
        { return get_val_bool(HTML_PREF_GRAPHICS_ON); }
    void set_graphics_on(int flag)
        { set_val_bool(HTML_PREF_GRAPHICS_ON, flag); }

    /* 
     *   show links setting 
     */
    
    int get_links_on() const
        { return get_val_bool(HTML_PREF_LINKS_ON); }
    void set_links_on(int flag)
        { set_val_bool(HTML_PREF_LINKS_ON, flag); }

    /* 
     *   show links on control key down setting 
     */
    
    int get_links_ctrl() const
        { return get_val_bool(HTML_PREF_LINKS_CTRL); }
    void set_links_ctrl(int flag)
        { set_val_bool(HTML_PREF_LINKS_CTRL, flag); }

    /* 
     *   hide DirectX warnings on startup 
     */
    
    int get_directx_hidewarn() const
        { return get_val_bool(HTML_PREF_DIRECTX_HIDEWARN); }
    void set_directx_hidewarn(int flag)
        { set_val_bool(HTML_PREF_DIRECTX_HIDEWARN, flag); }

    /* 
     *   DirectX error code from last startup 
     */
    
    enum htmlw32_directx_err_t get_directx_error_code() const
        { return ((htmlw32_directx_err_t)
                  get_val_longint(HTML_PREF_DIRECTX_ERROR_CODE)); }
    void set_directx_error_code(htmlw32_directx_err_t val)
        { set_val_longint(HTML_PREF_DIRECTX_ERROR_CODE, (long)val); }

    /* 
     *   File safety level.  The file safety level is a little different
     *   than other preferences, in that it can have a temporary setting
     *   that is different than the persisently-stored preference setting.
     *   This is used when the player uses a safety-level command line
     *   option when starting the run-time: the command-line setting
     *   overrides the setting from the preferences, but does not get
     *   stored in the preferences.
     *   
     *   set_temp_file_safety_level() overrides the settings in the
     *   preferences.  Calling set_file_safety_level() sets the current
     *   setting as well as the stored setting.  
     */
    int get_file_safety_level() const
    {
        return (temp_file_safety_level_set_
                ? temp_file_safety_level_ 
                : (int)get_val_longint(HTML_PREF_FILE_SAFETY_LEVEL));
    }
    void set_file_safety_level(int level)
    {
        /* set the value in the preferences */
        set_val_longint(HTML_PREF_FILE_SAFETY_LEVEL, level);

        /* from now on, use the preference setting */
        temp_file_safety_level_set_ = FALSE;
    }
    void set_temp_file_safety_level(int level)
    {
        /* remember the value in memory, but not in the saved settings */
        temp_file_safety_level_set_ = TRUE;
        temp_file_safety_level_ = level;
    }

    /*
     *   File safety get/set callbacks for TADS run-time
     */
    static void set_io_safety_level_cb(void *ctx, int level)
    {
        /* 
         *   TADS calls us to set the initial safety level from the
         *   command line - set the command line value as the temporary
         *   setting, overriding any saved preference setting for this
         *   session but not updating the preferences 
         */
        ((CHtmlPreferences *)ctx)->set_temp_file_safety_level(level);
    }
    static int get_io_safety_level_cb(void *ctx)
    {
        return ((CHtmlPreferences *)ctx)->get_file_safety_level();
    }

    /* 
     *   get a property value 
     */
    const textchar_t *get_val_str(HTML_pref_id_t id) const
        { return proplist_->get_val_str(id); }
    const CHtmlRect *get_val_rect(HTML_pref_id_t id) const
        { return proplist_->get_val_rect(id); }
    HTML_color_t get_val_color(HTML_pref_id_t id) const
        { return proplist_->get_val_color(id); }
    long get_val_longint(HTML_pref_id_t id) const
        { return proplist_->get_val_longint(id); }
    int get_val_bool(HTML_pref_id_t id) const
        { return proplist_->get_val_bool(id); }

    /* 
     *   set a property value 
     */
    void set_val_str(HTML_pref_id_t id, const textchar_t *str)
        { proplist_->set_val_str(id, str); }
    void set_val_str(HTML_pref_id_t id, const textchar_t *str, size_t len)
        { proplist_->set_val_str(id, str, len); }
    void set_val_rect(HTML_pref_id_t id, const CHtmlRect *rect)
        { proplist_->set_val_rect(id, rect); }
    void set_val_color(HTML_pref_id_t id, HTML_color_t color)
        { proplist_->set_val_color(id, color); }
    void set_val_longint(HTML_pref_id_t id, long val)
        { proplist_->set_val_longint(id, val); }
    void set_val_bool(HTML_pref_id_t id, int val)
        { proplist_->set_val_bool(id, val); }

    /* get the pointer to the custom colors array */
    COLORREF *get_cust_colors() { return cust_colors_; }

    /*
     *   Offered point sizes - this is an array of the point sizes, in
     *   ascending order, that we offer for the font preferences.  The last
     *   entry is always a zero to indicate the end of the list.  
     */
    static const int font_pt_sizes[];

    /* get/set the active profile name in the registry */
    const textchar_t *get_active_profile_name() const;
    void set_active_profile_name(const char *profile);

    /* get/set the default profile name */
    const textchar_t *get_default_profile() const
        { return proplist_->get_val_str(HTML_PREF_DEFAULT_PROFILE); }
    void set_default_profile(const textchar_t *profile)
        { proplist_->set_val_str(HTML_PREF_DEFAULT_PROFILE, profile); }

    /* get the name of the registry key containing the settings */
    void get_settings_key(char *buf, size_t buflen);

    /* get the name of the registry key for the given profile */
    static void get_settings_key_for(char *buf, size_t buflen,
                                     const char *profile);

    /* check to see if a profile exists */
    static int profile_exists(const char *profile);

    /* 
     *   check a profile name to see if it's a "standard" profile - this is
     *   one of the pre-configured profiles, which shouldn't be deleted or
     *   renamed 
     */
    static int is_standard_profile(const char *profile)
    {
        return (stricmp(profile, "Multimedia") == 0
                || stricmp(profile, "Plain Text") == 0
                || stricmp(profile, "Web Style") == 0);
    }

    /* set the factory defaults for the given standard theme */
    void set_theme_defaults(const textchar_t *theme);

private:
    /* initialize the standard profiles */
    void init_standard_profiles();

    /* read/write a property from/to the system registry */
    void write_to_registry(HTML_pref_id_t id, HKEY key);
    void read_from_registry(HTML_pref_id_t id, HKEY key);

    /* compare a value in memory to the value in the registry */
    int equals_registry_value(HTML_pref_id_t id, HKEY key);
    
    /* reference count */
    int refcnt_;

    /* preference property list */
    class CHtmlPropList *proplist_;

    /* owning window */
    class CHtmlSysWin_win32 *win_;

    /* registry custom color value name */
    static const textchar_t custclr_val_name[];

    /* the active profile name */
    CStringBuf active_profile_;

    /* custom color settings */
    COLORREF cust_colors_[16];

    /* our global 'preferences updated' window message */
    UINT prefs_updated_msg_;

    /* temporary file safety level setting */
    int temp_file_safety_level_;
    int temp_file_safety_level_set_ : 1;
};

#endif /* HTMLPREF_H */

