#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlfmt.cpp,v 1.4 1999/07/11 00:46:40 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlfmt.cpp - HTML formatter
Function
  
Notes
  
Modified
  09/07/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Display factory for unlinkable objects 
 */
class CHtmlFmtDispFactoryNormal: public CHtmlFmtDispFactory
{
public:
    CHtmlFmtDispFactoryNormal(CHtmlFormatter *formatter)
        : CHtmlFmtDispFactory(formatter)
    {
    }

    virtual CHtmlDisp *new_disp_text(class CHtmlSysWin *win,
                                     class CHtmlSysFont *font,
                                     const textchar_t *txt, size_t txtlen,
                                     unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispText(win, font, txt, txtlen, txtofs);
    }

    virtual CHtmlDispTextInput *new_disp_text_input(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new ((CHtmlFormatter *)0) CHtmlDispTextInput(
            win, font, txt, txtlen, txtofs);
    }

    virtual class CHtmlDisp *new_disp_soft_hyphen(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispSoftHyphen(win, font, txt, txtlen, txtofs);
    }

    virtual class CHtmlDisp *new_disp_nbsp(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispNBSP(win, font, txt, txtlen, txtofs);
    }

    virtual class CHtmlDisp *new_disp_space(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs, int wid)
    {
        return new (formatter_)
            CHtmlDispSpace(win, font, txt, txtlen, txtofs, wid);
    }

    virtual CHtmlDisp *new_disp_text_pre(class CHtmlSysWin *win,
                                         class CHtmlSysFont *font,
                                         const textchar_t *txt, size_t txtlen,
                                         unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispTextPre(win, font, txt, txtlen, txtofs);
    }

    virtual CHtmlDisp *new_disp_text_debug(class CHtmlSysWin *win,
                                           class CHtmlSysFont *font,
                                           const textchar_t *txt,
                                           size_t txtlen,
                                           unsigned long txtofs,
                                           unsigned long linenum)
    {
        return new (formatter_)
            CHtmlDispTextDebugsrc(win, font, txt, txtlen, txtofs, linenum);
    }

    virtual CHtmlDispImg *new_disp_img(class CHtmlSysWin *win,
                                       class CHtmlResCacheObject *image,
                                       CStringBuf *alt,
                                       HTML_Attrib_id_t align,
                                       long hspace, long vspace,
                                       long width, int width_set,
                                       long height, int height_set,
                                       long border, CHtmlUrl *usemap,
                                       int ismap)
    {
        return new (formatter_)
            CHtmlDispImg(win, image, alt, align, hspace, vspace,
                         width, width_set, height, height_set,
                         border, usemap, ismap);
    }
};

/*
 *   Display factory for unlinkable objects in character-wrap mode 
 */
class CHtmlFmtDispFactoryCharWrap: public CHtmlFmtDispFactoryNormal
{
public:
    CHtmlFmtDispFactoryCharWrap(class CHtmlFormatter *fmt)
        : CHtmlFmtDispFactoryNormal(fmt) { }

    /* create character-wrapping text items */
    virtual CHtmlDisp *new_disp_text(class CHtmlSysWin *win,
                                     class CHtmlSysFont *font,
                                     const textchar_t *txt, size_t txtlen,
                                     unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispTextCharWrap(win, font, txt, txtlen, txtofs);
    }

    virtual CHtmlDispTextInput *new_disp_text_input(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new ((CHtmlFormatter *)0) CHtmlDispTextInputCharWrap(
            win, font, txt, txtlen, txtofs);
    }
};

/*
 *   Display factor for linkable objects 
 */
class CHtmlFmtDispFactoryLink: public CHtmlFmtDispFactory
{
public:
    CHtmlFmtDispFactoryLink(CHtmlFormatter *formatter)
        : CHtmlFmtDispFactory(formatter)
    {
    }

    virtual CHtmlDisp *new_disp_text(class CHtmlSysWin *win,
                                     class CHtmlSysFont *font,
                                     const textchar_t *txt, size_t txtlen,
                                     unsigned long txtofs)
    {
        /* create the text item */
        return new (formatter_)
            CHtmlDispTextLink(win, font, txt, txtlen, txtofs,
                              formatter_->get_cur_link());
    }

    virtual class CHtmlDisp *new_disp_soft_hyphen(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispSoftHyphenLink(win, font, txt, txtlen, txtofs,
                                    formatter_->get_cur_link());
    }

    virtual class CHtmlDisp *new_disp_nbsp(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispNBSPLink(win, font, txt, txtlen, txtofs,
                              formatter_->get_cur_link());
    }

    virtual class CHtmlDisp *new_disp_space(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs, int wid)
    {
        return new (formatter_)
            CHtmlDispSpaceLink(win, font, txt, txtlen, txtofs, wid,
                               formatter_->get_cur_link());
    }

    virtual CHtmlDispTextInput *new_disp_text_input(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        /* input text can't be meaningfully linked; use a regular text item */
        return new ((CHtmlFormatter *)0) CHtmlDispTextInput(
            win, font, txt, txtlen, txtofs);
    }

    virtual CHtmlDisp *new_disp_text_pre(class CHtmlSysWin *win,
                                         class CHtmlSysFont *font,
                                         const textchar_t *txt, size_t txtlen,
                                         unsigned long txtofs)
    {
        /* create the text item */
        return new (formatter_)
            CHtmlDispTextPreLink(win, font, txt, txtlen, txtofs,
                                 formatter_->get_cur_link());
    }

    virtual CHtmlDisp *new_disp_text_debug(class CHtmlSysWin *win,
                                           class CHtmlSysFont *font,
                                           const textchar_t *txt,
                                           size_t txtlen,
                                           unsigned long txtofs,
                                           unsigned long linenum)
    {
        /* create the text item */
        return new (formatter_)
            CHtmlDispTextDebugsrc(win, font, txt, txtlen,
                                  txtofs, linenum);
    }

    virtual CHtmlDispImg *new_disp_img(class CHtmlSysWin *win,
                                       class CHtmlResCacheObject *image,
                                       CStringBuf *alt,
                                       HTML_Attrib_id_t align,
                                       long hspace, long vspace,
                                       long width, int width_set,
                                       long height, int height_set,
                                       long border, CHtmlUrl *usemap,
                                       int ismap)
    {
        /* create and return the image item */
        return new (formatter_)
            CHtmlDispImgLink(win, image, alt, align, hspace, vspace,
                             width, width_set, height, height_set,
                             border, usemap, ismap,
                             formatter_->get_cur_link());
    }
};

/*
 *   Display factory for linked objects in character-wrap mode 
 */
class CHtmlFmtDispFactoryLinkCharWrap: public CHtmlFmtDispFactoryLink
{
public:
    CHtmlFmtDispFactoryLinkCharWrap(class CHtmlFormatter *fmt)
        : CHtmlFmtDispFactoryLink(fmt) { }

    /* create character-wrapping text items */
    virtual CHtmlDisp *new_disp_text(class CHtmlSysWin *win,
                                     class CHtmlSysFont *font,
                                     const textchar_t *txt, size_t txtlen,
                                     unsigned long txtofs)
    {
        return new (formatter_)
            CHtmlDispTextLinkCharWrap(win, font, txt, txtlen, txtofs,
                                      formatter_->get_cur_link());
    }

    virtual CHtmlDispTextInput *new_disp_text_input(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs)
    {
        /* input text can't be meaningfully linked; use a regular text item */
        return new ((CHtmlFormatter *)0) CHtmlDispTextInputCharWrap(
            win, font, txt, txtlen, txtofs);
    }

};



/* ------------------------------------------------------------------------ */
/*
 *   formatter implementation
 */

/*
 *   statics 
 */
CHtmlResCache *CHtmlFormatter::res_cache_ = 0;
CHtmlResFinder *CHtmlFormatter::res_finder_ = 0;
int CHtmlFormatter::formatter_cnt_ = 0;
CHtmlSoundQueue *CHtmlFormatter::sound_queues_[4] = { 0, 0, 0, 0 };

/*
 *   construction 
 */
CHtmlFormatter::CHtmlFormatter(CHtmlParser *parser)
{
    /* count the new formatter */
    ++formatter_cnt_;

    /* if this is the first formatter, initialize the static singletons */
    if (formatter_cnt_ == 1)
    {
        /* create the global resource cache */
        res_cache_ = new CHtmlResCache();

        /* create the global resource finder */
        res_finder_ = new CHtmlResFinder();
    }

    /* save parser setting */
    parser_ = parser;

    /* window is as yet unknown */
    win_ = 0;

    /* no font yet */
    curfont_ = 0;

    /* no lines stored yet */
    line_count_ = 0;
    line_id_ = 0;

    /* allocate the line starts table */
    line_starts_ = new CHtmlLineStarts;

    /* there's no display list yet */
    disp_head_ = disp_tail_ = 0;

    /* there's no deferred floater list yet */
    defer_head_ = defer_tail_ = 0;

    /* no selection yet */
    sel_start_ = sel_end_ = 0;

    /* create the tab stop hash table */
    tab_table_ = new CHtmlHashTable(32, new CHtmlHashFuncCI);

    /* create the image map hash table */
    map_table_ = new CHtmlHashTable(128, new CHtmlHashFuncCI);
    cur_map_ = 0;

    /* use fairly large heap allocation units for our heap allocator */
    heap_alloc_unit_ = 16*1024;

    /* no heap pages yet */
    heap_pages_alloced_ = 0;
    heap_pages_max_ = 0;
    heap_pages_ = 0;
    heap_page_cur_ = 0;
    heap_page_cur_ofs_ = 0;

    /* not in a link yet */
    cur_link_ = 0;

    /* not in a DIV yet */
    cur_div_ = 0;

    /* set up the display item factories */
    disp_factory_normal_ = new CHtmlFmtDispFactoryNormal(this);
    disp_factory_charwrap_ = new CHtmlFmtDispFactoryCharWrap(this);
    disp_factory_link_ = new CHtmlFmtDispFactoryLink(this);
    disp_factory_link_charwrap_ = new CHtmlFmtDispFactoryLinkCharWrap(this);
    cur_disp_factory_ = disp_factory_normal_;

    /* start off in word-wrap mode */
    char_wrap_mode_ = FALSE;

    /* start at first sound generation */
    sound_gen_id_ = 1;

    /* no saved table environments yet */
    table_env_save_sp_ = 0;

    /* not yet freezing adjustments */
    freeze_display_adjust_ = FALSE;

    /* we haven't set any positions yet */
    disp_max_y_pos_ = 0;
    layout_max_y_pos_ = 0;
}

/*
 *   destruction 
 */
CHtmlFormatter::~CHtmlFormatter()
{
    size_t i;
    
    /* delete any display list */
    delete_display_list();

    /* delete line starts table */
    delete line_starts_;

    /* delete the tab stop hash table */
    delete tab_table_;

    /* delete the image map table */
    delete map_table_;

    /* if there's an image map under construction, delete it */
    if (cur_map_ != 0)
        delete cur_map_;

    /* delete the display item heap pages */
    for (i = 0 ; i < heap_pages_alloced_ ; ++i)
        th_free(heap_pages_[i].mem);

    /* delete the heap page array as well */
    if (heap_pages_ != 0)
        th_free(heap_pages_);

    /* delete the display item factories */
    delete disp_factory_normal_;
    delete disp_factory_charwrap_;
    delete disp_factory_link_;
    delete disp_factory_link_charwrap_;

    /* count the loss of a formatter */
    --formatter_cnt_;

    /* if there are no formatters left in the system, delete the statics */
    if (formatter_cnt_ == 0)
    {
        /* delete the global resource cache */
        delete res_cache_;

        /* delete the global resource finder */
        delete res_finder_;
    }

    /* delete the body display item if it's still around */
    if (body_disp_ != 0)
        delete body_disp_;
}

/*
 *   Unset the window.  This is called just before my window is about to be
 *   deleted.  
 */
void CHtmlFormatter::unset_win()
{
    CHtmlDisp *cur;

    /* if we've already unset our window, there's nothing to do here */
    if (win_ == 0)
        return;
    
    /* unset the window in our display list and defer list */
    for (cur = disp_head_ ; cur != 0 ; cur = cur->get_next_disp())
        cur->unset_win();
    for (cur = defer_head_ ; cur != 0 ; cur = cur->get_next_disp())
        cur->unset_win();

    /* unset the window in the special body display item */
    body_disp_->unset_win();

    /* forget our window */
    win_ = 0;

    /* delete the special "body" display item */
    delete body_disp_;
    body_disp_ = 0;
}

/*
 *   delete the display list 
 */
void CHtmlFormatter::delete_display_list()
{
    /* delete everything in the display list */
    CHtmlDisp::delete_list(disp_head_);

    /* delete everything in the deferred display list */
    CHtmlDisp::delete_list(defer_head_);

    /* clear out the division list */
    div_list_.clear();
    cur_div_ = 0;

    /* there's nothing in the lists now */
    disp_head_ = disp_tail_ = 0;
    defer_head_ = defer_tail_ = 0;

    /* forget any current line start we were keeping */
    line_head_ = 0;

    /* 
     *   Reset the display item pool - start at the first byte of the
     *   first page.  Note that we don't delete the old pages; we'll
     *   re-use any existing pages if we're reformatting.  
     */
    heap_page_cur_ = 0;
    heap_page_cur_ofs_ = 0;
}

/*
 *   Terminate playback of all active media in the display list.
 */
void CHtmlFormatter::cancel_playback()
{
    CHtmlDisp *cur;

    /* run through the display lists and cancel all playback */
    for (cur = disp_head_ ; cur != 0 ; cur = cur->get_next_disp())
        cur->cancel_playback();
    for (cur = defer_head_ ; cur != 0 ; cur = cur->get_next_disp())
        cur->cancel_playback();

    /* notify the body display item (if we still have one) */
    if (body_disp_ != 0)
        body_disp_->cancel_playback();
}

/*
 *   set the window that will be displaying our data 
 */
void CHtmlFormatter::set_win(class CHtmlSysWin *win,
                             const CHtmlRect *margins)
{
    /* remember the window */
    win_ = win;

    /* store the physical margins provided by the window */
    phys_margins_ = *margins;

    /* initialize everything by resetting to the top of the page */
    start_at_top(FALSE);

    /* 
     *   Create the special "body" pseudo-display item.  Don't actually add
     *   this to the display list; just create it and set it aside.  
     */
    body_disp_ = new (0) CHtmlDispBody(win_);
}

/*
 *   set the physical margins 
 */
void CHtmlFormatter::set_phys_margins(const CHtmlRect *margins)
{
    /* store the margins */
    phys_margins_ = *margins;

    /* we'll need to reformat everything */
    start_at_top(FALSE);
}

/*
 *   Clear the display list 
 */
void CHtmlFormatter::reset_disp_list()
{
    /* if there's no window, there's nothing to do */
    if (win_ == 0)
        return;

    /* advise the window that we're about to delete the display list */
    win_->advise_clearing_disp_list();

    /* delete any old display list */
    delete_display_list();

    /* no lines yet in line start list */
    line_starts_->clear_count();
    line_count_ = 0;
    line_id_ = 0;
}

/*
 *   Initialize or reset internal formatting settings.  This resets all
 *   internal state *except* the display list, which we leave intact; this is
 *   normally used to reset state during start_at_top().  
 */
void CHtmlFormatter::reset_formatter_state(int reset_sounds)
{
    /* not in title mode */
    title_mode_ = FALSE;

    /* if there's no window, there's nothing to do */
    if (win_ == 0)
        return;

    /* we're at the upper left corner of the document */
    curpos_.set(margin_left_, phys_margins_.top);
    disp_max_y_pos_ = 0;
    layout_max_y_pos_ = 0;

    /* make sure the window forgets any background image */
    win_->set_html_bg_image(0);

    /* allow formatting */
    stop_formatting_ = FALSE;

    /* at the beginning of a line */
    last_was_newline_ = TRUE;
    line_spacing_ = 0;

    /* we don't have anything in the line under construction yet */
    avail_line_width_ = 0;

    /* default to wrapping lines that are too wide to fit in the margins */
    wrap_lines_ = TRUE;

    /* we're not indented from the margins yet */
    margin_left_ = phys_margins_.left;
    margin_right_ = phys_margins_.right;
    temp_margins_ = FALSE;

    /* clear the margin stack */
    margin_stack_depth_ = 0;
    left_flow_stk_.reset();
    right_flow_stk_.reset();

    /* use default alignment */
    div_alignment_ = HTML_Attrib_invalid;
    blk_alignment_ = HTML_Attrib_invalid;

    /* ask the window for the default starting font */
    curfont_ = win_->get_default_font();

    /* default base font size is 3 */
    font_base_size_ = 3;

    /* nothing in the current line yet */
    line_head_ = 0;

    /* maximum line width is nil so far */
    max_line_width_ = 0;
    outer_max_line_width_ = 0;

    /* no pending tab yet */
    pending_tab_ = FALSE;
    pending_tab_disp_ = 0;

    /* clear out the tab table */
    tab_table_->delete_all_entries();

    /* not in a link yet */
    cur_link_ = 0;

    /* clear out the image map table */
    map_table_->delete_all_entries();

    /* get rid of any image map under construction */
    if (cur_map_ != 0)
    {
        delete cur_map_;
        cur_map_ = 0;
    }

    /* we haven't seen any ordered lists yet - start at sequence number 1 */
    ol_cont_seqnum_ = 1;

    /* no current or previous tag yet */
    curtag_ = prvtag_ = 0;

    /* no table yet */
    table_pass_ = 0;
    current_table_ = 0;

    /*
     *   If we're resetting sounds, clear all current sounds in all layers,
     *   and start a new generation of sounds.  Otherwise, keep the
     *   generation ID unchanged, so that we can tell that sounds we've
     *   already played during this generation should not be played again.  
     */
    if (reset_sounds)
    {
        /* clear current sounds in all layers */
        cancel_sound(HTML_Attrib_invalid);

        /* start a new sound generation  */
        ++sound_gen_id_;
    }

    /* start with the normal display item factory */
    cur_disp_factory_ = disp_factory_normal_;

    /* start off in word-wrap mode */
    char_wrap_mode_ = FALSE;

    /* no saved table environments yet */
    table_env_save_sp_ = 0;
}

/*
 *   reset the formatter to restart from the beginning of the document 
 */
void CHtmlFormatter::start_at_top(int reset_sounds)
{
    /* reset the formatting state */
    reset_formatter_state(reset_sounds);

    /* clear the display list */
    reset_disp_list();
}

/*
 *   Get the text array pointer 
 */
class CHtmlTextArray *CHtmlFormatter::get_text_array() const
{
    return parser_ != 0 ? parser_->get_text_array() : 0;
}

/*
 *   get the character at the given text offset 
 */
textchar_t CHtmlFormatter::get_char_at_ofs(unsigned long ofs) const
{
    /* ask the parser's text array for the character */
    return parser_->get_text_array()->get_char(ofs);
}

/*
 *   Get the number of characters between two offsets 
 */
unsigned long CHtmlFormatter::get_chars_in_ofs_range(
    unsigned long startofs, unsigned long endofs) const
{
    /* ask the parser's text array for the information */
    return parser_->get_text_array()->get_char_count(startofs, endofs);
}

/*
 *   Get a pointer to the text starting at the given offset 
 */
const textchar_t *CHtmlFormatter::get_text_ptr(unsigned long ofs) const
{
    /* get the text from the parser's text array */
    return parser_->get_text_array()->get_text(ofs);
}

/* increment a text offset */
unsigned long CHtmlFormatter::inc_text_ofs(unsigned long ofs) const
{
    return parser_->get_text_array()->inc_ofs(ofs);
}

/* decrement a text offset */
unsigned long CHtmlFormatter::dec_text_ofs(unsigned long ofs) const
{
    return parser_->get_text_array()->dec_ofs(ofs);
}

/*
 *   get the maximum text address assigned so far 
 */
unsigned long CHtmlFormatter::get_text_ofs_max() const
{
    return get_text_array()->get_max_addr();
}


/*
 *   Search for the given text 
 */
int CHtmlFormatter::search_for_text(const textchar_t *txt, size_t txtlen,
                                    int exact_case, int wrap, int dir,
                                    unsigned long startofs,
                                    unsigned long *match_start,
                                    unsigned long *match_end)
{
    /* ask the parser's text array to perform the search */
    return parser_->get_text_array()->search(txt, txtlen, exact_case,
                                             wrap, dir, startofs,
                                             match_start, match_end);
}

/* 
 *   Extract text from the text stream into the given buffer.  Pulls the
 *   text from the given starting text offset to the given ending text
 *   offset.  
 */
void CHtmlFormatter::extract_text(CStringBuf *buf,
                                  unsigned long start_ofs,
                                  unsigned long end_ofs) const
{
    CHtmlDisp *disp;

    /* traverse our display items and pull out their text values */
    for (disp = find_by_txtofs(start_ofs, FALSE, FALSE) ;
         disp != 0 ; disp = disp->get_next_disp())
    {
        /* if we're past the ending offset, we're done */
        if (disp->is_past_text_ofs(end_ofs))
            break;

        /* add this item's text to the buffer */
        disp->add_text_to_buf_clip(buf, start_ofs, end_ofs);
    }
}

/*
 *   Do some formatting and return.  We'll remember where we were, so the
 *   client can repeatedly call this routine to format an entire document.
 *   We return without formatting everything so that an interactive
 *   program can continue servicing user interface events while a long
 *   formatting operation is in progress.  
 */
void CHtmlFormatter::do_formatting()
{
    /* 
     *   make sure we have a non-container as the trailing tag, so that we
     *   can properly process exits 
     */
    parser_->fix_trailing_cont();

    /*
     *   Keep going until we've built a line to display, or we run out of
     *   tags, whichever comes first.  Do not stop at the end of a line if
     *   the line is within a table, since we need to format each table as
     *   a unit.  
     */
    for (line_output_ = FALSE ;
         curtag_ != 0 && curtag_->ready_to_format() && !stop_formatting_
             && (!line_output_ || table_pass_ != 0) ; )
    {
        /* format the current tag */
        curtag_->format(win_, this);

        /* remember the last tag we formatted */
        prvtag_ = curtag_;

        /* move on to the next tag in format order */
        curtag_ = curtag_->get_next_fmt_tag(this);
    }

    /*
     *   If we just did the last tag, figure the positions for items in
     *   the partially-filled last line, since we'd normally figure line
     *   positions at the next line break.  
     */
    if (curtag_ == 0 || stop_formatting_)
    {
        /* put the last line's items temporarily on a new line */
        ++line_id_;

        /* adjust positions of items in final line */
        set_line_positions(line_head_, 0);

        /* 
         *   in case we re-do the last line, do it at the same line number
         *   as we just did 
         */
        --line_id_;
    }
}

/*
 *   Figure out if I have more work to do.
 */
int CHtmlFormatter::more_to_do()
{
    /* if we've halted formatting, there's nothing left to do */
    if (stop_formatting_)
        return FALSE;

    /* if the current tag isn't ready to proceed, stop for now */
    if (curtag_ != 0 && !curtag_->ready_to_format())
        return FALSE;
    
    /* if we have a non-null current tag, there's definitely more to do */
    if (curtag_ != 0)
        return TRUE;

    /*
     *   There's no current tag, so we must have formatted everything we
     *   had in the parse tree when last we formatted anything; however,
     *   more parse tree may have since been built.  To find out, we can
     *   look to see if there's a tag following the last tag we parsed; if
     *   so, move to that new tag and proceed. 
     */
    if (prvtag_ != 0)
    {
        /* 
         *   make sure we have a non-container as the trailing tag, so that
         *   we can properly process exits 
         */
        parser_->fix_trailing_cont();

        /*
         *   Ask the previous tag that we formatted to check again to see
         *   if there's anything after it, and re-set the current tag to
         *   point to the item after the last tag.  If we added anything,
         *   this will find it. 
         */
        curtag_ = prvtag_->get_next_fmt_tag(this);

        /* if we have a non-null current tag after that, there's more to do */
        if (curtag_ != 0 && !stop_formatting_)
            return TRUE;
    }

    /* nothing left to do */
    return FALSE;
}

/*
 *   Add an item to the line starts table 
 */
void CHtmlFormatter::add_line_start(CHtmlDisp *item)
{
    /* 
     *   only add the item if we're not in multi-column mode, and the item
     *   is non-null 
     */
    if (item != 0 && !is_multi_col())
    {
        unsigned long item_ofs;

        /*
         *   The line starts table is indexed in ascending order of both y
         *   position and text offset.  The y position is taken care of
         *   automatically by our insistence that we only add line starts in
         *   single-column areas and our monotonic y positioning strategy,
         *   so we need only check the text offset.  If this item has a
         *   lower text offset than previous items, remove old items until
         *   this one is greater than the last item.  
         */
        item_ofs = item->get_text_ofs();
        for ( ; line_count_ != 0 ; --line_count_)
        {
            CHtmlDisp *cur;

            /* get this item */
            cur = line_starts_->get(line_count_ - 1);

            /* 
             *   If this one is ordered properly with respect to text
             *   offset, we can add the new one after it.  Note that, since
             *   we always make this check before adding an item, the entire
             *   existing list is always in order; so as soon as we find an
             *   item we're higher than, we know we're higher than all of
             *   the earlier items as well.  
             */
            if (cur->get_text_ofs() <= item_ofs)
                break;
        }

        /* add the item */
        line_starts_->add(line_count_, item, curpos_.y);
        ++line_count_;
    }

    /* update the line ID */
    ++line_id_;
}

/*
 *   Add an item to the display list.  This is an internal routine that
 *   simply links the item into the list without performing any formatting
 *   on it.  
 */
void CHtmlFormatter::add_to_disp_list(CHtmlDisp *item)
{
    CHtmlDisp *nxt;
    
    /*
     *   add it after the current end; or, if the list is empty, make it
     *   the head of the list 
     */
    if (disp_tail_)
        item->add_list_after(disp_tail_);
    else
        disp_head_ = item;

    /* move the display tail to the end of the list */
    for (disp_tail_ = item ; (nxt = disp_tail_->get_next_disp()) != 0 ;
         disp_tail_ = nxt) ;
}

