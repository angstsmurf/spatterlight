/* $Header: d:/cvsroot/tads/html/win32/htmlw32.h,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlsys_w32.h - HTML system class implementation for Win32
Function
  
Notes
  
Modified
  09/13/97 MJRoberts  - Creation
*/

#ifndef HTMLSYS_W32_H
#define HTMLSYS_W32_H

#include <windows.h>

/* TADS OS layer - for I/O routines */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSJPEG_H
#include "tadsjpeg.h"
#endif
#ifndef TADSPNG_H
#include "tadspng.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   private window message ID's 
 */

/* INIT message */
static const int HTMLM_INIT = WM_USER + 0;

/* SCHEDULE REFORMAT mesage */
static const int HTMLM_REFORMAT = WM_USER + 1;

/* reformat-after-resize message */
static const int HTMLM_ONRESIZE = WM_USER + 2;

/* flags for HTMLM_REFORMAT - encoded in WPARAM for the message */
static const int HTML_F_SHOW_STAT    = 0x01;
static const int HTML_F_FREEZE_DISP  = 0x02;
static const int HTML_F_RESET_SND    = 0x04;

/* WAV playback completion message */
static const int HTMLM_WAV_DONE = WM_USER + 3;

/* 
 *   debug display - display (textchar_t *) message in LPARAM; WPARAM
 *   indicates whether to bring the window to the front (TRUE) or leave it
 *   in the background (FALSE) 
 */
static const int HTMLM_DBG_PRINT = WM_USER + 4;

/* 
 *   debug display, using a specific font; the LPARAM is a pointer to an
 *   CHtmlDbgPrintFontMsg object
 */
static const int HTMLM_DBG_PRINT_FONT = WM_USER + 5;

/* reload game chest data */
static const int HTMLM_RELOAD_GC = WM_USER + 6;

/* last HTMLM message - subclasses can use this as their base */
static const int HTMLM_LAST = WM_USER + 6;

/*
 *   hyperlink HREF for our synthesized link back to the game chest page -
 *   we add this link at the end of a game, to provide an easy way to get
 *   back to the game chest once a game terminates 
 */
#define SHOW_GAME_CHEST_CMD "show-game-chest"

/* 
 *   Maximum number of registered active sounds.  We keep a simple fixed-size
 *   array of sounds for our registration list; this isn't very flexible, but
 *   in practice, our sound architecture only requires us to have a number
 *   simultaneously active sounds equal to the maximum number of sound
 *   layers, which at the of this writing is four.  Set a slightly higher
 *   maximum to make room for a small amount of future expansion.  
 */
static const size_t HTML_MAX_REG_SOUNDS = 16;


/* ------------------------------------------------------------------------ */
/*
 *   Mouse tracking modes
 */
enum html_track_mode_t
{
    /* tracking left button */
    HTML_TRACK_LEFT,

    /* tracking right button */
    HTML_TRACK_RIGHT,

    /* cancel capture by non-mouse event */
    HTML_TRACK_CANCEL
};

/* ------------------------------------------------------------------------ */
/*
 *   Our CHtmlSysTimer subclass 
 */
