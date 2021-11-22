#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlinp.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlinp.cpp - input editor services class
Function
  
Notes
  
Modified
  09/28/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLINP_H
#include "htmlinp.h"
#endif

CHtmlInputBuf::~CHtmlInputBuf()
{
    size_t i;
    
    /* drop history items */
    for (i = 0 ; i < histcnt_ ; ++i)
        th_free(history_[i]);
    
    /* drop history buffer and the saved current line */
    if (history_ != 0)
        th_free(history_);
    if (saved_cur_line_ != 0)
        th_free(saved_cur_line_);

    /* delete the undo buffer if we have one */
    if (undo_buf_)
        th_free(undo_buf_);
}

/*
 *   set the buffer 
 */
void CHtmlInputBuf::setbuf(textchar_t *buf, size_t bufsiz, size_t initlen)
{
    /* remember the buffer information */
    changebuf(buf, bufsiz);

    /* set up with the initial command */
    len_ = initlen;

    /* set up at the end of the current command */
    sel_start_ = len_;
    sel_end_ = len_;
    caret_ = len_;

    /* reset the history position */
    set_last_hist();

    /* caret starts off visible */
    caret_vis_ = TRUE;

    /* drop any undo we have */
    if (undo_buf_ != 0)
    {
        th_free(undo_buf_);
        undo_buf_ = 0;
    }
}

/*
 *   Change the buffer, without resetting any of the buffer settings.  This
 *   can be used to resume an existing editing session with a different
 *   buffer.  
 */
void CHtmlInputBuf::changebuf(textchar_t *buf, size_t bufsiz)
{
    /* remember the buffer information */
    buf_ = buf;
    bufsiz_ = bufsiz;

    /* if this is a null buffer, note that we have nothing in the buffer */
    if (buf_ == 0)
    {
        /* we have no contents, so the content length is zero */
        len_ = 0;

        /* forget any selection */
        sel_start_ = sel_end_ = caret_ = 0;
    }
}


/*
 *   delete the selected range 
 */
int CHtmlInputBuf::del_selection()
{
    /* if nothing's selected, there's nothing to do */
    if (sel_start_ == sel_end_)
        return FALSE;

    /* save undo */
    save_undo();

    /* move the tail over the selection, if necessary */
    if (len_ != sel_end_)
        memmove(buf_ + sel_start_, buf_ + sel_end_, len_ - sel_end_);

    /* reduce the length by the selection size */
    len_ -= sel_end_ - sel_start_;

    /* move the selection end to the start */
    caret_ = sel_end_ = sel_start_;
    show_caret();

    /* we made a change */
    return TRUE;
}

/*
 *   move the cursor left one character 
 */
void CHtmlInputBuf::move_left(int extend, int word)
{
    size_t newpos;

    /* if not already showing the caret, make it visible again */
    show_caret();

    /* if we're already at the start of the command, there's nothing to do */
    if (caret_ == 0)
        return;

    /* see if we're doing a word or a character */
    if (word)
    {
        const textchar_t *p;

        /* move left until we're not in spaces */
        p = buf_ + caret_ - 1;
        while (p > buf_ && is_space(*p))
            --p;

        /* move left until the character to the left is a space */
        while (p > buf_ && !is_space(*(p-1)))
            --p;

        /* this is the new position */
        newpos = p - buf_;
    }
    else
    {
        /* move by one character */
        newpos = caret_ - 1;
    }

    /* adjust selection */
    adjust_sel(newpos, extend);
}

/*
 *   move the cursor right one character 
 */
void CHtmlInputBuf::move_right(int extend, int word)
{
    size_t newpos;
    
    /* if not already showing the caret, make it visible again */
    show_caret();

    /* if we're already at the end of the command, there's nothing to do */
    if (caret_ == len_)
        return;

    /* see if we're doing a word or a character */
    if (word)
    {
        const textchar_t *p;
        const textchar_t *eol;

        /* move right until we find a space */
        p = buf_ + caret_;
        eol = buf_ + len_;
        while (p < eol && !is_space(*p))
            ++p;

        /* move right until we find something other than a space */
        while (p < eol && isspace(*p))
            ++p;

        /* set the new position */
        newpos = p - buf_;
    }
    else
    {
        /* move by one character */
        newpos = caret_ + 1;
    }

    /* adjust selection */
    adjust_sel(newpos, extend);
}

/*
 *   backspace 
 */