/*
 *   Add an item to the end of the display list 
 */
void CHtmlFormatter::add_disp_item(CHtmlDisp *item)
{
    CHtmlRect pos;
    CHtmlDisp *new_line_head;

    /* if we're in title mode, simply ignore the item */
    if (title_mode_)
    {
        /* this item doesn't show up as actual text, so delete it */
        delete item;

        /* that's all we need to do with it */
        return;
    }

    /* assume the item isn't a line break */
    last_was_newline_ = FALSE;

    /* 
     *   for now, set the new item to the maximum y position, so it
     *   doesn't show up at the top of the screen if we should draw at an
     *   intermediate state before setting the final positions of all of
     *   the items 
     */
    pos = item->get_pos();
    pos.offset(0, layout_max_y_pos_ + 1000 - pos.top);
    item->set_pos(&pos);

    /* add it to the display list */
    add_to_disp_list(item);

    /*
     *   Add this item to the current line, and check for breaks.  Keep
     *   going as long as we find new line breaks, since we may break up
     *   this item into numerous lines. 
     */
    do
    {
        /*
         *   If we don't have anything in the current line yet, remember
         *   this item as the start of the current line.  
         */
        if (line_head_ == 0)
        {
            /* this is the first item in the line */
            line_head_ = item;

            /* reset the break position record */
            breakpos_.clear();
            
            /* the current margins will determine the width of this line */
            avail_line_width_ = get_space_between_margins();
        }

    refigure_line_break:
        /* 
         *   Try breaking the line if needed.  Note that we don't break
         *   lines if we're not in line-wrapping mode, and we also don't
         *   break lines if we're in the first pass through a table, since
         *   the format results of the first pass through a table are
         *   always discarded (so breaking lines would be wasted work).  
         */
        if (wrap_lines_ && table_pass_ != 1)
        {
            CHtmlDisp *cur;
            long wid;

            /*
             *   Scan the current item and all subsequent items until we
             *   find a line break.  We may have new items due to other
             *   line breaks we've added in past iterations on this item.  
             */
            for (wid = 0, cur = item ; cur != 0 ; cur = cur->get_next_disp())
            {
                /* scan the current item for a line break */
                new_line_head = cur->find_line_break(
                    this, win_, avail_line_width_ - wid, &breakpos_);

                /* if we found a break, we can stop looking */
                if (new_line_head != 0)
                    break;

                /* add this item's width into our running width */
                wid += cur->measure_width(win_);
            }
        }
        else
            new_line_head = 0;

        /* 
         *   if this item indicates that a line break should precede it,
         *   and it's already at the start of the line, ignore its desire
         *   to place a line break here 
         */
        if (new_line_head == item && item == line_head_)
            new_line_head = 0;

        /*
         *   Go through the items that will remain on this line and make
         *   sure they fit.  If they don't, try breaking free of marginal
         *   material to make them fit, to ensure that we don't overlap
         *   material in the right margin.  This exercise is pointless if
         *   we don't have any marginal material, since there's nothing to
         *   break clear of, so skip it in that case.  
         */
        if (left_flow_stk_.can_end_top_item(current_table_)
            || right_flow_stk_.can_end_top_item(current_table_))
        {
            int changed_margins;
            int reached_line_head;
            CHtmlDisp *prv;

            /*
             *   Check to see if the new line head is *prior* to the
             *   current item.  If this is the case, we went back and
             *   found an earlier break, in which case everything should
             *   definitely fit, and we don't need to worry about clearing
             *   the margins.  
             */
            for (prv = new_line_head ; prv != 0 && prv != item ;
                 prv = prv->get_next_disp()) ;
            if (prv != item)
            {
                /* haven't yet changed any margins */
                changed_margins = FALSE;
                
                /* 
                 *   Go through the items left on this line, and make sure
                 *   they all fit.  
                 */
                for (reached_line_head = FALSE ; item != 0 ; )
                {
                    long wid;
                    
                    /* get this item's width */
                    wid = item->measure_width(win_);
                    
                    /* 
                     *   note if we've reached the line break we
                     *   previously found 
                     */
                    if (item == new_line_head)
                        reached_line_head = TRUE;
                    
                    /* 
                     *   If it doesn't fit, the line is too long, and we
                     *   couldn't break it anywhere; in this case, try
                     *   breaking clear of any marginal material so that
                     *   we don't overlap anything in the right margin.  
                     */
                    if (wid > avail_line_width_)
                    {
                        /*
                         *   If we've already changed margins, try
                         *   re-breaking the line without making any
                         *   further margin changes 
                         */
                        if (changed_margins)
                            goto refigure_line_break;
                        
                        /* 
                         *   if we were already going to break the line by
                         *   this point, there's no need to clear any more
                         *   space - everything up to this point fits 
                         */
                        if (reached_line_head)
                        {
                            new_line_head = item;
                            break;
                        }
                        
                        /*
                         *   If we've already determined that this is to
                         *   be the last item on the line, check its size
                         *   minus trailing whitespace and see if it fits
                         *   then.  
                         */
                        if (item->get_next_disp() == new_line_head
                            && (wid - item->measure_trailing_ws_width(win_)
                                <= avail_line_width_))
                        {
                            /* 
                             *   it actually does fit - adjust the width
                             *   accordingly and keep going 
                             */
                            wid -= item->measure_trailing_ws_width(win_);
                        }
                        else
                        {
                            /* see if we can free up any space */
                            if (try_clearing_margins(wid))
                                changed_margins = TRUE;
                        
                            /* 
                             *   if this item still doesn't fit, try
                             *   breaking the line again 
                             */
                            if (wid > avail_line_width_)
                                goto refigure_line_break;
                        }
                    }
                    
                    /* 
                     *   deduct this one's width from the available line
                     *   width, and add it to the current position 
                     */
                    avail_line_width_ -= wid;
                    curpos_.x += wid;

                    /*
                     *   Move on to the next item 
                     */
                    item = item->get_next_disp();
                    if (item == 0)
                        break;

                    /*
                     *   If we don't have a new line head, scan this item
                     *   for a line break.  We don't care about finding an
                     *   actual line break; what we care about is making
                     *   sure that we've noted possible positions to break
                     *   the line when adding future items.  If we've
                     *   already found a line break don't bother with
                     *   this, since we'll scan from the start of the new
                     *   line when we format that line.  
                     */
                    if (new_line_head == 0)
                        item->find_line_break(this, win_, avail_line_width_,
                                              &breakpos_);
                }
            }
        }

        /* if there was a line break, process it */
        if (new_line_head != 0)
        {
            /* break the line */
            start_new_line(new_line_head);

            /* start the line breaking process again at the new line head */
            item = new_line_head;

            /*
             *   since we added the new item to the list, we need to make
             *   sure we have the correct tail pointer 
             */
            while (disp_tail_->get_next_disp() != 0)
                disp_tail_ = disp_tail_->get_next_disp();
        }
        else
        {
            /* 
             *   everything fits on this line - go through the items and
             *   deduct their space from the running total 
             */
            if (item != 0)
            {
                for (;;)
                {
                    long wid;

                    /* deduct this item's space from the total */
                    wid = item->measure_width(win_);
                    avail_line_width_ -= wid;
                    curpos_.x += wid;

                    /* move on to the next item, and stop if it's the last */
                    item = item->get_next_disp();
                    if (item == 0)
                        break;

                    /* 
                     *   Scan this item for a line break -- since we
                     *   already know it fits, we won't actually find a
                     *   line break, but we at least want to go through it
                     *   and find its last possible break position in case
                     *   the next item we add doesn't fit.  
                     */
                    item->find_line_break(this, win_, avail_line_width_,
                                          &breakpos_);
                }
            }
        }
    } while (new_line_head != 0);
}


/*
 *   Add a hypertext link display item 
 */
void CHtmlFormatter::add_disp_item_link(CHtmlDispLink *link)
{
    /* remember the current link */
    cur_link_ = link;

    /* adjust the display item factory now that we're in link mode */
    adjust_disp_factory();

    /* add the item to the display list as normal */
    add_disp_item(link);
}

/*
 *   End the current hypertext link 
 */
void CHtmlFormatter::end_link()
{
    /* forget the link */
    cur_link_ = 0;

    /* adjust the display item factory now that we're not in link mode */
    adjust_disp_factory();
}

/*
 *   Enter a <DIV> item 
 */
void CHtmlFormatter::begin_div(CHtmlDispDIV *disp)
{
    /* add the display item to our division list */
    div_list_.add_ele(disp);

    /* note this DIV's parent, and make this DIV current */
    disp->set_parent_div(cur_div_);
    cur_div_ = disp;

    /* add it to the main display list as well */
    add_to_disp_list(disp);
}

/*
 *   End the current <DIV> 
 */
void CHtmlFormatter::end_div()
{
    /* if we have a current DIV display item, close it up */
    if (cur_div_ != 0)
    {
        /* note the last item covered in the DIV range */
        cur_div_->set_div_tail(disp_tail_);

        /* pop the DIV stack */
        cur_div_ = cur_div_->get_parent_div();
    }
}

/*
 *   Adjust the display item factory for the current set of modes. 
 */
void CHtmlFormatter::adjust_disp_factory()
{
    /* 
     *   if we're within a link, use a linking factory; otherwise, use a
     *   normal non-linking factory 
     */
    if (cur_link_ != 0)
    {
        /* use the appropriate linking factory for the wrapping mode */
        cur_disp_factory_ = (char_wrap_mode_ ? disp_factory_link_charwrap_
                                             : disp_factory_link_);
    }
    else
    {
        /* use the appropriate non-linking factory for the wrapping mode */
        cur_disp_factory_ = (char_wrap_mode_ ? disp_factory_charwrap_
                                             : disp_factory_normal_);
    }
}


/*
 *   Hash table entry for a tab stop 
 */
class CHtmlFmtHashEntryTab: public CHtmlHashEntryCI
{
public:
    CHtmlFmtHashEntryTab(const textchar_t *str, size_t len,
                         HTML_Attrib_id_t align, textchar_t dp, long xpos)
        : CHtmlHashEntryCI(str, len, FALSE)
    {
        align_ = align;
        dp_ = dp;
        xpos_ = xpos;
    }

    /* alignment type */
    HTML_Attrib_id_t align_;

    /* decimal point (for ALIGN=DECIMAL) */
    textchar_t dp_;

    /* x position */
    long xpos_;
};

/*
 *   Define a tab stop 
 */
void CHtmlFormatter::set_tab(const textchar_t *tab_id, HTML_Attrib_id_t align,
                             textchar_t dp)
{
    CHtmlFmtHashEntryTab *entry;

    /* if no alignment was specified, assume left */
    if (align == HTML_Attrib_invalid)
        align = HTML_Attrib_left;

    /* if no DP was specified, use '.' */
    if (dp == 0)
        dp = '.';
    
    /* if we already have an entry for this tab ID, re-use it */
    entry = (CHtmlFmtHashEntryTab *)tab_table_->
            find(tab_id, get_strlen(tab_id));
    if (entry != 0)
    {
        /* already there - re-define this entry */
        entry->align_ = align;
        entry->dp_ = dp;
        entry->xpos_ = curpos_.x;
    }
    else
    {
        /* create a new entry and add it to the table */
        entry = new CHtmlFmtHashEntryTab(tab_id, get_strlen(tab_id),
                                         align, dp, curpos_.x);
        tab_table_->add(entry);
    }
}

/*
 *   Tab to a defined tab stop 
 */
void CHtmlFormatter::tab_to(CHtmlTagTAB *tag)
{
    CHtmlFmtHashEntryTab *entry;
    HTML_Attrib_id_t align;
    textchar_t dp;
    long tab_xpos;

    /* if there's an INDENT setting, use that */
    if (tag->use_indent_ || tag->use_multiple_)
    {
        long en_size;
        long spacing;

        /* 
         *   measure an 'n' in the current font (okay, it's not
         *   technically what the 'en' unit means, but it gives us a
         *   pretty good approximation) 
         */
        en_size = win_->measure_text(get_font(), "n", 1, 0).x;
        spacing = tag->indent_ * en_size;

        /* check for the MULTIPLE attribute */
        if (tag->use_multiple_)
        {
            long next_stop;

            /* 
             *   since we need to divide by the spacing, if the multiple
             *   is zero, force it to one en
             */
            if (spacing == 0)
                spacing = en_size;

            /* it's MULTIPLE - space to a multiple of the given spacing */
            next_stop = ((curpos_.x / spacing) + 1) * spacing;

            /* if that doesn't get us at least an en, use next stop */
            if (next_stop < curpos_.x + en_size)
                next_stop += spacing;

            /* figure the amount of space to get to this stop */
            spacing = next_stop - curpos_.x;
        }

        /* add the spacing */
        add_disp_item(new (this) CHtmlDispTab(&curpos_, spacing));

        /* all done */
        return;
    }

    /* close any pending tab */
    close_pending_tab();

    /* get the tag's alignment attributes */
    align = tag->align_;
    dp = tag->dp_;

    /* if there's no ID, treat it as a right-margin stop */
    if (tag->id_.get() == 0)
    {
        /* anonymous tab - see what kind of alignment we have */
        switch(align)
        {
        case HTML_Attrib_left:
            /* treat it as a space */
            add_disp_item(new (this) CHtmlDispText(win_, get_font(),
                " ", 1, tag->txtofs_));
            return;

        default:
            /* align text with respect to right margin */
            tab_xpos = win_->get_disp_width() - margin_right_;
            break;
        }
    }
    else
    {
        /* find the given ID */
        entry = (CHtmlFmtHashEntryTab *)tab_table_->
                find(tag->id_.get(), get_strlen(tag->id_.get()));
        
        /* see if we found it */
        if (entry == 0)
        {
            /* treat the tab as a single space */
            add_disp_item(new (this)CHtmlDispText(win_, get_font(),
                " ", 1, tag->txtofs_));

            /* there's nothing more to do */
            return;
        }

        /* get the position of the original tab */
        tab_xpos = entry->xpos_;

        /* use the original tab's alignment if not overridden */
        if (align == HTML_Attrib_invalid)
            align = entry->align_;

        /* use the original tab's DP if not overridden */
        if (dp == 0)
            dp = entry->dp_;
    }

    /* 
     *   Add a display item for the tab.  We'll set the width of this
     *   display item when we figure out how to align the material at the
     *   tab stop. 
     */
    pending_tab_disp_ = new (this) CHtmlDispTab(&curpos_);
    add_disp_item(pending_tab_disp_);

    /* check the alignment */
    switch(align)
    {
    case HTML_Attrib_left:
        /* add space up to the tab stop */
        if (curpos_.x < tab_xpos)
        {
            pending_tab_disp_->set_width(tab_xpos - curpos_.x);
            curpos_.x += tab_xpos - curpos_.x;
        }

        /* this tab is fully resolved */
        pending_tab_ = FALSE;
        pending_tab_disp_ = 0;
        break;

    case HTML_Attrib_center:
    case HTML_Attrib_right:
    case HTML_Attrib_decimal:
        /* 
         *   we can't determine the tab's spacing until the next tab or
         *   the end of the line 
         */
        pending_tab_ = TRUE;
        pending_tab_align_ = align;
        pending_tab_xpos_ = tab_xpos;
        pending_tab_dp_ = (dp == 0 ? '.' : dp);
        break;

    default:
        /* other cases should never occur; ignore them if they do */
        break;
    }
}

/*
 *   Close any pending tab.  For tabs aligned other than LEFT, we need to
 *   wait until we reach the next tab stop or the end of the line to set a
 *   tab; when one of these events happens, this routine must be called to
 *   finish setting the pending tab. 
 */
void CHtmlFormatter::close_pending_tab()
{
    long start_x;
    long text_wid;
    long open_space;
    long spacing;
    unsigned long ofs;
    CHtmlDisp *disp;

    /* if there's no pending tab, there's nothing to do */
    if (pending_tab_ == 0)
        return;

    /* note where the material after the tab started */
    start_x = pending_tab_disp_->get_pos().left;

    /* note the size of the material after the tab */
    text_wid = curpos_.x - start_x;

    /* note the amount of space between the tab and the starting point */
    open_space = pending_tab_xpos_ - start_x;

    /* 
     *   take appropriate action based on the alignment of the previous
     *   tab 
     */
    switch(pending_tab_align_)
    {
    case HTML_Attrib_center:
        /*
         *   Special case: if we're centering at an anonymous tab stop
         *   centered, center text between the start of the tab material
         *   and the right margin 
         */
        if (pending_tab_xpos_ == win_->get_disp_width() - margin_right_)
        {
            spacing = (pending_tab_xpos_ - start_x - text_wid) / 2;
            pending_tab_disp_->set_width(spacing);
            curpos_.x += spacing;
            break;
        }
        
        /*
         *   Expand the tab filler so that it fills up half the space left
         *   between here and the tab stop.  
         */
        spacing = open_space - text_wid/2;
        if (spacing > 0)
        {
            long max_spacing;
            
            /* 
             *   Check to make sure this doesn't push material past the
             *   right margin 
             */
            max_spacing = win_->get_disp_width() - margin_right_
                          - text_wid - start_x;
            if (spacing > max_spacing)
                spacing = max_spacing;

            /* apply the spacing if possible */
            if (spacing > 0)
            {
                pending_tab_disp_->set_width(spacing);
                curpos_.x += spacing;
            }
        }
        break;

    case HTML_Attrib_right:
    do_align_right:
        /* 
         *   Expand the tab filler so that it fills up the space left
         *   between here and the tab stop.  Leave it unexpanded if we're
         *   already past the tab stop.  
         */
        spacing = open_space - text_wid;
        if (spacing > 0)
        {
            pending_tab_disp_->set_width(spacing);
            curpos_.x += spacing;
        }
        break;

    case HTML_Attrib_decimal:
        /*
         *   Align material to the left of the decimal point character
         *   with flush right with the tab position.  If we can't find the
         *   decimal point, align the whole thing right.  
         */
        for (ofs = pending_tab_disp_->get_text_ofs() ;; )
        {
            unsigned long nxtofs;
            
            /* if this is the character we're looking for, we're done */
            if (get_char_at_ofs(ofs) == pending_tab_dp_)
            {
                /* move forward to include the decimal character */
                ofs = inc_text_ofs(ofs);

                /* no need to look further */
                break;
            }

            /* get the next character offset; stop if we're at the end */
            nxtofs = inc_text_ofs(ofs);
            if (nxtofs == ofs)
                break;

            /* move on to the next position */
            ofs = nxtofs;
        }

        /* measure the text from the start to this offset */
        for (text_wid = 0, disp = pending_tab_disp_ ; disp != 0 ;
             disp = disp->get_next_disp())
        {
            CHtmlRect pos;

            /* get the position of this item */
            pos = disp->get_pos();

            /* 
             *   if this is the item that contains the ending offset,
             *   measure only up to the ending offset and stop looking 
             */
            if (disp->contains_text_ofs(ofs))
            {
                CHtmlPoint pt;

                /* find out where the character is within the item */
                pt = disp->get_text_pos(win_, ofs);

                /* add the width to that point */
                text_wid += pt.x - pos.left;
                
                /* no need to look any further */
                break;
            }

            /* include the full width of this item */
            text_wid += pos.right - pos.left;
        }
        
        /* now align this amount of text flush right with the tab */
        goto do_align_right;

    default:
        /* other cases should never occur; ignore them if they do */
        break;
    }

    /* no more pending tab */
    pending_tab_ = FALSE;
    pending_tab_disp_ = 0;
}


/*
 *   Add an item to the tail of the display list, replacing the current
 *   command input display item(s), if any.  This is used during editing
 *   of a command line to update the display after the user has made a
 *   change in the contents of the command line.  The default
 *   implementation here is not input-capable, so we simply add the
 *   display item as though it were any other display item.  
 */
void CHtmlFormatter::add_disp_item_input(CHtmlDispTextInput *item,
                                         CHtmlTagTextInput *)
{
    add_disp_item(item);
}

/*
 *   Add a floating display item.  We don't add this type of item to the
 *   display list immediately, but put it in a list of items that will be
 *   added when we start the next line. 
 */
void CHtmlFormatter::add_disp_item_deferred(CHtmlDisp *item)
{
    /* if we're on pass 1 of a table, treat this as an ordinary item */
    if (table_pass_ == 1)
    {
        add_disp_item(item);
        return;
    }

    /* add the floating item to the deferred floating item list */
    if (defer_tail_ != 0)
        item->add_after(defer_tail_);
    else
        defer_head_ = item;
    defer_tail_ = item;

    /*
     *   If we haven't yet consumed any of the horizontal space of the
     *   current line, or we don't have anything on the current line yet,
     *   add the deferred item immediately.  
     */
    if (line_head_ == 0 || avail_line_width_ == get_space_between_margins())
    {
        add_all_deferred_items(0, 0, FALSE);
        line_head_ = 0;
    }
}

/*
 *   Add an item that was previously deferred to the display list.
 */
void CHtmlFormatter::deferred_item_ready(CHtmlDisp *disp)
{
    /* tell the item that it's being added */
    disp->format_deferred(win_, this, line_spacing_);
}

/*
 *   Add all of the items from the deferred floating item list to the
 *   display list. 
 */
void CHtmlFormatter::add_all_deferred_items(CHtmlDisp *prv_line_head,
                                            CHtmlDisp *nxt_line_head,
                                            int force)
{
    CHtmlDisp *again_head = 0;
    CHtmlDisp *again_tail = 0;

    /* if there are no deferred items, there's nothing to do */
    if (defer_head_ == 0)
        return;
    
    /*
     *   We want to make sure the deferred items are inserted at the start
     *   of the new line.  If the items in the next line have already been
     *   inserted into the display list, we need to temporarily remove
     *   them before inserting the deferred items, then re-insert those
     *   items. 
     */
    if (nxt_line_head != 0)
    {
        CHtmlDisp *prv;
        
        /* find the item preceding the head of the new line */
        for (prv = prv_line_head ;
             prv != 0 && prv->get_next_disp() != nxt_line_head ;
             prv = prv->get_next_disp()) ;

        /* make the end of the previous line the end of the list for now */
        if (prv != 0)
        {
            /* found it - remove it from the list */
            prv->clear_next_disp();
            disp_tail_ = prv;
        }
        else
        {
            /* 
             *   didn't find it - it's not there, so we can't remove it,
             *   so don't add it in later 
             */
            nxt_line_head = 0;
        }
    }
    
    /* keep going until we run out of items */
    while (defer_head_ != 0)
    {
        CHtmlDisp *cur;
        
        /* remember this item */
        cur = defer_head_;

        /* move on to the next item */
        defer_head_ = cur->get_next_disp();
        if (defer_head_ == 0) defer_tail_ = 0;

        /* unlink this item from the list */
        cur->clear_next_disp();

        /* 
         *   If this item fits or we don't have indented margins, add it; if
         *   not, we'll have to add it later when the margins get wider.  If
         *   we're forcing adding the deferred items, add it even if it
         *   doesn't fit.  
         */
        if (force
            || !is_multi_col()
            || cur->measure_width(win_) <= get_space_between_margins())
        {
            /* add this item to the display list */
            deferred_item_ready(cur);
        }
        else
        {
            /* 
             *   it doesn't fit - hang onto it, and put it back into the
             *   deferred item list for another try later (we'll just keep
             *   trying at the end of each line until we can find a place
             *   to put it) 
             */
            if (again_tail == 0)
                again_head = cur;
            else
                cur->add_after(again_tail);
            again_tail = cur;
        }
    }

    /*
     *   If we found any items that didn't fit, put them back into the
     *   deferred item list.  Note that we don't want to go through any of
     *   the processing that accompanied their first insertion into the
     *   deferred item list, since they were already in the list -- just
     *   put them directly into the list.  This is especially easy, since
     *   the list is entirely empty right now. 
     */
    defer_head_ = again_head;
    defer_tail_ = again_tail;

    /*
     *   Now that we've added the deferred items, add the items from the
     *   next line back onto the end of the display list. 
     */
    if (nxt_line_head != 0)
        add_to_disp_list(nxt_line_head);
}

/*
 *   Remove the last item added to the display list from the current line,
 *   and start a new line flowing around the last item.  The flow mode
 *   will continue until we reach the y position given as the bottom of
 *   this object. 
 */
void CHtmlFormatter::begin_flow_around(CHtmlDisp *item,
                                       long new_left_margin,
                                       long new_right_margin,
                                       long flow_bottom)
{
    CHtmlRect pos;

    /* 
     *   if we're not already in multi-column mode, and we have no lines
     *   yet, add this item to the line list so that we can find items
     *   within the multi-column area 
     */
    if (line_count_ == 0)
        add_line_start(item);

    /* 
     *   add this item to the display list, but don't perform any of the
     *   usual formatting on it 
     */
    add_to_disp_list(item);
    
    /* push the change onto the appropriate flow-around stack */
    if (new_left_margin != margin_left_)
        left_flow_stk_.push(margin_left_ - new_left_margin, flow_bottom,
                            current_table_);
    if (new_right_margin != margin_right_)
        right_flow_stk_.push(margin_right_ - new_right_margin, flow_bottom,
                             current_table_);

    /* set the new margins */
    set_margins(new_left_margin, new_right_margin);

    /* 
     *   if this item goes past the current maximum width of the document,
     *   notify the window that it will have to adjust the horizontal
     *   scrolling limits 
     */
    pos = item->get_pos();
    if (pos.right > outer_max_line_width_)
    {
        /* remember the outer-level max line width */
        outer_max_line_width_ = pos.right;

        /* adjust the window's horizontal scrollbar */
        win_->fmt_adjust_hscroll();
    }
}

/*
 *   Remove items from the display list starting with the given item.
 *   We'll work forward from the given line to find the item. 
 */
