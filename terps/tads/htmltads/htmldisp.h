/* $Header: d:/cvsroot/tads/html/htmldisp.h,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldisp.h - HTML formatter display objects
Function
  The display objects are the objects that the HTML formatter
  prepares from the parse tree.  Display objects map directly onto
  the visual display, so it's easy to take a list of display objects
  and draw the document into a window.

  Display objects are organized into a simple linear list.  Each
  object has a rectangular boundary in the drawing coordinate system.
Notes
  
Modified
  09/08/97 MJRoberts  - Creation
*/

#ifndef HTMLDISP_H
#define HTMLDISP_H

#include <limits.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLATTR_H
#include "htmlattr.h"
#endif
#ifndef HTMLURL_H
#include "htmlurl.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Memory heap source ID.  For each memory block we allocate in
 *   CHtmlDisp::operator new, we keep track of whether the memory came
 *   from the formatter heap or from the system heap. 
 */
enum htmldisp_heapid_t
{
    HTMLDISP_HEAPID_SYS = 0,                                 /* system heap */
    HTMLDISP_HEAPID_FMT = 1                               /* formatter heap */
};

/* ------------------------------------------------------------------------ */
/*
 *   Whitespace search object.  This lets us search a series of display
 *   items to find and remove trailing whitespace. 
 */
class CHtmlDisp_wsinfo
{
public:
    CHtmlDisp_wsinfo()
    {
        /* we haven't found any whitespace yet */
        last_ws_disp_ = 0;
    }

    /* 
     *   Scan for and remove trailing whitespace.  We'll start with the
     *   given display item and scan ahead to (but not including) 'nxt', or
     *   the end of the list if 'nxt' is null.  We'll trim off any
     *   contiguous run of whitespace we find at the end of the display
     *   list.  
     */
    void remove_trailing_whitespace(class CHtmlSysWin *win,
                                    class CHtmlDisp *disp,
                                    class CHtmlDisp *nxt);

    /* 
     *   Hide trailing whitespace.  This doesn't actually remove any
     *   trailing whitespace, but merely suppresses the display of trailing
     *   whitespace on a line. 
     */
    void hide_trailing_whitespace(class CHtmlSysWin *win,
                                  class CHtmlDisp *disp,
                                  class CHtmlDisp *nxt);

    /* 
     *   For the display item's use: remember a position in the current
     *   display item as the start of the trailing whitespace in this
     *   item.  If we find that this is the last item with text, we'll
     *   come back here to remove trailing whitespace later. 
     */
    void set_ws_pos(class CHtmlDisp *disp, void *pos)
    {
        /* if we don't have a previous position already, set this one */
        if (last_ws_disp_ == 0)
        {
            last_ws_disp_ = disp;
            last_ws_pos_ = pos;
        }
    }

    /*
     *   For the display item's use: clear the trailing whitespace
     *   position.  If we find that the current display item doesn't have
     *   any whitespace, but it has something that negates the "trailing"
     *   status of any previous whitespace (such as more non-whitespace
     *   text), we need to forget about the previous whitespace, since
     *   it's not trailing after all. 
     */
    void clear_ws_pos()
    {
        last_ws_disp_ = 0;
    }

private:
    /* last display item containing whitespace */
    class CHtmlDisp *last_ws_disp_;

    /* 
     *   position of whitespace in last display item; this context
     *   information is meaningful only to the display item 
     */
    void *last_ws_pos_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Display site implementation.  This class can be multiply inherited into
 *   a CHtmlDisp subclass to implement a CHtmlSysImageDisplaySite interface
 *   on the display object.
 *   
 *   To use this mix-in with a CHtmlDisp subclass, the subclass must do the
 *   following:
 *   
 *   1. Add this class to its base class list
 *   
 *   2. Explicitly call our constructor from the subclass constructor,
 *   passing us the window object, the address of the 'pos_' record from the
 *   subclass, and the image object.
 *   
 *   3. On a call to the subclass's virtual unset_win() method, explicitly
 *   call our unset_win() method.
 *   
 *   4. On a call to the subclass's virtual cancel_playback() method,
 *   explicitly call our cancel_playback() method.  
 */
class CHtmlDispDisplaySite: public CHtmlSysImageDisplaySite
{
public:
    /* create */
    CHtmlDispDisplaySite(class CHtmlSysWin *win, const CHtmlRect *pos,
                         class CHtmlResCacheObject *image);

    /* destroy */
    virtual ~CHtmlDispDisplaySite();

    /* unset our window reference */
    void unset_win();

    /* cancel playback - if our image is animated, we'll halt it */
    void cancel_playback();

    /* get/set the current image being displayed in the site */
    class CHtmlResCacheObject *get_site_image() const { return site_image_; }
    void set_site_image(class CHtmlResCacheObject *image);

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlSysImageDisplaySite interface implementation 
     */

    /* invalidate an area of the display site */
    virtual void dispsite_inval(unsigned int x, unsigned int y,
                                unsigned int width, unsigned int height);

    /* set a timer event */
    virtual void dispsite_set_timer(unsigned long delay_ms,
                                    class CHtmlSysImageAnimated *image);

    /* cancel timer events */
    virtual void dispsite_cancel_timer(class CHtmlSysImageAnimated *image);


protected:
    /* timer callback */
    static void img_timer_cb(void *ctx);

    /* the position record from the CHtmlDisp object */
    const CHtmlRect *posp_;

    /* the window we're associated with */
    class CHtmlSysWin *win_;

    /* 
     *   our timer - this is used if the object being displayed requests
     *   timer operations from its display site (i.e., us)
     */
    class CHtmlSysTimer *timer_;

    /* the image we're displaying */
    class CHtmlResCacheObject *site_image_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic display object 
 */
class CHtmlDisp
{
public:
    CHtmlDisp() { nxt_ = 0; line_id_bit_ = 0; }
    virtual ~CHtmlDisp() { }

    /* 
     *   Unset the window - receive notification that the window is about to
     *   be deleted.  Most display items don't keep a reference to their
     *   window, so this does nothing by default.  This can be overridden in
     *   subclasses that keep a reference to the containing window, so they
     *   know the reference is not usable after this point. 
     */
    virtual void unset_win() { }

    /*
     *   Cancel playback.  This should stop playback of any active media in
     *   the display item; for example, animations should be halted. 
     */
    virtual void cancel_playback() { }

    /* memory allocator - use the formatter for memory management */
    static void *operator new(size_t siz, class CHtmlFormatter *formatter);
    static void operator delete(void *ptr);

    /*
     *   Get/set the line ID.  The formatter needs to be able to tell when
     *   two adjacent items in the display list are in different lines.
     *   To accomplish this, it tags each item with a line ID; upon
     *   starting a new line, it increments the ID.  If the line ID's of
     *   two adjacent items are the same, the items are on the same line,
     *   otherwise they're on different lines.  Note that since we only
     *   need to distinguish adjacent items, we don't need a lot of
     *   precision in the line ID; we keep more than one bit, though, to
     *   allow for line ID's to be updated in the formatter *too often*
     *   (more than once per line), since it's often difficult to
     *   increment the line ID exactly once per line.  
     */
    int get_line_id() const { return line_id_bit_; }
    void set_line_id(int id) { line_id_bit_ = id; }

    /* 
     *   Draw the object in a window.  If the selection range intersects
     *   the item, the appropriate portion should be drawn selected, if it
     *   has a special selected appearance.  If 'clicked' is true, it
     *   means the mouse button is being held down with the mouse over the
     *   item, so the item should be drawn with any appropriate feedback.  
     */
    virtual void draw(class CHtmlSysWin *win,
                      unsigned long sel_start, unsigned long sel_end,
                      int clicked) = 0;

    /*
     *   Get the cursor type that should be used when the cursor is over
     *   this item.  Returns the standard arrow cursor by default.  
     */
    virtual Html_csrtype_t
        get_cursor_type(class CHtmlSysWin *, class CHtmlFormatter *,
                        int /*x*/, int /*y*/, int /*more_mode*/) const
        { return HTML_CSRTYPE_ARROW; }

    /* delete this item and all following items in its list */
    static void delete_list(CHtmlDisp *head);

    /*
     *   Measure the native width of this display object.  This is the
     *   amount of space that the object would take up without any
     *   external constraints.  For text, this is the width of the text in
     *   the current font; for a picture, it's the width of the graphic or
     *   the declared size. 
     */
    virtual int measure_width(class CHtmlSysWin *win) = 0;

    /*
     *   Get the amount of space this item needs above the text baseline
     *   for displaying text.  Generally, items that don't display text
     *   should return zero, since they don't contribute to the text area.
     */
    virtual int get_text_height(class CHtmlSysWin *) { return 0; }

    /*
     *   Get the overall vertical space needs for the item.  text_height
     *   is the vertical space above the text baseline used by the tallest
     *   text item.  *above_base is the amount of space this item needs
     *   above the text baseline, and *total is the amount of vertical
     *   space this item needs overall.  
     */
    virtual void get_vertical_space(class CHtmlSysWin *win, int text_height,
                                    int *above_base, int *below_base,
                                    int *total) = 0;

    /*
     *   Set my position, given the top of the area for the current line,
     *   the total height of the line, the offset of the text baseline from
     *   the top of the line area, and the maximum height of the text in the
     *   line.  The default implementation simply sets this item aligned at
     *   the top of the line.  
     */
    virtual void set_line_pos(class CHtmlSysWin *, int left, int top,
                              int /*total_height*/, int /*text_base_offset*/,
                              int /*max_text_height*/)
    {
        pos_.offset(left - pos_.left, top - pos_.top);
    }

    /*
     *   Receive notification that the item has been moved from the deferred
     *   list to the normal display list.  Only certain types of display
     *   items are deferred, so this default implementation does nothing and
     *   must be overridden for deferrable items.
     *   
     *   line_spacing is the vertical whitespace above the current line; the
     *   item should generally be vertically offset by this amount.  
     */
    virtual void format_deferred(class CHtmlSysWin *,
                                 class CHtmlFormatter *,
                                 long /*line_spacing*/) { }

    /* get next object in display list */
    CHtmlDisp *get_next_disp() const { return nxt_; }

    /*
     *   Find the line break position, given the amount of space
     *   remaining in the current line.  If the current display object can
     *   be broken into two pieces such that the first piece fits into the
     *   availble space but adding the second piece would not, we will
     *   break it into two pieces, creating a new display object for the
     *   second piece and linking the new object into the list, and return
     *   the new object.  If the current object fits in its entirety,
     *   we'll see if we can break within the next object.  prev_char is
     *   the last character of text from the previous display object, or
     *   zero if the previous object did not contain text; if two text
     *   objects are adjacent, they'll use this in determining if a line
     *   break can be inserted between them.  If prv and prv_pos are
     *   non-null, and we find that this object won't fit and we can't
     *   break before it, we'll call the previous object's
     *   find_line_break_prv method to tell it that it should break at the
     *   previous break position it found.  
     */
    virtual CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                                       class CHtmlSysWin *win,
                                       long space_avail,
                                       class CHtmlLineBreak *break_pos);

    /* 
     *   Do a break at a position we previously found.  'break_pos' is the
     *   break position information that we previously set up. 
     */
    virtual CHtmlDisp *do_line_break(class CHtmlFormatter *formatter,
                                     class CHtmlSysWin *win,
                                     class CHtmlLineBreak *break_pos);