class CHtmlSysTimer_win32: public CHtmlSysTimer
{
public:
    CHtmlSysTimer_win32(void (*func)(void *), void *ctx, int id)
        : CHtmlSysTimer(func, ctx)
    {
        /* remember our system timer */
        id_ = id;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* set/set next in list */
    CHtmlSysTimer_win32 *get_next() const { return nxt_; }
    void set_next(CHtmlSysTimer_win32 *nxt) { nxt_ = nxt; }

    /* get my system timer ID */
    int get_id() const { return id_; }

protected:
    /* our system timer ID */
    int id_;

    /* next timer in linked list of timers */
    CHtmlSysTimer_win32 *nxt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Private structure that we use for keeping track of registered timer
 *   functions 
 */
struct htmlw32_timer_t
{
    htmlw32_timer_t(void (*func)(void *), void *ctx)
    {
        func_ = func;
        ctx_ = ctx;
        nxt_ = 0;
    }

    /* the function and its context data */
    void (*func_)(void *);
    void *ctx_;

    /* next structure in list */
    struct htmlw32_timer_t *nxt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Private structure for HTMLM_DBG_PRINT_FONT messages 
 */
class CHtmlDbgPrintFontMsg
{
public:
    CHtmlDbgPrintFontMsg(const class CHtmlFontDesc *font, int foreground,
                         const textchar_t *msg)
    {
        font_ = font;
        foreground_ = foreground;
        msg_ = msg;
    }

    /* font for display */
    const class CHtmlFontDesc *font_;

    /* flag: bring debugger window to foreground */
    int foreground_;

    /* message text to display */
    const textchar_t *msg_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Enumeration for DirectX error types 
 */
enum htmlw32_directx_err_t
{
    HTMLW32_DIRECTX_OK = 0,                              /* DirectX is okay */
    HTMLW32_DIRECTX_MISSING,                    /* DirectX is not installed */
    HTMLW32_DIRECTX_BADVSN                     /* incorrect DirectX version */
};

/* ------------------------------------------------------------------------ */
/*
 *   System window object implementation for 32-bit Windows.  This window
 *   is actually a child window - we create panels of this type to display
 *   HTML within the main window (CHtmlSys_mainwin).  Note that this class
 *   multiply inherits from CTadsWin, which provides the basic Windows
 *   window implementation, and from CHtmlSysWin, which is simply an
 *   interface that we need to provide for use by the HTML system.  
 */

/* amount indicators for release_more_mode */
enum Html_more_t
{
    HTMLW32_MORE_PAGE,                                /* show the next page */
    HTMLW32_MORE_LINE,                                /* show the next line */
    HTMLW32_MORE_ALL,                              /* show all pending text */
    HTMLW32_MORE_CLOSE   /* cancel in preparation for destroying the window */
};

class CHtmlSysWin_win32: public CTadsWinScroll, public CHtmlSysWin,
    public CTadsStatusSource, public CTadsAudioControl
{
    friend class CHtmlSysWin_win32_Hist;
    
public:
    CHtmlSysWin_win32(class CHtmlFormatter *formatter,
                      class CHtmlSysWin_win32_owner *owner,
                      int has_vscroll, int has_hscroll,
                      class CHtmlPreferences *prefs);
    ~CHtmlSysWin_win32();

    /*
     *   Set the format-after-resize mode.  This is normally on.  Windows
     *   that don't need to reformat after resizing can turn this off;
     *   this should only be done if the window contains only preformatted
     *   text whose line breaking, centering, and other features are
     *   independent of the window size. 
     */
    void set_format_on_resize(int flag) { need_format_on_resize_ = flag; }

    /* 
     *   Set the border flag.  If the border flag is set, we'll draw a
     *   border at the edge of the window adjacent to our parent.
     */
    void set_has_border(int has_border);

    /* set auto-vscroll style */
    void set_auto_vscroll(int f) { auto_vscroll_ = f; }

    /*
     *   Set the banner flag.  This should be set for windows that are
     *   being used as banners. 
     */
    void set_is_banner_win(int is_banner,
                           CHtmlSysWin_win32 *parent,
                           int where, CHtmlSysWin_win32 *other,
                           HTML_BannerWin_Type_t typ,
                           HTML_BannerWin_Pos_t pos);

    /* get the banner window type */
    HTML_BannerWin_Type_t get_banner_type() const
        { return banner_type_; }

    /* get the banner alignment type */
    HTML_BannerWin_Pos_t get_banner_alignment() const
        { return banner_alignment_; }

    /* get the banner size settings */
    void get_banner_size_info(
        long *width, HTML_BannerWin_Units_t *width_units,
        long *height, HTML_BannerWin_Units_t *height_units)
    {
        *width = banner_width_;
        *width_units = banner_width_units_;
        *height = banner_height_;
        *height_units = banner_height_units_;
    }

    /* close this banner window - removes the window from the display */
    void close_banner_window();

    /* lay out the banner */
    void calc_banner_layout(RECT *parent_rc,
                            class CHtmlSysWin_win32 *first_chi);

    /* get the first banner child */
    class CHtmlSysWin_win32 *get_first_banner_child() const
        { return banner_first_child_; }

    /* determine whether the window is in MORE mode */
    int in_more_mode() const { return more_mode_; }

    /* enable/disable MORE mode in this window */
    void enable_more_mode(int ena) { more_mode_enabled_ = ena; }

    /* set non-stop mode */
    void set_nonstop_mode(int flag)
    {
        /* 
         *   If we were previously in nonstop mode, and we're switching out
         *   of nonstop mode, set the 'last input height' to the current
         *   content height, so that we don't stop for a MORE prompt until we
         *   show another screen-full of text past the point where we
         *   disabled non-stop mode.  
         */
        if (nonstop_mode_ && !flag)
            last_input_height_ = content_height_;

        /* remember the new mode */
        nonstop_mode_ = flag;
    }

    /* set the exit-pause flag */
    void set_exit_pause(int flag);

    /* get the exit-pause flag */
    int get_exit_pause() const { return exit_pause_; }

    /* get my preferences object */
    class CHtmlPreferences *get_prefs() const { return prefs_; }

    /* bring the window to the front */
    void bring_to_front();

    /* 
     *   Get the outer frame window handle.  This may not be the same as
     *   the handle of this window, since this window may be embedded in a
     *   container window.  This call returns the handle of the container
     *   window. 
     */
    HWND get_frame_handle() const;

    /* get the owner window interface */
    class CHtmlSysWin_win32_owner *get_owner() const { return owner_; }

    /*
     *   Process a command.  Default windows don't have any inherent command
     *   processing, so the default implementation asks the owner to do
     *   something with the command.  Windows with their own command
     *   processing can override this to take action directly.
     *   
     *   If 'append' is true, we'll append the text to the end of the current
     *   command; otherwise, we'll clear any text in the buffer before adding
     *   this text.  If 'enter' is true, we'll return immediately from
     *   get_input(), as though the user had pressed Enter; otherwise, we'll
     *   just leave the new text in the buffer so that the player can
     *   continue editing.  
     *   
     *   'os_cmd_id' is the OS_CMD_xxx identifier for the command, if this is
     *   a system menu command.  For game-defined hyperlinks, this is
     *   OS_CMD_NONE.  
     */
    virtual void process_command(const textchar_t *cmd, size_t cmdlen,
                                 int append, int enter, int os_cmd_id);

    /* process a [more]-mode keystroke */
    int process_moremode_key(int vkey, TCHAR ch);

    /* release "MORE" mode, showing the next screen of text */
    void release_more_mode(Html_more_t amount);

    /*
     *   CHtmlSysWin interface implementation 
     */
    
    /* get my window group */
    CHtmlSysWinGroup *get_win_group();

    /* 
     *   Receive notification that the contents of the window are being
     *   cleared.  The owner should call this when it clears the window; the
     *   caller is responsible for updating the parser/formatter contents and
     *   reformatting.  
     */
    void notify_clear_contents()
    {
        /*
         *   we haven't seen any of the text on the new page yet, so we'll
         *   need a "more" prompt if it goes off the bottom of the screen
         *   before we get to the next command line 
         */
        reset_last_input_height();

        /* 
         *   since we have no contents any more, we obviously can't be
         *   scrolled anywhere - reset the internal scrollbar settings 
         */
        vscroll_ofs_ = hscroll_ofs_ = 0;
    }

    /* close the window */
    int close_window(int force);

    /* get the current width of the HTML display area of the window */
    long get_disp_width();

    /* get the current height of the HTML display area of the window */
    long get_disp_height();

    /* get number of pixels per inch */
    long get_pix_per_inch();

    /*
     *   Get the bounds of a string rendered in the given font.  Returns
     *   a point, with x set to the width of the string and y set to the
     *   height of the string.  
     */
    CHtmlPoint measure_text(CHtmlSysFont *font,
                            const textchar_t *str, size_t len,
                            int *ascent);

    /*
     *   Get the size of the debugger source window icon.  Does nothing in
     *   default windows.  
     */
    CHtmlPoint measure_dbgsrc_icon() { return CHtmlPoint(0, 0); }

    /* get the maximum number of characters that fit in a given width */
    size_t get_max_chars_in_width(class CHtmlSysFont *font,
                                  const textchar_t *str,
                                  size_t len, long wid);

    /* draw text */
    void draw_text(int hilite, long x, long y, CHtmlSysFont *font,
                   const textchar_t *str, size_t len);

    /* draw typographical space */
    void draw_text_space(int hilite, long x, long y,
                         class CHtmlSysFont *font, long wid);

    /* draw a bullet */
    void draw_bullet(int hilite, long x, long y, CHtmlSysFont *font,
                     HTML_SysWin_Bullet_t style);

    /* draw horizontal rule */
    void draw_hrule(const CHtmlRect *pos, int shade);

    /* draw a table/cell border */
    void draw_table_border(const CHtmlRect *pos, int width, int cell);

    /* draw a table/cell background */
    void draw_table_bkg(const CHtmlRect *pos, HTML_color_t bgcolor);

    /* draw a debugger source line icon - do nothing in default window */
    void draw_dbgsrc_icon(const CHtmlRect *pos, unsigned int stat) { }

    /* adjust the horizontal scrollbar */
    void fmt_adjust_hscroll() { adjust_scrollbar_ranges(); }

    /* adjust the vertical scrollbar */
    void fmt_adjust_vscroll();

    /* scroll a part of the document into view */
    void scroll_to_doc_coords(const class CHtmlRect *pos);

    /* get the current scrolling position */
    void get_scroll_doc_coords(class CHtmlRect *pos);

    /* invalidate an area given in document coordinates */
    void inval_doc_coords(const class CHtmlRect *area);

    /* receive notification that the display list is being deleted */
    void advise_clearing_disp_list();

    /* do formatting */
    int do_formatting(int show_status, int update_win, int freeze_display);

    /* clear the selection range */
    void clear_sel_range();

    /* clear hover information */
    void clear_hover_info();

    /* set the link we're hovering over */
    void set_hover_link(class CHtmlDispLink *disp);

    /* recalculate the system palette */
    void recalc_palette();

    /* 
     *   Determine if we're using an index-based palette.  Windows always
     *   uses a palette when operating in 8-bit (or less) color resolution
     *   modes. 
     */
    int get_use_palette() { return (get_bits_per_pixel() <= 8); }

    /*
     *   Font management.  The window always owns all of the font
     *   objects, which means that it is responsible for tracking them and
     *   deleting them when the window is destroyed.  Fonts that are given
     *   to a formatter must remain valid as long as the window is in
     *   existence.  Note that this doesn't mean that the font class can't
     *   internally release system resources, if it's necessary on a given
     *   system to minimize the number of system font resources in use
     *   simultaneously -- the portable code can never access the actual
     *   system resources directly, so the implementation of this
     *   interface can manage the system resources internally as
     *   appropriate.  
     */

    /* get the default font */
    CHtmlSysFont *get_default_font();

    /* get a font matching the given characteristics */
    CHtmlSysFont *get_font(const class CHtmlFontDesc *font_desc);

    /* get the font for drawing bullets, given the current font */
    CHtmlSysFont *get_bullet_font(class CHtmlSysFont *font);

    /* -------------------------------------------------------------------- */
    /*
     *   Timers 
     */
    void register_timer_func(void (*timer_func)(void *), void *func_ctx);
    void unregister_timer_func(void (*timer_func)(void *), void *func_ctx);

    /* create a timer */
    virtual class CHtmlSysTimer *create_timer(void (*timer_func)(void *),
                                              void *func_ctx);

    /* set a timer */
    virtual void set_timer(class CHtmlSysTimer *timer, long interval_ms,
                           int repeat);

    /* cancel a timer */
    virtual void cancel_timer(class CHtmlSysTimer *timer);
    
    /* delete a timer */
    virtual void delete_timer(class CHtmlSysTimer *timer);

    /* get the primary DirectSound interface */
    struct IDirectSound *get_directsound();

    /* set the window's title */
    void set_window_title(const textchar_t *title);

    /* set the background, text, and link colors */
    virtual void set_html_bg_color(HTML_color_t color, int use_default);
    virtual void set_html_text_color(HTML_color_t color, int use_default);
    void set_html_link_colors(HTML_color_t link_color,
                              int link_use_default,
                              HTML_color_t vlink_color,
                              int vlink_use_default,
                              HTML_color_t alink_color,
                              int alink_use_default,
                              HTML_color_t hlink_color,
                              int hlink_use_default);

    /* set the background image */
    void set_html_bg_image(class CHtmlResCacheObject *image);

    /* invalidate a portion of the background image */
    void inval_html_bg_image(unsigned int x, unsigned int y,
                             unsigned int wid, unsigned int ht);

    /* map a parameterized color */
    HTML_color_t map_system_color(HTML_color_t color);

    /* get the link colors */
    HTML_color_t get_html_link_color() const
        { return COLORREF_to_HTML_color(link_color_); }
    HTML_color_t get_html_alink_color() const
        { return COLORREF_to_HTML_color(alink_color_); }
    HTML_color_t get_html_vlink_color() const
        { return COLORREF_to_HTML_color(vlink_color_); }
    HTML_color_t get_html_hlink_color() const;

    /* determine if textual links should be drawn underlined */
    int get_html_link_underline() const;

    /* determine if we're to show links at all */
    virtual int get_html_show_links() const;

    /* invalidate all visible links */
    void inval_links_on_screen();

    /* determine if we're to show graphics */
    int get_html_show_graphics() const;

    /* set the size of the window, if it's a banner */
    void set_banner_size(
        long width, HTML_BannerWin_Units_t width_units, int use_width,
        long height, HTML_BannerWin_Units_t height_units, int use_height);

    /* change the banner alignment and style settings, if it's a banner */
    void set_banner_info(HTML_BannerWin_Pos_t pos, unsigned long style);

    /* get banner information */
    void get_banner_info(HTML_BannerWin_Pos_t *pos, unsigned long *style);

    /*
     *   Windows-specific implementation 
     */

    /* do a complete reformat */
    void do_reformat(int show_status, int freeze_display,
                     int reset_sounds);

    /* 
     *   Remove all of the banner windows.  We'll call this whenever we
     *   switch to a new page (or go back to an old history page), since
     *   each page may have its own unique set of banners. 
     */
    void remove_all_banners();

    /*
     *   Schedule reformatting.  This routine can be used when a batch of
     *   changes are in progress, so further changes may be made before
     *   reformatting should be done.  This routine sends a message to the
     *   window indicating that reformatting must eventually occur, unless
     *   such a message is already pending.
     */
    void schedule_reformat(int show_status, int freeze_display,
                           int reset_sounds);

    /* receive notification of a change to sound preferences */
    void notify_sound_pref_change();

    /* receive notification of a change to link enabling preferences */
    virtual void notify_link_pref_change() { }

    /* schedule reloading the game chest data */
    void schedule_reload_game_chest();

    /* save the game chest database to the given file */
    void save_game_chest_db_as(const char *fname);

    /* process palette changes */
    int do_querynewpalette();
    void do_palettechanged(HWND initiating_window);

    /* select a font */
    HGDIOBJ select_font(HDC dc, class CHtmlSysFont *font);

    /* restore the font previously selected */
    void unselect_font(HDC dc, class CHtmlSysFont *font, HGDIOBJ oldfont);

    /* 
     *   Set up a status line -- we never have our own status line, but
     *   the containing window may have one.  If we're provided with a
     *   status line, we'll provide status line messages for formatting
     *   operations.  
     */
    void set_status_line(class CTadsStatusline *statusline);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* is the given OS_CMD_xxx command enabled for process_command() use? */
    virtual int is_cmd_evt_enabled(int os_cmd_id) const;

    /* translate an internal menu command ID to an OS_CMD_xxx code */
    static int cmd_id_to_os_cmd_id(int command_id);

    /* 
     *   receive notification that the parent is being destroyed -- clean
     *   up any references we have to the parent 
     */
    virtual void on_destroy_parent();

    /* forget my owner */
    void forget_owner() { owner_ = 0; }

    /* forget my formatter */
    void forget_formatter() { formatter_ = 0; }

    /* update the caret position for scrolling or reformatting */
    virtual void update_caret_pos(int bring_on_screen,
                                  int update_sel_range);

    /* 
     *   reset the last input height counter for the purposes of figuring
     *   out where the "more" prompt goes - this should be called whenever
     *   the screen is cleared 
     */
    void reset_last_input_height() { last_input_height_ = 0; }

    /*
     *   Display a MORE prompt for the game - this is used when the game
     *   explicitly asks to display a MORE prompt.  We'll clear any
     *   current more mode first, then wait for the user to acknowledge
     *   another MORE prompt before returning.  
     */
    void do_game_more_prompt();

    /* set the "pruning" status bit */
    void set_pruning_msg(int flag) { pruning_msg_ = flag; }

    /* 
     *   note a change in the debugger display preferences (this is called on
     *   debugger windows only) 
     */
    virtual void note_debug_format_changes(HTML_color_t bkg_color,
                                           HTML_color_t text_color,
                                           int use_windows_colors);

    /* notify registered active sounds of a change in the mute status */
    void mute_registered_sounds(int mute);

    /*
     *   CTadsAudioControl implementation 
     */

    /* reference control - defer to our OLE IUnknown implementation */
    virtual void audioctl_add_ref() { AddRef(); }
    virtual void audioctl_release() { Release(); }

    /* get the current muting status */
    virtual int get_mute_sound();

    /* register/unregister an active sound */
    virtual void register_active_sound(class CTadsAudioPlayer *s);
    virtual void unregister_active_sound(class CTadsAudioPlayer *s);
    
protected:
    /* draw text with optional clipping and Windows ExtTextOut flags */
    void draw_text_clip(int hilite, long x, long y,
                        class CHtmlSysFont *font,
                        const textchar_t *str, size_t len,
                        unsigned int flags, const RECT *doc_cliprect);

    /* run a 'find' command */
    void do_find(int command_id);

    /* show the text-not-found message for a 'find' command */
    virtual void find_not_found();

    /* get the starting position for a 'find' command */
    virtual void find_get_start(int start_at_top,
                                unsigned long *sel_start,
                                unsigned long *sel_end);

    /* map a parameterized font name */
    int get_param_font(struct CTadsLOGFONT *tlf, int *base_point_size,
                       const textchar_t *name, size_t len,
                       CHtmlFontDesc *font_desc);

    /* show/hide the caret */
    void show_caret();
    void hide_caret();

    /* 
     *   Hide/show the caret for a temporary modal operation.  One 'show'
     *   call must be made for each 'hide' call.  
     */
    void modal_hide_caret();
    void modal_show_caret();

    /* determine if I'm in the foreground */
    int is_in_foreground() const;

    /*
     *   CTadsWin subclass overrides
     */

    /* get window creation style flags */
    DWORD get_winstyle() { return WS_CHILD | WS_CLIPCHILDREN; }
    DWORD get_winstyle_ex() { return 0; }

    /* process creation event */
    void do_create();

    /* receive system window destruction notification */
    void do_destroy();

    /* mouse event handlers */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_mousemove(int keys, int x, int y);
    int do_leftbtn_up(int keys, int x, int y);
    int do_rightbtn_down(int keys, int x, int y, int clicks);
    int do_rightbtn_up(int keys, int x, int y);

    /* handle focus */
    void do_setfocus(HWND prev_focus);
    void do_killfocus(HWND next_focus);

    /* handle control notifications */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* capture change event handler */
    int do_capture_changed(HWND new_capture_win);

    /* paint the window contents */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* erase the background */
    int do_erase_bkg(HDC hdc);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* handle timer messages */
    int do_timer(int timer_id);

    /* 
     *   perform miscellaneous background processing tasks - this is
     *   called periodically on a timer 
     */
    void do_idle();

    /* enter/exit size/move operation */
    void do_entersizemove();
    void do_exitsizemove();

    /* handle a keystroke */
    int do_char(TCHAR c, long keydata);
    int do_keydown(int virtual_key, long keydata);
    int do_keyup(int virtual_key, long keydata);
    int do_syschar(TCHAR c, long keydata);

    /* handle setcursor event */
    int do_setcursor(HWND hwnd, int hittest, int mousemsg);

    /* set the mouse cursor based on the given display item */
    virtual void set_disp_item_cursor(class CHtmlDisp *disp,
                                      int docx, int docy);

    /*
     *   Set the background cursor.  do_setcursor() calls this when it
     *   finds that the mouse isn't over a display list item.  By default,
     *   this simply returns false to indicate that the window should set
     *   the default arrow cursor.  Subclasses can override as desired to
     *   set different background cursors.  
     */
    virtual int do_setcursor_bkg() { return FALSE; }

    /* determine if scrollbars are present */
    int vscroll_is_visible() const
    {
        return get_vscroll_handle() != 0
            && IsWindowVisible(get_vscroll_handle());
    }
    int hscroll_is_visible() const
    {
        return get_hscroll_handle() != 0
            && IsWindowVisible(get_hscroll_handle());
    }

    /* receive notification that scrolling has occurred */
    virtual void notify_scroll(HWND hwnd, long oldpos, long newpos);

    /* get the scrollbar settings */
    int get_scroll_info(int vert, SCROLLINFO *info);

    /* adjust scrollbars */
    void adjust_scrollbar_ranges();

    /* set the current accelerator table based on the preference settings */
    virtual void set_current_accel()
    {
        /* 
         *   default implementation does nothing, since the default window
         *   doesn't allow input and thus isn't sensitive to any of the
         *   keyboard preference settings 
         */
    }

    /*
     *   Internal routines to set the colors.  HTML and prefrence dialog
     *   changes should go through set_html_xxx_color, since HTML changes
     *   must be processed through the preferences settings. 
     */
    void internal_set_bg_color(COLORREF color);
    void internal_set_text_color(COLORREF color);

    /*
     *   CTadsStatusSource implementation 
     */
    textchar_t *get_status_message(int *caller_deletes);

protected:
    /* set the keyboard focus to this window */
    virtual void take_focus()
    {
        /* 
         *   if I don't already have focus, and I'm allowed to take it, set
         *   focus to my window handle 
         */
        if (GetFocus() != handle_)
            SetFocus(handle_);
    }

    /* map the foreground object's palette - return true if palette changed */
    int recalc_palette_fg();

    /* 
     *   map the palettes for the background objects - returns true if the
     *   palette changed 
     */
    int recalc_palette_bg();
    
    /* font creation function - used as a callback for CTadsApp::get_font */
    static class CTadsFont *create_font(const CTadsLOGFONT *lf);

    /* adjust the scrollbars and caret after reformatting the window */
    void adjust_for_reformat();

    /* reformat the window's contents after a resize operation */
    void format_after_resize(long new_width);

    /* figure out where the "MORE" prompt goes */
    void get_moreprompt_rect(RECT *rc);

    /* determine if the MORE prompt is showing */
    int moreprompt_is_at_bottom();

    /* invalidate the "more" prompt area */
    void inval_more_area();

    /* process a scrolling key */
    int process_scrolling_key(int vkey);

    /* scroll as needed to bring the caret on-screen */
    void scroll_to_show_caret();

    /* scroll to show the current selection */
    void scroll_to_show_selection();

    /* scroll to show a point (in screen coordinates) */
    void scroll_to_show_point(const struct CHtmlPoint *pt, int wid, int ht);

    /* set the size of the caret according to a given font */
    virtual void set_caret_size(CHtmlSysFont *font);

    /*
     *   update the command selection range using the current formatter
     *   selection range - call this whenever the formatter selection
     *   range is updated directly, as when the user selects text with the
     *   mouse.  If caret_at_high_end is true, the caret should be placed
     *   at the high end of the selection range; otherwise, the caret is
     *   placed at the low end of the range.  
     */
    void fmt_sel_to_cmd_sel(int caret_at_high_end);

    /* update the display after changing the input buffer contents */
    virtual void update_input_display() { }

    /* finish tracking the mouse */
    virtual int end_mouse_tracking(html_track_mode_t mode);

    /* cut/copy text to clipboard; returns true if any work was done */
    int do_copy();
    int do_cut();

    /* 
     *   Copy the current selection to a new global memory handle.  We'll
     *   allocate the new memory with GlobalAlloc using the given flags.
     *   Returns the length of the text in *len, if desired (len can be
     *   null if the caller doesn't care how long the result is).  
     */
    HGLOBAL copy_to_new_hglobal(UINT alloc_flags, size_t *len);

    /* paste text from the clipboard; returns true if any work was done */
    int do_paste();

    /* 
     *   Insert text from an hglobal in clipboard text format.  Returns true
     *   if a display change occurs, false if not.  
     */
    int insert_text_from_hglobal(const char *buf);

    /* determine if we can cut/copy/paste */
    int can_cut();
    int can_copy();
    int can_paste();

    /*
     *   Drag source support - provide the data object and effects for
     *   dragging from this window.  We provide a text data object
     *   containing the selection range, and we allow only copying the
     *   selection.  
     */
    IDataObject *get_drag_dataobj();
    ULONG get_drag_effects() { return DROPEFFECT_COPY; }

    /* before/after drag notifications */
    void drag_pre();
    void drag_post();

    /* turn "MORE" mode on or off */
    void set_more_mode(int flag);

    /* 
     *   Check to see if we can start reading a command.  Since the
     *   default window doesn't support input, this doesn't do anything by
     *   default; subclasses that support input will override this as
     *   needed. 
     */
    virtual void check_cmd_start() { }

    /* set the font scale */
    void set_font_scale(int new_scale);

    /*
     *   Start a system timer for registered timer callbacks.  This should
     *   be called when we create a window or when we register a new
     *   timer. 
     */
    void start_sys_timer();

    /* 
     *   Load a context popup menu, replacing any existing one.  This
     *   should be called during subclass construction to load the
     *   appropriate context popup for the subclass; by default, we'll
     *   load the standard editing popup. 
     */
    void load_context_popup(int menu_id);

    /*
     *   Set the temporary link visibility state.  This is used to
     *   implement a mode where we only show links when a particular key
     *   combination is being held down. 
     */
    void set_temp_show_links(int flag);

    /* 
     *   get the hyperlink associated with a given mouse position and a given
     *   display item 
     */
    class CHtmlDispLink *find_link_for_disp(
        class CHtmlDisp *disp, const CHtmlPoint *pt);

    /* load a resource string into a CStringBuf */
    void load_res_str(class CStringBuf *buf, int res);

    /* create our fixed palette */
    void create_palette();

    /* add a banner window to my child list */
    void add_banner_child(class CHtmlSysWin_win32 *chi,
                          int where, class CHtmlSysWin_win32 *other);

    /* remove a banner window from my child list */
    void remove_banner_child(class CHtmlSysWin_win32 *chi);

    /* receive notification that our banner parent is closing */
    void on_close_parent_banner(int deleting_parent);

    /* "MORE" prompt message */
    CStringBuf more_prompt_str_;

    /* status line strings */
    CStringBuf more_status_str_;
    CStringBuf working_str_;
    CStringBuf press_key_str_;
    CStringBuf exit_pause_str_;

    /* 
     *   Our palette, for 8-bit video modes.  We use a fixed palette
     *   consisting of the 216-color cube of "web-safe" colors that most web
     *   browsers support, the 20 standard Windows colors, and ramps of gray,
     *   blue, green, and red.  
     */
    HPALETTE hpal_;

    /*
     *   My owner.  The owner provides us with this abstract interface
     *   that allows us to perform certain operations that depend on our
     *   container.  The actual owner object is irrelevant to us; all we
     *   need is this interface that lets us perform operations that we
     *   can't perform ourselves because they exceed our scope.  
     */
    class CHtmlSysWin_win32_owner *owner_;

    /*
     *   Current window font size setting.  This is a global setting that
     *   affects the scaling of all of the fonts.  Values range from 0
     *   (smallest) to 4 (largest).  
     */
    int font_scale_;

    /*
     *   width at which we did the last formatting - if the window size
     *   changes, we'll need to reformat, so we need to keep track of the
     *   size at the time we do a format pass
     */
    long fmt_width_;

    /* default font */
    class CHtmlSysFont_win32 *default_font_;

    /*
     *   Vertical extent of the content we're displaying.  This may
     *   differ from the amount in the formatter, because we top between
     *   screens during a long run of output to make sure the user has had
     *   a chance to read the current screenfull before adding more text.
     */
    long content_height_;

    /*
     *   The content height at the time of the last user input.  When the
     *   formatter is producing a long run of output, we pause between
     *   screens to make sure the user has had a chance to see all of the
     *   text before we scroll it away.  This member records the position
     *   of the last input, so we can be sure not to add more than another
     *   screenfull without getting more input. 
     */
    long last_input_height_;

    /*
     *   Flag indicating whether MORE mode is enabled.
     */
    int more_mode_enabled_ : 1;

    /*
     *   Flag indicating that we're in "MORE" mode.  We're in "MORE" mode
     *   whenever we have more formatter output pending than we're
     *   currently displaying.  At these times, we await user input before
     *   showing more information, so that the user has a chance to see
     *   and acknowledge the information we've displayed so far. 
     */
    int more_mode_ : 1;

    /* flag: processing an explicit game morePrompt request */
    int game_more_request_ : 1;

    /*
     *   Another more mode flag, this one for modal event loops.  This
     *   flag gets set whenever we terminate more mode.  Code that needs
     *   to wait until more mode is over can run a modal event loop based
     *   on this flag. 
     */
    int released_more_;

    /* 
     *   EOF flag for MORE mode - when this is set, we will not wait for
     *   an event when entering MORE mode but simply continue 
     */
    int eof_more_mode_ : 1;

    /*
     *   Flag indicating whether we need to reformat the window after
     *   resizing it 
     */
    int need_format_on_resize_ : 1;

    /* 
     *   Flag indicating that we're in the process of reformatting after
     *   resizing the window.  When this flag is set, we'll suppress
     *   additional recursive reformattings that result from further size
     *   changes, since we need to force the process to converge.  
     */
    int doing_resize_reformat_ : 1;
    
    /*
     *   y position at which the formatter clipped the last drawing.  We
     *   need to keep track of this so that we can draw those areas the
     *   next time we scroll. 
     */
    long clip_ypos_;

    /* command buffer */
    class CHtmlInputBuf *cmdbuf_;

    /* parser tag coordinating the input with the formatter */
    class CHtmlTagTextInput *cmdtag_;

    /*
     *   Flag indicating that we've started reading a command.  We can
     *   start reading a command when we've finished showing all the text
     *   up to the command. 
     */
    int started_reading_cmd_ : 1;

    /* 
     *   "Nonstop" mode flag.  When this is set, it overrides and disables
     *   our MORE mode until the next input.  This is used to allow the
     *   console layer to flag that it's reading a script in non-stop mode,
     *   so pauses for MORE prompts are not desired until the script
     *   completes.  
     */
    int nonstop_mode_ : 1;

    /* caret position */
    CHtmlPoint caret_pos_;

    /*
     *   total height of the caret, and the height of the part above the
     *   text baseline 
     */
    int caret_ht_;
    int caret_ascent_;
    
    /* 
     *   Caret temporarily hiding during a modal operation.  This indicates
     *   that we're hiding the caret for a modal operation only, and will
     *   show it again once the operation is done.  This overrides
     *   caret_vis_ - caret_vis_ may continue to be true while
     *   caret_modal_hide_ is set.
     *   
     *   This keeps track of the depth of the modal hide.  Each time we
     *   enter a modal operation that hides the caret, we'll increment this.
     */
    int caret_modal_hide_;

    /* caret visibility */
    int caret_vis_ : 1;

    /* 
     *   Caret enabled.  We'll set this to false by default; subclasses
     *   should set to true if they want the caret to be shown. 
     */
    int caret_enabled_ : 1;

    /* I-beam cursor */
    HCURSOR ibeam_csr_;

    /* hand cursor */
    HCURSOR hand_csr_;

    /* flag: true -> we're tracking a mouse button click */
    int tracking_mouse_ : 1;

    /* starting position of the current mouse drag (doc coords) */
    CHtmlPoint track_start_;

    /* starting selection range for the current mouse drag */
    unsigned long track_start_left_;
    unsigned long track_start_right_;

    /* number of clicks that started the current mouse drag */
    int track_clicks_;

    /* 
     *   flag indicating that we should clear the selection on mouse-up if
     *   the current click doesn't end up doing something that shouldn't
     *   clear the selection (such as initiate a drag/drop operation or a
     *   context menu) 
     */
    int clear_sel_pending_ : 1;

    /* 
     *   link item that we're tracking, if the mouse was clicked over a
     *   link item 
     */
    class CHtmlDispLink *track_link_;

    /*
     *   link item that the mouse is hovering over - we'll display its
     *   title message on the status line 
     */
    class CHtmlDispLink *status_link_;

    /*
     *   Last good caret position in command input line.  We'll restore
     *   this position if the user makes a zero-length selection outside
     *   of the command line, since a zero-length selection outside of the
     *   command line is useless and doesn't provide any visual feedback.
     */
    size_t last_cmd_caret_;

    /*
     *   For last selection, true -> we ended the drag at the right end
     *   of the selection, false -> ended at left end We use this when
     *   extending a selection to determine the starting point of the
     *   selection range for the continuation; we always take the starting
     *   point of the previous range as the starting point for the
     *   continuation of the selection.  
     */
    int caret_at_right_ : 1;

    /* menu containing the context menus */
    HMENU popup_container_;

    /* context menu for text area */
    HMENU edit_popup_;

    /* background image */
    class CHtmlResCacheObject *bg_image_;

    /* background color and brush */
    COLORREF bgcolor_;
    HBRUSH bgbrush_;

    /* default text color */
    COLORREF text_color_;

    /* default link colors */
    COLORREF link_color_;
    COLORREF vlink_color_;
    COLORREF alink_color_;
    COLORREF hlink_color_;

    /* note availability of WingDing font */
    int wingdings_avail_ : 1;

    /* characters to use for bullets: square, circle, disc */
    textchar_t bullet_square_;
    textchar_t bullet_disc_;
    textchar_t bullet_circle_;

    /* 
     *   Flag indicating that we should display a "reformatting" status
     *   line message.  This should be set before commencing possibly long
     *   operations, such as reformatting everything after resizing the
     *   window. 
     */
    int formatting_msg_ : 1;

    /*
     *   Flag indicating that we're in the process of pruning the parse
     *   tree, so that the status line can display an appropriate message.
     */
    int pruning_msg_ : 1;

    /* status line */
    class CTadsStatusline *statusline_;

    /* preferences object */
    class CHtmlPreferences *prefs_;

    /* flag indicating that we're waiting for a keystroke to exit */
    int exit_pause_ : 1;

    /* 
     *   Flag indicating an interactive size/move operation is in progress
     *   -- we set this flag when we receive WM_ENTERSIZEMOVE, and clear
     *   it when we receive WM_EXITSIZEMOVE.  While an interactive
     *   size/move operation is running, we defer formatting until the
     *   operation is finished, since we can't do a complete reformat
     *   quickly enough to keep up with an interactive operation. 
     */
    int in_size_move_ : 1;

    /* current display width (for get_disp_width) */
    long disp_width_;

    /* current display height (for get_disp_height) */
    long disp_height_;

    /* ID of the system timer we use to service registered timer callbacks */
    int cb_timer_id_;

    /* ID of the system timer for our miscellaneous background timer */
    int bg_timer_id_;

    /* list of timer callback functions */
    struct htmlw32_timer_t *timer_list_head_;

    /* list of system timer objects */
    class CHtmlSysTimer_win32 *sys_timer_head_;

    /* flag indicating whether we have a border at the bottom of the window */
    unsigned int has_border_ : 1;

    /* 
     *   flag indicating whether we scroll vertically to keep the current
     *   output position in view when adding text to the window 
     */
    int auto_vscroll_ : 1;

    /* flag indicating whether this window is a banner */
    int is_banner_win_ : 1;

    /* 
     *   Banner parent.  During layout, our space is carved out of the area
     *   of the parent window.
     *   
     *   Note that this isn't the parent for Windows API purposes: we're not
     *   actually a child, from Win32's perspective, of our parent window.
     *   The parent/child relationship is purely internal, and is used for
     *   our tiled layout mechanism; this is different from the relationship
     *   implied in Win32 between parent and child windows.  
     */
    CHtmlSysWin_win32 *banner_parent_;

    /* head of list of banner children */
    CHtmlSysWin_win32 *banner_first_child_;

    /* 
     *   handle for our border window - we use this to display the border at
     *   the edge of a banner window 
     */
    HWND border_handle_;

    /* next banner sibling */
    CHtmlSysWin_win32 *banner_next_;

    /* if this is a banner, the window type */
    HTML_BannerWin_Type_t banner_type_;

    /* if this is a banner, the alignment type for the banner */
    HTML_BannerWin_Pos_t banner_alignment_;

    /* banner size setting */
    long banner_width_;
    long banner_height_;
    HTML_BannerWin_Units_t banner_width_units_;
    HTML_BannerWin_Units_t banner_height_units_;

    /* array of our registered active sounds */
    class CTadsAudioPlayer *active_sounds_[HTML_MAX_REG_SOUNDS];

    /* 
     *   flag indicating that the keystroke we're waiting for is simply
     *   for pausing -- if this is set, we'll display a status line
     *   message to tell the user we're waiting for input 
     */
    int waiting_for_key_pause_ : 1;

    /* timer ID for starting temp_show_links_ mode */
    int temp_link_timer_id_;

    /* 
     *   link toggle mode: this specifies the result if we toggle the
     *   linking mode 
     */
    int toggle_link_on_ : 1;
    int toggle_link_ctrl_ : 1;

    /* 
     *   our tooltip control - we use this for purposes such as displaying
     *   pop-up tooltips over things with ALT tags 
     */
    HWND tooltip_;

    /* flag indicating that the tooltip just vanished */
    int tooltip_vanished_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Subclass of HTML system window for the history panel.  We show history
 *   with a window that mimics the main HTML panel, but is actually a
 *   separate window; we don't need input in this window, so we use the
 *   basic window rather than the more elaborate input-enabled window.
 *   
 *   When we need to show history, we hide the main input window (leaving it
 *   otherwise unaffected), and show the history window in exactly the same
 *   area of the display where the main window was.  We then load the
 *   history window with a saved parse tree from the point where we created
 *   that history page; we create a history page whenever the game program
 *   clears the screen, saving the contents of the main panel as it is just
 *   before clearing the window.
 */
class CHtmlSysWin_win32_Hist: public CHtmlSysWin_win32
{
public:
    CHtmlSysWin_win32_Hist(class CHtmlFormatter *formatter,
                           class CHtmlSysWin_win32_owner *owner,
                           int has_vscroll, int has_hscroll,
                           class CHtmlPreferences *prefs)
        : CHtmlSysWin_win32(formatter, owner,
                            has_vscroll, has_hscroll, prefs)
        { }

    /* handle keystrokes */
    int do_char(TCHAR c, long keydata);
    int do_keydown(int virtual_key, long keydata);

    /* handle commands */
    int do_command(int notify_code, int command_id, HWND ctl);
    TadsCmdStat_t check_command(int command_id);
};

/* ------------------------------------------------------------------------ */
/*
 *   Subclass of HTML System Window that allows text input.  This class
 *   operates essentially the same as the normal HTML window, but extends
 *   it to allow reading input.  
 */
class CHtmlSysWin_win32_Input: public CHtmlSysWin_win32
{
public:
    CHtmlSysWin_win32_Input(class CHtmlFormatterInput *formatter,
                            class CHtmlSysWin_win32_owner *owner,
                            int has_vscroll, int has_hscroll,
                            class CHtmlPreferences *prefs);

    /*
     *   Read a keyboard command, with an optional timeout.  Returns an
     *   OS_EVT_xxx status code.  
     */
    int get_input_timeout(char *buf, size_t bufsiz,
                          unsigned long timeout, int use_timeout);

    /* begin command line input editing mode */
    void get_input_begin(size_t bufsiz);

    /* 
     *   cancel input previously started with get_input_timeout() and
     *   subsequently interrupted by a timeout 
     */
    void get_input_cancel(int reset);

    /*
     *   Read an event.  Follows TADS os_get_event() semantics.  Returns
     *   OS_EVT_EOF if the application is quitting.  
     */
    int get_input_event(unsigned long timeout, int use_timeout,
                        os_event_info_t *info);

    /* check for a break key sequence */
    int check_break_key();

    /* 
     *   Wait for a keystroke.  If 'pause_only' is true, it means that we
     *   don't care about the return value but are simply waiting for a key
     *   for a user-controlled pause; we'll use this to display an
     *   appropriate status message.  Returns an OS_EVT_xxx status code.  
     */
    int wait_for_keystroke(textchar_t *ch, int pause_only,
                           time_t timeout, int use_timeout);

    /*
     *   Set or clear the end-of-file flag.  When the EOF flag is set, any
     *   pending or subsequent get_input, wait_for_keystroke,
     *   get_input_event, or similar method will return end of file.  
     */
    void set_eof_flag(int f);

    /* get the current EOF status */
    int get_eof_flag() const { return eof_; }

    /* 
     *   Set the game started/game over flags.  "Game started" indicates
     *   that a game has *ever* been started; "game over" indicates that
     *   no game is currently running.  These are used primarily for
     *   status messages.  
     */
    void set_game_started(int f) { game_started_ = f; }
    void set_game_over(int f) { game_over_ = f; }

    /* get/set the game chest flag */
    int is_in_game_chest() const { return in_game_chest_; }
    void set_in_game_chest(int f)
    {
        /* remember the new game chest setting */
        in_game_chest_ = f;

        /* 
         *   In game chest mode, turn off auto-vscroll; otherwise, turn it on
         *   by default.  The game chest window isn't an input window, so we
         *   don't want it to scroll anywhere by itself.  
         */
        auto_vscroll_ = !f;
    }

    /* determine if a game is active */
    int is_game_running() const { return game_started_ && !game_over_; }

    /* handle a keystroke */
    int do_char(TCHAR c, long keydata);
    int do_keydown(int virtual_key, long keydata);

    /* handle a system key */
    int do_syschar(TCHAR c, long keydata);

    /* set the current accelerator table based on the preference settings */
    void set_current_accel();

    /* is the given OS_CMD_xxx command enabled for process_command() use? */
    virtual int is_cmd_evt_enabled(int os_cmd_id) const;

    /* 
     *   Process a command.  We'll enter the command into the command buffer.
     *   
     *   If 'append' is true, we'll append the command to any existing text
     *   in the buffer, otherwise we'll clear the current command first.  If
     *   'enter' is true, we'll immediately return from get_input() or
     *   get_input_timeout(), as though the user had hit the Enter key;
     *   otherwise, we'll let the user continue editing the resulting
     *   command.  
     */
    void process_command(const textchar_t *cmd, size_t cmdlen,
                         int append, int enter, int os_cmd_id);

    /* update the display after changing the input buffer contents */
    virtual void update_input_display();

    /* update the caret position for scrolling or reformatting */
    virtual void update_caret_pos(int bring_on_screen, int update_sel_range);

    /* 
     *   Abort the current command line.  This is intended for use by the
     *   debugger, so that it can regain control without waiting for the
     *   user to finish a command. 
     */
    void abort_command();

    /* get my current accelerator */
    HACCEL get_accel() const { return current_accel_; }

    /* receive notification of a change to link enabling preferences */
    virtual void notify_link_pref_change();

    /*
     *   CTadsStatusSource implementation 
     */
    textchar_t *get_status_message(int *caller_deletes);

    /*
     *   IDropTarget implementation 
     */
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject __RPC_FAR *pDataObj,
                                        DWORD grfKeyState, POINTL pt,
                                        DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                       DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragLeave();
    HRESULT STDMETHODCALLTYPE Drop(IDataObject __RPC_FAR *pDataObj,
                                   DWORD grfKeyState, POINTL pt,
                                   DWORD __RPC_FAR *pdwEffect);

    /* determine if we're to show links at all */
    virtual int get_html_show_links() const;

protected:
    /* process an event in command editing mode */
    int process_input_edit_event(int event_type, os_event_info_t *event_info);

    /* 
     *   finish reading a line of text in get_input_timeout() - this is used
     *   to terminate input mode when get_input_timeout() ends normally, and
     *   in get_input_cancel() to terminate input mode from an interrupted
     *   input session
     */
    void get_input_done();

    /* notify before scrolling */
    virtual void notify_pre_scroll(HWND hwnd);

    /* notify after scrolling */
    virtual void notify_scroll(HWND hwnd, long oldpos, long newpos);

    /* get the starting position for a 'find' command */
    virtual void find_get_start(int start_at_top,
                                unsigned long *sel_start,
                                unsigned long *sel_end);

    /* receive system window creation notification */
    void do_create();

    /* receive system window destruction notification */
    void do_destroy();

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* check to see if we've started reading a command */
    virtual void check_cmd_start();

    /* handle timer messages */
    int do_timer(int timer_id);

    /*
     *   Determine if we're waiting for a keystroke or generic event.  If
     *   this returns true, then a keystroke should be returned as an
     *   event to the client, rather than processed normally. 
     */
    int is_waiting_for_key() const
    {
        /*
         *   Process a keystroke as an event if we're waiting for a
         *   keystroke event, or waiting for a generic event.  In any
         *   case, if we're in MORE mode, do NOT return the key to the
         *   client, since MORE mode always takes precedence over any
         *   other keystroke event waiter. 
         */
        return ((waiting_for_key_ || waiting_for_evt_) && !more_mode_);
    }

    /*
     *   Set the keystroke event for returning to the client code.  This
     *   stores the keystroke in the appropriate place for returning to
     *   the key waiter or generic event waiter.  'c' is the normal
     *   keystroke code, and 'extc' is the extended code; 'extc' is
     *   ignored unless 'c' is zero, in which case 'extc' should contain
     *   the CMD_xxx code for the extended key sequence.  
     */
    void set_key_event(int c, int extc)
    {
        /* flag the keystroke, if we're waiting for one */
        if (waiting_for_key_)
        {
            /* remember the key code */
            last_keystroke_ = (textchar_t)c;
            last_keystroke_cmd_ = extc;

            /* note that we got a key */
            got_keystroke_ = TRUE;
        }

        /* flag the event, if we're waiting for one */
        if (waiting_for_evt_)
        {
            /* fill in the event information for the extended key */
            if (last_evt_info_ != 0)
            {
                last_evt_info_->key[0] = c;
                last_evt_info_->key[1] = extc;
            }

            /* note that it's a keyboard event */
            last_evt_type_ = OS_EVT_KEY;

            /* note that we got an event */
            got_event_ = TRUE;
        }
    }

    /*
     *   Cancel the current input wait, if any.  If we're waiting for any
     *   kind of input, this will set the appropriate flag to indicate
     *   that we got the desired input event.  This can be used when a
     *   timeout or end-of-file event occurs to make sure that a nested
     *   event loop for pending input terminates immediately.  
     */
    void cancel_input_wait();

    /* 
     *   set the input buffer caret position for a given mouse position
     *   during a drag-drop operation with me as the target 
     */
    void set_drop_caret(POINTL pt);

    /*
     *   Drop target handlers for game chest.  Our main IDropTarget
     *   implementation entrypoints all call these as their first operation
     *   to check for a game chest drop; these return true if they handle
     *   the operation, false if it's not a game chest drop after all.  If
     *   these return false, we fall back on the regular drag/drop code.  
     */
    int DragEnter_game_chest(IDataObject __RPC_FAR *pDataObj,
                             DWORD grfKeyState, POINTL pt,
                             DWORD __RPC_FAR *pdwEffect);
    int DragOver_game_chest(DWORD grfKeyState, POINTL pt,
                            DWORD __RPC_FAR *pdwEffect);
    int DragLeave_game_chest();
    int Drop_game_chest(IDataObject __RPC_FAR *pDataObj,
                        DWORD grfKeyState, POINTL pt,
                        DWORD __RPC_FAR *pdwEffect);

    /* process a game chest file icon drop */
    void drop_gch_file(IDataObject __RPC_FAR *dataobj,
                       DWORD __RPC_FAR *effect);

    /* process a game chest URL drop */
    void drop_gch_url(IDataObject __RPC_FAR *dataobj,
                      DWORD __RPC_FAR *effect);

    /* current accelerator table - depends on preference settings */
    HACCEL current_accel_;

    /* accelerator tables for different preference settings */
    HACCEL accel_emacs_;
    HACCEL accel_win_;

    /* flag indicating when we've finished reading a command */
    int command_read_;

    /* flag indicating that we've read a keystroke */
    int got_keystroke_;

    /* flag indicating that we've read an event */
    int got_event_;

    /* last keystroke we read */
    textchar_t last_keystroke_;

    /* 
     *   last keystroke command code - this is used to store the CMD_xxx
     *   code when we have an extended key to return via the os_getc
     *   mechanism 
     */
    int last_keystroke_cmd_;

    /* flag indicating that we're waiting for a single keystroke */
    int waiting_for_key_ : 1;

    /* 
     *   flag indicating that we're waiting for a general input event
     *   (keystroke, link click) 
     */
    int waiting_for_evt_ : 1;

    /*
     *   Flag: end of file reached.  When this flag is set, any get_input,
     *   get_input_event, wait_for_keystroke, or similar method will
     *   return end of file. 
     */
    unsigned int eof_ : 1;

    /* game started/over */
    unsigned int game_started_ : 1;
    unsigned int game_over_ : 1;

    /* showing game chest page */
    unsigned int in_game_chest_ : 1;

    /*
     *   The os_get_input() information structure into which we're
     *   returning event information, if any.  If this pointer is
     *   non-null, we'll fill it in when a suitable event occurs.  
     */
    os_event_info_t *last_evt_info_;

    /* 
     *   the OS_EVT_xxx type code for the last event - when we set this,
     *   we'll fill in *last_evt_info_ with any additional data describing
     *   the event 
     */
    int last_evt_type_;

    /* "UniformResourceLocator" clipboard format ID */
    UINT url_clipboard_fmt_;

    /* 
     *   Interrupted input buffer contents.  When get_input_timeout() is
     *   interrupted by a timeout event, we keep track of the editing status
     *   of the input line that was under construction, so that we can later
     *   resume editing if get_input_timeout() is invoked again without
     *   having canceled the editing session.  
     */
    CStringBuf in_prog_buf_;

    /* subclass-specific status line strings */
    CStringBuf no_game_status_str_;
    CStringBuf game_over_status_str_;

    /* 
     *   selection range and caret position, in the CHtmlInputBuf object, of
     *   the interrupted input 
     */
    size_t in_prog_sel_start_;
    size_t in_prog_sel_end_;
    size_t in_prog_caret_;

    /* flag: input is in progress (with get_input_timeout()) */
    int input_in_progress_ : 1;

    /* 
     *   flag indicating that we timed out waiting for a keystroke or
     *   other event 
     */
    int waiting_for_evt_timeout_ : 1;

    /* ID of the system timer for input timeouts */
    int input_timer_id_;

    /* drag/drop flag: data source is valid */
    int drag_source_valid_ : 1;

    /* drag/drop flag: valid game chest desktop file icon drag/drop */
    int drag_gch_file_valid_ : 1;

    /* drag/drop flag: valid game chest URL drag/drop */
    int drag_gch_url_valid_ : 1;

    /* drag/drop caret object */
    class CTadsCaret *drop_caret_;

    /* drag/drop caret is currently visible */
    int drop_caret_vis_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Subclass of HTML panel window for pop-up menu emulation.  We use this
 *   type of window to handle os_show_popup_menu().
 */
class CHtmlSysWin_win32_Popup: public CHtmlSysWin_win32, public CTadsMsgFilter
{
public:
    CHtmlSysWin_win32_Popup(class CHtmlFormatter *formatter,
                            class CHtmlSysWin_win32_owner *owner,
                            int has_vscroll, int has_hscroll,
                            class CHtmlPreferences *prefs)
        : CHtmlSysWin_win32(formatter, owner,
                            has_vscroll, has_hscroll, prefs)
    {
        /* we haven't clicked a link yet */
        clicked_link_ = FALSE;
    }

    /* 
     *   Track the mouse in this window using pop-up menu semantics.  If the
     *   user clicks on a hyperlink in the window, we'll close the window and
     *   return the hyperlink event.  If the user clicks the mouse outside
     *   the window, we close the window and return false to indicate that
     *   the menu was canceled without a selection.  
     */
    int track_as_popup_menu(os_event_info_t *evt);

    /* filter a Windows message */
    int filter_system_message(MSG *msg);

protected:
    /* process left/right mouse buttons */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* 
     *   we have nothing to do on left-button up, as link clicks take effect
     *   immediately on left-button-down in this window 
     */
    int do_leftbtn_up(int keys, int x, int y) { return TRUE; }

    /* ignore right-button up, as we have no context menu to display */
    int do_rightbtn_up(int keys, int x, int y) { return TRUE; }

    /* handle mouse-move events */
    int do_mousemove(int keys, int x, int y);

    /* set the mouse cursor based on the given display item */
    virtual void set_disp_item_cursor(class CHtmlDisp *disp,
                                      int docx, int docy);

    /* finish tracking the mouse */
    virtual int end_mouse_tracking(html_track_mode_t mode);

    /* process a keyboard event */
    int do_keydown(int virtual_key, long keydata);

    /* 
     *   We don't ever want to take focus forcibly in this window, because we
     *   want our owner window to remain the active window as long as we're
     *   showing.  Menus on Windows conventionally leave focus and activation
     *   with their owner, and we want to act the same way.  
     */
    virtual void take_focus() { }

    /* check a mouse click to see if it should dismiss the menu window */
    int dismiss_on_click(int x, int y, DWORD flags);

private:
    /* flag: we've read an event that's dismissed the pop-up menu */
    int dismissed_;

    /* event structure where we'll put the HREF if we click on a link */
    os_event_info_t *evtp_;

    /* the previous message filter, so that we can restore it when done */
    CTadsMsgFilter *old_filter_;

    /* flag: we clicked on a hyperlink */
    int clicked_link_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   HTML Window Owner.  This is an abstract interface that provides
 *   certain operations that an HTML window needs to perform, but which
 *   exceed the scope of the HTML window itself.  The object that created
 *   the HTML window must provide this interface.  
 */

class CHtmlSysWin_win32_owner
{
public:
    /* get my window group object, if there is one */
    virtual CHtmlSysWinGroup *get_owner_win_group() { return 0; }

    /*
     *   Close the owner window.  If 'force' is true, we won't ask any
     *   questions; otherwise, we'll use the normal closing mechanism.  
     */
    virtual int close_owner_window(int force) = 0;

    /* 
     *   Bring the owner window to the front.  Since the HTML windows are
     *   intended to exist as child windows, this method brings the owner
     *   window itself to the front.  
     */
    virtual void bring_owner_to_front() = 0;

    /*
     *   Get the frame window handle.  This should return the handle of
     *   the top-level system window that contains windows owned by this
     *   owner. 
     */
    virtual HWND get_owner_frame_handle() const = 0;
    
    /* receive notification that a banner's size has been changed */
    virtual void on_set_banner_size(CHtmlSysWin_win32 *subwin) = 0;

    /* reformat all HTML windows */
    virtual void reformat_all_html(int show_status, int freeze_display,
                                   int reset_sounds) = 0;

    /* invalidate on-screen links (due to a link-display preference change) */
    virtual void owner_inval_links_on_screen() = 0;

    /* set the accelerator key for Paste */
    virtual void set_paste_accel(textchar_t paste_key) = 0;

    /* process a command */
    virtual void process_command(const textchar_t *cmd, size_t cmdlen,
                                 int append, int enter, int os_cmd_id) = 0;

    /* 
     *   Go forward/back a page.  If the container supports keeping a page
     *   list, these should switch to the next or previous page in the
     *   list.  The go_xxx_page routines actually switch to the new page;
     *   the exists_xxx_page routines simply return an indication as to
     *   whether we can go any further forward or backwards.  
     */
    virtual void go_next_page() = 0;
    virtual void go_previous_page() = 0;
    virtual int exists_next_page() = 0;
    virtual int exists_previous_page() = 0;

    /* Go the active page.  Has no effect if we're already there. */
    virtual void go_to_active_page() = 0;

    /*
     *   Get the active page window.  If the container supports keeping a
     *   page list, this returns the window object for the active window.
     *   Returns zero if there is no page list in the container.  
     */
    virtual CHtmlSysWin_win32 *get_active_page_win() { return 0; }
    
    /*
     *   Determine if we're on an active page.  When we have a command
     *   input page, only the last page should be active, since the
     *   previous pages can no longer accept input once a new page has
     *   been started. 
     */
    virtual int is_active_page() const = 0;

    /* 
     *   clear the selection range in all subwindows, optionally excluding
     *   a particular subwindow 
     */
    virtual void clear_other_sel(class CHtmlSysWin_win32 *exclude_win) = 0;

    /* 
     *   Move focus to the main HTML panel.  This should be called after
     *   any click in a panel window, or after any command that changes
     *   the selection window, to make sure that we don't leave focus in a
     *   window that doesn't accept input.  
     */
    virtual void focus_to_main() { }

    /*
     *   clear any hover information (such as the link the mouse is
     *   currently over) from all subwindows, optionally excluding a
     *   particular subwindow 
     */
    virtual void clear_other_hover_info(class CHtmlSysWin_win32 *exclude) = 0;

    /*
     *   Get the Direct Sound interface object.  We need one master
     *   IDirectSound object for the entire application; the owner is
     *   responsible for creating it and giving windows access to it via
     *   this method. 
     */
    virtual struct IDirectSound *get_directsound() = 0;

    /*
     *   Show a dialog explaining why DirectSound initialization failed.
     *   We'll display this if the user attempts to re-enable sounds
     *   explicitly (via a menu option).  Since the user may have disabled
     *   the warning dialog on startup, this will be the only way the user
     *   will be able to figure out why sound isn't working.  The owner is
     *   expected to remember the reason for the sound initialization
     *   failure and display an appropriate dialog here.  
     */
    virtual void owner_show_directx_warning() = 0;

    /*
     *   Get the default character set.  When a game sets an explicit
     *   internal character set, we'll choose an appropriate default
     *   character set that provides a good mapping for the game's
     *   internal character set.  For games that don't specify a character
     *   set, we'll use the default system ANSI code page. 
     */
    virtual oshtml_charset_id_t owner_get_default_charset() const
        { return oshtml_charset_id_t(ANSI_CHARSET, CP_ACP); }

    /*
     *   Get the name of the game's internal character set 
     */
    virtual const textchar_t *owner_get_game_internal_charset() const
        { return "Unknown"; }

    /* determine if the game is paused in the debugger */
    virtual int get_game_paused() const { return FALSE; }

    /* determine if the game has ended */
    virtual int get_game_over() const { return FALSE; }

    /* get the text for a find or find-next command */
    virtual const char *get_find_text(int command_id, int *exact_case,
                                      int *start_at_top, int *wrap, int *dir)
        { return 0; }

    /* reload the game chest data if necessary */
    virtual void owner_maybe_reload_game_chest() { }

    /* save the game chest database to the given file */
    virtual void owner_write_game_chest_db(const char *fname) { }

    /* show the Game Chest page, if we provide one */
    virtual void owner_show_game_chest(int refresh_only,
                                       int reload_file, int mark_dirty)
    {
        /* by default, we don't provide a game chest implementation */
    }

    /* process a Game Chest command */
    virtual void owner_process_game_chest_cmd(const textchar_t *, size_t)
    {
        /* by default, we don't provide a game chest implementation */
    }

    /* add a file to the game chest favorites list */
    virtual int owner_game_chest_add_fav(const textchar_t *, int group_id,
                                         int interactive)
    {
        return FALSE;
    }

    /* 
     *   add a directory (i.e., all of the game files within the directory
     *   and its subdirectories) to the game chest favorites list 
     */
    virtual int owner_game_chest_add_fav_dir(const textchar_t *)
    {
        return FALSE;
    }

    /* add a download dialog to the list of active downloads */
    virtual void owner_add_download(class CHtmlGameChestDownloadDlg *)
    {
        /* by default, ignore it */
    }

    /* remove a download dialog from the active list */
    virtual void owner_remove_download(class CHtmlGameChestDownloadDlg *)
    {
        /* by default, ignore it */
    }

    /* 
     *   Receive notification of a change to the caret position.  This is
     *   called when a caret is displayed and the position of the caret is
     *   changed.  
     */
    virtual void owner_update_caret_pos() { }

    /* set/set the temporary-show-links status */
    virtual int get_temp_show_links() const { return FALSE; }
    virtual void set_temp_show_links(int f) { }

    /*
     *   Manage the list of windows paused waiting for a response to a [MORE]
     *   prompt.  Any number of windows can be paused at a [MORE] prompt.
     *   When at least one window is paused, other windows should dispatch
     *   keyboard input events to the first paused window.  
     */
    virtual void add_more_prompt_win(class CHtmlSysWin_win32 *) { }
    virtual void remove_more_prompt_win(class CHtmlSysWin_win32 *) { }
    virtual int owner_process_moremode_key(int, TCHAR) { return FALSE; }
    virtual void release_all_moremode() { }
};


/*
 *   Empty owner implementation.  This can be used when a frame needs to
 *   create an HTML window but doesn't need to provide any meaningful
 *   implementation of the methods.  If a frame class needs to provide
 *   some but not all of the owner methods, it can inherit from this class
 *   and override those methods for which it wants to provide non-trivial
 *   implementations.  
 */
class CHtmlSysWin_win32_owner_null: public CHtmlSysWin_win32_owner
{
    virtual int close_owner_window(int /*force*/) { return FALSE; }
    virtual void bring_owner_to_front() { }
    virtual HWND get_owner_frame_handle() const { return 0; }
    virtual void on_set_banner_size(CHtmlSysWin_win32 *) { }
    virtual void reformat_all_html(int show_status, int freeze_display,
                                   int reset_sounds) { }
    virtual void set_paste_accel(textchar_t) { }
    virtual void owner_inval_links_on_screen() { }
    virtual void process_command(const textchar_t *, size_t, int, int, int)
        { }
    virtual void go_next_page() { }
    virtual void go_previous_page() { }
    virtual int exists_next_page() { return FALSE; }
    virtual int exists_previous_page() { return FALSE; }
    virtual void go_to_active_page() { }
    virtual int is_active_page() const { return TRUE; }
    virtual void clear_other_sel(class CHtmlSysWin_win32 *) { }
    virtual void clear_other_hover_info(class CHtmlSysWin_win32 *) { }
    virtual struct IDirectSound *get_directsound() { return 0; }
    virtual void owner_show_directx_warning() { }
};


/* ------------------------------------------------------------------------ */
/*
 *   Generic frame window class.  This is a port-specific base class for
 *   the frame window types (main window, debug log window).  It provides
 *   some simple functionality for implementing a top-level system window
 *   that contains HTML subwindows. 
 */
class CHtmlSys_framewin: public CTadsWin
{
public:
    /* get my preferences object */
    class CHtmlPreferences *get_prefs() const { return prefs_; }

protected:
    /* 
     *   remember my position in the preferences - this should be called
     *   any time the window is moved or resize
     */
    void set_winpos_prefs();

    /* 
     *   store a position in the appropriate preferences entry (override
     *   per subclass to write the correct key) 
     */
    virtual void set_winpos_prefs(const class CHtmlRect *pos) = 0;
    
    /* 
     *   Enter/exit a size or move operation.  We'll send the same event
     *   to all of our child windows. 
     */
    void do_entersizemove();
    void do_exitsizemove();

    /* move the window */
    void do_move(int x, int y);

    /* resize the window */
    void do_resize(int mode, int x, int y);

    /* MDI child activation */
    int do_childactivate();

protected:
    /* preferences - the subclass must set this to something meaningful */
    class CHtmlPreferences *prefs_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Main window class.  This is an entirely port-specific class; it
 *   doesn't have a portable interface.  This window contains one or more
 *   HTML window panels, each of which is represented by a
 *   CHtmlSysWin_win32 object.
 *   
 *   This object implements the CHtmlSysWin_win32_owner interface; this
 *   provides application container functions to HTML display subwindows.
 *   
 *   This object also implements CHtmlSysFrame, the application frame
 *   interface.  This provides main HTML window functions to the TADS
 *   runtime, and in particular is the glue that the HTML implementation
 *   of the TADS user input/display OS layer uses to operate the HTML
 *   display.  
 */

/* status line's control ID (used by windows in messages) */
static const int HTML_MAINWIN_ID_STATUS = 1;

class CHtmlSys_mainwin:
   public CHtmlSys_framewin, public CHtmlSysWin_win32_owner,
   public CHtmlSysFrame, public CHtmlSysWinGroup
{
public:
    CHtmlSys_mainwin(class CHtmlFormatterInput *formatter,
                     class CHtmlParser *parser, int in_debugger);
    ~CHtmlSys_mainwin();

    /*
     *   Get the main window.  We keep track of one static, global main
     *   window for the application.  Since the TADS runtime doesn't keep
     *   track of an active window, it can only direct its output to a
     *   window defined globally. 
     */
    static CHtmlSys_mainwin *get_main_win() { return main_win_; }

    /* load embedded resources from the .EXE file */
    void load_exe_resources(const char *exe_fname);

    /* 
     *   flag that we're starting a new game - clears any EOF flag in the
     *   main window, and clears old text 
     */
    void start_new_game();

    /* if a game is underway, ask the player if it's okay to end it */
    int query_end_game();

    /* flag that the current game has terminated */
    void end_current_game();

    /* receive notification that the TADS engine is loading a new game */
    void notify_load_game(const textchar_t *fname);

    /* 
     *   Change the active profile, remembering the setting as the
     *   game-specific profile for the current game.  In addition to saving
     *   the association between the current game and the given profile, we
     *   will update the active profile in the preferences, load the settings
     *   from the given profile, and update the display.  
     */
    void set_game_specific_profile(const char *profile);

    /* get the profile associated with the given game file */
    void get_game_specific_profile(char *profile_buf, size_t profile_buf_len,
                                   const char *filename, int must_exist);

    /* load the game-specific preferences profile */
    void load_game_specific_profile(const char *fullname);

    /* rename all game-specific references to a given profile */
    void rename_profile_refs(const char *old_name, const char *new_name);

    /* 
     *   Get/set the profile associated with the given file.  The 'set'
     *   routine doesn't change the current profile; it merely updates the
     *   game-to-profile association stored in the registry.  
     */
    int get_profile_assoc(const char *fname,
                          char *profile_buf, size_t profile_buf_len);
    void set_profile_assoc(const char *fname, const char *profile);

    /* 
     *   load a new game file - prompt for a game file; if provided,
     *   terminate the current game and start a new one 
     */
    void load_new_game();

    /* load a new game given the filename */
    void load_new_game(const textchar_t *fname);

    /*
     *   Set the game pause flag; when this flag is true, it means that
     *   the game is suspended in the debugger.  When the game is paused,
     *   we'll reject certain operations, such as menu commands that would
     *   enter a command into the game.  
     */
    void set_game_paused(int f) { game_paused_ = f; }
    virtual int get_game_paused() const { return game_paused_; }

    /* store a position in my preference settings */
    virtual void set_winpos_prefs(const class CHtmlRect *pos);

    /* get my parser */
    CHtmlParser *get_parser() { return parser_; }

    /* get my formatter */
    CHtmlFormatter *get_formatter()
    {
        /* return the main panel's formatter */
        return (main_panel_ != 0 ? main_panel_->get_formatter() : 0);
    }

    /* resize */
    void do_resize(int mode, int x, int y);

    /* exit a size/move operation */
    void do_exitsizemove();

    /* recalculate the banner layout */
    void recalc_banner_layout();

    /* process creation event */
    void do_create();

    /* process close-window event */
    int do_close();

    /* destroy the window */
    void do_destroy();

    /* handle activation */
    int do_activate(int flag, int minimized, HWND other_win);

    /* handle application activation */
    int do_activate_app(int flag, DWORD thread_id);

    /* handle control notifications */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* erase the background */
    int do_erase_bkg(HDC);

    /* paint the window contents */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* gain focus */
    void do_setfocus(HWND previous_focus);

    /* get our subwindow that contains input focus, if any */
    CHtmlSysWin_win32 *get_focus_subwin();

    /* process palette changes */
    int do_querynewpalette();
    void do_palettechanged(HWND initiating_window);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* process timer events */
    int do_timer(int timer_id);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* start a new page, and delete all previous pages */
    void clear_all_pages();

    /* delete all orphaned banner windows */
    void delete_orphaned_banners();

    /* view a transcript file */
    void view_script();

    /* display plain output, even if we're in HTML mode */
    void display_plain_output(const textchar_t *buf, int break_long_lines);

    /* add/remove a banner to our banner panel list */
    void add_banner_to_list(class CHtmlSysWin_win32 *win);
    void remove_banner_from_list(class CHtmlSysWin_win32 *win);

    /*
     *   wait for a keystroke with a timeout; returns an OS_EVT_xxx event
     *   code 
     */
    int wait_for_keystroke_timeout(textchar_t *ch, int pause_only,
                                   time_t timeout);

    /* 
     *   Pause after quitting a game.  If the preferences are set so that we
     *   keep our game window open after the game exits, we'll do nothing.
     *   If the preferences are set so that we pause for a key and then
     *   terminate, we'll pause for a key.  
     */
    void pause_after_game_quit();

    /* 
     *   Enable or disable the exit pause.  If the exit pause is disabled,
     *   there will be no exit pause even if the portable code calls
     *   os_expause().  
     */
    void enable_pause_for_exit(int enabled)
        { exit_pause_enabled_ = (enabled != 0); }

    /*
     *   Abort the current command.  This is intended mainly for use by the
     *   debugger so that it can regain control while the game is running.  
     */
    void abort_command();

    /*
     *   Show a DirectX warning.  If init is true, we'll suppress this
     *   warning if the user has previously told us not to show it again on
     *   startup.  
     */
    void show_directx_warning(int init, htmlw32_directx_err_t warn_type);

    /*
     *   Receive notification that a character mapping file has been loaded 
     */
    void advise_load_charmap(const textchar_t *ldesc,
                             const textchar_t *sysinfo);

    /* 
     *   display a debug message, bringing the debug log window to the
     *   foreground if 'foreground' is true 
     */
    void dbg_print(const char *msg, int foreground);

    /* print with printf-style formatting */
    void dbg_printf(int foreground, const char *msg, ...);

    /* print with printf-style formatting, using a specific font */
    void dbg_printf(const class CHtmlFontDesc *font, int foreground,
                    const char *msg, ...);

    /* get/set the debug window */
    class CHtmlSys_dbglogwin *get_debug_win() const { return dbgwin_; }
    void set_debug_win(class CHtmlSys_dbglogwin *dbgwin, int add_menu_item);

    /* 
     *   Add a file to the favorites list - returns true if they accept the
     *   dialog, false if not.  If 'interactive' is false, we'll skip the
     *   dialog and just add the item directly and return true.  
     */
    int game_chest_add_fav(const char *fname, int group_id, int interactive);

    /* add a directory's contents to the favorites list */
    int game_chest_add_fav_dir(const char *dir);

    /* check to see if Game Chest is present in this build */
    static int is_game_chest_present();

    /* initialize a popup menu that's about to be opened */
    virtual void init_menu_popup(HMENU menuhdl, unsigned int pos,
                                 int sysmenu);

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlSysFrame implementation 
     */

    /* create a banner */
    virtual class CHtmlSysWin
        *create_banner_window(class CHtmlSysWin *parent,
                              HTML_BannerWin_Type_t typ,
                              class CHtmlFormatter *formatter,
                              int where, class CHtmlSysWin *other,
                              HTML_BannerWin_Pos_t pos, unsigned long style);

    /* create an about box subwindow for the game's "About" box */
    virtual class CHtmlSysWin
        *create_aboutbox_window(class CHtmlFormatter *formatter);

    /* remove a banner subwindow */
    virtual void remove_banner_window(class CHtmlSysWin *subwin);

    /* orphan a banner window */
    virtual void orphan_banner_window(CHtmlFormatterBannerExt *fmt);

    /* look up an embedded resource in the executable */
    virtual int get_exe_resource(const textchar_t *resname, size_t resnamelen,
                                 textchar_t *fname_buf, size_t fname_buf_len,
                                 unsigned long *seek_pos,
                                 unsigned long *siz);

    /*
     *   Read a keyboard command.  Returns false if the application is
     *   quitting.
     */
    int get_input_timeout(textchar_t *buf, size_t bufsiz,
                          unsigned long timeout, int use_timeout);

    /*
     *   Cancel input that was started with get_input_timeout() and
     *   subsequently interrupted by a timeout. 
     */
    void get_input_cancel(int reset);

    /* get input from the keyboard */
    int get_input(textchar_t *buf, size_t bufsiz)
    {
        /* cancel any previously interrupted command input */
        get_input_cancel(TRUE);
        
        /* 
         *   Call our input-line-with-timeout method, but tell it not to use
         *   the timeout.  Return success (TRUE) if we get a LINE event,
         *   failure (FALSE) otherwise.
         */
        return (get_input_timeout(buf, bufsiz, 0, FALSE) == OS_EVT_LINE);
    }

    /*
     *   Read an event 
     */
    int get_input_event(unsigned long timeout_in_milliseconds,
                        int use_timeout, os_event_info_t *info);

    /* set non-stop mode */
    void set_nonstop_mode(int flag);

    /* check for a break key sequence */
    int check_break_key();

    /* flush buffered text to the parser, and optionally to the display */
    void flush_txtbuf(int fmt, int immediate_redraw);

    /* clear the screen and start a new page */
    void start_new_page();

    /* display output */
    void display_output(const textchar_t *buf, size_t len);
    void display_outputz(const textchar_t *buf)
        { display_output(buf, get_strlen(buf)); }

    /* 
     *   display output with quotes - this is useful for writing attribute
     *   values that might contain characters that are markup-significant 
     */
    void display_output_quoted(const textchar_t *buf, size_t len);
    void display_outputz_quoted(const textchar_t *buf)
        { display_output_quoted(buf, get_strlen(buf)); }

    /* 
     *   Wait for a keystroke from the main panel.  If pause_only is true,
     *   it means that we don't care about the keystroke but are simply
     *   waiting for a user-controlled pause; we'll display a status
     *   message in this case to let the user know we're waiting.  
     */
    textchar_t wait_for_keystroke(int pause_only);

    /* pause before exiting */
    void pause_for_exit();

    /* show the MORE prompt */
    void pause_for_more();

    /* display a message on the debug console */
    void dbg_print(const char *msg) { dbg_print(msg, TRUE); }

    /* ----------------------------------------------------------------- */
    /*
     *   CHtmlSysWinGroup implementation 
     */

    /* get the default character set */
    oshtml_charset_id_t get_default_win_charset() const
        { return default_charset_; }

    /* translate an HTML 4 code point value */
    virtual size_t xlat_html4_entity(textchar_t *result, size_t result_size,
                                     unsigned int charval,
                                     oshtml_charset_id_t *charset,
                                     int *changed_charset);

    /* get the window group dimensions */
    virtual void get_win_group_dimensions(unsigned long *width,
                                          unsigned long *height)
    {    
        RECT rc;

        /* get my client area */
        GetClientRect(handle_, &rc);
        
        /* return the parent window dimensons */
        *width = (unsigned long)(rc.right - rc.left);
        *height = (unsigned long)(rc.bottom - rc.top);
    }

    /* ----------------------------------------------------------------- */
    /*
     *   CHtmlSysWin_win32_owner implementation 
     */

    /* get my window group object (that would be me) */
    CHtmlSysWinGroup *get_owner_win_group() { return this; }

    /* close the owner window */
    int close_owner_window(int force);

    /* bring the window to the front */
    void bring_owner_to_front();

    /* get the frame handle */
    virtual HWND get_owner_frame_handle() const { return handle_; }

    /* set the height of a banner window */
    void on_set_banner_size(CHtmlSysWin_win32 *subwin);

    /* reformat all HTML windows */
    virtual void reformat_all_html(int show_status, int freeze_display,
                                   int reset_sounds);

    /* set the accelerator key for Paste */
    virtual void set_paste_accel(textchar_t paste_key);

    /* invalidate on-screen link displays */
    virtual void owner_inval_links_on_screen();

    /* process a command */
    virtual void process_command(const textchar_t *cmd, size_t cmdlen,
                                 int append, int enter, int os_cmd_id);

    /* switch pages */
    virtual void go_next_page();
    virtual void go_previous_page();
    virtual int exists_next_page();
    virtual int exists_previous_page();

    /* go to the active page */
    virtual void go_to_active_page();

    /* get the active page window */
    virtual CHtmlSysWin_win32 *get_active_page_win() { return main_panel_; }

    /* determine if this is the active page */
    virtual int is_active_page() const
    {
        /* 
         *   only the last page is active -- once we've started a new
         *   page, the previous page becomes permanently inactive 
         */
        return cur_page_state_ == last_page_state_;
    }

    /* clear the selection in all subwindows except one */
    virtual void clear_other_sel(class CHtmlSysWin_win32 *exclude_win);

    /* move focus to the main panel */
    virtual void focus_to_main();

    /* clear hover tracking info in all subwindows except one */
    virtual void clear_other_hover_info(class CHtmlSysWin_win32 *exclude);

    /* get the IDirectSound interface object */
    virtual struct IDirectSound *get_directsound();

    /* 
     *   show an appropriate warning explaining why directsound
     *   initialization failed previously 
     */
    virtual void owner_show_directx_warning()
        { show_directx_warning(FALSE, directsound_init_err_); }

    /*
     *   Set a new game to be loaded after the current game exits.  The
     *   main outer loop will check this just before terminating to find a
     *   new game to load. 
     */
    void set_pending_new_game(const char *filename)
    {
        /* store the new name */
        pending_new_game_.set(filename);

        /* remember that we have a game pending */
        is_new_game_pending_ = TRUE;
    }

    /* foget any pending game name */
    void clear_pending_new_game()
    {
        pending_new_game_.clear();
        is_new_game_pending_ = FALSE;
    }

    /* get the pending game */
    const char *get_pending_new_game() const
    {
        return (is_new_game_pending_ ? pending_new_game_.get() : 0);
    }

    /* 
     *   Wait for the user to select a new game or quit.  Returns true if
     *   the user selects (or has selected) a new game, false if the user
     *   closes the main game window.  'quitting' is true if we just
     *   finished another game, false if we have yet to load our first
     *   game.  
     */
    int wait_for_new_game(int quitting);

    /* get the text for a find or find-next command */
    const char *get_find_text(int command_id, int *exact_case,
                              int *start_at_top, int *wrap, int *dir);

    /* reload the game chest data if necessary */
    void owner_maybe_reload_game_chest();

    /* save the game chest database to the given file */
    virtual void owner_write_game_chest_db(const textchar_t *fname);

    /* show the game chest window */
    void owner_show_game_chest(int refresh_only,
                               int reload_file, int mark_dirty)
        { show_game_chest(refresh_only, reload_file, mark_dirty); }

    /* process a game chest command */
    void owner_process_game_chest_cmd(const textchar_t *cmd, size_t cmdlen)
        { process_game_chest_cmd(cmd, cmdlen); }

    /* add a directory's contents to the game chest favorites */
    virtual int owner_game_chest_add_fav_dir(const textchar_t *dir)
        { return game_chest_add_fav_dir(dir); }

    /* add a file to the game chest favorites list */
    virtual int owner_game_chest_add_fav(const textchar_t *fname,
                                         int group_id, int interactive)
        { return game_chest_add_fav(fname, group_id, interactive); }

    /* add a download dialog to the list of active downloads */
    virtual void owner_add_download(class CHtmlGameChestDownloadDlg *dlg)
        { add_download(dlg); }

    /* remove a download dialog from the active list */
    virtual void owner_remove_download(class CHtmlGameChestDownloadDlg *dlg)
        { remove_download(dlg); }

    /* get the currently loaded game's internal character set name */
    const textchar_t *owner_get_game_internal_charset() const
        { return game_internal_charset_.get(); }

    /* get the default character set */
    oshtml_charset_id_t owner_get_default_charset() const
        { return default_charset_; }

    /* set/set the temporary-show-links status */
    virtual int get_temp_show_links() const { return temp_show_links_; }
    virtual void set_temp_show_links(int f) { temp_show_links_ = f; }

    /* [MORE] prompt window handling */
    virtual void add_more_prompt_win(class CHtmlSysWin_win32 *win);
    virtual void remove_more_prompt_win(class CHtmlSysWin_win32 *win);
    int owner_process_moremode_key(int vkey, TCHAR ch);
    virtual void release_all_moremode();

private:
    /* finish command line input */
    void get_input_done();

    /* create our toolbar */
    void create_toolbar();

    /* adjust the layout of the status bar */
    void adjust_statusbar_layout();

    /* set up my recent game list in the File menu */
    void set_recent_game_menu();

    /* add a game as the most recent game */
    void add_recent_game(const textchar_t *new_game);

    /* load a game from the recent game list - 0 = most recent */
    void load_recent_game(int idx);

    /* 
     *   Prune the main window's parse tree, if we're using too much
     *   memory.  This should be called before getting user input; we'll
     *   check to see how much memory the parse tree is taking up, and cut
     *   it down a bit if it's too big.  
     */
    void maybe_prune_parse_tree();

    /*
     *   Determine how much memory we're using for parse tree text.  We'll
     *   add up the memory used by old pages in the navigation list, plus
     *   the memory used up in the active page. 
     */
    unsigned long get_parse_tree_mem() const;
    
    /* 
     *   Add a new page state structure, and switch to the new page.  This
     *   should be called when the game clears the window and starts
     *   showing a new page, so that we have a place to put this page's
     *   parser state.  
     */
    void add_new_page_state();

    /* 
     *   Get the current page state structure.  At any given time, we have
     *   a current page; this gets the state structure that should be used
     *   to save the current page's parse tree.  This can be used when the
     *   game clears the screen, so that we can save the parse tree that
     *   is being cleared away -- this lets the user go back and view old
     *   pages.  
     */
    class CHtmlParserState *get_cur_page_state();

    /* switch to a saved page */
    void go_to_page(class CHtmlSys_mainwin_state_link *new_page);

    /* show the Game Chest page */
    void show_game_chest(int refresh_only, int reload_file, int mark_dirty);

    /* offer the Game Chest page via a hyperlink in the current window */
    void offer_game_chest();

    /* look up a game chest entry by filename */
    class CHtmlGameChestEntry *look_up_fav_by_filename(
        const textchar_t *fname);

    /* look up a game chest title by filename */
    const textchar_t *look_up_fav_title_by_filename(const textchar_t *fname);

    /* process a Game Chest command */
    void process_game_chest_cmd(const char *cmd, size_t cmdlen);

    /* save the game chest file if it's been modified since the last save */
    void save_game_chest_file();

    /* unconditionally write the game chest database to the given file */
    void write_game_chest_file(const textchar_t *fname);

    /* read the game chest file */
    void read_game_chest_file();

    /* 
     *   read the game chest file if it's not already in memory and up to
     *   date with the disk copy 
     */
    void read_game_chest_file_if_needed();

    /* process application activation for the game chest */
    void do_activate_game_chest(int flag);

    /* delete the in-memory game chest information */
    void delete_game_chest_info();

    /* add a download to the active list */
    void add_download(class CHtmlGameChestDownloadDlg *dlg);

    /* remove a download from the active list */
    void remove_download(class CHtmlGameChestDownloadDlg *dlg);

    /* check before closing for active downloads */
    int do_close_check_downloads();

    /* 
     *   cancel any pending downloads and wait for the background threads to
     *   exit 
     */
    void cancel_downloads();

    /* get the game chest database file name */
    const textchar_t *get_game_chest_filename();

    /* get the game chest database file timestamp */
    void get_game_chest_timestamp(FILETIME *ft);

    /* note any change in the game chest background image file */
    int note_gc_bkg();

    /* load a menu with profile names */
    void load_menu_with_profiles(HMENU menuhdl);

    /* main HTML panel window */
    CHtmlSysWin_win32_Input *main_panel_;

    /* 
     *   HTML history panel window - this is used to show old pages that
     *   have been cleared off the screen.  We also need a separate parser
     *   and formatter for the history panel.  
     */
    CHtmlSysWin_win32 *hist_panel_;
    class CHtmlParser *hist_parser_;
    class CHtmlFormatterStandalone *hist_fmt_;

    /* offset of main panel from top of main client area */
    int main_panel_yoffset_;

    /* array of banner panels */
    class CHtmlSysWin_win32 **banners_;
    int banners_max_;
    int banner_cnt_;

    /* array of orphaned banners */
    class CHtmlFormatterBannerExt **orphaned_banners_;
    int orphaned_banners_max_;
    int orphaned_banner_cnt_;

    /* array of windows in MORE mode */
    CHArrayList more_wins_;

    /* flag: we're in the process of deleting orphaned banners */
    int deleting_orphaned_banners_;

    /* status line */
    class CTadsStatusline *statusline_;

    /* our parser */
    class CHtmlParser *parser_;

    /* text buffer for sending text to the parser */
    class CHtmlTextBuffer *txtbuf_;

    /* the main window for the application, tracked statically */
    static CHtmlSys_mainwin *main_win_;

    /* list of page state structures */
    CHtmlSys_mainwin_state_link *first_page_state_;
    CHtmlSys_mainwin_state_link *last_page_state_;

    /* current page's state structure link */
    CHtmlSys_mainwin_state_link *cur_page_state_;

    /* direct sound interface */
    struct IDirectSound *directsound_;

    /* error that caused DirectSound initialization to fail */
    htmlw32_directx_err_t directsound_init_err_;

    /* debug window */
    class CHtmlSys_dbglogwin *dbgwin_;

    /* the about box */
    class CHtmlSys_aboutgamewin *aboutbox_;

    /* the game's internal character set display name */
    CStringBuf game_internal_charset_;

    /* the new game waiting to be loaded after the current game exits */
    CStringBuf pending_new_game_;

    /* current game for profile purposes */
    CStringBuf profile_game_;

    /* flag: a new game is pending */
    int is_new_game_pending_;

    /* default character set */
    oshtml_charset_id_t default_charset_;

    /* 
     *   default "invalid" character in default character set - this is
     *   the character value that we'll display when we have an unmappable
     *   character (this is normally rendered as an open rectangle, to
     *   indicate a missing character) 
     */
    textchar_t invalid_char_val_;

    /* our toolbar control handle */
    HWND toolbar_;

    /* idle timer ID */
    int idle_timer_id_;

    /* starting time (for calculating the elapsed time display) */
    DWORD starting_ticks_;

    /* 
     *   starting time of the current timer pause - we use this to adjust
     *   starting_ticks_ when coming out of a pause to reflect the
     *   appropriate interval not counting the pause 
     */
    DWORD pause_starting_ticks_;

    /* the "Find" dialog object */
    class CTadsDialogFind *find_dlg_;

    /* text of current/last find */
    textchar_t find_text_[512];

    /* flag indicating whether exact case should be used in searches */
    int search_exact_case_;

    /* flag indicating whether the search wraps at the end of the file */
    int search_wrap_;

    /* search direction: 1 for forwards, -1 for backwards */
    int search_dir_;

    /* new searches start from top of document */
    int search_from_top_;

    /* menu containing the context menus */
    HMENU popup_container_;

    /* statusline popup menu */
    HMENU statusline_popup_;

    /* 
     *   profiles menu - this is valid only after the profile menu has been
     *   opened 
     */
    HMENU profile_menu_;

    /* name of executable file - for loading embedded resources */
    CStringBuf exe_fname_;

    /* hash table of executable-embedded HTML resources */
    class CHtmlHashTable *exe_res_table_;

    /* Game Chest information */
    class CHtmlGameChest *gch_info_;

    /* timestamp of Game Chest file */
    FILETIME gch_timestamp_;

    /* background image file */
    CStringBuf gch_bkg_;

    /* head of list of active downloads */
    class CHtmlGameChestDownloadDlg *download_head_;

    /* flag: pause_for_exit is enabled */
    unsigned int exit_pause_enabled_ : 1;

    /* flag: running under the debugger */
    unsigned int in_debugger_ : 1;

    /* flag: game is paused under the debugger */
    unsigned int game_paused_ : 1;

    /* 
     *   flag: we're running as a stand-alone executable, with the game
     *   file bundled into the application executable 
     */
    unsigned int standalone_exe_ : 1;

    /* flag: the timer is paused */
    unsigned int timer_paused_ : 1;

    /* 
     *   flag: go directly to game chest after game ends - this is set when
     *   the user selects the "go to game chest" command while a game is
     *   running, so that we can terminate the game, wait for it to exit,
     *   and then show the game chest screen 
     */
    unsigned int direct_to_game_chest_ : 1;

    /* 
     *   Flag: input is in progress.  We use this to keep track of
     *   line-input operations suspended by timeouts; since we can resume
     *   interrupted line-input operations, we need to know whether or not
     *   we have one pending at certain times.  
     */
    unsigned int input_in_progress_ : 1;

    /* 
     *   Flag indicating that we're temporarily showing links.  When the
     *   preferences are set so that we only show links when holding down a
     *   special key sequence, we use this flag to indicate that the special
     *   key sequence is currently active.  
     */
    int temp_show_links_ : 1;
};

/*
 *   Linked list entry for our list of saved parse tree state structures.
 *   We keep one of these entries per page that we've shown; each time the
 *   game clears the window, we create a new one to hold the contents of
 *   the new page's parse tree, so that we can go back and display the old
 *   page if the user asks us to.  
 */
class CHtmlSys_mainwin_state_link
{
public:
    CHtmlSys_mainwin_state_link(class CHtmlParserState *state)
    {
        state_ = state;
        next_ = prev_ = 0;
    }

    /* when we're deleted, we'll delete our state structure as well */
    ~CHtmlSys_mainwin_state_link();

    /* parse tree state structure for this page */
    class CHtmlParserState *state_;

    /* 
     *   Scrolling position in effect when we last visited this page --
     *   we'll restore this same scrolling position when we revisit the
     *   page. 
     */
    CHtmlPoint scrollpos_;

    /* next/previous items in our linked list */
    CHtmlSys_mainwin_state_link *next_;
    CHtmlSys_mainwin_state_link *prev_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Interface that must be provided by a window that can be an active
 *   debugger sub window.  Source and stack windows provide this interface.
 *   Active windows can have commands routed from the main window.  
 */
class CHtmlDbgSubWin
{
public:
    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(int command_id) = 0;

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id,
                                  HWND ctl) = 0;

    /* get my window handle */
    virtual HWND active_get_handle() = 0;

    /*
     *   Get the current selection, if appropriate.  Returns the true and
     *   fills in the pointers if a selection range was returned, false if
     *   not.  
     */
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Basic debug window class.  This is an entirely port-specific class.
 *   This window contains a single HTML window panel represented by a
 *   CHtmlSysWin_win32 object.  This class is subclassed for the debug log
 *   window and the debugger source file window.
 */

class CHtmlSys_dbgwin: public CHtmlSys_framewin,
    public CHtmlSysWin_win32_owner_null,
    public CHtmlDbgSubWin
{
public:
    CHtmlSys_dbgwin(class CHtmlParser *parser,
                    class CHtmlPreferences *prefs);
    ~CHtmlSys_dbgwin();

    /* initialize the HTML panel */
    virtual void init_html_panel(class CHtmlFormatter *formatter);

    /* 
     *   Turn on the sizebox.  By default, we'll turn on the sizebox in the
     *   underlying HTML panel.  
     */
    virtual void set_panel_has_sizebox();

    /* process creation event */
    void do_create();

    /* destroy the window */
    void do_destroy();

    /* resize the window */
    void do_resize(int mode, int x, int y);

    /* get the HTML panel window object */
    CHtmlSysWin_win32 *get_html_panel() const { return html_panel_; }

    /* close the owner window */
    int close_owner_window(int force);

    /* bring the window to the front */
    void bring_owner_to_front();

    /* get the frame handle */
    virtual HWND get_owner_frame_handle() const { return handle_; }

    /* gain focus */
    void do_setfocus(HWND previous_focus);

    /*
     *   Get the current selection, if appropriate.  Returns the true and
     *   fills in the pointers if a selection range was returned, false if
     *   not.  
     */
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end);

protected:
    /* 
     *   Load my menu.  This is virtual so that this window can be
     *   subclassed for other debugger windows (such as the source
     *   window).  
     */
    virtual void load_menu() { }

    /* initialize the parser - override as needed */
    virtual void init_parser() { }

    /* main HTML panel window */
    CHtmlSysWin_win32 *html_panel_;

    /* parser */
    class CHtmlParser *parser_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger host system interface for the debug log window.  This
 *   interface can be provided to the debug log window when the debugger
 *   is running, so that the debug log window can invoke certain services
 *   in the debugger.  
 */
class CHtmlSys_dbglogwin_hostifc
{
public:
    /* add a reference */
    virtual void dbghostifc_add_ref() = 0;

    /* remember a reference */
    virtual void dbghostifc_remove_ref() = 0;

    /* open a source file at a given line number */
    virtual void dbghostifc_open_source(const textchar_t *fname,
                                        long linenum, long colnum) = 0;

    /* receive notification of window activation change */
    virtual void dbghostifc_notify_active_dbg_win(
        class CHtmlSys_dbglogwin *win, int active) = 0;

    /* 
     *   Receive notification that the window is being destroyed.  The
     *   'subwin' parameter gives the HTML panel subwindow contained in
     *   our window.
     */
    virtual void dbghostifc_on_destroy(class CHtmlSys_dbglogwin *win,
                                       class CHtmlSysWin_win32 *subwin) = 0;

    /* get FIND text */
    virtual const textchar_t *dbghostifc_get_find_text(
        int command_id, int *exact_case, int *start_at_top,
        int *wrap, int *dir) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debug log window.  
 */

class CHtmlSys_dbglogwin: public CHtmlSys_dbgwin, public CHtmlSysWinGroup
{
public:
    CHtmlSys_dbglogwin(class CHtmlParser *parser,
                       class CHtmlPreferences *prefs,
                       CHtmlSys_dbglogwin_hostifc *debugger_ifc)
        : CHtmlSys_dbgwin(parser, prefs)
    {
        /* remember the debugger interface, and add a reference to it */
        debugger_ifc_ = debugger_ifc;
        if (debugger_ifc != 0)
            debugger_ifc->dbghostifc_add_ref();
    }

    ~CHtmlSys_dbglogwin();

    /* initialize the HTML panel */
    virtual void init_html_panel(class CHtmlFormatter *formatter);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* store a position in my preference settings */
    void set_winpos_prefs(const class CHtmlRect *pos);

    /* process close-window event */
    int do_close();

    /* process destruction */
    void do_destroy();

    /* display some text */
    void disp_text(const textchar_t *msg);

    /* display text with the given font */
    void disp_text(const class CHtmlFontDesc *font, const textchar_t *msg);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* process a notification */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* non-client-area activation notification */
    int do_ncactivate(int flag);

    /* load my menu */
    virtual void load_menu();

    /* initialize the parser */
    virtual void init_parser();

    /* get the text for a 'find' command */
    const textchar_t *get_find_text(int command_id, int *exact_case,
                                    int *start_at_top, int *wrap, int *dir);

    /* ----------------------------------------------------------------- */
    /*
     *   CHtmlSysWinGroup implementation 
     */

    /* get the default character set */
    oshtml_charset_id_t get_default_win_charset() const
        { return oshtml_charset_id_t(ANSI_CHARSET, CP_ACP); }

    /* translate an HTML 4 code point value */
    virtual size_t xlat_html4_entity(textchar_t *result, size_t result_size,
                                     unsigned int charval,
                                     oshtml_charset_id_t *charset,
                                     int *changed_charset)
    {
        /* 
         *   return the character unchanged - this window type doesn't
         *   provide real entity mapping, nor should it need to, since we
         *   only display normal text 
         */
        result[0] = (textchar_t)charval;
        result[1] = '\0';
        if (charset != 0)
        {
            charset->charset = ANSI_CHARSET;
            charset->codepage = CP_ACP;
            *changed_charset = FALSE;
        }
        return 1;
    }

    /* get the window group dimensions */
    virtual void get_win_group_dimensions(unsigned long *width,
                                          unsigned long *height)
    {
        RECT rc;
        
        /* get my client area */
        GetClientRect(handle_, &rc);

        /* return the parent window dimensons */
        *width = (unsigned long)(rc.right - rc.left);
        *height = (unsigned long)(rc.bottom - rc.top);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlDbgSubWin interface implementation 
     */

    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(int command_id);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id,
                                  HWND ctl);

    /* get my window handle */
    virtual HWND active_get_handle() { return handle_; }

    /* 
     *   get my style: if we're running in a stand-alone interpreter, we're
     *   the same kind of top-level window as the interpreter's main window;
     *   otherwise, we're a child window inside MDI or the docking mechanism 
     */
    DWORD get_winstyle()
    {
        /* our style varies according to how we're running */
        if (debugger_ifc_ != 0)
        {
            /* 
             *   we're in the debugger, which means we are a dockable window
             *   - make it a child 
             */
            return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        }
        else
        {
            /* we're in a normal interpreter - use the inherited style */
            return CHtmlSys_dbgwin::get_winstyle();
        }
    }

private:
    /* 
     *   service routine for disp_text variants - append text to our
     *   internal buffer and parse it, sending only complete lines to the
     *   parser 
     */
    void append_and_parse(const textchar_t *msg);

    /* receive notification of activation from parent */
    void do_parent_activate(int flag);

    /* text buffer for sending text to the parser */
    class CHtmlTextBuffer *txtbuf_;

    /* debugger host interface */
    class CHtmlSys_dbglogwin_hostifc *debugger_ifc_;
};

/*
 *   HTML panel subclass for debug log window 
 */
class CHtmlSysWin_win32_dbglog: public CHtmlSysWin_win32
{
public:
    CHtmlSysWin_win32_dbglog(class CHtmlFormatter *formatter,
                             class CHtmlSysWin_win32_owner *owner,
                             int has_vscroll, int has_hscroll,
                             class CHtmlPreferences *prefs,
                             class CHtmlSys_dbglogwin_hostifc *debugger_ifc);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* handle a mouse click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* 
     *   Set the body colors.  We override these to do nothing, since we want
     *   to use the color settings from the debugger options settings rather
     *   than the HTML body colors.  
     */
    virtual void set_html_bg_color(HTML_color_t, int) { }
    virtual void set_html_text_color(HTML_color_t, int) { }

protected:
    /* 
     *   if we have an error message line at the cursor, open the source
     *   file named in the error message and go to the line indicated 
     */
    void show_error_line();

    /* debugger host interface */
    class CHtmlSys_dbglogwin_hostifc *debugger_ifc_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Simple window class for the "about this game" dialog. 
 */

class CHtmlSys_aboutgamewin: public CTadsWin, CHtmlSysWin_win32_owner_null
{
public:
    CHtmlSys_aboutgamewin(class CHtmlPreferences *prefs)
    {
        html_subwin_ = 0;
        prefs_ = prefs;
    }

    /* process window creation */
    void do_create();

    /* process window deletion */
    void do_destroy();

    /* paint the contents of the window */
    void do_paint_content(HDC hdc, const RECT *paintrc);

    /* create the subwindow */
    void create_html_subwin(class CHtmlFormatter *formatter);

    /* delete the subwindow */
    void delete_html_subwin();

    /* get my HTML subwindow */
    class CHtmlSysWin_win32 *get_html_subwin() const { return html_subwin_; }

    /* run the "about" dialog */
    void run_aboutbox(class CTadsWin *owner);

    /* close the window */
    int do_close();

    /* resize */
    void do_resize(int mode, int x, int y);

    /* handle keystrokes */
    int do_char(TCHAR ch, long keydata);

    /* handle commands */
    int do_command(int notify_code, int cmd, HWND ctl);
    
protected:
    /* leave off the min and max boxes and the size box */
    DWORD get_winstyle()
    {
        return (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME
                | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

    /* make it a dialog frame */
    DWORD get_winstyle_ex() { return WS_EX_DLGMODALFRAME; }

    /* my HTML subwindow */
    class CHtmlSysWin_win32 *html_subwin_;

    /* handle of the "OK" button */
    HWND okbtn_;

    /* preferences object */
    class CHtmlPreferences *prefs_;

    /* flag indicating that the dialog has been dismissed */
    int closing_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Special window class for top-level windows that serve as HTML frames. 
 */
class CHtmlSys_top_win: public CTadsWin, public CHtmlSysWin_win32_owner_null
{
public:
    CHtmlSys_top_win(class CHtmlSys_mainwin *mainwin);
    ~CHtmlSys_top_win();

    /* process window creation */
    void do_create();

    /* process window deletion */
    void do_destroy();

    /* close the window */
    int do_close()
    {
        /* 
         *   if we're marked as ready to close for real, simply tell the
         *   system to proceed; otherwise, mark ourselves as closing so we
         *   return from our modal recursive event handler 
         */
        if (ending_)
        {
            /* we're finished - allow the system to close us for real */
            return TRUE;
        }
        else
        {
            /* note that we're closing */
            closing_ = TRUE;

            /* don't actually allow it to close yet */
            return FALSE;
        }
    }

    /* resize */
    void do_resize(int mode, int x, int y);

    /* get my HTML subwindow */
    class CHtmlSysWin_win32 *get_html_subwin() const { return html_subwin_; }

protected:
    /* create the HTML panel subwindow */
    virtual void create_html_subwin();

    /* adjust our client rectangle to the subwindow area */
    virtual void adjust_subwin_rect(RECT *rc)
    {
        /* 
         *   by default, we give the entire client area to the subwindow, so
         *   we don't need to make any adjustments here 
         */
    }

    /* fill in a text buffer with the contents of the dialog */
    virtual void build_contents(class CHtmlTextBuffer *txtbuf) = 0;

    /* does the HTML subpanel have a vertical/horizontal scrollbar? */
    virtual int panel_has_vscroll() const { return FALSE; }
    virtual int panel_has_hscroll() const { return FALSE; }

    /* parser and formatter for subwindow */
    class CHtmlParser *parser_;
    class CHtmlFormatter *formatter_;

    /* my HTML subwindow */
    class CHtmlSysWin_win32 *html_subwin_;

    /* main window that created us */
    class CHtmlSys_mainwin *mainwin_;

    /* flag indicating that the dialog has been dismissed */
    int closing_;

    /* flag indicating that we're done and ready to close on WM_CLOSE */
    int ending_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Simple window class for the "about HTML TADS" dialog.  
 */
class CHtmlSys_abouttadswin: public CHtmlSys_top_win
{
public:
    CHtmlSys_abouttadswin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_top_win(mainwin) { }
    
    /* 
     *   Run the modal dialog.  Creates the window, shows the window, runs a
     *   recursive event loop, then deletes 'this' when done.  The caller can
     *   simply 'new' us and then call this routine, all in one operation,
     *   without having to worry about cleaning up after us.  
     */
    void run_dlg(CTadsWin *parent);

    /* handle keystrokes */
    int do_char(TCHAR ch, long keydata);

    /* process a command */
    virtual void process_command(const textchar_t *cmd, size_t cmdlen,
                                 int append, int enter, int os_cmd_id);

protected:
    /* get our display width and height */
    virtual void get_dlg_size(int *wid, int *ht)
    {
        *wid = 478;
        *ht = 279;
    }

    /* get our window title */
    virtual const char *get_dlg_title() const { return "About HTML TADS"; }

    /* get the name of the background picture */
    virtual const char *get_bkg_name() const { return "about.jpg"; }

    /* fill in a text buffer with the contents of the dialog */
    virtual void build_contents(class CHtmlTextBuffer *txtbuf);

    /* show the "Credits" dialog */
    virtual void show_credits_dlg();

    /* does the HTML subpanel have a vertical/horizontal scrollbar? */
    virtual int panel_has_vscroll() const { return FALSE; }
    virtual int panel_has_hscroll() const { return FALSE; }

    /* leave off the min and max boxes and the size box */
    DWORD get_winstyle()
    {
        return (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU
                | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

    /* make it a dialog frame */
    DWORD get_winstyle_ex() { return WS_EX_DLGMODALFRAME; }
};

/* ------------------------------------------------------------------------ */
/*
 *   "Credits" dialog.  This a sub-dialog of the "About" dialog that shows
 *   detailed credits for the system.  This is implemented as a simple
 *   subclass of the "About" box dialog, since it operates essentially the
 *   same way but with different contents.  
 */
class CHtmlSys_creditswin: public CHtmlSys_abouttadswin
{
public:
    CHtmlSys_creditswin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_abouttadswin(mainwin)
    {
    }

protected:
    /* get the dialog size */
    virtual void get_dlg_size(int *wid, int *ht)
    {
        *wid = 450;
        *ht = 500;
    }

    /* get our window title */
    virtual const char *get_dlg_title() const
        { return "About HTML TADS"; }

    /* get the program title for the dialog's display text */
    virtual const char *get_prog_title() const
    {
        return "<font size=5><b>HTML TADS</b></font><br><br>"
            "A Multimedia TADS Interpreter<br>"
            "for Windows 95/98/Me/NT/2000/XP";
    }

    /* fill in a text buffer with the dialog contents */
    virtual void build_contents(class CHtmlTextBuffer *txtbuf);

    /* give the panel a vertical scrollbar */
    virtual int panel_has_vscroll() const { return TRUE; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Popup menu HTML window.  We use this for the os_show_popup_menu()
 *   implementation.  
 */
class CHtmlSys_popup_menu_win: public CHtmlSys_top_win
{
public:
    CHtmlSys_popup_menu_win(class CHtmlSys_mainwin *mainwin,
                            const char *txt, size_t txtlen)
        : CHtmlSys_top_win(mainwin)
    {
        /* remember my contents HTML text */
        txt_ = txt;
        txtlen_ = txtlen;
    }

    /* set our size and position */
    void set_pos_and_size(int default_pos, int x, int y);
    
    /* make this a plain popup window with no title bar */
    DWORD get_winstyle()
    {
        return (WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

    /* make it a top-most window */
    DWORD get_winstyle_ex()
    {
        return (WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    }

    /* handle mouse activation */
    int do_mouseactivate(HWND hwnd, int hit, unsigned int msg,
                         LRESULT *result)
    {
        /* 
         *   don't allow the window to be activated, but keep processing the
         *   click 
         */
        *result = MA_NOACTIVATE;
        return TRUE;
    }

    /* paint the contents of the window */
    void do_paint_content(HDC hdc, const RECT *paintrc);

protected:
    /* create the HTML panel subwindow */
    virtual void create_html_subwin();

    /* adjust our client rectangle to the subwindow area */
    virtual void adjust_subwin_rect(RECT *rc)
    {
        /* 
         *   we want to leave a one-pixel border in the main window, so
         *   adjust our subwindow accordingly 
         */
        rc->top += 1;
        rc->bottom -= 2;
        rc->left += 1;
        rc->right -= 2;
    }

    /* fill in a text buffer with the contents of the dialog */
    virtual void build_contents(class CHtmlTextBuffer *txtbuf);

    /* HTML contents text */
    const char *txt_;
    size_t txtlen_;
};


/* ------------------------------------------------------------------------ */
/*
 *   DirectX warning dialog 
 */

class CTadsDlg_dxwarn: public CTadsDialog
{
public:
    CTadsDlg_dxwarn(class CHtmlPreferences *prefs,
                    htmlw32_directx_err_t warn_type)
        : CTadsDialog()
    {
        prefs_ = prefs;
        warn_type_ = warn_type;
    }

    /* initialize the dialog */
    void init();

    /* process a command */
    int do_command(WPARAM id, HWND ctl);

private:
    /* preferences object */
    class CHtmlPreferences *prefs_;
    
    /* warning type */
    htmlw32_directx_err_t warn_type_;
};

#endif /* HTMLSYS_W32_H */

