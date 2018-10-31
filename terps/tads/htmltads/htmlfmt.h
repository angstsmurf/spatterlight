/* $Header: d:/cvsroot/tads/html/htmlfmt.h,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlfmt.h - TADS HTML formatter
Function
  Takes an HTML parse tree as input, and formats the text into a
  draw list.  The draw list is a set of rectangular areas suitable
  for display.
Notes
  
Modified
  09/07/97 MJRoberts  - Creation
*/

#ifndef HTMLFMT_H
#define HTMLFMT_H

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
 *   Line break state object.  This structure lets each display item keep
 *   track of the last location for a line break, so that when we exceed
 *   the available display width we can apply a line break.  
 */
class CHtmlLineBreak
{
public:
    CHtmlLineBreak() { clear(); }

    /* clear the break position */
    void clear()
    {
        item_ = 0;
        pos_ = 0;
        last_char_ = 0;
        no_break_ = FALSE;
        ws_start_item_ = 0;
        ws_start_pos_ = 0;
        brk_ws_start_item_ = 0;
        brk_ws_start_pos_ = 0;
    }

    /* 
     *   Set the break position.  If we're currently in a run of whitespace,
     *   and the existing break position is in the same run of whitespace,
     *   we'll ignore this new break position, so that we always break at
     *   the start of a run of whitespace.  
     */
    void set_break_pos(class CHtmlDisp *item, void *pos)
    {
        /* 
         *   if we're in a run of whitespace, and the current break position
         *   is in the same run of whitespace, ignore the new position,
         *   since breaking anywhere in the run of whitespace is equivalent
         *   to breaking at the earliest possible point in the run of
         *   whitespace 
         */
        if (ws_start_item_ != 0
            && brk_ws_start_item_ == ws_start_item_
            && brk_ws_start_pos_ == ws_start_pos_)
            return;

        /* note the new break position */
        item_ = item;
        pos_ = pos;

        /* note the run of whitespace containing the break position */
        brk_ws_start_item_ = ws_start_item_;
        brk_ws_start_pos_ = ws_start_pos_;
    }

    /* note a character, for the whitespace run calculation */
    void note_char(textchar_t ch, class CHtmlDisp *item, void *pos)
    {
        /* note the whitspace or non-whitespace, as appropriate */
        if (is_space(ch))
            note_ws(item, pos);
        else
            note_non_ws();
    }

    /* 
     *   set the current whitespace position, if it's not already set - this
     *   can be called each time whitespace is encountered to set the start
     *   of a whitespace run if we're not already in a whitespace run 
     */
    void note_ws(class CHtmlDisp *item, void *pos)
    {
        /* if we're not already in a run, this is the start of a run */
        if (ws_start_item_ == 0)
        {
            /* remember the given item and position as the start of a run */
            ws_start_item_ = item;
            ws_start_pos_ = pos;
        }
    }

    /* 
     *   clear the whitespace run start - this can be called each time a
     *   non-whitespace character is encountered to note that we're not in a
     *   run of whitespace 
     */
    void note_non_ws() { ws_start_item_ = 0; }

    /*
     *   Display object containing the line break.  Whenever a display
     *   object identifies a line break, it should store a pointer to
     *   itself here, so that as soon as we exceed the display width we
     *   can go back to the last break point.  
     */
    class CHtmlDisp *item_;

    /*
     *   position pointer, for use of display object - when a display
     *   object finds a suitable break position, it can use this to
     *   identify the internal location of the break 
     */
    void *pos_;

    /* 
     *   The item and position of the run of whitespace containing the
     *   current break position.  Each time we accept a new break position,
     *   we copy the current whitespace run start point into these members;
     *   this way, we can tell if a subsequent break position is part of the
     *   same run of whitespace by comparing these to the current whitespace
     *   start point.  
     */
    class CHtmlDisp *brk_ws_start_item_;
    void *brk_ws_start_pos_;

    /*
     *   Last character of previous item, for break purposes.  Even when
     *   we don't find any breaks in an item, the item should set this
     *   variable so that the next item can determine whether it's
     *   possible to break between the two items.  If an item doesn't
     *   contain text, it should set this to a character value that allows
     *   the next item to determine if a break is allowed between the two
     *   items; setting this to a space always allows a break, whereas
     *   setting it to an alphabetic character prevents a break it
     *   non-whitespace text follows.  
     */
    textchar_t last_char_;

    /* 
     *   No-break flag: this indicates that we have an explicitly no-break
     *   indication just before the current position, which means that we
     *   can't break to the left of the current position, even if the next
     *   item would normally allow a break here.  
     */
    int no_break_;

    /*
     *   Display item and position giving the start of the last run of
     *   whitespace we encountered.  We'll set these to null every time we
     *   encounter anything other than whitespace, and set them to the
     *   current item and position every time they're null and we find a
     *   whitespace character.  The line breaking routine can use these to
     *   go back and remove visual trailing whitespace after breaking the
     *   line.  
     */
    class CHtmlDisp *ws_start_item_;
    void *ws_start_pos_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Table cell metrics measurement object.  This object is used to
 *   measure the minimum and maximum widths of items in a table cell.  
 */
class CHtmlTableMetrics
{
public:
    /* clear metrics - set up for the start of a new cell */
    void clear()
    {
        last_char_ = 0;
        leftover_width_ = 0;
        cur_line_width_ = 0;
        min_width_ = max_width_ = 0;
        no_break_ = FALSE;
        no_break_sect_ = FALSE;
    }

    /*
     *   Add an indivisible item (i.e., an item that can't be broken up
     *   across lines).  If this item is wider than the widest indivisible
     *   item so far, we'll update the minimum width to reflect this
     *   item's width.  This does not affect the total length of the
     *   current line; the item must be separately added to the total line
     *   width if appropriate.  
     */
    void add_indivisible(long wid)
    {
        if (wid > min_width_)
            min_width_ = wid;
    }

    /*
     *   Add to the size of the current line.  If this makes the current
     *   line longer than the maximum width so far, we'll update the
     *   maximum width to reflect the width of this line. 
     */
    void add_to_cur_line(long wid)
    {
        cur_line_width_ += wid;
        if (cur_line_width_ > max_width_)
            max_width_ = cur_line_width_;
    }

    /*
     *   Clear the leftover width.  This should be called any time an item
     *   doesn't need to use or aggregate any space into the leftover
     *   width.  This will add any current leftover width to the current
     *   line and indivisible widths, and clear the leftover. 
     */
    void clear_leftover()
    {
        /* add the leftover to the indivisible width and line width */
        add_indivisible(leftover_width_);
        add_to_cur_line(leftover_width_);

        /* forget the leftover width */
        leftover_width_ = 0;
    }

    /*
     *   Start a new line.  Resets the current line's width to zero. 
     */
    void start_new_line()
    {
        /* 
         *   if there's any leftover width, add it to both the indivisible
         *   width and the total line width 
         */
        add_to_cur_line(leftover_width_);
        add_indivisible(leftover_width_);
        
        /* forget the current line width and leftover width */
        cur_line_width_ = 0;
        leftover_width_ = 0;

        /* there is no previous character on the line */
        last_char_ = 0;

        /* there's no no-break flag yet */
        no_break_ = FALSE;
    }

    /*
     *   Minimum width so far.  This is the size of the largest single
     *   item that can't be broken across lines; since this item can't be
     *   broken, its width is the smallest we can go without clipping
     *   anything. 
     */
    long min_width_;

    /*
     *   Maximum width so far.  This is the size of the longest line.
     *   This is the widest that we'd have to go to avoid having to break
     *   up any lines.  
     */
    long max_width_;

    /*
     *   Size of the current line so far.  Each time we encounter a line
     *   break, we'll reset this to zero. 
     */
    long cur_line_width_;

    /* 
     *   last character of previous item, for break purposes (see the
     *   notes on the last_char_ member of CHtmlLineBreak for details on
     *   how this works) 
     */
    textchar_t last_char_;

    /*
     *   Leftover width from previous item - this is the amount of space
     *   left over after the last break in the previous item.  If we can't
     *   insert a break between the end of the previous item and the start
     *   of the current item, this leftover space must be added to the
     *   space at the start of the current item.  
     */
    long leftover_width_;

    /* flag: we just passed an explicit no-break item (such as an &nbsp;) */
    unsigned int no_break_ : 1;

    /* flag: we're in a <NOBR> section */
    unsigned int no_break_sect_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Line-start table.  We need to be able to find line starts quickly,
 *   so that we can navigate from a screen coordinate (such as might come
 *   from a mouse click) to a display object.  To facilitate quick
 *   searching by position, we keep an array of the display items that are
 *   first in their lines, and we keep the array in order of vertical line
 *   position.  Since the number of lines could grow quite large, we keep
 *   a two-level array: the top-level array keeps pointers to the
 *   second-level arrays, which in turn keep pointers to the line starts.
 *   The second-level arrays are fixed-size, so it's a simple calculation
 *   to determine which second-level array contains a given index, and the
 *   offset of the line within the second-level array.  On storing a new
 *   item, we ensure that the array is large enough to hold the item, so
 *   the caller is not responsible for doing any initialization or bounds
 *   checking on storing a value.  However, note that we don't check items
 *   on retrieval to ensure that they're within bounds or have been
 *   initialized; the caller is responsible for checking that items
 *   retrieved have actually been stored.  
 */
const int HTML_LINE_START_PAGESIZE = 1024;  /* pointers in a 2nd-level page */
const int HTML_LINE_START_SHIFT = 10;                     /* log2(pagesize) */

struct CHtmlLineStartEntry
{
    long ypos;
    class CHtmlDisp *disp;
};

/*
 *   line start searcher class 
 */
class CHtmlLineStartSearcher
{
public:
    virtual ~CHtmlLineStartSearcher() {}

    /*
     *   true -> the item we're seeking is between the two given items; b
     *   == 0 means that a is the last entry 
     */
    virtual int is_between(const CHtmlLineStartEntry *a,
                           const CHtmlLineStartEntry *b) const = 0;

    /*
     *   true -> item is too low - we're looking for something higher 
     */
    virtual int is_low(const CHtmlLineStartEntry *item) const = 0;
};

class CHtmlLineStarts
{
public:
    CHtmlLineStarts();
    ~CHtmlLineStarts();

    /* add an entry at a given index with a given line start position */
    void add(long index, class CHtmlDisp *item, long y);

    /* get an entry at a given index */
    class CHtmlDisp *get(long index) const
    {
        return (index >= count_ ? 0 : get_internal(index)->disp);
    }

    /* get the y position of a line */
    long get_ypos(long index) const
    {
        return (index >= count_ ? 0 : get_internal(index)->ypos);
    }

    /* change the y position of a line */
    void set_ypos(long index, long ypos)
    {
        if (index < count_)
            get_internal(index)->ypos = ypos;
    }

    /* forget about all of our entries */
    void clear_count() { count_ = 0; }

    /* get the current count of line starts */
    long get_count() const { return count_; }