void CHtmlFormatter::remove_from_disp_list(long start_line,
                                           CHtmlDisp *item)
{
    CHtmlDisp *cur;
    
    /* start at the head of the given starting line */
    cur = line_starts_->get(start_line);

    /* try to find the item just before the given item */
    for ( ; cur != 0 && cur->get_next_disp() != item ;
          cur = cur->get_next_disp()) ;

    /* if we found it, remove this item */
    if (cur != 0)
    {
        /* clear the forward pointer of the previous item */
        cur->clear_next_disp();

        /* move the display list tail to the previous item */
        disp_tail_ = cur;
    }

    /* if this item was the head, forget it */
    if (disp_head_ == item)
        disp_head_ = disp_tail_ = 0;
}

/*
 *   Check flow-around items to see if we're past the end of them and can
 *   return to the enclosing margins. 
 */
int CHtmlFormatter::check_flow_end()
{
    int found_end;

    /* haven't found any ended items yet */
    found_end = FALSE;

    /* check the left flow stack */
    while (left_flow_stk_.is_past_end(curpos_.y, current_table_))
    {
        /* restore the margins in effect with this item */
        set_margins(margin_left_ + left_flow_stk_.get_top()->old_margin_delta,
                    margin_right_);

        /* pop this flow stack item */
        left_flow_stk_.pop();

        /* note that we found an ended item */
        found_end = TRUE;
    }

    /* check the right flow stack */
    while (right_flow_stk_.is_past_end(curpos_.y, current_table_))
    {
        /* restore the margins in effect with this item */
        set_margins(margin_left_,
                    margin_right_
                    + right_flow_stk_.get_top()->old_margin_delta);

        /* pop this flow stack item */
        right_flow_stk_.pop();

        /* note that we found an ended item */
        found_end = TRUE;
    }

    /* 
     *   return an indication as to whether or not we found any items that
     *   reached their end 
     */
    return found_end;
}


/*
 *   Compute the total amount of space available between the margins 
 */
long CHtmlFormatter::get_space_between_margins()
{
    /* 
     *   calculate the total space between the margins as the display
     *   width available in the window less the sum of the margin sizes 
     */
    return win_->get_disp_width() - (margin_left_ + margin_right_);
}

/*
 *   Add an item on a new line 
 */
void CHtmlFormatter::add_disp_item_new_line(CHtmlDisp *item)
{
    /* start a new line with this item */
    start_new_line(item);

    /* add the item to the display list */
    add_disp_item(item);

    /* note that we just started a new line */
    last_was_newline_ = TRUE;
}

/*
 *   Start a new line after the given item, making the item the last item
 *   in the current line.  
 */
void CHtmlFormatter::add_disp_item_new_line_after(CHtmlDisp *item)
{
    /* add this item to the display list */
    add_disp_item(item);

    /*
     *   Create an empty line break item and add it as the first item on
     *   the next line.  We'll make the vertical size of the new item zero
     *   so that it doesn't contribute to the size of the next line.  
     */
    add_disp_item_new_line(new (this) CHtmlDispBreak(0));

    /* note that we just started a new line */
    last_was_newline_ = TRUE;
}


/*
 *   Add minimum line spacing.  This makes sure that we're starting a new
 *   line, and that the line spacing for the new line is at least as much
 *   as requested.  If we're already on a new line and have provided the
 *   requested amount of inter-line spacing, this doesn't do anything.  
 */
void CHtmlFormatter::add_line_spacing(int spacing)
{
    /* if we're not at the beginning of a line, add a new line */
    if (!last_was_newline_)
        add_disp_item_new_line(new (this) CHtmlDispBreak(0));

    /* set the desired spacing to at least the requested amount */
    if (line_spacing_ < spacing)
        line_spacing_ = spacing;
}

/*
 *   Add vertical whitespace as needed to move past floating items in one
 *   or both margins.  Returns true if any line spacing was added.  
 */
int CHtmlFormatter::break_to_clear(int left, int right)
{
    int i;
    int y;
    int cur;

    /* presume we won't need to go anywhere */
    y = 0;

    /* get the bottom-most item in the left stack, if desired */
    if (left)
    {
        /* scan from the most deeply nested outward */
        for (i = left_flow_stk_.get_count() ; i != 0 ; )
        {
            /* move to the next item */
            --i;

            /* 
             *   if this item can't close due to table nesting, we can stop
             *   scanning - we must remove items in stack order, so once we
             *   reach an unclosable item, we can't close anything enclosing
             *   it, either 
             */
            if (!left_flow_stk_.can_end_item(i, current_table_))
                break;

            /* get this item's bottom */
            cur = left_flow_stk_.get_by_index(i)->bottom;

            /* if it's the bottom-most bottom so far, remember it */
            if (cur > y)
                y = cur;
        }
    }

    /* do the same for the right stack, if desired */
    if (right)
    {
        /* scan from most depely nested outward */
        for (i = right_flow_stk_.get_count() ; i != 0 ; )
        {
            /* move to the next item */
            --i;

            /* stop if we can't remove this item */
            if (!right_flow_stk_.can_end_item(i, current_table_))
                break;

            /* get this item's bottom */
            cur = right_flow_stk_.get_by_index(i)->bottom;

            /* if it's the bottom-most bottom so far, remember it */
            if (cur > y)
                y = cur;
        }
    }

    /* if necessary, insert enough space to move below this point */
    if (y + 1 > curpos_.y)
    {
        add_line_spacing(y - curpos_.y + 1);
        return TRUE;
    }

    /* we didn't add any spacing */
    return FALSE;
}

/*
 *   Try to add vertical whitespace until we free up the given amount of
 *   space, if we have any floating material in the margins.  If we don't
 *   have floating material, this routine has no effect; otherwise, it
 *   will add the minimum amount of whitespace necessary to have the given
 *   width free in the line.  Returns true if we made any changes, false
 *   if not.  Note that we return true if we adjust the margins, even if
 *   we don't free up the required amount of space.  
 */
int CHtmlFormatter::try_clearing_margins(long wid)
{
    int changed;

    /* presume we won't make any changes */
    changed = FALSE;
    
    /* 
     *   keep going until we run out of marginal material or free up
     *   enough space 
     */
    while ((left_flow_stk_.can_end_top_item(current_table_)
            || right_flow_stk_.can_end_top_item(current_table_))
           && avail_line_width_ < wid)
    {
        CHtmlRect area;
        long y, yleft, yright;
        
        /* see which currently endable item has less space left on it */
        yleft = (left_flow_stk_.can_end_top_item(current_table_)
                 ? left_flow_stk_.get_top()->bottom : 0);
        yright = (right_flow_stk_.can_end_top_item(current_table_)
                  ? right_flow_stk_.get_top()->bottom : 0);

        /* 
         *   use the one with less space remaining (or the one with an
         *   item if the other is empty) 
         */
        if (yleft == 0)
            y = yright;
        else if (yright == 0)
            y = yleft;
        else
            y = (yleft < yright ? yleft : yright);

        /* invalidate the entire vertical area we're skipping */
        area.set(0, curpos_.y, HTMLSYSWIN_MAX_RIGHT, y + 1);
        win_->inval_doc_coords(&area);

        /* advance our y position so we're just clear of this item */
        curpos_.y = y + 1;

        /* 
         *   Remove the flow items from the margins.  This will update
         *   avail_line_width_ and curpos_.x appropriately for the change,
         *   and will remove from the flow stacks any items that we have
         *   moved past.  
         */
        if (!check_flow_end())
            break;

        /* we've made a change */
        changed = TRUE;
    }

    /* return margin change indication */
    return changed;
}


/*
 *   Start a new line.  If a display list item is given, the item will be
 *   set as the first item on the new line; otherwise, everything in the
 *   list from the start of the last line will be on one line.  If a new
 *   line head isn't given, this routine doesn't actually break the line,
 *   but instead simply sets the positions for the items in the current
 *   line.  
 */
void CHtmlFormatter::start_new_line(CHtmlDisp *next_line_head)
{
    CHtmlDisp *old_line_head;
    CHtmlDisp *cur;

    /* finish any pending tab alignment */
    close_pending_tab();

    /* remember the start of the current line */
    old_line_head = line_head_;
    
    /* add this line start item to the line starts table */
    add_line_start(line_head_);

    /* set positions of items in the current line */
    set_line_positions(line_head_, next_line_head);

    /* forget the old line head if we have a new one */
    if (next_line_head != 0)
        line_head_ = 0;

    /* if temporary margins are in effect, restore original margins */
    if (temp_margins_)
        set_margins(margin_left_ + margin_left_delta_nxt_,
                    margin_right_ + margin_right_delta_nxt_);

    /* check the flow-around items to see if we're past them */
    check_flow_end();

    /* 
     *   if we have an item for the next line, we're definitely moving to
     *   a new line (we could add more later to the current line, if not);
     *   if this is the case, we can now move all of the deferred items
     *   into the display list, since these items were deferred until the
     *   start of the next line, which we have now reached 
     */
    if (next_line_head != 0)
    {
        /* add the deferred items */
        add_all_deferred_items(old_line_head, next_line_head, FALSE);

        /* set the x position back to the start of the line */
        curpos_.x = margin_left_;
    }

    /* we've output a line */
    line_output_ = TRUE;

    /* 
     *   make sure the items on the next lines have a reasonable default
     *   position, in case we draw them while we're still working on this
     *   line 
     */
    for (cur = next_line_head ; cur != 0 ; cur = cur->get_next_disp())
    {
        CHtmlRect pos;
        
        pos = cur->get_pos();
        pos.bottom = layout_max_y_pos_ + (pos.bottom - pos.top);
        pos.top = layout_max_y_pos_;
        cur->set_pos(&pos);
    }
}

/*
 *   Set line positions - sets the horizontal and vertical positions of
 *   the items in the line according to their sizes and display
 *   properties.  
 */
void CHtmlFormatter::set_line_positions(CHtmlDisp *line_head,
                                        CHtmlDisp *next_line_head)
{
    CHtmlDisp *cur;
    long x;
    int max_text_height;
    int max_above_base;
    int max_below_base;
    int max_total;
    int total_width;
    CHtmlRect area;
    HTML_Attrib_id_t align;

    /*
     *   Go through the display items and determine how much space we
     *   need for the text. 
     */
    for (max_text_height = 0, cur = line_head ;
         cur != 0 && cur != next_line_head ;
         cur = cur->get_next_disp())
    {
        int text_height;

        /* set this item's line ID */
        cur->set_line_id(line_id_);

        /* get this item's text space needs */
        text_height = cur->get_text_height(win_);

        /* if this exceeds the maximum needed so far, it's the new max */
        if (text_height > max_text_height) max_text_height = text_height;
    }

    /*
     *   Now that we know the total height of the text, go through and
     *   ask each item how much space it needs overall, and how much space
     *   it needs above the text baseline. 
     */
    for (max_total = 0, max_below_base = 0, max_above_base = 0,
         cur = line_head, total_width = 0 ;
         cur != 0 && cur != next_line_head ;
         cur = cur->get_next_disp())
    {
        int total;
        int above_base;
        int below_base;
        
        /* get this item's specific vertical space needs */
        cur->get_vertical_space(win_, max_text_height,
                                &above_base, &below_base, &total);

        /* if this exceeds the maxima needed so far, it's the new max */
        if (total > max_total) max_total = total;
        if (below_base > max_below_base) max_below_base = below_base;
        if (above_base > max_above_base) max_above_base = above_base;

        /* get this item's horizontal space needs */
        total_width += cur->measure_width(win_);
    }

    /*
     *   If the total above plus the total below exceeds the overall
     *   total, increase the overall total accordingly 
     */
    if (max_above_base + max_below_base > max_total)
        max_total = max_above_base + max_below_base;

    /* figure out where to start, according to the alignment settings */
    align = (blk_alignment_ == HTML_Attrib_invalid
             ? div_alignment_ : blk_alignment_);
    switch(align)
    {
    case HTML_Attrib_invalid:
    case HTML_Attrib_left:
    case HTML_Attrib_justify:
    default:
        /* left alignment -- start at the left margin */
        x = margin_left_;
        break;

    case HTML_Attrib_right:
        /*
         *   Right alignment -- start far enough in to line up with the
         *   right margin: subtract the total space needs for the line
         *   from the total amount of physical space in the window minus
         *   the size of the right margin.  Limit the position so that we
         *   don't start left of the left margin.  
         */
        x = (win_->get_disp_width() - margin_right_) - total_width;
        if (x < margin_left_)
            x = margin_left_;
        break;

    case HTML_Attrib_center:
        /*
         *   Centered -- start far enough in to center the line: subtract
         *   the total space needs for the line from the total amount of
         *   logical space in the line (which is the amount of physical
         *   space minus the space taken up by the margins), divide that
         *   in half, and add it to the left margin.  Limit the position
         *   so that we don't start left of the left margin.  
         */
        x = ((win_->get_disp_width() - margin_right_ - margin_left_)
             - total_width)/2 + margin_left_;
        if (x < margin_left_)
            x = margin_left_;
        break;
    }


    /* add the line spacing */
    max_total += line_spacing_;
    max_above_base += line_spacing_;

    /*
     *   Now that we know how much vertical space is required overall, we
     *   can figure out where the text baseline goes, so go through the
     *   items in the line again and set each item's position.  
     */
    for (cur = line_head ; cur != 0 && cur != next_line_head ;
         cur = cur->get_next_disp())
    {
        /* set this item's position */
        cur->set_line_pos(win_, x, curpos_.y,
                          max_total, max_above_base, max_text_height);

        /* move past this item horizontally */
        x += cur->measure_width(win_);
    }

    /* hide trailing whitespace on the line */
    CHtmlDisp_wsinfo wsinfo;
    wsinfo.hide_trailing_whitespace(win_, line_head, next_line_head);

    /*
     *   If the line width exceeds the maximum line width so far, note
     *   the new maximum and let the window know that it's changing, so
     *   that it can adjust scrollbar settings if necessary. 
     */
    if (x > max_line_width_)
    {
        /* remember the maximum line width */
        max_line_width_ = x;
    }

    /* 
     *   if we're not within a table, set the outermost max line width as
     *   well 
     */
    if (table_pass_ == 0 && x > outer_max_line_width_)
    {
        /* remember the outer-level max line width */
        outer_max_line_width_ = x;
        
        /* adjust the window's horizontal scrollbar */
        win_->fmt_adjust_hscroll();
    }

    /* adjust the total height record to include this line */
    layout_max_y_pos_ = curpos_.y + max_total;

    /* 
     *   if this is above the display max y position, and we're not laying
     *   out a table, advance the display pos 
     */
    if (layout_max_y_pos_ > disp_max_y_pos_ && table_pass_ == 0)
        disp_max_y_pos_ = layout_max_y_pos_;

    /*
     *   invalidate the screen area of the new text (unless display
     *   updating is frozen, in which case the caller will have to take
     *   care of it) 
     */
    if (!freeze_display_adjust_)
    {
        area.set(0, curpos_.y, HTMLSYSWIN_MAX_RIGHT,
                 curpos_.y + max_total);
        win_->inval_doc_coords(&area);
    }

    /*
     *   adjust the current vertical position to the top of the next
     *   line, if another line follows 
     */
    if (next_line_head != 0)
    {
        /* adjust the vertical position */
        curpos_.y += max_total;

        /* we've used the line spacing */
        line_spacing_ = 0;
    }
    
    /* note the new x position */
    curpos_.x = x;

    /* 
     *   notify the window of the new line (unless the display is frozen
     *   or we're in the middle of a table -- in either case, we'll wait
     *   until we're done with the frozen section or the table to update
     *   the window size range) 
     */
    if (!freeze_display_adjust_ && table_pass_ == 0)
        win_->fmt_adjust_vscroll();
}

/*
 *   Add an extra line height to the display height, beyond the height
 *   required for the layout height.  
 */
void CHtmlFormatter::add_line_to_disp_height()
{
    int ascent;
    unsigned long new_disp_ht;

    /* 
     *   figure the new display height - add one line height in the current
     *   font to the current layout height 
     */
    new_disp_ht = layout_max_y_pos_
                  + win_->measure_text(curfont_, " ", 1, &ascent).y;

    /* if it's higher than the maximum so far, update the maximum */
    if (new_disp_ht > disp_max_y_pos_ && table_pass_ == 0)
    {
        /* update the height */
        disp_max_y_pos_ = new_disp_ht;

        /* refigure the scrollbars */
        win_->fmt_adjust_vscroll();
    }
}

/*
 *   Push the current margin settings and set new margins 
 */
void CHtmlFormatter::push_margins(long left, long right)
{
    /* make sure there's room on the stack */
    if (margin_stack_depth_ >= MARGIN_STACK_DEPTH_MAX)
    {
        /*
         *   simply note the error and forget the enclosing margins, but
         *   go ahead and count the stack depth change anyway, since we'll
         *   want to at least keep count as the blocks get closed 
         */
        parser_->log_error("too many nested blocks with margin changes");
    }
    else
    {
        long left_delta, right_delta;
        
        /* 
         *   store the deltas from the current margins -- we want to store
         *   the amount to add to the new margins to get back to the
         *   current margins
         */
        left_delta = margin_left_ - left;
        right_delta = margin_right_ - right;
        margin_stack_[margin_stack_depth_].set(left_delta, right_delta);
    }

    /* increase the stack depth */
    ++margin_stack_depth_;

    /* set the new margins */
    set_margins(left, right);
}

/*
 *   Pop the margin stack, restoring the margins that were in effect just
 *   before the last call to push_margins() 
 */
void CHtmlFormatter::pop_margins()
{
    /* decrease the nesting level */
    --margin_stack_depth_;
    
    /*
     *   if we got too deeply nested, we can't restore the margins because
     *   we didn't save them; otherwise, restore them by applying the
     *   deltas to the current margins 
     */
    if (margin_stack_depth_ < MARGIN_STACK_DEPTH_MAX)
    {
        long new_left, new_right;

        new_left = margin_left_
                   + margin_stack_[margin_stack_depth_].left_delta_;
        new_right = margin_right_
                    + margin_stack_[margin_stack_depth_].right_delta_;
        set_margins(new_left, new_right);
    }
}

/*
 *   Set margins, and adjust the available line width for the change 
 */
void CHtmlFormatter::set_margins(long left, long right)
{
    /* adjust the available line width for the change in margins */
    avail_line_width_ -= (left - margin_left_);
    avail_line_width_ -= (right - margin_right_);
    curpos_.x += (left - margin_left_);
    
    /* set the new margins */
    margin_left_ = left;
    margin_right_ = right;

    /* temporary margins are not in effect */
    temp_margins_ = FALSE;
}

/*
 *   Set temporary margins.  These margins will remain in effect until the
 *   end of the current line, at which time the margins in effect prior to
 *   this call will be restored. 
 */
void CHtmlFormatter::set_temp_margins(long left, long right)
{
    /* remember the current margins as deltas from the new margins */
    margin_left_delta_nxt_ = margin_left_ - left;
    margin_right_delta_nxt_ = margin_right_ - right;

    /* set the new margins */
    set_margins(left, right);

    /* note that we have temporary margins */
    temp_margins_ = TRUE;
}

/*
 *   Draw everything in a given area 
 */
void CHtmlFormatter::draw(const CHtmlRect *area, int clip_lines,
                          long *clip_y)
{
    CHtmlDisp *cur;
    CHtmlRect curpos;
    long cur_ypos;
    long line_index;

    /* check line start records */
    if (line_starts_->get_count() == 0)
    {
        /* no line starts - start at beginning of list */
        line_index = 0;
        cur = disp_head_;
        cur_ypos = 0;
    }
    else
    {
        /* start at the first line containing the top of the area to draw */
        line_index = line_starts_->find_by_ypos(area->top);
        cur = line_starts_->get(line_index);
        cur_ypos = line_starts_->get_ypos(line_index);
    }

    /* draw lines until we're out of view */
    for (;;)
    {
        CHtmlDisp *nxt_line;
        long nxt_ypos;

        /* remember the next line and its position */
        if (line_index + 1 < line_count_)
        {
            /* there's another line - get it */
            nxt_line = line_starts_->get(line_index + 1);
            nxt_ypos = line_starts_->get_ypos(line_index + 1);
        }
        else
        {
            /* this is the last line */
            nxt_line = 0;
            nxt_ypos = layout_max_y_pos_;
        }

        /*
         *   if we're clipping lines, and this line doesn't entirely fit,
         *   don't draw anything more
         */
        if (clip_lines && nxt_ypos > area->bottom)
        {
            /* tell the caller where we started clipping */
            if (clip_y) *clip_y = cur_ypos;

            /* 
             *.  .. but keep drawing - we don't actually clip out the
             *   lines, but just note where clipping would have occurred 
             */
        }

        /* 
         *   draw the current line; stop if we reach the start of the next
         *   line
         */
        for ( ; cur != 0 && cur != nxt_line ; cur = cur->get_next_disp())
        {
            /* draw it only if it intersects the display area horizontally */
            curpos = cur->get_pos();
            if (curpos.right >= area->left && curpos.left <= area->right)
                cur->draw(win_, sel_start_, sel_end_, FALSE);
        }

        /* move on to the next line */
        ++line_index;
        cur_ypos = nxt_ypos;

        /* if the next line isn't in view, don't display it */
        if (cur == 0 || nxt_ypos > area->bottom)
            break;
    }
}

/*
 *   Invalidate links that are visible on the screen.  This is used to
 *   quickly redraw the window's links (and only its links) for a
 *   short-term change in the link visibility state.  This can be used to
 *   implement a mode where links are highlighted only if a key is held
 *   down.  
 */
void CHtmlFormatter::inval_links_on_screen(const CHtmlRect *area)
{
    CHtmlDisp *cur;
    CHtmlRect curpos;
    long cur_ypos;
    long line_index;

    /* check line start records */
    if (line_starts_->get_count() == 0)
    {
        /* no line starts - start at beginning of list */
        line_index = 0;
        cur = disp_head_;
        cur_ypos = 0;
    }
    else
    {
        /* start at the first line containing the top of the area to draw */
        line_index = line_starts_->find_by_ypos(area->top);
        cur = line_starts_->get(line_index);
        cur_ypos = line_starts_->get_ypos(line_index);
    }

    /* invalidate links until we're out of view */
    for (;;)
    {
        CHtmlDisp *nxt_line;
        long nxt_ypos;

        /* remember the next line and its position */
        if (line_index + 1 < line_count_)
        {
            /* there's another line - get it */
            nxt_line = line_starts_->get(line_index + 1);
            nxt_ypos = line_starts_->get_ypos(line_index + 1);
        }
        else
        {
            /* this is the last line */
            nxt_line = 0;
            nxt_ypos = layout_max_y_pos_;
        }

        /* 
         *   invalidate links in the current line; stop when we reach the
         *   start of the next line 
         */
        for ( ; cur != 0 && cur != nxt_line ; cur = cur->get_next_disp())
            cur->inval_link(win_);

        /* move on to the next line */
        ++line_index;
        cur_ypos = nxt_ypos;

        /* if the next line isn't in view, don't display it */
        if (cur == 0 || nxt_ypos > area->bottom)
            break;
    }
}


/*
 *   Get the height of the line at the given y position 
 */
long CHtmlFormatter::get_line_height_ypos(long ypos) const
{
    long line_index;
    long ypos_cur;
    long ypos_nxt;

    /* get the line at the given position */
    line_index = line_starts_->find_by_ypos(ypos);

    /* get the y position of that line */
    ypos_cur = line_starts_->get_ypos(line_index);

    /* get the y position of the next line */
    if (line_index + 1 < line_count_)
        ypos_nxt = line_starts_->get_ypos(line_index + 1);
    else
        ypos_nxt = layout_max_y_pos_;

    /*
     *   the height of the desired line is the difference in positions of
     *   the desired line and the following line 
     */
    return ypos_nxt - ypos_cur;
}


/*
 *   Get the position of a character, given the text offset in the
 *   character array 
 */
CHtmlPoint CHtmlFormatter::get_text_pos(unsigned long txtofs) const
{
    long index;
    CHtmlDisp *disp;
    
    /*
     *   Search the line starts table for the line containing the given
     *   text offset; this won't find the exact item, but will quickly
     *   narrow it down to a single line.  If we don't find any index
     *   entries, start at the first display item.  
     */
    index = line_starts_->find_by_txtofs(txtofs);
    disp = line_starts_->get(index);
    if (disp == 0)
        disp = disp_head_;

    /* search for the item starting at this line start */
    return get_text_pos_list(disp, txtofs);
}

/*
 *   Search the display list for an item containing a text offset,
 *   starting at a particular point in the display list, and return the
 *   position of the text in that item.  
 */
CHtmlPoint CHtmlFormatter::get_text_pos_list(CHtmlDisp *start,
                                             unsigned long txtofs) const
{
    CHtmlDisp *cur;

    /* scan the line for the particular item containing this offset */
    for (cur = start ; cur ; cur = cur->get_next_disp())
    {
        /* if this one is in range, it's the one we want */
        if (txtofs >= cur->get_text_ofs()
            && (cur->get_next_disp() == 0
                || txtofs < cur->get_next_disp()->get_text_ofs()))
        {
            /* return the position of the text within this item */
            return cur->get_text_pos(win_, txtofs);
        }
    }

    /* didn't find it - return the next formatting position */
    if (disp_tail_)
    {
        CHtmlRect r = disp_tail_->get_pos();
        return CHtmlPoint(r.left, r.top);
    }
    else
        return curpos_;
}

/*
 *   Find an item given a position 
 */