    /* 
     *   Do a break to the right of a position we previously found, skipping
     *   leading spaces on the new line.  This is called when the break
     *   position was in a preceding item, but it found that it had only
     *   whitespace after the break position; we should simply break at the
     *   start of this item, or wherever the next non-whitespace can be
     *   found.  
     */
    virtual CHtmlDisp *do_line_break_skipsp(class CHtmlFormatter *formatter,
                                            class CHtmlSysWin *win);

    /*
     *   Find the location of trailing whitespace in the display item.  If
     *   the item doesn't have any trailing whitespace, but it does have
     *   text or something else that would prevent any previous whitespace
     *   from being the trailing whitespace, this should clear the
     *   whitespace position in the search state.  If we have trailing
     *   whitespace, this should set the position in the search state
     *   object so that we can come back and remove this whitespace.  By
     *   default, this routine does nothing, since default display items
     *   don't affect the text stream.  
     */
    virtual void find_trailing_whitespace(class CHtmlDisp_wsinfo *) { }

    /*
     *   Remove trailing whitespace that we previously found with a call to
     *   find_trailing_whitespace().
     *   
     *   'pos' is the position of the whitespace that this object set on the
     *   previous call to find_trailing_whitespace().  If 'pos' is null, it
     *   means that the start of the run of whitespace was in an earlier
     *   item in the list, in which case this item must be all whitespace
     *   and can be made completely invisible.  
     */
    virtual void remove_trailing_whitespace(class CHtmlSysWin *,
                                            void * /*pos*/) { }

    /* 
     *   hide trailing whitespace - this doesn't actually delete the
     *   whitespace, but merely prevents it from being displayed visually 
     */
    virtual void hide_trailing_whitespace(class CHtmlSysWin *,
                                          void * /*pos*/) { }

    /*
     *   Measure the width of the trailing whitespace, if any.  
     */
    virtual long measure_trailing_ws_width(class CHtmlSysWin *) const
        { return 0; }

    /*
     *   Measure the metrics of this item for use in a table cell.
     *   Measures this item's contribution to the minimum and maximum
     *   width of items in the cell.  
     */
    virtual void measure_for_table(class CHtmlSysWin *win,
                                   class CHtmlTableMetrics *metrics);

    /*
     *   Measure the metrics of this item for use in a banner.  Measurs
     *   the item's contribution to the minimum and maximum width of the
     *   items within the banner, so that we can determine the proper
     *   horizontal extent of a vertical banner.  This does almost exactly
     *   the same thing as measure_for_table, and in most cases simply
     *   calls that method to carry out the work.  However, in a few
     *   cases, we want to do something different; in particular, for tags
     *   that can be based on a percentage of the window size, we don't
     *   make a contribution to the banner sizes at all, since we won't
     *   know the window size until we know the banner size, hence we
     *   can't guess about our contribution.  
     */
    virtual void measure_for_banner(class CHtmlSysWin *win,
                                    class CHtmlTableMetrics *metrics)
    {
        /* for most cases, simply do the same thing as we'd do in a table */
        measure_for_table(win, metrics);
    }

    /* get/set the position of this display object */
    CHtmlRect get_pos() const { return pos_; }
    void set_pos(CHtmlRect *pos) { pos_ = *pos; }
    void set_pos(int left, int top, int right, int bottom)
        { pos_.set(left, top, right, bottom); }

    /* 
     *   Insert me after another display object in a list.  This can only
     *   be used to add a single item.  
     */
    void add_after(CHtmlDisp *prv)
        { nxt_ = prv->nxt_; prv->nxt_ = this; }

    /* 
     *   Insert a list after another display object in a list.  This adds
     *   me and any other items following me in my list. 
     */
    void add_list_after(CHtmlDisp *prv);

    /* make me the end of my chain by forgetting my next item */
    void clear_next_disp() { nxt_ = 0; }

    /* invalidate my area on the display */
    void inval(class CHtmlSysWin *win);

    /* 
     *   Invalidate my area on the display, limiting the invalidation to
     *   the given selection range.  By default, we'll just invalidate our
     *   area.  Some subclasses (text, in particular) should override this
     *   to limit the invalidation to the selected range.  
     */
    virtual void inval_range(class CHtmlSysWin *win,
                             unsigned long start_ofs, unsigned long end_ofs)
    {
        /* by default, invalidate our entire area */
        inval(win);
    }

    /* 
     *   Invalidate my area on the display only if I'm a link.  By
     *   default, I won't do anything, since default dispaly items aren't
     *   linked. 
     */
    virtual void inval_link(class CHtmlSysWin *) { }

    /* invalidate if appropriate for a change in the link 'clicked' status */
    virtual void on_click_change(class CHtmlSysWin *win)
        { inval(win); }

    /*
     *   Get my offset in the linear array containing the text stream.
     *   If this item doesn't contain text, return the text offset of the
     *   next item in the display list.  By default, items don't contain
     *   any text.  
     */
    virtual unsigned long get_text_ofs() const
    {
        return (nxt_ == 0 ? ULONG_MAX : nxt_->get_text_ofs());
    }

    /*
     *   Determine if the item contains a given text offset.  By default,
     *   we'll simply return false, since non-text items don't contain any
     *   text offsets. 
     */
    virtual int contains_text_ofs(unsigned long /*ofs*/) const
    {
        return FALSE;
    }

    /* determine if I intersect the given text offset range */
    virtual int overlaps_text_range(unsigned long l, unsigned long r) const
    {
        return get_text_ofs() >= l && get_text_ofs() <= r;
    }

    /*
     *   Determine if the item is past a given text offset.  If there's no
     *   next item, considers this item part of the range (hence returns
     *   false).
     *   
     *   If there's a next item, we return true if the next item's offset is
     *   greater than the given offset.  If the next item's offset is less
     *   than the given offset, we're obviously not beyond the offset because
     *   the next item is still within the range, hence we must return false.
     *   If the next item's offset is equal to the given offset, we haven't
     *   yet reached that offset, so we want to return false in this case as
     *   well.  
     */
    virtual int is_past_text_ofs(unsigned long ofs) const
    {
        return nxt_ == 0 ? FALSE : (nxt_->get_text_ofs() > ofs);
    }

    /*
     *   Get the position of a character within this item.  txtofs is the
     *   offset within the linear text array for the text stream of the
     *   character we're interested in.  By default, we don't have any
     *   text, so we'll just return our own position.  
     */
    virtual CHtmlPoint get_text_pos(class CHtmlSysWin *,
                                    unsigned long /*txtofs*/)
    {
        return CHtmlPoint(pos_.left, pos_.top);
    }

    /*
     *   Get the text offset of a character, given its position.  Since
     *   basic objects don't contain text, this will simply return the
     *   object's text offset by default.  
     */
    virtual unsigned long find_textofs_by_pos(class CHtmlSysWin *,
                                              CHtmlPoint) const
        { return get_text_ofs(); }

    /*
     *   Append my text to a string buffer.  By default, this does
     *   nothing, since default objects have no text.  
     */
    virtual void add_text_to_buf(CStringBuf *) const { }

    /* 
     *   add my text to a buffer, clipping the text to the given starting
     *   and ending offsets 
     */
    virtual void add_text_to_buf_clip(CStringBuf *buf,
                                      unsigned long startofs,
                                      unsigned long endofs) const { }

    /* 
     *   Get the hyperlink object associated with the item.  Returns null if
     *   this item isn't hyperlinked.  If the item is associated with a
     *   hypertext link, this returns the link object.
     *   
     *   The base display item isn't linkable itself, but it can inherit a
     *   division link from an enclosing DIV.  So, by default, we just look
     *   for an inherited DIV link.  
     */
    virtual class CHtmlDispLink *get_link(class CHtmlFormatter *formatter,
                                          int /*x*/, int /*y*/) const;

    /*
     *   Get the link I inherit from my enclosing DIV, if any.  This looks
     *   for the innermost enclosing DIV that has an associated link.  
     */
    class CHtmlDispLinkDIV *find_div_link(class CHtmlFormatter *formatter)
        const;

    /* 
     *   Get the container link.  If this item is part of a group of items
     *   linked to a containing <A> tag, this returns the containing <A>
     *   link.  This ignores any image map or other internal structure.  
     */
    virtual class CHtmlDispLink *get_container_link() const
        { return 0; }

    /*
     *   Determine if this item can qualify as an inexact hit when seeking
     *   the item nearest a mouse location.  Since inexact hits are used
     *   to determine text selection ranges, we only allow inexact matches
     *   of text items. 
     */
    virtual int allow_inexact_hit() const { return FALSE; }

    /*
     *   Determine if this item is in the background as far as mouse
     *   clicks are concerned.  If this returns true, we'll ignore this
     *   object for the purposes of locating a mouse click, and instead
     *   look further in the list for an overlaid object that contains the
     *   click.  Table cells use this, for example, so that mouse clicks
     *   go to the cell contents rather than to the cell object itself. 
     */
    virtual int is_in_background() const { return FALSE; }

    /*
     *   Get my ALT text, if I have any.  Some object types, such as
     *   IMG's, have an alternative textual representation that they can
     *   display when a textual description is needed. 
     */
    virtual const textchar_t *get_alt_text() const { return 0; }

protected:
    /*
     *   Basic deferred formatting implementation for floating margin items.
     *   The subclass must calculate the height and provide the margin
     *   alignment, and we'll set things up in the formatter accordingly.  
     */
    void basic_format_deferred(class CHtmlSysWin *win,
                               class CHtmlFormatter *formatter,
                               long line_spacing,
                               HTML_Attrib_id_t align, long height);
    
    /* position */
    CHtmlRect pos_;

private:
    /* next object in the list */
    CHtmlDisp *nxt_;

    /* line ID bit */
    int line_id_bit_ : 8;
};


/* ------------------------------------------------------------------------ */
/*
 *   A display item representing a DIV (division) tag.  This doesn't display
 *   anything; it's an internal placeholder to keep track of the contents of
 *   the division.  
 */
class CHtmlDispDIV: public CHtmlDisp
{
public:
    CHtmlDispDIV(unsigned long startofs, unsigned long endofs)
    {
        /* remember the text range */
        startofs_ = startofs;
        endofs_ = endofs;

        /* we don't know our parent DIV yet */
        parent_div_ = 0;

        /* we don't have a DIV-level hyperlink */
        div_link_ = 0;

        /* we don't know the last display item in the DIV yet */
        div_tail_ = 0;
    }

    /* get our associated hyperlink object */
    virtual class CHtmlDispLink *get_link(class CHtmlFormatter *formatter,
                                          int x, int y) const;
    
    /* get our start/end offset */
    unsigned long get_start_ofs() const { return startofs_; }
    unsigned long get_end_ofs() const { return endofs_; }

    /* we have no visual presence, so there's nothing to draw */
    virtual void draw(class CHtmlSysWin *, unsigned long, unsigned long,
                      int clicked)
        { }

    /* we don't take up any space */
    virtual int measure_width(class CHtmlSysWin *) { return 0; }
    virtual void get_vertical_space(class CHtmlSysWin *, int,
                                    int *above_base, int *below_base,
                                    int *total)
        { *above_base = *below_base = *total = 0; }

    /* get/set the parent DIV */
    CHtmlDispDIV *get_parent_div() const { return parent_div_; }
    void set_parent_div(CHtmlDispDIV *par) { parent_div_ = par; }

    /* get/set the DIV-level hyperlink object */
    class CHtmlDispLinkDIV *get_div_link() const { return div_link_; }
    void set_div_link(class CHtmlDispLinkDIV *link) { div_link_ = link; }