    /* restore the count to a setting previously noted with get_count */
    void restore_count(long count) { count_ = count; }

    /*
     *   Get the index of the entry that contains a given y offset.  This
     *   will find the entry with the greatest y offset less than or equal
     *   to the given y offset. 
     */
    long find_by_ypos(long ypos) const;

    /*
     *   Get the index of the entry that contains a given text offset 
     */
    long find_by_txtofs(unsigned long txtofs) const;

    /*
     *   Get the index of the entry at the given debugger source file
     *   position.  This is used only for debug source windows. 
     */
    long find_by_debugsrcpos(unsigned long srcfpos) const;

    /* 
     *   General finder routine, using the given searcher to determine the
     *   ordering.  Performs a binary search of our line start array using
     *   the given searcher, and returns the index of the item we found.  
     */
    long find(const CHtmlLineStartSearcher *searcher) const;

private:
    /* get the internal entry for an item */
    CHtmlLineStartEntry *get_internal(long index) const
    {
        unsigned int page;
        unsigned int offset;

        calc_location(index, &page, &offset);
        return *(pages_ + page) + offset;
    }

    /*
     *   calculate the second-level array containing the item, and the
     *   item's index within the second-level array 
     */
    void calc_location(long index, unsigned int *page,
                       unsigned int *offset) const
    {
        *page = (unsigned int)(index >> HTML_LINE_START_SHIFT);
        *offset = (unsigned int)(index & (HTML_LINE_START_PAGESIZE - 1));
    }

    /* ensure that a particular page is allocated */
    void ensure_page(unsigned int page);

    /* the top-level array */
    CHtmlLineStartEntry **pages_;

    /* size of the top-level array */
    size_t top_pages_allocated_;

    /* number of second-level pages allocated */
    size_t second_pages_allocated_;

    /*
     *   number of items stored so far (actually, the maximum index plus
     *   one, since the caller may not fill in all items) 
     */
    long count_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Flow-around item stack.  A flow-around item is an item in the left or
 *   right margin that text flows around.  To control the text, we
 *   increase the margins temporarily, until we get past the bottom of the
 *   object in the margin.  This stack tracks the objects that we're
 *   currently flowing text around; there's one of these for each margin. 
 */

/* flow stack item */
class CHtmlFmtFlowStkItem
{
public:
    /* 
     *   margin setting in effect before this item was pushed, stored as a
     *   delta from the new margin set for the item 
     */
    long old_margin_delta;

    /* y position of bottom of this flow item */
    long bottom;

    /* 
     *   table active when stack position was entered - we cannot leave this
     *   stack level unless this same table is active 
     */
    class CHtmlTagTABLE *cur_table;
};

/* flow stack */
class CHtmlFmtFlowStack
{
public:
    CHtmlFmtFlowStack()
    {
        /* nothing on the stack yet */
        sp = 0;
    }

    /* push the current margins */
    void push(long old_margin_delta, long bottom,
              class CHtmlTagTABLE *cur_table);

    /* get the item at the top of the stack */
    const CHtmlFmtFlowStkItem *get_top() const { return &stk[sp - 1]; }

    /* determine if there's anything on the stack */
    int is_empty() const { return sp == 0; }

    /* pop the top item */
    void pop();

    /* determine if we're past the end of the item at the top of the stack */
    int is_past_end(long ypos, class CHtmlTagTABLE *cur_table) const;

    /* 
     *   determine if it's legal to end this item, based on the current
     *   table nesting level 
     */
    int can_end_item(const CHtmlFmtFlowStkItem *item,
                     const class CHtmlTagTABLE *cur_table) const;

    /* determine if it's legal to end an item given its index */
    int can_end_item(int index, class CHtmlTagTABLE *cur_table) const
        { return can_end_item(get_by_index(index), cur_table); }

    /* determine if we can end the top item */
    int can_end_top_item(class CHtmlTagTABLE *cur_table) const
        { return !is_empty() && can_end_item(get_top(), cur_table); }

    /* get the number of items on the stack */
    int get_count() const { return sp; }

    /* 
     *   get an item at an index from the top - 0 is the top, 1 is the
     *   next item, and so on 
     */
    const CHtmlFmtFlowStkItem *get_by_index(int index) const
        { return &stk[sp - index - 1]; }

    /* reset - discard all items */
    void reset() { sp = 0; }

private:
    /* stack pointer */
    size_t sp;

    /* stack */
    CHtmlFmtFlowStkItem stk[100];
};

/*
 *   Table formatting environment snapshot.  This records the necessary
 *   information to resume formatting after finishing with a floating table.
 */
class CHtmlFmtTableEnvSave
{
public:
    /* left/right stack depths */
    int left_count;
    int right_count;

    /* left/right margin settings at the current stack level */
    long margin_left;
    long margin_right;

    /* output position and status */
    CHtmlDisp *disp_tail;
    CHtmlDisp *line_head;
    long max_line_width;
    long avail_line_width;
    CHtmlPoint curpos;
    int line_spacing;
    long line_count;
    long line_id;
    long line_starts_count;
    int last_was_newline : 1;
    
};

/* maximum table environment save depth */
const size_t CHTMLFMT_TABLE_ENV_SAVE_MAX = 20;


/* ------------------------------------------------------------------------ */
/*
 *   formatter heap page structure 
 */
struct CHtmlFmt_heap_page_t
{
    unsigned char *mem;
    size_t siz;
};

/* ------------------------------------------------------------------------ */
/*
 *   Display item factory.  Certain types of display items come in
 *   linkable and unlinkable varieties.  When we're in a link, we need to
 *   create a linkable type, and at other times we need to create the
 *   normal unlinkable kind.  To simplify and centralize these decisions,
 *   we maintain a display item factory that creates the appropriate types
 *   of objects.  The current item factory should always be used when
 *   creating an object; it will create the appropriate type of objects
 *   for the current context.  Note that most display items are not
 *   linkable at all, so there's no need to go through the factory -- it's
 *   only needed where both linkable and unlinkable types exist.  
 */
class CHtmlFmtDispFactory
{
public:
    CHtmlFmtDispFactory(class CHtmlFormatter *formatter)
    {
        formatter_ = formatter;
    }

    virtual ~CHtmlFmtDispFactory() {}

    /* create a text display item */
    virtual CHtmlDisp *new_disp_text(class CHtmlSysWin *win,
                                     class CHtmlSysFont *font,
                                     const textchar_t *txt, size_t txtlen,
                                     unsigned long txtofs) = 0;

    /* create an input-editor text display item */
    virtual class CHtmlDispTextInput *new_disp_text_input(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs) = 0;

    /* create a soft-hyphen display item */
    virtual class CHtmlDisp *new_disp_soft_hyphen(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs) = 0;

    /* create a non-breaking space display item */
    virtual class CHtmlDisp *new_disp_nbsp(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs) = 0;

    /* create an special typographical space display item */
    virtual class CHtmlDisp *new_disp_space(
        class CHtmlSysWin *win, class CHtmlSysFont *font,
        const textchar_t *txt, size_t txtlen, unsigned long txtofs,
        int wid) = 0;

    /* create a pre-formatted text display item */
    virtual CHtmlDisp *new_disp_text_pre(class CHtmlSysWin *win,
                                         class CHtmlSysFont *font,
                                         const textchar_t *txt,
                                         size_t txtlen,
                                         unsigned long txtofs) = 0;

    /* create a debugger-window text display item */
    virtual CHtmlDisp *new_disp_text_debug(class CHtmlSysWin *win,
                                           class CHtmlSysFont *font,
                                           const textchar_t *txt,
                                           size_t txtlen,
                                           unsigned long txtofs,
                                           unsigned long linenum) = 0;

    /* create an image display item */
    virtual class CHtmlDispImg *new_disp_img(
        class CHtmlSysWin *win, class CHtmlResCacheObject *image,
        CStringBuf *alt, HTML_Attrib_id_t align, long hspace, long vspace,
        long width, int width_set, long height, int height_set,
        long border, class CHtmlUrl *usemap, int ismap) = 0;

protected:
    class CHtmlFormatter *formatter_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Formatter 
 */

/* maximum depth of margin stack */
const int MARGIN_STACK_DEPTH_MAX = 20;

class CHtmlFormatter
{
public:
    CHtmlFormatter(class CHtmlParser *parser);
    virtual ~CHtmlFormatter();

    /* 
     *   get/set T3 mode - this indicates that our caller is a T3 program,
     *   which changes certain parsing rules 
     */
    int is_t3_mode() const { return t3_mode_; }
    void set_t3_mode(int f) { t3_mode_ = f; }

    /* 
     *   receive notification that the parser is about to be deleted; we
     *   must release any references we have to the parser or the format
     *   tag list (which is owned by the parser, so will be deleted when
     *   the parser is deleted).  
     */
    virtual void release_parser()
    {
        /* forget the parser */
        parser_ = 0;
    }

    /* set my window and the physical margins to use */
    virtual void set_win(class CHtmlSysWin *win, const CHtmlRect *r);

    /* 
     *   Receive notification that my window is being deleted.  By
     *   default, I'll just forget the window. 
     */
    virtual void unset_win();

    /* get my window pointer */
    class CHtmlSysWin *get_win() const { return win_; }

    /* invalidate the whole window area */
    void inval_window();

    /* get the special BODY display item */
    class CHtmlDispBody *get_body_disp() const{ return body_disp_; }

    /* get the resource cache (a global singleton) */
    static class CHtmlResCache *get_res_cache() { return res_cache_; }

    /* get the resource finder (a global singleton) */
    static class CHtmlResFinder *get_res_finder() { return res_finder_; }

    /* play a sound */
    void play_sound(class CHtmlTagSOUND *tag,
                    HTML_Attrib_id_t layer,
                    class CHtmlResCacheObject *res,
                    int repeat_count, int random_start,
                    HTML_Attrib_id_t sequence);

    /* stop a sound */
    virtual void cancel_sound(HTML_Attrib_id_t layer);

    /* terminate all active playback (such as animations) */
    void cancel_playback();

    /* get/set the last sound tag */
    virtual class CHtmlTagSOUND *get_last_sound_tag() const = 0;
    virtual void set_last_sound_tag(class CHtmlTagSOUND *tag) = 0;

    /* do we allow <SOUND> tags in this window? */
    virtual int allow_sound_tags() const = 0;

    /*
     *   Flush the text buffer.  By default, we do nothing, since the basic
     *   formatter doesn't own its own text buffer.  This can be overridden
     *   for formatter subclasses that do own an independent text buffer.  
     */
    virtual void flush_txtbuf(int /*fmt*/) { }

    /*
     *   Add an extra line's worth of vertical whitespace to the display
     *   height, beyond what's strictly needed for the layout.  This can be
     *   used to provide visual feedback after a user presses Enter at the
     *   end of an input line.  We simply add one line height worth of space
     *   to the display height and refigure the scrollbar positions, which
     *   will generally cause the display window to scroll one line.  
     */
    void add_line_to_disp_height();