CHtmlDisp *CHtmlFormatter::find_by_pos(CHtmlPoint pos, int exact) const
{
    CHtmlDisp *cur;
    CHtmlDisp *curline;
    CHtmlDisp *nxtline;
    CHtmlDisp *best_inexact_match;
    CHtmlDisp *prv;
    long index;

    /* check if position is out of range above */
    if (pos.y < 0)
    {
        /*
         *   it's a negative y position - start with the first item if
         *   they'll allow a non-exact match, otherwise there's nothing
         *   that matches 
         */
        if (exact)
            return 0;
        else
            return get_first_disp();
    }

    /* if there are no line starts, use the entire list */
    if (line_starts_->get_count() == 0)
    {
        /* no line starts - search the entire list */
        index = 0;
        curline = get_first_disp();
        nxtline = 0;
    }
    else
    {
        /* find the line containing the y position */
        index = line_starts_->find_by_ypos(pos.y);
        curline = line_starts_->get(index);

        /* note the first item in the next line */
        nxtline = (index + 1 >= line_starts_->get_count() ? 0
            : line_starts_->get(index + 1));
    }

    /* search the line for an item containing the given point */
    for (prv = 0, best_inexact_match = 0, cur = curline ;
         cur != 0 && cur != nxtline ;
         prv = cur, cur = cur->get_next_disp())
    {
        CHtmlRect curpos;
        
        /* if this item contains the given point, it's the one */
        curpos = cur->get_pos();
        if (curpos.contains(pos))
        {
            /* 
             *   If this display item ignores clicks, go on to the next
             *   one.  Some display items are overlaid on other items,
             *   such as table cells and their contents; in these cases,
             *   the "background" item will ignore clicks so that we move
             *   on to the "foreground" objects. 
             */
            if (cur->is_in_background())
                continue;
            
            /* this is the one */
            return cur;
        }

        /* 
         *   if we'll allow non-exact matches, and we're to the left of
         *   or above this item, it could be an inexact match
         */
        if (!exact
            && cur->allow_inexact_hit()
            && (pos.y < curpos.top
                || (pos.x < curpos.left
                    && pos.y >= curpos.top && pos.y <= curpos.bottom)))
        {
            /*
             *   if we don't have a possible inexact match yet, note this
             *   one, since the only one is automatically the best; if we
             *   have one already, take this one if it's above the current
             *   best inexact match 
             */
            if (best_inexact_match == 0)
            {
                /* this is the first one that could be an inexact match */
                best_inexact_match = cur;
            }
            else
            {
                CHtmlRect matchpos;

                /* get the position of the best fitting one so far */
                matchpos = best_inexact_match->get_pos();

                /*
                 *   if it's above the last match, or it's left of the
                 *   last match but not below the last match, use it 
                 */
                if (curpos.top < matchpos.top
                    || (curpos.left < matchpos.left
                        && curpos.top < matchpos.bottom))
                    best_inexact_match = cur;
            }
        }
    }

    /*
     *   We didn't find a match.  If they wanted an exact match, return
     *   null; if they only wanted the nearest item, return the first item
     *   on the next line. 
     */
    if (exact)
    {
        /* 
         *   We required exact match, and we didn't find one.  As a last
         *   resort, try looking for a <DIV> tag that covers this area. 
         */
        return find_div_by_pos(pos);
    }
    else if (best_inexact_match != 0)
    {
        /*
         *   exact match is not required, and we found a near fit; return
         *   the best near fit we found 
         */
        return best_inexact_match;
    }
    else
    {
        /*
         *   Exact match is not required; return the last item on the
         *   line, if we found one, otherwise return the first item on
         *   next line; if this is the last line, return the start of the
         *   current (i.e., last) line 
         */
        if (prv != 0)
            return prv;
        else if (nxtline != 0)
            return nxtline;
        else
            return disp_tail_;
    }
}

/*
 *   Find the enclosing <DIV>, if any, given spatial coordinates within the
 *   document.  
 */
CHtmlDispDIV *CHtmlFormatter::find_div_by_pos(CHtmlPoint pos) const
{
    /* get our text offste, and find the item that way */
    return find_div_by_ofs(find_textofs_by_pos(pos), pos.y);
}

/*
 *   Find the enclosing <DIV>, if any, given a text offset. 
 */
CHtmlDispDIV *CHtmlFormatter::find_div_by_ofs(
    unsigned long ofs, long ypos) const
{
    int lo, hi;
    
    /* 
     *   Search the <DIV> table for a match.  The <DIV> table is always in
     *   ascending order of text offset, so we can do a binary search. 
     */
    for (lo = 0, hi = div_list_.get_count() - 1 ; ; )
    {
        int cur;
        CHtmlDispDIV *div;
        CHtmlDispDIV *par;
        
        /* stop if we've exhausted the list */
        if (lo > hi)
            return 0;

        /* split the difference */
        cur = lo + (hi - lo)/2;

        /* get this entry */
        div = (CHtmlDispDIV *)div_list_.get_ele(cur);

        /*
         *   DIVs can be nested, which means that the ranges aren't strictly
         *   monotonic, which means that we can't search for ranges with a
         *   simple binary search.  However, we can search for *top-level*
         *   DIV ranges with a binary search, because top-level DIVs are
         *   non-overlapping and monotonic.  So, only consider top-level
         *   DIVs. 
         */
        while ((par = div->get_parent_div()) != 0)
            div = par;

        /* if we've gone below the bounds of the range, readjust the range */
        if (cur < lo)
            lo = cur;

        /* check to see if we're high or low */
        if (ofs < div->get_start_ofs())
        {
            /* this one's too high - continue searching the lower half */
            hi = (cur == hi ? hi - 1 : cur);
        }
        else if (ofs > div->get_end_ofs())
        {
            /* this one's too low - continue searching the upper half */
            lo = (cur == lo ? lo + 1 : cur);
        }
        else
        {
            CHtmlDisp *nxt;
            size_t chi_idx;

            /* 
             *   find the actual index of 'div' - the index 'cur' might
             *   currently refer to a child of 'div' since we always seek the
             *   top-level item at each point 
             */
            while (cur != 0 && (CHtmlDispDIV *)div_list_.get_ele(cur) != div)
                --cur;

            /*
             *   This DIV covers the area, but make sure that we're within
             *   the vertical extent of the actual contents of the DIV.  We
             *   could be to the right of the last display item above the
             *   DIV, in which case we're in the right text-offset range but
             *   not within the actual visual area of the DIV.
             *   
             *   So, get the next item, and make sure our y position is
             *   greater than (i.e., visually below) the top of the next
             *   item.  If not, we're not within this DIV after all; and
             *   since we only scan top-level DIVs, we can be sure there's
             *   not another one that contains the text area.  Therefore,
             *   we're simply not in a DIV at all.  
             */
            nxt = div->get_next_disp();
            if (nxt == 0 || nxt->get_pos().top > ypos)
                return 0;

            /*
             *   Okay, we found the top-level DIV that contains this
             *   position.  There's one more thing to do: we need to check to
             *   see if any child of this DIV contains the point.  If so, we
             *   want to return the innermost child that contains the point.
             *   
             *   We can find the child as follows.  Scan ahead and look at
             *   subsequent items, stopping when we reach another top-level
             *   item.  (All of our children will be after us and before the
             *   next top-level item, so this will find all of our children.)
             *   Only consider our *direct* children.  For each child we
             *   find, if that child contains the position, start this
             *   process over with that child's direct children.  As with
             *   top-level items, a child item's children are after the child
             *   and before the next sibling - i.e., the next item with the
             *   same parent.  On any pass, if we don't find children
             *   containing the position, return the current item.  
             */

        child_scan:
            /* scan the current DIV's children */
            for (par = div->get_parent_div(), chi_idx = cur + 1 ;
                 chi_idx < div_list_.get_count() ; ++chi_idx)
            {
                CHtmlDispDIV *chi;
                
                /* get this child element */
                chi = (CHtmlDispDIV *)div_list_.get_ele(chi_idx);
                
                /* 
                 *   if this is a sibling of 'div', we're done scanning
                 *   children of 'div' 
                 */
                if (chi->get_parent_div() == par)
                    break;
                
                /* if this isn't a direct child of 'div', skip it */
                if (chi->get_parent_div() != div)
                    continue;
                
                /* 
                 *   does this child contain the text offset and vertical
                 *   position?  
                 */
                if (ofs >= chi->get_start_ofs()
                    && ofs <= chi->get_end_ofs()
                    && (nxt = chi->get_next_disp()) != 0
                    && ypos >= nxt->get_pos().top)
                {
                    /* 
                     *   This child contains the position, so we want to
                     *   return this child rather than the parent.  However,
                     *   we need to continue the scan into the child.  Simply
                     *   reset everything with the main find set to this
                     *   child, and start the child scan again, this time
                     *   with this item's children.  
                     */
                    cur = chi_idx;
                    div = chi;
                    goto child_scan;
                }
            }

            /* 
             *   We didn't find any children of 'div' that contain the
             *   position, so 'div' is definitely the one - return it.
             */
            return div;
        }
    }
}

/*
 *   Get the text offset given a position 
 */
unsigned long CHtmlFormatter::find_textofs_by_pos(CHtmlPoint pos) const
{
    CHtmlDisp *disp;
    unsigned long ofs;
    unsigned long max_ofs;
    
    /*
     *   Find the display item containing the position.  Don't require an
     *   exact match; if we can't find anything containing the given
     *   position, use the nearest item. 
     */
    if ((disp = find_by_pos(pos, FALSE)) == 0)
        return 0;

    /* note the maximum text offset */
    max_ofs = get_text_ofs_max();

    /*
     *   As a special case, if we're out of range above or below the
     *   entire text stream, go to the beginning of the first item or end
     *   of the last item, respectively 
     */
    if (pos.y < 0)
        return 0;
    else if (disp == disp_tail_ && pos.y > disp->get_pos().bottom)
        return max_ofs;

    /* find the offset of the given position */
    ofs = disp->find_textofs_by_pos(win_, pos);
    return (ofs > max_ofs ? max_ofs : ofs);
}

/*
 *   Find an item by the debugger source file position 
 */
CHtmlDisp *CHtmlFormatter::find_by_debugsrcpos(unsigned long linenum) const
{
    long index;

    /* if there are no lines, there's nothing to do */
    if (line_starts_->get_count() == 0)
        return 0;

    /* if the file position is zero, start at the beginning */
    if (linenum == 0)
        return disp_head_;

    /* find the line at the given position */
    index = line_starts_->find_by_debugsrcpos(linenum);
    return line_starts_->get(index);
}

/*
 *   Find a line number given the debugger source file position 
 */
long CHtmlFormatter::findidx_by_debugsrcpos(unsigned long linenum) const
{
    /* if there are no lines, there's nothing to do */
    if (line_starts_->get_count() == 0)
        return 0;

    /* if the file position is zero, start at the beginning */
    if (linenum == 0)
        return 0;

    /* find the line at the given position and return the line index */
    return line_starts_->find_by_debugsrcpos(linenum);
}

/*
 *   Find an item given the line number
 */
CHtmlDisp *CHtmlFormatter::get_disp_by_linenum(long linenum) const
{
    /* if there aren't any lines, there's nothing to return */
    if (line_starts_->get_count() == 0)
        return 0;
    
    /* if it's past the last line, return the last line */
    if (linenum >= line_starts_->get_count())
        linenum = line_starts_->get_count() - 1;
    
    /* get the item from the line starts table */
    return line_starts_->get(linenum);
}


/*
 *   Find an item by text offset 
 */
CHtmlDisp *CHtmlFormatter::find_by_txtofs(unsigned long ofs, int use_prv,
                                          int use_nxt) const
{
    CHtmlDisp *cur;
    CHtmlDisp *prv;
    CHtmlDisp *nxtline;
    long index;

    /* if the text offset is zero, start at the beginning */
    if (ofs == 0)
        return disp_head_;

    /* use the line starts if possible; if not, search the entire list */
    if (line_starts_->get_count() != 0)
    {
        /* find the line containing the text offset in the line starts */
        index = line_starts_->find_by_txtofs(ofs);
        cur = line_starts_->get(index);

        /* note the first item in the next line */
        nxtline = (index + 1 >= line_starts_->get_count() ? 0
            : line_starts_->get(index + 1));
    }
    else
    {
        /* no line starts - search the entire list */
        index = 0;
        cur = disp_head_;
        nxtline = 0;
    }

    /* search the line for an item containing the given point */
    for (prv = 0 ; cur != 0 && cur != nxtline ;
         prv = cur, cur = cur->get_next_disp())
    {
        /* if this item contains the given point, it's the one */
        if (cur->contains_text_ofs(ofs)
            || (use_nxt && cur->get_text_ofs() > ofs)
            || (use_prv && (cur->get_next_disp() == 0
                            || cur->get_next_disp()->get_text_ofs() > ofs)))
            return cur;

        /* 
         *   as a special case, if this item is exactly at the text offset
         *   (which means that this item contains zero length text, since
         *   otherwise we would have matched it), and the next line is not
         *   at the same offset, and the offset we're looking for is right
         *   at this item, return this item if we're allowed any sort of
         *   approximation (i.e., either use_nxt or use_prv is set) 
         */
        if (cur->get_text_ofs() == ofs
            && (cur->get_next_disp() == 0
                || cur->get_next_disp()->get_text_ofs() != ofs)
            && (use_nxt || use_prv))
            return cur;
    }

    /* 
     *   if we didn't find anything, use either the last item on the
     *   previous line or the first item on the next line, depending on
     *   which way they asked us to go 
     */
    if (use_prv)
        return prv;
    else if (use_nxt)
    {
        /* 
         *   if the last item we found is the last item, and starts at the
         *   desired offset, it must be a zero-length item; since there's
         *   no next item, it's the desired item 
         */
        if (prv != 0 && prv->get_next_disp() == 0
            && prv->get_text_ofs() == ofs)
            return prv;
        
        /* return start of the next line */
        return nxtline;
    }

    /* we didn't find a match */
    return 0;
}

/*
 *   Set the selection range 
 */
void CHtmlFormatter::set_sel_range(unsigned long start, unsigned long end)
{
    unsigned long l1, l2, r1, r2;
    
    /* make sure they're in canonical order - start <= end */
    if (start > end)
    {
        long tmp = end;
        end = start;
        start = tmp;
    }
    
    /*
     *   Invalidate the area outside the intersection of the old range
     *   and the new range, since that area is changing it's highlighting
     *   state.  First, calculate l1 (the leftmost starting point), l2
     *   (the rightmost starting point), r1 (the leftmost ending point),
     *   and r2 (the rightmost ending point).  
     */
    if (start <= sel_start_)
        l1 = start, l2 = sel_start_;
    else
        l1 = sel_start_, l2 = start;
    if (end <= sel_end_)
        r1 = end, r2 = sel_end_;
    else
        r1 = sel_end_, r2 = end;

    /*
     *   Invalidate from the leftmost edge (which is always l1) to the
     *   further left of l2 and r1 
     */
    inval_offset_range(l1, (l2 < r1 ? l2 : r1));

    /*
     *   Invalidate from the further right of r1 and l2 to the rightmost
     *   edge (which is always r2) 
     */
    inval_offset_range((l2 > r1 ? l2 : r1), r2);
    
    /* remember the new range */
    sel_start_ = start;
    sel_end_ = end;
}

/*
 *   Set the selection range given the starting and ending coordinates 
 */
void CHtmlFormatter::set_sel_range(CHtmlPoint start, CHtmlPoint end,
                                   unsigned long *startofsp,
                                   unsigned long *endofsp)
{
    unsigned long startofs;
    unsigned long endofs;
    
    /* find the text offsets of the starting and ending points */
    startofs = find_textofs_by_pos(start);
    if (end.x == start.x && end.y == start.y)
        endofs = startofs;
    else
        endofs = find_textofs_by_pos(end);

    /* set the selection range for the given offsets */
    set_sel_range(startofs, endofs);

    /* return the start and end offsets if desired */
    if (startofsp) *startofsp = startofs;
    if (endofsp) *endofsp = endofs;
}


/*
 *   Increment or decrement a text array address to the next or previous
 *   word boundary.  If dir is positive, we'll increment the pointer,
 *   otherwise we'll decrement the pointer.  
 */
unsigned long CHtmlFormatter::find_word_boundary(
    unsigned long start, int dir) const
{
    int start_class;
    int cur_class;
    unsigned long cur;
    unsigned long nxt;

    /* note whether we started off in a word or not */
    start_class = get_char_class(get_char_at_ofs(start));

    /* keep going until we cross a word boundary or run out of text */
    for (cur = start ;; )
    {
        /* move the pointer in the appropriate direction */
        if (dir > 0)
        {
            /* going forward - increment the pointer */
            nxt = inc_text_ofs(cur);
            if (nxt > get_text_ofs_max())
                break;
        }
        else
        {
            /* going backwards - decrement the pointer */
            if (cur == 0)
                break;
            nxt = dec_text_ofs(cur);
        }

        /* if we've reached the beginning or end of the text, stop here */
        if (nxt == cur)
            break;

        /* check if the next character is in a word */
        cur_class = get_char_class(get_char_at_ofs(nxt));
        
        /* if we crossed a boundary, stop */
        if (cur_class != start_class)
            break;

        /* move to the next character */
        cur = nxt;
    }

    /* return the final position */
    return (dir > 0 ? nxt : cur);
}


/*
 *   Given a text offset, get the extent of the word containing the
 *   offset 
 */
void CHtmlFormatter::get_word_limits(unsigned long *start,
                                     unsigned long *end,
                                     unsigned long txtofs) const
{
    /* find the start and end boundaries */
    *start = find_word_boundary(txtofs, -1);
    *end = find_word_boundary(txtofs, 1);
}


/*
 *   Given a text offset, get the extent of the line containing the
 *   offset.  If the offset is in the current input line, we'll get the
 *   limits of the input line, not including any prompt that appears on
 *   the same line.  
 */
void CHtmlFormatter::get_line_limits(unsigned long *start,
                                     unsigned long *end,
                                     unsigned long txtofs) const
{
    CHtmlDisp *cur;
    CHtmlDisp *lstart;
    long index;
    
    /* if we have no lines, there's nothing to do */
    if (line_starts_->get_count() == 0)
    {
        *start = *end = 0;
        return;
    }

    /* find the start of this line */
    index = line_starts_->find_by_txtofs(txtofs);
    cur = line_starts_->get(index);

    /*
     *   We may not have a line start record at the start of the line with
     *   the target offset, since we don't keep line starts for every
     *   line; hence, we need to search for the start of the target line.
     *   Scan from the starting display item to the display item
     *   containing the text offset we're looking for, noting line
     *   changes.  
     */
    for (lstart = cur ; cur != 0 ; cur = cur->get_next_disp())
    {
        /* 
         *   if this item is on a different line than the start of the
         *   current line, it's the start of a new line 
         */
        if (cur->get_line_id() != lstart->get_line_id())
            lstart = cur;

        /* if this item contains our text offset, we can stop looking */
        if (cur->contains_text_ofs(txtofs))
            break;

        /* 
         *   if the next item is above our target offset, consider the
         *   point to be the current item 
         */
        if (cur->get_next_disp() != 0
            && cur->get_next_disp()->get_text_ofs() > txtofs)
            break;
    }

    /* the start offset is the offset of the first item on the line */
    *start = (lstart != 0 ? lstart->get_text_ofs() : 0);

    /*
     *   Now keep scanning until we find the start of the next line 
     */
    for ( ; cur != 0 ; cur = cur->get_next_disp())
    {
        /* 
         *   if this item is on a different line than the previous one,
         *   we're done 
         */
        if (cur->get_line_id() != lstart->get_line_id())
            break;
    }

    /*
     *   if there's another line, the end offset is the offset of the
     *   first item on the next line, otherwise select to the end of the
     *   text 
     */
    if (cur != 0)
        *end = cur->get_text_ofs();
    else
        *end  = get_text_ofs_max();
}
    
/*
 *   Invalidate a range of text given by offsets 
 */
void CHtmlFormatter::inval_offset_range(long l, long r)
{
    CHtmlDisp *displ;
    // CHtmlDisp *dispr;
    CHtmlDisp *dispmax;
    CHtmlDisp *cur;
    long index;

    /* if the offsets match, do nothing */
    if (l == r)
        return;
    
    /* find the items bounding the given offsets */
    displ = find_by_txtofs(l, TRUE, FALSE);
    // dispr = find_by_txtofs(r, FALSE, TRUE);

    /* 
     *   Find the next line after the ending text offset.  This gives us a
     *   reliable upper bound for how far we have to go.  We can't merely
     *   stop when we reach a text offset higher than our ending text
     *   offset, because we might have non-monotonic offsets due to tables
     *   or other multi-column areas.  To allow for this possibility, we'll
     *   keep going until we reach the start of the next line.  
     */
    index = line_starts_->find_by_txtofs(r);
    if (index + 1 < line_count_)
    {
        /* stop at the first item from the next line */
        ++index;
        dispmax = line_starts_->get(index);
    }
    else
    {
        /* there is no next line, so look at all remaining items */
        dispmax = 0;
    }

    /* 
     *   invalidate all of the items within range, starting with the first
     *   item and continuing until we reach the start of the next line 
     */
    for (cur = displ ; cur != 0 ; cur = cur->get_next_disp())
    {
        /* invalidate this item if it's within range */
        if (cur->overlaps_text_range(l, r))
            cur->inval_range(win_, l, r);

        /* if we've reached the start of the next line, stop scanning */
        if (cur == dispmax)
            break;
    }
}

/*
 *   Invalidate the whole window 
 */
void CHtmlFormatter::inval_window()
{
    /* if we have a window, invalidate it */
    if (win_ != 0)
    {
        CHtmlRect rc;

        /* invalidate the entire window */
        rc.set(0, 0, HTMLSYSWIN_MAX_RIGHT, HTMLSYSWIN_MAX_BOTTOM);
        win_->inval_doc_coords(&rc);
    }
}

/*
 *   Heap allocator.  The formatter provides a simple heap for allocating
 *   display items.  Only display items should be allocated here, since an
 *   item allocated here is automatically deleted when we start
 *   reformatting, and can't be removed from memory any other way. 
 */
void *CHtmlFormatter::heap_alloc(size_t siz)
{
    void *ret;

    /* 
     *   if they're asking for more than the maximum page size, we must
     *   increase the unit size to satisfy the request 
     */
    if (siz > heap_alloc_unit_)
    {
        size_t i;
        
        /* increase the page size */
        heap_alloc_unit_ = siz;

        /* 
         *   to avoid a big pile of pages that we can't use, delete all of
         *   the pages that we've previously allocated after the current
         *   page -- we'll reallocate them as needed eventually, but at
         *   the new larger size 
         */
        for (i = heap_page_cur_ + 1 ; i < heap_pages_alloced_ ; ++i)
            th_free(heap_pages_[i].mem);
        heap_pages_alloced_ = heap_page_cur_ + 1;
    }

    /* keep going until we satisfy the request */
    for (;;)
    {
        /* if there's room in the current page, allocate out of it */
        if (heap_page_cur_ < heap_pages_alloced_
            && heap_page_cur_ofs_ + siz <= heap_pages_[heap_page_cur_].siz)
        {
            /* there's room in this page - return pointer to next free byte */
            ret = (void *)(heap_pages_[heap_page_cur_].mem
                           + heap_page_cur_ofs_);
            
            /* advance past the new block */
            heap_page_cur_ofs_ += os_align_size(siz);
            
            /* return the pointer */
            return ret;
        }

        /*
         *   We've exhausted the current page, so we need to move on to
         *   the next one.  If we haven't allocated a page at the new
         *   slot, do so now. 
         */
        if (heap_page_cur_ + 1 < heap_pages_alloced_)
        {
            /* 
             *   we've already allocated a page here - start over at the
             *   new page 
             */
            ++heap_page_cur_;
            heap_page_cur_ofs_ = 0;

            /* go back and try with this new page */
            continue;
        }

        /*
         *   If necessary, extend the page array 
         */
        if (heap_page_cur_ + 1 >= heap_pages_max_)
        {
            size_t alloc_size;

            /* allocate space for another bunch of pages */
            heap_pages_max_ += 20;
            alloc_size = heap_pages_max_ * sizeof(CHtmlFmt_heap_page_t);
            
            /* allocate or reallocate the page array */
            if (heap_pages_ == 0)
            {
                /* allocate the initial page array */
                heap_pages_ = (CHtmlFmt_heap_page_t *)th_malloc(alloc_size);

                /* start at the first page */
                heap_page_cur_ = 0;
            }
            else
            {
                /* reallocate the current page array */
                heap_pages_ = (CHtmlFmt_heap_page_t *)
                              th_realloc(heap_pages_, alloc_size);

                /* move on to the next page */
                ++heap_page_cur_;
            }
        }
        else
        {
            /* there's room in the array - move on to the next page */
            ++heap_page_cur_;
        }

        /* allocate a new page */
        heap_pages_[heap_pages_alloced_].siz = heap_alloc_unit_;
        heap_pages_[heap_pages_alloced_].mem = (unsigned char *)
                                               th_malloc(heap_alloc_unit_);
        ++heap_pages_alloced_;

        /* start at the beginning of the new page */
        heap_page_cur_ofs_ = 0;
    }
}


/*
 *   Hash table entry for an image map 
 */
class CHtmlFmtHashEntryMap: public CHtmlHashEntryCI
{
public:
    CHtmlFmtHashEntryMap(const textchar_t *str, size_t len,
                         CHtmlFmtMap *map)
        : CHtmlHashEntryCI(str, len, TRUE)
    {
        map_ = map;
    }

    ~CHtmlFmtHashEntryMap()
    {
        /* delete my map */
        delete map_;
    }

    /* my map */
    CHtmlFmtMap *map_;
};

/*
 *   Begin a map 
 */
void CHtmlFormatter::begin_map(const textchar_t *map_name, size_t name_len)
{
    /* make sure the previous map is closed */
    end_map();

    /* create a new map */
    cur_map_ = new CHtmlFmtMap(map_name, name_len);
}

/*
 *   end the current map 
 */
void CHtmlFormatter::end_map()
{
    CHtmlFmtHashEntryMap *entry;

    /* if there's no map, there's nothing to do */
    if (cur_map_ == 0)
        return;

    /* if there's already a map with this name, replace it */
    entry = (CHtmlFmtHashEntryMap *)
            map_table_->find(cur_map_->get_name(),
                             get_strlen(cur_map_->get_name()));
    if (entry != 0)
    {
        /* delete the old map in the entry */
        delete entry->map_;

        /* replace it with the new map */
        entry->map_ = cur_map_;
    }
    else
    {
        /* create a new entry */
        entry = new CHtmlFmtHashEntryMap(cur_map_->get_name(),
                                         get_strlen(cur_map_->get_name()),
                                         cur_map_);

        /* add it to the table */
        map_table_->add(entry);
    }

    /* 
     *   the new map is finished and has been added to the table, so
     *   forget about it in the "under construction" area
     */
    cur_map_ = 0;
}