int CHtmlInputBuf::backspace()
{
    /* if not already showing the caret, make it visible again */
    show_caret();

    /* see if there's a selection range */
    if (sel_start_ != sel_end_)
    {
        /* there's a range - delete it */
        return del_selection();
    }
    else if (caret_ > 0)
    {
        /* save undo */
        save_undo();

        /*
         *   no range - delete character to left of cursor.  Move the
         *   characters to the right of the cursor over the character to
         *   the left of the cursor, if necessary.  
         */
        if (len_ > caret_)
            memmove(buf_ + caret_ - 1, buf_ + caret_, len_ - caret_);

        /* it's now one character shorter */
        --len_;

        /* back up the caret one position */
        sel_start_ = sel_end_ = --caret_;

        /* we made a change */
        return TRUE;
    }
    else
    {
        /* we didn't change anything */
        return FALSE;
    }
}

/*
 *   delete right
 */
int CHtmlInputBuf::del_right()
{
    /* if not already showing the caret, make it visible again */
    show_caret();

    /* see if there's a selection range */
    if (sel_start_ != sel_end_)
    {
        /* there's a range - delete it */
        return del_selection();
    }
    else if (caret_ < len_)
    {
        /* save undo */
        save_undo();

        /*
         *   no range - delete character to right of cursor.  Move the
         *   characters after the character to the right of the cursor
         *   over the character to the right of the cursor, if necessary.
         */
        if (len_ > caret_ + 1)
            memmove(buf_ + caret_, buf_ + caret_ + 1, len_ - caret_ - 1);

        /* it's now one character shorter */
        --len_;

        /* we made a change */
        return TRUE;
    }
    else
    {
        /* we didn't change anything */
        return FALSE;
    }
}

/*
 *   add a character 
 */
int CHtmlInputBuf::add_char(textchar_t c)
{
    int changed;

    /* if there's a selection range, delete it */
    changed = del_selection();

    /* if there's room, insert the character */
    if (len_ < bufsiz_)
    {
        /* if we didn't already make a change, save undo */
        if (!changed)
            save_undo();
        
        /* move characters to right of cursor to make room if necessary */
        if (caret_ != len_)
            memmove(buf_ + caret_ + 1, buf_ + caret_, len_ - caret_);

        /* add the new character */
        buf_[caret_] = c;
        ++len_;

        /* move the insertion point */
        sel_start_ = sel_end_ = ++caret_;
        show_caret();

        /* we made a change */
        changed = TRUE;
    }

    /* return change indication */
    return changed;
}

/*
 *   Add a string to the buffer.  Inserts the new string at the current
 *   insertion point, replacing any current selection range.  
 */
int CHtmlInputBuf::add_string(const textchar_t *str, size_t len, int trunc)
{
    int changed;

    /* see if it fits, or if we need to truncate */
    if (len_ + len > bufsiz_ - (sel_end_ - sel_start_))
    {
        /* it doesn't fit - if we can't truncate, we can't do anything */
        if (!trunc)
            return FALSE;

       /*
        *   Truncate the new text to what will fit in our buffer.  First,
        *   figure out how much buffer size is simply not currently used -
        *   this is the size of the buffer minus the length of the current
        *   text in the buffer (bufsiz_ - len_).  Then, since we're replacing
        *   the current selection range, the length of the current selection
        *   range (sel_end_ - sel_start_) will also be available to us as
        *   free space.  
        */
       len = (bufsiz_ - len_) +  (sel_end_ - sel_start_);
    }

    /* if there's a selection range, delete it */
    changed = del_selection();

    /* add the new string, if there is one */
    if (len != 0)
    {
        /* if we didn't already make a change, save undo */
        if (!changed)
            save_undo();
        
        /* move characters to right of cursor to make room if necessary */
        if (caret_ != len_)
            memmove(buf_ + caret_ + len, buf_ + caret_, len_ - caret_);
        
        /* add the string */
        memcpy(buf_ + caret_, str, len);

        /* update the length */
        len_ += len;

        /* move the insertion point past the new string */
        caret_ += len;
        sel_start_ = sel_end_ = caret_;
        show_caret();
        
        /* we changed it */
        changed = TRUE;
    }

    /* return the change indication */
    return changed;
}


/*
 *   Move to the start of the command 
 */
void CHtmlInputBuf::start_of_line(int extend)
{
    /* adjust selection */
    adjust_sel(0, extend);
}


/*
 *   Move to the end of the command 
 */
void CHtmlInputBuf::end_of_line(int extend)
{
    /* adjust selection */
    adjust_sel(len_, extend);
}

/*
 *   Delete to end of line 
 */
int CHtmlInputBuf::del_eol()
{
    /* if not already showing the caret, make it visible again */
    show_caret();

    /* if there's anything to the right of the cursor, delete it all */
    if (caret_ < len_)
    {
        /* save undo */
        save_undo();
        
        /* drop everything after the cursor */
        len_ = caret_;

        /* zero out the selection */
        sel_start_ = sel_end_ = caret_;

        /* it's different now */
        return TRUE;
    }

    /* nothing changed */
    return FALSE;
}