    /*
     *   Set the "physical" margins.  The window should call this if it
     *   wants to specify an inset from the window coordinates, to provide
     *   some whitespace around the edges of the window.  The bottom and
     *   right coordinates in the rectangle are inset amounts, so they
     *   will usually be small positive numbers (.right probably will
     *   equal .left).  Note that the window generally won't need to
     *   change this after it's initially set up, which happens when it
     *   plugs in to the formatter with set_win().  
     */
    void set_phys_margins(const CHtmlRect *r);

    /* get the physical margin settings */
    CHtmlRect get_phys_margins() const { return phys_margins_; }

    /* set the physical margins */
    void set_phys_margins(long left, long top, long right, long bottom)
        { phys_margins_.set(left, top, right, bottom); }

    /*
     *   Re-start rendering at the top of the document.  This should be
     *   called whenever the current formatting becomes invalid, such as
     *   when the window is resized horizontally.
     *   
     *   If reset_sounds is true, we'll stop all currently playing sounds
     *   and start over with the <SOUND> tags that we encounter in the
     *   course of formatting the page.  If reset_sounds is false, we'll
     *   leave current sounds playing and ignore all <SOUND> tags that we
     *   encounter.  reset_sounds should be false whenever we're
     *   reformatting the current page (for example, after resizing the
     *   window or changing the default font size), and should be true
     *   when switching to a new page.  
     */
    virtual void start_at_top(int reset_sounds);

    /*
     *   Do some more formatting.  This routine does a little work and
     *   then returns, so that an interactive program can check for events
     *   and respond to user actions while formatting is in progress.  A
     *   non-interactive program can simply call this routine repeatedly
     *   as long as more_to_do() returns true.  An interactive program can
     *   monitor the progress of the formatting with getcurpos();
     *   normally, an interactive program will want to do formatting until
     *   it has enough to fill the current window (or until the formatting
     *   is done, whichever comes first), then process user events while
     *   formatting any additional text in the background, since further
     *   text is out of view at the moment.  
     */
    void do_formatting();

    /* do we have more rendering left to do? */
    int more_to_do();

    /* get the current rendering position */
    CHtmlPoint getcurpos() const { return curpos_; }

    /* 
     *   get the maximum y position so far (i.e., the display height of the
     *   page) 
     */
    unsigned long get_max_y_pos() const { return disp_max_y_pos_; }

    /* get the maximum y position assigned to a layout item so far */
    unsigned long get_layout_max_y_pos() const { return layout_max_y_pos_; }

    /*
     *   Increase the page height by a given amount (which can be negative
     *   to decrease the position).  This should be used only for temporary
     *   adjustments to the scrolling area during times when new text won't
     *   be added, since the new y position will affect future text
     *   formatting if not undone.  
     */
    void inc_max_y_pos(long amount) { disp_max_y_pos_ += amount; }

    /*
     *   Add an item to the tail of the display list, adjust the current
     *   line under construction to compensate for the new item.  
     */
    virtual void add_disp_item(class CHtmlDisp *item);

    /*
     *   Add a link display item.  To enter a hypertext link (<A HREF>),
     *   create a link item and call this routine.  Subsequent linkable
     *   items will refer back to this link object.  Call end_link() when
     *   the matching </A> is reached.
     */
    void add_disp_item_link(class CHtmlDispLink *link);

    /* end the current link (opened with add_disp_item_link()) */
    void end_link();

    /* 
     *   Get/set character-wrapping mode.  The default is word-wrap mode;
     *   when character-wrap mode is off, word-wrap mode is selected.  
     */
    int is_char_wrap_mode() const { return char_wrap_mode_; }
    void set_char_wrap_mode(int f)
    {
        /* remember the new wrap mode */
        char_wrap_mode_ = f;

        /* set the correct factory based on the new mode */
        adjust_disp_factory();
    }

    /* get the current link item */
    class CHtmlDispLink *get_cur_link() const { return cur_link_; }

    /* get the current display item factory */
    CHtmlFmtDispFactory *get_disp_factory() const
        { return cur_disp_factory_; }

    /*
     *   Add an item to the tail of the display list, replacing the
     *   current command input display item(s), if any.  This is used
     *   during editing of a command line to update the display after the
     *   user has made a change in the contents of the command line.
     *   Default formatters are not input-capable, so we simply add the
     *   display item as though it were any other display item.
     *   Input-capable formatter subclasses should override this to
     *   provide proper input editing formatting.  
     */
    virtual void add_disp_item_input(class CHtmlDispTextInput *item,
                                     class CHtmlTagTextInput *);

    /*
     *   Add a display item that floats to the left or right margin with
     *   text flowing around it.  We can't add this type of item
     *   immediately; instead, we need to wait until we start a new line,
     *   at which point we can add the item and adjust the margins so that
     *   text flows around the item.  This routine adds the item to an
     *   internal list of items to be deferred until the start of the next
     *   line.  
     */
    void add_disp_item_deferred(class CHtmlDisp *item);

    /* Add an item, starting a new line with the item */
    void add_disp_item_new_line(class CHtmlDisp *item);

    /* Add an item, starting a new line immediately after the item. */
    void add_disp_item_new_line_after(class CHtmlDisp *item);

    /* Add a <DIV> */
    void begin_div(class CHtmlDispDIV *div);

    /* end the current <DIV> */
    void end_div();

    /* define a tab stop at the current position */
    void set_tab(const textchar_t *tab_id, HTML_Attrib_id_t align,
                 textchar_t dp);

    /* tab to a previously-defined tab stop */
    void tab_to(class CHtmlTagTAB *tag);

    /*
     *   Add minimum line spacing.  This makes sure that we're starting a
     *   new line, and that the line spacing for the new line is at least
     *   as much as requested.  If we're already on a new line and have
     *   provided the requested amount of inter-line spacing, this doesn't
     *   do anything. 
     */
    void add_line_spacing(int spacing);

    /*
     *   Add vertical whitespace as needed to move past floating items in
     *   one or both margins.  Returns true if any line spacing was added,
     *   false if not.  
     */
    int break_to_clear(int left, int right);

    /* get current font */
    class CHtmlSysFont *get_font() const { return curfont_; }

    /* set a new font */
    void set_font(class CHtmlSysFont *font) { curfont_ = font; }

    /* get/set the current base font size */
    int get_font_base_size() const { return font_base_size_; }
    void set_font_base_size(int siz) { font_base_size_ = siz; }

    /*
     *   Draw everything in a given area.  If clip_lines is false, we'll
     *   draw the last line even if it doesn't fit entirely in the area,
     *   otherwise we'll stop when we reach a line that doesn't entirely
     *   fit.  If clip_lines is true, we'll return in clip_y the y
     *   position of the first line we didn't draw.  
     */
    void draw(const CHtmlRect *area, int clip_lines, long *clip_y);

    /*
     *   Invalidate links that are visible on the screen.  This is used to
     *   quickly redraw the window's links (and only its links) for a
     *   short-term change in the link visibility state.  This can be used
     *   to implement a mode where links are highlighted only if a key is
     *   held down.  
     */
    void inval_links_on_screen(const CHtmlRect *area);

    /* invalidate the display area occupied by the selected text */
    void inval_sel_range()
    {
        /* if the selection range isn't empty, invalidate it */
        if (sel_start_ != sel_end_)
            inval_offset_range(sel_start_, sel_end_);
    }

    /* get the maximum line width */
    long get_max_line_width() const { return max_line_width_; }

    /* 
     *   Get the maximum line width at the outer level.  If we're inside a
     *   table, we'll return the outermost enclosing max line width;
     *   otherwise, we'll return the current maximum line width.  The
     *   outer maximum line width is useful for calculating the horizontal
     *   scrollbar range; the current line width is not interesting for
     *   scrollbars if it is ultimately wrapped within a table.  
     */
    long get_outer_max_line_width() const { return outer_max_line_width_; }

    /* 
     *   Extract text from the text stream into the given buffer.  Pulls the
     *   text from the given starting text offset to the given ending text
     *   offset.  
     */
    void extract_text(CStringBuf *buf,
                      unsigned long start_ofs, unsigned long end_ofs) const;

    /* get the height of the line at the given y position */
    long get_line_height_ypos(long ypos) const;

    /*
     *   Get the position of a character, given the text offset in the
     *   character array 
     */
    CHtmlPoint get_text_pos(unsigned long txtofs) const;

    /*
     *   Get the text offset, given a position in doc coordinates 
     */
    unsigned long find_textofs_by_pos(CHtmlPoint pos) const;

    /*
     *   Given a text offset, get the extent of the word containing the
     *   offset 
     */
    void get_word_limits(unsigned long *start, unsigned long *end,
                         unsigned long txtofs) const;

    /*
     *   Given a text offset, get the extent of the line containing the
     *   offset.  If the offset is in the current input line, we'll get
     *   the limits of the input line, not including any prompt that
     *   appears on the same line. 
     */
    void get_line_limits(unsigned long *start, unsigned long *end,
                         unsigned long txtofs) const;

    /*
     *   Find an object by position.  If 'exact' is true, we'll return null
     *   if we don't find an item that actually contains the given item;
     *   otherwise, we'll return the item at the start of the next line after
     *   the given position.  
     */
    class CHtmlDisp *find_by_pos(CHtmlPoint pos, int exact) const;

    /*
     *   Find the <DIV> tag that covers the area containing the given
     *   position.  
     */
    class CHtmlDispDIV *find_div_by_pos(CHtmlPoint pos) const;

    /* 
     *   Find a <DIV> tag by text offset.  The vertical coordinate must be
     *   provided because the text offset isn't quite precise enough - it
     *   could lead us to include areas to the right of the line above the
     *   start of the division's vertical extent.  
     */
    class CHtmlDispDIV *find_div_by_ofs(unsigned long txtofs, long y) const;

    /*
     *   Find an object by text offset.  If we don't find an item
     *   containing the exact position, we'll check use_prv and use_nxt:
     *   if use_prv is true, we'll return the last item on the previous
     *   line; otherwise, if use_nxt is true, we'll return the first item
     *   on the next line; otherwise we'll return null.  
     */
    class CHtmlDisp *find_by_txtofs(unsigned long ofs, int use_prv,
                                    int use_nxt) const;

    /*
     *   Find an object given a debugger source file seek position.  This
     *   is used only for debugger source windows. 
     */
    class CHtmlDisp *find_by_debugsrcpos(unsigned long srcfpos) const;

    /*
     *   Get the line number given the debugger source position 
     */
    long findidx_by_debugsrcpos(unsigned long srcpos) const;

    /*
     *   Get the item given a line number 
     */
    class CHtmlDisp *get_disp_by_linenum(long linenum) const;

    /* get the first display item in the display list */
    class CHtmlDisp *get_first_disp() const { return disp_head_; }

    /* set the selection range given the starting and ending text offsets */
    void set_sel_range(unsigned long start, unsigned long end);