/*
 *   Get a map given a map name 
 */
CHtmlFmtMap *CHtmlFormatter::get_map(const textchar_t *mapname,
                                     size_t namelen)
{
    CHtmlFmtHashEntryMap *entry;

    /* look up the map in the hash table */
    entry = (CHtmlFmtHashEntryMap *)map_table_->find(mapname, namelen);

    /* if we found the entry, return the map */
    return (entry != 0 ? entry->map_ : 0);
}

/*
 *   add a hot spot to the current map 
 */
void CHtmlFormatter::add_map_area(HTML_Attrib_id_t shape,
                                  CHtmlUrl *href,
                                  const textchar_t *alt, size_t altlen,
                                  const CHtmlTagAREA_coords_t *coords,
                                  int coord_cnt, int append, int noenter)
{
    CHtmlDispLink *link;
    CHtmlFmtMapZone *zone;

    /* if there's no open map, we can't add a map area */
    if (cur_map_ == 0)
        return;

    /* 
     *   Set up a link for the hot spot and add it to the display list.
     *   Note that we use the direct item adder, not the link item adder,
     *   because we do not want to open a link range -- this is a
     *   single-use link that doesn't contain any other display items. 
     */
    link = new (this)
           CHtmlDispLink(FALSE, append, noenter, href, alt, altlen);
    add_disp_item(link);

    /* set up an appropriate zone object based on the shape setting */
    switch(shape)
    {
    case HTML_Attrib_rect:
        zone = new CHtmlFmtMapZoneRect(coords, link);
        break;

    case HTML_Attrib_circle:
        zone = new CHtmlFmtMapZoneCircle(coords, link);
        break;

    case HTML_Attrib_poly:
        zone = new CHtmlFmtMapZonePoly(coords, coord_cnt, link);
        break;

    default:
        /* other cases should never occur; ignore them if they do */
        break;
    }

    /* add the new zone to the current map */
    cur_map_->add_zone(zone);
}

/*
 *   Begin a table 
 */
CHtmlTagTABLE *CHtmlFormatter::begin_table(CHtmlTagTABLE *table_tag,
                                           CHtmlDispTable *table_disp,
                                           CHtmlPoint *enclosing_table_pos,
                                           CHtmlTagCAPTION *caption_tag)
{
    CHtmlTagTABLE *enclosing_table;
    long y_ofs;

    /* remember the enclosing table, so we can return it to the caller */
    enclosing_table = current_table_;

    /* remember the new table tag as the current table */
    current_table_ = table_tag;

    /* presume we won't need a y offset for the contents as we format */
    y_ofs = 0;

    /* check which pass over the table we're running */
    switch(table_pass_)
    {
    case 0:
        /*
         *   If we're on the first line (i.e., there are no line starts),
         *   insert a zero-height line before the start of the table.  
         */
        if (line_count_ == 0)
        {
            /* 
             *   add a blank item to anchor the first line, then start a
             *   new line -- make both of these zero-height breaks, since
             *   we only need something to serve as a placeholder in the
             *   list and don't want to actually take up any screen space 
             */
            add_disp_item(new (this) CHtmlDispBreak(0));
            add_disp_item_new_line(new (this) CHtmlDispBreak(0));
        }
        
        /*
         *   We're not yet doing a table, so this is an outermost (not
         *   nested) table.  Prepare for the first pass, which will simply
         *   measure the contents of the table without actually setting
         *   any positions.  We'll go through the motions of formatting
         *   contents during this pass, but we won't break any lines and
         *   we won't save any of the results of our work apart from the
         *   statistics on how large the contents of the various cells
         *   are.  So, we'll remember the state of the display list and
         *   heap as it was prior to the table, so that we can restore all
         *   of this state when the first pass is through. 
         */
        pre_table_heap_page_cur_ = heap_page_cur_;
        pre_table_heap_page_cur_ofs_ = heap_page_cur_ofs_;
        pre_table_disp_tail_ = disp_tail_;
        pre_table_line_head_ = line_head_;
        pre_table_line_count_ = line_count_;
        pre_table_line_id_ = line_id_;
        pre_table_line_starts_count_ = line_starts_->get_count();
        pre_table_div_list_count_ = div_list_.get_count();
        pre_table_cur_div_ = cur_div_;
        pre_table_max_line_width_ = max_line_width_;
        pre_table_avail_line_width_ = avail_line_width_;
        pre_table_line_spacing_ = line_spacing_;
        pre_table_last_was_newline_ = last_was_newline_;

        /* we're now in pass 1 */
        table_pass_ = 1;

        /*
         *   In case we try to refresh the display while we're working on the
         *   table, "hide" everything in the table under construction by
         *   pushing it all off the screen below.  
         */
        if (win_ != 0)
            y_ofs = win_->get_disp_height() * 2;
        break;

    case 1:
        /*
         *   We're doing the first pass through a table, so this must be a
         *   nested table.  There's nothing extra to do at this point.  
         */
        break;

    case 2:
        /*
         *   If this is a floating table, put it at the start of a new line.
         *   This is unnecessary for regular tables, since they're formatted
         *   as paragraph breaks anyway, but for floating tables we need to
         *   make sure we're at the start of a line in order for everything
         *   to line up properly.  Note that we need to do this before
         *   calculating the table size information, so that the table is
         *   initially positioned under the same conditions as its contents,
         *   which will always effectively start on new lines since each new
         *   cell is considered a new line.  
         */
        if (table_tag->is_floating())
        {
            CHtmlRect pos;

            /* 
             *   momentarily restore the enclosing table as the active
             *   table, so that we can end any margin items at the enclosing
             *   level before starting this new table 
             */
            current_table_ = enclosing_table;

            /* 
             *   push a snapshot of the current environment, in case it
             *   changes - we'll need to restore this information when we
             *   finish with the table so that we can resume formatting the
             *   line under construction under the same conditions 
             */
            push_table_env();

            /* start a new line here, in the context of the enclosing table */
            start_new_line(table_disp);

            /* 
             *   make sure that display item is operating with the full
             *   window width available, in case we changed the margins
             *   because of floating items 
             */
            table_disp->set_win_width(win_->get_disp_width()
                                      - margin_left_ - margin_right_);

            /* restore the current table tag */
            current_table_ = table_tag;
        }

        /*
         *   We're doing the second pass through a table.  All of the
         *   horizontal metrics of the table are known on this pass, so
         *   tell the display item to calculate the actual sizes of the
         *   columns and thus the actual size of the table.  
         */
        table_disp->calc_table_size(this, table_tag);
        break;
    }
        
    /*
     *   If the caption is to be set at the top of the table, format its
     *   contents here.  
     */
    check_format_caption(caption_tag, table_disp, TRUE);

    /*
     *   Add the display item to the display list directly (we can bypass the
     *   normal line formatting, since a table is never part of a line -
     *   tables are vertical blocks).  
     */
    add_to_disp_list(table_disp);

    /* 
     *   If there's nothing in the line list yet, make the new table the
     *   first thing on the line.  This ensures that we'll position it
     *   properly when we close the cell.
     *   
     *   Likewise, if it's a floating table, it's the start of its own line.
     */
    if (line_head_ == 0 || table_tag->is_floating())
        line_head_ = table_disp;

    /*
     *   Remember the output position at the start of the table, since we'll
     *   want to come back here at the end of the table.  Let the caller
     *   remember where the enclosing table began, so that we can restore
     *   the enclosing table start position when we end this table.  
     */
    *enclosing_table_pos = pre_table_curpos_;
    pre_table_curpos_ = curpos_;

    /* adjust the output position for the "hiding" y offset, if needed */
    curpos_.y += y_ofs;

    /* return the enclosing table */
    return enclosing_table;
}

/*
 *   End a table 
 */
void CHtmlFormatter::end_table(CHtmlDispTable *table_disp,
                               CHtmlTagTABLE *enclosing_table,
                               const CHtmlPoint *enclosing_table_pos,
                               CHtmlTagCAPTION *caption_tag)
{
    CHtmlTagTABLE *ending_table;
    
    /* 
     *   If there is no current table, there's nothing to do.  This can
     *   happen when we try to exit formatting of a table incorrectly or
     *   prematurely; if we just ignore it, we will usually come back and try
     *   again at a more appropriate time later.  
     */
    if (current_table_ == 0)
        return;
    
    /* remember the current table as long as we're working on it */
    ending_table = current_table_;
    
    /* restore the enclosing table as the current table */
    current_table_ = enclosing_table;

    switch(table_pass_)
    {
    case 1:
        /*
         *   First pass.  If there's an enclosing table, we simply need to
         *   restore the enclosing table as the current table; otherwise,
         *   we need to run table formatting pass 2.  Note that, in the
         *   case of a nested table, running the second pass on the
         *   enclosing table will also run the second pass on all nested
         *   tables; furthermore, we can't run the second pass on embedded
         *   tables until we've finished pass 1 on the enclosing table,
         *   because the metrics of the embedded tables depend on the
         *   metrics of the enclosing table, which aren't known until the
         *   end of pass 1 on the enclosing table.  Therefore, we can't
         *   run pass 2 on embedded tables individually; we can only run
         *   pass 2 when we've finished pass 1 on the outermost table.  
         */
        if (enclosing_table == 0)
        {
            /* time for pass two */
            run_table_pass_two(ending_table);

            /* 
             *   our table's display item will have changed, since we will
             *   have reformatted the table on the second pass - get the new
             *   table display item from the table tag 
             */
            table_disp = ending_table->get_disp();
        }
        else
        {
            /* 
             *   insert the embedded table's caption if appropriate, to
             *   ensure that it's counted in the sizes of the embedded
             *   items 
             */
            check_format_caption(caption_tag, table_disp, FALSE);
            
            /* restore starting position of enclosing table */
            pre_table_curpos_ = *enclosing_table_pos;
        }

        /* done */
        break;

    case 2:
        /*
         *   Second pass.  Tell the table to calculate the row sizes, then
         *   set the final vertical positions of the rows.  Note that the
         *   operations must be performed in sequence, since we must
         *   compute not only the main table's height, but also the
         *   heights of any nested tables, before we can even begin
         *   setting final row positions.  
         */
        ending_table->compute_table_height();
        ending_table->set_row_positions(this);

        /*
         *   Insert the caption, if it's meant to be at the bottom of the
         *   table 
         */
        check_format_caption(caption_tag, table_disp, FALSE);

        /*
         *   If it's a floating item, remove it from the stream and add it
         *   back in for deferred placement.  
         */
        if (ending_table->is_floating())
        {
            CHtmlDisp *sub_head;

            /*
             *   Pull out everything after the table's initial item as a
             *   separate list; this gives us the list of all of the display
             *   items within the table.  We don't defer this list in the
             *   usual way - we instead attach it to the table, so that the
             *   table can add it to the format list when the table itself
             *   is formatted.
             *   
             *   We don't want to defer the table contents, because they're
             *   not floating items themselves - they're just contained in a
             *   floating item.  
             */
            sub_head = table_disp->get_next_disp();

            /* detach the contents list from the table item itself */
            table_disp->clear_next_disp();

            /* attach the contents list to the table as its sublist */
            table_disp->set_contents_sublist(sub_head);

            /* 
             *   restore the formatting environment information we saved at
             *   the start of pass 2 for this table 
             */
            pop_table_env();

            /* add the table back to the list as a deferred item */
            add_disp_item_deferred(table_disp);
        }
        else
        {
            long table_right;

            /* 
             *   the table is added to its own vertical space, so check to
             *   see if we're at the end of any floating area 
             */
            check_flow_end();

            /* 
             *   if this leaves us at the outermost table level (i.e., we
             *   just finished with a top-level table), adjust the horizontal
             *   extent according to the table's position and width 
             */
            table_right = table_disp->get_pos().right;
            if (current_table_ == 0 && table_right > outer_max_line_width_)
            {
                /* remember the outer-level max line width */
                outer_max_line_width_ = table_right;

                /* adjust the window's horizontal scrollbar */
                win_->fmt_adjust_hscroll();
            }

            /* 
             *   Now that we have the final table position, invalidate its
             *   entire area.  We need to invalidate the area of the table
             *   itself, because there might be empty space (for borders,
             *   spacing, and padding) that cells will have left untouched,
             *   but which we nonetheless have to draw to ensure our
             *   background is displayed properly. 
             */
            CHtmlRect pos = table_disp->get_pos();
            win_->inval_doc_coords(&pos);
        }

        /* done with pass 2 */
        break;
    }
}

/*
 *   Receive information on the total size of the table.  After we've
 *   formatted the table, we'll use this to set the position of the next
 *   item after the table.  
 */
void CHtmlFormatter::end_table_set_size(CHtmlTagTABLE * /*enclosing_table*/,
                                        long height,
                                        const CHtmlPoint *enclosing_table_pos)
{
    /* 
     *   set the current position to the position at the start of the
     *   table offset vertically by the height of the table. 
     */
    curpos_ = pre_table_curpos_;
    curpos_.y += height;

    /* set the maximum y position based to the bottom of the table */
    layout_max_y_pos_ = curpos_.y;

    /* make the display height at least the layout height */
    if (layout_max_y_pos_ > disp_max_y_pos_)
        disp_max_y_pos_ = layout_max_y_pos_;

    /* restore the position of the enclosing table */
    pre_table_curpos_ = *enclosing_table_pos;
}

/*
 *   Format the caption tag if we're at the proper point in the table.
 *   'top' is true if we're at the top of the table, false if at the
 *   bottom.  This routine should be called once just before starting the
 *   table and again just after finishing it, to ensure that the caption
 *   is inserted at the proper point.  
 */
void CHtmlFormatter::check_format_caption(CHtmlTagCAPTION *caption_tag,
                                          CHtmlDispTable *table_disp,
                                          int top)
{
    /* 
     *   if we have a caption, and we're at the right point, format the
     *   caption 
     */
    if (caption_tag != 0
        && (caption_tag->is_at_top_of_table() != 0) == (top != 0))
    {
        CHtmlRect pos;
        long left, right;

        /* get margins for the table */
        pos = table_disp->get_pos();
        left = pos.left;
        right = win_->get_disp_width()
                - (left + (pos.right - pos.left));

        /* 
         *   offset everything by the left margin if we're at the top of
         *   the table, since we haven't set the table display item's
         *   final line position yet 
         */
        if (top)
        {
            left += margin_left_;
            right -= margin_left_;
        }

        /* set the margins */
        push_margins(left, right);

        /* format the caption */
        caption_tag->format_caption(win_, this);

        /* restore outer margins */
        pop_margins();
    }
}

/*
 *   Run the second table formatting pass on the current table 
 */
void CHtmlFormatter::run_table_pass_two(CHtmlTagTABLE *table_tag)
{
    CHtmlTag *tag;
    
    /*
     *   First, delete the display list that resulted from the first table
     *   formatting pass.  The first pass was a measuring pass; we didn't
     *   know how to lay out the table, so the display list that resulted
     *   from the first pass is not correct.  Restore the display list and
     *   formatter heap to the state they were in prior to starting the
     *   table.  
     */
    if (pre_table_disp_tail_ != 0)
        CHtmlDisp::delete_list(pre_table_disp_tail_->get_next_disp());
    heap_page_cur_ = pre_table_heap_page_cur_;
    heap_page_cur_ofs_ = pre_table_heap_page_cur_ofs_;
    disp_tail_ = pre_table_disp_tail_;
    if (disp_tail_ == 0)
    {
        /* there was nothing in the display list before the table */
        disp_head_ = 0;
    }
    else
    {
        /* clear the 'next' pointer of the previous item in the list */
        disp_tail_->clear_next_disp();
    }
    line_head_ = pre_table_line_head_;
    line_count_ = pre_table_line_count_;
    line_id_ = pre_table_line_id_;
    line_starts_->restore_count(pre_table_line_starts_count_);
    div_list_.trunc(pre_table_div_list_count_);
    cur_div_ = pre_table_cur_div_;
    max_line_width_ = pre_table_max_line_width_;
    avail_line_width_ = pre_table_avail_line_width_;

    /* go back to the position of the start of the table */
    curpos_ = pre_table_curpos_;
    last_was_newline_ = pre_table_last_was_newline_;
    line_spacing_ = pre_table_line_spacing_;

    /*
     *   Begin pass two 
     */
    table_pass_ = 2;

    /*
     *   Run through the format tag list again, starting with the first tag
     *   in the table and continuing until we reach the current tag.
     *   
     *   The behavior of tag formatting on this pass will be largely the
     *   same as it was on the first pass, except that the table tags will
     *   see that they're in pass two, and so will format their contents at
     *   the proper sizes.  So, the results of this second pass will be
     *   directly usable for displaying.  In addition, since we're in pass
     *   two, we won't try to run pass two again when we finish with the
     *   table.
     *   
     *   Note that we want to format tags until we get to the present
     *   curtag_, and then format that tag as well - the enclosing tag
     *   scanning loop will have last set curtag_ to the final tag within
     *   the table, and clearly we need to format that tag again before we
     *   finish.  
     */
    for (tag = table_tag ; tag != 0 ; )
    {
        CHtmlTag *nxt;
        
        /* format this tag */
        tag->format(win_, this);

        /* get the next tag in format order */
        nxt = tag->get_next_fmt_tag(this);

        /* 
         *   if we just did the 'current' tag in the enclosing formatting
         *   loop, it means that we have just reformatted the last tag in
         *   the table, so we're done with the reformat 
         */
        if (tag == curtag_)
            break;

        /* move to the next tag */
        tag = nxt;
    }

    /*
     *   Done with the table.  Reset the table pass counter to zero to
     *   indicate that we're no longer formatting a table. 
     */
    table_pass_ = 0;
}

/*
 *   Push the formatting environment for later restoration 
 */
void CHtmlFormatter::push_table_env()
{
    /* if we have room in the stack, store the environment */
    if (table_env_save_sp_ < CHTMLFMT_TABLE_ENV_SAVE_MAX)
    {
        CHtmlFmtTableEnvSave *p;

        /* get a pointer to the stack slot */
        p = &table_env_save_[table_env_save_sp_];

        /* save the information at this level */
        p->left_count = left_flow_stk_.get_count();
        p->right_count = right_flow_stk_.get_count();
        p->margin_left = margin_left_;
        p->margin_right = margin_right_;
        p->disp_tail = disp_tail_;
        p->line_head = line_head_;
        p->max_line_width = max_line_width_;
        p->avail_line_width = avail_line_width_;
        p->curpos = curpos_;
        p->line_spacing = line_spacing_;
        p->last_was_newline = last_was_newline_;
        p->line_count = line_count_;
        p->line_id = line_id_;
        p->line_starts_count = line_starts_->get_count();
    }

    /* 
     *   consume the stack level - note that we do this even if we can't
     *   store anything at this level (due to having exhausted the stack),
     *   since this way we at least keep track of the nesting depth even if
     *   we go beyond what we can store 
     */
    ++table_env_save_sp_;
}

/*
 *   Pop a formatting environment 
 */
void CHtmlFormatter::pop_table_env()
{
    CHtmlFmtTableEnvSave *p;

    /* if we have nothing to pop, ignore it */
    if (table_env_save_sp_ == 0)
        return;

    /* pop a level */
    --table_env_save_sp_;

    /* 
     *   if this level is beyond the maximum number of levels we can store,
     *   there's nothing stored, so we can return without any further work 
     */
    if (table_env_save_sp_ >= CHTMLFMT_TABLE_ENV_SAVE_MAX)
        return;

    /* get a pointer to the stack slot */
    p = &table_env_save_[table_env_save_sp_];

    /*
     *   If the left or right flow stacks have had items removed since we
     *   saved the environment, add an item back in as necessary to restore
     *   the equivalent environment.
     *   
     *   Note that we don't have to restore the actual items or even the
     *   actual stack depth from the old environment; what's important is
     *   that we restore the *equivalent* environment, which for our
     *   purposes merely provides an item on each stack as needed to take
     *   margins from where they were before to where they are now as soon
     *   as we start a new line.  Note that the y position of each new flow
     *   stack item can be zero, since we merely need to set things up to
     *   pop this level at the end of the next line.  
     */
    if (p->left_count > left_flow_stk_.get_count())
        left_flow_stk_.push(margin_left_ - p->margin_left,
                            0, current_table_);
    if (p->right_count > right_flow_stk_.get_count())
        right_flow_stk_.push(margin_right_ - p->margin_right,
                             0, current_table_);

    /* 
     *   restore the left and right margins as they were when we saved this
     *   flow stack environment 
     */
    margin_left_ = p->margin_left;
    margin_right_ = p->margin_right;

    /* 
     *   remove everything that we formatted after saving the environment -
     *   the caller will have saved the new stuff before calling us and will
     *   add it back in when appropriate 
     */
    disp_tail_ = p->disp_tail;
    if (disp_tail_ == 0)
        disp_head_ = 0;
    else
        disp_tail_->clear_next_disp();

    /* go back to the output position as it was when saved */
    line_head_ = p->line_head;
    max_line_width_ = p->max_line_width;
    avail_line_width_ = p->avail_line_width;
    curpos_ = p->curpos;
    line_spacing_ = p->line_spacing;
    last_was_newline_ = p->last_was_newline;
    line_count_ = p->line_count;
    line_id_ = p->line_id;
    line_starts_->restore_count(p->line_starts_count);
}

/*
 *   Begin a table cell.  On pass 2, this will set the formatter's margins
 *   according to the table's column area. 
 */
void CHtmlFormatter::begin_table_cell(CHtmlDispTableCell *cell_disp,
                                      long cell_x_offset,
                                      long content_x_offset,
                                      long content_width)
{
    long right_margin;
    CHtmlRect pos;
    long wid;
    
    switch(table_pass_)
    {
    case 2:
        /*
         *   Start a new line.  This will ensure that the items in the
         *   previous cell are fully contained in the previous cell. 
         */
        start_new_line(cell_disp);

        /* 
         *   remember the current position, so that when we're done with
         *   the cell we can calculate the height of the cell 
         */
        curpos_ = pre_table_curpos_;
        
        /* set the horizontal position of the cell */
        pos = cell_disp->get_pos();
        wid = pos.right - pos.left;
        pos.left = margin_left_ + cell_x_offset;
        pos.right = pos.left + wid;
        cell_disp->set_pos(&pos);

        /*
         *   This is the second pass, so the column's position and size
         *   have been finalized.  Set up the margins so that the
         *   following text will be set inside the column's horizontal
         *   extent.  
         */
        right_margin = win_->get_disp_width()
                       - (margin_left_
                          + content_x_offset + content_width);
        push_margins(margin_left_ + content_x_offset, right_margin);
        break;

    default:
        /* there's nothing to do on other passes */
        break;
    }

    /* 
     *   Add the cell to the display list.  Don't format the display item
     *   at all, since it doesn't contribute to the line. 
     */
    add_to_disp_list(cell_disp);
}

/*
 *   Format all pending items for a table cell.  This will ensure that all
 *   pending deferred items have been added to the list.
 */
void CHtmlFormatter::pre_end_table_cell()
{
    /* end the current line */
    add_disp_item_new_line(new (this) CHtmlDispBreak(0));
    
    /* clear any floating items currently in the margins */
    break_to_clear(TRUE, TRUE);

    /* if we have any deferred items remaining, add them all now */
    while (defer_head_ != 0)
    {
        /* 
         *   Add the deferred items, forcing the addition of all of the
         *   items even if they don't fit.  
         */
        add_all_deferred_items(0, 0, TRUE);

        /* move past them */
        add_disp_item_new_line(new (this) CHtmlDispBreak(0));
        break_to_clear(TRUE, TRUE);
    }
}

/*
 *   End the current table cell.  On table pass 1, this will calculate the
 *   minimum and maximum width of the cell's contents, and tell the cell
 *   about it.  
 */
void CHtmlFormatter::end_table_cell(CHtmlTagTableCell *cell_tag,
                                    CHtmlDispTableCell *cell_disp)
{
    CHtmlDisp *disp;
    CHtmlTableMetrics metrics;

    /*
     *   See which pass we're on 
     */
    switch(table_pass_)
    {
    case 1:
        /*
         *   First pass: this pass measures the contents of each table
         *   cell so that we can determine the metrics of the overall
         *   table.
         */

        /* if there's no cell display item, there's nothing to calculate */
        if (cell_disp == 0)
            break;

        /*
         *   Run through all of the items in the display list, starting
         *   with the item after the cell item itself, and have them
         *   calculate their contributions to the minimum and maximum
         *   widths.  
         */
        for (metrics.clear(), disp = cell_disp->get_next_disp() ; disp != 0 ;
             disp = disp->get_next_disp())
            disp->measure_for_table(win_, &metrics);

        /* make sure we flush out the current line */
        metrics.start_new_line();

        /* tell the cell the results of the measurements */
        cell_tag->set_table_cell_metrics(&metrics);
        break;

    case 2:
        /*
         *   Second pass: restore the margins that were in effect before
         *   we started the cell.  
         */
        pop_margins();

        /*
         *   Calculate the height of the cell, and tell the cell about it.
         *   The cell will use this to figure its contribution to the
         *   total row height. 
         */
        cell_tag->set_table_cell_height(curpos_.y - pre_table_curpos_.y,
                                        pre_table_curpos_.y);
        break;
    }
}

/*
 *   Set the y positions of the contents of a table cell.  y_offset is an
 *   offset from the top of the table.  disp_first and disp_last are the
 *   first and last display items in the cell.  base_y_pos is the original
 *   (tentative) vertical position of the cell's display items; we'll keep
 *   the offset from the final position for each item the same as its
 *   offset from this position.  
 */