/*
 *   Delete entire line 
 */
int CHtmlInputBuf::del_line()
{
    /* if not already showing the caret, make it visible again */
    show_caret();

    /* if there's nothing in the line, there's nothing to do */
    if (len_ == 0)
        return FALSE;

    /* save undo */
    save_undo();

    /* drop everything */
    len_ = 0;

    /* move the cursor to the beginning of the line */
    sel_start_ = sel_end_ = caret_ = 0;

    /* it's changed */
    return TRUE;
}

/*
 *   Adjust the selection range 
 */
void CHtmlInputBuf::adjust_sel(size_t newpos, int extend)
{
    if (extend)
    {
        /*
         *   Extending range - move whichever end of the current range is
         *   at the caret so that it stays with the caret, and leave the
         *   other end of the range alone.  If we have no selection range
         *   now, start one. 
         */
        if (sel_start_ == sel_end_)
        {
            /*
             *   no range now - move one of the ends to follow the
             *   cursor, keeping start <= end 
             */
            if (newpos < sel_start_)
                sel_start_ = newpos;
            else
                sel_end_ = newpos;
        }
        else
        {
            /*
             *   we already have a range - move the end which is at the
             *   cursor to the new position, and leave the other end where
             *   it is 
             */
            if (sel_start_ == caret_)
                sel_start_ = newpos;
            else
                sel_end_ = newpos;
        }
    }
    else
    {
        /* not extending -- simply move everything to the new position */
        sel_start_ = sel_end_ = newpos;
    }

    /* in any case, move the caret to its new position */
    caret_ = newpos;
    show_caret();
}

/*
 *   Set the maximum history size 
 */
void CHtmlInputBuf::set_hist_size(size_t max_hist_cnt)
{
    /* allocate the buffer */
    history_ = (textchar_t **)th_malloc(max_hist_cnt * sizeof(textchar_t *));
    histsiz_ = max_hist_cnt;

    /* no history entries yet */
    histcnt_ = 0;
    histpos_ = 0;

    /* no saved current line yet */
    saved_cur_line_ = 0;
}

/*
 *   Select the previous history item 
 */
int CHtmlInputBuf::select_prev_hist()
{
    /* if we're at the first item already, we can't go back any further */
    if (histpos_ == 0)
        return FALSE;

    /* if we're editing a line not in the history buffer, save it */
    if (histpos_ == histcnt_)
        save_cur_line();

    /* save undo */
    save_undo();

    /* back up one position */
    --histpos_;

    /* select this item */
    return select_cur_hist();
}

/*
 *   Select a previous history item that matches the given leading substring.
 *   This treats the history buffer as circular.  
 */
int CHtmlInputBuf::select_prev_hist_prefix(const textchar_t *pre, size_t len)
{
    size_t cur;

    /* if there's nothing in the history buffer, ignore this */
    if (histcnt_ == 0)
        return FALSE;
        
    /* start with the previous line, wrapping at the start of the list */
    cur = (histpos_ == 0 ? histcnt_ : histpos_) - 1;

    /* search for a matching line; if we get back to where we started, fail */
    while (cur != histpos_)
    {
        /* if this one matches, stop here */
        if (strlen(history_[cur]) > len
            && memcmp(history_[cur], pre, len) == 0)
        {
            int ret;
            
            /* remember the current caret position so we can restore it */
            size_t old_caret = caret_;
            
            /* activate this position */
            histpos_ = cur;

            /* we're about to change the buffer, so save undo */
            save_undo();

            /* select this item */
            ret = select_cur_hist();

            /* if that succeeded, restore the caret position */
            if (ret)
                sel_start_ = sel_end_ = caret_ = old_caret;

            /* return the result */
            return ret;
        }

        /* move to the previous line, wrapping at the start of the list */
        cur = (cur == 0 ? histcnt_ : cur) - 1;
    }

    /* we didn't find a match; ignore it */
    return FALSE;
}

/*
 *   Select a previous history item based on the current buffer contents and
 *   caret position. 
 */
int CHtmlInputBuf::select_prev_hist_prefix()
{
    /* select based on the current buffer up to the caret position */
    return select_prev_hist_prefix(buf_, caret_);
}

/*
 *   Select the next history item 
 */
int CHtmlInputBuf::select_next_hist()
{
    /* if we're at the last item already, we can't go any further */
    if (histpos_ == histcnt_)
        return FALSE;

    /* save undo */
    save_undo();

    /* move on to the next item */
    ++histpos_;

    /* select the item */
    return select_cur_hist();
}

/*
 *   Select the current history item into the buffer 
 */