    /*
     *   Set the selection range given the positions of the starting and
     *   ending points.  If startofs or endofs are non-null, we'll fill
     *   these in with the text offsets of the start and end points; these
     *   will correspond to the start and end positions given, so they
     *   will not necessarily be in canonical order.  
     */
    void set_sel_range(CHtmlPoint start, CHtmlPoint end,
                       unsigned long *startofs, unsigned long *endofs);

    /* get the selection range */
    void get_sel_range(unsigned long *start, unsigned long *end)
        { *start = sel_start_; *end = sel_end_; }

    /* 
     *   enter/exit <TITLE> capture mode - this captures (or ignores) the
     *   contents of the tag 
     */
    virtual void begin_title() { title_mode_ = TRUE; }
    virtual void end_title(class CHtmlTagTITLE *) { title_mode_ = FALSE; }

    /*
     *   Enter/exit a MAP.  When the map is open, we can add AREA values
     *   to the map.  
     */
    void begin_map(const textchar_t *map_name, size_t name_len);
    void end_map();

    /*
     *   Add a hot spot area to the current map.  If href is null, it
     *   indicates that the hot spot is a NOHREF area.  'append' and
     *   'noenter' specify the APPEND and NOENTER attribute settings for
     *   the link.  
     */
    void add_map_area(HTML_Attrib_id_t shape,
                      CHtmlUrl *href,
                      const textchar_t *alt, size_t altlen,
                      const struct CHtmlTagAREA_coords_t *coords,
                      int coord_cnt, int append, int noenter);

    /* get a map given a map name */
    class CHtmlFmtMap *get_map(const textchar_t *mapname, size_t namelen);

    /*
     *   Begin a table.  Returns the enclosing table.  The caller is
     *   responsible for restoring the enclosing table with a call to
     *   end_table() when this table is finished.  *enclosing_table_pos
     *   will be filled in with the position of the enclosing table; this
     *   must be passed to end_table_set_size() to restore the enclosing
     *   table position at the end of the table.  
     */
    class CHtmlTagTABLE *begin_table(class CHtmlTagTABLE *table_tag,
                                     class CHtmlDispTable *table_disp,
                                     CHtmlPoint *enclosing_table_pos,
                                     class CHtmlTagCAPTION *caption_tag);
    /*
     *   End the current table, and restore the enclosing table (which was
     *   returned from the earlier corresponding call to begin_table()). 
     */
    void end_table(class CHtmlDispTable *table_disp,
                   class CHtmlTagTABLE *enclosing_table,
                   const CHtmlPoint *enclosing_table_pos,
                   class CHtmlTagCAPTION *caption_tag);

    /*
     *   Set the total size of the current table.  The <TABLE> tag will
     *   call this after it's calculated the total size of the table.  For
     *   the outermost table, we'll use this to set the position of the
     *   next item after the table.  enclosing_table_pos is the position
     *   of the enclosing table that was returned from the corresponding
     *   call to begin_table().  
     */
    void end_table_set_size(class CHtmlTagTABLE *enclosing_table,
                            long height,
                            const CHtmlPoint *enclosing_table_pos);

    /*
     *   Begin a table cell.  On table pass 2, this will set up the
     *   formatter's margins according to the column settings. 
     */
    void begin_table_cell(class CHtmlDispTableCell *cell_disp,
                          long cell_x_offset,
                          long content_x_offset, long content_width);

    /*
     *   Prepare to close out a table cell.  This will add any pending
     *   deferred items immediately, and clear any marginal floating
     *   items. 
     */
    void pre_end_table_cell();

    /*
     *   End the current table cell.  On table pass 1, this will calculate
     *   the minimum and maximum width of the cell's contents, and tell
     *   the cell about it. 
     */
    void end_table_cell(class CHtmlTagTableCell *cell_tag,
                        class CHtmlDispTableCell *cell_disp);

    /*
     *   Set the y positions of the contents of a table cell.
     *   row_y_offset and cell_y_offset are offsets from the top of the
     *   table.  base_y_pos is the original base position of the cell; the
     *   vertical offsets of the cell's contents from this position are
     *   preserved, so that the cell contents end up at the same offset
     *   from the new position of the cell.  disp_first and disp_last are
     *   the first and last display items in the cell; disp_first must be
     *   the display item for the cell itself.  
     */
    void set_table_cell_y_pos(class CHtmlDisp *disp_first,
                              class CHtmlDisp *disp_last,
                              long cell_y_offset, long base_y_pos,
                              long row_y_offset);

    /*
     *   freeze display adjustment - this can be used when a large
     *   formatting job is about to be done, and the caller wants to wait
     *   until the whole job is done to redraw the display 
     */
    void freeze_display(int flag) { freeze_display_adjust_ = flag; }

    /* check if we're at the beginning of a line */
    int last_was_newline() { return last_was_newline_; }

    /* 
     *   Start text flow around the given item.  The new margins remain in
     *   effect until we reach the y position given by flow_bottom.  The
     *   item must not be part of the display list when this is called.  
     */
    void begin_flow_around(class CHtmlDisp *item,
                           long new_left_margin, long new_right_margin,
                           long flow_bottom);

    /*
     *   Push a margin change.  The margins will be set to the new sizes;
     *   note that the right margin is an amount to inset the margin from
     *   the right.  These margins will remain in effect until the margin
     *   settings are popped.  
     */
    void push_margins(long left, long right);

    /*
     *   Set temporary margins.  The margins set here will be in effect
     *   until the end of the current line, at which time we'll revert to
     *   the normal margins.  This can be used to indent the first line of
     *   a paragraph, or un-indent the bullet of a list item. 
     */
    void set_temp_margins(long left, long right);

    /*
     *   Restore margins that were in effect before the most recent call
     *   to push_margins().  These calls nest, two pushes must be matched
     *   by two pops. 
     */
    void pop_margins();

    /* get the current left and right margin settings */
    long get_left_margin() const { return margin_left_; }
    long get_right_margin() const { return margin_right_; }

    /* 
     *   Get the current top and bottom margin settings.  Note that we
     *   don't have our own vertical margins; these are simply the
     *   "physical" vertical insets that the window says we should use. 
     */
    long get_top_margin() const { return phys_margins_.top; }
    long get_bottom_margin() const { return phys_margins_.bottom; }

    /* turn line wrapping on or off */
    void set_line_wrapping(int wrap) { wrap_lines_ = wrap; }

    /* get line wrapping state */
    int get_line_wrapping() { return wrap_lines_; }

    /*
     *   Get/set the division alignment.  The enclosing division
     *   alignment is used to align a block element whenever the block
     *   element doesn't specify an explicit alignment of its own.
     */
    HTML_Attrib_id_t get_div_alignment() const { return div_alignment_; }
    void set_div_alignment(HTML_Attrib_id_t align) { div_alignment_ = align; }

    /*
     *   Get/set the block alignment.  This overrides the division
     *   alignment if set to a valid value. 
     */
    HTML_Attrib_id_t get_blk_alignment() const { return blk_alignment_; }
    void set_blk_alignment(HTML_Attrib_id_t align) { blk_alignment_ = align; }

    /*
     *   Start formatting a banner.  Default formatters can't create
     *   banners, so this routine simply ignores the request, which will
     *   cause the banner contents to appear in the same window as their
     *   surrounding markups.  If height_set is true, the banner height
     *   should be set to the given height; otherwise, if height_prev is
     *   true, the banner height should be set to the same height it had
     *   previously.  If neither height_set nor height_prev is true, or
     *   height_prev is true but there is no previous instance of the
     *   banner, the banner is given the same height as its contents.
     *   Returns true if the banner was successfully formatted, false if
     *   not.  
     */
    virtual int format_banner(const textchar_t * /*banner_id*/,
                              long /*height*/, int /*height_set*/,
                              int /*height_prev*/, int /*height_pct*/,
                              long /*width*/, int /*width_set*/,
                              int /*width_prev*/, int /*width_pct*/,
                              class CHtmlTagBANNER *)
    {
        /* by default, we can't handle banners */
        return FALSE;
    }

    /* 
     *   Receive notification that we've reached the end of the banner.
     *   By default, we ignore this, since the default formatter isn't
     *   itself a banner. 
     */
    virtual void end_banner() { }

    /*
     *   Remove a banner.  By default, we don't support banners, so there
     *   can't be anything to remove. 
     */
    virtual void remove_banner(const textchar_t * /*banner_id*/) { }

    /*
     *   Remove all in-line banners.  This should be used before switching to
     *   another history page, since each history page may have a unique set
     *   of banners.  The default implementation does nothing, since default
     *   formatters don't support banners at all.  
     */
    virtual void remove_all_banners(int /*replacing*/) { }

    /*
     *   Set/get the continuation sequence number for ordered lists.  This
     *   allows a new ordered list to pick up numbering where the previous
     *   ordered list left off. 
     */
    void set_ol_continue_seqnum(long val) { ol_cont_seqnum_ = val; }
    long get_ol_continue_seqnum() const { return ol_cont_seqnum_; }

    /*
     *   Formatter heap management.  The formatter maintains its own heap
     *   for allocating memory for display items, since display items have
     *   particular behavior that we can exploit to achieve a more
     *   efficient heap implementation.  In particular, the display list
     *   can be treated as a single large chunk of memory, so we can have
     *   a very simple allocator that doesn't allow a chunk to be
     *   individually deleted, thus saving the overhead the
     *   general-purpose allocator needs to keep track of individual
     *   allocation units. 
     */
    void *heap_alloc(size_t siz);

    /* get my "stop" flag */
    int get_stop_formatting() const { return stop_formatting_; }

    /* 
     *   Add one or more items to the internal display list, without any
     *   formatting.  If more items are linked after the given item, they'll
     *   be included in the display list.  
     */
    void add_to_disp_list(class CHtmlDisp *item);

    /* -------------------------------------------------------------------- */
    /*
     *   Text array access methods.  Rather than exposing our text array
     *   directly, we provide our own virtual interface to the text array.
     *   This allows subclasses that don't use text array objects to provide
     *   their own implementations of the text access methods.  
     */

    /* 
     *   Get the number of characters between two text offsets.  Offsets are
     *   not necessarily contiguous (i.e., there might be gaps), so this
     *   routine can be used to find out how many characters are actually
     *   stored in the given offset range.  
     */
    virtual unsigned long get_chars_in_ofs_range(
        unsigned long startofs, unsigned long endofs) const;

    /* get the character at the given text offset */
    virtual textchar_t get_char_at_ofs(unsigned long ofs) const;

    /* 
     *   Increment/decrement a text offset.  Since text offsets are not
     *   necessarily continuous (i.e., gaps can exist, so two adjacent
     *   characters might have text offsets that differ by more than 1),
     *   these routines should always be used to traverse the display list's
     *   text.  
     */
    virtual unsigned long inc_text_ofs(unsigned long ofs) const;
    virtual unsigned long dec_text_ofs(unsigned long ofs) const;