    /* 
     *   Get/set the last display item in the DIV.  The formatter gives us
     *   this information when building the display list; the DIV covers
     *   everything from the next item after the DIV to this last item. 
     */
    CHtmlDisp *get_div_tail() const { return div_tail_; }
    void set_div_tail(CHtmlDisp *tail) { div_tail_ = tail; }

private:
    /* the range of text offsets this division covers */
    unsigned long startofs_;
    unsigned long endofs_;

    /* the last display item in the DIV */
    CHtmlDisp *div_tail_;

    /* the parent DIV display item */
    CHtmlDispDIV *parent_div_;

    /* our DIV-level hyperlink object */
    class CHtmlDispLinkDIV *div_link_;
};

/* ------------------------------------------------------------------------ */
/*
 *   <NOBR> flag.  This doesn't actually display anything; it's just a flag
 *   that we can insert into the display stream to tell us that word-wrapping
 *   is being enabled or disabled.  This lets us take the wrapping mode into
 *   effect when calculating widths.  
 */
class CHtmlDispNOBR: public CHtmlDisp
{
public:
    CHtmlDispNOBR(int nobr) { nobr_ = nobr; }

    /* 
     *   Measure for a table or banner.  We don't take up any space, but we
     *   do need to set the current wrapping status in the metrics. 
     */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* we don't have any on-screen appearance */
    void draw(class CHtmlSysWin *, unsigned long, unsigned long, int) { }

    /* we don't take up any space */
    int measure_width(class CHtmlSysWin *) { return 0; }
    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above_base, int *below_base, int *total)
        { *above_base = *below_base = *total = 0; }

private:
    /* are we in a <NOBR> section? */
    int nobr_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Link object.  This item doesn't actually have any display presence,
 *   but is used to store the link information used by all of the linkable
 *   objects within a <A> ... </A> group.  Each linkable object refers
 *   back to this object for their link information.
 *   
 *   The link object keeps track of the first and last display items
 *   belonging to the group, so that group-wide operations (for example,
 *   highlighting all of the items in the group) can be done.
 *   
 *   Although this object doesn't have a display presence, it's
 *   implemented as a display item to simplify memory management - it goes
 *   in the display list along with all of the other display items, so it
 *   will be deleted along with them.  
 */

/* clicked states */
const int CHtmlDispLink_none = 0x00;                /* not clicked/hovering */
const int CHtmlDispLink_clicked = 0x01;           /* mouse is being clicked */
const int CHtmlDispLink_hover = 0x02;                  /* mouse is hovering */
const int CHtmlDispLink_divhover = 0x04;      /* mouse is hovering over DIV */
const int CHtmlDispLink_clickedoff = 0x08; /* mouse is clicked but off link */

class CHtmlDispLink: public CHtmlDisp
{
public:
    CHtmlDispLink(int style, int append, int noenter, CHtmlUrl *href,
                  const textchar_t *title, size_t titlelen)
    {
        /* remember our style */
        style_ = style;
        append_ = append;
        noenter_ = noenter;
        
        /* remember our link information */
        if (href != 0)
        {
            href_.set_url(href);
            nohref_ = FALSE;
        }
        else
            nohref_ = TRUE;
        title_.set(title, titlelen);

        /* contained items will be added later */
        first_ = last_ = 0;

        /* not yet clicked */
        clicked_ = FALSE;

        /* presume we're not using special hover settings */
        use_hover_fg_ = use_hover_bg_ = hover_underline_ = FALSE;
    }

    /*
     *   Is this a "clickable" link?  A clickable link is one with an active
     *   UI presence, where clicking on the link activates the hyperlink.  If
     *   a link isn't clickable, the UI should treat a click on the link as
     *   though it were a click on an ordinary, unlinked item.  
     */
    virtual int is_clickable_link() const { return TRUE; }

    /* set hover foreground color */
    void set_hover_fg(HTML_color_t color)
    {
        hover_fg_ = color;
        use_hover_fg_ = TRUE;
    }

    /* set hover background color */
    void set_hover_bg(HTML_color_t color)
    {
        hover_bg_ = color;
        use_hover_bg_ = TRUE;
    }

    /* set HOVER=UNDERLINE */
    void set_hover_underline() { hover_underline_ = TRUE; }

    /* 
     *   get the hover foreground/background color - returns true if the
     *   color is set, false if not 
     */
    int get_hover_fg(HTML_color_t *cp) const
    {
        *cp = hover_fg_;
        return use_hover_fg_;
    }
    int get_hover_bg(HTML_color_t *cp) const
    {
        *cp = hover_bg_;
        return use_hover_bg_;
    }

    /* add an underline while hovering? */
    int hover_underline() const { return hover_underline_; }

    /* links have no effect on line breaking, since they're invisible */
    CHtmlDisp *find_line_break(class CHtmlFormatter *, class CHtmlSysWin *,
                               long, class CHtmlLineBreak *)
        { return 0; }

    /* add an item linked to this item */
    void add_link_item(CHtmlDisp *item)
    {
        /* if we don't yet have a first item, remember it as the first */
        if (first_ == 0)
            first_ = item;

        /* it's the last one we've heard about so far */
        last_ = item;
    }

    /* get the first and last item in our list */
    class CHtmlDisp *get_first_link_item() const { return first_; }
    class CHtmlDisp *get_last_link_item() const { return last_; }

    /* get the link style (LINK_STYLE_xxx) */
    int get_style() const { return style_; }

    /* 
     *   Check if the link's contents are to be rendered in plain style
     *   (i.e., not using any special link rendering style).  The link is
     *   drawn plain if its style is PLAIN, or its style is HIDDEN and the
     *   mouse isn't currently over or clicking on the link.  
     */
    int is_plain() const
    {
        return (style_ == LINK_STYLE_PLAIN
                || (style_ == LINK_STYLE_HIDDEN
                    && !(clicked_
                         & (CHtmlDispLink_hover | CHtmlDispLink_clicked))));
    }

    /* check if the link is shown even when links are globally hidden */
    int is_forced() const { return style_ == LINK_STYLE_FORCED; }

    /* get current clicked state */
    int get_clicked() const { return clicked_; }

    /* 
     *   Set clicked state, and invalidate all items linked to me.
     *   clicked is a combination of the CHtmlDispLink_xxx states (see
     *   above).  
     */
    virtual void set_clicked(class CHtmlSysWin *win, int clicked);

    /*
     *   Set the clicked state for my sub-items only.  This doesn't notify
     *   our DIV.  
     */
    void set_clicked_sub(class CHtmlSysWin *win, int clicked);

    /*
     *   CHtmlDisp Overrides - mostly these are trivial implementations,
     *   since we don't do much as a display item 
     */

    /* no display presence, so drawing is trivial */
    void draw(class CHtmlSysWin *, unsigned long, unsigned long, int) { }

    /* no display area */
    int measure_width(class CHtmlSysWin *) { return 0; }
    int get_text_height(class CHtmlSysWin *) { return 0; }
    void get_vertical_space(class CHtmlSysWin *, int, int *above_base,
                            int *below_base, int *total)
    {
        *above_base = 0;
        *below_base = 0;
        *total = 0;
    }

    /* invalidate if I'm a link - I am, so invalidate me */
    void inval_link(class CHtmlSysWin *win) { inval(win); }

    /* get my command-entering attributes */
    int get_append() const { return append_; }
    int get_noenter() const { return noenter_; }

public:
    /* 
     *   Link information.  If nohref_ is set, it means that the link has
     *   no HREF - this is appropriate for AREA tags with the NOHREF
     *   attribute.
     */
    CHtmlUrl href_;
    int nohref_ : 1;
    CStringBuf title_;

protected:
    /* first and last display items linked to this item */
    CHtmlDisp *first_;
    CHtmlDisp *last_;

    /* hover colors, foreground and background */
    HTML_color_t hover_fg_;
    HTML_color_t hover_bg_;

    /* 
     *   flag indicating that I'm in the clicked state - i.e., the mouse
     *   button was clicked down over this item, the mouse is still over
     *   this item, and the mouse button is still being held down 
     */
    int clicked_;

    /* link style (LINK_STYLE_xxx) */
    unsigned int style_ : 2;

    /* 
     *   NOENTER - true means that we don't automatically enter the
     *   command, but allow the player to continue editing after adding
     *   our HREF 
     */
    int noenter_ : 1;

    /* 
     *   APPEND - true means that we append our HREF to the command line
     *   under construction, rather than clearing any existing command 
     */
    int append_ : 1;

    /* use the hover foreground/background colors */
    int use_hover_fg_ : 1;
    int use_hover_bg_ : 1;

    /* 
     *   HOVER=UNDERLINE - true means that we add an underline when the mouse
     *   is hovering on this link; false means that we leave the underlining
     *   status as it is while hovering. 
     */
    int hover_underline_ : 1;
};

/*
 *   A special hyperlink object for a DIV tag.  A DIV link isn't a real link;
 *   it's just a proxy for the links within the DIV group.  
 */
class CHtmlDispLinkDIV: public CHtmlDispLink
{
public:
    /* 
     *   create - a DIV link isn't a real hyperlink, so it doesn't have an
     *   HREF or any display style; it's just a proxy for all of the actual
     *   links within the division 
     */
    CHtmlDispLinkDIV(class CHtmlDispDIV *div);

    /* 
     *   DIV links aren't clickable.  These links are for hovering purposes
     *   only, and don't have any effect when clicked.  For the purposes of
     *   clicking, an item linked to a DIV link looks like an ordinary,
     *   unlinked item.  
     */
    virtual int is_clickable_link() const { return FALSE; }

    /* set the clicked state */
    virtual void set_clicked(class CHtmlSysWin *win, int clicked);

protected:
    /* the DIV I belong to */
    class CHtmlDispDIV *div_;
};


#if 0
/* ------------------------------------------------------------------------ */
/*
 *   Delegator display item.  This display item type can be subclassed to
 *   implement a plug-in class that can override virtuals from the
 *   CHtmlDisp base class for any instance of any CHtmlDisp subclass.  For
 *   methods not overridden by a subclass of this class, we call the
 *   corresponding method in the underlying object.
 */
class CHtmlDispDelegator: public CHtmlDisp
{
public:
    CHtmlDispDelegator(CHtmlDisp *underlying_object)
    {
        /* remember the underlying object */
        delegate_ = underlying_object;
    }

    ~CHtmlDispDelegator()
    {
        /* delete the underlying object */
        delete delegate_;
    }

    void draw(class CHtmlSysWin *win,
              unsigned long sel_start, unsigned long sel_end, int clicked)
    {
        delegate_->draw(win, sel_start, sel_end, clicked);
    }

    Html_csrtype_t get_cursor_type(class CHtmlSysWin *win,
                                   class CHtmlFormatter *fmt, int x, int y,
                                   int more_mode)
        const
    {
        return delegate_->get_cursor_type(win, fmt, x, y, more_mode);
    }

    int measure_width(class CHtmlSysWin *win)
    {
        return delegate_->measure_width(win);
    }

