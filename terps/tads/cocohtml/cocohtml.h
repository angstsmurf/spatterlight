/*
 * CocoTADS HTML header file.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os.h"
#include "html_os.h"
#include "tadshtml.h"
#include "htmlprs.h"
#include "htmlfmt.h"
#include "htmlsys.h"
#include "htmlrf.h"
//#include "htmlpng.h"
//#include "htmlmng.h"
//#include "htmljpeg.h"

struct CocoHtmlSysFrame : public CHtmlSysFrame, public CHtmlSysWinGroup
{
    CHtmlParser *parser;
    CHtmlFormatter *formatter;
    CHtmlTextBuffer *txtbuf;
    CHtmlResFinder *resfinder;

    class CocoHtmlSysWin *mainpanel;

    CocoHtmlSysFrame();

    // CHtmlSysFrame interface

    virtual CHtmlParser * get_parser();

    virtual void flush_txtbuf(int fmt, int immediate_redraw);
    virtual void start_new_page();
    virtual void display_output(const textchar_t *buf, size_t len);

    virtual int get_input_event(unsigned long timeout_in_milliseconds, int use_timeout, os_event_info_t *info);
    virtual int get_input_timeout(textchar_t *buf, size_t buflen, unsigned long timeout, int use_timeout);
    virtual int get_input(textchar_t *buf, size_t buflen);
    virtual void get_input_cancel(int reset);
    virtual textchar_t wait_for_keystroke(int pause_only);
    virtual void pause_for_exit();
    virtual void pause_for_more();
    virtual void set_nonstop_mode(int flag);
    virtual int check_break_key();

    virtual void dbg_print(const char *msg);

    // CHtmlSysWinGroup interface

    virtual oshtml_charset_id_t get_default_win_charset() const { return charset_UTF8; }
    virtual size_t xlat_html4_entity(textchar_t *result, size_t result_size,
			     unsigned int charval,
			     oshtml_charset_id_t *charset,
			     int *changed_charset);

    // hmm?

    void orphan_banner_window(class CHtmlFormatterBannerExt *banner);
    CHtmlSysWin * create_aboutbox_window(class CHtmlFormatter *formatter);
    void remove_banner_window(CHtmlSysWin *win);
    
    class CHtmlSysWin * create_banner_window(class CHtmlSysWin *parent,
					     HTML_BannerWin_Type_t window_type,
					     class CHtmlFormatter *formatter,
					     int where, class CHtmlSysWin *other,
					     HTML_BannerWin_Pos_t pos,
					     unsigned long style);
    
    int get_exe_resource(const textchar_t *resname, size_t resnamelen,
			 textchar_t *fname_buf, size_t fname_buf_len,
			 unsigned long *seek_pos,
			 unsigned long *siz);
    
    
    void get_input_done();
};

struct CocoHtmlSysWin : public CHtmlSysWin
{
    CHtmlSysWinGroup *wingroup;

    HTML_BannerWin_Pos_t pos;
    HTML_BannerWin_Units_t w_units;
    HTML_BannerWin_Units_t h_units;
    long width, height;
    unsigned long style;

    CocoHtmlSysWin(CHtmlSysWinGroup *wg, class CHtmlFormatter *formatter);
    ~CocoHtmlSysWin();

    virtual CHtmlSysWinGroup* get_win_group();
    virtual void notify_clear_contents();
    virtual int close_window(int);
    virtual long int get_disp_width();
    virtual long int get_disp_height();
    virtual long int get_pix_per_inch();
    virtual CHtmlPoint measure_text(CHtmlSysFont*, const textchar_t*, size_t, int*);
    virtual CHtmlPoint measure_dbgsrc_icon();
    virtual size_t get_max_chars_in_width(CHtmlSysFont*, const textchar_t*, size_t, long int);
    virtual void draw_text(int, long int, long int, CHtmlSysFont*, const textchar_t*, size_t);
    virtual void draw_text_space(int, long int, long int, CHtmlSysFont*, long int);
    virtual void draw_bullet(int, long int, long int, CHtmlSysFont*, HTML_SysWin_Bullet_t);
    virtual void draw_hrule(const CHtmlRect*, int);
    virtual void draw_table_border(const CHtmlRect*, int, int);
    virtual void draw_table_bkg(const CHtmlRect*, HTML_color_t);
    virtual void draw_dbgsrc_icon(const CHtmlRect*, unsigned int);
    virtual int do_formatting(int, int, int);
    virtual void recalc_palette();
    virtual int get_use_palette();
    virtual CHtmlSysFont* get_default_font();
    virtual CHtmlSysFont* get_font(const CHtmlFontDesc*);
    virtual CHtmlSysFont* get_bullet_font(CHtmlSysFont*);
    virtual void register_timer_func(void (*)(void*), void*);
    virtual void unregister_timer_func(void (*)(void*), void*);
    virtual CHtmlSysTimer* create_timer(void (*)(void*), void*);
    virtual void set_timer(CHtmlSysTimer*, long int, int);
    virtual void cancel_timer(CHtmlSysTimer*);
    virtual void delete_timer(CHtmlSysTimer*);
    virtual void fmt_adjust_hscroll();
    virtual void fmt_adjust_vscroll();
    virtual void inval_doc_coords(const CHtmlRect*);
    virtual void advise_clearing_disp_list();
    virtual void scroll_to_doc_coords(const CHtmlRect*);
    virtual void get_scroll_doc_coords(CHtmlRect*);
    virtual void set_window_title(const textchar_t*);
    virtual void set_html_bg_color(HTML_color_t, int);
    virtual void set_html_text_color(HTML_color_t, int);
    virtual void set_html_link_colors(HTML_color_t, int, HTML_color_t, int, HTML_color_t, int, HTML_color_t, int);
    virtual HTML_color_t map_system_color(HTML_color_t);
    virtual HTML_color_t get_html_link_color() const;
    virtual HTML_color_t get_html_alink_color() const;
    virtual HTML_color_t get_html_vlink_color() const;
    virtual HTML_color_t get_html_hlink_color() const;
    virtual int get_html_link_underline() const;
    virtual int get_html_show_links() const;
    virtual int get_html_show_graphics() const;
    virtual void set_html_bg_image(CHtmlResCacheObject*);
    virtual void inval_html_bg_image(unsigned int, unsigned int, unsigned int, unsigned int);
    virtual void set_banner_size(long int, HTML_BannerWin_Units_t, int, long int, HTML_BannerWin_Units_t, int);
    virtual void set_banner_info(HTML_BannerWin_Pos_t, long unsigned int);
    virtual void get_banner_info(HTML_BannerWin_Pos_t*, long unsigned int*);
};

struct CocoHtmlSysFont : public CHtmlSysFont
{
    CocoHtmlSysFont();
    virtual void get_font_metrics(CHtmlFontMetrics*);
    virtual int is_fixed_pitch();
    virtual int get_em_size();
};