    /* get the maximum text offset */
    virtual unsigned long get_text_ofs_max() const;

    /* 
     *   Get a pointer to the text starting at the given offset.  The
     *   returned text is not guaranteed to hold more than the text of a
     *   single span of text or of a single tag.
     *   
     *   This routine should be used ONLY by tags in the course of formatting
     *   themselves.  For callers needing to obtain a run of text from the
     *   display list, use extract_text().  
     */
    virtual const textchar_t *get_text_ptr(unsigned long ofs) const;

    /* determine if searching is possible in this display list */
    virtual int can_search_for_text() const { return TRUE; }

    /* search for text in the display list */
    virtual int search_for_text(const textchar_t *txt, size_t len,
                                int exact_case, int wrap, int dir,
                                unsigned long startofs,
                                unsigned long *match_start,
                                unsigned long *match_end);

protected:
    /* get our text array */
    class CHtmlTextArray *get_text_array() const;

    /* adjust the display factory for the current mode settings */
    void adjust_disp_factory();

    /*
     *   Internal service routines to reset formatting.  start_at_top()
     *   normally calls both of these, but some subclasses might want just
     *   the display list clearing or just the internal state reset, so we
     *   provide this separation of operations.  
     */
    void reset_disp_list();
    void reset_formatter_state(int reset_sounds);

    /*
     *   Insert a table's caption here if we're on the proper side of the
     *   table 
     */
    void check_format_caption(class CHtmlTagCAPTION *caption,
                              class CHtmlDispTable *table_disp, int top);
    
    /* remove items from the display list starting at the given item */
    void remove_from_disp_list(long search_start_line,
                               class CHtmlDisp *first_item_to_remove);

    /* add an item as a new line to the line starts table */
    void add_line_start(class CHtmlDisp *item);
    
    /* calculate the space available between the margins */
    long get_space_between_margins();

    /* add an item that was previously deferred to the display list */
    void deferred_item_ready(class CHtmlDisp *disp);

    /* add all of the deferred items to the display list */
    void add_all_deferred_items(class CHtmlDisp *prv_line_head,
                                class CHtmlDisp *nxt_line_head,
                                int force);
    
    /*
     *   Check to see if we're past the end of the flow items, and restore
     *   enclosing margins if so.  Returns true if any flow items ended,
     *   false if not.  
     */
    int check_flow_end();

    /* 
     *   Try to free up a given amount of width in the current line by
     *   breaking clear of marginal material.  Returns true if any changes
     *   are made to the margins, whether or not the requested amount of
     *   space is made available.  
     */
    int try_clearing_margins(long width);
    
    /* set margins and adjust available line width for the change */
    void set_margins(long left, long right);

    /* invalidate a range of text given by offsets */
    void inval_offset_range(long l, long r);

    /*
     *   Find a text position for a given text offset by searching the
     *   display list, starting at a particular item in the list.  
     */
    CHtmlPoint get_text_pos_list(class CHtmlDisp *start,
                                 unsigned long txtofs) const;

    /*
     *   Start a new line.  If a new line head is given, we'll break the
     *   line so that the new line head is at the start of the next line,
     *   otherwise we'll break the line starting with the next item added
     *   to the display list (hence all items in the current display list
     *   from the start of the current line will be in the current line).
     *   
     */
    void start_new_line(CHtmlDisp *next_line_head);

    /* 
     *   Close a pending tab -- align text between the previous tab stop
     *   and the current position.  Call this whenever a new tab is
     *   encountered, or the end of a line is reached.  
     */
    void close_pending_tab();

    /* delete the display list */
    virtual void delete_display_list();

    /* set positions for objects in a newly set line */
    void set_line_positions(CHtmlDisp *line_head, CHtmlDisp *next_line_head);

    /* determine if a character is part of a word, punctuation, or space */
    int get_char_class(textchar_t c) const
    {
        if (is_alpha(c) || is_digit(c) || c == '\'' || c == '-' || c == '_')
        {
            /* part of a word */
            return 1;
        }
        else if (is_space(c))
        {
            /* whitespace */
            return 2;
        }
        else
        {
            /* punctuation/other */
            return 3;
        }
    }
        

    /* move forward or backward to a word boundary */
    unsigned long find_word_boundary(unsigned long start, int dir) const;

    /* 
     *   Determine if we're in a multi-column mode.  If we have a
     *   flow-around item in either margin, we're in a multi-column mode. 
     */
    int is_multi_col() const
    {
        /* if there are any flow-around items, treat as multi-column */
        if (!left_flow_stk_.is_empty() || !right_flow_stk_.is_empty())
            return TRUE;

        /* tables are always multi-column */
        if (table_pass_ != 0)
            return TRUE;

        /* not multi-column */
        return FALSE;
    }

    /*
     *   Run the second table formatting pass on the current table 
     */
    void run_table_pass_two(class CHtmlTagTABLE *table_tag);

    /*
     *   Push/pop a table formatting environment.  This allows us to save
     *   the environment at the start of a floating table so that we can
     *   pick up where we left off when we finish with the table.  
     */
    void push_table_env();
    void pop_table_env();

    /* parser */
    class CHtmlParser *parser_;
    
    /* system window object into which we'll draw the formatted results */
    class CHtmlSysWin *win_;

    /* my special "body" pseudo-display item */
    class CHtmlDispBody *body_disp_;

    /*
     *   Current font.  Note that we never "own" a font object -- we are
     *   not responsible for deleting the object.  The window must keep
     *   track of font objects and delete them when the window closes.
     *   The window should take care not to delete any font objects that
     *   it gives to the formatter as long as the formatter is around.  
     */
    class CHtmlSysFont *curfont_;

    /*
     *   Base font size.  Relative font size settings are given as
     *   amounts above or below this base font size setting. 
     */
    int font_base_size_;

    /* horizontal space remaining in current line */
    long avail_line_width_;

    /* flag indicating that we've finished building a new line */
    int line_output_ : 1;
    
    /* current margins, as offsets from the bounds of the window */
    long margin_left_;
    long margin_right_;

    /* 
     *   Margin deltas to put into effect at the end of the current line.
     *   When temporary margins are in effect, these members store the
     *   amounts to add to the current margins to restore at the end of
     *   the current line, when the temporary margins go out of scope.  
     */
    long margin_left_delta_nxt_;
    long margin_right_delta_nxt_;

    /* flag indicating that temporary margins are in effect */
    int temp_margins_ : 1;

    /* current margin stack depth - each push increases the depth by one */
    int margin_stack_depth_;

    /* margin stack */
    struct margin_stack_t
    {
        long left_delta_;
        long right_delta_;
        void set(long left_delta, long right_delta)
        {
            left_delta_ = left_delta;
            right_delta_ = right_delta;
        }
    } margin_stack_[MARGIN_STACK_DEPTH_MAX];

    /*
     *   division alignment type - left, center, right, justify, or
     *   invalid (which indicates that the default should be used) 
     */
    HTML_Attrib_id_t div_alignment_;

    /*
     *   block alignment - this overrides the division alignment, if set
     *   to a valid value 
     */
    HTML_Attrib_id_t blk_alignment_;

    /*
     *   "physical" margins, specified by the window; we'll always set
     *   our layout inset from the window coordinates by these amounts 
     */
    CHtmlRect phys_margins_;

    /* current rendering position */
    CHtmlPoint curpos_;

    /* 
     *   highest y position for display purposes - this is the display
     *   height of the page 
     */
    unsigned long disp_max_y_pos_;

    /* 
     *   highest y position for layout purposes - this is the maximum y
     *   position we've assigned to a display item 
     */
    unsigned long layout_max_y_pos_;

    /* display list head and tail */
    class CHtmlDisp *disp_head_;
    class CHtmlDisp *disp_tail_;

    /*
     *   Deferred item list head and tail.  Certain items can't be added
     *   to the display list immediately when encountered in the parse
     *   tree, but must instead be added at the start of the next line.
     *   When we encounter such an item, we'll put it into this deferred
     *   item list; then, whenever we start a new line, we'll put all the
     *   items in the deferred item list into the display list. 
     */
    class CHtmlDisp *defer_head_;
    class CHtmlDisp *defer_tail_;

    /*
     *   suppress scrolling and window invalidation while formatting
     *   lines -- normally, we scroll and update the window as we display
     *   text, but while formatting input we save updates until we're done
     *   formatting the entire input, to avoid flashing the screen 
     */
    int freeze_display_adjust_ : 1;

    /*
     *   First display element in the current line.  This points to an
     *   item in the main display list; as we build a line, we keep this
     *   pointed at the first element in the current line so that we can
     *   go back through the line and position everything correctly once
     *   we're done formatting the entire line. 
     */
    class CHtmlDisp *line_head_;

    /* number of lines we've stored */
    long line_count_;

    /* 
     *   ID of line - this is an arbitrary designation that we use to
     *   distinguish adjacent items on different lines.  We update this
     *   whenever we start a new line, whether or not we store a line
     *   start record for the line.
     */
    long line_id_;

    /* width of longest line */
    long max_line_width_;

    /* width of longest line at outermost level */
    long outer_max_line_width_;

    /*
     *   Tag we're currently rendering (i.e., the tag that we'll render
     *   on the next call to do_formatting).  
     */
    class CHtmlTag *curtag_;

    /*
     *   Last tag we rendered.  We hold on to this so that we can keep
     *   going from the last position if more text gets added after we've
     *   formatted everything, at which point curtag_ will be null and we
     *   wouldn't otherwise be able to figure out where we left off. 
     */
    class CHtmlTag *prvtag_;

    /*
     *   Flag indicating that we should stop formatting, even if we have
     *   more left in our list.  This is used by sub-formatters to limit
     *   the extent of the tags they format: when a sub-formatter reaches
     *   its ending point, it will set this flag, and we'll act as though
     *   there were nothing left in the tag list. 
     */
    int stop_formatting_ : 1;

    /* last valid break position */
    CHtmlLineBreak breakpos_;

    /* line start table */
    CHtmlLineStarts *line_starts_;

    /* DIV list */
    CHArrayList div_list_;

    /* current DIV tag we're processing */
    class CHtmlDispDIV *cur_div_;

    /* current selection range */
    unsigned long sel_start_;
    unsigned long sel_end_;

    /* true -> we're at the beginning of a new line */
    int last_was_newline_ : 1;

    /* amount of space to add before the current line */
    int line_spacing_;

    /* line wrapping mode - false means lines are only broken explicitly */
    int wrap_lines_ : 1;

    /* flow stacks for left and right margins */
    CHtmlFmtFlowStack left_flow_stk_;
    CHtmlFmtFlowStack right_flow_stk_;

    /* flow stack environment save stack (for floating table formatting) */
    CHtmlFmtTableEnvSave table_env_save_[CHTMLFMT_TABLE_ENV_SAVE_MAX];
    size_t table_env_save_sp_;

    /* tab stop table */
    class CHtmlHashTable *tab_table_;