    int get_text_height(class CHtmlSysWin *win)
    {
        return delegate_->get_text_height(win);
    }

    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base,
                            int *total)
    {
        delegate_->get_vertical_space(win, text_height, above_base,
                                      below_base, total);
    }

    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height)
    {
        CHtmlRect pos;
        
        /* set the position in the delegate */
        delegate_->set_line_pos(win, left, top, total_height,
                                text_base_offset, max_text_height);

        /* copy the delegate's position */
        pos = delegate_->get_pos();
        set_pos(&pos);
    }
    
    void format_deferred(class CHtmlSysWin *win,
                         class CHtmlFormatter *formatter, long line_spacing)
    {
        delegate_->format_deferred(win, formatter, line_spacing);
    }

    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos)
    {
        return delegate_line_break(formatter, delegate_->find_line_break(
            formatter, win, space_avail, break_pos), break_pos);
    }
    
    CHtmlDisp *do_line_break(class CHtmlFormatter *formatter,
                             class CHtmlSysWin *win,
                             class CHtmlLineBreak *break_pos)
    {
        return delegate_line_break(formatter, delegate_->do_line_break(
            formatter, win, break_pos), break_pos);
    }

    CHtmlDisp *do_line_break_skipsp(class CHtmlFormatter *formatter,
                                    class CHtmlSysWin *win)
    {
        "Not Implemented (but this class isn't currently used)!";
    }

    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info)
    {
        delegate_->find_trailing_whitespace(info);
    }

    void remove_trailing_whitespace(class CHtmlSysWin *win, void *pos)
    {
        delegate_->remove_trailing_whitespace(win, pos);
    }

    long measure_trailing_ws_width(class CHtmlSysWin *win) const
    {
        return delegate_->measure_trailing_ws_width(win);
    }

    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics)
    {
        delegate_->measure_for_table(win, metrics);
    }

    void on_click_change(class CHtmlSysWin *win)
    {
        delegate_->on_click_change(win);
    }

    unsigned long get_text_ofs() const
    {
        return delegate_->get_text_ofs();
    }

    int contains_text_ofs(unsigned long ofs) const
    {
        return delegate_->contains_text_ofs(ofs);
    }
    
    virtual int overlaps_text_range(unsigned long l, unsigned long r) const
    {
        return delegate_->overlaps_text_range(l, r);
    }

    int is_past_text_ofs(unsigned long ofs) const
    {
        return delegate_->is_past_text_ofs(ofs);
    }

    CHtmlPoint get_text_pos(class CHtmlSysWin *win,
                            unsigned long txtofs)
    {
        return delegate_->get_text_pos(win, txtofs);
    }
    
    unsigned long find_textofs_by_pos(CHtmlSysWin *win, CHtmlPoint pos)
        const
    {
        return delegate_->find_textofs_by_pos(win, pos);
    }
    
    void add_text_to_buf(CStringBuf *buf) const
    {
        delegate_->add_text_to_buf(buf);
    }

    void add_text_to_buf_clip(CStringBuf *buf, unsigned long startofs,
                              unsigned long endofs) const
    {
        delegate_->add_text_to_buf_clip(buf, startofs, endofs);
    }

    int allow_inexact_hit() const
    {
        return delegate_->allow_inexact_hit();
    }

    int is_in_background() const
    {
        return delegate_->is_in_background();
    }


protected:
    /*
     *   Delegate the result of a line break within this item.  We apply
     *   this to the return value of any line breaking routine.  A line
     *   break routine can return null, which we'll map to null; it can
     *   return the underlying item, in which case we'll return 'this',
     *   since we always want to cover references to the underlying item;
     *   or it can return an entirely new item, in which case we'll create
     *   a clone of this delegator to cover the new item. 
     */
    CHtmlDisp *delegate_line_break(class CHtmlFormatter *formatter,
                                   CHtmlDisp *line_start,
                                   class CHtmlLineBreak *break_pos);

    /*
     *   Clone myself, setting the underlying delegate object to the given
     *   object.  This must be overridden by each concrete subclass to
     *   create an object of its type and set up the new object with the
     *   same values.  
     */
    virtual CHtmlDisp *clone_delegator(class CHtmlFormatter *formatter,
                                       CHtmlDisp *delegate) = 0;
    
    /* 
     *   the original object to which we delegate methods not overridden
     *   by our subclasses 
     */
    CHtmlDisp *delegate_;
};
#endif // removed


/* ------------------------------------------------------------------------ */
/*
 *   "Body" display object.  This is a pseudo-display object representing the
 *   entire document body.  We keep one body object per window.  
 */
class CHtmlDispBody: public CHtmlDisp, public CHtmlDispDisplaySite
{
public:
    CHtmlDispBody(class CHtmlSysWin *win)
        : CHtmlDisp(), CHtmlDispDisplaySite(win, &pos_, 0)
    {
        /* I'm a fake display object that takes up no actual screen space */
        pos_.set(0, 0, 0, 0);
    }

    /* 
     *   Animated image display site - invalidate a portion of the display
     *   site.  We must override this, because we need to handle this
     *   specially through our window: only our window knows exactly how it
     *   draws the background image.  
     */
    void dispsite_inval(unsigned int x, unsigned int y,
                        unsigned int width, unsigned int height);

    /* forward unset_win() on to the display site */
    void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* forward cancel_playback() on to the display site */
    void cancel_playback() { CHtmlDispDisplaySite::cancel_playback(); }

    /* we're not a real display item, so we don't do a bunch of things */
    void draw(class CHtmlSysWin *, unsigned long, unsigned long, int) { }
    int measure_width(class CHtmlSysWin *) { return 0; }
    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above, int *below, int *total)
        { *above = *below = *total = 0; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Text display object.  Each text object keeps track of its font and
 *   text string.  
 */
class CHtmlDispText: public CHtmlDisp
{
public:
    CHtmlDispText(class CHtmlSysWin *win, class CHtmlSysFont *font,
                  const textchar_t *txt, size_t len,
                  unsigned long txtofs);

    /* use the I-beam cursor when over text */
    Html_csrtype_t get_cursor_type(class CHtmlSysWin *,
                                   class CHtmlFormatter *, int, int, int)
        const { return HTML_CSRTYPE_IBEAM; }

    /* get my width */
    int measure_width(class CHtmlSysWin *win);

    /* invalidate a range */
    virtual void inval_range(class CHtmlSysWin *win,
                             unsigned long start_ofs, unsigned long end_ofs);

    /* draw the text */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* do a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);

    /* do a break at a position we previously found */
    CHtmlDisp *do_line_break(class CHtmlFormatter *formatter,
                             class CHtmlSysWin *win,
                             class CHtmlLineBreak *break_pos);

    /* do a break, skipping whitespace from a preceding item */
    CHtmlDisp *do_line_break_skipsp(class CHtmlFormatter *formatter,
                                    class CHtmlSysWin *win);

    /* find/remove/hide trailing whitespace */
    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info);
    void remove_trailing_whitespace(class CHtmlSysWin *win, void *pos);
    void hide_trailing_whitespace(class CHtmlSysWin *win, void *pos);

    /* measure trailing whitespace */
    long measure_trailing_ws_width(class CHtmlSysWin *win) const;

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* get the text height */
    int get_text_height(class CHtmlSysWin *win);

    /* get my overall vertical space needs */
    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base, int *total);

    /* set my position */
    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height);

    /* get my text offset */
    unsigned long get_text_ofs() const { return txtofs_; }

    /* see if I contain a given text offset */
    int contains_text_ofs(unsigned long ofs) const
        { return (ofs >= txtofs_ && ofs < txtofs_ + len_); }

    int overlaps_text_range(unsigned long l, unsigned long r) const
        { return (txtofs_ <= r && txtofs_ + len_ >= l); }

    /* see if I'm past a given text offset */
    int is_past_text_ofs(unsigned long ofs) const
        { return (txtofs_ > ofs); }

    /* 
     *   this is a text item, so allow inexact hits when seeking the items
     *   contained in a selection range 
     */
    virtual int allow_inexact_hit() const { return TRUE; }

    /* get the position of a character within this item */
    CHtmlPoint get_text_pos(class CHtmlSysWin *win, unsigned long txtofs);

    /* find the text offset of a given position */
    unsigned long find_textofs_by_pos(CHtmlSysWin *win,
                                      CHtmlPoint pos) const;

    /* add my text to a buffer */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append(txt_, len_); }

    /* 
     *   add my text to a buffer, clipping the text to the given starting
     *   and ending offsets 
     */
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long startofs,
                              unsigned long endofs) const;

    /* get my text length */
    size_t get_text_len() const { return len_; }

protected:
    /* do some special construction-time initialization for linked text */
    void linked_text_cons(class CHtmlSysWin *win, class CHtmlDispLink *link);

    /* 
     *   service routine for linkable subclasses: draw the text in the style
     *   appropriate for a linked item 
     */
    void draw_as_link(class CHtmlSysWin *win,
                      unsigned long sel_start, unsigned long sel_end,
                      class CHtmlDispLink *link);

    /* 
     *   service routine for linkable subclasses: get the cursor type over
     *   this as a linked text item 
     */
    Html_csrtype_t get_cursor_type_for_link(
        class CHtmlSysWin *win, class CHtmlFormatter *formatter,
        int x, int y, int more_mode) const;

    /* 
     *   Find a line break in character-wrapping mode.  This method is
     *   provided so that each subclass of the text item can easily
     *   implement a separate subclass that wraps in character-wrapping
     *   mode; to do this, subclass the subclass, and implement
     *   find_line_break() as simply a call to this routine.  
     */
    CHtmlDisp *find_line_break_char(class CHtmlFormatter *formatter,
                                    class CHtmlSysWin *win,
                                    unsigned long space_avail,
                                    class CHtmlLineBreak *break_pos);

    /*
     *   Get the effective last character of our text, for the purposes of
     *   calculating line breaks.  By default, we'll return the last
     *   character from our text.
     *   
     *   For subclasses that implement character-wrap mode, this should be
     *   overridden to return a space instead of the actual last character.
     *   In character-wrap mode, no matter what our last character is, we
     *   can break to its right; we can indicate this by returning a space
     *   as our last character, since in any wrapping mode we can break to
     *   the right of a space.  
     */
    virtual textchar_t get_break_pos_last_char()
        { return *(txt_ + len_ - 1); }

    /*
     *   Determine if we allow a line break at the given character position.
     *   prev_char is the previous character, and cur_char is the current
     *   character.
     *   
     *   Our default implementation is for word-wrapping mode.  We allow
     *   breaking at a space, or after a hyphen not followed by a another
     *   hyphen.
     *   
     *   Subclasses can override this to implement character-wrap mode,
     *   which should generally allow breaking anywhere.  
     */
    virtual int allow_line_break_at(textchar_t prev_char, textchar_t cur_char)
    {
        /* 
         *   If this is a space, or the previous character was a hyphen and
         *   this character isn't a hyphen, we can break here.  
         */
        return is_space(cur_char) || (prev_char == '-' && cur_char != '-');
    }

    /*
     *   Determine if we allow line breaking after the given character.  Our
     *   default implementation is for word-wrapping mode, so we allow line
     *   breaking after a space, or after a hyphen not followed by another
     *   hyphen. 
     */
    virtual int allow_line_break_after(textchar_t prev_char,
                                       textchar_t cur_char)
    {
        /* 
         *   if the previous character is a space, or the previous character
         *   is a hyphen and the current character isn't a space, we can
         *   break after the previous character 
         */
        return is_space(prev_char) || (prev_char == '-' && cur_char != '-');
    }

    /* do a line break at a given position */
    CHtmlDisp *break_at_pos(class CHtmlFormatter *formatter,
                            class CHtmlSysWin *win,
                            const textchar_t *last_break_pos);

    /* create a new display item of the same type for breaking the line */
    virtual CHtmlDispText *create_disp_for_break(
        class CHtmlFormatter *formatter, class CHtmlSysWin *win,
        class CHtmlSysFont *font, const textchar_t *txt, size_t len,
        unsigned long txtofs)
    {
        /* default implementation returns new item of base text type */
        return new (formatter) CHtmlDispText(win, font, txt, len, txtofs);
    }

    /*
     *   Draw the text.  If 'link' is true, draw with the appropriate
     *   style for a hypertext link.  If 'clicked' is true, the mouse
     *   button is being held down with the mouse over the item. 
     */
    virtual void draw_text(class CHtmlSysWin *win,
                           unsigned long sel_start, unsigned long sel_end,
                           class CHtmlDispLink *link, int clicked,
                           const CHtmlRect *pos);

    /* get the font to use for drawing our text */
    class CHtmlSysFont *get_draw_text_font(class CHtmlSysWin *win,
                                           class CHtmlDispLink *link,
                                           int clicked);

    /*
     *   Draw a portion of the text, with optional highlighting.  Draws
     *   the lesser of len and *rem; updates *p, *pos, *rem, and *ofs
     *   according to the amount of text actually drawn. 
     */
    void draw_text_part(class CHtmlSysWin *win, int hilite,
                        CHtmlRect *pos, const textchar_t **p,
                        size_t *rem, unsigned long *ofs, unsigned long len,
                        class CHtmlSysFont *font);

    /* font for displaying our text */
    class CHtmlSysFont *font_;

    /* our text, and its address in the formatter's text array */
    const textchar_t *txt_;
    unsigned long txtofs_;
    unsigned short len_;

    /* 
     *   Display length - this is the amount of text we're actually
     *   displaying, which can be less than our actual underlying text
     *   length (usually because we're suppressing the display of trailing
     *   spaces).  
     */
    unsigned short displen_;

    /* portion of text above the text baseline */
    unsigned short ascent_ht_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Macro to define a character-wrapping version of a CHtmlDispText
 *   subclass.  Each CHtmlDispText subclass should define a subclass of
 *   itself for character-wrapping mode.  This version requires the explicit
 *   list of formals and actuals to the constructor.  
 */