void CHtmlFormatter::set_table_cell_y_pos(CHtmlDisp *disp_first,
                                          CHtmlDisp *disp_last,
                                          long y_offset, long base_y_pos,
                                          long row_y_offset)
{
    CHtmlDisp *cur;
    CHtmlRect pos;
    long ht;

    /*
     *   Run through the display list from first to last, and set each
     *   item's vertical position.  Note that we skip the very first item,
     *   since it's always the cell; we set the cell's position
     *   separately.  Note also that if there is no last display item
     *   (disp_last == 0), the cell must have no contents at all, hence we
     *   don't need to do any work here.  
     */
    if (disp_last != 0)
    {
        for (cur = disp_first->get_next_disp() ;
             cur != 0 && cur != disp_last ; cur = cur->get_next_disp())
        {
            /* get the old position */
            pos = cur->get_pos();
            
            /* calculate the new position */
            ht = pos.bottom - pos.top;
            pos.top = pre_table_curpos_.y + y_offset + (pos.top - base_y_pos);
            pos.bottom = pos.top + ht;
            
            /* set the new position */
            cur->set_pos(&pos);
        }
    }

    /*
     *   Set the y position of the display item for the cell itself to the
     *   top of the row 
     */
    pos = disp_first->get_pos();
    ht = pos.bottom - pos.top;
    pos.top = pre_table_curpos_.y + row_y_offset;
    pos.bottom = pos.top + ht;
    disp_first->set_pos(&pos);
}

/*
 *   Start playing a sound 
 */
void CHtmlFormatter::play_sound(CHtmlTagSOUND *tag,
                                HTML_Attrib_id_t layer,
                                class CHtmlResCacheObject *res,
                                int repeat_count, int random_start,
                                HTML_Attrib_id_t sequence)
{
    CHtmlSoundQueue *queue;

    /* 
     *   if the sound's generation is the same as the current generation, we
     *   can ignore the sound, since it's already been played during this
     *   generation 
     */
    if (tag->get_gen_id() == sound_gen_id_)
        return;

    /* mark the sound as having been played in this generation */
    tag->set_gen_id(sound_gen_id_);

    /* remember the last sound tag */
    set_last_sound_tag(tag);

    /* choose the appropriate queue based on the layer */
    switch(layer)
    {
    case HTML_Attrib_background:
        queue = sound_queues_[0];
        break;

    case HTML_Attrib_bgambient:
        queue = sound_queues_[1];
        break;

    case HTML_Attrib_ambient:
        queue = sound_queues_[2];
        break;

    case HTML_Attrib_foreground:
        queue = sound_queues_[3];
        break;

    default:
        /* invalid - ignore the request */
        return;
    }

    /* if the sound queue isn't set up, ignore the request */
    if (queue == 0)
        return;

    /* play the sound */
    queue->add_sound(res, repeat_count, random_start, sequence);
}

/*
 *   Cancel the current sound in a given layer 
 */