int CHtmlInputBuf::select_cur_hist()
{
    size_t len;
    const textchar_t *src;

    /* if we're after the last item, simply clear the line */
    if (histpos_ == histcnt_)
        src = saved_cur_line_;
    else
        src = history_[histpos_];

    /* if we don't have anything, delete the line and be done with it */
    if (src == 0)
        return del_line();

    /* if this item doesn't fit in the buffer, truncate it */
    len = strlen(src);
    if (len > bufsiz_)
        len = bufsiz_;

    /* copy this item into the buffer */
    memcpy(buf_, src, len);
    len_ = len;

    /* position the cursor at the end of this item */
    sel_start_ = sel_end_ = caret_ = len;
    show_caret();

    /* we changed the buffer */
    return TRUE;
}

/*
 *   Save the current line, so that if we scroll through the history we
 *   can come back to the original line under construction 
 */
void CHtmlInputBuf::save_cur_line()
{
    /* if we have no buffer, there's nothing to save */
    if (buf_ == 0)
        return;

    /* if we have an old saved line, lose it */
    if (saved_cur_line_)
        th_free(saved_cur_line_);

    /* allocate space for the current line and store it */
    saved_cur_line_ = (textchar_t *)
                      th_malloc((len_ + 1) * sizeof(textchar_t));
    memcpy(saved_cur_line_, buf_, len_);
    saved_cur_line_[len_] = '\0';
}

/*
 *   Add an item to the history buffer.  If the history is full, delete
 *   the oldest item to make room. 
 */
void CHtmlInputBuf::add_hist(const textchar_t *str, size_t len)
{
    textchar_t *cpy;
    
    /* if it's an exact copy of the most recent history item, ignore it */
    if (histcnt_ != 0
        && strlen(history_[histcnt_ - 1]) == len
        && memcmp(history_[histcnt_ - 1], str, len) == 0)
    {
        /* it's an exact duplicate - don't bother adding it */
        return;
    }

    /* if we don't have room, make room by deleting the oldest item */
    if (histcnt_ == histsiz_)
    {
        /* delete the oldest one */
        th_free(history_[0]);

        /* move the array down a notch */
        memmove(history_, history_ + 1,
                (histsiz_ - 1) * sizeof(history_[0]));

        /* that's one less entry */
        --histcnt_;
    }

    /* make a copy of this item, adding null termination */
    cpy = (textchar_t *)th_malloc((len + 1) * sizeof(textchar_t));
    memcpy(cpy, str, len);
    cpy[len] = '\0';

    /* add the new item to our list */
    history_[histcnt_++] = cpy;
}

/*
 *   Undo the last change 
 */
int CHtmlInputBuf::undo()
{
    textchar_t *undo_buf;
    size_t undo_len;
    size_t undo_sel_start, undo_sel_end, undo_caret;
    size_t undo_histpos;

    /* if we have no buffer, there's nothing to do */
    if (buf_ == 0)
        return FALSE;
    
    /* do nothing if there's no undo */
    if (undo_buf_ == 0)
        return FALSE;

    /*
     *   take over the current undo buffer, so that we can save the
     *   current buffer as the new undo buffer but still have the old undo
     *   information around so that we can apply it 
     */
    undo_buf = undo_buf_;
    undo_len = undo_len_;
    undo_sel_start = undo_sel_start_;
    undo_sel_end = undo_sel_end_;
    undo_caret = undo_caret_;
    undo_histpos = undo_histpos_;

    /* we now own the old undo buffer - forget it globally */
    undo_buf_ = 0;

    /* save the current buffer as the new undo */
    save_undo();

    /* apply the undo */
    memcpy(buf_, undo_buf, undo_len);
    len_ = undo_len;
    sel_start_ = undo_sel_start;
    sel_end_ = undo_sel_end;
    caret_ = undo_caret;
    histpos_ = undo_histpos;

    /* drop the old undo buffer */
    th_free(undo_buf);

    /* we made some changes */
    return TRUE;
}

/*
 *   Save a change for later undoing 
 */
void CHtmlInputBuf::save_undo()
{
    /* if we have no buffer, there's nothing to do */
    if (buf_ == 0)
        return;

    /* drop any previous undo buffer */
    if (undo_buf_ != 0)
        th_free(undo_buf_);
    
    /* allocate space for the current buffer */
    undo_buf_ = (textchar_t *)th_malloc(len_);
    undo_len_ = len_;
    memcpy(undo_buf_, buf_, len_);

    /* remember the selection points for after the undo */
    undo_sel_start_ = sel_start_;
    undo_sel_end_ = sel_end_;
    undo_caret_ = caret_;

    /* remember history position */
    undo_histpos_ = histpos_;
}