#define DEF_CHARWRAP_SUBCLASS_CT(cls, ct_formals, ct_actuals, cl_actuals) \
class cls##CharWrap: public cls \
{ \
public: \
    /* constructor */ \
    cls##CharWrap ct_formals : cls ct_actuals { } \
    \
    /* allow wrapping to our right by indicating we end with a space */ \
    virtual textchar_t get_break_pos_last_char() \
        { return ' '; } \
    \
    /* allow breaking at any of our characters */ \
    virtual int allow_line_break_at(textchar_t, textchar_t) \
        { return TRUE; } \
    \
    /* allow breaking after any character */ \
    virtual int allow_line_break_after(textchar_t, textchar_t) \
        { return TRUE; } \
    \
    /* create a new display item of the same type for breaking the line */ \
    virtual CHtmlDispText *create_disp_for_break( \
        class CHtmlFormatter *formatter, class CHtmlSysWin *win, \
        class CHtmlSysFont *font, const textchar_t *txt, size_t len, \
        unsigned long txtofs) \
    { \
        /* return a new item of the same type */ \
        return new (formatter) cls##CharWrap cl_actuals; \
    } \
}

    
/* define a char-wrapping subclass with the standard constructor */
#define DEF_CHARWRAP_SUBCLASS(cls) \
    DEF_CHARWRAP_SUBCLASS_CT(cls, \
                             (class CHtmlSysWin *win, \
                              class CHtmlSysFont *font, \
                              const textchar_t *txt, size_t len, \
                              unsigned long txtofs), \
                             (win, font, txt, len, txtofs), \
                             (win, font, txt, len, txtofs))


/* ------------------------------------------------------------------------ */
/*
 *   Define a character-wrapping version of the base text display item.
 *   We'll create one of these whenever we create a regular text item and
 *   the formatter is currently in character-wrapping mode.  
 */
DEF_CHARWRAP_SUBCLASS(CHtmlDispText);


/* ------------------------------------------------------------------------ */
/*
 *   Define some add-in methods for linked text subclasses.  To define a
 *   linked version of a text subclass, add this macro to the 'public'
 *   section of the class definition.  This will define some overrides for
 *   CHtmlDispText virtuals, and declare a link_ member to store the
 *   CHtmlDispLink pointer for the link.  
 */
#define LINKED_TEXT_METHODS \
    /* use the hand cursor when over a link */ \
    Html_csrtype_t get_cursor_type(class CHtmlSysWin *win, \
                                   class CHtmlFormatter *formatter, \
                                   int x, int y, int more_mode) const \
    { \
        return get_cursor_type_for_link(win, formatter, x, y, more_mode); \
    } \
    \
    /* draw as a link */ \
    void draw(class CHtmlSysWin *win, unsigned long sel_start, \
              unsigned long sel_end, int /*clicked*/) \
        { draw_as_link(win, sel_start, sel_end, link_); } \
    \
    /* invalidate if I'm a link - I am, so invalidate me */ \
    void inval_link(class CHtmlSysWin *win) { inval(win); } \
    \
    /* get my link object */ \
    CHtmlDispLink *get_link(class CHtmlFormatter *, int, int) const \
        { return link_; } \
    \
    /* get my containing link object */ \
    class CHtmlDispLink *get_container_link() const \
        { return link_; } \
    \
    /* get my ALT text - return the TITLE text for the underlying link */ \
    virtual const textchar_t *get_alt_text() const \
    { \
        /* if I have a link and it has a TITLE, return the link's title */ \
        if (link_ != 0 && link_->title_.get() != 0) \
            return link_->title_.get(); \
        \
        /* return the inherited ALT text */ \
        return CHtmlDispText::get_alt_text(); \
    } \
    \
protected: \
    /* link object */ \
    CHtmlDispLink *link_; \
    \
public:

/*
 *   Linked-text initialization.  Remember our link object in our 'link_'
 *   member, and perform additional construction-time intialization.  This
 *   must be called in the subclass constructor for all linked text
 *   subclasses.  
 */
#define linked_text_init(win, link) \
    ((link_ = (link)), linked_text_cons(win, link))

/* ------------------------------------------------------------------------ */
/*
 *   Link text display object.  When text appears within a <A HREF> ...
 *   </A> region, we'll create one of these objects instead of the
 *   standard text display object.
 */
class CHtmlDispTextLink: public CHtmlDispText
{
public:
    CHtmlDispTextLink(class CHtmlSysWin *win, class CHtmlSysFont *font,
                      const textchar_t *txt, size_t len,
                      unsigned long txtofs, class CHtmlDispLink *link)
        : CHtmlDispText(win, font, txt, len, txtofs)
    {
        /* initialize the linked text item */
        linked_text_init(win, link);
    }

    /* define the linked text methods */
    LINKED_TEXT_METHODS

    /* create a new display item of the same type for breaking the line */
    CHtmlDispText *create_disp_for_break
        (class CHtmlFormatter *formatter, class CHtmlSysWin *win,
         class CHtmlSysFont *font, const textchar_t *txt, size_t len,
         unsigned long txtofs)
    {
        /* default implementation returns new item of base text type */
        return new (formatter)
            CHtmlDispTextLink(win, font, txt, len, txtofs, link_);
    }
};

/*
 *   Define a character-wrapping version of the linked text display item 
 */
DEF_CHARWRAP_SUBCLASS_CT(CHtmlDispTextLink,
                         (class CHtmlSysWin *win, class CHtmlSysFont *font,
                          const textchar_t *txt, size_t len,
                          unsigned long txtofs, class CHtmlDispLink *link),
                         (win, font, txt, len, txtofs, link),
                         (win, font, txt, len, txtofs, link_));

/* ------------------------------------------------------------------------ */
/*
 *   Preformatted text display object.  This acts mostly like a normal
 *   text object, but doesn't break at the margins; instead, it just runs
 *   on as though it can't break internally.  Preformatted text should
 *   always be set on separate lines from normal text, so it should never
 *   be necessary to consider line breaks in any line containing
 *   preformatted text.  
 */
class CHtmlDispTextPre: public CHtmlDispText
{
public:
    CHtmlDispTextPre(class CHtmlSysWin *win, class CHtmlSysFont *font,
                     const textchar_t *txt, size_t len,
                     unsigned long txtofs)
        : CHtmlDispText(win, font, txt, len, txtofs) { }

    /* do a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);
};


/* ------------------------------------------------------------------------ */
/*
 *   Linkable subclass of preformatted text object 
 */

class CHtmlDispTextPreLink: public CHtmlDispTextPre
{
public:
    CHtmlDispTextPreLink(class CHtmlSysWin *win, class CHtmlSysFont *font,
                         const textchar_t *txt, size_t len,
                         unsigned long txtofs, CHtmlDispLink *link)
        : CHtmlDispTextPre(win, font, txt, len, txtofs)
    {
        /* initialize the linked text item */
        linked_text_init(win, link);
    }

    /* add in the linked text methods */
    LINKED_TEXT_METHODS
};

/* ------------------------------------------------------------------------ */
/*
 *   Display item for text of a command input line.  This behaves mostly
 *   the same as a normal text display item, but adds the ability to
 *   compare itself against another text display item and invalidate the
 *   screen where they differ.  
 */
class CHtmlDispTextInput: public CHtmlDispText
{
public:
    CHtmlDispTextInput(class CHtmlSysWin *win, class CHtmlSysFont *font,
                       const textchar_t *txt, size_t len,
                       unsigned long txtofs);

    ~CHtmlDispTextInput();

    /* compare myself to another text display list */
    void diff_input_lists(CHtmlDispTextInput *other,
                          class CHtmlSysWin *win);

    /* create a new display item of the same type for breaking the line */
    virtual CHtmlDispText *create_disp_for_break(
        class CHtmlFormatter *, class CHtmlSysWin *win,
        class CHtmlSysFont *font, const textchar_t *txt, size_t len,
        unsigned long txtofs)
    {
        /* 
         *   Create and remember the new item as my continuation item.
         *   Since this item is open for editing, allocate space in system
         *   memory (i.e., use null as the 'formatter' placement argument
         *   to operator new), because items in the formatter's heap don't
         *   get individually deleted, and we may be making many changes
         *   during active editing.  
         */
        input_continuation_ =
            new ((class CHtmlFormatter *)0) CHtmlDispTextInput(
                win, font, txt, len, txtofs);

        /* return the new item */
        return input_continuation_;
    }

private:
    /*
     *   invalidate from a given text offset to the end of the line on
     *   the display
     */
    void inval_eol(class CHtmlSysWin *win, int textofs);

    /*
     *   invalidate from below this item to the bottom of the window on
     *   the display 
     */
    void inval_below(class CHtmlSysWin *win);

    /*
     *   next item in this same input list - applies when the input line
     *   is broken across multiple lines on the display 
     */
    CHtmlDispTextInput *input_continuation_;
};

/*
 *   Define a character-wrapping version of the input text display item 
 */
DEF_CHARWRAP_SUBCLASS(CHtmlDispTextInput);

/* ------------------------------------------------------------------------ */
/*
 *   Display item for text in a debugger source file window.  This behaves
 *   very much like a preformatted text item, except that it records the
 *   line number in the source file of the line (so that it can be related
 *   back to information in the .GAM file's debug records), and displays
 *   an informational icon in the margin showing the breakpoint and
 *   execution status of the line.  
 */
class CHtmlDispTextDebugsrc: public CHtmlDispTextPre
{
public:
    CHtmlDispTextDebugsrc(class CHtmlSysWin *win, class CHtmlSysFont *font,
                          const textchar_t *txt, size_t len,
                          unsigned long txtofs, unsigned long linenum);