    /* pending tab stop information */
    int pending_tab_ : 1;
    long pending_tab_xpos_;
    HTML_Attrib_id_t pending_tab_align_;
    textchar_t pending_tab_dp_;
    class CHtmlDispTab *pending_tab_disp_;

    /*
     *   Heap increment size.  Subclasses may want to change this value at
     *   construction time according to their memory management
     *   characteristics. 
     */
    size_t heap_alloc_unit_;

    /* list of heap pages */
    CHtmlFmt_heap_page_t *heap_pages_;

    /* maximum number of entries in the heap page list */
    size_t heap_pages_max_;

    /* number of pages in the list that have actually been allocated */
    size_t heap_pages_alloced_;

    /* 
     *   current heap page - at any given time, we're allocating out of
     *   one heap page; when we exhaust it, we'll move on to the next one 
     */
    size_t heap_page_cur_;

    /* offset of next free byte in current heap page */
    size_t heap_page_cur_ofs_;

    /* 
     *   current hypertext link object - when we're within a link, this
     *   item must be set to contain the link information for objects
     *   within the link 
     */
    class CHtmlDispLink *cur_link_;

    /* 
     *   flag: character wrapping mode is in effect (if not, word-wrapping
     *   mode is used) 
     */
    int char_wrap_mode_;

    /* current display item factory */
    CHtmlFmtDispFactory *cur_disp_factory_;

    /* 
     *   display item factories for each combination of linkability and
     *   character/word wrap modes 
     */
    CHtmlFmtDispFactory *disp_factory_normal_;
    CHtmlFmtDispFactory *disp_factory_charwrap_;
    CHtmlFmtDispFactory *disp_factory_link_;
    CHtmlFmtDispFactory *disp_factory_link_charwrap_;

    /* current map under construction */
    class CHtmlFmtMap *cur_map_;

    /* map table */
    class CHtmlHashTable *map_table_;

    /* ordered list continuation sequence number */
    long ol_cont_seqnum_;

    /* 
     *   Current table formatting pass number:
     *.    0 -> not formatting a table at all
     *.    1 -> first pass: measuring cell contents
     *.    2 -> second pass: formatting cell contents 
     */
    int table_pass_;

    /* format tag for current table */
    class CHtmlTagTABLE *current_table_;

    /* pre-table heap state */
    size_t pre_table_heap_page_cur_;
    size_t pre_table_heap_page_cur_ofs_;

    /* pre-table display list and line list state */
    class CHtmlDisp *pre_table_disp_tail_;
    class CHtmlDisp *pre_table_line_head_;
    long pre_table_line_count_;
    long pre_table_line_id_;
    long pre_table_line_starts_count_;
    size_t pre_table_div_list_count_;
    class CHtmlDispDIV *pre_table_cur_div_;
    long pre_table_max_line_width_;
    long pre_table_avail_line_width_;

    /* pre-table output position */
    CHtmlPoint pre_table_curpos_;
    int pre_table_line_spacing_;
    int pre_table_last_was_newline_ : 1;

    /*
     *   Formatter sound generation ID.  Each time we start formatting a
     *   brand new page, we'll increment the sound generation ID.  When a
     *   sound is played, we'll mark it with its generation.  If a sound
     *   has already been played on the current generation, we'll ignore
     *   it; this allows us to resize and otherwise reformat an existing
     *   page repeatedly without having to restart all of its sounds,
     *   while still detecting when we need to start sounds again from
     *   scratch. 
     */
    unsigned int sound_gen_id_;

    /* 
     *   Flag: we're in T3 mode.  This means our caller is a T3 program,
     *   which changes certain parsing rules. 
     */
    int t3_mode_;

    /* flag: we're capturing a <TITLE> */
    unsigned int title_mode_ : 1;

    /* resource cache (a global singleton) */
    static class CHtmlResCache *res_cache_;

    /* resource finder (a global singleton) */
    static class CHtmlResFinder *res_finder_;

    /*
     *   Sound queues.  We maintain four sound queues: one for the
     *   foreground layer, one for the background ambient layer, one for the
     *   ambient layer, and one for the background layer.
     *   
     *   The sound queues are global to the entire application.  We maintain
     *   these with the main text window, because that window is always
     *   around.  
     */
    static class CHtmlSoundQueue *sound_queues_[4];

    /* 
     *   number of active formatters (we use this for initializing and
     *   uninitializing the global singletons)
     */
    static int formatter_cnt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Source window formatter.  This is used for displaying the text of a
 *   source file window in the debugger. 
 */
class CHtmlFormatterSrcwin: public CHtmlFormatter
{
public:
    CHtmlFormatterSrcwin(class CHtmlParser *parser)
        : CHtmlFormatter(parser) { }

    /* 
     *   get/set the last sound tag - these do nothing in this subclass, as
     *   source windows don't use sound tags 
     */
    virtual class CHtmlTagSOUND *get_last_sound_tag() const { return 0; }
    virtual void set_last_sound_tag(class CHtmlTagSOUND *) { }

    /* <SOUND> tags are not allowed in source windows */
    virtual int allow_sound_tags() const { return FALSE; }

    /* start formatting from the top of the document */
    virtual void start_at_top(int reset_sounds);

protected:
};

/* ------------------------------------------------------------------------ */
/*
 *   Stand-alone window formatter.  This can be used for any independent
 *   HTML window.
 *   
 *   This formatter can be used for displaying old pages in history-browsing
 *   mode.  A history window is like a main window, but can't redisplay its
 *   banners.  This formatter can also be used for displaying separate
 *   windows such as "About" boxes.  
 */
class CHtmlFormatterStandalone: public CHtmlFormatter
{
public:
    CHtmlFormatterStandalone(class CHtmlParser *parser)
        : CHtmlFormatter(parser) { }

    /* <SOUND> tags are not allowed in standalone windows */
    virtual int allow_sound_tags() const { return FALSE; }

    /* 
     *   get/set the last sound tag - these do nothing in this subclass,
     *   since we don't use sounds in old windows recalled via history
     *   browsing 
     */
    virtual class CHtmlTagSOUND *get_last_sound_tag() const { return 0; }
    virtual void set_last_sound_tag(class CHtmlTagSOUND *) { }

    /* start formatting from the top of the document */
    virtual void start_at_top(int reset_sounds);

protected:
};

/* ------------------------------------------------------------------------ */
/*
 *   Main formatter.  This formatter subclass is used for the main text
 *   window.  
 */
class CHtmlFormatterMain: public CHtmlFormatter
{
public:
    CHtmlFormatterMain(class CHtmlParser *parser);
    ~CHtmlFormatterMain();

    /* set my window and the physical margins to use */
    virtual void set_win(class CHtmlSysWin *win, const CHtmlRect *r);

    /* receive notification that the window is about to be deleted */
    virtual void unset_win();

    /* prepare to delete the parser */
    virtual void release_parser();

    /* start formatting from the top of the document */
    virtual void start_at_top(int reset_sounds);

    /* <SOUND> tags are allowed in the main window */
    virtual int allow_sound_tags() const { return TRUE; }

    /* get the last sound tag */
    virtual class CHtmlTagSOUND *get_last_sound_tag() const
    {
        /* return the last sound tag we stored */
        return last_sound_tag_;
    }

    /* set the last sound tag */
    virtual void set_last_sound_tag(class CHtmlTagSOUND *tag)
    {
        /* remember the tag */
        last_sound_tag_ = tag;
    }

    /*
     *   Add an item to the tail of the display list, adjust the current line
     *   under construction to compensate for the new item.  If we're in
     *   <TITLE> mode, capture text to our title buffer for later use.  
     */
    virtual void add_disp_item(class CHtmlDisp *item);

    /* when we end the <TITLE> tag, set the window title */
    virtual void end_title(class CHtmlTagTITLE *tag);

    /* format a banner */
    virtual int format_banner(const textchar_t *banner_id,
                              long height, int height_set, int height_prev,
                              int height_pct,
                              long width, int width_set, int width_prev,
                              int width_pct,
                              class CHtmlTagBANNER *banner_tag);

    /* remove all of the in-line banners */
    virtual void remove_all_banners(int replacing);

    /* remove a banner */
    virtual void remove_banner(const textchar_t *banner_id);

protected:
    /* remove a banner */
    void remove_banner(class CHtmlFmtBannerItem *item, int replacing);
    
    /* title buffer */
    CStringBuf title_buf_;

    /*
     *   Banner array.  Since the number of banners used in a particular
     *   game should be small, we're not bothering to use an
     *   efficiently-searched data structure; we just use a linked list of
     *   banner tracking objects.  
     */
    class CHtmlFmtBannerItem *banners_;

    /* last sound tag */
    class CHtmlTagSOUND *last_sound_tag_;
};

/* banner list item */
class CHtmlFmtBannerItem
{
public:
    CHtmlFmtBannerItem(const textchar_t *banner_id, int is_about_box,
                       class CHtmlFormatterBannerSub *subformatter)
    {
        id_.set(banner_id);
        is_about_box_ = is_about_box;
        subformatter_ = subformatter;
        nxt_ = 0;
        height_set_ = FALSE;
        width_set_ = FALSE;
        last_tag_ = first_tag_ = 0;
        is_first_ = TRUE;
    }

    /* does this item match the given ID? */
    int matches(const textchar_t *banner_id, int is_about_box)
    {
        return ((is_about_box_ != 0) == (is_about_box != 0)
                && get_strcmp(banner_id, id_.get()) == 0);
    }

    /* the ID string for the banner */
    CStringBuf id_;

    /* flag indicating whether this is the about box banner */
    int is_about_box_ : 1;

    /* the sub-formatter that does the formatting for the banner contents */
    class CHtmlFormatterBannerSub *subformatter_;

    /* next banner in banner list */
    CHtmlFmtBannerItem *nxt_;

    /* flag indicating that the height/width have been set */
    int height_set_ : 1;
    int width_set_ : 1;

    /* last BANNER tag using this banner */
    class CHtmlTagBANNER *last_tag_;

    /* 
     *   First BANNER tag using this same ID, at least since the last
     *   <BANNER REMOVE> tag for this ID.  We always keep the first and
     *   last banners of a series of replacements (without removals) of
     *   the same banner, so that the banner will be re-created in the
     *   same order when the entire list is reformatted from scratch.  
     */
    class CHtmlTagBANNER *first_tag_;

    /* flag indicating that this is the first occurrence of this banner */
    int is_first_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Input-capable formatter.  An input-capable formatter is always a main
 *   formatter. 
 */
class CHtmlFormatterInput: public CHtmlFormatterMain
{
public:
    CHtmlFormatterInput(CHtmlParser *parser);

    /* start formatting from the top of the document */
    virtual void start_at_top(int reset_sounds);

    /* delete the display list */
    virtual void delete_display_list();

    /* do we have more rendering prior to the input line? */
    int more_before_input();

