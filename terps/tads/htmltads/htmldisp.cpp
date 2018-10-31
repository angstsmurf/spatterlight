#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmldisp.cpp,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldisp.cpp - HTML display objects
Function
  
Notes
  
Modified
  09/08/97 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <memory.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef HTMLURL_H
#include HTMLURL_H
#endif
#ifndef HTMLATTR_H
#include "htmlattr.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Display Site implementation mix-in 
 */

/*
 *   create 
 */
CHtmlDispDisplaySite::CHtmlDispDisplaySite(CHtmlSysWin *win,
                                           const CHtmlRect *pos,
                                           CHtmlResCacheObject *image)
{
    /* remember our window and position in the window */
    win_ = win;
    posp_ = pos;

    /* no timer yet */
    timer_ = 0;

    /* remember our image, and set a reference if it's valid */
    site_image_ = image;
    if (image != 0)
    {
        /* add a reference */
        image->add_ref();

        /* if the image is valid, register as its display site */
        if (image->get_image() != 0)
            image->get_image()->set_display_site(this);
    }
}

/*
 *   destroy 
 */
CHtmlDispDisplaySite::~CHtmlDispDisplaySite()
{
    /* unset our window reference */
    unset_win();

    /* if we have an image, remove our reference to it */
    if (site_image_ != 0)
    {
        /* remove myself as the image's display site */
        if (site_image_->get_image() != 0)
            site_image_->get_image()->set_display_site(0);

        /* remove our reference */
        site_image_->remove_ref();
    }
}

/*
 *   unset our window reference 
 */
void CHtmlDispDisplaySite::unset_win()
{
    /* if we have a timer, delete it */
    if (win_ != 0 && timer_ != 0)
    {
        win_->delete_timer(timer_);
        timer_ = 0;
    }
    
    /* forget our window reference */
    win_ = 0;
}

/*
 *   Cancel playback.  If our image is an animation, we'll stop animating it.
 */
void CHtmlDispDisplaySite::cancel_playback()
{
    /* if we have an image resource, tell it to halt playback */
    if (site_image_ != 0 && site_image_->get_image() != 0)
        site_image_->get_image()->cancel_playback();
}

/*
 *   Set the image being displayed in the site. 
 */
void CHtmlDispDisplaySite::set_site_image(CHtmlResCacheObject *image)
{
    /* if we have an outgoing image, release it */
    if (site_image_ != 0)
    {
        /* we're no longer the display site for this image */
        if (site_image_->get_image() != 0)
        {
            /* pause the playback, in case it's animated */
            site_image_->get_image()->pause_playback();

            /* unset the display site */
            site_image_->get_image()->set_display_site(0);
        }

        /* remove our reference to the outgoing image */
        site_image_->remove_ref();
    }

    /* remember the new image */
    site_image_ = image;

    /* if we have a new image, attach to it */
    if (image != 0)
    {
        /* add a reference to the new image */
        image->add_ref();

        /* register as the image's display site */
        if (image->get_image() != 0)
        {
            /* resume playback, if it's animated */
            site_image_->get_image()->resume_playback();

            /* set the display site */
            image->get_image()->set_display_site(this);
        }
    }
}

/*
 *   CHtmlSysImageDisplaySite implementation: invalidate an area of the
 *   display site 
 */
void CHtmlDispDisplaySite::dispsite_inval(unsigned int x, unsigned int y,
                                          unsigned int width,
                                          unsigned int height)
{
    /* if we have a window, invalidate the desired area */
    if (win_ != 0)
    {
        CHtmlRect area;

        /* 
         *   set up the area - start with the relative coordinates within our
         *   display area, then offset the rectangle by our own document
         *   coordinates to get the actual document coordinates in the window
         *   of the sub-area to be invalidated 
         */
        area.set(x, y, x + width, y + height);
        area.offset(posp_->left, posp_->top);

        /* invalidate the area of our window */
        win_->inval_doc_coords(&area);
    }
}

/*
 *   Callback for our timer event 
 */
void CHtmlDispDisplaySite::img_timer_cb(void *ctx)
{
    /* 
     *   the context is the CHtmlSysImageAnimated object that wants the timer
     *   event notification 
     */
    ((CHtmlSysImageAnimated *)ctx)->notify_timer();
}

/* 
 *   CHtmlSysImageDisplaySite implementation: set a timer event 
 */
void CHtmlDispDisplaySite::dispsite_set_timer(unsigned long delay_ms,
                                              CHtmlSysImageAnimated *image)
{
    /* if we have a window, set up our timer */
    if (win_ != 0)
    {
        /* if we don't have a timer yet, create one */
        if (timer_ == 0)
            timer_ = win_->create_timer(&img_timer_cb, image);
        
        /* if we managed to create a timer, set it */
        if (timer_ != 0)
            win_->set_timer(timer_, delay_ms, FALSE);
    }
}

/*
 *   CHtmlSysImageDisplaySite implementation: cancel the timer 
 */