    /* get the cursor type */
    Html_csrtype_t get_cursor_type(class CHtmlSysWin *,
                                   class CHtmlFormatter *formatter,
                                   int x, int y, int more_mode) const;

    /* draw the line of text */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* find text given a mouse position */
    unsigned long find_textofs_by_pos(class CHtmlSysWin *, CHtmlPoint) const;

    /* find the position of the given text */
    CHtmlPoint get_text_pos(class CHtmlSysWin *win, unsigned long txtofs);

    /* get the debugger source file position for this item */
    unsigned long get_debugsrcpos() const { return line_num_; }

    /* 
     *   Set the current line status.  Invalidates the line's icon on the
     *   screen so that it redraws with the new status. 
     */
    void set_dbg_status(class CHtmlSysWin *win, unsigned int newstat);

    /* get my text into a buffer */
    void add_text_to_buf(CStringBuf *buf) const;
    void add_text_to_buf_clip(CStringBuf *buf,
                              unsigned long startofs,
                              unsigned long endofs) const;

private:
    /* source file line number of this line of text */
    unsigned long line_num_;

    /* debugger line status */
    unsigned int dbgstat_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Display item for text in a Text Grid banner window.  This behaves very
 *   much like a preformatted text item, except that it records the row
 *   number in the text grid, and we can dynamically add to and overwrite the
 *   text in the display item.  
 */
class CHtmlDispTextGrid: public CHtmlDisp
{
public:
    CHtmlDispTextGrid(class CHtmlSysWin *win, class CHtmlSysFont *font,
                      unsigned long ofs);
    ~CHtmlDispTextGrid();

    /* draw the line of text */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* 
     *   Writes text into the line, starting at the given column position.
     *   Returns true if we expand the row, which will affect text offsets in
     *   subsequent rows, false if we merely overwrite text that was already
     *   in the row. 
     */
    int write_text(class CHtmlSysWin *win, class CHtmlSysFont *font,
                   int col, const textchar_t *txt, size_t len);

    /* change our font, recalculating our size */
    void set_font(class CHtmlSysWin *win, class CHtmlSysFont *font);

    /* get my text offset */
    virtual unsigned long get_text_ofs() const { return ofs_; }

    /* set my text offset */
    void set_text_ofs(unsigned long ofs) { ofs_ = ofs; }

    /* get the number of characters in my text */
    int get_text_columns() const { return max_col_; }

    /* 
     *   check to see if we contain a text offset - add one to our range for
     *   a newline at the end of the line 
     */
    virtual int contains_text_ofs(unsigned long ofs) const
        { return ofs >= ofs_ && ofs < ofs_ + max_col_ + 1; }

    /* determine if I'm past a given offset */
    int is_past_text_ofs(unsigned long ofs) const
        { return (ofs_ > ofs); }

    /* determine if I overlap a given text range */
    virtual int overlaps_text_range(unsigned long l, unsigned long r) const
        { return ofs_ <= r && ofs_ + max_col_ + 1 > l; }

    /* get my width */
    int measure_width(class CHtmlSysWin *)
    {
        /* we keep our position up to date with our current text width */
        return pos_.right - pos_.left;
    }

    /* get my text height */
    virtual int get_text_height(class CHtmlSysWin *) { return ascent_ht_; }

    /* get my vertical measurements */
    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above_base, int *below_base, int *total)
    {
        /* our total size is always up to date in our position */
        *total = pos_.bottom - pos_.top;

        /* our ascent height is the amount above the baseline */
        *above_base = ascent_ht_;

        /* the balance is below the baseline */
        *below_base = *total - ascent_ht_;
    }

    /* get the character at the given text offset */
    textchar_t get_char_at_ofs(unsigned long ofs) const
    {
        /* if it's outside of our offset range, it's not ours */
        if (ofs < ofs_ || ofs > ofs_ + max_col_)
            return '\0';

        /* if it's just past our last character, it's a newline */
        if (ofs == ofs_ + max_col_)
            return '\n';

        /* return the character from our buffer, if we have a buffer */
        return buf_.get() != 0 ? buf_.get()[ofs - ofs_] : '\0';
    }

    /* use an I-beam cursor over our text */
    virtual Html_csrtype_t
        get_cursor_type(class CHtmlSysWin *, class CHtmlFormatter *,
                        int /*x*/, int /*y*/, int /*more_mode*/) const
        { return HTML_CSRTYPE_IBEAM; }

    /* invalidate a text range */
    virtual void inval_range(class CHtmlSysWin *win,
                             unsigned long start_ofs, unsigned long end_ofs);

    /* get the position of a character within the item */
    virtual CHtmlPoint get_text_pos(class CHtmlSysWin *win,
                                    unsigned long txtofs);

    /* get the text offset of a character given a position */
    virtual unsigned long find_textofs_by_pos(class CHtmlSysWin *win,
                                              CHtmlPoint pt) const;

    /* append my text to a string buffer */
    virtual void add_text_to_buf(CStringBuf *buf) const
    {
        /* if I have anything to append, add it */
        if (buf_.get() != 0)
        {
            /* add my text */
            buf->append(buf_.get());

            /* append a newline */
            buf->append("\n");
        }
    }

    /* get my text, clipping to the given offsets */
    virtual void add_text_to_buf_clip(CStringBuf *buf,
                                      unsigned long startofs,
                                      unsigned long endofs) const
    {
        size_t ofs;
        size_t len;
        int add_nl;
        
        /* start with our entire range */
        ofs = 0;
        len = max_col_;

        /* ignore if it's entirely outside our range, or we have no data */
        if (startofs >= ofs_ + max_col_ + 1
            || endofs < ofs_
            || buf_.get() == 0)
            return;

        /* limit on the left if the starting offset is after our start */
        if (startofs > ofs_)
        {
            /* adjust our offset and length to skip the leading part */
            ofs += startofs - ofs_;
            len -= startofs - ofs_;
        }

        /* limit on the right if the ending offset is before our end */
        if (endofs < ofs_ + max_col_)
            len -= ofs_ + max_col_ - endofs;

        /* add a newline if they want the char just past our last column */
        add_nl = (endofs >= ofs_ + max_col_);

        /* append the data */
        buf->append(buf_.get() + ofs, len);

        /* append the newline if desired */
        if (add_nl)
            buf->append("\n");
    }

private:
    /* draw a portion of our text in the given selection range */
    void draw_part(CHtmlSysWin *win, int hilite,
                   unsigned long l, unsigned long r);

    /* our text buffer */
    CStringBuf buf_;

    /* array of character colors - one per character cell */
    struct textgrid_cellcolor_t *colors_;
    size_t colors_max_;

    /* our font */
    class CHtmlSysFont *font_;

    /* ascender height of my font */
    unsigned short ascent_ht_;

    /* the highest column number we've filled in */
    int max_col_;

    /* my starting text offset */
    unsigned long ofs_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base class for special text types.  These are used for text-like display
 *   elements that display something other than ordinary text, such as
 *   non-breaking spaces, soft hyphens, and specific-width spaces.  
 */
class CHtmlDispTextSpecial: public CHtmlDispText
{
public:
    /* refer the constructor to the base text constructor */
    CHtmlDispTextSpecial(class CHtmlSysWin *win, class CHtmlSysFont *font,
                         const textchar_t *txt, size_t len,
                         unsigned long txtofs)
        : CHtmlDispText(win, font, txt, len, txtofs)
    {
    }

    /* 
     *   Get my width.  Assume that we've calculated our width in advance, so
     *   that we can simply use our position any time we're asked. 
     */
    int measure_width(class CHtmlSysWin *) { return pos_.right - pos_.left; }

    /* invalidate a range */
    void inval_range(class CHtmlSysWin *win,
                     unsigned long start_ofs, unsigned long end_ofs);

    /* get the position of the given text */
    CHtmlPoint get_text_pos(CHtmlSysWin *win,
                            unsigned long txtofs)
    {
        /* if the offset is equal to my offset, it's my left edge */
        if (txtofs == txtofs_)
            return CHtmlPoint(pos_.left, pos_.top);

        /* 
         *   if my length is exactly one, and they want the position of the
         *   next character, use my full width 
         */
        if (len_ == 1)
            return CHtmlPoint(pos_.right, pos_.top);

        /* in other cases, use the inherited code */
        return CHtmlDispText::get_text_pos(win, txtofs);
    }

    /* get the text offset of a character given a position */
    unsigned long find_textofs_by_pos(class CHtmlSysWin *win,
                                      CHtmlPoint pt) const
    {
        /* if we have no text, just return out text offset */
        if (len_ == 0)
            return txtofs_;

        /* 
         *   if we're exactly one character long, and the position is within
         *   my area, base the calculation on our width alone 
         */
        if (len_ == 1 && pos_.contains(pt))
        {
            /* 
             *   if it's in my left half, return my text offset; otherwise,
             *   return the next text offset 
             */
            return txtofs_
                + (pt.x - pos_.left > (pos_.right - pos_.left)/2 ? 1 : 0);
        }

        /* in other cases, use the inherited handling */
        return CHtmlDispText::find_textofs_by_pos(win, pt);
    }

protected:
};

/*
 *   Soft Hyphen display item. 
 */
class CHtmlDispSoftHyphen: public CHtmlDispTextSpecial
{
public:
    CHtmlDispSoftHyphen(class CHtmlSysWin *win, class CHtmlSysFont *font,
                        const textchar_t *txt, size_t len,
                        unsigned long txtofs)
        : CHtmlDispTextSpecial(win, font, txt, len, txtofs)
    {
        /* we're not showing as a hyphen yet */
        vis_ = FALSE;
    }

    /* find a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);

    /* do a line break */
    CHtmlDisp *do_line_break(class CHtmlFormatter *formatter,
                             class CHtmlSysWin *win,
                             class CHtmlLineBreak *break_pos);

    /* do a line break from a previous item, skipping whitespace */
    CHtmlDisp *do_line_break_skipsp(class CHtmlFormatter *formatter,
                                    class CHtmlSysWin *win)
    {
        /* we don't have any whitespace, so just do our normal line break */
        return do_line_break(formatter, win, 0);
    }

    /* draw our text */
    void draw_text(class CHtmlSysWin *win,
                   unsigned long sel_start, unsigned long sel_end,
                   class CHtmlDispLink *link, int clicked,
                   const CHtmlRect *pos);

protected:
    /* 
     *   flag: we're showing as an actual visible hyphen, because we've
     *   taken the option of breaking the line at this point 
     */
    int vis_;
};

/*
 *   Linked version of soft hyphen - this is for soft hyphens that occur
 *   inside hyperlinked text. 
 */
class CHtmlDispSoftHyphenLink: public CHtmlDispSoftHyphen
{
public:
    CHtmlDispSoftHyphenLink(class CHtmlSysWin *win, class CHtmlSysFont *font,
                            const textchar_t *txt, size_t len,
                            unsigned long txtofs, CHtmlDispLink *link)
        : CHtmlDispSoftHyphen(win, font, txt, len, txtofs)
    {
        /* initialize the linked text item */
        linked_text_init(win, link);
    }

    /* define the linked text methods */
    LINKED_TEXT_METHODS
};


/* ------------------------------------------------------------------------ */
/*
 *   Non-breaking space display item.  
 */
class CHtmlDispNBSP: public CHtmlDispTextSpecial
{
public:
    CHtmlDispNBSP(class CHtmlSysWin *win, class CHtmlSysFont *font,
                  const textchar_t *txt, size_t len,
                  unsigned long txtofs)
        : CHtmlDispTextSpecial(win, font, txt, len, txtofs) { }