    /*
     *   Add an item to the tail of the display list, replacing the
     *   current command input display item(s), if any.  This is used
     *   during editing of a command line to update the display after the
     *   user has made a change in the contents of the command line.  
     */
    virtual void add_disp_item_input(class CHtmlDispTextInput *item,
                                     class CHtmlTagTextInput *tag);

    /* 
     *   Prepare for input.  This can be called prior to finishing formatting
     *   in preparation for input.  The purpose of this routine is to correct
     *   any ill-formed HTML prior to pausing for interactive command
     *   editing.  Specifically, we close any <TABLE> that hasn't yet been
     *   finished.  
     */
    void prepare_for_input();

    /*
     *   Begin a new input.  'buf' is the buffer, managed by the caller,
     *   which the caller will use to edit the input.  Returns the text
     *   input tag which the caller uses to coordinate the input.
     *   
     *   'buflen' is the initial length of the text in the buffer -- this
     *   isn't the size of the buffer, but simply the length of the text
     *   that we should display as part of the initial input contents.
     *   When starting a new line with nothing in the buffer, this should
     *   be zero.  Note that in some cases (such as after a
     *   partially-entered command interrupted by a real-time event), we
     *   will start a new command with some text already typed in for the
     *   user; in these cases, 'buflen' indicates the length of this
     *   pre-typed text.  
     */
    class CHtmlTagTextInput *begin_input(const textchar_t *buf,
                                         size_t buflen);

    /*
     *   End the current command input line.  No further editing of the
     *   current line is possible after calling this method.
     */
    void end_input();

    /*
     *   Get the position of a character, given the text offset in the
     *   character array, searching only the current input line.  This is
     *   somewhat faster than the generic get_text_pos(), since it only
     *   needs to search the current input line.  
     */
    CHtmlPoint get_text_pos_input(unsigned long txtofs) const;

    /* 
     *   Store input text temporarily in the text array.  This is used by the
     *   command-input text tag to update the text array with an input line
     *   under construction.  This doesn't commit the text to the text array,
     *   but simply stores it temporarily so that it can be displayed during
     *   interactive command editing.  
     */
    virtual unsigned long store_input_text_temp(
        const textchar_t *txt, size_t len);

protected:
    /* clear the old input line */
    void clear_old_input();

    /*
     *   Item just before the active command input display list head --
     *   the active command input items are always the last items in the
     *   display list.  Note that we keep track of the item before the
     *   start of the command input list, since this is the tail of the
     *   non-command list.  
     */
    class CHtmlDisp *input_pre_;

    /* the item which starts the active input */
    class CHtmlDispTextInput *input_head_;

    /* input text tag coordinating the current command line input */
    class CHtmlTagTextInput *input_tag_;

    /* available line width when the input line was started */
    long input_avail_line_width_;

    /* x,y position at start of input */
    long input_x_pos_;
    long input_y_pos_;

    /* line count at start of input */
    long input_line_count_;
    long input_line_id_;
    long input_line_starts_count_;

    /* first item on input line */
    class CHtmlDisp *input_line_head_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Banner formatter.  This formatter subclass is used to format banner
 *   subwindows. 
 */
class CHtmlFormatterBanner: public CHtmlFormatter
{
public:
    CHtmlFormatterBanner(class CHtmlParser *parser)
        : CHtmlFormatter(parser)
    {
        /* 
         *   use a fairly small heap allocation unit size, since banners
         *   don't tend to contain a lot of data 
         */
        heap_alloc_unit_ = 2048;
    }

    /* 
     *   Initialize the window size.  If width_set is true, it indicates that
     *   the window's width should be set to the width value explicitly
     *   given, which is in pixels unless width_pct is true, in which case
     *   it's a percentage of the main frame window size.  If width_set is
     *   false, and width_prev is true, then we simply keep the existing
     *   window size with no changes.  If width_set and width_prev are both
     *   false, then we size the window according to its current contents.
     *   The height parameters are parallel in meaning.  
     */
    void init_size(HTML_BannerWin_Pos_t alignment,
                   long width, HTML_BannerWin_Units_t width_units,
                   int width_set, int width_prev,
                   long height, HTML_BannerWin_Units_t height_units,
                   int height_set, int height_prev);

protected:
    /* 
     *   Calculate the natural content height and width, for purposes of
     *   sizing the banner to its contents. 
     */
    virtual long calc_content_height(HTML_BannerWin_Pos_t alignment);
    virtual long calc_content_width(HTML_BannerWin_Pos_t alignment);
};

/* ------------------------------------------------------------------------ */
/*
 *   Formatter for banners created with the <BANNER> tag.  A tag banner is
 *   in many ways a child window of the main text window.  
 */
class CHtmlFormatterBannerSub: public CHtmlFormatterBanner
{
public:
    CHtmlFormatterBannerSub(class CHtmlParser *parser,
                            class CHtmlFormatter *parent,
                            class CHtmlTagBANNER *banner_tag)
        : CHtmlFormatterBanner(parser)
    {
        /* remember our parent and starting point */
        parent_ = parent;
        banner_start_tag_ = banner_tag;
    }

    /* <SOUND> tags are allowed in banner subwindows */
    virtual int allow_sound_tags() const { return TRUE; }

    /* get the last sound tag */
    virtual class CHtmlTagSOUND *get_last_sound_tag() const
    {
        /* 
         *   we're effectively part of our parent's tag stream, so return
         *   the last sound tag from our parent formatter 
         */
        return parent_->get_last_sound_tag();
    }

    /* set the last sound tag */
    virtual void set_last_sound_tag(class CHtmlTagSOUND *tag)
    {
        /* we're part of the parent's stream of tags */
        parent_->set_last_sound_tag(tag);
    }

    /*
     *   Set the starting tag for the subformatter -- this is the BANNER
     *   container tag that opened the banner.  
     */
    void set_banner_start_tag(class CHtmlTagBANNER *start_tag)
    {
        banner_start_tag_ = start_tag;
    }
    
    /* receive notification that we've reached the end of the banner */
    virtual void end_banner();    

    /*
     *   Re-start rendering at the top of the document.  This should be
     *   called whenever the current formatting becomes invalid, such as
     *   when the window is resized horizontally.  
     */
    virtual void start_at_top(int reset_sounds);

protected:
    /* parent formatter */
    CHtmlFormatter *parent_;

    /* BANNER tag at the top of the banner contents */
    class CHtmlTagBANNER *banner_start_tag_;
};

/* ------------------------------------------------------------------------ */
/*
 *   "External" banner formatter.  An external banner is a banner not
 *   associated with a <BANNER> tag in the main text stream, but rather
 *   controlled via a completely separate text stream.
 *   
 *   The external banner provides its own management of the source text, so
 *   that the external stream source can write the stream data directly to
 *   the formatter object.  
 */
class CHtmlFormatterBannerExt: public CHtmlFormatterBanner
{
public:
    /* 
     *   Create an external banner.  This is essentially an implementation of
     *   the tads2 osifc routine os_banner_create().
     *   
     *   'parent' is the parent banner.  A child takes space out of its
     *   parent's display area.  If 'parent' is null, then the new banner is
     *   a child of the main text area.
     *   
     *   'where' is OS_BANNER_FIRST, OS_BANNER_LAST, OS_BANNER_BEFORE, or
     *   OS_BANNER_AFTER, specifying where the new banner is to go in the
     *   banner list.  'other' is the existing banner object that we're using
     *   as the relative insertion point for OS_BANNER_BEFORE or
     *   OS_BANNER_AFTER.
     *   
     *   'wintype' is an OS_BANNER_TYPE_xxx value giving the type of banner
     *   window to create.
     *   
     *   'siz' is the initial size of the window, in units given by
     *   'siz_units', which is an OS_BANNER_SIZE_xxx value.
     *   
     *   'style' is a combination of OS_BANNER_STYLE_xxx flags giving the
     *   requested style of the banner window.  
     */
    static CHtmlFormatterBannerExt *create_extern_banner(
        CHtmlFormatterBannerExt *parent,
        int where, CHtmlFormatterBannerExt *other,
        int wintype, HTML_BannerWin_Pos_t alignment, int siz, int siz_units,
        unsigned long style);

    /* deletion */
    ~CHtmlFormatterBannerExt()
    {
        size_t i, cnt;
        
        /* detach from our parent */
        if (parent_ != 0)
            parent_->remove_child(this);

        /* detach from our children */
        for (i = 0, cnt = children_.get_count() ; i < cnt ; ++i)
            ((CHtmlFormatterBannerExt *)children_.get_ele(i))->parent_ = 0;
    }

    /* delete an external banner */
    static void delete_extern_banner(class CHtmlFormatterBannerExt *banner);

    /* clear the contents */
    virtual void clear_contents() = 0;

    /* add source text for formatting */
    virtual void add_source_text(const textchar_t *txt, size_t len) = 0;

    /* 
     *   Set the window's size.  'siz_units' is an OS_BANNER_SIZE_xxx value
     *   specifying the units for 'siz'. 
     */
    virtual void set_banner_size(int siz, int siz_units);

    /* size the window to its current contents */
    virtual void size_to_contents(int set_width, int set_height);

    /* flush the source text */
    virtual void flush_txtbuf(int fmt) = 0;

    /* turn HTML parsing mode on or off */
    virtual void set_html_mode(int mode) = 0;

    /* get HTML parsing mode */
    virtual int get_html_mode() const = 0;

    /* set the text color */
    virtual void set_banner_text_color(os_color_t fg, os_color_t bg) = 0;

    /* set the current text attributes */
    virtual void set_banner_text_attr(int attrs) = 0;

    /* set the banner's window background color */
    virtual void set_banner_screen_color(os_color_t color) = 0;

    /*
     *   Set the output cursor position to the given row and column.  This is
     *   meaningless on stream-oriented banner windows; on grid windows, this
     *   sets the position to the given coordinates, expressed in character
     *   cells, numbered from 0,0 for the upper left corner.  
     */
    virtual void set_cursor_pos(int /*row*/, int /*col*/)
    {
        /* this is not meaningful by default, so simply ignore it */
    }

    /* get my "natural" size units */
    virtual HTML_BannerWin_Units_t get_natural_size_units() const = 0;

    /* <SOUND> tags are not allowed in external banner windows */
    virtual int allow_sound_tags() const { return FALSE; }
    virtual class CHtmlTagSOUND *get_last_sound_tag() const { return 0; }
    virtual void set_last_sound_tag(class CHtmlTagSOUND *) { }

protected:
    /*
     *   Construction and destruction are handled via static method we expose
     *   publicly - callers do not call our constructors or destructors
     *   directly, so make these protected.
     */
    CHtmlFormatterBannerExt(CHtmlFormatterBannerExt *parent,
                            class CHtmlParser *parser, unsigned long style)
        : CHtmlFormatterBanner(parser)
    {
        /* remember my parent */
        parent_ = parent;

        /* tell my parent about its new child */
        if (parent != 0)
            parent->add_child(this);

        /* remember the original requested style */
        orig_style_ = style;
    }