void CHtmlFormatter::cancel_sound(HTML_Attrib_id_t layer)
{
    /* check the layer */
    switch(layer)
    {
    case HTML_Attrib_background:
        if (sound_queues_[0] != 0)
            sound_queues_[0]->stop_current_sound(FALSE);
        break;

    case HTML_Attrib_bgambient:
        if (sound_queues_[1] != 0)
            sound_queues_[1]->stop_current_sound(FALSE);
        break;

    case HTML_Attrib_ambient:
        if (sound_queues_[2] != 0)
            sound_queues_[2]->stop_current_sound(FALSE);
        break;

    case HTML_Attrib_foreground:
        if (sound_queues_[3] != 0)
            sound_queues_[3]->stop_current_sound(FALSE);
        break;

    default:
        /* cancel sound in all layers */
        {   
            int i;
            
            for (i = 0 ; i < 4 ; ++i)
            {
                if (sound_queues_[i] != 0)
                    sound_queues_[i]->stop_current_sound(FALSE);
            }
        }
        break;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger source window formatter 
 */

/*
 *   start formatting at the start of the document 
 */
void CHtmlFormatterSrcwin::start_at_top(int reset_sounds)
{
    /* inherit default */
    CHtmlFormatter::start_at_top(reset_sounds);

    /* ask the parser for the first tag in the document */
    prvtag_ = 0;
    curtag_ = parser_->get_outer_container();
}


/* ------------------------------------------------------------------------ */
/*
 *   Stand-alone window formatter 
 */

/*
 *   start formatting at the start of the document 
 */
void CHtmlFormatterStandalone::start_at_top(int reset_sounds)
{
    /* inherit default */
    CHtmlFormatter::start_at_top(reset_sounds);

    /* ask the parser for the first tag in the document */
    prvtag_ = 0;
    curtag_ = parser_->get_outer_container();
}

/* ------------------------------------------------------------------------ */
/*
 *   Main formatter implementation 
 */

CHtmlFormatterMain::CHtmlFormatterMain(CHtmlParser *parser)
    : CHtmlFormatter(parser)
{
    /* no banners yet */
    banners_ = 0;
}

CHtmlFormatterMain::~CHtmlFormatterMain()
{
    /* 
     *   Delete the banner list.  Do this before deleting the display
     *   list, since banners may refer to tags in the display list. 
     */
    remove_all_banners(FALSE);

    /* 
     *   Delete any display list.  Note that we need to do this here, even
     *   though it's also done in our base class destructor, because we
     *   need to run the *overridden* version of the display list deleter,
     *   and by the time the base class destructor is called it will be
     *   too late to do that.  So, run it here, too; it's harmless to call
     *   it twice.  Note also that we need to delete the display list
     *   *before* deleting the resource cache, because the display list
     *   may contain references into the resource cache.  
     */
    delete_display_list();

    /* 
     *   if our window is still around, detach from it (note that we must
     *   do this BEFORE we delete the resource cache, because we cancel
     *   any active sounds in unset_win(), which will in turn delete the
     *   sounds from the cache)
     */
    if (win_ != 0)
        unset_win();
}

/*
 *   Set my window
 */
void CHtmlFormatterMain::set_win(class CHtmlSysWin *win, const CHtmlRect *r)
{
    int i;
    CHtmlSoundQueue *prv;
    
    /* inherit default */
    CHtmlFormatter::set_win(win, r);

    /* 
     *   Create our sound queues - one each for the background, background
     *   ambient, ambient, and foreground layers.  Note that we start with
     *   the background queue and move through ambient to the foreground, so
     *   that for each new queue, the previous queue created is the next one
     *   in the background.
     *   
     *   The sound queues are global, but we manage them as part of the main
     *   text formatter, because this formatter is always around.  
     */
    for (prv = 0, i = 0 ; i < 4 ; ++i)
    {
        /* create this sound queue */
        sound_queues_[i] = new CHtmlSoundQueue(win, prv);

        /* this is the background queue for the next queue */
        prv = sound_queues_[i];
    }
}

/*
 *   Receive notification that the window is about to be deleted.  Our
 *   sound queues depend on the window, so we need to delete the sound
 *   queues before the window is deleted.  
 */
void CHtmlFormatterMain::unset_win()
{
    int i;

    /* delete the sound queues */
    for (i = 0 ; i < 4 ; ++i)
    {
        if (sound_queues_[i] != 0)
        {
            delete sound_queues_[i];
            sound_queues_[i] = 0;
        }
    }

    /* inherit default */
    CHtmlFormatter::unset_win();
}

/*
 *   Prepare to delete the parser.  We'll forget about any banner tags
 *   that we're keeping a pointer to, since those tags will be deleted
 *   when the parser is deleted. 
 */
void CHtmlFormatterMain::release_parser()
{
    CHtmlFmtBannerItem *cur;

    /* go through all of our banners */
    for (cur = banners_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* forget any BANNER tags we're keeping pointers to */
        cur->last_tag_ = 0;
        cur->first_tag_ = 0;

        /* tell the subformatter to release the parser as well */
        if (cur->subformatter_ != 0)
            cur->subformatter_->release_parser();
    }

    /* inherit default */
    CHtmlFormatter::release_parser();
}

/*
 *   start formatting at the start of the document 
 */
void CHtmlFormatterMain::start_at_top(int reset_sounds)
{
    CHtmlFmtBannerItem *cur;

    /* inherit default */
    CHtmlFormatter::start_at_top(reset_sounds);
    
    /* ask the parser for the first tag in the document */
    prvtag_ = 0;
    curtag_ = parser_->get_outer_container();

    /* no title */
    title_buf_.set("");

    /* no sound tags yet */
    last_sound_tag_ = 0;

    /* 
     *   Reset the heights of all of the banner subwindows, since we may
     *   be reformatting with some new metric that would affect the banner
     *   heights (for example, the font size may have changed).  In
     *   addition, forget the previous tags of the banners, since these
     *   are no longer valid.  
     */
    for (cur = banners_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* no height set yet */
        cur->height_set_ = FALSE;

        /* no tags seen on this run */
        cur->last_tag_ = 0;
        cur->first_tag_ = 0;
        cur->is_first_ = TRUE;

        /* the banner doesn't have a start tag yet */
        cur->subformatter_->set_banner_start_tag(0);
    }
}

/*
 *   add a display item 
 */
void CHtmlFormatterMain::add_disp_item(CHtmlDisp *item)
{
    /* if we're in title mode, add the text to the title buffer */
    if (title_mode_)
        item->add_text_to_buf(&title_buf_);

    /* inherit default behavior */
    CHtmlFormatter::add_disp_item(item);
}

/*
 *   End the title.  Sets the window title from the contents of the title
 *   buffer.  
 */
void CHtmlFormatterMain::end_title(CHtmlTagTITLE *tag)
{
    /* note we're no longer in title mode */
    title_mode_ = FALSE;

    /* set the window's caption to the contents of the title buffer */
    win_->set_window_title(title_buf_.get());

    /* set the title text in the TITLE tag */
    tag->set_title_text(title_buf_.get());

    /* forget the title buffer contents */
    title_buf_.set("");
}

/*
 *   Begin a banner
 */
int CHtmlFormatterMain::
   format_banner(const textchar_t *banner_id,
                 long height, int height_set, int height_prev, int height_pct,
                 long width, int width_set, int width_prev, int width_pct,
                 CHtmlTagBANNER *banner_tag)
{
    CHtmlFmtBannerItem *cur;
    CHtmlFormatterBannerSub *subformatter;
    CHtmlSysWin *subwin;
    CHtmlSysFrame *frame;
    HTML_BannerWin_Units_t width_units;
    HTML_BannerWin_Units_t height_units;
        
    /* get the frame object; if there isn't one, we can't proceed */
    if ((frame = CHtmlSysFrame::get_frame_obj()) == 0)
        return FALSE;

    /* 
     *   If we're in T3 mode, and the OS layer supports the banner API, don't
     *   allow the <BANNER> tag - the caller must use the banner API instead.
     */
    if (is_t3_mode() && !banner_tag->is_about_box())
    {
        long result;
        
        /* 
         *   Get information on banner API support from the OS.  If the API
         *   is supported, then don't allow <BANNER>. 
         */
        if (os_get_sysinfo(SYSINFO_BANNERS, 0, &result) && result != 0)
        {
            /* generate a debug message to explain what's wrong */
            oshtml_dbg_printf("<BANNER> is not allowed in TADS 3 - use "
                              "the banner API instead\n");

            /* indicate failure */
            return FALSE;
        }
    }

    /* 
     *   for our purposes, act as though the width and height haven't been
     *   set if we're being asked to use a previous width and height 
     */
    if (height_prev)
        height_set = FALSE;
    if (width_prev)
        width_set = FALSE;

    /* if the width or height aren't set, they're not percentages */
    if (!width_set)
        width_pct = FALSE;
    if (!height_set)
        height_pct = FALSE;

    /* use pixels or percentage units, as appropriate */
    width_units = (width_pct ? HTML_BANNERWIN_UNITS_PCT
                             : HTML_BANNERWIN_UNITS_PIX);
    height_units = (height_pct ? HTML_BANNERWIN_UNITS_PCT
                               : HTML_BANNERWIN_UNITS_PIX);
    
    /*
     *   See if we can find an existing banner with the same name.  If so,
     *   re-use its window space. 
     */
    for (cur = banners_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* is this a match? */
        if (cur->matches(banner_id, banner_tag->is_about_box()))
        {
            /* stop looking */
            break;
        }
    }

    /* see if we found a match */
    if (cur != 0)
    {
        unsigned long style;

        /* found it - reuse the existing banner */
        subformatter = cur->subformatter_;
        subwin = subformatter->get_win();

        /* 
         *   if there's no window, it means we've closed the UI, which means
         *   the app is terminating; no need to proceed 
         */
        if (subwin == 0)
            return FALSE;

        /* 
         *   make sure we redraw the window, since we're replacing its
         *   entire contents 
         */
        subformatter->inval_window();

        /* set the style flags */
        style = 0;
        if (banner_tag->has_border())
            style |= OS_BANNER_STYLE_BORDER;

        /* 
         *   update the banner settings, in case they've changed in the
         *   new tag 
         */
        subwin->set_banner_info(banner_tag->get_alignment(), style);

        /* 
         *   we can't use the previous height if the height hasn't been
         *   set yet, or has been reset due to a full reformat; likewise
         *   for the width 
         */
        if (!cur->height_set_)
            height_prev = FALSE;
        if (!cur->width_set_)
            width_prev = FALSE;

        /*
         *   If this banner was previously used by another tag earlier in
         *   this formatting list, and the last tag wasn't the first
         *   instance of the banner, we can tell the old tag to skip
         *   formatting on future passes through the formatting list,
         *   since the new tag will always override the previous tag.  (We
         *   need to keep the first instance around, so that the banner is
         *   re-created in the proper order relative to other banners if
         *   the window should be reformatted from scratch.)
         */
        if (cur->last_tag_ != 0 && !cur->is_first_)
            cur->last_tag_->notify_obsolete(get_text_array());

        /* if we have a previous instance, this new one isn't the first */
        if (cur->last_tag_ != 0)
            cur->is_first_ = FALSE;
    }
    else
    {
        unsigned long style;

        /*
         *   This banner doesn't exist yet.  Create a new banner
         *   subformatter to format the banner contents 
         */
        subformatter = new CHtmlFormatterBannerSub(parser_, this, banner_tag);

        /* set up the style flags */
        style = 0;
        if (banner_tag->has_border())
            style |= OS_BANNER_STYLE_BORDER;

        /*
         *   Ask the window to create a banner subwindow.  If this operation
         *   fails, the window either doesn't allow banners or is not able to
         *   create a new banner; in any case, ignore the banner entirely if
         *   we can't create a banner window.  Note that the <BANNER> tag can
         *   only be used to create banners within the main window, never as
         *   sub-banners of other banners, so the parent is null to indicate
         *   the new banner is a child of the main text area.  
         */
        subwin = (banner_tag->is_about_box()
                  ? frame->create_aboutbox_window(subformatter)
                  : frame->create_banner_window(0, HTML_BANNERWIN_BANNERTAG,
                                                subformatter,
                                                OS_BANNER_LAST, 0,
                                                banner_tag->get_alignment(),
                                                style));
        if (subwin == 0)
        {
            /* delete the sub-formatter, since we won't be needing it */
            delete subformatter;

            /* indicate that we couldn't format the banner */
            return FALSE;
        }

        /* create a new list entry for this item */
        cur = new CHtmlFmtBannerItem(banner_id, banner_tag->is_about_box(),
                                     subformatter);

        /* link it at the head of the list */
        cur->nxt_ = banners_;
        banners_ = cur;

        /* 
         *   if HEIGHT=PREVIOUS or WIDTH=PREVIOUS were specified, ignore
         *   them, since there wasn't a previous height or width to use -
         *   use the natural window size (i.e, the height and width of the
         *   contents) instead 
         */
        height_prev = FALSE;
        width_prev = FALSE;
    }

    /* 
     *   set default color scheme, in case the banner doesn't have a BODY
     *   tag anywhere 
     */
    subwin->set_html_bg_color(0, TRUE);
    subwin->set_html_text_color(0, TRUE);
    subwin->set_html_link_colors(0, TRUE, 0, TRUE, 0, TRUE, 0, TRUE);

    /* tell the subformatter where to begin */
    subformatter->set_banner_start_tag(banner_tag);

    /* tell the subformatter to start at the top */
    subformatter->start_at_top(FALSE);

    /* tell the subwindow to format the banner contents */
    subwin->do_formatting(FALSE, FALSE, FALSE);

    /* 
     *   initialize the window's size, now that we have its contents laid out
     *   (we must wait until after we've formatted the contents because the
     *   size of the window could depend upon the size of the contents) 
     */
    subformatter->init_size(banner_tag->get_alignment(),
                            width, width_units, width_set, width_prev,
                            height, height_units, height_set, height_prev);

    /* this item's width and height have now been set */
    cur->width_set_ = TRUE;
    cur->height_set_ = TRUE;

    /* remember that this tag formatted to the banner */
    cur->last_tag_ = banner_tag;

    /* if we don't have a first tag yet, this is the first */
    if (cur->first_tag_ == 0)
        cur->first_tag_ = banner_tag;

    /* indicate success */
    return TRUE;
}

#if 0
/*
 *   Force all open banners closed 
 */
void CHtmlFormatterMain::force_banners_closed()
{
    int changed;

    /* keep going until we fail to find an open <banner> */
    for (changed = FALSE ; ; )
    {
        CHtmlTagContainer *cur;
        int found;

        /* 
         *   scan from the current inner container outward looking for
         *   banners 
         */
        for (found = FALSE, cur = parser_->get_inner_container() ;
             cur != 0 ; cur = cur->get_container())
        {
            /* if this is a BANNER tag, note that we found one */
            if (cur->tag_name_matches("BANNER", 6))
            {
                /* note that we found a banner */
                found = TRUE;

                /* look no further */
                break;
            }
        }

        /* if we didn't find a BANNER tag, we're done */
        if (!found)
            break;

        /* close the current tag, and check again for open BANNER tags */
        parser_->close_current_tag();
    }

    /* we if changed anything, reformat */
    if (changed)
        win_->do_formatting(FALSE, FALSE, FALSE);
}
#endif

/*
 *   Remove all of the banners 
 */
void CHtmlFormatterMain::remove_all_banners(int replacing)
{
    /* remove the first banner repeatedly until we have no more banners */
    while (banners_ != 0)
        remove_banner(banners_, replacing);
}

/*
 *   Remove a banner 
 */
void CHtmlFormatterMain::remove_banner(const textchar_t *banner_id)
{
    CHtmlFmtBannerItem *cur;

    /* find the banner matching this ID */
    for (cur = banners_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* is this a match? */
        if (cur->matches(banner_id, FALSE))
        {
            /* remove it */
            remove_banner(cur, TRUE);

            /* done */
            break;
        }
    }
}

/*
 *   Remove a banner 
 */
void CHtmlFormatterMain::remove_banner(CHtmlFmtBannerItem *cur, int replacing)
{
    CHtmlFmtBannerItem *prv;
    CHtmlSysFrame *frame;

    /* get the frame object */
    frame = CHtmlSysFrame::get_frame_obj();
    
    /* 
     *   Tell the window to remove the banner -- this will destroy the
     *   window in the process.  Note that we can skip this if the window
     *   has already been deleted.  
     */
    if (cur->subformatter_->get_win() != 0 && frame != 0)
    {
        CHtmlSysWin *win;

        /* 
         *   remember the window for a moment, then dissociate the formatter
         *   from the window, since we're about to destroy the windwo 
         */
        win = cur->subformatter_->get_win();
        cur->subformatter_->unset_win();
        
        /* remove the system window */
        frame->remove_banner_window(win);
    }

    /* 
     *   If we're replacing the banner, and the banner was formatted by a
     *   tag in this same tag list, tell the tag it's no longer needed,
     *   since the banner it creates will always be destroyed.  Also
     *   notify the first instance of the tag of the same thing; we always
     *   keep around the first and last instance of a particular banner
     *   until the banner is removed, so that the banner is created in the
     *   correct order relative to other banners on reformattings of the
     *   entire list.  
     */
    if (replacing && cur->last_tag_ != 0)
    {
        /* notify the last tag that it's obsolete */
        cur->last_tag_->notify_obsolete(get_text_array());

        /* if the first tag is different, obsolete it as well */
        if (cur->first_tag_ != cur->last_tag_)
            cur->first_tag_->notify_obsolete(get_text_array());
    }

    /* delete the associated subformatter */
    delete cur->subformatter_;

    /* unlink this item from the list */
    if (cur == banners_)
    {
        /* it's at the head of the list - advance the head */
        banners_ = banners_->nxt_;
    }
    else
    {
        /* find the previous item in the list */
        for (prv = banners_ ; prv != 0 && prv->nxt_ != cur ; prv = prv->nxt_);

        /* if we found it, advance the previous item's link */
        if (prv != 0)
            prv->nxt_ = cur->nxt_;
    }

    /* delete this item -- we have no other use for it */
    delete cur;
}

/* ------------------------------------------------------------------------ */
/*
 *   Input-capable formatter 
 */

CHtmlFormatterInput::CHtmlFormatterInput(CHtmlParser *parser)
    : CHtmlFormatterMain(parser)
{
    /* there's no command input yet */
    input_pre_ = 0;
    input_tag_ = 0;
}

/*
 *   start formatting from the top of the document 
 */
void CHtmlFormatterInput::start_at_top(int reset_sounds)
{
    /* inherit default */
    CHtmlFormatterMain::start_at_top(reset_sounds);

    /* forget all the input information */
    input_pre_ = 0;
    input_tag_ = 0;
    input_head_ = 0;
    input_line_head_ = 0;
    input_line_count_ = 0;
    input_line_id_ = 0;
    freeze_display_adjust_ = FALSE;
}

/*
 *   delete the display list 
 */
void CHtmlFormatterInput::delete_display_list()
{
    /* inherit default */
    CHtmlFormatterMain::delete_display_list();

    /* forget the current input line tag */
    input_pre_ = 0;
    input_tag_ = 0;
}

/*
 *   Figure out if I have more work to do before the command input line 
 */
int CHtmlFormatterInput::more_before_input()
{
    /* if there's nothing left at all, there's nothing before the input */
    if (!more_to_do())
        return FALSE;

    /*
     *   if the current tag is at or beyond the input tag, there's nothing
     *   left to do 
     */
    if (input_tag_ != 0)
    {
        CHtmlTag *cur;

        /*
         *   scan from the input tag forward, and see if the next
         *   rendering tag is within the input line 
         */
        for (cur = input_tag_ ; cur != 0 ; cur = cur->get_next_fmt_tag(this))
        {
            /* if this is the next rendering tag, we're in the command line */
            if (cur == curtag_)
                return FALSE;
        }
    }

    /* we must have more left before the command line */
    return TRUE;
}


/*
 *   Add an input item, replacing the current input item(s) 
 */
void CHtmlFormatterInput::add_disp_item_input(CHtmlDispTextInput *item,
                                              class CHtmlTagTextInput *tag)
{
    int old_freeze;

    /* freeze display updating while we're formatting the input line */
    old_freeze = freeze_display_adjust_;
    freeze_display_adjust_ = TRUE;

    /*
     *   if the tag differs from the current one, end the old command and
     *   start a new one 
     */
    if (input_tag_ != tag)
    {
        /* if we have an input tag already, end it */
        clear_old_input();

        /* remember the new coordinating input tag */
        input_tag_ = tag;
    }

    /*
     *   if we have an active input item, unchain it from the list and set
     *   it aside 
     */
    if (input_pre_ != 0)
    {
        /* unlink it from the list */
        input_pre_->clear_next_disp();
        disp_tail_ = input_pre_;

        /*
         *   Reset our internal running total for the amount of space
         *   remaining in the current line, since we're setting a portion
         *   of the line over again.  Set the total to the same point it
         *   was at when we started the command.  
         */
        avail_line_width_ = input_avail_line_width_;

        /* back up to the y position at the start of the input */
        curpos_.y = input_y_pos_;
        curpos_.x = input_x_pos_;

        /* back up to the input line number */
        line_count_ = input_line_count_;
        line_id_ = input_line_id_;
        line_starts_->restore_count(input_line_starts_count_);

        /* restore the line head at the start of the input line */
        line_head_ = input_line_head_;

        /* forget any break position */
        breakpos_.clear();

        /* add the new item */
        add_disp_item(item);

        /* set line positions in the last line */
        set_line_positions(line_head_, 0);

        /* ensure that the scroll position is properly set */
        if (!old_freeze)
            win_->fmt_adjust_vscroll();

        /*
         *   compare the old input item list and the new input item list;
         *   if we find a point at which they differ, invalidate the
         *   display from that point forward so that we'll redraw the
         *   changes 
         */
        item->diff_input_lists(input_head_, win_);

        /* we're done with the old input display items, so delete them */
        CHtmlDisp::delete_list(input_head_);

        /* set the new input list */
        input_head_ = item;
    }
    else
    {
        /*
         *   No command is active.  Note the current command tail, so that
         *   we can replace the display item we're adding now on future
         *   changes.  
         */
        input_pre_ = disp_tail_;

        /* remember the first item of the input */
        input_head_ = item;

        /*
         *   Remember the available line width before we started on the
         *   command, since we'll have to come back to this point again
         *   whenever we reformat the input line after the UI changes the
         *   buffer contents.  Also remember the current y position and
         *   line number, since we'll need to back up here each time we
         *   reformat the line (since the input may spill over onto
         *   multiple lines).  
         */
        input_avail_line_width_ = avail_line_width_;
        input_y_pos_ = curpos_.y;
        input_x_pos_ = curpos_.x;
        input_line_count_ = line_count_;
        input_line_id_ = line_id_;
        input_line_starts_count_ = line_starts_->get_count();
        input_line_head_ = line_head_;

        /* add the input line item as normal */
        add_disp_item(item);

        /* set line positions in the last line */
        set_line_positions(line_head_, 0);

        /* make sure we redraw the new item immediately */
        item->inval(win_);

        /* ensure that the scroll position is properly set */
        if (!old_freeze)
            win_->fmt_adjust_vscroll();
    }

    /* resume normal display adjustments */
    freeze_display_adjust_ = old_freeze;
}

/*
 *   Prepare for input 
 */
void CHtmlFormatterInput::prepare_for_input()
{
    CHtmlTagContainer *cont;
    int depth;
    int close_count;

    /* 
     *   If there are any <TABLE> tags still open, close them.  This is
     *   important because the two-pass table layout algorithm makes it
     *   impossible to read input interactively in the middle of a table.  
     */
    for (depth = 1, close_count = 0, cont = parser_->get_inner_container() ;
         cont != 0 ; cont = cont->get_container(), ++depth)
    {
        /* if this is a TABLE tag, note that we have to close this far */
        if (cont->tag_name_matches("TABLE", 5))
        {
            /* it's a TABLE tag - we need to close at least to this depth */
            close_count = depth;
        }
    }

    /* close each open <TABLE> tag */
    for ( ; close_count != 0 ; --close_count)
        parser_->close_current_tag();
}

/*
 *   Begin a new command input line 
 */
class CHtmlTagTextInput
   *CHtmlFormatterInput::begin_input(const textchar_t *buf, size_t len)
{
    /* create and remember a new input item tag */ 
    input_tag_ = parser_->append_input_tag(buf, len);

    /* 
     *   if we've already formatted everything that was previously queued
     *   up, make sure we're set up to start formatting again at the new
     *   input tag - the new input tag itself won't require anything at this
     *   point, so just consider it to be the previously formatted tag 
     */
    if (curtag_ == 0)
        prvtag_ = input_tag_;

    /* return the new tag */
    return input_tag_;
}

/*
 *   Store input text temporarily in the text array 
 */
unsigned long CHtmlFormatterInput::store_input_text_temp(
    const textchar_t *txt, size_t len)
{
    /* ask the parser's text array to store the text */
    return parser_->get_text_array()->store_text_temp(txt, len);
}

/*
 *   End the current command input line 
 */
void CHtmlFormatterInput::end_input()
{
    /* clear the old input line */
    clear_old_input();

    /* consider this tag to have been formatted if desired */
    while (curtag_ != 0)
    {
        prvtag_ = curtag_;
        curtag_ = curtag_->get_next_fmt_tag(this);
    }
}

/*
 *   Clear the old command input line 
 */
void CHtmlFormatterInput::clear_old_input()
{
    /* if we have a command, commit it */
    if (input_tag_ != 0)
        input_tag_->commit(get_text_array());

    /* forget the current input line settings */
    input_tag_ = 0;
    input_head_ = 0;
    input_pre_ = 0;
    input_line_head_ = 0;
}

/*
 *   Get the position of a character in the input.  When seeking a
 *   position that is known to be in the current input buffer, this is
 *   somewhat faster than the generic get_text_pos() function, because we
 *   don't need to search beyond the input line.  
 */
CHtmlPoint
   CHtmlFormatterInput::get_text_pos_input(unsigned long txtofs) const
{
    return get_text_pos_list(input_head_, txtofs);
}

/* ------------------------------------------------------------------------ */
/*
 *   Banner formatter implementation 
 */

/*
 *   Initialize the banner window's size.  Use the sizes given, if they're
 *   specified; if one or the other size isn't specified, size the banner
 *   according to its current contents in that dimension.  
 */
void CHtmlFormatterBanner::init_size(
    HTML_BannerWin_Pos_t alignment,
    long width, HTML_BannerWin_Units_t width_units,
    int width_set, int width_prev,
    long height, HTML_BannerWin_Units_t height_units,
    int height_set, int height_prev)
{
    /* 
     *   determine the height - if it wasn't explicitly set to a value or to
     *   PREVIOUS, use the new content height 
     */
    if (!height_set && !height_prev)
    {
        /* calculate the height of the contents */
        height = calc_content_height(alignment);

        /* we're now using pixel units */
        height_units = HTML_BANNERWIN_UNITS_PIX;
    }

    /*
     *   determine the width - if it wasn't explicitly set to a value or to
     *   PREVIOUS, use the new content width 
     */
    if (!width_set && !width_prev)
    {
        /* calculate the content width */
        width = calc_content_width(alignment);

        /* we're now using pixel units */
        width_units = HTML_BANNERWIN_UNITS_PIX;
    }

    /*
     *   set the size if they provided a setting or didn't specify
     *   PREVIOUS for either dimension
     */
    win_->set_banner_size(width, width_units, width_set || !width_prev,
                          height, height_units, height_set || !height_prev);
}

/*
 *   Calculate the height of the current contents of the banner.  This is
 *   used when we wish to size the banner to its natural content height. 
 */
long CHtmlFormatterBanner::calc_content_height(HTML_BannerWin_Pos_t)
{
    /* 
     *   use the current y position of the banner's contents, and add in the
     *   bottom margin (we don't need to include the top margin, since we
     *   will have already incorporated the top margin when positioning our
     *   contents) 
     */
    return get_layout_max_y_pos() + get_bottom_margin();
}

/*
 *   Calculate the width of the current contents of the banner.  This is used
 *   when we wish to size the banner to its natural content width. 
 */
long CHtmlFormatterBanner::calc_content_width(HTML_BannerWin_Pos_t alignment)
{
    long width;
    CHtmlDisp *disp;
    CHtmlTableMetrics metrics;
    CHtmlRect submargins;

    /* find the appropriate width, according to the banner alignment */
    switch(alignment)
    {
    case HTML_BANNERWIN_POS_LEFT:
    case HTML_BANNERWIN_POS_RIGHT:
        /*   
         *   This is a vertical banner, so default to the minimum width that
         *   will work.  To determine this, run through our window's display
         *   list and ask for the banner metrics.  
         */
        for (metrics.clear(), disp = get_first_disp() ;
             disp != 0 ; disp = disp->get_next_disp())
            disp->measure_for_banner(get_win(), &metrics);
        
        /* flush the last line */
        metrics.start_new_line();
        
        /* use the minimum width from the metrics */
        width = metrics.min_width_;
        
        /* add the window's physical margins as well */
        submargins = get_phys_margins();
        width += submargins.left + submargins.right;
        break;
        
    default:
        /* 
         *   for horizontal banners, the width doesn't really matter, but get
         *   the maximum line width in case the OS layer is interested for
         *   some reason 
         */
        width = get_max_line_width();
        break;
    }

    /* return the calculated width */
    return width;
}

/* ------------------------------------------------------------------------ */
/*
 *   <BANNER> subformatter implementation
 */

/*
 *   Receive notification that we've finished formatting the banner.
 *   We'll set our stop_formatting_ flag to indicate that we shouldn't do
 *   any more formatting. 
 */
void CHtmlFormatterBannerSub::end_banner()
{
    /* note that it's time to stop formatting */
    stop_formatting_ = TRUE;

    /* make sure we clear any margin floating items */
    break_to_clear(TRUE, TRUE);

    /* end the current line */
    add_disp_item_new_line(new (this) CHtmlDispBreak(0));
}

/*
 *   start new banner contents 
 */
void CHtmlFormatterBannerSub::start_at_top(int reset_sounds)
{
    /* inherit default to clear out our formatting list */
    CHtmlFormatterBanner::start_at_top(reset_sounds);

    /* get the first tag in the banner's contents */
    curtag_ = (banner_start_tag_ == 0 ?
               0 : banner_start_tag_->get_banner_contents());
}

/* ------------------------------------------------------------------------ */
/*
 *   External banner formatter subclass 
 */

/*
 *   Create an "external" banner.  An external banner is one that is
 *   controlled from a separate output stream unrelated to the main
 *   formatter.  Returns the new formatter object.  
 */
CHtmlFormatterBannerExt *CHtmlFormatterBannerExt::create_extern_banner(
    CHtmlFormatterBannerExt *parent_fmt,
    int where, CHtmlFormatterBannerExt *other_fmt,
    int wintype, HTML_BannerWin_Pos_t alignment, int siz, int siz_units,
    unsigned long style)
{
    CHtmlFormatterBannerExt *formatter;
    CHtmlParser *parser;
    CHtmlSysWin *win;
    HTML_BannerWin_Type_t bantype;
    CHtmlSysWin *other_win;
    CHtmlSysWin *parent_win;
    CHtmlSysFrame *frame;
    
    /* get the frame object; if there isn't one, we can't proceed */
    if ((frame = CHtmlSysFrame::get_frame_obj()) == 0)
        return 0;

    /* check for a supported window type */
    switch(wintype)
    {
    case OS_BANNER_TYPE_TEXT:
        /* translate to our internal banner type */
        bantype = HTML_BANNERWIN_TEXT;

        /*
         *   This is a text stream banner, so it requires its own parser.
         *   Unlike an ordinary banner, which shares the parser with the
         *   parent formatter, an external banner gets an independent parser
         *   of its own.  
         */
        parser = new CHtmlParser();

        /* 
         *   Create the new banner formatter.  Note that we don't have to
         *   worry about deleting the parser after this point, because the
         *   formatter takes ownership of it from now on.  
         */
        formatter = new CHtmlFormatterBannerExtText(
            parent_fmt, parser, style);

        /* we're ready to create the window */
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* translate to our internal banner type */
        bantype = HTML_BANNERWIN_TEXTGRID;

        /* 
         *   A text grid banner requires the special text grid formatter.
         *   Text grids don't use parsers, since they do not support HTML. 
         */
        parser = 0;
        formatter = new CHtmlFormatterBannerExtGrid(parent_fmt, style);

        /* ready to create the window */
        break;

    default:
        /* unsupported window type */
        return 0;
    }

    /* get the parent window, if given */
    parent_win = (CHtmlSysWin *)(parent_fmt == 0 ? 0 : parent_fmt->get_win());

    /* if a relative insertion point is specified, get its window object */
    if ((where == OS_BANNER_BEFORE || where == OS_BANNER_AFTER)
        && other_fmt != 0)
        other_win = other_fmt->get_win();
    else
        other_win = 0;

    /* create the window */
    win = frame->create_banner_window(parent_win, bantype, formatter,
                                      where, other_win, alignment, style);

    /* 
     *   if we failed to create the new window, give up - the underlying
     *   display system must not support banner windows 
     */
    if (win == 0)
    {
        /* delete the formatter */
        delete formatter;

        /* return failure */
        return 0;
    }

    /* set the default color scheme in the new window */
    win->set_html_bg_color(0, TRUE);
    win->set_html_text_color(0, TRUE);
    win->set_html_link_colors(0, TRUE, 0, TRUE, 0, TRUE, 0, TRUE);

    /* set the initial size, if a non-zero size was given */
    if (siz != 0)
    {
        HTML_BannerWin_Units_t html_units;
        
        /* convert the units to our own unit system */
        switch(siz_units)
        {
        case OS_BANNER_SIZE_PCT:
        default:
            /* use a percentage-based size */
            html_units = HTML_BANNERWIN_UNITS_PCT;
            break;

        case OS_BANNER_SIZE_ABS:
            /* use the natural units, according to the window type */
            html_units = formatter->get_natural_size_units();
            break;
        }

        /* set the size, according to the alignment */
        switch(alignment)
        {
        case HTML_BANNERWIN_POS_TOP:
        case HTML_BANNERWIN_POS_BOTTOM:
            /* top/bottom banner - set the height */
            win->set_banner_size(0, html_units, FALSE, siz, html_units, TRUE);
            break;

        case HTML_BANNERWIN_POS_LEFT:
        case HTML_BANNERWIN_POS_RIGHT:
            win->set_banner_size(siz, html_units, TRUE, 0, html_units, FALSE);
            break;
        }
    }

    /* return the new window's formatter */
    return formatter;
}

/*
 *   Delete an external banner.  
 */
void CHtmlFormatterBannerExt::delete_extern_banner(
    CHtmlFormatterBannerExt *fmt)
{
    CHtmlSysFrame *frame;

    /* get the frame object */
    frame = CHtmlSysFrame::get_frame_obj();

    /* 
     *   if we have a frame, and the formatter still has a window (which it
     *   might not - the window could have been deleted first), delete the
     *   window 
     */
    if (frame != 0 && fmt->get_win() != 0)
    {
        CHtmlSysWin *win;
        
        /* 
         *   remember the window for a moment, then dissociate the formatter
         *   from the window, since we're about to destroy the window 
         */
        win = fmt->get_win();
        fmt->unset_win();
        
        /* remove the window */
        frame->remove_banner_window(win);
    }

    /* delete the formatter */
    delete fmt;
}

/*
 *   Set the size 
 */
void CHtmlFormatterBannerExt::set_banner_size(int siz, int siz_units)
{
    HTML_BannerWin_Pos_t align;
    unsigned long style;
    HTML_BannerWin_Units_t html_units;

    /* if there's no window, ignore it */
    if (get_win() == 0)
        return;

    /* get our window information (for the alignment) */
    get_win()->get_banner_info(&align, &style);

    /* convert the size units to the appropriate HTML constants */
    switch(siz_units)
    {
    case OS_BANNER_SIZE_PCT:
        /* percentage units */
        html_units = HTML_BANNERWIN_UNITS_PCT;
        break;

    case OS_BANNER_SIZE_ABS:
        /* absolute units - use our natural size */
        html_units = get_natural_size_units();
        break;

    default:
        /* invalid units - ignore it */
        return;
    }

    /* 
     *   initialize the size, keeping the existing size for each dimension we
     *   don't want to set, sizing to the contents for each dimension we do
     *   want to set 
     */
    switch(align)
    {
    case HTML_BANNERWIN_POS_TOP:
    case HTML_BANNERWIN_POS_BOTTOM:
        /* top/bottom banner - set the height */
        get_win()->set_banner_size(0, HTML_BANNERWIN_UNITS_PIX, FALSE,
                                   siz, html_units, TRUE);
        break;

    case HTML_BANNERWIN_POS_LEFT:
    case HTML_BANNERWIN_POS_RIGHT:
        /* left/right banner - set the width */
        get_win()->set_banner_size(siz, html_units, TRUE,
                                   0, HTML_BANNERWIN_UNITS_PIX, FALSE);
        break;
    }
}

/*
 *   Size to our contents 
 */
void CHtmlFormatterBannerExt::size_to_contents(int set_width, int set_height)
{
    HTML_BannerWin_Pos_t pos;
    unsigned long style;

    /* if there's no window, ignore it */
    if (get_win() == 0)
        return;

    /* get our window information (for the alignment) */
    get_win()->get_banner_info(&pos, &style);

    /* 
     *   initialize the size, keeping the existing size for each dimension we
     *   don't want to set, sizing to the contents for each dimension we do
     *   want to set 
     */
    init_size(pos, 0, HTML_BANNERWIN_UNITS_PIX, FALSE, !set_width,
              0, HTML_BANNERWIN_UNITS_PIX, FALSE, !set_height);
}


/*
 *   Calculate the height of the current contents of the banner.  This is
 *   used when we wish to size the banner to its natural content height.  
 */
long CHtmlFormatterBannerExt::calc_content_height(HTML_BannerWin_Pos_t align)
{
    size_t i;
    long ht;

    /* inherit the default to calculate our internal content height */
    ht = CHtmlFormatterBanner::calc_content_height(align);

    /* include the heights of any "strut" children */
    for (i = 0 ; i < children_.get_count() ; ++i)
    {
        CHtmlFormatterBannerExt *chi;
        
        /* get this child */
        chi = (CHtmlFormatterBannerExt *)children_.get_ele(i);

        /* if it has the "strut" style, include its height */
        if (chi->get_win() != 0)
        {
            HTML_BannerWin_Pos_t pos;
            unsigned long style;
            
            /* get the style */
            chi->get_win()->get_banner_info(&pos, &style);

            /* if it's a vertical strut, include its height */
            if ((chi->orig_style_ & OS_BANNER_STYLE_VSTRUT) != 0)
            {
                /* calculate the child's content height */
                long chi_ht = chi->calc_content_height(pos);

                /* 
                 *   If the child is a horizontal (top or bottom) banner, add
                 *   its height to our internal height, since its contents
                 *   are vertically added to our own.  If it's vertical (left
                 *   or right), use its height as a minimum, since it gets a
                 *   column slice of our space, sharing our overall height.  
                 */
                switch (pos)
                {
                case HTML_BANNERWIN_POS_TOP:
                case HTML_BANNERWIN_POS_BOTTOM:
                    /* it's horizontal - add its height to our own */
                    ht += chi_ht;
                    break;

                case HTML_BANNERWIN_POS_LEFT:
                case HTML_BANNERWIN_POS_RIGHT:
                    /* it's vertical - its height is our minimum height */
                    if (ht < chi_ht)
                        ht = chi_ht;
                    break;
                }
            }
        }
    }

    /* return the calculated height */
    return ht;
}

/*
 *   Calculate the width of the current contents of the banner.  This is used
 *   when we wish to size the banner to its natural content width.  
 */
long CHtmlFormatterBannerExt::calc_content_width(HTML_BannerWin_Pos_t align)
{
    size_t i;
    long wid;

    /* inherit the default to calculate our internal content width */
    wid = CHtmlFormatterBanner::calc_content_width(align);

    /* include the widths of any "strut" children */
    for (i = 0 ; i < children_.get_count() ; ++i)
    {
        CHtmlFormatterBannerExt *chi;

        /* get this child */
        chi = (CHtmlFormatterBannerExt *)children_.get_ele(i);

        /* if it has the "strut" style, include its width */
        if (chi->get_win() != 0)
        {
            HTML_BannerWin_Pos_t pos;
            unsigned long style;

            /* get the style */
            chi->get_win()->get_banner_info(&pos, &style);

            /* if it's a horizontal strut, include its width */
            if ((chi->orig_style_ & OS_BANNER_STYLE_HSTRUT) != 0)
            {
                /* calculate the child's content width */
                long chi_wid = chi->calc_content_width(pos);

                /* 
                 *   If the child is a vertical (left or right) banner, add
                 *   its width to our internal width, since its contents are
                 *   horizontally added to our own.  If it's horizontal (top
                 *   or bottom), use its width as a minimum, since it gets a
                 *   row slice of our space, sharing our overall width.  
                 */
                switch (pos)
                {
                case HTML_BANNERWIN_POS_TOP:
                case HTML_BANNERWIN_POS_BOTTOM:
                    /* it's horizontal - its width is our minimum width */
                    if (wid < chi_wid)
                        wid = chi_wid;
                    break;

                case HTML_BANNERWIN_POS_LEFT:
                case HTML_BANNERWIN_POS_RIGHT:
                    /* it's vertical - add its width to our own */
                    wid += chi_wid;
                    break;
                }
            }
        }
    }

    /* return the calculated width */
    return wid;
}

/* ------------------------------------------------------------------------ */
/*
 *   External banner formatter - ordinary text stream window 
 */

/*
 *   create 
 */
CHtmlFormatterBannerExtText::CHtmlFormatterBannerExtText(
    CHtmlFormatterBannerExt *parent, CHtmlParser *parser,
    unsigned long style)
    : CHtmlFormatterBannerExt(parent, parser, style)
{
    /* create our text buffer */
    txtbuf_ = new CHtmlTextBuffer();
}

/*
 *   delete 
 */
CHtmlFormatterBannerExtText::~CHtmlFormatterBannerExtText()
{
    /* we own our parser, so we must delete it if it's still around */
    if (parser_ != 0)
    {
        CHtmlParser *parser;

        /* remember our parser for a moment */
        parser = parser_;

        /* forget the parser, so we don't try to access it again */
        parser_ = 0;

        /* now that we're disentangled, we can delete the parser */
        delete parser;
    }

    /* delete our text buffer */
    delete txtbuf_;
}

/*
 *   clear my contents 
 */
void CHtmlFormatterBannerExtText::clear_contents()
{
    /* cancel any animations on the outgoing page */
    cancel_playback();

    /* clear the page in the parser */
    if (parser_ != 0)
        parser_->clear_page();

    /* invalidate the entire window */
    inval_window();

    /* notify the window that we're clearing it */
    if (win_ != 0)
        win_->notify_clear_contents();

    /* 
     *   reset the formatter - don't reset sounds, as sounds are the province
     *   of the main window only 
     */
    start_at_top(FALSE);
}

/*
 *   turn HTML mode on or off 
 */
void CHtmlFormatterBannerExtText::set_html_mode(int mode)
{
    /* adjust the parser mode, according to the caller's request */
    if (mode)
    {
        /* 
         *   turning HTML mode on - tell the parser to interpret markups and
         *   treat whitespace as insignificant 
         */
        parser_->obey_whitespace(FALSE, TRUE);
        parser_->obey_markups(TRUE);
    }
    else
    {
        /* 
         *   Turn HTML mode off - treat whitespace as significant, and do not
         *   interpret markups.  Since this is a programmatic mode rather
         *   than a mode set with a tag, there's no way for a tag to end this
         *   mode, so there's no need to interpret any text that looks like
         *   an end tag.  
         */
        parser_->obey_whitespace(TRUE, TRUE);
        parser_->obey_markups(FALSE);
        parser_->obey_end_markups(FALSE);
    }
}

/*
 *   Get HTML mode 
 */
int CHtmlFormatterBannerExtText::get_html_mode() const
{
    /* HTML mode is on if we're obeying markups */
    return parser_->get_obey_markups();
}

/*
 *   start at the top of our format list 
 */
void CHtmlFormatterBannerExtText::start_at_top(int reset_sounds)
{
    /* inherit default handling */
    CHtmlFormatterBannerExt::start_at_top(reset_sounds);

    /* set up at the parser's outer tag */
    curtag_ = parser_->get_outer_container();
}

/*
 *   Add source text for formatting 
 */
void CHtmlFormatterBannerExtText::add_source_text(
    const textchar_t *txt, size_t len)
{
    /* append the text to our buffer - we'll parse and format it later */
    txtbuf_->append(txt, len);
}

/*
 *   Flush the source text 
 */
void CHtmlFormatterBannerExtText::flush_txtbuf(int fmt)
{
    /* if the OS window has already been closed, ignore the request */
    if (win_ == 0)
        return;

    /* parse what's in the source buffer */
    parser_->parse(txtbuf_, win_->get_win_group());

    /* we're done with this source, so clear the buffer */
    txtbuf_->clear();

    /* parse and format the source if desired */
    if (fmt)
        win_->do_formatting(FALSE, FALSE, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Text Grid Banner implementation 
 */

/*
 *   construction 
 */
CHtmlFormatterBannerExtGrid::CHtmlFormatterBannerExtGrid(
    CHtmlFormatterBannerExt *parent, unsigned long style)
    : CHtmlFormatterBannerExt(parent, 0, style)
{
    /* start the output position at the upper left corner */
    csr_row_ = 0;
    csr_col_ = 0;

    /* we haven't written any content yet */
    row_cnt_ = 0;
    col_cnt_ = 0;

    /* we don't have any text stored yet */
    max_ofs_ = 0;

    /* we've never been sized to our contents */
    last_sized_ht_ = 0;
    last_sized_wid_ = 0;
}

/* 
 *   start formatting at the top of the tag list 
 */
void CHtmlFormatterBannerExtGrid::start_at_top(int reset_sounds)
{
    CHtmlDispTextGrid *disp;
    int row;
    CHtmlSysFont *old_font;

    /* remember the old font while we're resetting the state */
    old_font = get_font();

    /* 
     *   We don't need to rebuild the display list the way an ordinary
     *   formatter does, since our display list isn't affected by window
     *   layout changes.  We also don't need to worry about sounds, since
     *   grid windows don't process sound tags at all.  We do need to reset
     *   the formatter to the initial state, though.  
     */
    reset_formatter_state(FALSE);

    /* if we had a font before, restore it, so that we restore the colors */
    if (old_font != 0)
        set_font(old_font);

    /* 
     *   get the current font, in case it's changed, but keep the current
     *   font color scheme 
     */
    get_grid_font(TRUE);

    /* start at the top margin */
    curpos_.set(margin_left_, phys_margins_.top);

    /* run through the display list and set everything to the new font */
    for (disp = (CHtmlDispTextGrid *)disp_head_, row = 0 ; disp != 0 ;
         disp = (CHtmlDispTextGrid *)disp->get_next_disp(), ++row)
    {
        CHtmlRect pos;

        /* set this item's font - this will update its size */
        disp->set_font(get_win(), curfont_);

        /* get this item's updated size */
        pos = disp->get_pos();

        /* fix up this line start */
        line_starts_->set_ypos(row, pos.top);

        /* set line positions for the item */
        set_line_positions(disp, disp->get_next_disp());

        /* move to the next line */
        curpos_.y = disp->get_pos().bottom;
        curpos_.x = margin_left_;
        layout_max_y_pos_ = curpos_.y;
    }

    /*
     *   If we've ever been sized to our contents, or sized to a particular
     *   character cell size, recalculate our size at the same character cell
     *   size.  We could have a new font that changes our pixel size.  
     */
    if (last_sized_ht_ != 0 && last_sized_wid_ != 0)
    {
        HTML_BannerWin_Pos_t align;
        unsigned long style;
        CHtmlPoint pos;
        
        /* get the display item for the last row */
        disp = get_disp_by_row(last_sized_ht_ - 1);

        /* if we didn't find the row, write some text to create one */
        if (disp == 0)
        {
            /* write some text at the desired position */
            write_text_at(" ", 1,
                          last_sized_ht_ - 1, last_sized_wid_ - 1, FALSE);

            /* get the display item again */
            disp = get_disp_by_row(last_sized_ht_ - 1);
        }

        /* if it's not wide enough, write some text to expand it */
        if (disp->get_text_columns() < last_sized_wid_)
            disp->write_text(get_win(), curfont_,
                             last_sized_wid_ - 1, " ", 1);

        /* 
         *   Now, finally, we can be sure that we actually have a display
         *   item at the row with text at the column we're interested in.
         *   Get the display item's position, and measure its width up to the
         *   column of interest, and that'll give us the pixel coordinates to
         *   use as the extent of the window.  With that in hand, we can set
         *   the new window size. 
         */
        pos = disp->get_text_pos(get_win(),
                                 disp->get_text_ofs() + last_sized_wid_);

        /* set the size based on the alignment type */
        get_win()->get_banner_info(&align, &style);
        switch(align)
        {
        case HTML_BANNERWIN_POS_TOP:
        case HTML_BANNERWIN_POS_BOTTOM:
            /* top/bottom banner - set the height */
            get_win()->set_banner_size(0, HTML_BANNERWIN_UNITS_PIX, FALSE,
                                       disp->get_pos().bottom,
                                       HTML_BANNERWIN_UNITS_PIX, TRUE);
            break;

        case HTML_BANNERWIN_POS_LEFT:
        case HTML_BANNERWIN_POS_RIGHT:
            get_win()->set_banner_size(pos.x, HTML_BANNERWIN_UNITS_PIX, TRUE,
                                       0, HTML_BANNERWIN_UNITS_PIX, FALSE);
            break;
        }
    }
}

/*
 *   Set the size 
 */
void CHtmlFormatterBannerExtGrid::set_banner_size(int siz, int siz_units)
{
    /* 
     *   if we're sizing in character cell units, remember the explicit size
     *   setting so that we can adapt at the same cell-count size to any
     *   change in our font 
     */
    if (siz_units == OS_BANNER_SIZE_ABS)
    {
        HTML_BannerWin_Pos_t align;
        unsigned long style;

        /* get our alignment */
        get_win()->get_banner_info(&align, &style);
        
        /* remember the new height or width, depending on our orientation */
        if (align == HTML_BANNERWIN_POS_TOP
            || align == HTML_BANNERWIN_POS_BOTTOM)
        {
            /* top/bottom - the size is our height */
            last_sized_ht_ = siz;
        }
        else
        {
            /* left/right - the size is our width */
            last_sized_wid_ = siz;
        }
    }

    /* inherit the default handling */
    CHtmlFormatterBannerExt::set_banner_size(siz, siz_units);
}

/*
 *   Get the default font for the grid 
 */
void CHtmlFormatterBannerExtGrid::get_grid_font(int keep_color)
{
    CHtmlFontDesc desc;

    /* use the window's default fixed-pitch font at the default size */
    desc.htmlsize = 3;
    desc.fixed_pitch = TRUE;

    /* 
     *   if we're keeping the existing color scheme, and we already have a
     *   font, get the colors from the current font
     */
    if (keep_color && get_font() != 0)
    {
        CHtmlFontDesc old_desc;
        
        /* get the current font's description */
        get_font()->get_font_desc(&old_desc);

        /* copy the text foreground color from the old description */
        desc.color = old_desc.color;
        desc.default_color = old_desc.default_color;

        /* copy the text background color from the old description */
        desc.bgcolor = old_desc.bgcolor;
        desc.default_bgcolor = old_desc.default_bgcolor;
    }

    /* get the font */
    curfont_ = get_win()->get_font(&desc);
}

/*
 *   Get the display item for a given row number 
 */
CHtmlDispTextGrid *CHtmlFormatterBannerExtGrid::get_disp_by_row(int row) const
{
    /* 
     *   The items are in the line starts array by row number, so simply use
     *   the row as an index into the line starts.  If there is no such line
     *   start, there's no such line. 
     */
    return (CHtmlDispTextGrid *)line_starts_->get(row);
}

/* 
 *   add source text for formatting 
 */
void CHtmlFormatterBannerExtGrid::write_text_at(
    const textchar_t *txt, size_t len, int row, int col, int move_cursor)
{
    CHtmlDispTextGrid *fix_disp;
    CHtmlDispTextGrid *disp;
    const textchar_t *start;
    unsigned long ofs;

    /* if we have no window, ignore it */
    if (get_win() == 0)
        return;

    /* presume we won't have to fix up any row offsets */
    fix_disp = 0;

    /* keep going until we run out of text to add */
    for (start = txt ; ; ++txt, --len)
    {
        /* 
         *   If we're out of text, or this is a newline or return character,
         *   add the text up to this point.  If we don't have anything in the
         *   preceding chunk (i.e., txt == start), then there's nothing to
         *   flush at this point.  
         */
        if ((len == 0 || *txt == '\n' || *txt == '\r') && txt != start)
        {
            /*
             *   First, find the display item for the current row.  If this
             *   past the last row for which we have a display item, add new
             *   display items up to this row. 
             */
            if (row >= row_cnt_)
            {
                int cur_row;
                
                /* add new rows until we get to the desired item */
                for (cur_row = row_cnt_ ; cur_row <= row ; ++cur_row)
                {
                    /* create the new display item */
                    disp = new (this) CHtmlDispTextGrid(
                        get_win(), curfont_, max_ofs_++);

                    /* add the item to our list */
                    add_to_disp_list(disp);

                    /* set the line positions for the item */
                    set_line_positions(disp, 0);

                    /* add a line start for the item */
                    add_line_start(disp);

                    /* set curpos_ to the bottom of this item */
                    curpos_.x = margin_left_;
                    curpos_.y = disp->get_pos().bottom;

                    /* count the new row */
                    ++row_cnt_;
                }
            }
            else
            {
                /* get the display item for the given row */
                disp = get_disp_by_row(row);
            }

            /* if we have a display item, add the text to the item */
            if (disp != 0
                && disp->write_text(get_win(), curfont_,
                                    col, start, txt - start)
                && fix_disp == 0)
            {
                /*
                 *   Writing this text expanded the row.  We must fix up the
                 *   offsets for all subsequent display items accordingly.
                 *   Don't do this immediately, because we might still have
                 *   more to write that will change even more; so, simply
                 *   remember this item for later. 
                 */
                fix_disp = disp;
            }

            /* adjust the column position */
            col += (txt - start);

            /* if this is the highest column so far, note the new maximum */
            if (col + (txt - start) > col_cnt_)
                col_cnt_ = col + (txt - start);
        }

        /* if we're out of text, we're done */
        if (len == 0)
            break;

        /* check for special characters */
        switch(*txt)
        {
        case '\n':
            /* newline - move to the first column of the next row */
            ++row;
            col = 0;
            
            /* the next chunk starts after the newline */
            start = txt + 1;
            break;

        case '\r':
            /* carriage return - move to the start of this row */
            col = 0;
            
            /* the next chunk starts after the return character */
            start = txt + 1;
            break;

        default:
            /* other characters are simply displayed */
            break;
        }
    }

    /* 
     *   if we expanded any of the lines we wrote, we must fix up the text
     *   offsets for all subsequent lines to account for the new text offsets
     *   in the preceding lines 
     */
    if (fix_disp != 0)
    {
        /* scan lines, starting at the first one we expanded */
        for (disp = fix_disp, ofs = disp->get_text_ofs() ; disp != 0 ;
             disp = (CHtmlDispTextGrid *)disp->get_next_disp())
        {
            /* set this item's new text offset */
            disp->set_text_ofs(ofs);
            
            /* adjust the offset by this item's width */
            ofs += disp->get_text_columns();
        }

        /* remember the new maximum offset */
        max_ofs_ = ofs;
    }

    /* move the cursor to the new position if desired */
    if (move_cursor)
    {
        /* remember the new row and column */
        csr_row_ = row;
        csr_col_ = col;
    }
}

/* 
 *   clear the contents 
 */
void CHtmlFormatterBannerExtGrid::clear_contents()
{
    CHtmlSysFont *old_font;
    
    /* invalidate the entire window */
    inval_window();

    /* reset the display list */
    reset_disp_list();

    /* remember the old font before resetting the formatter state */
    old_font = get_font();

    /* 
     *   reset formatter state - since we don't have any sounds, there's no
     *   need to reset anything in the sound queues
     */
    reset_formatter_state(FALSE);

    /* if we had a font previously, restore it, so we can keep its colors */
    if (old_font != 0)
        set_font(old_font);

    /* get the correct current font, keeping the current color scheme */
    get_grid_font(TRUE);

    /* notify the window that we're clearing it */
    if (win_ != 0)
        win_->notify_clear_contents();

    /* move the output position to the upper left corner */
    csr_row_ = 0;
    csr_col_ = 0;

    /* we no longer have any content */
    row_cnt_ = 0;
    col_cnt_ = 0;

    /* we don't have any text stored any more */
    max_ofs_ = 0;
}

/*
 *   get the character at the given text offset 
 */
textchar_t CHtmlFormatterBannerExtGrid::
   get_char_at_ofs(unsigned long ofs) const
{
    CHtmlDispTextGrid *disp;

    /* find the display item containing the offset */
    disp = (CHtmlDispTextGrid *)find_by_txtofs(ofs, TRUE, FALSE);

    /* if we didn't find a display item for the offset, there's no text */
    if (disp == 0)
        return '\0';

    /* get the character at the given offset from the display item */
    return disp->get_char_at_ofs(ofs);
}

/*
 *   Internal service routine: translate a color from an os_color_t to an
 *   HTML_color_t value.
 */
HTML_color_t CHtmlFormatterBannerExtGrid::xlat_color(os_color_t oscolor)
{
    /* check for parameterized colors */
    if (os_color_is_param(oscolor))
    {
        HTML_color_t hc;
        
        /* map to the appropriate system color */
        switch(oscolor)
        {
        case OS_COLOR_P_TRANSPARENT:
            /* 
             *   it doesn't matter what we use for this - it'll turn into a
             *   default color whatever we use 
             */
            hc = HTML_COLOR_TEXT;
            break;
            
        case OS_COLOR_P_TEXT:
            hc = HTML_COLOR_TEXT;
            break;

        case OS_COLOR_P_TEXTBG:
            hc = HTML_COLOR_BGCOLOR;
            break;

        case OS_COLOR_P_STATUSLINE:
            hc = HTML_COLOR_STATUSTEXT;
            break;

        case OS_COLOR_P_STATUSBG:
            hc = HTML_COLOR_STATUSBG;
            break;

        case OS_COLOR_P_INPUT:
            hc = HTML_COLOR_INPUT;
            break;

        default:
            /* unknown color - use the text color */
            hc = HTML_COLOR_TEXT;
            break;
        }

        /* map the color through the system window */
        return get_win()->map_system_color(hc);
    }
    else
    {
        /* it's a simple RGB color - map to the HTML_color_t type */
        return HTML_make_color(os_color_get_r(oscolor),
                               os_color_get_g(oscolor),
                               os_color_get_b(oscolor));
    }
}

/*
 *   Set the color 
 */
void CHtmlFormatterBannerExtGrid::set_banner_text_color(
    os_color_t fg, os_color_t bg)
{
    CHtmlFontDesc desc;

    /* if we don't have a window, ignore it */
    if (get_win() == 0)
        return;

    /* get the formatter's current font description */
    get_font()->get_font_desc(&desc);

    /* set the new foreground color */
    desc.color = xlat_color(fg);
    desc.default_color = (fg == OS_COLOR_P_TRANSPARENT);

    /* set the new background color, if it's not 'transparent' */
    desc.bgcolor = xlat_color(bg);
    desc.default_bgcolor = (bg == OS_COLOR_P_TRANSPARENT);

    /* set the new font */
    set_font(get_win()->get_font(&desc));
}

/*
 *   Set the window background color
 */
void CHtmlFormatterBannerExtGrid::set_banner_screen_color(os_color_t color)
{
    /* if we don't have a window, there's nothing we can do */
    if (get_win() == 0)
        return;
    
    /* set the window's background color */
    get_win()->set_html_bg_color(xlat_color(color),
                                 color == OS_COLOR_P_TRANSPARENT);
}

/* ------------------------------------------------------------------------ */
/*
 *   Line-start table implementation 
 */

CHtmlLineStarts::CHtmlLineStarts()
{
    /* allocate an initial block of page pointers */
    top_pages_allocated_ = 1024;
    second_pages_allocated_ = 0;
    pages_ = (CHtmlLineStartEntry **)
             th_malloc(top_pages_allocated_ * sizeof(CHtmlLineStartEntry *));
    count_ = 0;
}

CHtmlLineStarts::~CHtmlLineStarts()
{
    /* if we have a page array, delete it */
    if (pages_)
    {
        size_t i;
        CHtmlLineStartEntry **page;
        
        /* free each second-level page array we've allocated */
        for (i = 0, page = pages_ ; i < second_pages_allocated_ ;
             ++i, ++page)
            th_free(*page);
        
        /* free the top-level page array */
        th_free(pages_);
        pages_ = 0;
        top_pages_allocated_ = 0;
        second_pages_allocated_ = 0;
    }
}

/*
 *   Add an item at a given index 
 */
void CHtmlLineStarts::add(long index, class CHtmlDisp *item, long ypos)
{
    unsigned int page;
    unsigned int offset;
    
    /* get the location of this item */
    calc_location(index, &page, &offset);

    /* if the second-level page isn't allocated, allocate it */
    ensure_page(page);

    /* store it */
    (*(pages_ + page) + offset)->disp = item;
    (*(pages_ + page) + offset)->ypos = ypos;

    /* note it if it's the highest index */
    if (index >= count_)
        count_ = index + 1;
}

/*
 *   Ensure that a page is allocated 
 */
void CHtmlLineStarts::ensure_page(unsigned int page)
{
    /* make sure the top-level array is big enough */
    if (page >= top_pages_allocated_)
    {
        /* increase the master page size */
        top_pages_allocated_ += 1024;
        
        /* reallocate the block */
        pages_ = (CHtmlLineStartEntry **)
                 th_realloc(pages_, (top_pages_allocated_
                                     * sizeof(CHtmlLineStartEntry **)));
    }

    /* make sure the second-level array is allocated */
    while (page >= second_pages_allocated_)
    {
        /* allocate a new page */
        *(pages_ + second_pages_allocated_) = (CHtmlLineStartEntry *)
            th_malloc(HTML_LINE_START_PAGESIZE * sizeof(CHtmlLineStartEntry));

        /* count it */
        ++second_pages_allocated_;
    }
}

/*
 *   Search for a given entry.  Performs a binary search through the
 *   items, so the search criterion must be something on which the line
 *   start entries are sorted.  
 */
long CHtmlLineStarts::find(const CHtmlLineStartSearcher *searcher) const
{
    long low_index;
    long high_index;
    long cur_index;

    /* do a binary search for the item containing the given position */
    for (low_index = 0, high_index = count_ - 1 ; ; )
    {
        CHtmlLineStartEntry *cur_entry;
        CHtmlLineStartEntry *nxt_entry;
        
        /* stop if we've exhausted the list */
        if (low_index > high_index)
            return 0;

        /* split the difference between the two ends of the range */
        cur_index = low_index + (high_index - low_index)/2;

        /* get this item and the next item */
        cur_entry = get_internal(cur_index);
        nxt_entry = (cur_index == count_ - 1 ? 0
                     : get_internal(cur_index + 1));

        /* if this one matches, accept it */
        if (searcher->is_between(cur_entry, nxt_entry))
            return cur_index;

        /* see if we're above or below */
        if (searcher->is_low(cur_entry))
        {
            /*
             *   We're looking for something above this position.  If
             *   we're also below the next item, this is the one they
             *   want.  If this is the last item, consider this item to be
             *   a match, since the high end is effectively unbounded.  
             */
            if (cur_index == count_ - 1)
                return cur_index;

            /*
             *   This one doesn't match.  Move on to the next item; since
             *   we want something above this item, only consider those
             *   items after the current one. 
             */
            low_index = (cur_index == low_index ? low_index + 1 : cur_index);
        }
        else
        {
            /*
             *   This item is too high -- consider only items below this
             *   item. 
             */
            high_index = (cur_index == high_index ? high_index - 1
                          : cur_index);
        }
    }
}

/*
 *   Find a line start item given a y position 
 */
class CHtmlLineStartSearcherYpos: public CHtmlLineStartSearcher
{
public:
    CHtmlLineStartSearcherYpos(long ypos) { ypos_ = ypos; }

    int is_between(const CHtmlLineStartEntry *a,
                   const CHtmlLineStartEntry *b) const
    {
        /*
         *   if the desired y position is over a's y position, and either
         *   a is the last item or the desired y position is below b's y
         *   position, this is the one we're looking for 
         */
        return (ypos_ >= a->ypos
                && (b == 0 || ypos_ < b->ypos));
    }

    int is_low(const CHtmlLineStartEntry *item) const
    {
        /* this one is low if the desired y position is higher */
        return (ypos_ >= item->ypos);
    }

private:
    long ypos_;
};

long CHtmlLineStarts::find_by_ypos(long ypos) const
{
    CHtmlLineStartSearcherYpos searcher(ypos);
    return find(&searcher);
}


/*
 *   Find a line start item given a text offset
 */
class CHtmlLineStartSearcherTxtofs: public CHtmlLineStartSearcher
{
public:
    CHtmlLineStartSearcherTxtofs(unsigned long txtofs) { txtofs_ = txtofs; }

    int is_between(const CHtmlLineStartEntry *a,
                   const CHtmlLineStartEntry *b) const
    {
        /*
         *   if the desired offset is over a's offset, and either a is
         *   the last item or the desired offset is below b's offset, this
         *   is the one we're looking for 
         */
        return (txtofs_ >= a->disp->get_text_ofs()
                && (b == 0 || txtofs_ < b->disp->get_text_ofs()));
    }

    int is_low(const CHtmlLineStartEntry *item) const
    {
        /* this one is low if the desired offset is higher */
        return (txtofs_ >= item->disp->get_text_ofs());
    }

private:
    unsigned long txtofs_;
};

long CHtmlLineStarts::find_by_txtofs(unsigned long txtofs) const
{
    CHtmlLineStartSearcherTxtofs searcher(txtofs);
    return find(&searcher);
}

/*
 *   Find a line start item given a debugger source file position 
 */
class CHtmlLineStartSearcherDebugpos: public CHtmlLineStartSearcher
{
public:
    CHtmlLineStartSearcherDebugpos(unsigned long fpos) { fpos_ = fpos; }

    int is_between(const CHtmlLineStartEntry *a,
                   const CHtmlLineStartEntry *b) const
    {
        CHtmlDispTextDebugsrc *dispa = (CHtmlDispTextDebugsrc *)a->disp;
        CHtmlDispTextDebugsrc *dispb = (b == 0
                                        ? 0
                                        : (CHtmlDispTextDebugsrc *)b->disp);

        /*
         *   return true if the desired source file location is between
         *   the two positions 
         */
        return (fpos_ >= dispa->get_debugsrcpos()
                && (b == 0 || fpos_ < dispb->get_debugsrcpos()));
    }

    int is_low(const CHtmlLineStartEntry *item) const
    {
        CHtmlDispTextDebugsrc *disp = (CHtmlDispTextDebugsrc *)item->disp;

        /* this one is low if the desired offset is higher */
        return (fpos_ >= disp->get_debugsrcpos());
    }

private:
    /* debugger source file position */
    unsigned long fpos_;
};

long CHtmlLineStarts::find_by_debugsrcpos(unsigned long linenum) const
{
    CHtmlLineStartSearcherDebugpos searcher(linenum);
    return find(&searcher);
}

/* ------------------------------------------------------------------------ */
/*
 *   Flow stack implementation
 */

/*
 *   push a new stack level 
 */
void CHtmlFmtFlowStack::push(long old_margin_delta, long bottom,
                             CHtmlTagTABLE *cur_table)
{
    /* if we don't have any room, don't write anything */
    if (sp >= sizeof(stk)/sizeof(stk[0]))
        return;

    /* record this item */
    stk[sp].old_margin_delta = old_margin_delta;
    stk[sp].bottom = bottom;
    stk[sp].cur_table = cur_table;

    /* add it to the stack */
    ++sp;
}

/*
 *   pop a stack level 
 */
void CHtmlFmtFlowStack::pop()
{
    /* make sure we have something to pop */
    if (sp == 0)
        return;

    /* remove it */
    --sp;
}

/*
 *   check to see if we're past the end of the current flow stack level 
 */
int CHtmlFmtFlowStack::is_past_end(long ypos, CHtmlTagTABLE *cur_table) const
{
    /* if we're empty, there's nothing to be past the end of */
    if (is_empty())
        return FALSE;

    /* 
     *   if we're not past the vertical extent of the last item on the
     *   stack, we're not past this level 
     */
    if (ypos <= get_top()->bottom)
        return FALSE;

    /* 
     *   we're past the vertical extent of the most deeply nested item, but
     *   we can only clear it if the table nesting allows it 
     */
    return can_end_item(get_top(), cur_table);
}

/* 
 *   Check to see if table nesting allows an item to end now.  We can only
 *   pop a level if we're not nested inside a new table started after this
 *   level was created, because a table must be contiguously in a single
 *   level.  
 */
int CHtmlFmtFlowStack::can_end_item(const CHtmlFmtFlowStkItem *item,
                                    const CHtmlTagTABLE *cur_table) const
{
    CHtmlTagTABLE *enc;

    /* 
     *   if there's no current table, there's obviously no new table nesting
     *   since the floating item was created, so we can end the item 
     */
    if (cur_table == 0)
        return TRUE;

    /* 
     *   We have a current table, so we must make sure it's not nested
     *   within the level that created this stack position.  Check this by
     *   looking up the table stack from this level's active table to see if
     *   we can find the currently active table among this level's active
     *   table and its enclosing tables - if so, we CAN close this level,
     *   because we're not inside any new tables created since this level
     *   was created.  
     */
    for (enc = item->cur_table ; enc != 0 && enc != cur_table ;
         enc = enc->get_enclosing_table()) ;

    /* 
     *   if we found the active table among the enclosing tables, we can
     *   close the level; otherwise, we must be within a table created since
     *   this level was created, so we cannot yet close the level 
     */
    return (enc != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Image maps 
 */

/*
 *   delete a map - delete all zones in the map 
 */
CHtmlFmtMap::~CHtmlFmtMap()
{
    while (first_zone_ != 0)
    {
        CHtmlFmtMapZone *nxt;

        /* remember the next zone */
        nxt = first_zone_->next_;

        /* delete this zone */
        delete first_zone_;

        /* advance the list */
        first_zone_ = nxt;
    }
}

/*
 *   add a zone to an image map 
 */
void CHtmlFmtMap::add_zone(CHtmlFmtMapZone *zone)
{
    if (last_zone_ == 0)
        first_zone_ = zone;
    else
        last_zone_->next_ = zone;
    last_zone_ = zone;
}

/*
 *   find a zone containing a point 
 */
CHtmlFmtMapZone *CHtmlFmtMap::find_zone(int x, int y,
                                        const CHtmlRect *image_bounds) const
{
    CHtmlFmtMapZone *cur;
    
    /* go through the list looking for a matching zone */
    for (cur = first_zone_ ; cur != 0 ; cur = cur->next_)
    {
        /* if this zone contains the point, return it */
        if (cur->pt_in_zone(x, y, image_bounds))
            return cur;
    }

    /* didn't find a matching zone */
    return 0;
}


/*
 *   Zone implementation 
 */

/*
 *   Compute a coordinate, given the COORD settinng and the image bounds.
 */
long CHtmlFmtMapZone::compute_coord(const struct CHtmlTagAREA_coords_t *coord,
                                    const CHtmlRect *image_bounds,
                                    int vert)
{
    long offset;
    
    /* get the base offset from the image bounds */
    offset = (vert ? image_bounds->top : image_bounds->left);

    /* see if it's a percentage */
    if (coord->pct_)
    {
        long range;

        /* get the range from the image bounds */
        range = (vert ? image_bounds->bottom - image_bounds->top
                      : image_bounds->right - image_bounds->left);

        /* compute the value as a percentage of the range */
        return offset
            + (long)(((double)coord->val_ * (double)range) / 100.0);
    }
    else
    {
        /* compute the value as an offset from the base */
        return offset + coord->val_;
    }
}


/*
 *   RECT shape implementation 
 */

CHtmlFmtMapZoneRect::CHtmlFmtMapZoneRect
   (const struct CHtmlTagAREA_coords_t *coords, CHtmlDispLink *link)
    : CHtmlFmtMapZone(link)
{
    /* save the coordinates */
    memcpy(coords_, coords, sizeof(coords_));
}

int CHtmlFmtMapZoneRect::pt_in_zone(int x, int y,
                                    const CHtmlRect *image_bounds)
{
    long left, top, right, bottom;
    
    /* compute my rectangle */
    left = compute_coord(&coords_[0], image_bounds, FALSE);
    top = compute_coord(&coords_[1], image_bounds, TRUE);
    right = compute_coord(&coords_[2], image_bounds, FALSE);
    bottom = compute_coord(&coords_[3], image_bounds, TRUE);

    /* see if the point is in this area */
    return (x >= left && x <= right && y >= top && y <= bottom);
}

/*
 *   CIRCLE shape implementation 
 */

CHtmlFmtMapZoneCircle::CHtmlFmtMapZoneCircle
   (const struct CHtmlTagAREA_coords_t *coords, CHtmlDispLink *link)
    : CHtmlFmtMapZone(link)
{
    /* save the x,y coordinates */
    memcpy(coords_, coords, sizeof(coords_));

    /* save the radius - it can't be a percentage */
    radius_ = coords[2].val_;
}

int CHtmlFmtMapZoneCircle::pt_in_zone(int x, int y,
                                      const CHtmlRect *image_bounds)
{
    long x0, y0;
    long dx, dy;
    long dist_squared;

    /* compute my origin */
    x0 = compute_coord(&coords_[0], image_bounds, FALSE);
    y0 = compute_coord(&coords_[1], image_bounds, TRUE);

    /* compute the point's distance from the origin */
    dx = x - x0;
    dy = y - y0;
    dist_squared = (dx * dx) + (dy * dy);

    /* if this is within the radius, it's in the circle */
    return (dist_squared <= radius_ * radius_);
}


/*
 *   POLY shape implementation 
 */

CHtmlFmtMapZonePoly::CHtmlFmtMapZonePoly
   (const struct CHtmlTagAREA_coords_t *coords, int coord_cnt,
    CHtmlDispLink *link)
    : CHtmlFmtMapZone(link)
{
    /* save the coordinates */
    memcpy(coords_, coords, coord_cnt * sizeof(coords_[0]));
}

int CHtmlFmtMapZonePoly::pt_in_zone(int /*x*/, int /*y*/,
                                    const CHtmlRect * /*image_bounds*/)
{
    // $$$
    return FALSE;
}