    /* find a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);

    /* measure for table inclusion */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* find trailing whitespace */
    void find_trailing_whitespace(CHtmlDisp_wsinfo *info);

protected:
};


/*
 *   Hyperlinked version of non-breaking space 
 */
class CHtmlDispNBSPLink: public CHtmlDispNBSP
{
public:
    CHtmlDispNBSPLink(class CHtmlSysWin *win, class CHtmlSysFont *font,
                      const textchar_t *txt, size_t len,
                      unsigned long txtofs, CHtmlDispLink *link)
        : CHtmlDispNBSP(win, font, txt, len, txtofs)
    {
        /* initialize the linked text item */
        linked_text_init(win, link);
    }

    /* define the linked text methods */
    LINKED_TEXT_METHODS
};


/* ------------------------------------------------------------------------ */
/*
 *   Special explicit-width space display item.  This type of item is used
 *   to display the various typographical Unicode spacing characters.  These
 *   are all breakable spaces for line-wrapping purposes.  
 */
class CHtmlDispSpace: public CHtmlDispTextSpecial
{
public:
    CHtmlDispSpace(class CHtmlSysWin *win, class CHtmlSysFont *font,
                   const textchar_t *txt, size_t len,
                   unsigned long txtofs, int wid);

    /* find a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);
    
    /* do a line break */
    CHtmlDisp *do_line_break(class CHtmlFormatter *formatter,
                             class CHtmlSysWin *win,
                             class CHtmlLineBreak *break_pos);

    /* do a line break from a previous item, skipping whitespace */
    CHtmlDisp *do_line_break_skipsp(class CHtmlFormatter *formatter,
                                    class CHtmlSysWin *win)
    {
        /* do our normal line breaking, which will skip to the next item */
        return do_line_break(formatter, win, 0);
    }

    /* draw our text */
    void draw_text(class CHtmlSysWin *win,
                   unsigned long sel_start, unsigned long sel_end,
                   class CHtmlDispLink *link, int clicked,
                   const CHtmlRect *pos);

    /* find trailing whitespace */
    void find_trailing_whitespace(CHtmlDisp_wsinfo *info);

    /* remove/hide trailing whitespace */
    void remove_trailing_whitespace(CHtmlSysWin *win, void *pos);
    void hide_trailing_whitespace(class CHtmlSysWin *win, void *pos);

    /* measure for table inclusion */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics)
    {
        /* 
         *   use the base display item handling, not the text handling - we
         *   don't have the usual text contents 
         */
        CHtmlDisp::measure_for_table(win, metrics);
    }

protected:
};

/*
 *   Hyperlinked version of explicit-width space display item.  
 */
class CHtmlDispSpaceLink: public CHtmlDispSpace
{
public:
    CHtmlDispSpaceLink(class CHtmlSysWin *win, class CHtmlSysFont *font,
                       const textchar_t *txt, size_t len,
                       unsigned long txtofs, int wid, CHtmlDispLink *link)
        : CHtmlDispSpace(win, font, txt, len, txtofs, wid)
    {
        /* initialize the linked text item */
        linked_text_init(win, link);
    }

    /* define the linked text methods */
    LINKED_TEXT_METHODS
};


/* ------------------------------------------------------------------------ */
/*
 *   Tab display object.  This object's purpose is to provide horizontal
 *   spacing to align material at a tab stop. 
 */
class CHtmlDispTab: public CHtmlDisp
{
public:
    CHtmlDispTab(const CHtmlPoint *pos) { init(pos, 0); }
    CHtmlDispTab(const CHtmlPoint *pos, long width) { init(pos, width); }

    /* my width is fixed */
    int measure_width(class CHtmlSysWin *) { return (int)width_; }

    /* nothing to draw */
    void draw(class CHtmlSysWin *, unsigned long /*sel_start*/,
              unsigned long /*sel_end*/, int /*clicked*/) { }

    /* no vertical space */
    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above_base, int *below_base, int *total)
    {
        *above_base = 0;
        *below_base = 0;
        *total = 0;
    }

    /* 
     *   Set my width.  The formatter cannot always set a tab's width
     *   immediately, but must sometimes wait until the next tab stop to
     *   determine the size of the tab.  This routine lets the formatter
     *   set the size of the tab when it becomes known.
     */
    void set_width(long width) { width_ = width; }

    /* this item is effectively text, so allow inexact hits */
    virtual int allow_inexact_hit() const { return TRUE; }

    /* treat my text contents as a tab character */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\t", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append("\t", 1); }

private:
    void init(const CHtmlPoint *pos, long width)
    {
        width_ = width;
        pos_.left = pos_.right = pos->x;
        pos_.top = pos_.bottom = pos->y;
    }

    /* horizontal spacing */
    long width_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Line break display object.  This object's only purpose is to provide
 *   a marker for a line break in the display list.
 */
class CHtmlDispBreak: public CHtmlDisp
{
public:
    CHtmlDispBreak(int vertical_space) { vertical_space_ = vertical_space; }

    /* I have no width */
    int measure_width(class CHtmlSysWin *) { return 0; }

    /* nothing to draw */
    void draw(class CHtmlSysWin *, unsigned long /*sel_start*/,
              unsigned long /*sel_end*/, int /*clicked*/) { }

    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above_base, int *below_base, int *total)
    {
        *above_base = 0;
        *below_base = 0;
        *total = vertical_space_;
    }

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* treat my text contents as a newline */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\n", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append("\n", 1); }

private:
    int vertical_space_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Horizontal rule object 
 */
class CHtmlDispHR: public CHtmlDisp, public CHtmlDispDisplaySite
{
public:
    CHtmlDispHR(class CHtmlSysWin *win, int shade, long height,
                long width, int width_is_pct,
                long margin_left, long margin_right,
                class CHtmlResCacheObject *image);
    ~CHtmlDispHR();

    /* unset our window reference - process via the display site */
    virtual void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* cancel playback - process via the display site */
    virtual void cancel_playback()
        { CHtmlDispDisplaySite::cancel_playback(); }

    /* measure my width */
    int measure_width(class CHtmlSysWin *) { return width_; }
    
    /* draw it */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* get my overall vertical space needs */
    void get_vertical_space(class CHtmlSysWin *, int,
                            int *above_base, int *below_base, int *total)
    {
        *above_base = 0;
        *below_base = 0;
        *total = height_;
    }

    /* set my position */
    void set_line_pos(class CHtmlSysWin *, int left, int top,
                      int /*total_height*/, int text_base_offset,
                      int /*max_text_height*/)
    {
        pos_.offset(left - pos_.left, top + text_base_offset - pos_.top);
    }

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* find trailing whitespace */
    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info);

    /* treat my text contents as a newline */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\n", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long startofs,
                              unsigned long endofs) const
        { buf->append("\n", 1); }

private:
    /* width in pixels */
    long width_;

    /* 
     *   Flag indicating whether the width is absolute or relative.  The
     *   distinction is only important when measuring a table; when a
     *   relative rule is in a table, it adds no space during the table
     *   measurement phase, because the rule will be resized according to
     *   the table's final size. 
     */
    int width_is_pct_ : 1;

    /* vertical size */
    long height_;

    /* shading */
    int shade_ : 1;

    /* the image, if we have one */
    class CHtmlResCacheObject *image_;
};


/* ------------------------------------------------------------------------ */
/*
 *   List item display items.  One of these items is displayed in the left
 *   margin of each item of a list.
 */
class CHtmlDispListitem: public CHtmlDisp
{
public:
    CHtmlDispListitem(class CHtmlSysWin *win, class CHtmlSysFont *font,
                      long width);

    /* get my width - use the fixed width set at creation time */
    int measure_width(class CHtmlSysWin *)
        { return (int)(pos_.right - pos_.left); }

    /* get my overall vertical space needs */
    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base, int *total);

    /* set my position */
    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height);

    /* treat my text contents as a newline */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\n", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append("\n", 1); }

protected:
    /* ascender height of my font */
    unsigned short ascent_ht_;

    /* my font */
    CHtmlSysFont *font_;
};

/*
 *   Numbered list item header 
 */
class CHtmlDispListitemNumber: public CHtmlDispListitem
{
public:
    CHtmlDispListitemNumber(class CHtmlSysWin *win, class CHtmlSysFont *font,
                            long width, textchar_t style, long value);

    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* set my position */
    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height);
private:
    CStringBuf num_;
};

/*
 *   Bulleted list item header 
 */
class CHtmlDispListitemBullet: public CHtmlDispListitem,
    public CHtmlDispDisplaySite
{
public:
    CHtmlDispListitemBullet(class CHtmlSysWin *win, class CHtmlSysFont *font,
                            long width, HTML_Attrib_id_t type,
                            class CHtmlResCacheObject *image);
    ~CHtmlDispListitemBullet();

    /* unset our window reference - process via the display site */
    virtual void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* cancel playback - process via the display site */
    virtual void cancel_playback()
        { CHtmlDispDisplaySite::cancel_playback(); }

    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base, int *total);

    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height);

    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

private:
    /* bullet type (disc, square, circle) */
    enum HTML_SysWin_Bullet_t type_;

    /* image, if we're displaying an image as our bullet */
    class CHtmlResCacheObject *image_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image display object 
 */
class CHtmlDispImg: public CHtmlDisp, public CHtmlDispDisplaySite
{
public:
    CHtmlDispImg(class CHtmlSysWin *win,
                 class CHtmlResCacheObject *image, CStringBuf *alt,
                 HTML_Attrib_id_t align, long hspace, long vspace,
                 long width, int width_set, long height, int height_set,
                 long border, CHtmlUrl *usemap, int ismap);

    ~CHtmlDispImg();

    /* unset our window reference - process via the display site */
    virtual void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* cancel playback - process via the display site */
    virtual void cancel_playback()
        { CHtmlDispDisplaySite::cancel_playback(); }

    /* get my width */
    int measure_width(class CHtmlSysWin *win);

    /* get my height */
    int measure_height() { return height_ + 2*vspace_ + 2*border_; }

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* draw the image */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int clicked);

    /* find a line break */
    CHtmlDisp *find_line_break(class CHtmlFormatter *formatter,
                               class CHtmlSysWin *win,
                               long space_avail,
                               class CHtmlLineBreak *break_pos);

    /* 
     *   find trailing whitespace - an image counts as non-whitespace
     *   text, so we'll just forget about any preceding whitespace (it's
     *   not trailing, since the image follows) 
     */
    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info);

    /* get my overall vertical space needs */
    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base, int *total);

    /* set my position */
    void set_line_pos(class CHtmlSysWin *win, int left, int top,
                      int total_height, int text_base_offset,
                      int max_text_height);

    /* determine what type of cursor to use over the image */
    Html_csrtype_t get_cursor_type(class CHtmlSysWin *,
                                   class CHtmlFormatter *,
                                   int, int, int) const;

    /* get my link object */
    CHtmlDispLink *get_link(class CHtmlFormatter *, int, int) const;

    /* 
     *   An unliked image won't change appearance on click changes, so don't
     *   invalidate.  (Images can take up large areas of the screen, so it's
     *   worth overriding this to avoid this default invalidation that would
     *   otherwise occur.)  
     */
    void on_click_change(class CHtmlSysWin *) { }

    /* treat my text contents as a space */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append(" ", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append(" ", 1); }

    /* 
     *   invalidate if I'm a link - if I have a map, I have a link, so
     *   invalidate me, otherwise do nothing 
     */
    void inval_link(class CHtmlSysWin *win)
    {
        if (usemap_.get_url() != 0 && usemap_.get_url()[0] != '\0')
            inval(win);
    }

    /* get my ALT text */
    virtual const textchar_t *get_alt_text() const
    {
        return alt_.get();
    }

    /* format deferred (i.e., as a floating item) */
    virtual void format_deferred(class CHtmlSysWin *win,
                                 class CHtmlFormatter *formatter,
                                 long line_spacing);

    /*
     *   Set the hovering/active images.  For regular image tags, this has no
     *   effect; only hyperlinked images can have these extra images.  As a
     *   diagnostic aid, we'll log an error to the debug log mentioning that
     *   the extra subimages aren't allowed in ordinary images.  
     */
    virtual void set_extra_images(class CHtmlResCacheObject *a_image,
                                  class CHtmlResCacheObject *h_image);