    /* calculate the natural content height and width */
    virtual long calc_content_height(HTML_BannerWin_Pos_t alignment);
    virtual long calc_content_width(HTML_BannerWin_Pos_t alignment);

    /* add a new child to my list of children */
    void add_child(CHtmlFormatterBannerExt *chi)
        { children_.add_ele(chi); }

    /* remove a child from my list of children */
    void remove_child(CHtmlFormatterBannerExt *chi)
        { children_.remove_ele(chi); }

    /* my parent formatter, if any */
    CHtmlFormatterBannerExt *parent_;

    /* my list of child formatters, if any */
    CHArrayList children_;

    /* 
     *   The original style flags passed at creation.  Some of these flags
     *   are handled by the underlying OS implementation, so the presence of
     *   a flag bit doesn't necessarily mean that the corresponding style is
     *   actually in use.  This merely records the style flags that were
     *   requested.  
     */
    unsigned long orig_style_;
};

/* ------------------------------------------------------------------------ */
/*
 *   External Banner Formatter subclass for ordinary text stream windows. 
 */
class CHtmlFormatterBannerExtText: public CHtmlFormatterBannerExt
{
    friend class CHtmlFormatterBannerExt;
    
public:
    /* start formatting at the top of the tag list */
    virtual void start_at_top(int reset_sounds);

    /* add source text for formatting */
    virtual void add_source_text(const textchar_t *txt, size_t len);

    /* flush the source text */
    virtual void flush_txtbuf(int fmt);

    /* clear the contents */
    virtual void clear_contents();

    /* turn HTML parsing mode on or off */
    virtual void set_html_mode(int mode);

    /* get HTML parsing mode */
    virtual int get_html_mode() const;

    /* 
     *   set the text color - callers must use HTML in text banners, so we
     *   can simply ignore this entirely 
     */
    virtual void set_banner_text_color(os_color_t, os_color_t) { }

    /* 
     *   set the current text attributes - callers must use HTML tags in
     *   text banners, so we can ignore this 
     */
    virtual void set_banner_text_attr(int) { }

    /* set the background color - callers must use HTML tags, so ignore it */
    virtual void set_banner_screen_color(os_color_t) { }

    /* get my "natural" size units */
    virtual HTML_BannerWin_Units_t get_natural_size_units() const
    {
        /* text windows are sized in character cells */
        return HTML_BANNERWIN_UNITS_CHARS;
    }

protected:
    /* construction and destruction */
    CHtmlFormatterBannerExtText(CHtmlFormatterBannerExt *parent,
                                class CHtmlParser *parser,
                                unsigned long style);
    ~CHtmlFormatterBannerExtText();

    /* text buffer for sending text to the parser */
    class CHtmlTextBuffer *txtbuf_;
};

/* ------------------------------------------------------------------------ */
/*
 *   External Banner Formatter subclass for Text Grid windows.  
 */
class CHtmlFormatterBannerExtGrid: public CHtmlFormatterBannerExt
{
    friend class CHtmlFormatterBannerExt;

public:
    /* start formatting at the top of the tag list */
    virtual void start_at_top(int reset_sounds);

    /* add source text for formatting */
    virtual void add_source_text(const textchar_t *txt, size_t len)
    {
        /* 
         *   write the text at the current cursor position, and move the
         *   cursor to the end of the new text
         */
        write_text_at(txt, len, csr_row_, csr_col_, TRUE);
    }

    /* flush the source text - we don't buffer anything, so this is trivial */
    virtual void flush_txtbuf(int) { }

    /* clear the contents */
    virtual void clear_contents();

    /* move the output cursor */
    virtual void set_cursor_pos(int row, int col)
    {
        /* remember the new cursor position */
        csr_row_ = row;
        csr_col_ = col;
    }

    /* 
     *   Turn HTML parsing mode on or off.  A text grid banner doesn't
     *   support HTML parsing at all, so there's nothing to do here. 
     */
    virtual void set_html_mode(int) { }

    /* get the HTML mode - we're always in non-HTML mode */
    virtual int get_html_mode() const { return FALSE; }

    /* set the text color */
    virtual void set_banner_text_color(os_color_t fg, os_color_t bg);

    /* set text attributes - we don't support any attributes, so ignore it */
    virtual void set_banner_text_attr(int) { }

    /* set the background color */
    virtual void set_banner_screen_color(os_color_t color);

    /* set the window's size */
    virtual void set_banner_size(int siz, int siz_units);

    /* size the window to its current contents */
    void size_to_contents(int set_width, int set_height)
    {
        /* 
         *   remember my character size as of this last explicit sizing, so
         *   that we can restore our size in terms of the character cell size
         *   if the font changes and we thus have to recalculate the
         *   new pixel size corresponding to the same character size
         */
        last_sized_ht_ = row_cnt_;
        last_sized_wid_ = col_cnt_;

        /* inherit the default handling */
        CHtmlFormatterBannerExt::size_to_contents(set_width, set_height);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   We don't use a text array to store our text, so we must override the
     *   text array access methods. 
     */

    /* get the character at the given text offset */
    virtual textchar_t get_char_at_ofs(unsigned long ofs) const;

    /* 
     *   Get the number of characters in an offset range.  We store our
     *   character offsets continuously (without gaps), so the number of
     *   characters in an offset range is simply the difference between the
     *   offsets. 
     */
    virtual unsigned long get_chars_in_ofs_range(
        unsigned long startofs, unsigned long endofs) const
        { return endofs - startofs; }

    /* our text offsets are always continuous without gaps */
    virtual unsigned long inc_text_ofs(unsigned long ofs) const
        { return ofs + 1; }
    virtual unsigned long dec_text_ofs(unsigned long ofs) const
        { return ofs - 1; }

    /* get the maximum text offset */
    virtual unsigned long get_text_ofs_max() const { return max_ofs_; }

    /* 
     *   Get the text for the given offset.  We don't format our tags through
     *   a parser, so we don't support this operation. 
     */
    virtual const textchar_t *get_text_ptr(unsigned long ofs) const
        { return 0; }

    /* we don't implement searching */
    virtual int can_search_for_text() const { return FALSE; }
    virtual int search_for_text(const textchar_t *, size_t, int, int, int,
                                unsigned long,
                                unsigned long *, unsigned long *)
        { return FALSE; }

    /* get my "natural" size units */
    virtual HTML_BannerWin_Units_t get_natural_size_units() const
    {
        /* text grid windows are sized in character cells */
        return HTML_BANNERWIN_UNITS_CHARS;
    }

protected:
    /* construction */
    CHtmlFormatterBannerExtGrid(CHtmlFormatterBannerExt *parent,
                                unsigned long style);

    /* translate an os_color_t to an HTML_color_t */
    HTML_color_t xlat_color(os_color_t oscolor);

    /* write text at the given row and column, optionally moving the cursor */
    void write_text_at(const textchar_t *txt, size_t len,
                       int row, int col, int move_cursor);

    /* get a display item by row number */
    class CHtmlDispTextGrid *get_disp_by_row(int row) const;

    /* get the current default fixed-pitch font and remember it */
    void get_grid_font(int keep_color);

    /* current output position, in character cell coordinates */
    int csr_row_;
    int csr_col_;

    /* number of rows in the display list */
    int row_cnt_;

    /* number of characters in the widest row */
    int col_cnt_;

    /* 
     *   the maximum text offset - we don't use a text array to store our
     *   text, so we need to keep track of our own text array information 
     */
    unsigned long max_ofs_;

    /*
     *   Our height and width, in character cells, as of the last explicit
     *   size-to-contents invocation.  When the font changes on us, we might
     *   have to resize the window to adjust for the change in font size;
     *   since we're effectively sized in character cells, this information
     *   allows us to adapt to the new font size automatically.  
     */
    int last_sized_ht_;
    int last_sized_wid_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image maps 
 */

/*
 *   An image map.  A map has a name and a collection of zones. 
 */
class CHtmlFmtMap
{
public:
    CHtmlFmtMap(const textchar_t *map_name, size_t name_len)
    {
        /* remember my name */
        name_.set(map_name, name_len);

        /* no zones yet */
        first_zone_ = last_zone_ = 0;
    }

    ~CHtmlFmtMap();

    /* add a zone */
    void add_zone(class CHtmlFmtMapZone *zone);

    /* get my name */
    const textchar_t *get_name() const { return name_.get(); }

    /*
     *   Find a zone, given a point and an image's bounds.  Returns null
     *   if there's no zone containing the given point.  
     */
    class CHtmlFmtMapZone *find_zone(int x, int y,
                                     const CHtmlRect *image_bounds) const;

private:
    /* name of the map */
    CStringBuf name_;

    /* first/last zones in my list */
    class CHtmlFmtMapZone *first_zone_;
    class CHtmlFmtMapZone *last_zone_;
};

/*
 *   A map zone
 */
class CHtmlFmtMapZone
{
public:
    CHtmlFmtMapZone(class CHtmlDispLink *link)
    {
        /* remember my link object */
        link_ = link;

        /* nothing else in list yet */
        next_ = 0;
    }

    virtual ~CHtmlFmtMapZone() {}

    /* 
     *   Determine if a point within a given image is in the zone.  This
     *   must be overridden by each subclass to perform the appropriate
     *   computation for its shape.  
     */
    virtual int pt_in_zone(int x, int y, const CHtmlRect *image_bounds) = 0;

public:
    /* next zone in map's zone list */
    CHtmlFmtMapZone *next_;

    /* my link object */
    class CHtmlDispLink *link_;

protected:
    /* compute a coordinate, given the COORD settinng and the image bounds */
    long compute_coord(const struct CHtmlTagAREA_coords_t *coord,
                       const CHtmlRect *image_bounds, int vert);
};

/*
 *   Map zones for the various shapes
 */
class CHtmlFmtMapZoneRect: public CHtmlFmtMapZone
{
public:
    CHtmlFmtMapZoneRect(const struct CHtmlTagAREA_coords_t *coords,
                        class CHtmlDispLink *link);

    int pt_in_zone(int x, int y, const CHtmlRect *image_bounds);

private:
    CHtmlTagAREA_coords_t coords_[4];
};

class CHtmlFmtMapZoneCircle: public CHtmlFmtMapZone
{
public:
    CHtmlFmtMapZoneCircle(const struct CHtmlTagAREA_coords_t *coords,
                          class CHtmlDispLink *link);

    int pt_in_zone(int x, int y, const CHtmlRect *image_bounds);

private:
    CHtmlTagAREA_coords_t coords_[2];
    long radius_;
};

class CHtmlFmtMapZonePoly: public CHtmlFmtMapZone
{
public:
    CHtmlFmtMapZonePoly(const struct CHtmlTagAREA_coords_t *coords,
                        int coord_cnt, class CHtmlDispLink *link);

    int pt_in_zone(int x, int y, const CHtmlRect *image_bounds);

private:
    CHtmlTagAREA_coords_t coords_[CHtmlTagAREA_max_coords];
};

#endif /* HTMLFMT_H */