void CHtmlDispDisplaySite::dispsite_cancel_timer(CHtmlSysImageAnimated *)
{
    /* if we have a window and a timer, delete the timer */
    if (win_ != 0 && timer_ != 0)
    {
        /* delete the timer */
        win_->delete_timer(timer_);

        /* forget it, now that we've deleted it */
        timer_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic display object implementation 
 */

/*
 *   Memory allocation - use the formatter for memory management 
 */
void *CHtmlDisp::operator new(size_t siz, CHtmlFormatter *formatter)
{
    void *mem;
    htmldisp_heapid_t heap_id;

    /* 
     *   increase the size by one byte (bumped to the proper system
     *   alignment boundary), so we can store the heap ID at the first
     *   byte of each block we allocate; we'll need to identify the heap
     *   source when it comes time to delete the block 
     */
    siz += os_align_size(1);

    /* see if we have a formatter */
    if (formatter != 0)
    {
        /* we have a formatter - ask it for the memory from its heap */
        mem = formatter->heap_alloc(siz);
        heap_id = HTMLDISP_HEAPID_FMT;
    }
    else
    {
        /* no formatter - get the memory from the system heap */
        mem = ::operator new(siz);
        heap_id = HTMLDISP_HEAPID_SYS;
    }

    /* store the heap ID in the first byte of the block we allocated */
    *(char *)mem = (char)heap_id;

    /* 
     *   skip past the prefix, so that the caller sees the part after our
     *   heap ID entry, which is for our use only
     */
    mem = (void *)(((char *)mem) + os_align_size(1));

    /* return the memory */
    return mem;
}

void CHtmlDisp::operator delete(void *ptr)
{
    /* 
     *   adjust the memory to point to our original block, including the
     *   prefix 
     */
    ptr = (void *)(((char *)ptr) - os_align_size(1));
    
    /* check the prefix, to determine where the memory came from */
    switch(*(char *)ptr)
    {
    case HTMLDISP_HEAPID_FMT:
        /* 
         *   it's in the formatter's heap -- ignore the request, since the
         *   formatter will take care of deleting the object when it
         *   resets its heap, and doesn't free individual objects 
         */
        break;

    case HTMLDISP_HEAPID_SYS:
        /*
         *   it came from the system heap - delete the memory as normal 
         */
        ::operator delete(ptr);
        break;
    }
}


/*
 *   measure for table metrics 
 */
void CHtmlDisp::measure_for_table(CHtmlSysWin *win,
                                  CHtmlTableMetrics *metrics)
{
    int mywid = measure_width(win);

    /* if there's a no-break flag before me, clear out the leftover */
    if (!metrics->no_break_ && !metrics->no_break_sect_)
        metrics->clear_leftover();

    /* add to the running leftover space */
    metrics->leftover_width_ += mywid;

    /* allow breaking immediately after this item */
    metrics->last_char_ = ' ';
    metrics->no_break_ = FALSE;
}

/*
 *   find the next line break 
 */
CHtmlDisp *CHtmlDisp::find_line_break(CHtmlFormatter *formatter,
                                      CHtmlSysWin *win,
                                      long space_avail,
                                      CHtmlLineBreak *break_pos)
{
    int mywid = measure_width(win);
    
    /*
     *   Set the "previous character" to a space, so that any text item
     *   that immediately follows can put a line break between this item
     *   and the text item if necessary.  
     */
    break_pos->last_char_ = ' ';

    /*
     *   By default, we can't break up a display object across two lines.
     *   So, if we don't fit into the available space, break at the start of
     *   this object (that is, end the current line just before this object,
     *   and put this object on the next line).  Note that we ignore the
     *   previous character in making this determination, since this default
     *   implementation is for objects that don't contain any text, hence we
     *   can always set this object on a new line even if it was immediately
     *   preceded by non-breakable text.  Note also that we ignore prv and
     *   prv_pos, since these are used when an object can't be separated from
     *   its previous object, and by default we will allow a break just
     *   before this object.  However, we cannot break on this item if we
     *   have an explicit no-break flag just before this item.  
     */
    if (space_avail < mywid)
    {
        /* 
         *   if we're allowed to break before me, break before me; otherwise,
         *   if we have a prior break position, break there 
         */
        if (!break_pos->no_break_)
            return this;
        else if (break_pos->item_ != 0)
            return break_pos->item_->do_line_break(formatter, win, break_pos);
    }

    /* 
     *   we're not an explicit no-break element, and we can break to the
     *   right of this item if necessary: clear the no-break indication 
     */
    break_pos->no_break_ = FALSE;

    /* we're not whitespace */
    break_pos->note_non_ws();

    /* return null to indicate that we didn't break the line */
    return 0;
}

/*
 *   Do a line break at a position previously noted.  The default
 *   implementation never notes a previous position for a break, so this
 *   routine should never be called. 
 */
CHtmlDisp *CHtmlDisp::do_line_break(CHtmlFormatter *,
                                    CHtmlSysWin *,
                                    CHtmlLineBreak *)
{
    /* by default, just break at the start of this item */
    return this;
}

/*
 *   Do a line break after skipping whitespace in a break position we
 *   previously found in an item before us. 
 */
CHtmlDisp *CHtmlDisp::do_line_break_skipsp(CHtmlFormatter *, CHtmlSysWin *)
{
    /* by default, just break at the start of this item */
    return this;
}

/*
 *   Delete this item and all following items in its list 
 */
void CHtmlDisp::delete_list(CHtmlDisp *head)
{
    CHtmlDisp *cur;
    CHtmlDisp *nxt;

    /* loop through the list */
    for (cur = head ; cur != 0 ; cur = nxt)
    {
        /*
         *   remember the next item, since we're about to delete the
         *   current one, which will invalidate its forward link pointer 
         */
        nxt = cur->get_next_disp();

        /* delete the current item */
        delete cur;
    }
}

/*
 *   invalidate my area on the display 
 */
void CHtmlDisp::inval(CHtmlSysWin *win)
{
    win->inval_doc_coords(&pos_);
}

/* 
 *   Insert a list after another display object in a list.  This adds me
 *   and any other items following me in my list.  
 */
void CHtmlDisp::add_list_after(CHtmlDisp *prv)
{
    CHtmlDisp *list_next;
    CHtmlDisp *my_last;
    
    /* 
     *   remember the next item after the previous item in the list into
     *   which we're inserting 
     */
    list_next = prv->nxt_;

    /* add me right after the previous item in the list */
    prv->nxt_ = this;

    /* find the last item in my list */
    for (my_last = this ; my_last->nxt_ != 0 ; my_last = my_last->nxt_) ;

    /* 
     *   set the next item after the last item in my list to point to the
     *   next item in the list into which we're inserting me 
     */
    my_last->nxt_ = list_next;
}

/* 
 *   Receive notification that we've been moved into the display list from
 *   the deferred list.  If we're aligned left or right, we can't be added
 *   to the display list until the start of a new line; at that point, we
 *   need to float to the appropriate margin and adjust the text margins to
 *   exclude the space we occupy.  
 */
void CHtmlDisp::basic_format_deferred(class CHtmlSysWin *win,
                                      class CHtmlFormatter *formatter,
                                      long line_spacing,
                                      HTML_Attrib_id_t align,
                                      long height)
{
    long wid;
    long disp_wid;
    long ypos;
    long left_margin, right_margin;

    /* get the total horizontal space we need */
    wid = measure_width(win);

    /* get the total size of the window */
    disp_wid = win->get_disp_width();

    /* get the current y position, and adjust for line spacing */
    ypos = formatter->getcurpos().y + line_spacing;

    /* get the current margin settings */
    left_margin = formatter->get_left_margin();
    right_margin = formatter->get_right_margin();

    /* float over to the appropriate margin */
    switch(align)
    {
    case HTML_Attrib_left:
        /* set my position at the current left margin */
        pos_.set(left_margin, ypos, left_margin + wid, ypos + height);

        /* 
         *   move left margin past me; if we take up more than the available
         *   space, though, simply move the left margin into the right
         *   margin 
         */
        if (wid <= disp_wid - left_margin - right_margin)
            left_margin += wid + 1;
        else
            left_margin = disp_wid - right_margin - 1;
        break;

    case HTML_Attrib_right:
        /* 
         *   set my position at the current right margin, but in any case no
         *   further left than the left margin, if we're too wide to fit
         *   between the margins 
         */
        if (wid < disp_wid - left_margin - right_margin)
        {
            /* I fit between the margins - align flush right */
            pos_.set(disp_wid - right_margin - wid, ypos,
                     disp_wid - right_margin, ypos + height);

            /* move right margin before me */
            right_margin += wid + 1;
        }
        else
        {
            /* I don't fit - align left */
            pos_.set(left_margin, ypos, left_margin + wid, ypos + height);

            /* there's no room left after me */
            right_margin = disp_wid - left_margin - 1;
        }
        break;

    default:
        /* we're not floating, so there's nothing we need to do */
        return;
    }

    /* tell the formatter to flow text around this item */
    formatter->begin_flow_around(this, left_margin, right_margin,
                                 pos_.bottom);
}

/*
 *   Get the hyperlink associated with the item.
 */
CHtmlDispLink *CHtmlDisp::get_link(
    CHtmlFormatter *formatter, int /*x*/, int /*y*/) const
{
    /*
     *   Ordinary items aren't linkable themselves, so we have no direct link
     *   to return.  However, we can inherit a division link from an
     *   enclosing DIV tag.  So look for an inherited DIV link and return
     *   what we find. 
     */
    return find_div_link(formatter);
}

/*
 *   Get the hyperlink object associated with the item.  Most display items
 *   aren't directly linkable; however, an item can be in a division that has
 *   special hyperlink behavior, so we need to look at the containing
 *   divisions for hyperlink references.  
 */
CHtmlDispLinkDIV *CHtmlDisp::find_div_link(CHtmlFormatter *formatter) const
{
    CHtmlDispDIV *div;
    
    /*
     *   Since the base display item type isn't directly linkable, we have no
     *   link object of our own.  However, we can inherit a special link
     *   object from a containing division.  So, check to see if we're in a
     *   division, and if so look to see if it has a hyperlink.  If we have a
     *   division but it doesn't have a hyperlink, check the DIV's parents as
     *   well.  
     */
    for (div = formatter->find_div_by_ofs(get_text_ofs(), pos_.top) ;
         div != 0 ; div = div->get_parent_div())
    {
        CHtmlDispLinkDIV *link;

        /* if this division has a DIV-level hyperlink, return it */
        if ((link = div->get_div_link()) != 0)
            return link;
    }

    /* there's no hyperlink associated with this object */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   DIV display item 
 */

/*
 *   Get the hyperlink object associated with the item.
 */
CHtmlDispLink *CHtmlDispDIV::get_link(
    CHtmlFormatter *formatter, int x, int y) const
{
    const CHtmlDispDIV *div;
   
    /* 
     *   look for a link associated with this DIV or any enclosing DIV - the
     *   innermost link takes precedence 
     */
    for (div = this ; div != 0 ; div = div->get_parent_div())
    {
        CHtmlDispLink *link;

        /* if this division has a hyperlink, return it */
        if ((link = div->get_div_link()) != 0)
            return link;
    }

    /* we didn't find a link */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   DIV hyperlink 
 */

/* 
 *   Construction.  We're not a real link, so we can supply suitable dummy
 *   parameters for the underlying CHtmlDispLink construction.  Note that we
 *   use the 'hidden' style - this ensures that we'll add attributes (such as
 *   underlines) on hover only when those attributes are explicitly requested
 *   in the DIV HOVER=LINK(...) attribute.  
 */
CHtmlDispLinkDIV::CHtmlDispLinkDIV(CHtmlDispDIV *div)
    : CHtmlDispLink(LINK_STYLE_HIDDEN, FALSE, FALSE, 0, 0, 0)
{
    /* remember my DIV display item */
    div_ = div;
    
    /* link the display item back to me */
    div->set_div_link(this);
}

/* set the clicked state */
void CHtmlDispLinkDIV::set_clicked(CHtmlSysWin *win, int clicked)
{
    CHtmlDisp *cur;
    CHtmlDisp *tail;
    CHtmlDispLink *prv_link;

    /*
     *   For our purposes, we only care about hovering.  Treat 'clicked' as
     *   hovering, since the mouse is over the link in the clicked state.  
     */
    clicked = (clicked != 0 ? CHtmlDispLink_hover : 0);

    /* if the state isn't changing, there's nothing to do */
    if (clicked_ == clicked)
        return;

    /* remember the new state */
    clicked_ = clicked;

    /*
     *   For child links, we want to let them know that it's their parent DIV
     *   that we're hovering over, not the link itself.  So, notify them that
     *   their new clicked state is DIV-hover, not direct hover.  Use
     *   DIV-hover mode if the child is in either 'hover' or 'clicked' mode,
     *   because in either case the mouse is over the child.  
     */
    clicked = (clicked != 0 ? CHtmlDispLink_divhover : 0);

    /* 
     *   run through all the links within the DIV and set their state to our
     *   new state 
     */
    for (cur = get_next_disp(), tail = div_->get_div_tail(), prv_link = 0 ;
         cur != 0 && cur != tail ; cur = cur->get_next_disp())
    {
        CHtmlDispLink *link;
        
        /* get this item's link container */
        link = cur->get_container_link();

        /* 
         *   if we found a link, and it's different from the last one we
         *   found, update this link 
         */
        if (link != 0 && link != prv_link)
        {
            /* 
             *   set the new state for the link and its subitems (we
             *   obviously don't want to go back and notify the division,
             *   since we'd come right back here and recurse forever) 
             */
            link->set_clicked_sub(win, clicked);

            /* 
             *   remember this as the previous link - this will ensure that
             *   we don't repeatedly set the same item's state if there are
             *   several items within the same <A>...</A> section 
             */
            prv_link = link;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Line-wrapping marker implementation 
 */

/* measure in a table */
void CHtmlDispNOBR::measure_for_table(CHtmlSysWin *,
                                      CHtmlTableMetrics *metrics)
{
    /* set the no-break-section flag according to our status */
    metrics->no_break_sect_ = nobr_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Link object implementation 
 */

/*
 *   set the clicked state, and invalidate the screen area of all linked
 *   items 
 */
void CHtmlDispLink::set_clicked(CHtmlSysWin *win, int clicked)
{
    CHtmlDispLinkDIV *div_link;
    
    /* if the state hasn't changed, there's nothing to do */
    if (clicked_ == clicked)
        return;

    /* 
     *   if we're part of a division, and the 'hover' part of the clicked
     *   state is changing, notify the division of the change as well 
     */
    if ((div_link = find_div_link(win->get_formatter())) != 0)
    {
        /* notify the enclosing division */
        div_link->set_clicked(win, clicked);

        /* 
         *   keep the div-hover mode if we set it in the DIV call (the DIV
         *   will have looped back and set our mode to div-hover if
         *   applicable; we want to keep that in case we're not hovering
         *   directly any longer) 
         */
        clicked |= (clicked_ & CHtmlDispLink_divhover);
    }

    /* set the new state for myself and my linked sub-items */
    set_clicked_sub(win, clicked);
}

/*
 *   Set the clicked state for myself and my linked sub-items.  This routine
 *   doesn't notify the enclosing DIV - it only affects my sub-items. 
 */
void CHtmlDispLink::set_clicked_sub(CHtmlSysWin *win, int clicked)
{
    CHtmlDisp *disp;

    /* 
     *   ignore the 'clicked-off' state - this is equivalent to no
     *   highlighting, or div-hover if there's an enclosing division that
     *   reveals links 
     */
    clicked &= ~CHtmlDispLink_clickedoff;

    /* remember the new state */
    clicked_ = clicked;

    /* 
     *   invalidate all of the linkable items in my list, since they may
     *   have a different appearance when clicked 
     */
    for (disp = first_ ; disp != 0 ; disp = disp->get_next_disp())
    {
        /* 
         *   if this item is linked to me, invalidate it; don't bother
         *   with items that aren't linked to me (which should simply mean
         *   that they're not linkable at all - everything in my list
         *   that's linkable at all should be linked to me), since their
         *   screen appearance isn't affected by this change 
         */
        if (disp->get_container_link() == this)
            disp->on_click_change(win);

        /* if this was the last one, we're done */
        if (disp == last_)
            break;
    }
}


#if 0 // removed

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlDispDelegator implementation 
 */

/*
 *   Delegate the result of a line break.  Let the underlying item find
 *   its own line break, then redirect the return value accordingly: if
 *   the underlying item breaks at its own start, return myself, since I
 *   always cover the underlying item.  If the underlying item creates a
 *   new item, create a clone of myself to cover the new item.  
 */
CHtmlDisp *CHtmlDispDelegator::
   delegate_line_break(CHtmlFormatter *formatter, CHtmlDisp *line_start,
                       CHtmlLineBreak *break_pos)
{
    /* 
     *   if they inserted themselves as the next line break object,
     *   translate that to me 
     */
    if (break_pos->item_ == delegate_)
        break_pos->item_ = this;
    
    /* if it returned null, return null */
    if (line_start == 0)
        return 0;

    /* if it returned itself, return me, since I always cover it */
    if (line_start == delegate_)
        return this;

    /* 
     *   it must have created a new item - we need to create a new
     *   delegator (of my same type) to cover the newly created item 
     */
    return clone_delegator(formatter, line_start);
}

#endif // removed

/* ------------------------------------------------------------------------ */
/*
 *   "Body" display object 
 */

/* 
 *   animated image display site - invalidate 
 */
void CHtmlDispBody::dispsite_inval(unsigned int x, unsigned int y,
                                   unsigned int width, unsigned int height)
{
    /* ask the window to invalidate the background image area */
    if (win_ != 0)
        win_->inval_html_bg_image(x, y, width, height);
}

/* ------------------------------------------------------------------------ */
/*
 *   text display object implementation 
 */

CHtmlDispText::CHtmlDispText(CHtmlSysWin *win, CHtmlSysFont *font,
                             const textchar_t *txt, size_t len,
                             unsigned long txtofs)
{
    CHtmlPoint siz;
    int ascent;
    
    /* set members */
    font_ = font;
    txt_ = txt;
    len_ = (unsigned short)len;
    displen_ = (unsigned short)len;
    txtofs_ = txtofs;

    /* measure the size */
    siz = win->measure_text(font, txt, len, &ascent);
    pos_.set(0, 0, siz.x, siz.y);
    ascent_ht_ = (unsigned short)ascent;
}

/*
 *   additional construction-time initialization for linked text 
 */
void CHtmlDispText::linked_text_cons(CHtmlSysWin *win, CHtmlDispLink *link)
{
    CHtmlSysFont *font;
    CHtmlPoint siz;
    int ascent;

    /* tell the link object about us */
    link->add_link_item(this);

    /* 
     *   get the font we use for drawing as a link in 'hover' and 'clicked'
     *   state - this will ensure that we'll leave space for any
     *   ornamentation (such as underlining) that we apply in our fully
     *   ornamented mode 
     */
    font = get_draw_text_font(win, link,
                              CHtmlDispLink_clicked | CHtmlDispLink_hover);

    /* calculate our *real* size, now that we have the proper font */
    siz = win->measure_text(font, txt_, len_, &ascent);
    pos_.bottom = pos_.top + siz.y;
    ascent_ht_ = (unsigned short)ascent;
}

/*
 *   draw the text
 */
void CHtmlDispText::draw(CHtmlSysWin *win,
                         unsigned long sel_start, unsigned long sel_end,
                         int clicked)
{
    /* draw the text normally (normal text is never linked) */
    draw_text(win, sel_start, sel_end, 0, clicked, &pos_);
}

/*
 *   draw the text, with appropriate style settings
 */
void CHtmlDispText::draw_text(CHtmlSysWin *win,
                              unsigned long sel_start, unsigned long sel_end,
                              class CHtmlDispLink *link, int clicked,
                              const CHtmlRect *orig_pos)
{
    const textchar_t *p;
    size_t rem;
    unsigned long ofs;
    CHtmlRect pos;
    CHtmlSysFont *font;

    /* start at the beginning of the string */
    p = txt_;
    rem = displen_;
    ofs = txtofs_;
    pos = *orig_pos;

    /* get the font, adjusting for the link appearance if necessary */
    font = get_draw_text_font(win, link, clicked);

    /* 
     *   as an optimization, if we're entirely normal or entirely
     *   highlighted, draw in one piece -- this should go faster than the
     *   general routine, since we don't have to measure the text again
     *   (measuring requires a system call) 
     */
    if (ofs >= sel_end || ofs + displen_ <= sel_start
        || (ofs >= sel_start && ofs + displen_ <= sel_end))
    {
        /* draw all of our text with the appropriate highlighting */
        win->draw_text(ofs >= sel_start && ofs + displen_ <= sel_end,
                       pos.left, pos.top, font, txt_, displen_);

        /* we're done - there's nothing more to draw */
        return;
    }

    /*
     *   If we contain the starting point, draw up to the starting point
     *   in normal color 
     */
    if (ofs < sel_start)
        draw_text_part(win, FALSE, &pos, &p, &rem, &ofs,
                       (unsigned long)(sel_start - ofs), font);

    /*
     *   If we contain the ending point, draw up to the ending point in
     *   highlight color 
     */
    if (ofs >= sel_start && ofs < sel_end)
        draw_text_part(win, TRUE, &pos, &p, &rem, &ofs, sel_end - ofs, font);

    /*
     *   If there's anything left after the ending point, draw the
     *   remainder in normal color 
     */
    if (rem > 0)
        draw_text_part(win, FALSE, &pos, &p, &rem, &ofs, rem, font);
}

/*
 *   Get the font to use when drawing our text.  We'll use our current font,
 *   adjusting it if needed for the hyperlink appearance.  
 */
CHtmlSysFont *CHtmlDispText::get_draw_text_font(CHtmlSysWin *win,
                                                class CHtmlDispLink *link,
                                                int clicked)
{
    CHtmlSysFont *font;
    int link_style = (link != 0 ? link->get_style() : LINK_STYLE_PLAIN);

    /* start with the current font */
    font = font_;

    /*
     *   If we're drawing a link, add the appropriate color and underlining
     *   style for link text to the font.  We're drawing this as a link if
     *   either we have the "normal" link style and links are being shown, OR
     *   we have the "forced" link style, OR we have the "hidden" link style
     *   and the link is in 'hover' mode and links are being shown.  (The
     *   "forced" style overrides any global link visibility setting, so we
     *   use the link appearance in this case regardless of the global
     *   settings.)  
     */
    if (link_style == LINK_STYLE_FORCED
        || (link_style == LINK_STYLE_NORMAL && win->get_html_show_links())
        || (link_style == LINK_STYLE_HIDDEN
            && (clicked & (CHtmlDispLink_hover | CHtmlDispLink_clicked)) != 0
            && win->get_html_show_links()))
    {
        CHtmlFontDesc font_desc;

        /* 
         *   get the current font's description, so that we can add the
         *   hyperlink rendering features to the attributes inherited from
         *   the surrounding text 
         */
        font_->get_font_desc(&font_desc);

        /* 
         *   Get the font color.  If the containing font has a non-default
         *   color, it means that we have an explicit <FONT COLOR=xxx> inside
         *   the <A> tag, so use the font's color setting.  If the containing
         *   font uses the default color, then use the appropriate color
         *   depending on the link's status.  
         */
        if (font_desc.default_color)
        {
            /* 
             *   the font doesn't have an explicit color setting, so use the
             *   appropriate hyperlink text color according to the current
             *   status of the link (normal, clicked, or hovering) 
             */
            font_desc.color = ((clicked & CHtmlDispLink_clicked) != 0
                               ? win->get_html_alink_color()
                               : ((clicked & CHtmlDispLink_hover) != 0
                                  ? win->get_html_hlink_color()
                                  : win->get_html_link_color()));

            /* note that we now have an explicitly-set color */
            font_desc.default_color = FALSE;
        }

        /* 
         *   If we're hovering or clicked, and we have explicit hover colors,
         *   apply them.  Any explicit hover colors override the color we've
         *   come up with so far.  
         */
        if ((clicked & (CHtmlDispLink_hover | CHtmlDispLink_clicked)) != 0
             && link != 0)
        {
            HTML_color_t c;

            /* apply the explicit foreground hover color, if set */
            if (link->get_hover_fg(&c))
            {
                font_desc.color = c;
                font_desc.default_color = FALSE;
            }

            /* apply the explicit background hover color, if set */
            if (link->get_hover_bg(&c))
            {
                font_desc.bgcolor = c;
                font_desc.default_bgcolor = FALSE;
            }

            /* add underlining, if set */
            if (link->hover_underline())
                font_desc.underline = TRUE;
        }

        /* 
         *   Underline the font if appropriate.  For normal and FORCED links,
         *   add the underline if the window style says so.  For HOVER links,
         *   don't add the underline - we'll let the HOVER style determine
         *   that.  For PLAIN links, don't add the underline.  Note that we
         *   never take away an underline if it's inherited from enclosing
         *   text.  
         */
        if ((link_style == LINK_STYLE_NORMAL
             || link_style == LINK_STYLE_FORCED)
            && win->get_html_link_underline())
            font_desc.underline = TRUE;

        /* get a new font matching the modified description */
        font = win->get_font(&font_desc);
    }

    /* return the font we decided upon */
    return font;
}

/*
 *   Draw a portion of the text, with optional selection highlighting,
 *   from a given point in the string, for a given maximum length.  We'll
 *   update the string pointer (*p), length remaining pointer (*rem),
 *   offset pointer (*ofs), and position pointer (*pos), according to how
 *   much we draw; we'll draw the lesser of *rem and len.  
 */
void CHtmlDispText::draw_text_part(CHtmlSysWin *win, int hilite,
                                   CHtmlRect *pos, const textchar_t **p,
                                   size_t *rem, unsigned long *ofs,
                                   unsigned long len, CHtmlSysFont *font)
{
    /* limit drawing to the lesser of *rem and len */
    if (len > *rem) len = *rem;

    /* draw it */
    win->draw_text(hilite, pos->left, pos->top, font, *p, len);

    /*
     *   update the counters according to what we drew; as an
     *   optimization, we don't need to update anything but the remaining
     *   length pointer (*rem) if there's nothing left, since we won't be
     *   doing any more drawing on this string in that case 
     */
    *rem -= len;
    if (*rem != 0)
    {
        /* move right by the size of the text we drew */
        pos->left += win->measure_text(font, *p, len, 0).x;

        /* update the string pointer and the offset */
        *p += len;
        *ofs += len;
    }
}

/*
 *   get the horizontal extent of the text
 */
int CHtmlDispText::measure_width(CHtmlSysWin *)
{
    /* return the horizontal extent of the text in the current font */
    return pos_.right - pos_.left;
}

/*
 *   invalidate a range 
 */
void CHtmlDispText::inval_range(CHtmlSysWin *win,
                                unsigned long start_ofs,
                                unsigned long end_ofs)
{
    unsigned long ofs;
    size_t len;
    CHtmlRect rc;

    /* if the range is outside of my text, do nothing */
    if (end_ofs <= txtofs_ || start_ofs >= txtofs_ + len_)
        return;

    /* start off with my entire area and range */
    rc = pos_;
    ofs = 0;
    len = len_;

    /* if the starting point is within my text, set the left limit */
    if (start_ofs > txtofs_)
    {
        /* skip to the starting offset */
        ofs = start_ofs - txtofs_;

        /* limit the length */
        len -= ofs;

        /* adjust the left end for the offset */
        rc.left = get_text_pos(win, txtofs_ + ofs).x;
    }

    /* if the ending point is within my text, set the right limit */
    if (end_ofs < txtofs_ + len_)
    {
        /* reduce the length */
        len -= txtofs_ + len_ - end_ofs;

        /* adjust the right end for the offset */
        rc.right = get_text_pos(win, txtofs_ + ofs + len).x;
    }

    /* 
     *   Adjust the range by a few pixels at each side for character
     *   overhand.  (We really should get the actual text metrics here,
     *   but we'll put that off for now and just use a few pixels of slop
     *   instead.)  
     */
    if (rc.left < 3)
        rc.left = 0;
    else
        rc.left -= 3;

    rc.right += 3;

    /* invalidate the area */
    win->inval_doc_coords(&rc);
}

/*
 *   measure for table metrics 
 */
void CHtmlDispText::measure_for_table(CHtmlSysWin *win,
                                      CHtmlTableMetrics *metrics)
{
    const textchar_t *last_break_pos;
    const textchar_t *p;
    const textchar_t *nxt;
    textchar_t prev_char;
    size_t rem;
    oshtml_charset_id_t charset;

    /* note our character set */
    charset = font_->get_charset();

    /* start at our first character */
    p = txt_;
    rem = len_;
    
    /* 
     *   the last character of the previous item is the initial previous
     *   character 
     */
    prev_char = metrics->last_char_;

    /* 
     *   if we have a no-break flag just before us, or we're in a <NOBR>
     *   section, skip our first character 
     */
    if ((metrics->no_break_ || metrics->no_break_sect_) && len_ != 0)
    {
        /* skip our first character, since we can't break there */
        nxt = os_next_char(charset, p);
        rem -= (nxt - p);
        p = nxt;
    }

    /* 
     *   check to see if we can break just after the previous item - that
     *   is, immediately to the left of this item 
     */
    if (len_ != 0
        && !metrics->no_break_sect_
        && allow_line_break_after(metrics->last_char_, *txt_))
    {
        /* 
         *   We can break just before our first character - add any trailing
         *   space from the previous item, and start at the beginning of our
         *   line.  
         */
        last_break_pos = txt_;

        /* add our width up to this point to the leftover */
        metrics->leftover_width_ +=
            win->measure_text(font_, txt_, p - txt_, 0).x;

        /* we don't need to use the leftover width */
        metrics->clear_leftover();
    }
    else
    {
        /*
         *   We can't break at our first character - we don't have a break
         *   position established yet 
         */
        last_break_pos = 0;
    }

    /*
     *   Run through the line, find word breaks, and measure unbreakable
     *   stretches of text (words in word-wrap mode, characters in
     *   character-wrap mode).  
     */
    for ( ; rem != 0 ; rem -= (nxt - p), p = nxt)
    {
        /* note the location of the next character after this one */
        nxt = os_next_char(charset, p);
        
        /*
         *   we can break here if it's a space, or the previous character
         *   is a hyphen and this character isn't a hyphen 
         */
        if (!metrics->no_break_sect_ && allow_line_break_at(prev_char, *p))
        {
            long wid;

            /* measure the word if there's anything to measure */
            if (p != last_break_pos)
            {
                /* 
                 *   Figure the size of the current word.  If there's a
                 *   previous break position, it's the width from the
                 *   previous break position to the current position;
                 *   otherwise, it's the width of the start of the item to
                 *   the current position plus the leftover from the
                 *   previous item.  
                 */
                if (last_break_pos != 0)
                {
                    /* measure the width from the last break position */
                    wid = win->
                          measure_text(font_, last_break_pos,
                                        (size_t)(p - last_break_pos), 0).x;
                }
                else
                {
                    /* 
                     *   measure the width from the start of the line to
                     *   the current position, and add the leftover width
                     *   from the previous item 
                     */
                    wid = win->measure_text(font_, txt_,
                                            (size_t)(p - txt_), 0).x
                          + metrics->leftover_width_;

                    /* we've now consumed the leftover width */
                    metrics->leftover_width_ = 0;
                }

                /* add this word to the metrics as an indivisible item */
                metrics->add_indivisible(wid);

                /* add the word to the line total */
                metrics->add_to_cur_line(wid);
            }

            /* add the separator to the line total */
            wid = win->measure_text(font_, p, nxt - p, 0).x;
            metrics->add_to_cur_line(wid);

            /* remember the next character as the most recent break */
            last_break_pos = nxt;
        }

        /* remember the previous character */
        prev_char = *p;
    }

    /*
     *   If there aren't any break positions within this item, add my
     *   entire width to the leftover total.  Otherwise, add the remainder
     *   from the last break position to the leftover total.  
     */
    if (last_break_pos == 0)
        metrics->leftover_width_ += measure_width(win);
    else if (last_break_pos < txt_ + len_)
        metrics->leftover_width_ +=
            win->measure_text(font_, last_break_pos,
                              (size_t)(len_ - (last_break_pos - txt_)), 0).x;

    /* remember my last character, if I have any characters */
    if (len_ != 0)
        metrics->last_char_ = prev_char;
}

/*
 *   Find the next line break.  This routine is a little complicated,
 *   mostly because it's important that it goes as fast as possible.  Our
 *   strategy is to first determine if we can fit in the allotted space;
 *   if so, we'll simply find the last break position within our text and
 *   return.  Otherwise, we'll figure out how many of our characters will
 *   fit by asking the operating system to do the measurement, then we'll
 *   find the last breakable position before that size.  
 */
CHtmlDisp *CHtmlDispText::find_line_break(CHtmlFormatter *formatter,
                                          CHtmlSysWin *win,
                                          long space_avail, 
                                          CHtmlLineBreak *break_pos)
{
    size_t fit_cnt;
    size_t cur_cnt;
    textchar_t prev_char;
    const textchar_t *p;
    oshtml_charset_id_t charset;

    /* get our font's character set */
    charset = font_->get_charset();

    /* determine how much of our text will fit in the available space */
    if (space_avail <= 0)
    {
        /* we obviously can't fit anything into zero or negative space */
        fit_cnt = 0;
    }
    else if (space_avail == 0)
    {
        /* 
         *   there's no space available, so we can't fit anything; but since
         *   the preceding material fits exactly, we can check for a break
         *   at the current position 
         */
        fit_cnt = 0;
    }
    else if (space_avail >= pos_.right - pos_.left)
    {
        /* the available space is bigger than we are, so we fit entirely */
        fit_cnt = len_;
    }
    else
    {
        /* 
         *   we're bigger than the available space, so calculate how much of
         *   our text (starting from the left) will fit in the available
         *   space 
         */
        fit_cnt = win->get_max_chars_in_width(font_, txt_, len_, space_avail);
    }

    /* start scanning at the start of our text */
    prev_char = break_pos->last_char_;
    p = txt_;
    cur_cnt = 0;

    /*
     *   store the last character of this item in the break state, so that
     *   the next item can determine if it can break at its start (if we
     *   don't have any text at all, don't change the previous character,
     *   since we didn't add anything) 
     */
    if (len_ != 0)
        break_pos->last_char_ = get_break_pos_last_char();

    /* 
     *   if an explicit no-break flag immediately precedes us, then skip out
     *   first character, since we can't break at our first character if
     *   we're explicitly told not to 
     */
    if (break_pos->no_break_ && cur_cnt < len_)
    {
        /* note the initial character in the break tracker */
        break_pos->note_char(*p, this, (void *)p);

        /* we can't break at the first character, so skip it */
        prev_char = *p;
        p = os_next_char(charset, p);
        cur_cnt = p - txt_;
    }

    /*
     *   If we allow breaking after the previous character, and we have at
     *   least one more character, we can break at the current position.
     *   
     *   We have to check this case specially because of the possibility of
     *   a no-break flag preceding us: we'll have skipped our first
     *   character, because we can't break to its left, but if we have at
     *   least two characters then we might still be able to break to the
     *   right of the first character.  In the main loop, we only check to
     *   see if we can break to the left of the current character, so we
     *   must check specially for breaking to the right of the previous
     *   character on the first iteration only.  
     */
    if (cur_cnt < len_ && allow_line_break_after(prev_char, *p))
    {
        /* 
         *   Tell the break tracker that we can break at the current
         *   position.
         *   
         *   Note that we haven't yet told the break tracker about the
         *   current character (what's at *p), because we're breaking to the
         *   LEFT of the current character - thus the only thing that
         *   matters to the break tracker is what comes before the current
         *   character.  
         */
        break_pos->set_break_pos(this, (void *)p);
    }

    /* 
     *   If we have non-zero length, clear out the no-break flag in the
     *   break state, as we are not an explicit no-break indicator.  (If
     *   we're empty, we have no effect on the no-break state, as we do not
     *   contribute any text that would allow breaking.)  
     */
    if (len_ != 0)
        break_pos->no_break_ = FALSE;

    /* scan our text for breaking points */
    for ( ; cur_cnt < len_ ; p = os_next_char(charset, p), cur_cnt = p - txt_)
    {
        /* note the current character */
        break_pos->note_char(*p, this, (void *)p);

        /* check to see if we allow breaking here */
        if (allow_line_break_at(prev_char, *p))
        {
            /* note the break position */
            break_pos->set_break_pos(this, (void *)p);
        }

        /* 
         *   If we've at least reached the first character past what will
         *   fit, and we've found a valid breaking point somewhere up to
         *   this point, stop here and break the line at the latest breaking
         *   point.  We know we can't fit anything more in the available
         *   space, so we have to go back to the most recent breaking point
         *   and break the line there.
         *   
         *   If we haven't yet reached the maximum fit point, keep looking,
         *   since we might find another, better breakpoint later in our
         *   text.  
         */
        if (cur_cnt >= fit_cnt && break_pos->item_ != 0)
        {
            /* break at the most recent valid break point */
            return break_pos->item_->do_line_break(formatter, win, break_pos);
        }

        /* remember the previous character for the next round */
        prev_char = *p;
    }

    /*
     *   If we got this far, it either means that we've scanned this entire
     *   item and we still haven't found a breaking point, or that we didn't
     *   need a breaking point because this item entirely fits.
     *   
     *   If we've scanned past the maximum fit size, and we have found a
     *   break point, we now know that we can't do any better than that
     *   previous break point, so break there.  
     */
    if (cur_cnt > fit_cnt && break_pos->item_ != 0)
    {
        /* 
         *   there's no break within this item, and we're past the maximum
         *   fit point - go back and break at the previous item 
         */
        return break_pos->item_->do_line_break(formatter, win, break_pos);
    }

    /* return null to indicate that we didn't break the line */
    return 0;
}

/*
 *   Apply a line break position that we found in this object and noted,
 *   before we moved on to another object.  An object after us in the
 *   display list will call back to us if it finds that it needs to do a
 *   break but can't find a suitable spot later than the position we
 *   already noted. 
 */
CHtmlDisp *CHtmlDispText::do_line_break(CHtmlFormatter *formatter,
                                        CHtmlSysWin *win,
                                        CHtmlLineBreak *break_pos)
{
    return break_at_pos(formatter, win, (const textchar_t *)break_pos->pos_);
}

/*
 *   Apply a line break position that we previously found in a prior item,
 *   skipping whitespace at the start of the new line.  
 */
CHtmlDisp *CHtmlDispText::do_line_break_skipsp(CHtmlFormatter *formatter,
                                               CHtmlSysWin *win)
{
    /* break from the start of this item */
    return break_at_pos(formatter, win, txt_);
}

/*
 *   Break the line into two pieces at a given position within our text 
 */
CHtmlDisp *CHtmlDispText::break_at_pos(CHtmlFormatter *formatter,
                                       CHtmlSysWin *win,
                                       const textchar_t *last_break_pos)
{
    CHtmlDispText *newdisp;
    CHtmlRect newpos;
    const textchar_t *p;
    size_t rem;
    oshtml_charset_id_t charset;

    /* note our character set */
    charset = font_->get_charset();

    /*
     *   if the last break position contains a space, skip the leading
     *   space, since this text will be at the start of a new line 
     */
    p = last_break_pos;
    rem = len_ - (p - txt_);
    while (rem != 0 && is_space(*p))
    {
        const textchar_t *nxt = os_next_char(charset, p);
        rem -= (nxt - p);
        p = nxt;
    }
    
    /* 
     *   If we're breaking at the beginning or end of the current item,
     *   skip creation of an empty text item.  
     */
    if (rem == len_)
    {
        /* 
         *   my entire text will go into the new item, so this is
         *   equivalent to just breaking at the start of this item 
         */
        return this;
    }
    else if (rem == 0)
    {
        /*
         *   I'm going to break after all of my text, so this is equivalent
         *   to breaking at the start of the next item.  Make sure we skip
         *   past any spaces before we select a breaking position.  
         */
        if (get_next_disp() != 0)
            return get_next_disp()->do_line_break_skipsp(formatter, win);
        else
            return 0;
    }

    /*
     *   create a new text object for the part of the text after the last
     *   break position, link it into the display list after the current
     *   item, and return it - we'll break just before the new item.  
     */
    newdisp = create_disp_for_break(formatter, win, font_, p, rem,
                                    txtofs_ + len_ - rem);
    newdisp->add_after(this);

    /* adjust my size to remove the newly created area */
    newpos = newdisp->get_pos();
    pos_.right -= (newpos.right - newpos.left);

    /* keep only the text up to the new part */
    len_ -= (unsigned short)rem;
    displen_ = len_;

    /* return the new object - it's the start of the next line */
    return newdisp;
}

/*
 *   Find trailing whitespace 
 */
void CHtmlDispText::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    const textchar_t *p;
    const textchar_t *nxt;
    size_t rem;
    oshtml_charset_id_t charset;

    /* note our character set */
    charset = font_->get_charset();

    /* scan for whitespace */
    for (p = txt_, rem = len_ ; rem != 0 ;
         nxt = os_next_char(charset, p), rem -= nxt - p, p = nxt)
    {
        /* check if this is whitespace */
        if (is_space(*p))
        {
            /* it's whitespace - note the start of a run of whitespace */
            info->set_ws_pos(this, (void *)p);
        }
        else
        {
            /* it's not whitespace, so we're not in a run of whitespace */
            info->clear_ws_pos();
        }
    }
}

/*
 *   Remove trailing whitespace from a position we previously found. 
 */
void CHtmlDispText::remove_trailing_whitespace(CHtmlSysWin *win, void *pos)
{
    /* check to see if the run of whitespace started in me */
    if (pos != 0)
    {
        /* 
         *   The run of whitespace starts within this item.  Calculate our
         *   new length as the distance from the start of our text to the
         *   first character of the trailing whitespace, and set the new
         *   length - this will remove the whitespace from the display item.
         */
        len_ = ((const textchar_t *)pos) - txt_;
    }
    else
    {
        /* 
         *   the run of whitespace started in a prior item - simply collapse
         *   down to zero width 
         */
        len_ = 0;
    }

    /* display at the new length */
    displen_ = len_;

    /* calculate our new display size */
    pos_.right = pos_.left + win->measure_text(font_, txt_, len_, 0).x;
}

/*
 *   Hide trailing whitespace 
 */
void CHtmlDispText::hide_trailing_whitespace(CHtmlSysWin *win, void *pos)
{
    /* check to see if the run of whitespace started in me */
    if (pos != 0)
    {
        /* it's part of my text, so hide from the whitespace on */
        displen_ = ((const textchar_t *)pos) - txt_;
    }
    else
    {
        /* it's not part of my text, so hide the entire thing */
        displen_ = 0;
    }
}

/*
 *   Measure width of trailing whitesapce 
 */
long CHtmlDispText::measure_trailing_ws_width(CHtmlSysWin *win) const
{
    const textchar_t *p;
    size_t rem;
    const textchar_t *wspos;

    /* scan for whitespace */
    for (p = txt_ + len_, rem = len_, wspos = 0 ; rem != 0 ; --rem)
    {
        /* move to the next character */
        --p;

        /* if it's not whitespace, we can stop looking */
        if (!is_space(*p))
            break;
    }

    /* if we didn't find any spaces, the width is zero */
    if (rem == len_)
        return 0;

    /* measure the width of the spaces */
    return win->measure_text(font_, p + 1, len_ - rem, 0).x;
}

/*
 *   Get my text space needs 
 */
int CHtmlDispText::get_text_height(CHtmlSysWin *)
{
    /* return my space above the text baseline */
    return ascent_ht_;
}

/*
 *   Get my overall vertical space needs 
 */
void CHtmlDispText::get_vertical_space(CHtmlSysWin *, int /*text_height*/,
                                       int *above_base, int *below_base,
                                       int *total)
{
    const CHtmlFontDesc *desc;

    /* calculate the total height */
    *total = (pos_.bottom - pos_.top);

    /* set the amount above the font baseline */
    *above_base = ascent_ht_;

    /* 
     *   calculate the amount below the baseline as the total less the
     *   amount above the baseline 
     */
    *below_base = *total - *above_base;

    /* 
     *   if I'm a superscript or a subscript, offset the positions
     *   accordingly 
     */
    desc = font_->get_font_desc_ref();
    if (desc->superscript)
    {
        *above_base += *total / 4;
        *below_base -= *total / 4;
        if (*below_base < 0) *below_base = 0;
    }
    else if (desc->subscript)
    {
        *above_base -= *total / 4;
        *below_base += *total / 4;
        if (*above_base < 0) *above_base = 0;
    }
}

/*
 *   Set my position 
 */
void CHtmlDispText::set_line_pos(class CHtmlSysWin *, int left, int top,
                                 int /*total_height*/, int text_base_offset,
                                 int /*max_text_height*/)
{
    int total;
    const CHtmlFontDesc *desc;

    /* reset to my original display size */
    displen_ = len_;

    /* measure the total height and the amount above the baseline we need */
    total = (pos_.bottom - pos_.top);

    /*
     *   We want to be aligned such that our text baseline matches the
     *   text baselines of other text items on the line, so set my top to
     *   my text height above the text baseline. 
     */
    pos_.offset(left - pos_.left,
                top - pos_.top + text_base_offset - ascent_ht_);

    /* 
     *   if I'm a superscript or a subscript, offset the positions
     *   accordingly 
     */
    desc = font_->get_font_desc_ref();
    if (desc->superscript)
        pos_.offset(0, -total/4);
    else if (desc->subscript)
        pos_.offset(0, total/4);
}

/*
 *   Get the position of a character within this item 
 */
CHtmlPoint CHtmlDispText::get_text_pos(CHtmlSysWin *win,
                                       unsigned long txtofs)
{
    int wid;
    size_t ofs;

    /* get the offset, and make sure it's limited to our text */
    ofs = (size_t)(txtofs - txtofs_);
    if (ofs > len_)
        ofs = len_;
    
    /* get the width of the text up to the given character */
    if (ofs != 0)
        wid = win->measure_text(font_, txt_, ofs, 0).x;
    else
        wid = 0;

    /*
     *   return my upper left position offset right by the width of the
     *   characters up to the character of interest 
     */
    return CHtmlPoint(pos_.left + wid, pos_.top);
}

/*
 *   Get the text offset of a character given its position 
 */
unsigned long CHtmlDispText::find_textofs_by_pos(CHtmlSysWin *win,
    CHtmlPoint pos) const
{
    size_t fit_cnt;

    /*
     *   if it's outside my bounds, place it at the start or end of my
     *   text run; if it's above or to the left or me, it's at the start,
     *   otherwise at the end 
     */
    if (!pos_.contains(pos))
    {
        /* see whether it's above/left or below/right */
        if (pos.y <= pos_.top || pos.x <= pos_.left)
            return txtofs_;
        else
            return txtofs_ + len_;
    }

    /* figure out how much of our text is before the given position */
    fit_cnt = win->get_max_chars_in_width(font_, txt_, len_,
                                          pos.x - pos_.left);

    /* if we're over halfway into the next character, skip ahead one */
    if (fit_cnt + 1 <= len_
        && pos.x > (pos_.left + win->measure_text(font_, txt_, fit_cnt, 0).x
                    + win->measure_text(font_, txt_ + fit_cnt, 1, 0).x / 2))
        ++fit_cnt;

    /* return the text offset of the selected character */
    return txtofs_ + fit_cnt;
}

    
/*
 *   Add my text to a buffer, limiting the change to the given starting
 *   and ending offsets 
 */
void CHtmlDispText::add_text_to_buf_clip(CStringBuf *buf,
                                         unsigned long startofs,
                                         unsigned long endofs) const
{
    size_t ofs;
    size_t len;

    /* 
     *   compute the starting offset within my buffer: if the desired
     *   starting offset is after my starting point, offset my starting
     *   point accordingly 
     */
    if (startofs > txtofs_)
        ofs = startofs - txtofs_;
    else
        ofs = 0;

    /* limit the length to the amount remaining */
    if (ofs > len_)
        len = 0;
    else
        len = len_ - ofs;

    /* 
     *   if the desired ending offset is before my ending point, limit the
     *   amount we'll copy accordingly 
     */
    if (endofs < txtofs_ + ofs)
        len = 0;
    else if (endofs < txtofs_ + ofs + len)
        len = endofs - txtofs_ - ofs;

    /* if we have anything left to copy, copy it now */
    if (len > 0)
        buf->append(txt_ + ofs, len);
}


/*
 *   Get the cursor type for a linked text item.  This is a service routine
 *   for our linked subclasses.  
 */
Html_csrtype_t CHtmlDispText::get_cursor_type_for_link(
    class CHtmlSysWin *win, class CHtmlFormatter *formatter,
    int x, int y, int more_mode) const
{
    /* 
     *   If links are disabled, or we're in "more" mode, return the default
     *   I-beam cursor.  If we have the "forced" style, we're visible even
     *   when links are globally disabled, so show the link "hand" cursor
     *   instead in this case.  
     */
    if (more_mode
        || (!win->get_html_show_links()
            && !get_link(formatter, x, y)->is_forced()))
        return HTML_CSRTYPE_IBEAM;

    /* use the hand cursor */
    return HTML_CSRTYPE_HAND;
}

/*
 *   Draw this text item as a link.  This is a service routine for our
 *   linked subclasses. 
 */
void CHtmlDispText::draw_as_link(CHtmlSysWin *win,
                                 unsigned long sel_start,
                                 unsigned long sel_end,
                                 CHtmlDispLink *link)
{
    /*
     *   If we're in the DIV-hover state, we're to draw in the hover mode
     *   applicable to our linked DIV, not our own direct link.  If we have
     *   any other flags set, ignore the DIV-hover and use our direct link's
     *   appearance.  
     */
    if (link->get_clicked() == CHtmlDispLink_divhover)
    {
        CHtmlDispDIV *div;
        
        /* find our immediate enclosing division */
        div = win->get_formatter()->find_div_by_ofs(get_text_ofs(), pos_.top);

        /* find the nearest enclosing DIV that's in hover state */
        for ( ; div != 0 ; div = div->get_parent_div())
        {
            CHtmlDispLinkDIV *div_link;
            
            /* if this DIV has a hover-state link, it's the one */
            if ((div_link = div->get_div_link()) != 0
                && (div_link->get_clicked() & CHtmlDispLink_hover) != 0)
            {
                /* use this DIV link */
                link = div_link;

                /* stop looking - just go draw */
                break;
            }
        }
    }

    /*
     *   Draw my text as a linked item, drawing it in the "clicked" style if
     *   we're being clicked.  If the link has the "plain" rendering style,
     *   render as ordinary text unless we're being clicked. 
     */
    draw_text(win, sel_start, sel_end, link,
              link->is_plain() ? FALSE : link->get_clicked(), &pos_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Preformatted text item 
 */

/*
 *   Measure for table metrics.  Performatted items can't be broken, so
 *   this entire item must be added as an indivisible item.  
 */
void CHtmlDispTextPre::measure_for_table(CHtmlSysWin *win,
                                         CHtmlTableMetrics *metrics)
{
    /* 
     *   act like the base display item - we're indivisible, but we can
     *   break to our left or right 
     */
    CHtmlDisp::measure_for_table(win, metrics);
}

/*
 *   Find the next line break.  Preformatted text doesn't allow line
 *   breaks, since line breaks are part of the preformatting.  
 */
CHtmlDisp *CHtmlDispTextPre::find_line_break(CHtmlFormatter *,
                                             CHtmlSysWin *win,
                                             long space_avail,
                                             CHtmlLineBreak *break_pos)
{
    /* 
     *   If I don't fit in the given area, break at the start of this item.
     *   However, don't allow breaking here if we have an explicit no-break
     *   flag just before me.  
     */
    if (space_avail < measure_width(win) && !break_pos->no_break_)
        return this;

    /* allow breaking after this item */
    break_pos->last_char_ = ' ';
    break_pos->no_break_ = FALSE;

    /* 
     *   even if we contain whitespace, we don't count this as a run of
     *   whitespace, since pre-formatted spaces aren't trimmable 
     */
    break_pos->note_non_ws();

    /* return null to indicate that we didn't break the line */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Text input display item 
 */

CHtmlDispTextInput::CHtmlDispTextInput(class CHtmlSysWin *win,
                                       class CHtmlSysFont *font,
                                       const textchar_t *txt, size_t len,
                                       unsigned long txtofs)
    : CHtmlDispText(win, font, txt, len, txtofs)
{
    textchar_t *newbuf;

    /*
     *   make a private copy of our text buffer, so that if the
     *   underlying text buffer changes, we can compare our buffer to the
     *   new buffer 
     */
    newbuf = new textchar_t[len];
    memcpy(newbuf, txt, len);
    txt_ = newbuf;
        
    /* we're the only thing in the our input line so far */
    input_continuation_ = 0;
}

CHtmlDispTextInput::~CHtmlDispTextInput()
{
    /* delete our private buffer */
    delete [] (textchar_t *)txt_;
}

void CHtmlDispTextInput::diff_input_lists(CHtmlDispTextInput *other,
                                          class CHtmlSysWin *win)
{
    const textchar_t *p1;
    const textchar_t *p2;
    size_t len;
    int did_inval;
    
    /*
     *   Compare my buffer with the other one.  If we find a difference,
     *   start invalidating at the difference. 
     */
    for (did_inval = FALSE, len = 0, p1 = txt_, p2 = other->txt_ ;
         len < len_ && len < other->len_ ; ++p1, ++p2, ++len)
    {
        /* see if this character is different */
        if (*p1 != *p2)
        {
            /* they differ - invalidate from here to the end of the line */
            inval_eol(win, p1 - txt_);

            /* note that we did the invalidation */
            did_inval = TRUE;

            /*
             *   no need to continue checking, since we're redrawing the
             *   entire rest of the line 
             */
            break;
        }
    }

    /*
     *   If we didn't already invalidate the rest of the line, and if one
     *   of the lists ran out before the other, invalidate from the
     *   current position to the end of the line 
     */
    if (!did_inval && len_ != other->len_)
    {
        /* invalidate the one with more text remaining */
        if (len_ > other->len_)
            inval_eol(win, p1 - txt_);
        else
            other->inval_eol(win, p2 - other->txt_);
    }

    /* if more items follow in both lists, diff the following items */
    if (input_continuation_ != 0 && other->input_continuation_ != 0)
    {
        /* both have follow-on's - diff those */
        input_continuation_->
            diff_input_lists(other->input_continuation_, win);
    }
    else
    {
        /*
         *   one ends here, the other doesn't - invalidate to the bottom
         *   of the display
         */
        inval_below(win);
    }
}

/*
 *   invalidate the window from a given offset to the end of the line 
 */
void CHtmlDispTextInput::inval_eol(CHtmlSysWin *win, int textofs)
{
    CHtmlRect r;

    /* start with our own position */
    r = pos_;

    /* advance past the given offset in the text */
    r.left += win->measure_text(font_, txt_, textofs, 0).x;

    /* go all the way to the right */
    r.right = HTMLSYSWIN_MAX_RIGHT;

    /* invalidate the area */
    win->inval_doc_coords(&r);
}

/*
 *   invalidate the window below this item to the bottom of the window
 */
void CHtmlDispTextInput::inval_below(CHtmlSysWin *win)
{
    CHtmlRect r;

    /* start with our own position */
    r = pos_;

    /*
     *   go from my bottom to the bottom of the window, and all the way
     *   across the line below 
     */
    r.left = 0;
    r.right = HTMLSYSWIN_MAX_RIGHT;
    r.top = r.bottom;
    r.bottom = HTMLSYSWIN_MAX_BOTTOM;

    /* invalidate it */
    win->inval_doc_coords(&r);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger source file text display item 
 */

CHtmlDispTextDebugsrc::CHtmlDispTextDebugsrc(class CHtmlSysWin *win,
                                             class CHtmlSysFont *font,
                                             const textchar_t *txt,
                                             size_t len,
                                             unsigned long txtofs,
                                             unsigned long src_linenum)
    : CHtmlDispTextPre(win, font, txt, len, txtofs)
{
    CHtmlPoint iconsize;
    
    /* remember the source file line number */
    line_num_ = src_linenum;

    /* get the size of the icon in the left margin */
    iconsize = win->measure_dbgsrc_icon();

    /* add the width of the icon */
    pos_.right += iconsize.x;

    /* make sure the line is tall enough to contain the icon */
    if (pos_.bottom - pos_.top < iconsize.y)
        pos_.bottom = pos_.top + iconsize.y;

    /* clear the initial state */
    dbgstat_ = 0;
}

/*
 *   draw a debugger source line 
 */
void CHtmlDispTextDebugsrc::draw(CHtmlSysWin *win,
                                 unsigned long sel_start,
                                 unsigned long sel_end, int clicked)
{
    int margin_wid;
    CHtmlRect iconpos;
    CHtmlRect txtpos;

    /* 
     *   our position encompasses both the margin icon and the text;
     *   figure out where the text goes by insetting our left edge by the
     *   size of the margin icon 
     */
    margin_wid = win->measure_dbgsrc_icon().x;
    txtpos = pos_;
    txtpos.left += margin_wid;

    /* the rest is the icon */
    iconpos = pos_;
    iconpos.right = iconpos.left + margin_wid;

    /* draw the appropriate margin icon */
    win->draw_dbgsrc_icon(&iconpos, dbgstat_);

    /* draw the text normally within these bounds */
    draw_text(win, sel_start, sel_end, 0, clicked, &txtpos);
}

/*
 *   get the cursor type 
 */
Html_csrtype_t CHtmlDispTextDebugsrc::get_cursor_type(
    class CHtmlSysWin *win, class CHtmlFormatter *formatter,
    int x, int y, int more_mode) const
{
    /* determine if the cursor is within the margin icon */
    if (x < pos_.left + win->measure_dbgsrc_icon().x)
    {
        /* it's in the margin icon - use an arrow */
        return HTML_CSRTYPE_ARROW;
    }
    else
    {
        /* it's in the text - use the normal text cursor */
        return CHtmlDispTextPre::get_cursor_type(win, formatter, x, y,
                                                 more_mode);
    }
}

/*
 *   Find text given a window position.  We must adjust for the offset
 *   created by the margin icon. 
 */
unsigned long CHtmlDispTextDebugsrc::find_textofs_by_pos(
    CHtmlSysWin *win, CHtmlPoint pos) const
{
    /* adjust the position to account for the icon in the margin */
    pos.x -= win->measure_dbgsrc_icon().x;

    /* now do the calculation normally */
    return CHtmlDispTextPre::find_textofs_by_pos(win, pos);
}

/*
 *   Find the position of a given text offset.  We must adjust for the
 *   offset created by the margin icon. 
 */
CHtmlPoint CHtmlDispTextDebugsrc::get_text_pos(CHtmlSysWin *win,
                                               unsigned long txtofs)
{
    CHtmlPoint pos;
    
    /* get the position using the normal mechanism */
    pos = CHtmlDispTextPre::get_text_pos(win, txtofs);

    /* adjust for the margin icon */
    pos.x += win->measure_dbgsrc_icon().x;

    /* return the result */
    return pos;
}

/*
 *   Update the status 
 */
void CHtmlDispTextDebugsrc::set_dbg_status(CHtmlSysWin *win,
                                           unsigned int newstat)
{
    CHtmlRect iconrc;
    
    /* ignore the request if the status isn't changing */
    if (newstat == dbgstat_)
        return;
    
    /* set my new status */
    dbgstat_ = newstat;

    /* invalidate the icon area so that we redraw with the new status */
    iconrc = pos_;
    iconrc.right = iconrc.left + win->measure_dbgsrc_icon().x;
    win->inval_doc_coords(&iconrc);
}

/*
 *   get my text into a buffer - add a newline at the end 
 */
void CHtmlDispTextDebugsrc::add_text_to_buf(CStringBuf *buf) const
{
    /* inherit default */
    CHtmlDispTextPre::add_text_to_buf(buf);

    /* add a newline */
    buf->append("\n", 1);
}

void CHtmlDispTextDebugsrc::
   add_text_to_buf_clip(CStringBuf *buf, unsigned long startofs,
                        unsigned long endofs) const
{
    /* inherit default */
    CHtmlDispTextPre::add_text_to_buf_clip(buf, startofs, endofs);

    /* if the ending offset is beyond my ending offset, add a newline */
    if (startofs <= txtofs_ + len_ + 1 && endofs > txtofs_ + len_ + 1)
        buf->append("\n", 1);
}

/* ------------------------------------------------------------------------ */
/*
 *   Text Grid row 
 */

/* cell color record */
typedef struct textgrid_cellcolor_t textgrid_cellcolor_t;
struct textgrid_cellcolor_t
{
    /* foreground color */
    HTML_color_t fg;

    /* background color, or 0x01000000 if transparent */
    HTML_color_t bg;
};

/*
 *   construction 
 */
CHtmlDispTextGrid::CHtmlDispTextGrid(CHtmlSysWin *win, CHtmlSysFont *font,
                                     unsigned long ofs)
{
    CHtmlPoint siz;

    /* remember my initial offset */
    ofs_ = ofs;

    /* there's nothing in our buffer yet */
    max_col_ = 0;

    /* set the font */
    set_font(win, font);

    /* allocate an initial set of cell colors */
    colors_max_ = 50;
    colors_ = (textgrid_cellcolor_t *)th_malloc(colors_max_
                                                * sizeof(colors_[0]));
}

/*
 *   deletion 
 */
CHtmlDispTextGrid::~CHtmlDispTextGrid()
{
    /* delete our color list */
    th_free(colors_);
}

/*
 *   Write text into the grid row
 */
int CHtmlDispTextGrid::write_text(CHtmlSysWin *win, CHtmlSysFont *font,
                                  int col, const textchar_t *txt, size_t len)
{
    char *p;
    textgrid_cellcolor_t *cp;
    int i;
    int expanded;
    CHtmlFontDesc desc;

    /* assume we won't have to expand the row */
    expanded = FALSE;

    /* ensure that we have enough space in our string buffer */
    buf_.ensure_size(col + len);

    /* make sure we have enough space for the cell colors */
    if (col + len > colors_max_)
    {
        /* expand a bit over the immediate need, and reallocate */
        colors_max_ = col + len + 50;
        colors_ = (textgrid_cellcolor_t *)th_realloc(
            colors_, colors_max_ * sizeof(colors_[0]));
    }

    /* if we're going past the maximum column so far, pad with spaces */
    for (p = buf_.get() + max_col_, cp = colors_ + max_col_, i = max_col_ ;
         i < col ; ++i, ++p, ++cp)
    {
        /* add a space */
        *p = ' ';

        /* make it black/transparent (using our special transparent flag) */
        cp->fg = HTML_make_color(0x00, 0x00, 0x00);
        cp->bg = 0x01000000;
    }

    /* copy the text */
    memcpy(buf_.get() + col, txt, len);

    /* get the color of the text from the font */
    font->get_font_desc(&desc);

    /* set the colors */
    for (i = col, cp = colors_ + col ; i < col + (int)len ; ++i, ++cp)
    {
        /* set the foreground color */
        cp->fg = desc.color;

        /* set the background color; use our special flag for transparency */
        cp->bg = (desc.default_bgcolor ? 0x01000000 : desc.bgcolor);
    }

    /* adjust the maximum column and size if we've expanded the width */
    if (col + (int)len >= max_col_)
    {
        CHtmlPoint siz;
        
        /* note the new maximum column */
        max_col_ = col + len;

        /* calculate the new size */
        siz = win->measure_text(font_, buf_.get(), max_col_, 0);

        /* adjust our position for the new size */
        pos_.right = pos_.left + siz.x;

        /* note that we had to expand the row */
        expanded = TRUE;
    }

    /* invalidate our affected on-screen area */
    inval_range(win, ofs_ + col, ofs_ + col + len);

    /* return an indication of whether or not we expanded our width */
    return expanded;
}

/*
 *   Set our font 
 */
void CHtmlDispTextGrid::set_font(CHtmlSysWin *win, CHtmlSysFont *font)
{
    CHtmlPoint siz;
    int ascent;
    const textchar_t *txt;

    /* remember my new font */
    font_ = font;

    /* calculate my new size */
    txt = (max_col_ != 0 && buf_.get() != 0 ? buf_.get() : "X");
    siz = win->measure_text(font, txt, strlen(txt), &ascent);

    /* change my position to account for the new size */
    pos_.set(pos_.left, pos_.top, pos_.left + siz.x, pos_.top + siz.y);

    /* remember my new ascent height */
    ascent_ht_ = (unsigned short)ascent;
}

/*
 *   draw the text 
 */
void CHtmlDispTextGrid::draw(CHtmlSysWin *win,
                             unsigned long sel_start, unsigned long sel_end,
                             int /*clicked*/)
{
    /* draw the part up to the start of the selection */
    draw_part(win, FALSE, ofs_, sel_start);

    /* draw the selected section */
    draw_part(win, TRUE, sel_start, sel_end);

    /* draw the part after the end of the selection */
    draw_part(win, FALSE, sel_end, ofs_ + max_col_);
}

/* draw a text section, limiting to our range */
void CHtmlDispTextGrid::draw_part(CHtmlSysWin *win, int hilite,
                                  unsigned long l, unsigned long r)
{
    /* limit the start and end positions to our range */
    if (l < ofs_)
        l = ofs_;
    if (r > ofs_ + max_col_)
        r = ofs_ + max_col_;

    /* if that leaves anything to draw, draw it */
    if (l < r)
    {
        size_t col;
        size_t len;
        int x;
        
        /* get the starting column and length to draw */
        col = l - ofs_;
        len = r - l;

        /* figure out where the drawing starts */
        x = pos_.left;
        x += win->measure_text(font_, buf_.get(), col, 0).x;

        /* draw in pieces of like color */
        for (;;)
        {
            size_t ofs;
            CHtmlSysFont *font;
            CHtmlFontDesc desc;
            textgrid_cellcolor_t *cp;

            /* get the color of the first character to draw */
            cp = &colors_[col];
            
            /* find the next cell with a different color */
            for (ofs = 1 ; ofs < len ; ++ofs)
            {
                /* if the color differs, stop scanning */
                if (colors_[col + ofs].fg != cp->fg
                    || colors_[col + ofs].bg != cp->bg)
                    break;
            }

            /* 
             *   set up a descriptor for our font in the chunk's color - use
             *   the base font, but substitute the colors defined in the
             *   chunk 
             */
            font_->get_font_desc(&desc);
            desc.default_color = FALSE;
            desc.color = cp->fg;
            desc.default_bgcolor = (cp->bg == 0x01000000);
            desc.bgcolor = cp->bg;

            /* get the font for the chunk */
            font = win->get_font(&desc);

            /* draw the text */
            win->draw_text(hilite, x, pos_.top, font, buf_.get() + col, ofs);

            /* if that was everything, stop looping */
            if (ofs == len)
                break;

            /* move horizontally past the chunk's text size */
            x += win->measure_text(font_, buf_.get() + col, ofs, 0).x;

            /* skip this chunk */
            col += ofs;
            len -= ofs;
        }
    }
}

/* 
 *   invalidate a text range 
 */
void CHtmlDispTextGrid::inval_range(CHtmlSysWin *win,
                                    unsigned long start_ofs,
                                    unsigned long end_ofs)
{
    CHtmlRect inval;
    size_t col;
    size_t len;

    /* make sure the range overlaps us */
    if (start_ofs >= ofs_ + max_col_ || end_ofs < ofs_)
        return;

    /* limit the range to our actual range */
    if (start_ofs < ofs_)
        start_ofs = ofs_;
    if (end_ofs > ofs_ + max_col_)
        end_ofs = ofs_ + max_col_;

    /* calculate the starting column and number of columns in the range */
    col = (size_t)(start_ofs - ofs_);
    len = (size_t)(end_ofs - start_ofs);

    /* start with our full rectangle */
    inval = pos_;

    /* reduce the area to the desired range */
    inval.left += win->measure_text(font_, buf_.get(), col, 0).x;
    inval.right = inval.left
                  + win->measure_text(font_, buf_.get() + col, len, 0).x;

    /* invalidate our window area */
    win->inval_doc_coords(&inval);
}

/* 
 *   get the position of a character within the item 
 */
CHtmlPoint CHtmlDispTextGrid::get_text_pos(CHtmlSysWin *win,
                                           unsigned long txtofs)
{
    CHtmlPoint pt;

    /* 
     *   if the offset is before us, or we have no text, return our upper
     *   left corner 
     */
    if (txtofs < ofs_ || buf_.get() == 0)
        return CHtmlPoint(pos_.left, pos_.top);

    /* if the offset is after us, return our upper right corner */
    if (txtofs >= ofs_ + max_col_)
        return CHtmlPoint(pos_.right, pos_.top);

    /* start with our upper left corner */
    pt.x = pos_.left;
    pt.y = pos_.top;

    /* add the size of our text to the desired offset */
    pt.x += win->measure_text(font_, buf_.get(), txtofs - ofs_, 0).x;

    /* return the position */
    return pt;
}

/* 
 *   get the text offset of a character given a position 
 */
unsigned long CHtmlDispTextGrid::find_textofs_by_pos(
    CHtmlSysWin *win, CHtmlPoint pt) const
{
    size_t fit_cnt;
    textchar_t *txt = buf_.get();
    
    /* if it doesn't contain us, use an offset just before or after us */
    if (!pos_.contains(pt) || txt == 0)
    {
        /* see whether it's above/left or below/right */
        if (pt.y <= pos_.top || pt.x <= pos_.left)
            return ofs_;
        else
            return ofs_ + max_col_;
    }

    /* figure out how much of our text is before the given position */
    fit_cnt = win->get_max_chars_in_width(font_, txt, max_col_,
                                          pt.x - pos_.left);

    /* if we're over halfway into the next character, skip ahead one */
    if (fit_cnt + 1 <= (size_t)max_col_
        && pt.x > (pos_.left
                   + win->measure_text(font_, txt, fit_cnt, 0).x
                   + win->measure_text(font_, txt + fit_cnt, 1, 0).x / 2))
        ++fit_cnt;

    /* return the text offset of the selected character */
    return ofs_ + fit_cnt;
}

/* ------------------------------------------------------------------------ */
/*
 *   Special text item base class
 */

/* 
 *   invalidate range 
 */
void CHtmlDispTextSpecial::inval_range(CHtmlSysWin *win,
                                       unsigned long start_ofs,
                                       unsigned long end_ofs)
{
    /* if the range is outside of my text, do nothing */
    if (end_ofs <= txtofs_ || start_ofs >= txtofs_ + len_)
        return;

    /* 
     *   Simply invalidate my entire area.  Don't try to limit the area to
     *   the given range, as the normal text item does, since the special
     *   text item types don't necessarily map their display area directly to
     *   their nominal underlying text.  
     */
    win->inval_doc_coords(&pos_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Soft Hyphen 
 */

/* 
 *   Find a line break.  If a hyphen character will fit in the available
 *   space, we'll note this as a possible breaking point.  
 */
CHtmlDisp *CHtmlDispSoftHyphen::find_line_break(CHtmlFormatter *formatter,
                                                CHtmlSysWin *win,
                                                long space_avail,
                                                CHtmlLineBreak *break_pos)
{
    /* check to see if we have space to add a hyphen character */
    if (win->measure_text(font_, "-", 1, 0).x <= space_avail)
    {
        /* 
         *   our added hyphen will fit; remember us as a possible breaking
         *   point, in case we need to break here later 
         */
        break_pos->set_break_pos(this, 0);
    }

    /* 
     *   don't actually break here; we'll only break here if we can't find a
     *   better breaking point later 
     */
    return 0;
}

/*
 *   Do a line break at this item.  
 */
CHtmlDisp *CHtmlDispSoftHyphen::do_line_break(CHtmlFormatter *formatter,
                                              CHtmlSysWin *win,
                                              CHtmlLineBreak *break_pos)
{
    CHtmlPoint siz;
    
    /* 
     *   we're taking the option of breaking here, so actually display as a
     *   hyphen from now on 
     */
    vis_ = TRUE;

    /* set our width to the hyphen's width */
    siz = win->measure_text(font_, "-", 1, 0);
    pos_.right = pos_.left + siz.x;
    pos_.bottom = pos_.top + siz.y;

    /* 
     *   break after the hyphen; so, the start of the next line is the next
     *   item after us 
     */
    return get_next_disp();
}

/*
 *   Draw our text 
 */
void CHtmlDispSoftHyphen::draw_text(CHtmlSysWin *win,
                                    unsigned long sel_start,
                                    unsigned long sel_end,
                                    class CHtmlDispLink *link, int clicked,
                                    const CHtmlRect *orig_pos)
{
    /* if we're showing as a hyphen, draw the hyphen */
    if (vis_)
    {
        CHtmlSysFont *font;

        /* get the font, adjusting for the link appearance if necessary */
        font = get_draw_text_font(win, link, clicked);
        
        /*  
         *   draw our text as normal, except never draw highlighting; we're
         *   not really a part of the text, so we can't be selected 
         */
        win->draw_text(FALSE, pos_.left, pos_.top, font, "-", 1);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Non-breaking space
 */

/* 
 *   Find a line break.  A non-breaking space obviously can't be a line
 *   breaking point.  Furthermore, if we're a zero-width space, we prevent a
 *   line break from occurring immediately to our right, even if a break were
 *   otherwise allowed there.  
 */
CHtmlDisp *CHtmlDispNBSP::find_line_break(CHtmlFormatter *formatter,
                                          CHtmlSysWin *win,
                                          long space_avail,
                                          CHtmlLineBreak *break_pos)
{
    /* 
     *   for line-breaking purposes, we don't count as a space or any other
     *   breakable character, so clear out the previous-character indicator
     *   in the break state 
     */
    break_pos->last_char_ = 0;

    /* 
     *   If we're a zero-width space, we explicitly prevent a break from
     *   occurring to our right, even if such a break were ordinarily
     *   allowed; flag this with the explicit no-break flag in the break
     *   state.
     */
    break_pos->no_break_ = (len_ == 0);

    /* we don't count as ordinary whitespace */
    break_pos->note_non_ws();

    /* 
     *   don't actually break here; we'll only break here if we can't find a
     *   better breaking point later 
     */
    return 0;
}

/*
 *   measure for a table 
 */
void CHtmlDispNBSP::measure_for_table(class CHtmlSysWin *win,
                                      class CHtmlTableMetrics *metrics)
{
    int mywid = measure_width(win);

    /* 
     *   if breaking before is allowed, and I'm not zero-width (in which
     *   case I'm actually a no-break flag), clear the leftover 
     */
    if (len_ != 0 && !metrics->no_break_ && !metrics->no_break_sect_)
        metrics->clear_leftover();

    /* add our width to the running leftover */
    metrics->leftover_width_ += mywid;

    /* disallow breaking just after us if we're zero-width */
    metrics->no_break_ = (len_ == 0);

    /* we're not an ordinary whitespace character in any case */
    metrics->last_char_ = 0;
}

/*
 *   Find trailing whitespace.
 */
void CHtmlDispNBSP::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    /* we do not count as ordinary trimmable whitespace */
    info->clear_ws_pos();
}

/* ------------------------------------------------------------------------ */
/*
 *   Typographical space
 */

CHtmlDispSpace::CHtmlDispSpace(CHtmlSysWin *win, CHtmlSysFont *font,
                               const textchar_t *txt, size_t len,
                               unsigned long txtofs, int wid)
    : CHtmlDispTextSpecial(win, font, txt, len, txtofs)
{
    CHtmlPoint siz;
    CHtmlFontDesc desc;

    /* 
     *   Check to see if we're in a monospaced font; if so, then operate in
     *   whole space units.  For a proportional font, use proportional
     *   widths.  
     */
    if (font->is_fixed_pitch())
    {
        const textchar_t *sp;
        
        /*
         *   We have a fixed-pitch font.  Check the desired width and adjust
         *   to whole spaces. 
         */
        switch(wid)
        {
        case HTMLTAG_SPWID_ORDINARY:
        case HTMLTAG_SPWID_FIGURE:
            /* use one space for these */
            sp = " ";
            break;

        case 0:
            /* use zero for a zero-width space */
            sp = "";
            break;

            /* 
             *   For anything else, round to ordinary spaces, which we'll
             *   fix at about a third of an em.  For an em space, use three
             *   ordinary spaces; for an en, use two; a three-per-em is 2/3
             *   en or 2 ordinary; for anything smaller (i.e., higher
             *   denominator), use a single ordinary space.  Never round
             *   down to zero, as we want every non-zero-width space to show
             *   up as at least one space.  
             */
        case 1:
            /* em space - use three ordinary spaces */
            sp = "   ";
            break;

        case 2:
        case 3:
            /* en space/three-per-em - use two ordinary spaces */
            sp = "  ";
            break;

        case 8:
        case HTMLTAG_SPWID_PUNCT:
            /* punctuation/hair spaces - use zero width for these */
            sp = "";
            break;

        default:
            /* for anything smaller, use a single space */
            sp = " ";
            break;
        }

        /* use the width of the number of ordinary spaces we figured */
        siz = win->measure_text(font, sp, get_strlen(sp), 0);
        pos_.set(0, 0, siz.x, siz.y);
    }
    else
    {
        /* get our correct proportional width */
        switch(wid)
        {
        case HTMLTAG_SPWID_ORDINARY:
            /* 
             *   just use the width of a space - since our nominal text is
             *   also just an ordinary space, this is the width we've
             *   already calculated in the constructor, so we have nothing
             *   more to do 
             */
            break;
            
        case 0:
            /* 
             *   we have zero width, which is what we've already calculated
             *   in the constructor (since our nominal text is an empty
             *   string) 
             */
            break;
            
        case HTMLTAG_SPWID_FIGURE:
            /* calculate the width of a digit '0' */
            siz = win->measure_text(font, "0", 1, 0);
            pos_.set(0, 0, siz.x, siz.y);
            break;
            
        case HTMLTAG_SPWID_PUNCT:
            /* calculate the size of a period */
            siz = win->measure_text(font, ".", 1, 0);
            pos_.set(0, 0, siz.x, siz.y);
            break;
            
        default:
            /* 
             *   Anything else is the denominator of the fraction of an em
             *   space.  Get the em space size from the font, and use the
             *   desired fraction.  
             */
            pos_.right = pos_.left + font->get_em_size()/wid;
            break;
        }
    }
}

/* 
 *   Find a line break.  A typographical space can be a breaking point, as
 *   long as it's not bordered on both sides by a no-break flag.
 */
CHtmlDisp *CHtmlDispSpace::find_line_break(CHtmlFormatter *formatter,
                                           CHtmlSysWin *win,
                                           long space_avail,
                                           CHtmlLineBreak *break_pos)
{
    /* we count as ordinary whitespace for trimming purposes */
    break_pos->note_ws(this, 0);

    /*
     *   Never actually break at a space, because we can always leave spaces
     *   hanging past the right edge.  Simply remember this as a valid break
     *   position, as long as we don't have an explicit no-break flag
     *   preceding us.  
     */
    if (!break_pos->no_break_)
    {
        /* note that we're a valid breaking point */
        break_pos->set_break_pos(this, 0);
    }

    /*
     *   Clear out any explicit no-break flag, as we do allow a break to our
     *   right, and note that the last character for line-breaking purposes
     *   is an ordinary space.  
     */
    break_pos->no_break_ = FALSE;
    break_pos->last_char_ = ' ';

    /* do not break here */
    return 0;
}

/*
 *   Do a line break at this item.  
 */
CHtmlDisp *CHtmlDispSpace::do_line_break(CHtmlFormatter *formatter,
                                         CHtmlSysWin *win,
                                         CHtmlLineBreak *break_pos)
{
    /*
     *   Since we're pure whitespace, we don't want to start a line after a
     *   soft line break with this item.  Instead, simply ask the next item
     *   to skip any leading whitespace and find the next line start.
     *   
     *   This means we're going at the end of a line, which means that we're
     *   trailing space, which means we shouldn't be visible; so set my
     *   width to zero.  
     */
//$$$    pos_.right = pos_.left;

    /* skip any subsequent whitespace as well */
    if (get_next_disp() != 0)
        return get_next_disp()->do_line_break_skipsp(formatter, win);
    else
        return 0;
}

/*
 *   Draw our text 
 */
void CHtmlDispSpace::draw_text(CHtmlSysWin *win,
                               unsigned long sel_start, unsigned long sel_end,
                               class CHtmlDispLink *link, int clicked,
                               const CHtmlRect *orig_pos)
{
    CHtmlSysFont *font;

    /* do nothing if we're zero-width or we're entirely hidden */
    if (pos_.right == pos_.left || displen_ == 0)
        return;

    /* get the font, adjusting for the link appearance if necessary */
    font = get_draw_text_font(win, link, clicked);

    /* 
     *   Draw our text as a space of a particular pixel width.
     *   
     *   Since we're always exactly one character long, we're always either
     *   entirely highlighted or entirely unhighlighted - it's not possible
     *   for a one-character block of text to be partially highlighted.  So,
     *   simply draw our text in the appropriate highlighting mode.  
     */
    win->draw_text_space(txtofs_ >= sel_start && txtofs_ + len_ <= sel_end,
                         pos_.left, pos_.top, font, pos_.right - pos_.left);
}

/*
 *   Find trailing whitespace.
 */
void CHtmlDispSpace::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    /* we count as whitespace, so note our position */
    info->set_ws_pos(this, 0);
}

/*
 *   Remove trailing whitespace from a position we previously found.  
 */
void CHtmlDispSpace::remove_trailing_whitespace(CHtmlSysWin *, void *)
{
    /* 
     *   we're all whitespace, so disappear entirely - set our length and
     *   width to zero 
     */
    pos_.right = pos_.left;
    len_ = 0;
}

/*
 *   Hide trailing whitespace from a position we previously found.  
 */
void CHtmlDispSpace::hide_trailing_whitespace(CHtmlSysWin *, void *)
{
    /* 
     *   we're all whitespace, so disappear entirely - set our display
     *   length to zero 
     */
    displen_ = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   line break 
 */

void CHtmlDispBreak::measure_for_table(CHtmlSysWin *,
                                       CHtmlTableMetrics *metrics)
{
    /* start a new line in the metrics */
    metrics->start_new_line();
}

/* ------------------------------------------------------------------------ */
/*
 *   horizontal rule object 
 */

CHtmlDispHR::CHtmlDispHR(CHtmlSysWin *win, int shade, long height,
                         long width, int width_is_pct,
                         long margin_left, long margin_right,
                         CHtmlResCacheObject *image)
    : CHtmlDispDisplaySite(win, &pos_, image)
{
    /* figure out how big it is in absolute terms */
    if (width_is_pct)
    {
        /* take the specified percentage of the window's width */
        width_ = (long)((double)(win->get_disp_width()
                                 - margin_right - margin_left)
                        * (double)width / 100.0);
    }
    else
    {
        /* the width specified is in pixels */
        width_ = width;
    }

    /* note whether the width is relative */
    width_is_pct_ = (width_is_pct != 0);

    /* store the other attributes */
    shade_ = shade;
    height_ = height;

    /* remember the image */
    image_ = image;

    /* see if we have an image */
    if (image_ != 0)
    {
        /* add a reference to the image as long as we have it */
        image_->add_ref();
    }

    /* see if the height was specified */
    if (height == 0)
    {
        /* 
         *   if we have an image, use it's height; otherwise, use a
         *   default based on whether we have shading or not 
         */
        if (image_ != 0 && image_->get_image() != 0)
        {
            /* use the image's height as the default height */
            height_ = image_->get_image()->get_height();
        }
        else if (shade_)
        {
            /* shading - need two pixels for this */
            height_ = 2;
        }
        else
        {
            /* unshaded - use only a single pixel */
            height_ = 1;
        }
    }
    
    /* if it's shaded and there's no image, minimum height is 2 */
    if (shade_ && image_ == 0 && height < 2)
        height_ = 2;

    /* set my initial position */
    pos_.set(0, 0, width_, height_);
}

CHtmlDispHR::~CHtmlDispHR()
{
    /* we're no longer going to reference the image object */
    if (image_ != 0)
        image_->remove_ref();
}

void CHtmlDispHR::draw(CHtmlSysWin *win, unsigned long /*sel_start*/,
                       unsigned long /*sel_end*/, int /*clicked*/)
{
    /* if we have an image, draw the image */
    if (image_ != 0 && image_->get_image() != 0)
    {
        /* tile the image into the drawing area */
        image_->get_image()->draw_image(win, &pos_, HTMLIMG_DRAW_TILE);
    }
    else
    {
        /* tell the window to draw a standard hrule */
        win->draw_hrule(&pos_, shade_);
    }
}

/*
 *   measure for a table 
 */
void CHtmlDispHR::measure_for_table(CHtmlSysWin *win,
                                    CHtmlTableMetrics *metrics)
{
    /*
     *   If the rule's size is relative, it makes no contribution to the
     *   table metrics, since it can resize to any size according to the
     *   table's layout.  However, if the size is absolute, add it to the
     *   line width and indivisible length as we would any other object. 
     */
    if (!width_is_pct_)
    {
        int mywid = measure_width(win);
        metrics->add_indivisible(mywid);
        metrics->add_to_cur_line(mywid);
    }

    /* no leftover width from this item */
    metrics->clear_leftover();

    /* allow breaking after the rule */
    metrics->last_char_ = ' ';
}

/*
 *   Find trailing whitespace.  We count as text, so any previous
 *   whitespace is not trailing.  
 */
void CHtmlDispHR::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    info->clear_ws_pos();
}


/* ------------------------------------------------------------------------ */
/*
 *   List item header display items 
 */

CHtmlDispListitem::CHtmlDispListitem(CHtmlSysWin *win, CHtmlSysFont *font,
                                     long width)
{
    int ascent;
    
    /* remember the font */
    font_ = font;

    /* set my initial position based on our width and the font height */
    pos_.set(0, 0, width, win->measure_text(font, " ", 1, &ascent).y);
    ascent_ht_ = (unsigned short)ascent;
}

void CHtmlDispListitem::get_vertical_space(CHtmlSysWin *,
                                           int /*text_height*/,
                                           int *above_base, int *below_base,
                                           int *total)
{
    /* calculate my vertical space as though I contained some text */
    *total = (pos_.bottom - pos_.top);
    *above_base = ascent_ht_;
    *below_base = *total - *above_base;
}

void CHtmlDispListitem::set_line_pos(CHtmlSysWin *, int left, int top,
                                     int /*total_height*/,
                                     int text_base_offset,
                                     int /*max_text_height*/)
{
    /*
     *   We want to be aligned such that our text baseline matches the
     *   text baselines of other text items on the line, so set my top to
     *   my text height above the text baseline.  
     */
    pos_.offset(left - pos_.left,
                top - pos_.top + text_base_offset - ascent_ht_);
}

/* ------------------------------------------------------------------------ */
/*
 *   set up a bulleted list item header 
 */
CHtmlDispListitemBullet::CHtmlDispListitemBullet
   (CHtmlSysWin *win, CHtmlSysFont *font, long width, HTML_Attrib_id_t type,
    CHtmlResCacheObject *image)
    : CHtmlDispListitem(win, font, width),
      CHtmlDispDisplaySite(win, &pos_, image)
{
    /* note our image */
    image_ = image;
    if (image != 0)
    {
        /* add a reference, since we'll keep the pointer */
        image->add_ref();

        /* make sure we're big enough to show the entire image */
        if (image->get_image() != 0)
        {
            long image_wid = image->get_image()->get_width();
            long image_ht = image->get_image()->get_height();

            /* make sure we're wide enough */
            if (pos_.right - pos_.left < image_wid)
                pos_.right = pos_.left + image_wid;

            /* make sure we're tall enough */
            if (pos_.bottom - pos_.top < image_ht)
            {
                pos_.bottom = pos_.top + image_ht;
                ascent_ht_ = (unsigned short)image_ht;
            }
        }
    }

    /* see what type of bullet we're using */
    switch(type)
    {
    default:
    case HTML_Attrib_disc:
        type_ = HTML_SYSWIN_BULLET_DISC;
        break;

    case HTML_Attrib_circle:
        type_ = HTML_SYSWIN_BULLET_CIRCLE;
        break;

    case HTML_Attrib_square:
        type_ = HTML_SYSWIN_BULLET_SQUARE;
        break;

    case HTML_Attrib_plain:
        type_ = HTML_SYSWIN_BULLET_PLAIN;
        break;
    }
}

CHtmlDispListitemBullet::~CHtmlDispListitemBullet()
{
    /* if we have an image, remove our reference to it */
    if (image_ != 0)
        image_->remove_ref();
}

void CHtmlDispListitemBullet::get_vertical_space(CHtmlSysWin *win,
    int text_height, int *above_base, int *below_base, int *total)
{
    /* 
     *   if we have an image to display, calculate the space needs as we
     *   would for an image; otherwise, use the default handling 
     */
    if (image_ != 0 && image_->get_image() != 0)
    {
        /* center the image vertically */
        *total = pos_.bottom - pos_.top;
        *above_base = (text_height + *total) / 2;
        *below_base = *total - *above_base;
    }
    else
    {
        /* no image - use default handling */
        CHtmlDispListitem::get_vertical_space(win, text_height, above_base,
                                              below_base, total);
    }
}

void CHtmlDispListitemBullet::set_line_pos(CHtmlSysWin *win,
                                           int left, int top,
                                           int total_height,
                                           int text_base_offset,
                                           int max_text_height)
{
    /* 
     *   if we have an image, center the image vertically relative to the
     *   text area; otherwise, use the default list item handling 
     */
    if (image_ != 0 && image_->get_image() != 0)
    {
        long y_offset;

        /* calculate position as we would a vertically centered image */
        y_offset = (max_text_height + pos_.bottom - pos_.top) / 2;
        pos_.offset(left - pos_.left,
                    top - pos_.top + text_base_offset - y_offset);
    }
    else
    {
        CHtmlDispListitem::set_line_pos(win, left, top, total_height,
                                        text_base_offset, max_text_height);
    }
}

/*
 *   Draw a bulleted list item header 
 */
void CHtmlDispListitemBullet::draw(CHtmlSysWin *win,
                                   unsigned long /*sel_start*/,
                                   unsigned long /*sel_end*/,
                                   int /*clicked*/)
{
    /* see if we have an image */
    if (image_ != 0 && image_->get_image() != 0)
    {
        long image_wid, image_ht;
        long pos_wid, pos_ht;
        CHtmlRect drawpos;
        
        /* center the image in our area */
        pos_wid = pos_.right - pos_.left;
        pos_ht = pos_.bottom - pos_.top;
        image_wid = image_->get_image()->get_width();
        image_ht = image_->get_image()->get_height();
        drawpos.left = pos_.left + (pos_wid - image_wid)/2;
        drawpos.right = drawpos.left + image_wid;
        drawpos.top = pos_.top + (pos_ht - image_ht)/2;
        drawpos.bottom = drawpos.top + image_ht;
        
        /* draw the image, clipping to the drawing area */
        image_->get_image()->draw_image(win, &drawpos, HTMLIMG_DRAW_CLIP);
    }
    else
    {
        /* no image - draw a bullet of the appropriate type */
        win->draw_bullet(FALSE, pos_.left, pos_.top, font_, type_);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Set up a numbered list item header 
 */
CHtmlDispListitemNumber::CHtmlDispListitemNumber(
    CHtmlSysWin *win, CHtmlSysFont *font, long width,
    textchar_t style, long value)
    : CHtmlDispListitem(win, font, width)
{
    const int bufmax = 128;
    textchar_t buf[bufmax];
    textchar_t *p;

    /* prepare to write to our buffer in reverse order */
    p = buf + bufmax;

    /* null-terminate the value and end it with a period */
    *--p = '\0';
    *--p = '.';

    /* generate format appropriate for the style */
    switch(style)
    {
    default:
    case '1':
        /* if the value is less than zero, generate leading minus sign */
        if (value < 0)
        {
            value = -value;
            *--p = '-';
        }

        /* format the header as a number */
        do
        {
            *--p = int_to_ascii_digit(value % 10);
            value /= 10;
        } while (p > buf && value != 0);
        break;

    case 'A':
    case 'a':
        /* ignore nonpositive values */
        if (value <= 0)
        {
            *--p = '?';
            break;
        }
        
        /* format the header as a letter sequence */
        do
        {
            --value;
            *--p = (textchar_t)((value % 26) + style);
            value /= 26;
        } while (p > buf && value != 0);
        break;
        
    case 'i':
    case 'I':
        {
            static struct
            {
                const textchar_t *lc;
                const textchar_t *uc;
                int val;
            }
            *cur, mapping[] =
            {
                { "m", "M", 1000 },
                { "cm", "CM", 900 },
                { "d", "D", 500 },
                { "cd", "CD", 400 },
                { "c", "C", 100 },
                { "xc", "XC", 90 },
                { "l", "L", 50 },
                { "xl", "XL", 40 },
                { "x", "X", 10 },
                { "ix", "IX", 9 },
                { "v", "V", 5 },
                { "iv", "IV", 4 },
                { "i", "I", 1 }
            };

            /* don't allow non-positive values or values over 4999 */
            if (value <= 0 || value >= 5000)
            {
                *--p = '?';
                break;
            }

            /* do roman numbers in forward order */
            p = buf;

            /* 
             *   Map the number.  For each item in our table, add that
             *   item as many times as we can, subtracting its value each
             *   time we add it.  When the value is too small to add any
             *   more of that value, move on to the next value.  Repeat
             *   until the value has been entirely consumed.  
             */
            for (cur = mapping ; value > 0 ; ++cur)
            {
                /* add the current item as many times as it will fit */
                while (value >= cur->val)
                {
                    const textchar_t *src;
                    
                    /* add this value to the string */
                    for (src = (style == 'i' ? cur->lc : cur->uc) ; *src ;
                         *p++ = *src++) ;

                    /* reduce the value by the amount expressed this time */
                    value -= cur->val;
                }
            }

            /* since we went forward, point p back to the buffer start */
            *p++ = '.';
            *p = '\0';
            p = buf;
        }
        break;
    }

    /* set my display string to the string we generated */
    num_.set(p);
}

/*
 *   Set line position for a bulleted list item header
 */
void CHtmlDispListitemNumber::set_line_pos(CHtmlSysWin *win,
                                           int left, int top,
                                           int total_height,
                                           int text_base_offset,
                                           int max_text_height)
{
    long text_len;
    
    /* first, set up at default position */
    CHtmlDispListitem::set_line_pos(win, left, top, total_height,
                                    text_base_offset, max_text_height);

    /* 
     *   now, set my position so that my text is right-justified against
     *   the right edge we just calculated 
     */
    text_len = win->measure_text(font_, num_.get(),
                                 get_strlen(num_.get()), 0).x
               + win->measure_text(font_, "  ", 2, 0).x;
    pos_.left = pos_.right - text_len;
}

/*
 *   Draw a numbered list item header 
 */
void CHtmlDispListitemNumber::draw(CHtmlSysWin *win,
                                   unsigned long /*sel_start*/,
                                   unsigned long /*sel_end*/,
                                   int /*clicked*/)
{
    /* draw our number as ordinary text */
    win->draw_text(FALSE, pos_.left, pos_.top, font_,
                   num_.get(), get_strlen(num_.get()));
}

/* ------------------------------------------------------------------------ */
/*
 *   Image display item 
 */

CHtmlDispImg::CHtmlDispImg(CHtmlSysWin *win,
                           CHtmlResCacheObject *image, CStringBuf *alt,
                           HTML_Attrib_id_t align, long hspace, long vspace,
                           long width, int width_set,
                           long height, int height_set,
                           long border, class CHtmlUrl *usemap, int ismap)
    : CHtmlDispDisplaySite(win, &pos_, image)
{
    /* remember the settings */
    image_ = image;
    alt_.set(alt);
    align_ = align;
    hspace_ = hspace;
    vspace_ = vspace;
    width_ = width;
    height_ = height;
    border_ = border;
    usemap_.set_url(usemap);
    ismap_ = ismap;

    /* if we have an image, set it up */
    if (image_ != 0)
    {
        /* count our reference to the image as long as we're around */
        image_->add_ref();

        /* if we successfully loaded the image, get its size */
        if (image_->get_image() != 0)
        {
            /* 
             *   Get the size from the image.  If either dimension was
             *   explicitly set, though, keep the explicit setting. 
             */
            if (!width_set)
            {
                /* get the width from the image */
                width_ = image_->get_image()->get_width();
            }
            if (!height_set)
            {
                /* get the height from the image */
                height_ = image_->get_image()->get_height();
            }

            /*
             *   If one of the dimensions was explicitly set but the other
             *   one wasn't, scale the unset dimension so that the picture
             *   stays at its original aspect ratio. 
             */
            if (width_set && !height_set)
            {
                /* scale the height proportionally to the width */
                height_ = (long)((double)height_
                                 * (double)width_
                                 / (double)image_->get_image()->get_width());
            }
            else if (height_set && !width_set)
            {
                /* scale the width proportionally to the height */
                width_ = (long)((double)width_
                                * (double)height_
                                / (double)image_->get_image()->get_height());
            }
        }
    }

    /* set our position based on the final size */
    pos_.set(0, 0, width_, height_);
}

/*
 *   delete 
 */
CHtmlDispImg::~CHtmlDispImg()
{
    /* we're no longer going to reference the image object */
    if (image_ != 0)
        image_->remove_ref();
}

/*
 *   Forget an image.  This must be called if we wish to change one of our
 *   internal images dynamically.  
 */
void CHtmlDispImg::forget_image(CHtmlResCacheObject **image)
{
    /* if there's no existing image, there's nothing to do */
    if (*image == 0)
        return;

    /* 
     *   if this is our current display site image, we no longer have any
     *   image in our display site 
     */
    if (get_site_image() == *image)
        set_site_image(0);

    /* drop our reference to the image */
    (*image)->remove_ref();

    /* forget the image */
    *image = 0;
}

/*
 *   Set the extra "hover" and "active" sub-images.  Ordinary image display
 *   items don't allow these sub-images; we'll simply log an error to the
 *   debug log mentioning the problem.  
 */
void CHtmlDispImg::set_extra_images(CHtmlResCacheObject *a_image,
                                    CHtmlResCacheObject *h_image)
{
    /* log an error for the ASRC image, if one was specified */
    if (a_image != 0)
        oshtml_dbg_printf("<IMG>: ASRC is not allowed except in "
                          "hyperlinked images\n");

    /* log an error for the HSRC image, if one was specified */
    if (h_image != 0)
        oshtml_dbg_printf("<IMG>: HSRC is not allowed except in "
                          "hyperlinked images\n");
}

/*
 *   get the width of the picture 
 */
int CHtmlDispImg::measure_width(CHtmlSysWin *)
{
    /* 
     *   return the image width plus twice the size of the horizontal
     *   whitespace (since we draw it on both sides) plus twice the size
     *   of the border (since we draw that on both sides, as well) 
     */
    return width_ + 2*hspace_ + 2*border_;
}

/*
 *   draw the picture 
 */
void CHtmlDispImg::draw(CHtmlSysWin *win,
                        unsigned long /*sel_start*/,
                        unsigned long /*sel_end*/,
                        int /*clicked*/)
{
    /* draw the normal image */
    draw_image(win, image_);
}

/*
 *   draw the picture, using a particular image 
 */
void CHtmlDispImg::draw_image(CHtmlSysWin *win, CHtmlResCacheObject *image)
{
    CHtmlRect drawpos;

    /* 
     *   calculate where to draw the image - offset by border size and
     *   surrounding whitespace sizes, and set the width and height to our
     *   image width and height (to exclude surrounding whitespace from
     *   the image area) 
     */
    drawpos = pos_;
    drawpos.offset(border_ + hspace_, border_ + vspace_);
    drawpos.right = drawpos.left + width_;
    drawpos.bottom = drawpos.top + height_;

    /* draw it */
    if (image == 0 || image->get_image() == 0)
    {
        /* invalid image - draw a placeholder box instead */
        win->draw_hrule(&drawpos, TRUE);
    }
    else
    {
        /* draw the image, scaling to fill the target rectangle */
        image->get_image()->draw_image(win, &drawpos, HTMLIMG_DRAW_STRETCH);
    }
}

/*
 *   get my overall vertical space needs 
 */
void CHtmlDispImg::get_vertical_space(CHtmlSysWin *,
                                      int text_height, int *above_base,
                                      int *below_base, int *total)
{
    /* our total space is always the same */
    *total = measure_height();

    /* our arrangement of the space depends on our alignment */

    switch(align_)
    {
    case HTML_Attrib_top:
        /*
         *   We want to be aligned with the top of the text, so we need
         *   the maximum text height above the baseline.  Add extra for
         *   the top spacing (the vertical whitespace above the image and
         *   the top border), since we want the top of the image to align
         *   with the text.  
         */
        *above_base = text_height + vspace_ + border_;
        break;

    case HTML_Attrib_middle:
        /*
         *   we're centered relative to the text, so half of my height
         *   above the the text center point, and the text center point is
         *   half of the text height above the text baseline, so we need
         *   the sum of these two is the total we need above the text
         *   baseline 
         */
        *above_base = (text_height + *total) / 2;
        break;

    case HTML_Attrib_bottom:
        /*
         *   We're aligned at the bottom of the text, so we need our
         *   entire height above the text baseline.  Note that we want the
         *   image itself aligned at the bottom, so subtract off the
         *   bottom spacing (the vertical whitespace and border below the
         *   image).  
         */
        *above_base = *total - vspace_ - border_;
        break;

    default:
        /* other cases should never occur; ignore them if they do */
        break;
    }

    /* the amount needed below the baseline is whatever's left over */
    *below_base = *total - *above_base;
}

/*
 *   set my line position 
 */
void CHtmlDispImg::set_line_pos(CHtmlSysWin *, int left, int top,
                                int /*total_height*/,
                                int text_base_offset,
                                int max_text_height)
{
    int y_offset;

    /* our positioning depends on our alignment */
    switch(align_)
    {
    case HTML_Attrib_top:
        /*
         *   Align at the top of the text - put our top above the text
         *   baseline by the max text height, which means that our top is
         *   offset from the line top by the text baseline offset minus
         *   the text height.  Note that we want the top of the image
         *   itself to align with the top of the text, so offset by the
         *   top spacing (the vertical whitespace above the image and the
         *   top border).  
         */
        y_offset = max_text_height + vspace_ + border_;
        break;

    case HTML_Attrib_middle:
        /*
         *   We're centered relative to the text, so half of my height
         *   above the the text center point, and the text center point is
         *   half of the text height above the text baseline, so the sum
         *   of these two is the total we need above the text baseline.  
         */
        y_offset = (max_text_height + measure_height()) / 2;
        break;

    case HTML_Attrib_bottom:
        /*
         *   We're aligned at the bottom of the text, so put our top at
         *   the text baseline minus our total height.  Note that we want
         *   the image itself aligned at the bottom, so subtract off the
         *   bottom spacing (the border and the vertical whitespace below
         *   the image).  
         */
        y_offset = measure_height() - vspace_ - border_;
        break;

    default:
        /* 
         *   if I'm a floater, my position is outside the line, so ignore
         *   changes during line formatting 
         */
        return;
    }

    /* set my position */
    pos_.offset(left - pos_.left,
                top - pos_.top + text_base_offset - y_offset);
}


/*
 *   Measure for table.  Non-floating images act like default display
 *   items.  Floating images contribute to the minimum size, but do not
 *   contribute to the overall line length, because they always get
 *   inserted at a line break. 
 */
void CHtmlDispImg::measure_for_table(CHtmlSysWin *win,
                                     CHtmlTableMetrics *metrics)
{
    /* check our alignment type */
    switch(align_)
    {
    case HTML_Attrib_left:
    case HTML_Attrib_right:
        /* 
         *   we're floating, so we're not part of a line; just add our
         *   invidisible width 
         */
        metrics->add_indivisible(measure_width(win));
        break;

    default:
        /* 
         *   We're not floating, so we go inside a line.  Act like an
         *   ordinary display item: we're unbreakable within, but we can
         *   break at either edge. 
         */
        CHtmlDisp::measure_for_table(win, metrics);
        break;
    }

    /* allow breaking after me */
    metrics->last_char_ = ' ';
    metrics->no_break_ = FALSE;
}

/*
 *   Find line break.  If this image is aligned left or right, so that
 *   text floats around it, don't break at the image, since the image is
 *   never part of a line; otherwise, use the normal breaking strategy. 
 */
CHtmlDisp *CHtmlDispImg::find_line_break(CHtmlFormatter *formatter,
                                         CHtmlSysWin *win,
                                         long space_avail,
                                         CHtmlLineBreak *break_pos)
{
    switch(align_)
    {
    case HTML_Attrib_left:
    case HTML_Attrib_right:
        /* 
         *   we float outside the text - this type of item isn't part of a
         *   line at all, so it can't be the first item on a line 
         */
        return 0;

    default:
        /* 
         *   other alignment types go inline with the text - follow the
         *   normal line breaking algorithm 
         */
        return CHtmlDisp::find_line_break(formatter, win,
                                          space_avail, break_pos);
    }
}

/*
 *   Find trailing whitespace.  We count as text, so any previous
 *   whitespace is not trailing.  
 */
void CHtmlDispImg::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    info->clear_ws_pos();
}

/*
 *   Determine what type of cursor to use over the image 
 */
Html_csrtype_t CHtmlDispImg::get_cursor_type(CHtmlSysWin *win,
                                             CHtmlFormatter *formatter,
                                             int x, int y,
                                             int more_mode) const
{
    CHtmlFmtMapZone *zone;

    /* 
     *   If I have an image map, ask the formatter to find the zone at the
     *   current location - if there's a match, use its link.  If links are
     *   disabled, ignore the image map.  Note that if the link is a "forced"
     *   style link, we'll show it as a link even when links are globally
     *   disabled.  
     */
    if ((zone = get_map_zone(formatter, x, y)) != 0
        && !more_mode
        && (win->get_html_show_links() || zone->link_->is_forced()))
    {
        /* use a hand cursor if the zone has a URL, arrow otherwise */
        return zone->link_->href_.get_url() != 0 && !zone->link_->nohref_
            ? HTML_CSRTYPE_HAND : HTML_CSRTYPE_ARROW;
    }

    /* no image map - use default handling */
    return CHtmlDisp::get_cursor_type(win, formatter, x, y, more_mode);
}

CHtmlDispLink *CHtmlDispImg::get_link(CHtmlFormatter *formatter,
                                      int x, int y) const
{
    CHtmlFmtMapZone *zone;

    /* 
     *   if I have an image map, ask the formatter to find the zone at the
     *   current location - if there's a match, use its link 
     */
    if ((zone = get_map_zone(formatter, x, y)) != 0)
        return zone->link_;

    /* no image map - use default handling */
    return CHtmlDisp::get_link(formatter, x, y);
}

/*
 *   If we have an image map, find the map zone containing a given point.
 *   If we don't have an image map, or the image map doesn't have a zone
 *   containing the given point, returns null.  
 */
CHtmlFmtMapZone *CHtmlDispImg::get_map_zone(CHtmlFormatter *formatter,
                                            int x, int y) const
{
    const char *map_name;
    CHtmlFmtMap *map;
    
    /* if we don't have an image map, there's definitely not a zone */
    if ((map_name = usemap_.get_url()) == 0)
        return 0;

    /* 
     *   Get the map name.  If it starts with a pound sign, it means that
     *   it's a local map.  Since this is the only kind of map we accept,
     *   the pound sign doesn't make any actual difference to us; so,
     *   simply remove it if it's present.  
     */
    if (*map_name == '#')
        ++map_name;

    /* 
     *   ask the formatter for a map matching our map name; if there isn't
     *   any such map, there's no zone match 
     */
    map = formatter->get_map(map_name, get_strlen(map_name));
    if (map == 0)
        return 0;

    /* ask the map to find the point */
    return map->find_zone(x, y, &pos_);
}

/*
 *   Format deferred - this is used to format the image as a floating
 *   marginal item.  
 */
void CHtmlDispImg::format_deferred(class CHtmlSysWin *win,
                                   class CHtmlFormatter *formatter,
                                   long line_spacing)
{
    /* inherit default handling to set up our temporary margins */
    basic_format_deferred(win, formatter, line_spacing, align_,
                          measure_height());
}

/* ------------------------------------------------------------------------ */
/*
 *   Linkable image 
 */

/*
 *   destroy 
 */
CHtmlDispImgLink::~CHtmlDispImgLink()
{
    /* if we have a hover image, remove our reference to it */
    if (h_image_ != 0)
        h_image_->remove_ref();

    /* if we have an acive (clicked) image, remove our reference */
    if (a_image_ != 0)
        a_image_->remove_ref();
}

/*
 *   set the hover and activate images 
 */
void CHtmlDispImgLink::set_extra_images(CHtmlResCacheObject *a_image,
                                        CHtmlResCacheObject *h_image)
{
    /* if we have existing images, remove them */
    forget_image(&a_image_);
    forget_image(&h_image_);

    /* remember the new images */
    a_image_ = a_image;
    h_image_ = h_image;

    /* add references to them if they're valid */
    if (a_image_ != 0)
        a_image_->add_ref();
    if (h_image_ != 0)
        h_image_->add_ref();
}

/*
 *   Get the link, given the cursor position.  If we have an image map, we'll
 *   return the link for the region of the map we're over; otherwise, we'll
 *   return the fixed link for the entire image.  
 */
CHtmlDispLink *CHtmlDispImgLink::get_link(CHtmlFormatter *formatter,
                                          int x, int y) const
{
    CHtmlFmtMapZone *zone;

    /* 
     *   if I have an image map, ask the formatter to find the zone at the
     *   current location - if there's a match, use its link 
     */
    if ((zone = get_map_zone(formatter, x, y)) != 0)
        return zone->link_;

    /* no image map - return my enclosing link */
    return link_;
}

/*
 *   Get the cursor type when the mouse is at a particular position over the
 *   image.  If we have an image map, we'll check to see if we're over a
 *   linked part of the map; otherwise, we'll use the appropriate cursor for
 *   our fixed link. 
 */
Html_csrtype_t CHtmlDispImgLink::get_cursor_type(
    CHtmlSysWin *win, CHtmlFormatter *formatter,
    int x, int y, int more_mode) const
{
    CHtmlFmtMapZone *zone;
    CHtmlDispLink *link;

    /* get the map zone (if any), and the link for the given position */
    zone = get_map_zone(formatter, x, y);
    link = get_link(formatter, x, y);

    /* if there's no link, use an arrow cursor */
    if (link == 0)
        return HTML_CSRTYPE_HAND;

    /* 
     *   if links are disabled, and the link doesn't use the "forced" style,
     *   inherit the default handling 
     */
    if (more_mode
        || (!win->get_html_show_links() && !link->is_forced()))
        return CHtmlDispImg::get_cursor_type(win, formatter,
                                             x, y, more_mode);

    /* if I have an image map, see if the cursor is in an image map zone */
    if (zone != 0)
    {
        /* use a hand cursor if the zone has a URL, or an arrow otherwise */
        return link->href_.get_url() != 0 && !link->nohref_
            ? HTML_CSRTYPE_HAND : HTML_CSRTYPE_ARROW;
    }

    /* use the hand cursor, since the whole image is linked */
    return HTML_CSRTYPE_HAND;
}

/*
 *   draw the picture 
 */
void CHtmlDispImgLink::draw(CHtmlSysWin *win,
                            unsigned long sel_start,
                            unsigned long sel_end,
                            int /*clicked*/)
{
    int clicked;
    CHtmlResCacheObject *new_pic;
    
    /* get our link status */
    clicked = link_->get_clicked();

    /* figure out which picture to display based on our status */
    if (clicked == 0)
    {
        /* unclicked - use the ordinary SRC image */
        new_pic = image_;
    }
    else if ((clicked & CHtmlDispLink_clicked) != 0)
    {
        /* 
         *   clicked - use the ASRC image if there is one; otherwise fall
         *   back on the HSRC image if there is one; otherwise fall back on
         *   the regular SRC image 
         */
        new_pic = a_image_;
        if (new_pic == 0 || new_pic->get_image() == 0)
            new_pic = h_image_;
        if (new_pic == 0 || new_pic->get_image() == 0)
            new_pic = image_;
    }
    else if ((clicked & CHtmlDispLink_hover) != 0)
    {
        /* 
         *   hovering - use the HSRC image if there is one; otherwise fall
         *   back on the regular SRC image 
         */
        new_pic = h_image_;
        if (new_pic == 0 || new_pic->get_image() == 0)
            new_pic = image_;
    }

    /* 
     *   if the picture has changed, register the change with the display
     *   site implementation
     */
    if (new_pic != get_site_image())
        set_site_image(new_pic);

    /* draw using the selected image */
    draw_image(win, new_pic);
}

/* ------------------------------------------------------------------------ */
/*
 *   TABLE display item 
 */

CHtmlDispTable::CHtmlDispTable(class CHtmlSysWin *win,
                               long border, HTML_Attrib_id_t align,
                               int floating,
                               long width, int width_pct, int width_set,
                               long win_width,
                               HTML_color_t bgcolor, int use_bgcolor,
                               CHtmlResCacheObject *bg_image)
    : CHtmlDispDisplaySite(win, &pos_, bg_image)
{
    /* remember the settings */
    border_ = border;
    align_ = align;
    floating_ = floating;
    width_ = width;
    width_pct_ = width_pct;
    width_set_ = width_set;
    win_width_ = win_width;
    width_min_ = width_max_ = 0;
    pos_.set(0, 0, 0, 0);
    bgcolor_ = bgcolor;
    use_bgcolor_ = use_bgcolor;
    sub_head_ = 0;
}

/*
 *   Measure the table for inclusion in a table.  This will be used when
 *   the table is nested within another table's cell.  This is never
 *   called for a top-level table, because it's called for a table's
 *   contents rather than for the table itself; a table will only be
 *   measured when it's inside another table.
 *   
 *   We calculate the space needed for the nested table according to its
 *   contents.  The minimum width for the nested table is the sum of the
 *   minimum widths for its columns; likewise for the maximum width.
 *   
 *   Nested tables are measured before their enclosing tables, because the
 *   formatter goes through the table's display list after everything
 *   inside the table has been formatted during pass 1.  The nested table
 *   is measured first because it end first.  By the time the enclosing
 *   table is ready to ask for measurements, the nested table has been
 *   completely calculated.  Note that we'll run through the contents of
 *   the nested table and calculate their sizes; this is a redundant but
 *   harmless exercise, since the nested table's contents are necessarily
 *   no larger than the nested table itself.  
 */
void CHtmlDispTable::measure_for_table(CHtmlSysWin *,
                                       CHtmlTableMetrics *metrics)
{
    /* 
     *   add our minimum width to the indivisible widths, since this is
     *   the smallest we can go 
     */
    metrics->add_indivisible(width_min_);

    /* 
     *   add our maximum width to the total line width, since this is the
     *   largest we'd need
     */
    metrics->add_to_cur_line(width_max_);

    /* nothing left over from this item */
    metrics->clear_leftover();
    metrics->last_char_ = ' ';

    /* 
     *   clear the current line width in the metrics - our contents
     *   effectively go on a new line, since the table's contents overlap the
     *   table's area in the layout 
     */
    metrics->start_new_line();
}

/*
 *   Measure the table's contribution to a banner's width.  If the table
 *   is based on a percentage of the window width, we won't contribute
 *   anything to the minimum width, because we don't know yet how big the
 *   banner is going to be (since this routine is called to calculate
 *   that), hence we don't know what our percentage applies to yet. 
 */
void CHtmlDispTable::measure_for_banner(CHtmlSysWin *,
                                        CHtmlTableMetrics *metrics)
{
    /* 
     *   add our minimum width to the indivisible widths, since this is the
     *   smallest we can go 
     */
    metrics->add_indivisible(width_min_);

    /* 
     *   add our maximum width to the total line width, since this is the
     *   largest we'd need 
     */
    metrics->add_to_cur_line(width_max_);

    /* nothing left over from this item */
    metrics->clear_leftover();
    metrics->last_char_ = ' ';
}


/*
 *   draw the table 
 */
void CHtmlDispTable::draw(CHtmlSysWin *win,
                          unsigned long /*sel_start*/,
                          unsigned long /*sel_end*/,
                          int /*clicked*/)
{
    /* if there's a background image or color, draw it */
    if (get_site_image() != 0 && get_site_image()->get_image() != 0)
    {
        /* tile our image into our drawing area */
        get_site_image()->get_image()
            ->draw_image(win, &pos_, HTMLIMG_DRAW_TILE);
    }
    else if (use_bgcolor_)
    {
        /* fill in our area with our background color */
        win->draw_table_bkg(&pos_, bgcolor_);
    }

    /* if we have a border, draw it */
    if (border_ != 0)
        win->draw_table_border(&pos_, border_, FALSE);
}

int CHtmlDispTable::measure_width(CHtmlSysWin *)
{
    /* return the width we have set in our position */
    return pos_.right - pos_.left;
}

void CHtmlDispTable::get_vertical_space(CHtmlSysWin *, int /*text_height*/,
                                        int *above_base, int *below_base,
                                        int *total)
{
    /* 
     *   the text baseline is irrelevant to a table, since tables are not
     *   set as part of a line of text; so, put our entire height above
     *   the baseline 
     */
    *above_base = *total = pos_.bottom - pos_.top;
    *below_base = 0;
}

/*
 *   Find trailing whitespace.  We count as text, so any previous
 *   whitespace is not trailing.  
 */
void CHtmlDispTable::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    info->clear_ws_pos();
}

/*
 *   Calculate the table's actual size.  Based on the actual size,
 *   calculate the sizes of the columns.  
 */
void CHtmlDispTable::calc_table_size(CHtmlFormatter *formatter,
                                     CHtmlTagTABLE *tag)
{
    long wid;
    long x_offset;

    /* 
     *   check to see if the user asked for a particular size via the
     *   WIDTH attribute 
     */
    if (width_set_)
    {
        /*
         *   Use the requested size, but in no event go any smaller than
         *   the minimum width. 
         */
        wid = width_;
        if (wid < width_min_)
            wid = width_min_;
    }
    else
    {
        /*
         *   No WIDTH attribute is specified for the table, so let the
         *   contents determine the table's size, constraining the size if
         *   possible to our available window width.  
         */
        wid = tag->calc_table_width(width_min_, width_max_, win_width_);
    }

    /*
     *   Figure out where to put the table based on the horizontal
     *   alignment, now that we know the width of the available space and
     *   the amount of space we'll take up.  
     */
    switch(align_)
    {
    case HTML_Attrib_left:
    default:
        /* left alignment - put it at the left margin */
        x_offset = 0;
        break;

    case HTML_Attrib_right:
        /* 
         *   Right alignment - if it's smaller than the available space,
         *   offset it to the right by the amount of space left over so that
         *   it's flush against the right margin.  If we don't have room for
         *   that, align it at the left margin, as that's the furthest left
         *   we can go.  (It's better to hang off the right side, since the
         *   scrollbar allows scrolling in that direction, than to hang off
         *   the left side.)  
         */
        if (wid < win_width_)
            x_offset = win_width_ - wid;
        else
            x_offset = 0;
        break;

    case HTML_Attrib_center:
        /*
         *   centered - if it's smaller than the available space, center it
         *   in the available space; otherwise, put it at the left margin,
         *   since that's as far left as we're allowed to go 
         */
        if (wid < win_width_)
            x_offset = (win_width_ - wid) / 2;
        else
            x_offset = 0;
        break;
    }

    /* set the column widths */
    tag->set_column_widths(x_offset, wid);

    /* set our position and width */
    pos_.left += x_offset;
    pos_.right = pos_.left + wid;
}

/*
 *   Format deferred - this is used to format the table as a floating
 *   marginal item. 
 */
void CHtmlDispTable::format_deferred(class CHtmlSysWin *win,
                                     class CHtmlFormatter *formatter,
                                     long line_spacing)
{
    CHtmlRect old_pos;
    long dx, dy;
    CHtmlDisp *cur;

    /* 
     *   note the original position, so we can determine how much we move in
     *   setting our final position 
     */
    old_pos = pos_;

    /* 
     *   inherit default handling to set our final position and set up the
     *   temporary margins that exclude display items in the main flow from
     *   the area we're occupying at the edge of the document 
     */
    basic_format_deferred(win, formatter, line_spacing, align_,
                          pos_.bottom - pos_.top);

    /* calculate how far we've moved */
    dx = pos_.left - old_pos.left;
    dy = pos_.top - old_pos.top;

    /* move each of our subitems by the same amount we moved */
    for (cur = sub_head_ ; cur != 0 ; cur = cur->get_next_disp())
    {
        CHtmlRect pos;

        /* get the position of this item */
        pos = cur->get_pos();

        /* move it by the same amount that the table moved */
        pos.offset(dx, dy);

        /* set the new position */
        cur->set_pos(&pos);
    }

    /* 
     *   add our contents sublist to the master display list - our inherited
     *   base implementation just called will have added us to the display
     *   list, so we can simply add our contents now 
     */
    formatter->add_to_disp_list(sub_head_);
}

/* 
 *   Set our sublist - this gives us our list of contained display items for
 *   use in formatting floating tables.  
 */
void CHtmlDispTable:: set_contents_sublist(CHtmlDisp *head)
{
    /* remember the head of the list */
    sub_head_ = head;
}

/* ------------------------------------------------------------------------ */
/*
 *   Table cell 
 */

CHtmlDispTableCell::CHtmlDispTableCell(CHtmlSysWin *win, int border,
                                       HTML_color_t bgcolor, int use_bgcolor,
                                       CHtmlResCacheObject *bg_image,
                                       long width_attr, int width_pct,
                                       int width_set)
    : CHtmlDispDisplaySite(win, &pos_, bg_image)
{
    /* save the settings */
    pos_.set(0, 0, 0, 0);
    border_ = (border != 0);
    bgcolor_ = bgcolor;
    use_bgcolor_ = use_bgcolor;
    width_attr_ = width_attr;
    width_pct_ = width_pct;
    width_set_ = width_set;
}

void CHtmlDispTableCell::draw(CHtmlSysWin *win,
                              unsigned long /*sel_start*/,
                              unsigned long /*sel_end*/,
                              int /*clicked*/)
{
    /* if there's a background image or color, draw it */
    if (get_site_image() != 0 && get_site_image()->get_image() != 0)
    {
        /* tile our image into our area */
        get_site_image()->get_image()
            ->draw_image(win, &pos_, HTMLIMG_DRAW_TILE);
    }
    else if (use_bgcolor_)
    {
        /* fill in our area with our background color */
        win->draw_table_bkg(&pos_, bgcolor_);
    }

    /* 
     *   If the table has a border, draw the cell with a one-pixel border.
     *   (The size of the cell's border is always one pixel; the only
     *   effect the table's BORDER attribute has on the cell's border is
     *   whether it's drawn at all.) 
     */
    if (border_)
        win->draw_table_border(&pos_, 1, TRUE);
}

int CHtmlDispTableCell::measure_width(CHtmlSysWin *)
{
    /* return the width we have set in our position */
    return pos_.right - pos_.left;
}

/* 
 *   Measure the metrics of this item for use in a table cell.  Since this
 *   *is* a table cell, this routine is called only when we're in a nested
 *   table - we're being measured for use in the enclosing table's cell that
 *   contains our entire table.  
 */
void CHtmlDispTableCell::measure_for_table(class CHtmlSysWin *win,
                                           class CHtmlTableMetrics *metrics)
{
    /* 
     *   If we have a pixel (not percentage-based) WIDTH setting, include
     *   that as our minimum size.  If our width is not set, do not
     *   calculate any contribution, since we'll size either as a percentage
     *   of our container's size or according to our contents size; in
     *   either case, the size doesn't depend on us at all, so we have
     *   nothing to add.  
     */
    if (width_set_ && !width_pct_)
        metrics->add_indivisible(width_attr_);

    /* nothing left over from this item */
    metrics->clear_leftover();
    metrics->last_char_ = ' ';
}

/*
 *   Measure the cell for its contribution to the banner size.  We'll only
 *   contribute a minimum width if our WIDTH attribute was explicitly set
 *   to a pixel (not a percentage) value, since we will otherwise adapt to
 *   the size of the banner rather than vice versa.  
 */
void CHtmlDispTableCell::measure_for_banner(CHtmlSysWin *win,
                                            CHtmlTableMetrics *metrics)
{
    /* 
     *   if we have a pixel (not percentage-based) WIDTH setting, include
     *   that as our minimum size; otherwise, do not make a contribution,
     *   since we will adapt to the banner width 
     */
    if (width_set_ && !width_pct_)
        metrics->add_indivisible(width_attr_);

    /* add our current calculated width to the line width */
    metrics->add_to_cur_line(measure_width(win));

    /* nothing left over from this item */
    metrics->clear_leftover();
    metrics->last_char_ = ' ';
}

void CHtmlDispTableCell::get_vertical_space(CHtmlSysWin *,
                                            int /*text_height*/,
                                            int *above_base, int *below_base,
                                            int *total)
{
    /* 
     *   the text baseline is irrelevant to a table, since tables are not
     *   set as part of a line of text; so, put our entire height above
     *   the baseline 
     */
    *above_base = *total = pos_.bottom - pos_.top;
    *below_base = 0;
}

/*
 *   Find trailing whitespace.  We count as text, so any previous
 *   whitespace is not trailing.  
 */
void CHtmlDispTableCell::find_trailing_whitespace(CHtmlDisp_wsinfo *info)
{
    info->clear_ws_pos();
}

/* ------------------------------------------------------------------------ */
/*
 *   Whitespace search object implementation 
 */

/*
 *   remove trailing whitespace from the last item we found with trailing
 *   whitespace 
 */
void CHtmlDisp_wsinfo::remove_trailing_whitespace(
    CHtmlSysWin *win, CHtmlDisp *disp, CHtmlDisp *nxt)
{
    CHtmlDisp *cur;
    
    /* 
     *   scan for trailing whitespace, starting at the given item and
     *   continuing to the end of the display list 
     */
    for (cur = disp ; cur != 0 && cur != nxt ; cur = cur->get_next_disp())
        cur->find_trailing_whitespace(this);

    /* if we found trailing whitespace, go remove it */
    if (last_ws_disp_ != 0)
    {
        /* 
         *   remove trailing whitespace from the display item where we found
         *   the start of the run of whitespace 
         */
        last_ws_disp_->remove_trailing_whitespace(win, last_ws_pos_);

        /* 
         *   Any items after this item must be entirely whitespace, since
         *   they're part of the run of contiguous whitespace from the
         *   starting position to the end of the list.  Go through all of
         *   the remaining items and remove all of their whitespace. 
         */
        for (cur = last_ws_disp_->get_next_disp() ; cur != 0 && cur != nxt ;
             cur = cur->get_next_disp())
            cur->remove_trailing_whitespace(win, 0);
    }
}


/*
 *   hide trailing whitespace - this is similar to
 *   remove_trailing_whitespace, but merely hides the trailing spaces from
 *   the visual appearance, rather than deleting them from memory 
 */
void CHtmlDisp_wsinfo::hide_trailing_whitespace(
    CHtmlSysWin *win, CHtmlDisp *disp, CHtmlDisp *nxt)
{
    CHtmlDisp *cur;

    /* scan the given item list for trailing whitespace */
    for (cur = disp ; cur != 0 && cur != nxt ; cur = cur->get_next_disp())
        cur->find_trailing_whitespace(this);

    /* if we found trailing whitespace, go hide it */
    if (last_ws_disp_ != 0)
    {
        /* hide whitespace from the item where the trailing run starts */
        last_ws_disp_->hide_trailing_whitespace(win, last_ws_pos_);

        /* entirely hide all of the items after this one */
        for (cur = last_ws_disp_->get_next_disp() ; cur != 0 && cur != nxt ;
             cur = cur->get_next_disp())
            cur->hide_trailing_whitespace(win, 0);
    }
}