protected:
    /*
     *   If we have an image map, find the map zone containing a given
     *   point.  If we don't have an image map, or the image map doesn't
     *   have a zone containing the given point, returns null.  
     */
    class CHtmlFmtMapZone *get_map_zone(CHtmlFormatter *formatter,
                                        int x, int y) const;

    /* draw using the given image object */
    void draw_image(class CHtmlSysWin *win, class CHtmlResCacheObject *image);

    /* forget one of our internal images */
    void forget_image(class CHtmlResCacheObject **image);

protected:
    /* alignment type */
    HTML_Attrib_id_t align_;

    /* alternate textual representation */
    CStringBuf alt_;

    /* horizontal/vertical whitespace around edges */
    long hspace_;
    long vspace_;

    /* height and width */
    long height_;
    long width_;

    /* border size */
    long border_;

    /* map setting */
    CHtmlUrl usemap_;

    /* is-map flag */
    int ismap_ : 1;

    /* the image */
    class CHtmlResCacheObject *image_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Linkable image display item 
 */

class CHtmlDispImgLink: public CHtmlDispImg
{
public:
    CHtmlDispImgLink(class CHtmlSysWin *win,
                     class CHtmlResCacheObject *image, CStringBuf *alt,
                     HTML_Attrib_id_t align, long hspace, long vspace,
                     long width, int width_set, long height, int height_set,
                     long border, CHtmlUrl *usemap, int ismap,
                     CHtmlDispLink *link)
        : CHtmlDispImg(win, image, alt, align, hspace, vspace,
                       width, width_set, height, height_set, border,
                       usemap, ismap)
    {
        /* remember our link */
        link_ = link;

        /* add this item to the link */
        link->add_link_item(this);

        /* presume we won't have any special images */
        a_image_ = h_image_ = 0;
    }

    ~CHtmlDispImgLink();

    /* set our active/hover images */
    virtual void set_extra_images(class CHtmlResCacheObject *a_image,
                                  class CHtmlResCacheObject *h_image);

    /* use the hand cursor when over a link */
    Html_csrtype_t get_cursor_type(class CHtmlSysWin *,
                                   class CHtmlFormatter *,
                                   int, int, int) const;

    /* invalidate if I'm a link - I am, so invalidate me */
    void inval_link(class CHtmlSysWin *win) { inval(win); }

    /* get my link object */
    CHtmlDispLink *get_link(class CHtmlFormatter *, int, int) const;

    /* get my containing link object */
    class CHtmlDispLink *get_container_link() const
        { return link_; }

    /* draw as a link */
    void draw(class CHtmlSysWin *win, unsigned long sel_start,
              unsigned long sel_end, int /*clicked*/);

    /* 
     *   On a click state change, we might change appearance if we have a
     *   separate "hover" or "active" image - if so, invalidate our display
     *   area on a click change. 
     */
    void on_click_change(class CHtmlSysWin *win)
    {
        /* 
         *   if we have an alternate image, invalidate our area so that we'll
         *   display the new image as needed 
         */
        if (h_image_ != 0 || a_image_ != 0)
            inval(win);
    }

    /* get my ALT text - get the TITLE for the link, if it has one */
    virtual const textchar_t *get_alt_text() const
    {
        /* if I have a link, and it has a TITLE, return the title */
        if (link_ != 0 && link_->title_.get() != 0)
            return link_->title_.get();

        /* return the inherited ALT text from the image */
        return get_alt_text();
    }

private:
    /* our containing link object */
    CHtmlDispLink *link_;

    /* the image to display when hovering */
    class CHtmlResCacheObject *h_image_;

    /* the image to display when clicked ("active") */
    class CHtmlResCacheObject *a_image_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Tables 
 */

class CHtmlDispTable: public CHtmlDisp, public CHtmlDispDisplaySite
{
public:
    CHtmlDispTable(class CHtmlSysWin *win,
                   long border, HTML_Attrib_id_t align, int floating,
                   long width, int width_pct, int width_set, long win_width,
                   HTML_color_t bgcolor, int use_bgcolor,
                   class CHtmlResCacheObject *bg_image);

    /* change my window width */
    void set_win_width(long win_width) { win_width_ = win_width; }

    /* measure for inclusion in a table cell */
    void measure_for_table(class CHtmlSysWin *win,
                           class CHtmlTableMetrics *metrics);

    /* measure for inclusion in a banner */
    void measure_for_banner(class CHtmlSysWin *win,
                            class CHtmlTableMetrics *metrics);

    /* draw the table */
    void draw(class CHtmlSysWin *win,
              unsigned long sel_start, unsigned long sel_end,
              int clicked);

    /* measure the width of the table */
    int measure_width(class CHtmlSysWin *win);

    /* 
     *   table display items are in the background - let mouse clicks go
     *   to the table's contents instead 
     */
    virtual int is_in_background() const { return TRUE; }

    /* get the vertical space needed by the table */
    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base,
                            int *total);

    /* find trailing whitespace */
    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info);

    /*
     *   Set the size limits of the table.  This doesn't set the final
     *   size of the table, but rather sets the limits: the minimum size
     *   that the table can have with as much line wrapping as possible;
     *   and the maximum size that the table needs in order to display
     *   everything without any line wrapping at all.  These limits are
     *   needed later, so that they can be compared to the available space
     *   for the table to determine how the space is apportioned; here, we
     *   just calculate and remember the limits.  
     */
    void set_table_size_limits(long min_width, long max_width)
    {
        width_min_ = min_width;
        width_max_ = max_width;
    }

    /*
     *   Calculate the final size of the table.  Here we compute the
     *   actual size of the table, based on the available screen space and
     *   the limits that we calculated earlier in
     *   calc_table_size_limits().  Note that calc_table_size_limits()
     *   must be called before this routine.  
     */
    void calc_table_size(class CHtmlFormatter *formatter,
                         class CHtmlTagTABLE *tag);

    /*
     *   Set the table's height.  Our tag will call this after our final
     *   height has been determined in pass 2. 
     */
    void set_table_height(long height)
    {
        pos_.bottom = pos_.top + height;
    }

    /* treat my text contents as a newline */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\n", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append("\n", 1); }

    /* 
     *   Set our sublist - this gives us our list of contained display items
     *   for use in formatting floating tables. 
     */
    void set_contents_sublist(CHtmlDisp *head);

    /* format deferred (as a floating item) */
    virtual void format_deferred(class CHtmlSysWin *win,
                                 class CHtmlFormatter *formatter,
                                 long line_spacing);

    /* forward unset_win() on to the display site */
    void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* forward cancel_playback() on to the display site */
    void cancel_playback() { CHtmlDispDisplaySite::cancel_playback(); }

private:
    /* border size setting */
    long border_;

    /* alignment setting */
    HTML_Attrib_id_t align_;

    /* 
     *   width of the table, in pixels, as set in the TABLE tag's
     *   attributes 
     */
    long width_;

    /* floating? */
    int floating_ : 1;

    /* flag indicating whether the table's WIDTH is a percentage */
    int width_pct_ : 1;

    /* flag indicating whether the TABLE tag has a WIDTH setting */
    int width_set_ : 1;

    /* width of available space in the window */
    long win_width_;

    /*
     *   Width limits.  The minimum width is the smallset space we need to
     *   display the table's contents without clipping anything, but with
     *   maximum line wrapping.  The maximum width is the smallest space
     *   we need to display the table's contents without any line
     *   wrapping. 
     */
    long width_min_;
    long width_max_;

    /* background color, if set */
    HTML_color_t bgcolor_;
    int use_bgcolor_;

    /* our contents sublist */
    CHtmlDisp *sub_head_;
};

/*
 *   Table cell display item 
 */
class CHtmlDispTableCell: public CHtmlDisp, public CHtmlDispDisplaySite
{
public:
    CHtmlDispTableCell(class CHtmlSysWin *win,
                       int border, HTML_color_t bgcolor, int use_bgcolor,
                       class CHtmlResCacheObject *bg_image,
                       long width_attr, int width_pct, int width_set);

    /* draw the cell */
    void draw(class CHtmlSysWin *win,
              unsigned long sel_start, unsigned long sel_end,
              int clicked);

    /* measure the width of the cell */
    int measure_width(class CHtmlSysWin *win);

    /* measure our contribution to the containing banner's size */
    void measure_for_banner(class CHtmlSysWin *win,
                            class CHtmlTableMetrics *metrics);

    /* 
     *   Measure the metrics of this item for use in a table cell.  Since
     *   this *is* a table cell, this routine is called only when we're in a
     *   nested table - we're being measured for use in the enclosing
     *   table's cell that contains our entire table.  
     */
    virtual void measure_for_table(class CHtmlSysWin *win,
                                   class CHtmlTableMetrics *metrics);

    /* get the vertical space needed by the cell */
    void get_vertical_space(class CHtmlSysWin *win, int text_height,
                            int *above_base, int *below_base,
                            int *total);

    /* 
     *   cell display items are in the background - let mouse clicks go to
     *   the table's contents instead 
     */
    virtual int is_in_background() const { return TRUE; }

    /* find trailing whitespace */
    void find_trailing_whitespace(class CHtmlDisp_wsinfo *info);

    /* 
     *   Set the height of the cell.  Our tag will call this after pass 2
     *   is completed and the final height of the cell's row or rows is
     *   known.  
     */
    void set_cell_height(long height)
    {
        pos_.bottom = pos_.top + height;
    }

    /*
     *   Set the width of the cell.  Our tag will call this during pass 2,
     *   when the final width of the cell's column or columns is known.  
     */
    void set_cell_width(long width)
    {
        pos_.right = pos_.left + width;
    }

    /* treat my text contents as a tab */
    void add_text_to_buf(CStringBuf *buf) const
        { buf->append("\t", 1); }
    void add_text_to_buf_clip(CStringBuf *buf, unsigned long,
                              unsigned long) const
        { buf->append("\t", 1); }

    /* forward unset_win() on to the display site */
    void unset_win() { CHtmlDispDisplaySite::unset_win(); }

    /* forward cancel_playback() on to the display site */
    void cancel_playback() { CHtmlDispDisplaySite::cancel_playback(); }

private:
    /* 
     *   Border setting - true means that the cell has a border, false
     *   means that the cell is drawn without a border.  This should be
     *   set to true if the table has a non-zero BORDER attribute, false
     *   if the table has no BORDER attribute or the BORDER attribute has
     *   a value of zero.  
     */
    int border_ : 1;

    /* background color, if set */
    HTML_color_t bgcolor_;
    int use_bgcolor_;

    /* 
     *   WIDTH attribute value, and flags indicating whether the WIDTH
     *   attribute is present and whether it is based on a percentage 
     */
    long width_attr_;
    int width_set_ : 1;
    int width_pct_ : 1;
};

#endif /* HTMLDISP_H */

