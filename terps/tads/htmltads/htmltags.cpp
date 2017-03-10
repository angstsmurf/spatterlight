#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmltags.cpp,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmltags.cpp - implementation of HTML tag objects
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Basic tag implementation 
 */

CHtmlTag::~CHtmlTag()
{
}

/*
 *   check if my name matches a given name 
 */
int CHtmlTag::tag_name_matches(const textchar_t *nm, size_t nmlen) const
{
    const textchar_t *myname = get_tag_name();
    return tag_names_match(myname, get_strlen(myname), nm, nmlen);
}

/*
 *   check if two tag names match 
 */
int CHtmlTag::tag_names_match(const textchar_t *nm1, size_t len1,
                              const textchar_t *nm2, size_t len2)
{
    return (len1 == len2 && !memicmp(nm1, nm2, len1 * sizeof(nm1[0])));
}

/* process the end tag */
void CHtmlTag::process_end_tag(CHtmlParser *parser,
                               const textchar_t *tag, size_t len)
{
    /* process the normal end tag */
    parser->end_normal_tag(tag, len);
}

/* parse the tag */
void CHtmlTag::on_parse(CHtmlParser *parser)
{
    /* add myself to the current list */
    parser->append_tag(this);
}

/*
 *   Start a new paragraph
 */
void CHtmlTag::break_paragraph(CHtmlParser *parser, int isexplicit)
{
    parser->end_paragraph(isexplicit);
}

/*
 *   Set an attribute 
 */
HTML_attrerr CHtmlTag::set_attribute(CHtmlParser *, HTML_Attrib_id_t,
                                     const textchar_t * /*val*/,
                                     size_t /*vallen*/)
{
    /* we don't recognize any attributes by default */
    return HTML_attrerr_inv_attr;
}


/*
 *   Parse a color attribute setting 
 */
HTML_attrerr CHtmlTag::set_color_attr(CHtmlParser *parser,
                                      HTML_color_t *color_var,
                                      const textchar_t *val, size_t vallen)
{
    /*
     *   if it starts with #, it's a color sRGB value, otherwise it must
     *   be a name 
     */
    if (vallen > 0 && (*val == '#' || is_digit(*val)))
    {
        long num;
        HTML_attrerr err;

        /* skip leading '#' if provided */
        if (*val == '#')
        {
            ++val;
            --vallen;
        }

        /* parse the hex number for the color */
        err = set_hex_attr(&num, val, vallen);
        *color_var = num;
        return err;
    }
    else
    {
        /*
         *   ask the parser to try to find an identifier that matches the
         *   value 
         */
        switch(parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_black:   *color_var = 0x000000; break;
        case HTML_Attrib_silver:  *color_var = 0xc0c0c0; break;
        case HTML_Attrib_gray:    *color_var = 0x808080; break;
        case HTML_Attrib_white:   *color_var = 0xffffff; break;
        case HTML_Attrib_maroon:  *color_var = 0x800000; break;
        case HTML_Attrib_red:     *color_var = 0xff0000; break;
        case HTML_Attrib_purple:  *color_var = 0x800080; break;
        case HTML_Attrib_fuchsia: *color_var = 0xff00ff; break;
        case HTML_Attrib_green:   *color_var = 0x008000; break;
        case HTML_Attrib_lime:    *color_var = 0x00ff00; break;
        case HTML_Attrib_olive:   *color_var = 0x808000; break;
        case HTML_Attrib_yellow:  *color_var = 0xffff00; break;
        case HTML_Attrib_navy:    *color_var = 0x000080; break;
        case HTML_Attrib_blue:    *color_var = 0x0000ff; break;
        case HTML_Attrib_teal:    *color_var = 0x008080; break;
        case HTML_Attrib_aqua:    *color_var = 0x00ffff; break;

        case HTML_Attrib_statusbg:
            *color_var = HTML_COLOR_STATUSBG;
            break;

        case HTML_Attrib_statustext:
            *color_var = HTML_COLOR_STATUSTEXT; 
            break;

        case HTML_Attrib_link:
            *color_var = HTML_COLOR_LINK;
            break;

        case HTML_Attrib_alink:
            *color_var = HTML_COLOR_ALINK;
            break;

        case HTML_Attrib_hlink:
            *color_var = HTML_COLOR_HLINK;
            break;

        case HTML_Attrib_text:
            *color_var = HTML_COLOR_TEXT;
            break;

        case HTML_Attrib_bgcolor:
            *color_var = HTML_COLOR_BGCOLOR;
            break;

        default:
            /* this value is not valid */
            return HTML_attrerr_inv_enum;
        }

        /* if we didn't return an error already, we were successful */
        return HTML_attrerr_ok;
    }
}

/*
 *   Map a color to an appropriate system color 
 */
HTML_color_t CHtmlTag::map_color(CHtmlSysWin *win, HTML_color_t color)
{
    /* 
     *   if it's a special color, ask the window to map it to an RGB
     *   value; otherwise, it's already an RGB value, so return it
     *   unchanged 
     */
    if (html_is_color_special(color))
        return win->map_system_color(color);
    else
        return color;
}


/*
 *   Set a numeric attribute 
 */
HTML_attrerr CHtmlTag::set_number_attr(long *num_var,
                                       const textchar_t *val, size_t vallen)
{
    long acc;
    HTML_attrerr err;

    /* loop through the digits */
    for (err = HTML_attrerr_ok, acc = 0 ; vallen ; ++val, --vallen)
    {
        if (is_digit(*val))
        {
            /* accumulate one more digit */
            acc *= 10;
            acc += ascii_digit_to_int(*val);
        }
        else
        {
            /* generate an error and return */
            err = HTML_attrerr_inv_type;
            break;
        }
    }

    /* set the variable value */
    *num_var = acc;

    /* return any error code that we set */
    return err;
}

/*
 *   Set a coordinate attribute.  If pct is not null, we'll allow a
 *   percentage sign, and return in *pct whether or not a percentage sign
 *   was found.  Advances the string pointer to the next comma.  
 */
HTML_attrerr CHtmlTag::set_coord_attr(long *num_var, int *is_pct,
                                      const textchar_t **val,
                                      size_t *vallen)
{
    const textchar_t *start;
    size_t startlen;
    HTML_attrerr err;
    
    /* find the next comma */
    start = *val;
    startlen = *vallen;
    while (*vallen && **val != ',')
    {
        ++(*val);
        --(*vallen);
    }

    /* compute the length of the part up to the comma */
    startlen -= *vallen;

    /* parse the number up to the comma */
    if (is_pct == 0)
        err = set_number_attr(num_var, start, startlen);
    else
        err = set_number_or_pct_attr(num_var, is_pct, start, startlen);

    /* skip the comma and the whitespace after the comma */
    if (*vallen && **val == ',')
    {
        ++(*val);
        --(*vallen);
    }
    while (*vallen && is_space(**val))
    {
        ++(*val);
        --(*vallen);
    }

    /* return the result */
    return err;
}

/*
 *   Set a numeric attribute, which may be expressed either in absolute
 *   terms or as a percentage. 
 */
HTML_attrerr CHtmlTag::set_number_or_pct_attr(long *num_var, int *is_pct,
                                              const textchar_t *val,
                                              size_t vallen)
{
    /* 
     *   if the last character is a percent sign, it's relative;
     *   otherwise, it's absolute 
     */
    if (vallen > 0 && val[vallen - 1] == '%')
    {
        /* note that it's given as a percentage value */
        *is_pct = TRUE;

        /* remove the percent sign from the value */
        --vallen;
    }
    else
    {
        /* note that it's absolute */
        *is_pct = FALSE;
    }

    /* parse what's left as an ordinary number */
    return set_number_attr(num_var, val, vallen);
}


/*
 *   Set a hex numeric attribute 
 */
HTML_attrerr CHtmlTag::set_hex_attr(long *num_var,
                                    const textchar_t *val, size_t vallen)
{
    long acc;
    HTML_attrerr err;

    /* loop through the digits */
    for (err = HTML_attrerr_ok, acc = 0 ; vallen ; ++val, --vallen)
    {
        if (is_hex_digit(*val))
        {
            /* accumulate one more digit */
            acc <<= 4;
            acc += ascii_hex_to_int(*val);
        }
        else
        {
            /* generate an error and return */
            err = HTML_attrerr_inv_type;
            break;
        }
    }

    /* set the variable value */
    *num_var = acc;

    /* return any error code that we set */
    return err;
}


/*
 *   Get the next tag in format order.  When formatting, we start with
 *   the top container, then go to the first child of the container, then
 *   to it's first child, and so on; then to the next sibling of that tag,
 *   and so on; then to the next sibling of its container, then to its
 *   first child, and so on.  In effect, we do a depth-first traversal,
 *   but we process the container nodes on the way down.  An ordinary tag
 *   is not a container, so we only need to worry about moving to the next
 *   sibling or to the parent's next sibling.  
 */
CHtmlTag *CHtmlTag::get_next_fmt_tag(CHtmlFormatter *formatter)
{
    /* if I have a sibling, it's the next tag */
    if (get_next_tag() != 0)
        return get_next_tag();

    /*
     *   That's the end of the contents list for my container - tell the
     *   container we're leaving, and move on to its next sibling.  If we
     *   don't have a container, there are no more tags left to format.  
     */
    if (get_container() != 0)
    {        
        CHtmlTag *nxt;
        CHtmlTagContainer *cont;

        /*
         *   Find the container's next sibling.  If the container itself
         *   doesn't have a next sibling, find its container's next
         *   sibling, and so on until we find a sibling or run out of
         *   containers.
         */
        for (cont = get_container() ; cont != 0 ;
             cont = cont->get_container())
        {
            /* get the container's next sibling, and stop if it has one */
            nxt = cont->get_next_tag();
            if (nxt != 0)
                break;
        }

        /*
         *   If there is indeed another tag to format, or the tag is
         *   marked as "closed", tell our container that we're done
         *   formatting it.  If there isn't anything left, and the tag
         *   hasn't been closed yet, do NOT leave the container yet --
         *   more document parsing may occur that adds more items to the
         *   current container, in which case we'll still want the current
         *   container's settings in effect when we come back to do more
         *   formatting.  Hence, only exit the container if we have more
         *   work to do immediately, in which case we know that we'll
         *   never add anything more to our container.  Note that we need
         *   to inform as many containers as we're actually exiting, if
         *   we're exiting more than one level, so iterate up the
         *   containment tree until we find a container with a next
         *   sibling.  
         */
        for (cont = get_container() ; cont != 0 ;
             cont = cont->get_container())
        {
            /* 
             *   if this one isn't closed yet, and there's not another
             *   sibling, it's still open 
             */
            if (nxt == 0 && !cont->is_closed())
                break;
            
            /* tell this one we're exiting it */
            cont->format_exit(formatter);
            
            /* stop if this one has a sibling */
            if (cont->get_next_tag() != 0)
                break;
        }

        /* return the next tag after our container */
        return nxt;
    }

    /*
     *   there's nothing after us, and we have no container, so this is
     *   the end of the line 
     */
    return 0;
}

/*
 *   get the list nesting level 
 */
int CHtmlTag::get_list_nesting_level() const
{
        /* if I don't have a container, nesting level is zero */
    if (get_container() == 0)
        return 0;

        /* return my container's list nesting level */
    return (get_container()->get_list_nesting_level());
}

/*
 *   parse a CLEAR attribute value 
 */
HTML_attrerr CHtmlTag::set_clear_attr(HTML_Attrib_id_t *clear,
                                      CHtmlParser *parser,
                                      const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;
    
    /* get the attribute value, and check it for validity */
    switch(val_id = parser->attrval_to_id(val, vallen))
    {
    case HTML_Attrib_left:
    case HTML_Attrib_right:
    case HTML_Attrib_all:
        /* valid value - set it and return success */
        *clear = val_id;
        return HTML_attrerr_ok;

    default:
        /* not a valid CLEAR value */
        return HTML_attrerr_inv_enum;
    }
}

/*
 *   format a CLEAR setting 
 */
void CHtmlTag::format_clear(CHtmlFormatter *formatter,
                            HTML_Attrib_id_t clear)
{
    int changed;
    
    /*
     *   If we're to move past items in one of the margins, do so 
     */
    switch(clear)
    {
    case HTML_Attrib_left:
        changed = formatter->break_to_clear(TRUE, FALSE);
        break;

    case HTML_Attrib_right:
        changed = formatter->break_to_clear(FALSE, TRUE);
        break;

    case HTML_Attrib_all:
        changed = formatter->break_to_clear(TRUE, TRUE);
        break;

    default:
        changed = FALSE;
        break;
    }

    /* if we added any spacing, go to a new line */
    if (changed)
        formatter->add_disp_item_new_line(new (formatter) CHtmlDispBreak(0));
}

/*
 *   Find the end of a token in a delimited attribute list.  This simply
 *   scans a string until we find the next occurrence of a delimiter
 *   character, or the end of the string.  
 */
const textchar_t *CHtmlTag::find_attr_token(
    const textchar_t *val, size_t *vallen, const textchar_t *delims)
{
    /* scan until we find a delimiter or run out of input text */
    for ( ; *vallen != 0 ; ++val, --*vallen)
    {
        /* if this character is in the delimiter list, stop here */
        if (strchr(delims, *val) != 0)
            return val;
    }

    /* 
     *   we ran out of string without finding the delimiter - return a
     *   pointer to the character after the end of the string 
     */
    return val;
}

/* ------------------------------------------------------------------------ */
/*
 *   Container tag implementation 
 */

/*
 *   Destructor - delete sublists 
 */
CHtmlTagContainer::~CHtmlTagContainer()
{
    /* delete my sublist */
    delete_contents();
}

/*
 *   Pruning pre-deletion - pre-delete my children 
 */
void CHtmlTagContainer::prune_pre_delete(CHtmlTextArray *arr)
{
    CHtmlTag *cur;

    /* pass the notification on to my children */
    for (cur = contents_first_ ; cur != 0 ; cur = cur->get_next_tag())
        cur->prune_pre_delete(arr);
}

/*
 *   A container tag pushes itself onto the container stack upon being
 *   parsed 
 */
void CHtmlTagContainer::on_parse(CHtmlParser *parser)
{
    /* push onto parser container stack */
    parser->push_container(this);
}


/*
 *   Append a tag to the end of my sublist 
 */
void CHtmlTagContainer::append_to_sublist(CHtmlTag *tag)
{
    /* see if I have anything in my sublist yet */
    if (contents_last_)
    {
        /* add this item after the last item currently in my sublist */
        contents_last_->append(tag);

        /* it's the new sublist tail */
        contents_last_ = tag;
    }
    else
    {
        /* this is now the first, last, and only item in my sublist */
        contents_first_ = tag;
        contents_last_ = tag;
    }

    /* I'm the new item's container */
    tag->set_container(this);
}

/*
 *   Delete contents 
 */
void CHtmlTagContainer::delete_contents()
{
    CHtmlTag *tag;
    CHtmlTag *nxt;
    
    /* go through each item in my sublist and delete it */
    for (tag = contents_first_ ; tag ; tag = nxt)
    {
        /* remember the next thing in my sublist */
        nxt = tag->get_next_tag();

        /* delete this tag */
        delete tag;
    }

    /* there's nothing in my sublist now */
    contents_first_ = 0;
    contents_last_ = 0;
}


/*
 *   Get the next tag in format order.  The next tag after formatting a
 *   container is the first child of the container.  
 */
CHtmlTag *CHtmlTagContainer::get_next_fmt_tag(class CHtmlFormatter *formatter)
{
    /*
     *   if I have a child, return it; otherwise, return the next sibling
     *   or container's sibling (etc) as though we were an ordinary tag 
     */
    if (get_contents())
    {
        /* return my first child */
        return get_contents();
    }
    else
    {
        /*
         *   I have no children.  Call my format_exit() method right now.
         *   Normally, the last child would call this on the way out; since
         *   we have no children, though, no one would otherwise call this.
         *   Note that, as usual, we only want to call format_exit() if the
         *   tag is finished (i.e., closed).  
         */
        if (is_closed())
            format_exit(formatter);

        /*
         *   get the next item using the non-container algorithm; this
         *   will find our container's next sibling, or its container's
         *   next sibling, and so on, until we find a container with a
         *   sibling or run out of containers 
         */
        return CHtmlTag::get_next_fmt_tag(formatter);
    }
}

/*
 *   Re-insert my contents into the text array.  Runs through my contents
 *   and tells each to re-insert their contents. 
 */
void CHtmlTagContainer::reinsert_text_array(CHtmlTextArray *arr)
{
    CHtmlTag *cur;

    for (cur = get_contents() ; cur != 0 ; cur = cur->get_next_tag())
        cur->reinsert_text_array(arr);
}

/* ------------------------------------------------------------------------ */
/*
 *   Special outer container tag 
 */

void CHtmlTagOuter::format(CHtmlSysWin *win, CHtmlFormatter *)
{
    /* set default color scheme */
    win->set_html_bg_color(0, TRUE);
    win->set_html_text_color(0, TRUE);
    win->set_html_link_colors(0, TRUE, 0, TRUE, 0, TRUE, 0, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   TITLE tag 
 */

void CHtmlTagTITLE::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* put the formatter into title mode */
    formatter->begin_title();
}

void CHtmlTagTITLE::format_exit(CHtmlFormatter *formatter)
{
    /* tell the formatter this is the end of the title */
    formatter->end_title(this);

    /* 
     *   if we have some title text, display it directly -- if we've been
     *   pruned, our contents will have been deleted, hence we must rely
     *   on our internal record of the title 
     */
    if (title_.get() != 0)
        formatter->get_win()->set_window_title(title_.get());
}

/*
 *   We can prune the contents of a TITLE tag, even when we can't prune
 *   the TITLE tag itself 
 */
void CHtmlTagTITLE::try_prune_contents(CHtmlTextArray *txtarr)
{
    CHtmlTag *cur;
    CHtmlTag *nxt;

    /* delete each child */
    for (cur = contents_first_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next child */
        nxt = cur->get_next_tag();
        
        /* notify this child that it's about to be pruned */
        cur->prune_pre_delete(txtarr);

        /* delete it */
        delete cur;
    }

    /* forget about our former contents */
    contents_first_ = 0;
    contents_last_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   BLOCKQUOTE 
 */

void CHtmlTagBLOCKQUOTE::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    long indent;

    /* inherit default */
    CHtmlTagBlock::format(win, formatter);

    /* insert a line break */
    break_ht_ = win->measure_text(formatter->get_font(), " ", 1, 0).y / 2;
    formatter->add_line_spacing(break_ht_);

    /* indent a few en's in the current font */
    indent = win->measure_text(formatter->get_font(), "n", 1, 0).x * 5;
    formatter->push_margins(formatter->get_left_margin() + indent,
                            formatter->get_right_margin() + indent);
}

void CHtmlTagBLOCKQUOTE::format_exit(CHtmlFormatter *formatter)
{
    /* insert a line break */
    formatter->add_line_spacing(break_ht_);

    /* restore enclosing margins */
    formatter->pop_margins();

    /* inherit default */
    CHtmlTagBlock::format_exit(formatter);
}

/* ------------------------------------------------------------------------ */
/*
 *   Character set change tag
 */
int CHtmlTagCharset::add_attrs(class CHtmlFormatter *,
                               class CHtmlFontDesc *desc)
{
    /* use our character set, not the default */
    desc->default_charset = FALSE;
    desc->charset = charset_;

    /* indicate that we've changed the character set */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   CREDIT 
 */
CHtmlTagCREDIT::CHtmlTagCREDIT(CHtmlParser *parser)
{
    /* add our prefix to the text buffer */
    txtofs_ = parser->get_text_array()->append_text("---", 3);
}

/*
 *   pre-delete for pruning the tree - delete my text from the text array 
 */
void CHtmlTagCREDIT::prune_pre_delete(CHtmlTextArray *arr)
{
    /* tell the text array to delete my text */
    arr->delete_text(txtofs_, 3);

    /* inherit the default behavior */
    CHtmlTagFontCont::prune_pre_delete(arr);
}


void CHtmlTagCREDIT::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    long break_ht;

    /* insert a line break */
    break_ht = win->measure_text(formatter->get_font(), " ", 1, 0).y / 3;
    formatter->add_line_spacing(break_ht);

    /* right-align text in the credit */
    formatter->set_blk_alignment(HTML_Attrib_right);

    /* inherit default handling to set font attributes */
    CHtmlTagFontCont::format(win, formatter);

    /* add a text tag with an em dash */
    formatter->add_disp_item(formatter->get_disp_factory()->
                             new_disp_text(win, formatter->get_font(),
                                           "---", 3, txtofs_));
}

int CHtmlTagCREDIT::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if italics are already set, there's nothing to do */
    if (desc->italic)
        return FALSE;

    /* set italics and indicate a change */
    desc->italic = TRUE;
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Heading tags (H1 - H6)
 */

void CHtmlTagHeading::on_parse(CHtmlParser *parser)
{
    /* start a new paragraph */
    break_paragraph(parser, FALSE);

    /* inherit default */
    CHtmlTagBlock::on_parse(parser);
}

void CHtmlTagHeading::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagBlock::on_close(parser);

    /* start a new paragraph */
    break_paragraph(parser, FALSE);
}


/*
 *   parse attributes in a heading tag 
 */
HTML_attrerr CHtmlTagHeading::set_attribute(CHtmlParser *parser,
                                            HTML_Attrib_id_t attr_id,
                                            const textchar_t *val,
                                            size_t vallen)
{
    HTML_Attrib_id_t val_id;

    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* look up the alignment setting */
        val_id = parser->attrval_to_id(val, vallen);

        /* make sure it's valid - return an error if not */
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
        case HTML_Attrib_justify:
            align_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    default:
        /* return inherited default behavior */
        return CHtmlTagBlock::set_attribute(parser, attr_id, val, vallen);
    }
    
    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}


/*
 *   Add font attributes.  Headings are formatted at a particular HTML
 *   size (1-7), which depends on the particular heading subclass, in
 *   bold, and in the text font.  
 */
int CHtmlTagHeading::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    CHtmlFontDesc mydesc;
    
    /* ignore the current font descriptor, and set up our own settings */
    mydesc.weight = 700;
    mydesc.htmlsize = get_heading_fontsize();
    desc->copy_from(&mydesc);

    /* always report a change */
    return TRUE;
}

void CHtmlTagHeading::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* inherit default */
    CHtmlTagBlock::format(win, formatter);

    /* insert a full line break */
    break_ht_ = win->measure_text(formatter->get_font(), " ", 1, 0).y;
    formatter->add_line_spacing(break_ht_);

    /* set the alignment specified for the header */
    formatter->set_blk_alignment(align_);

    /* enter heading font */
    fontcont_format(win, formatter);
}

void CHtmlTagHeading::format_exit(CHtmlFormatter *formatter)
{
    /* exit heading font */
    fontcont_format_exit(formatter);

    /* insert a full line break */
    formatter->add_line_spacing(break_ht_);

    /* inherit default */
    CHtmlTagBlock::format_exit(formatter);
}


/* ------------------------------------------------------------------------ */
/*
 *   ADDRESS tag 
 */

void CHtmlTagADDRESS::on_parse(CHtmlParser *parser)
{
    /* start a new paragraph */
    break_paragraph(parser, FALSE);

    /* initialize the font-container add-in */
    fontcont_init();

    /* inherit default */
    CHtmlTagBlock::on_parse(parser);
}

void CHtmlTagADDRESS::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagBlock::on_close(parser);

    /* start a new paragraph */
    break_paragraph(parser, FALSE);
}

/*
 *   set font attributes 
 */
int CHtmlTagADDRESS::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if ADDRESS is already set, there's nothing to do */
    if (desc->pe_address)
        return FALSE;
    
    /* set ADDRESS and indicate a change */
    desc->pe_address = TRUE;
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   HR (horizontal rule) 
 */

CHtmlTagHR::~CHtmlTagHR()
{
    /* remove our reference on the image, if we have one */
    if (image_ != 0)
        image_->remove_ref();
}

void CHtmlTagHR::on_parse(CHtmlParser *parser)
{
    /* add a paragraph break before the rule */
    break_paragraph(parser, FALSE);

    /* inherit default handling */
    CHtmlTag::on_parse(parser);

    /* add a paragraph break after the rule */
    break_paragraph(parser, FALSE);
}

/*
 *   parse attributes 
 */
HTML_attrerr CHtmlTagHR::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* allow left, center, or right alignment */
        switch(align_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_left:
        case HTML_Attrib_right:
        case HTML_Attrib_center:
            /* no problem */
            break;

        default:
            /* invalid setting for ALIGN */
            align_ = HTML_Attrib_invalid;
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_noshade:
        /* ignore the value, set shading off */
        shade_ = FALSE;
        break;

    case HTML_Attrib_size:
        /* set height of the ruler */
        return set_number_attr(&size_, val, vallen);

    case HTML_Attrib_width:
        /* set the horizontal size */
        return set_number_or_pct_attr(&width_, &width_pct_, val, vallen);

    case HTML_Attrib_src:
        /* set the image source */
        src_.set_url(val, vallen);
        break;

    default:
        /* return inherited default behavior */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we made it this far, everything worked correctly */
    return HTML_attrerr_ok;
}

void CHtmlTagHR::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDispHR *disp;

    /* add a full line break before */
    formatter->add_line_spacing(win->measure_text(
        formatter->get_font(), " ", 1, 0).y);

    /* set the appropriate alignment in the formatter */
    formatter->set_blk_alignment(align_);

    /* 
     *   If we have a SRC attribute with an image, and haven't loaded the
     *   image yet, load it now.  Ignore the image if graphics are
     *   disabled.  
     */
    if (win->get_html_show_graphics())
    {
        if (src_.get_url() != 0 && image_ == 0)
        {
            /* 
             *   try to find the image in the cache, or load it if it's
             *   not already there 
             */
            image_ = formatter->get_res_cache()->
                 find_or_create(win, formatter->get_res_finder(), &src_);

            /* if it's not a valid image, log an error */
            if (image_ == 0 || image_->get_image() == 0)
                oshtml_dbg_printf("<HR>: resource %s is not a valid image\n",
                                  src_.get_url());

            /* count our reference to the cache object as long as we keep it */
            if (image_ != 0)
                image_->add_ref();
        }
    }
    else if (image_ != 0)
    {
        /* graphics aren't allowed - forget the image */
        image_->remove_ref();
        image_ = 0;
    }

    /* add a rule of the appropriate size */
    disp = new (formatter) CHtmlDispHR(win, shade_, size_,
                                       width_, width_pct_,
                                       formatter->get_left_margin(),
                                       formatter->get_right_margin(),
                                       image_);
    formatter->add_disp_item(disp);

    /* add a full line break after */
    formatter->add_line_spacing(win->measure_text(
        formatter->get_font(), " ", 1, 0).y);
}


/* ------------------------------------------------------------------------ */
/*
 *   List container - superclass for UL, OL, DL
 */

/*
 *   parse a list container 
 */
void CHtmlTagListContainer::on_parse(CHtmlParser *parser)
{
    /* inherit default to insert myself into the parser list */
    CHtmlTagBlock::on_parse(parser);

    /* set my nesting level to one higher than my container's */
    list_level_ = get_container()->get_list_nesting_level() + 1;
}

/* 
 *   pre-close the list - close any open list item before closing the list 
 */
void CHtmlTagListContainer::pre_close(CHtmlParser *parser)
{
    parser->close_tag_if_open(get_list_tag_name());
}

/*
 *   parse an attribute 
 */
HTML_attrerr CHtmlTagListContainer::set_attribute(
    CHtmlParser *parser, HTML_Attrib_id_t attr_id,
    const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_compact:
        /* note that we're compact */
        compact_ = TRUE;
        return HTML_attrerr_ok;

    default:
        /* return inherited default behavior */
        return CHtmlTagBlock::set_attribute(parser, attr_id, val, vallen);
    }
}

/*
 *   add attributes - we use this to add attributes for the list header 
 */
int CHtmlTagListContainer::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if the font is already bold, there's nothing to do */
    if (desc->weight >= 700)
        return FALSE;

    /* set bold weight and indicate a change */
    desc->weight = 700;
    return TRUE;
}

/* 
 *   format the start of the list 
 */
void CHtmlTagListContainer::format(CHtmlSysWin *win,
                                   CHtmlFormatter *formatter)
{
    /* inherit default */
    CHtmlTagBlock::format(win, formatter);

    /* 
     *   Insert a new line.  If this is the outermost list, insert a blank
     *   line's worth of vertical whitespace; if it's a nested list, just
     *   go to a new line without additional spacing.  
     */
    if (get_list_nesting_level() == 1)
        break_ht_ = win->measure_text(formatter->get_font(), " ", 1, 0).y / 2;
    else
        break_ht_ = 0;
    formatter->add_line_spacing(break_ht_);

    /* add attributes for the list header */
    fontcont_format(win, formatter);

    /* haven't started the list yet */
    list_started_ = FALSE;
}

/*
 *   begin the first list item - list items should call this before
 *   formatting themselves to make sure we've set up properly for entry
 *   into the list items 
 */
void CHtmlTagListContainer::begin_list_item(CHtmlSysWin *win,
                                            CHtmlFormatter *formatter)
{
    /* if I've already begun the list items, there's nothing to do */
    if (list_started_)
        return;

    /* note that we've started the list */
    list_started_ = TRUE;

    /* end the list header */
    fontcont_format_exit(formatter);

    /* make sure we're on a new line */
    formatter->add_line_spacing(0);
    
    /* indent the list by appropriate amount on the left */
    if (is_list_indented())
        formatter->push_margins(formatter->get_left_margin()
                                + get_left_indent(win, formatter),
                                formatter->get_right_margin());
}

/*
 *   get the default indenting 
 */
long CHtmlTagListContainer::get_left_indent(CHtmlSysWin *win,
                                            CHtmlFormatter *formatter) const
{
    static const textchar_t prefix[] = "99. ";
    static size_t prefix_len = sizeof(prefix)/sizeof(prefix[0]) - 1;

    /* make enough space for a 2-digit prefix */
    return win->measure_text(formatter->get_font(),
                             prefix, prefix_len, 0).x * 2;
}

void CHtmlTagListContainer::format_exit(CHtmlFormatter *formatter)
{
    /* 
     *   if we never started the list, we only need to reset the list
     *   header font attributes 
     */
    if (!list_started_)
    {
        /* remove the list header font settings */
        fontcont_format_exit(formatter);
    }
    else
    {
        /* 
         *   insert a line break with the same amount of vertical
         *   whitespace we used at the top of the list 
         */
        formatter->add_line_spacing(break_ht_);

        /* restore enclosing margins */
        if (is_list_indented())
            formatter->pop_margins();
    }

    /* inherit default */
    CHtmlTagBlock::format_exit(formatter);
}


/* ------------------------------------------------------------------------ */
/*
 *   UL - unordered list 
 */
HTML_attrerr CHtmlTagUL::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_type:
        /* get the bullet type */
        switch(type_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_disc:
        case HTML_Attrib_square:
        case HTML_Attrib_circle:
            return HTML_attrerr_ok;

        default:
            return HTML_attrerr_inv_enum;
        }

    case HTML_Attrib_plain:
        type_ = HTML_Attrib_plain;
        return HTML_attrerr_ok;

    case HTML_Attrib_src:
        src_.set_url(val, vallen);
        return HTML_attrerr_ok;

    default:
        /* return inherited default behavior */
        return CHtmlTagListContainer::
            set_attribute(parser, attr_id, val, vallen);
    }
}

/* 
 *   format the list 
 */
void CHtmlTagUL::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* inherit default */
    CHtmlTagListContainer::format(win, formatter);

    /* 
     *   figure out what type of bullet to use if a style hasn't been set
     *   explicitly 
     */
    if (type_ == HTML_Attrib_invalid)
    {
        switch(get_list_nesting_level())
        {
        case 1:
            /* first level - use a disc */
            type_ = HTML_Attrib_disc;
            break;

        case 2:
            /* second level - use a circle */
            type_ = HTML_Attrib_circle;
            break;

        default:
            /* anything further - use a square */
            type_ = HTML_Attrib_square;
            break;
        }
    }
}

/*
 *   set the bullet for an item 
 */
void CHtmlTagUL::set_item_bullet(CHtmlTagLI *item, CHtmlFormatter *)
{
    /* tell the tag to use my bullet and my source image */
    item->set_default_bullet(type_, &src_);
}

/*
 *   add a display item for the list header 
 */
void CHtmlTagUL::add_bullet_disp(CHtmlSysWin *win, CHtmlFormatter *formatter,
                                 CHtmlTagLI *item, long bullet_width)
{
    /* tell the tag to add a bullet display item */
    item->add_listhdr_bullet(win, formatter, bullet_width);
}


/* ------------------------------------------------------------------------ */
/*
 *   OL - unordered list 
 */
HTML_attrerr CHtmlTagOL::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_type:
        /* get the numbering style */
        if (vallen != 1)
            return HTML_attrerr_inv_enum;

        switch(style_ = val[0])
        {
        case '1':
        case 'a':
        case 'A':
        case 'i':
        case 'I':
            return HTML_attrerr_ok;

        default:
            return HTML_attrerr_inv_enum;
        }

    case HTML_Attrib_plain:
        style_ = 'p';
        return HTML_attrerr_ok;

    case HTML_Attrib_start:
    case HTML_Attrib_seqnum:
        /* get the starting number */
        return set_number_attr(&start_, val, vallen);

    case HTML_Attrib_continue:
        /* continue where last ordered list left off */
        continue_ = TRUE;
        return HTML_attrerr_ok;

    default:
        /* return inherited default behavior */
        return CHtmlTagListContainer::
            set_attribute(parser, attr_id, val, vallen);
    }
}

/*
 *   format the item 
 */
void CHtmlTagOL::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* inherit default */
    CHtmlTagListContainer::format(win, formatter);

    /* 
     *   set my value to the starting value - if we're continuing from a
     *   previous list, ask the formatter for the ending value of the old
     *   list, otherwise use the starting value from the attributes (or
     *   the default, if the attribute wasn't set) 
     */
    if (continue_)
        cur_value_ = formatter->get_ol_continue_seqnum();
    else
        cur_value_ = start_;

    /* if a numbering style isn't set, use regular numbers by default */
    if (style_ == '\0')
        style_ = '1';
}

/*
 *   set the bullet for an item 
 */
void CHtmlTagOL::set_item_bullet(CHtmlTagLI *item, CHtmlFormatter *formatter)
{
    /* tell the tag to use my current value and style */
    item->set_default_value(cur_value_);
    item->set_default_number_style(style_);

    /* set my value to the next value after this tag's value */
    cur_value_ = item->get_value() + 1;

    /* tell the formatter the last numbered list item */
    formatter->set_ol_continue_seqnum(cur_value_);
}

/*
 *   add a display item for the list header 
 */
void CHtmlTagOL::add_bullet_disp(CHtmlSysWin *win, CHtmlFormatter *formatter,
                                 CHtmlTagLI *item, long bullet_width)
{
    /* tell the tag to add a numeric list header display item */
    item->add_listhdr_number(win, formatter, bullet_width);
}

/* ------------------------------------------------------------------------ */
/*
 *   LI - list item 
 */

CHtmlTagLI::~CHtmlTagLI()
{
    /* if we have an image, remove our reference on it */
    if (image_ != 0)
        image_->remove_ref();
}

void CHtmlTagLI::on_parse(CHtmlParser *parser)
{
    /* if we're currently in a list header, end the list header */
    parser->close_tag_if_open("LH");
    
    /* if we're currently in a list item, end that list item */
    parser->close_tag_if_open("LI");

    /* add this tag as normal */
    CHtmlTagContainer::on_parse(parser);

    /* 
     *   add a space to the text array, so we find word breaks between
     *   list items 
     */
    parser->get_text_array()->append_text_noref(" ", 1);
}


/*
 *   parse an attribute for the list item when we're in an ordered list 
 */
HTML_attrerr CHtmlTagLI::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_type:
        /* if it's a single character, it must be a numbering style */
        if (vallen == 1)
        {
            switch(style_ = val[0])
            {
            case '1':
            case 'a':
            case 'A':
            case 'i':
            case 'I':
                return HTML_attrerr_ok;

            default:
                return HTML_attrerr_inv_enum;
            }
        }

        /* it must be a bullet type */
        switch(type_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_disc:
        case HTML_Attrib_square:
        case HTML_Attrib_circle:
            return HTML_attrerr_ok;
            
        default:
            return HTML_attrerr_inv_enum;
        }

    case HTML_Attrib_plain:
        type_ = HTML_Attrib_plain;
        return HTML_attrerr_ok;
        
    case HTML_Attrib_value:
        /* parse the value, and note that it's been set */
        value_set_ = TRUE;
        return set_number_attr(&value_, val, vallen);

    case HTML_Attrib_src:
        src_.set_url(val, vallen);
        return HTML_attrerr_ok;
        
    default:
        /* return inherited default behavior */
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }
}


/*
 *   format the list item 
 */
void CHtmlTagLI::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlTagListContainer *list;
    long bullet_width;
    
    /* get my list container - abort if not set */
    list = get_container()->get_list_container();
    if (list == 0)
        return;

    /* tell the list that we're starting a new item */
    list->begin_list_item(win, formatter);
    
    /* move to a new line */
    formatter->add_line_spacing(0);

    /* set my default bullet or value, as appropriate, if not specified */
    list->set_item_bullet(this, formatter);

    /* 
     *   make room for the bullet in the left margin by temporarily
     *   un-indenting by half of the list indent size; this temporary
     *   un-indenting will only last to the end of this line 
     */
    bullet_width = list->get_left_indent(win, formatter) / 2;
    formatter->set_temp_margins(formatter->get_left_margin() - bullet_width,
                                formatter->get_right_margin());
    
    /* add the appropriate bullet */
    list->add_bullet_disp(win, formatter, this, bullet_width);
}

/*
 *   Add a list header number display item
 */
void CHtmlTagLI::add_listhdr_number(CHtmlSysWin *win,
                                    CHtmlFormatter *formatter,
                                    long bullet_width)
{
    /* 
     *   if it's plain, add a plain bullet item, otherwise add a numeric
     *   list header item 
     */
    if (type_ == HTML_Attrib_plain || style_ == 'p')
        formatter->add_disp_item(new (formatter) CHtmlDispListitemBullet(
            win, win->get_bullet_font(formatter->get_font()),
            bullet_width, type_, 0));
    else
        formatter->add_disp_item(new (formatter) CHtmlDispListitemNumber(
            win, formatter->get_font(), bullet_width, style_, value_));
}

/*
 *   Add a list header bullet display item 
 */
void CHtmlTagLI::add_listhdr_bullet(CHtmlSysWin *win,
                                    CHtmlFormatter *formatter,
                                    long bullet_width)
{
    /* 
     *   if we have an image source, and we haven't loaded the image, do
     *   so now 
     */
    if (win->get_html_show_graphics())
    {
        if (src_.get_url() != 0)
        {
            /* load hte image if we haven't already */
            if (image_ == 0)
            {
                /* load it or find it in the cache */
                image_ = formatter->get_res_cache()->
                         find_or_create(win, formatter->get_res_finder(),
                                        &src_);
                
                /* if it's not a valid image, log an error */
                if (image_ == 0 || image_->get_image() == 0)
                    oshtml_dbg_printf("<LI>: resource %s is not a "
                                      "valid image\n", src_.get_url());

                /* add a reference to it as long as we keep the pointer */
                if (image_ != 0)
                    image_->add_ref();
            }
        }
    }
    else if (image_ != 0)
    {
        /* graphics aren't allowed - delete the image */
        image_->remove_ref();
        image_ = 0;
    }
    
    /* add a new bullet list item */
    formatter->add_disp_item(new (formatter) CHtmlDispListitemBullet(
        win, win->get_bullet_font(formatter->get_font()),
        bullet_width, type_, image_));
}

/* ------------------------------------------------------------------------ */
/*
 *   Definition list items 
 */

/* 
 *   parse a DT (definition term) item 
 */
void CHtmlTagDT::on_parse(CHtmlParser *parser)
{
    /* if there's an open DD item, implicitly close it */
    parser->close_tag_if_open("DD");

    /* parse this tag normally */
    CHtmlTagContainer::on_parse(parser);
}

/*
 *   format a DT (definition term) item 
 */
void CHtmlTagDT::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* start a new line */
    formatter->add_line_spacing(0);
}

/*
 *   parse a DD (definition list definition) item 
 */
void CHtmlTagDD::on_parse(CHtmlParser *parser)
{
    /* if there's an open DT item, implicitly close it */
    parser->close_tag_if_open("DT");

    /* parse this tag normally */
    CHtmlTagContainer::on_parse(parser);
}

/*
 *   format a DD (definition list definition) item
 */
void CHtmlTagDD::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlTagListContainer *list;

    /* get my list container - abort if not set */
    list = get_container()->get_list_container();
    if (list == 0)
        return;

    /* go to a new line */
    formatter->add_line_spacing(0);

    /* indent the definition on the left */
    formatter->push_margins(formatter->get_left_margin()
                            + list->get_left_indent(win, formatter),
                            formatter->get_right_margin());
}

/*
 *   finish formatting a DD item 
 */
void CHtmlTagDD::format_exit(CHtmlFormatter *formatter)
{
    /* go to a new line */
    formatter->add_line_spacing(0);
    
    /* restore margins */
    formatter->pop_margins();
}

/* ------------------------------------------------------------------------ */
/*
 *   Text tag 
 */
CHtmlTagText::CHtmlTagText(CHtmlTextArray *arr, CHtmlTextBuffer *buf)
{
    /* allocate space in the formatter's text array */
    len_ = buf->getlen();
    txtofs_ = arr->append_text(buf->getbuf(), len_);
}

CHtmlTagText::CHtmlTagText(CHtmlTextArray *arr, const textchar_t *buf,
                           size_t buflen)
{
    /* allocate space in the formatter's text array */
    len_ = buflen;
    txtofs_ = arr->append_text(buf, buflen);
}

CHtmlTagText::~CHtmlTagText()
{
}

/*
 *   pre-delete for pruning the tree - delete my text from the text array 
 */
void CHtmlTagText::prune_pre_delete(CHtmlTextArray *arr)
{
    /* tell the text array to delete my text */
    arr->delete_text(txtofs_, len_);
}

/*
 *   Format the text element 
 */
void CHtmlTagText::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *textobj;
    const textchar_t *txt;

    /* get the address of the memory containing the text */
    txt = formatter->get_text_ptr(txtofs_);

    /* create a new text display item, and add it to the formatter's list */
    textobj = formatter->get_disp_factory()->
              new_disp_text(win, formatter->get_font(), txt, len_, txtofs_);
    formatter->add_disp_item(textobj);
}

/*
 *   Re-insert my text into the text array 
 */
void CHtmlTagText::reinsert_text_array(CHtmlTextArray *arr)
{
    unsigned long old_txtofs;

    /* remember our original text offset so we can delete it momentarily */
    old_txtofs = txtofs_;
    
    /* 
     *   put a new copy of my text into the text array, and remember the
     *   new location 
     */
    txtofs_ = arr->append_text(arr->get_text(txtofs_), len_);

    /* 
     *   delete our old copy (do this *after* we insert the new copy, to
     *   make sure that we don't delete the underlying page accidentally
     *   by leaving it unreferenced for a moment) 
     */
    arr->delete_text(old_txtofs, len_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Preformatted text tag 
 */

CHtmlTagTextPre::CHtmlTagTextPre(int tab_size, CHtmlTextArray *arr,
                                 CHtmlTextBuffer *buf)
{
    init_text(tab_size, arr, buf->getbuf(), buf->getlen());
}

CHtmlTagTextPre::CHtmlTagTextPre(int tab_size, CHtmlTextArray *arr,
                                 const textchar_t *buf, size_t buflen)
{
    init_text(tab_size, arr, buf, buflen);
}

/*
 *   initialize the text - used by the constructor 
 */
void CHtmlTagTextPre::init_text(int tab_size, CHtmlTextArray *arr,
                                const textchar_t *buf, size_t buflen)
{
    int col;
    int target;
    size_t expanded_len;
    const textchar_t *p;
    size_t rem;

    /* 
     *   First, figure out how much space we'll need.  Since we need our
     *   text to be in a single chunk, we'll need to know how much space
     *   to reserve in advance, then we'll fill in the space after making
     *   the reservation. 
     */
    for (p = buf, rem = buflen, col = 0, expanded_len = 0 ; rem != 0 ;
         ++p, --rem)
    {
        switch(*p)
        {
        case '\t':
            /* tab - figure out how much space we'll need for expansion */
            target = (col/tab_size + 1) * tab_size;
            expanded_len += target - col;
            col = target;
            break;

        default:
            /* normal character - use one column */
            ++col;
            ++expanded_len;
            break;
        }
    }

    /*
     *   Reserve the necessary amount of space.  This will ensure that our
     *   entire chunk of text is stored contiguously, even though we'll be
     *   adding it to the array with multiple append_text calls.  
     */
    txtofs_ = arr->reserve_space(expanded_len);

    /*
     *   Now go through the buffer and append it to the text array.  For
     *   efficiency, append in as large pieces as we can.  
     */
    for (p = buf, rem = buflen, col = 0 ; rem != 0 ; ++p, --rem)
    {
        switch(*p)
        {
        case '\t':
            /* tab character - insert the expansion using spaces */
            {
                size_t sprem;

                /* append the chunk from the last tab to here */
                if (p != buf)
                    arr->append_text_noref(buf, p - buf);
                
                /* figure out how many spaces we need to add */
                target = (col/tab_size + 1) * tab_size;

                /* add the spaces */
                for (sprem = (size_t)(target - col) ; sprem != 0 ; )
                {
                    static const textchar_t spaces[] = "               ";
                    size_t spcur;

                    /* 
                     *   figure out how many we can insert from our static
                     *   space array - insert as many as we can, up to the
                     *   array size 
                     */
                    spcur = sprem;
                    if (spcur > sizeof(spaces)/sizeof(spaces[0]) - 1)
                        spcur = sizeof(spaces)/sizeof(spaces[0]) - 1;

                    /* insert the spaces */
                    arr->append_text_noref(spaces, spcur);

                    /* deduct the amount we just did from the total */
                    sprem -= spcur;
                }
                
                /* 
                 *   remember the location of the next character - it's
                 *   the start of the next chunk after this tab 
                 */
                buf = p + 1;
                
                /* note that we're at the target column now */
                col = target;
            }
            break;

        default:
            /* 
             *   simply count it for now - we'll actually append it when
             *   we get to the next tab stop or the end of the string 
             */
            ++col;
            break;
        }
    }

    /*
     *   If there's anything left over from the last tab, insert it now 
     */
    if (p != buf)
        arr->append_text_noref(buf, p - buf);

    /*
     *   Remember my length as the fully expanded length 
     */
    len_ = expanded_len;
}

/*
 *   Format the text element 
 */
void CHtmlTagTextPre::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *textobj;
    const textchar_t *txt;

    /* get the address of the memory containing the text */
    txt = formatter->get_text_ptr(txtofs_);

    /* create a new text display item, and add it to the formatter's list */
    textobj = formatter->get_disp_factory()->
              new_disp_text_pre(win, formatter->get_font(),
                                txt, len_, txtofs_);
    formatter->add_disp_item(textobj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Command input tag 
 */

CHtmlTagTextInput::CHtmlTagTextInput(CHtmlTextArray *txtarr,
                                     const textchar_t *buf, size_t len)
{
    /*
     *   Since the caller will manage space until the command is
     *   committed, do not allocate space yet; however, do get the
     *   tentative array address, so that it's clear where we are in the
     *   text stream. 
     */
    txtofs_ = txtarr->store_text_temp(0, 0);

    /* save the buffer information for editing */
    len_ = len;
    buf_ = buf;
}

/*
 *   Format the tag 
 */
void CHtmlTagTextInput::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDispTextInput *textobj;

    /* see if I'm being edited */
    if (buf_ != 0)
    {
        /* 
         *   I'm being edited - use my internal buffer for the text.
         *   Create in the system heap (by using null as the formatter
         *   placement argument for operator new) rather than in the
         *   formatter's heap, because the formatter's heap doesn't ever
         *   delete items individually; when we're open for editing, we'll
         *   want to be able to repeatedly create and delete temporary
         *   display items, so we want to make sure they really get
         *   deleted. 
         */
        textobj = formatter->get_disp_factory()->
                  new_disp_text_input(win, formatter->get_font(),
                                      buf_, len_, txtofs_);

        /*
         *   add the display item to the formatter's list, but tell it
         *   that it's replacing the current command 
         */
        formatter->add_disp_item_input(textobj, this);
    }
    else
    {
        /* no longer being edited - use the default implementation */
        CHtmlTagText::format(win, formatter);
    }
}

/*
 *   Change to a new buffer.  This can be used when we resume an interrupted
 *   editing session.  
 */
void CHtmlTagTextInput::change_buf(CHtmlFormatterInput *fmt,
                                   const textchar_t *buf)
{
    /* remember my new buffer */
    buf_ = buf;

    /* store the current text in the text array */
    txtofs_ = fmt->store_input_text_temp(buf_, len_);
}

/*
 *   Update the length of the underlying buffer
 */
void CHtmlTagTextInput::setlen(CHtmlFormatterInput *fmt, size_t len)
{
    /* update our internal length */
    len_ = len;

    /* store the current text in the text array */
    txtofs_ = fmt->store_input_text_temp(buf_, len_);
}

/*
 *   Commit the text.  This stores it in the text array. 
 */
void CHtmlTagTextInput::commit(CHtmlTextArray *arr)
{
    /* store it in the array, and note the final address */
    txtofs_ = arr->append_text(buf_, len_);

    /* forget the editing buffer, since we're done editing now */
    buf_ = 0;
}

/*
 *   Re-insert my text into the text array 
 */
void CHtmlTagTextInput::reinsert_text_array(CHtmlTextArray *arr)
{
    /* 
     *   if I haven't been committed, do nothing, since I'm not in my
     *   final position in the text array yet; if I've been committed,
     *   inherit the default behavior 
     */
    if (buf_ == 0)
        CHtmlTagText::reinsert_text_array(arr);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger source text 
 */

CHtmlTagTextDebugsrc::CHtmlTagTextDebugsrc(unsigned long linenum,
                                           int tab_size,
                                           CHtmlTextArray *arr,
                                           const textchar_t *buf,
                                           size_t buflen)
    : CHtmlTagTextPre(tab_size, arr, buf, buflen)
{
    /* remember my line number */
    linenum_ = linenum;

    /* 
     *   add a newline to the text array, so we find a word break between
     *   this line and the next one o
     */
    arr->append_text_noref("\n", 1);
}

void CHtmlTagTextDebugsrc::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *textobj;
    const textchar_t *txt;
    
    /* get the address of the memory containing the text */
    txt = formatter->get_text_ptr(txtofs_);

    /* create a new debug source text display item */
    textobj = formatter->get_disp_factory()->
              new_disp_text_debug(win, formatter->get_font(),
                                  txt, len_, txtofs_, linenum_);

    /* add the item on a new line */
    formatter->add_disp_item_new_line(textobj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Soft hyphen element 
 */

void CHtmlTagSoftHyphen::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *disp;
    
    /* create a soft hyphen display item */
    disp = formatter->get_disp_factory()->
           new_disp_soft_hyphen(win, formatter->get_font(),
                                formatter->get_text_ptr(txtofs_),
                                len_, txtofs_);

    /* add the item to the display list */
    formatter->add_disp_item(disp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Non-breaking space element
 */

void CHtmlTagNBSP::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *disp;

    /* create a non-breaking space display item */
    disp = formatter->get_disp_factory()->
           new_disp_nbsp(win, formatter->get_font(),
                         formatter->get_text_ptr(txtofs_),
                         len_, txtofs_);

    /* add the item to the display list */
    formatter->add_disp_item(disp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Special explicit-width space element 
 */

void CHtmlTagSpace::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDisp *disp;

    /* create a typographical space display item */
    disp = formatter->get_disp_factory()->
           new_disp_space(win, formatter->get_font(),
                          formatter->get_text_ptr(txtofs_),
                          len_, txtofs_, wid_);

    /* add the item to the display list */
    formatter->add_disp_item(disp);
}


/* ------------------------------------------------------------------------ */
/*
 *   <BR> tag 
 */

CHtmlTagBR::CHtmlTagBR(CHtmlParser *)
{
    /* assume no CLEAR attribute is provided */
    clear_ = HTML_Attrib_invalid;

    /* assume we'll use the default spacing */
    height_ = 1;
}

void CHtmlTagBR::on_parse(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTag::on_parse(parser);
    
    /*
     *   add a newline to the text array, so that this shows up as a word
     *   break, and so that it works nicely with copying to the system
     *   clipboard 
     */
    parser->get_text_array()->append_text_noref("\n", 1);

    /* skip any whitespace between the <BR> and the next text */
    parser->begin_skip_sp();
}

/*
 *   Format a line break
 */
void CHtmlTagBR::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* process any CLEAR setting */
    format_clear(formatter, clear_);

    /*
     *   If we're on a blank line, or we're explicitly adding extra
     *   spacing, add some vertical whitespace; otherwise, just end the
     *   current line.  
     */
    if (height_ > 1 || formatter->last_was_newline())
    {
        int ht;

        /* 
         *   Add some vertical whitespace -- space by half the font height
         *   for each height unit we're adding. 
         */
        ht = win->measure_text(formatter->get_font(), " ", 1, 0).y / 2
             * height_;
        formatter->add_line_spacing(ht);
    }

    /* end the current line, but don't add any vertical whitespace */
    formatter->add_disp_item_new_line(new (formatter) CHtmlDispBreak(0));
}

/*
 *   set attributes 
 */
HTML_attrerr CHtmlTagBR::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_clear:
        return set_clear_attr(&clear_, parser, val, vallen);

    case HTML_Attrib_height:
        /* read the HEIGHT setting */
        return set_number_attr(&height_, val, vallen);

    default:
        /* return inherited default behavior */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   <Body> tag 
 */
CHtmlTagBODY::~CHtmlTagBODY()
{
    /* remove our reference on the image object, if we have one */
    if (bg_image_ != 0)
        bg_image_->remove_ref();
}


HTML_attrerr CHtmlTagBODY::set_attribute(CHtmlParser *parser,
                                         HTML_Attrib_id_t attr_id,
                                         const textchar_t *val, size_t vallen)
{
    HTML_attrerr err;
    
    switch(attr_id)
    {
    case HTML_Attrib_background:
        /* store the background image URL */
        background_.set_url(val, vallen);
        break;

    case HTML_Attrib_bgcolor:
        /* parse the color */
        err = set_color_attr(parser, &bgcolor_, val, vallen);

        /* if we got a valid color, note that we want to use it*/
        if (err == HTML_attrerr_ok)
            use_bgcolor_ = TRUE;

        /* return the result of parsing the color */
        return err;

    case HTML_Attrib_text:
        /* parse the color */
        err = set_color_attr(parser, &textcolor_, val, vallen);

        /* if we got a valid color, note that we want to use it */
        if (err == HTML_attrerr_ok)
            use_textcolor_ = TRUE;

        /* return the result of parsing the color */
        return err;

    case HTML_Attrib_link:
        /* parse the link color */
        err = set_color_attr(parser, &link_color_, val, vallen);
        if (err == HTML_attrerr_ok)
            use_link_color_ = TRUE;
        return err;

    case HTML_Attrib_vlink:
        /* parse the vlink color */
        err = set_color_attr(parser, &vlink_color_, val, vallen);
        if (err == HTML_attrerr_ok)
            use_vlink_color_ = TRUE;
        return err;

    case HTML_Attrib_alink:
        /* parse the link color */
        err = set_color_attr(parser, &alink_color_, val, vallen);
        if (err == HTML_attrerr_ok)
            use_alink_color_ = TRUE;
        return err;

    case HTML_Attrib_hlink:
        /* parse the link hover color */
        err = set_color_attr(parser, &hlink_color_, val, vallen);
        if (err == HTML_attrerr_ok)
            use_hlink_color_ = TRUE;
        return err;

    default:
        /* return inherited default behavior */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}

void CHtmlTagBODY::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* 
     *   if there's a background, use it, otherwise use the background
     *   color 
     */
    if (background_.get_url() != 0)
    {
        /* load the image if we haven't already */
        if (win->get_html_show_graphics())
        {
            if (bg_image_ == 0)
            {
                /* load it into the cache, or find an existing copy */
                bg_image_ = formatter->get_res_cache()
                            ->find_or_create(win, formatter->get_res_finder(),
                                             &background_);
                
                /* if it's not a valid image, log an error */
                if (bg_image_ == 0 || bg_image_->get_image() == 0)
                    oshtml_dbg_printf("<BODY BACKGROUND>: "
                                      "resource %s is not a valid image\n",
                                      background_.get_url());
                
                /* add a reference to it, since we're keeping a pointer */
                if (bg_image_ != 0)
                    bg_image_->add_ref();
            }
        }
        else if (bg_image_ != 0)
        {
            /* graphics aren't allowed - delete the image */
            bg_image_->remove_ref();
            bg_image_ = 0;
        }

        /* set the background image */
        win->set_html_bg_image(bg_image_);

        /* 
         *   if we have an image, link it up to the display site for the
         *   special BODY display item 
         */
        if (bg_image_ != 0 && bg_image_->get_image() != 0
            && formatter->get_body_disp() != 0)
        {
            /* 
             *   Set the display site that's part of the special BODY display
             *   item to display the new image.  This will form the two-way
             *   link (i.e., it'll set the image's display site to the BODY
             *   item).  
             */
            formatter->get_body_disp()->set_site_image(bg_image_);
        }
    }
    else
    {
        /* no background image */
        win->set_html_bg_image(0);
    }

    /* tell the window to use my background color, if one is set */
    win->set_html_bg_color(map_color(win, bgcolor_), !use_bgcolor_);

    /* tell the window to use my text color, if one is set */
    win->set_html_text_color(map_color(win, textcolor_), !use_textcolor_);

    /* tell the window what to do with the link colors */
    win->set_html_link_colors(map_color(win, link_color_), !use_link_color_,
                              map_color(win, vlink_color_), !use_vlink_color_,
                              map_color(win, alink_color_), !use_alink_color_,
                              map_color(win, hlink_color_), !use_hlink_color_
                             );
}

/* ------------------------------------------------------------------------ */
/*
 *   Block-level tag base class
 */

HTML_attrerr CHtmlTagBlock::set_attribute(CHtmlParser *parser,
                                          HTML_Attrib_id_t attr_id,
                                          const textchar_t *val,
                                          size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_clear:
        return set_clear_attr(&clear_, parser, val, vallen);

    case HTML_Attrib_nowrap:
        nowrap_ = TRUE;
        break;

    default:
        /* return inherited default behavior */
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}

void CHtmlTagBlock::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* if we're to move past marginal items, do so */
    format_clear(formatter, clear_);

    /* set line wrapping mode */
    old_wrap_ = formatter->get_line_wrapping();
    formatter->set_line_wrapping(!nowrap_);
}

void CHtmlTagBlock::format_exit(CHtmlFormatter *formatter)
{
    /* restore old line wrapping state */
    formatter->set_line_wrapping(old_wrap_);
}


/* ------------------------------------------------------------------------ */
/*
 *   <P> (paragraph) tag 
 */

void CHtmlTagP::init(int isexplicit, CHtmlParser *prs)
{
    /* specify no alignment by default */
    align_ = HTML_Attrib_invalid;

    /* remember whether the paragraph break is explicit or not */
    explicit_ = isexplicit;

    /*
     *   add a newline to the text array, so that this shows up as a word
     *   break, and so that it works nicely with copying to the system
     *   clipboard 
     */
    prs->get_text_array()->append_text_noref("\n", 1);

    /* presume we'll wrap lines normally */
    nowrap_ = FALSE;

    /* presume no CLEAR setting */
    clear_ = HTML_Attrib_invalid;
}

/* process the ending tag */
void CHtmlTagP::process_end_tag(CHtmlParser *parser,
                                const textchar_t *, size_t)
{
    /* the </P> tag requires special treatment in the parser */
    parser->end_p_tag();
}

/* set an attribute in the <P> tag */
HTML_attrerr CHtmlTagP::set_attribute(CHtmlParser *parser,
                                      HTML_Attrib_id_t attr_id,
                                      const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;
    
    switch(attr_id)
    {
    case HTML_Attrib_clear:
        return set_clear_attr(&clear_, parser, val, vallen);

    case HTML_Attrib_align:
        /* look up the alignment setting */
        val_id = parser->attrval_to_id(val, vallen);

        /* make sure it's valid - return an error if not */
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
        case HTML_Attrib_justify:
            align_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_nowrap:
        nowrap_ = TRUE;
        break;
        
    default:
        /* return inherited default behavior */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}

/*
 *   Parse a paragraph marker 
 */
void CHtmlTagP::on_parse(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTag::on_parse(parser);

    /* 
     *   skip any whitespace between the <P> and the first text in the new
     *   paragraph 
     */
    parser->begin_skip_sp();
}

/*
 *   Format a paragraph marker
 */
void CHtmlTagP::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    int ht;

    /* process any CLEAR setting */
    format_clear(formatter, clear_);

    /*
     *   get the height of the break - if there's an explicit paragraph
     *   break, add in a blank line, otherwise add no additional vertical
     *   whitespace 
     */
    if (explicit_)
    {
        /* add in the height of two lines */
        ht = win->measure_text(formatter->get_font(), " ", 1, 0).y / 2;
    }
    else
    {
        /* implicit break - don't add any extra vertical space */
        ht = 0;
    }

    /* tell the formatter to go to a new line with this spacing */
    formatter->add_line_spacing(ht);

    /*
     *   Set the formatter alignment for this block element.  Note that
     *   we waited until after breaking the line (with add_line_spacing)
     *   to do this, since we don't want the new alignment to affect the
     *   tail end of the last paragraph. 
     */
    formatter->set_blk_alignment(align_);

    /* set line wrapping mode */
    formatter->set_line_wrapping(!nowrap_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Division tags 
 */

HTML_attrerr CHtmlTagDIV::set_attribute(CHtmlParser *parser,
                                        HTML_Attrib_id_t attr_id,
                                        const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;
    HTML_attrerr err;

    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* look up the alignment setting */
        val_id = parser->attrval_to_id(val, vallen);
        
        /* make sure it's valid - return an error if not */
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
        case HTML_Attrib_justify:
            align_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_hover:
        /*
         *   This sets the hover attributes for the division.  The general
         *   syntax is
         *   
         *   <DIV HOVER=item,item,...>
         *   
         *   where each 'item' can be one of the following:
         *   
         *   LINK - indicates that when the mouse is hovering anywhere over
         *   the division, all links in the division should highlight with
         *   the current <body hlink> color, if set, else with the standard
         *   system hover color.
         *   
         *   LINK(fgcolor,bgcolor,decoration) - same as LINK, but also
         *   specifies the colors and decorations to be shown for links
         *   within the division when the mouse is hovering over the
         *   division.  bgcolor and decoration are optional, and can simply
         *   be left blank to take the defaults; a comma can be ommitted if
         *   everything following it is left blank.
         *   
         *   No whitespace is allowed within the attribute value.  
         */
        const textchar_t *start;
        const textchar_t *p;
        size_t rem;

        /* scan until we run out of items */
        for (p = val, rem = vallen ;; )
        {
            size_t len;

            /* 
             *   scan for the end of the keyword - it ends at a comma or open
             *   paren (or the end of the string, of course) 
             */
            start = p;
            p = find_attr_token(p, &rem, ",(");

            /* note the length of the keyword */
            len = p - start;

            /* determine which keyword we have */
            if (len == 4 && memicmp(start, "link", 4) == 0)
            {
                /* note that we use HOVER=LINK mode */
                hover_link_ = TRUE;

                /* if the hover color is specified, scan it */
                if (rem != 0 && *p == '(')
                {
                    /* skip the paren */
                    ++p, --rem;

                    /* scan for the end of the foreground color keyword */
                    start = p;
                    p = find_attr_token(p, &rem, ",)");

                    /* if we have a non-blank color name, use it */
                    if ((len = p - start) != 0)
                    {
                        /* parse the foreground color */
                        err = set_color_attr(parser, &hover_fg_, start, len);

                        /* return failure on error */
                        if (err != HTML_attrerr_ok)
                            return err;

                        /* note that we have a foreground color */
                        use_hover_fg_ = TRUE;
                    }

                    /* parse the background color if it's there */
                    len = 0;
                    if (rem != 0 && *p == ',')
                    {
                        /* skip the comma */
                        ++p, --rem;

                        /* scan for the background color */
                        start = p;
                        p = find_attr_token(p, &rem, ",)");

                        /* note the length */
                        len = p - start;
                    }
                    
                    /* if we have a non-blank color name, use it */
                    if (len != 0)
                    {
                        /* parse the color */
                        err = set_color_attr(parser, &hover_bg_, start, len);
                        
                        /* return failure on error */
                        if (err != HTML_attrerr_ok)
                            return err;
                        
                        /* note that we have a background color */
                        use_hover_bg_ = TRUE;
                    }

                    /* parse the decoration if it's there */
                    len = 0;
                    if (rem != 0 && *p == ',')
                    {
                        /* skip the comma */
                        ++p, --rem;

                        /* scan for the background color */
                        start = p;
                        p = find_attr_token(p, &rem, ",)");

                        /* note the length */
                        len = p - start;
                    }

                    /* if we found a decoration, parse it */
                    if (len != 0)
                    {
                        if (len == 9 && memicmp(start, "underline", 9) == 0)
                            hover_underline_ = TRUE;
                        else
                            return HTML_attrerr_inv_enum;
                    }

                    /* we need to be looking at a close paren now */
                    if (rem == 0 || *p != ')')
                        return HTML_attrerr_missing_delim;

                    /* skip the paren */
                    ++p, --rem;
                }
            }
            else
            {
                /* unrecognized keyword */
                return HTML_attrerr_inv_enum;
            }

            /* if we're at the end of the attribute, we're done */
            if (rem == 0)
                break;

            /* if there's a comma, skip it; if there's not, it's an error */
            if (*p == ',')
            {
                /* skip the comma */
                ++p, --rem;
            }
            else
            {
                /* we need a comma to separate items */
                return HTML_attrerr_missing_delim;
            }
        }

        /* done with the tag */
        break;
        
    default:
        /* return inherited default behavior */
        return CHtmlTagBlock::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}

/*
 *   Parse 
 */
void CHtmlTagDIV::on_parse(CHtmlParser *parser)
{
    /* note our starting text offset */
    start_txtofs_ = parser->get_text_array()->append_text(0, 0);

    /* inherit default */
    CHtmlTagDIV_base::on_parse(parser);
}

/*
 *   Finish parse 
 */
void CHtmlTagDIV::on_close(CHtmlParser *parser)
{
    /* note our ending text offset */
    end_txtofs_ = parser->get_text_array()->append_text(0, 0);

    /* inherit default */
    CHtmlTagDIV_base::on_close(parser);
}

/*
 *   post-close processing 
 */
void CHtmlTagDIV::post_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagDIV_base::post_close(parser);

    /* 
     *   Add a 'relax' tag so that we have something to traverse into if
     *   we're at the very end of the format list - this ensures that we'll
     *   be closed properly when we're formatted at the end of the list. 
     */
    parser->append_tag(new CHtmlTagRelax());
}

/*
 *   Format a DIV tag 
 */
void CHtmlTagDIV::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* do division formatting with our set alignment */
    div_format(formatter, align_);

    /*
     *   If this division is active - specifically, if it has the HOVER
     *   attribute - we need to create a display list item for it. 
     */
    if (hover_link_)
    {
        CHtmlDispDIV *div;
        CHtmlDispLinkDIV *link;

        /* create a display item, and add it to the formatter's list */
        div = new (formatter) CHtmlDispDIV(start_txtofs_, end_txtofs_);
        formatter->begin_div(div);
    
        /* create the DIV-link for the division */
        link = new (formatter) CHtmlDispLinkDIV(div);

        /* set the link's hover colors */
        if (use_hover_fg_)
            link->set_hover_fg(map_color(win, hover_fg_));
        if (use_hover_bg_)
            link->set_hover_bg(map_color(win, hover_bg_));
        if (hover_underline_)
            link->set_hover_underline();

        /* 
         *   add the link to the formatter list - note that we don't add it
         *   as a full hyperlink, since it's not a real link and so doesn't
         *   generate any contents linked to it 
         */
        formatter->add_disp_item(link);
    }

    /* inherit default */
    CHtmlTagDIV_base::format(win, formatter);
}

/*
 *   End the current division 
 */
void CHtmlTagDIV::format_exit(CHtmlFormatter *formatter)
{
    /* if we're active, we need to close out our display list presence */
    if (hover_link_)
    {
        /* tell the formatter we're exiting */
        formatter->end_div();
    }

    /* inherit default */
    CHtmlTagDIV_base::format_exit(formatter);
}

void CHtmlTagDIV::prune_pre_delete(class CHtmlTextArray *arr)
{
    /* delete our start and end markers */
    arr->delete_text(start_txtofs_, 0);
    arr->delete_text(end_txtofs_, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Common base class for DIV-like tags - specifically DIV and CENTER 
 */

/*
 *   Start a new paragraph whenever we start a new division 
 */
void CHtmlTagDIV_base::on_parse(CHtmlParser *parser)
{
    /* start a new paragraph */
    break_paragraph(parser, FALSE);

    /* inherit default */
    CHtmlTagBlock::on_parse(parser);
}

/*
 *   Start a new paragraph whenever we end a division 
 */
void CHtmlTagDIV_base::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagBlock::on_close(parser);

    /* start a new paragraph */
    break_paragraph(parser, FALSE);
}
/*
 *   do formatting for a DIV or CENTER tag 
 */
void CHtmlTagDIV_base::div_format(CHtmlFormatter *formatter,
                                  HTML_Attrib_id_t alignment)
{
    /* remember the old alignment */
    old_align_ = formatter->get_div_alignment();

    /* set the new alignment */
    formatter->set_div_alignment(alignment);
}

/*
 *   leave a DIV or CENTER section 
 */
void CHtmlTagDIV_base::format_exit(CHtmlFormatter *formatter)
{
    /* make sure we finish up the current line before changing alignment */
    formatter->add_line_spacing(0);

    /* restore the old alignment */
    formatter->set_div_alignment(old_align_);
}



/* ------------------------------------------------------------------------ */
/*
 *   Font container add-in class
 */

/*
 *   start a font container - set the appropriate font attributes in the
 *   formatter 
 */
void CHtmlFontContAddin::fontcont_format(CHtmlSysWin *win,
                                         CHtmlFormatter *formatter)
{
    CHtmlFontDesc font_desc;
    
    /* remember the current font */
    old_font_ = formatter->get_font();

    /* get a description of the current font */
    old_font_->get_font_desc(&font_desc);

    /*
     *   add my attributes to the description, and note if doing so
     *   changed anything 
     */
    if (add_attrs(formatter, &font_desc))
    {
        CHtmlSysFont *new_font;
        
        /* we've changed the font - create a font for the new description */
        new_font = win->get_font(&font_desc);

        /* install the new font as the current formatter font */
        formatter->set_font(new_font);
    }
}

/*
 *   end a font container - remove the font attributes 
 */
void CHtmlFontContAddin::fontcont_format_exit(CHtmlFormatter *fmt)
{
    /* 
     *   restore the font in effect before we added our attributes (if we
     *   don't have an old font, don't bother - we must be unbalanced, so
     *   that we're closing more times than we opened) 
     */
    if (old_font_ != 0)
        fmt->set_font(old_font_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Explicit font tag 
 */

/*
 *   format
 */
void CHtmlTagFontExplicit::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* remember the old font */
    old_font_ = formatter->get_font();

    /* establish the new font */
    formatter->set_font(win->get_font(&font_));
}

/* 
 *   exit formatting 
 */
void CHtmlTagFontExplicit::format_exit(CHtmlFormatter *formatter)
{
    /* 
     *   restore the old font, if we have one (if we don't, there must be an
     *   error in the HTML, specifically an unbalanced tag; ignore such
     *   cases for more graceful failure) 
     */
    if (old_font_ != 0)
        formatter->set_font(old_font_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Verbatim tags 
 */

void CHtmlTagVerbatim::on_parse(CHtmlParser *parser)
{
    /* start with a paragraph break */
    break_paragraph(parser, FALSE);

    /* inherit default */
    CHtmlTagContainer::on_parse(parser);

    /* put the parser into obey-whitespace mode */
    parser->obey_whitespace(TRUE, FALSE);

    /* if necessary, put parser into literal-tags mode */
    if (!translate_tags())
    {
        parser->obey_markups(FALSE);
        parser->obey_end_markups(TRUE);
    }
}

void CHtmlTagVerbatim::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagContainer::on_close(parser);

    /* tell the parser to stop obeying whitespace */
    parser->obey_whitespace(FALSE, TRUE);

    /* tell the parser to resume translating tags if we there was a lapse */
    if (!translate_tags())
        parser->obey_markups(TRUE);
}

/*
 *   listing font - set small monospaced font 
 */
int CHtmlTagVerbatim::add_attrs(CHtmlFormatter *formatter,
                                CHtmlFontDesc *desc)
{
    int newsize;
    int change;

    /* presume no change */
    change = FALSE;

    /* calculate the new size down from the BASEFONT size */
    newsize = formatter->get_font_base_size() - get_font_size_decrement();
    if (newsize < 1) newsize = 1;

    /* if we're already at this size, there's nothing to do */
    if (desc->htmlsize != newsize)
        change = TRUE;

    /* if already in monospaced font and SMALL size, there's nothing to do */
    if (!desc->fixed_pitch)
        change = TRUE;

    /* set monospaced font and new size */
    desc->fixed_pitch = TRUE;
    desc->htmlsize = newsize;

    /* return change indication */
    return change;
}

void CHtmlTagVerbatim::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* insert a full line break */
    break_ht_ = win->measure_text(formatter->get_font(), " ", 1, 0).y;
    formatter->add_line_spacing(break_ht_);

    /* do font container entry */
    fontcont_format(win, formatter);
}

void CHtmlTagVerbatim::format_exit(CHtmlFormatter *formatter)
{
    /* exit font container */
    fontcont_format_exit(formatter);

    /* insert vertical whitespace below */
    formatter->add_line_spacing(break_ht_);
}


HTML_attrerr CHtmlTagVerbatim::set_attribute(CHtmlParser *parser,
                                             HTML_Attrib_id_t id,
                                             const textchar_t *val,
                                             size_t vallen)
{
    switch(id)
    {
    case HTML_Attrib_width:
        /* accept but ignore the WIDTH setting */
        return set_number_attr(&width_, val, vallen);
        
    default:
        /* not handled here; inherit the default */
        return CHtmlTagContainer::set_attribute(parser, id, val, vallen);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   FONT markup 
 */

HTML_attrerr CHtmlTagFONT::set_attribute(CHtmlParser *parser,
                                         HTML_Attrib_id_t id,
                                         const textchar_t *val,
                                         size_t vallen)
{
    HTML_attrerr err;
    
    switch(id)
    {
    case HTML_Attrib_size:
        /* check for relative notation */
        if (vallen > 0 && (*val == '-' || *val == '+'))
        {
            /* remember the relative setting */
            relative_ = *val;

            /* skip the +/- indicator */
            ++val;
            --vallen;
        }
        else
            relative_ = 0;

        /* get the number */
        err = set_number_attr(&size_, val, vallen);

        /* check the range */
        if (err == HTML_attrerr_ok && (size_ < 1 || size_ > 7))
        {
            /* leave size as default */
            size_ = 3;

            /* report the error */
            err = HTML_attrerr_out_of_range;
        }

        /* return the error indication */
        return err;

    case HTML_Attrib_face:
        face_.set(val, vallen);
        return HTML_attrerr_ok;

    case HTML_Attrib_color:
        use_color_ = TRUE;
        return set_color_attr(parser, &color_, val, vallen);

    case HTML_Attrib_bgcolor:
        use_bgcolor_ = TRUE;
        return set_color_attr(parser, &bgcolor_, val, vallen);

    default:
        /* we don't recognize it explicitly; inherit default handling */
        return CHtmlTagFontCont::set_attribute(parser, id, val, vallen);
    }
}

int CHtmlTagFONT::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    int change;
    
    /* presume nothing will change */
    change = FALSE;

    /* see what to do with the size */
    if (relative_)
    {
        /* if it's a non-zero relative setting, apply it */
        if (size_ != 0)
        {
            int oldsize;

            /* remember the original size */
            oldsize = desc->htmlsize;
            
            /* apply the delta */
            if (relative_ == '-')
                desc->htmlsize -= size_;
            else
                desc->htmlsize += size_;

            /* ensure it's within range */
            if (desc->htmlsize < 1)
                desc->htmlsize = 1;
            else if (desc->htmlsize > 7)
                desc->htmlsize = 7;

            /* note if anything's changed */
            change = (oldsize != desc->htmlsize);
        }
    }
    else
    {
        /* see if the size is the same as it already was */
        if (desc->htmlsize != size_)
        {
            /* it's changed - apply the new size and note the change */
            desc->htmlsize = size_;
            change = TRUE;
        }

    }

    /* use the face if one was provided */
    if (face_.get() != 0)
    {
        size_t facelen = get_strlen(face_.get());
        
        /* if the face name list has changed, use the new one */
        if (get_strlen(desc->face) != facelen
            || memicmp(desc->face, face_.get(),
                       facelen * sizeof(textchar_t)) != 0)
        {
            /* limit the length */
            if (facelen >= sizeof(desc->face)/sizeof(desc->face[0]))
                facelen = sizeof(desc->face)/sizeof(desc->face[0]) - 1;

            /* copy and null-terminate the value */
            memcpy(desc->face, face_.get(), facelen * sizeof(textchar_t));
            desc->face[facelen] = '\0';
            change = TRUE;
        }

        /* note that the face was set explicitly */
        desc->face_set_explicitly = TRUE;
    }
    else
    {
        /* note that the face was not set explicitly */
        desc->face_set_explicitly = FALSE;
    }

    /* apply the color if one was specified */
    if (use_color_ && (color_ != desc->color || desc->default_color))
    {
        /* set the new color and note the change */
        desc->color = map_color(formatter->get_win(), color_);
        desc->default_color = FALSE;
        change = TRUE;
    }

    /* apply the background color if one was specified */
    if (use_bgcolor_ && (bgcolor_ != desc->bgcolor || desc->default_bgcolor))
    {
        /* set the new color and note the change */
        desc->bgcolor = map_color(formatter->get_win(), bgcolor_);
        desc->default_bgcolor = FALSE;
        change = TRUE;
    }

    /* return the change indication */
    return change;
}


/* ------------------------------------------------------------------------ */
/*
 *   In-line attribute tags 
 */

/*
 *   HiliteON and HiliteOFF base class 
 */
void CHtmlTagHILITE::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlFontDesc desc;

    /* get the formatter's current font's description */
    formatter->get_font()->get_font_desc(&desc);

    /* apply our changes; if we make any changes, set the new font */
    if (set_attr(&desc))
        formatter->set_font(win->get_font(&desc));
}

/*
 *   HiliteON 
 */
int CHtmlTagHILITEON::set_attr(CHtmlFontDesc *desc)
{
    /* if we're already using bold type, there's nothing to do */
    if (desc->weight >= 700)
        return FALSE;

    /* set the new bold weight */
    desc->weight = 700;

    /* indicate that we made a change */
    return TRUE;
}

/*
 *   HiliteOFF
 */
int CHtmlTagHILITEOFF::set_attr(CHtmlFontDesc *desc)
{
    /* if we're already using non-bold type, there's nothing to do */
    if (desc->weight <= 400)
        return FALSE;

    /* set the new normal weight */
    desc->weight = 400;

    /* indicate that we made a change */
    return TRUE;
}



/* ------------------------------------------------------------------------ */
/*
 *   Font container markups.
 */

/*
 *   CITE 
 */
int CHtmlTagCITE::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if CITE is already set, there's nothing to do */
    if (desc->pe_cite)
        return FALSE;

    /* set CITE and indicate a change */
    desc->pe_cite = TRUE;
    return TRUE;
}

/*
 *   CODE 
 */
int CHtmlTagCODE::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if CODE is already set, there's nothing to do */
    if (desc->pe_code)
        return FALSE;

    /* set CODE and indicate a change */
    desc->pe_code = TRUE;
    return TRUE;
}

/*
 *   EM
 */
int CHtmlTagEM::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if EM is already set, there's nothing to do */
    if (desc->pe_em)
        return FALSE;

    /* set EM and indicate a change */
    desc->pe_em = TRUE;
    return TRUE;
}

/*
 *   KBD
 */
int CHtmlTagKBD::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if KBD is already set, there's nothing to do */
    if (desc->pe_kbd)
        return FALSE;

    /* set KBD and indicate a change */
    desc->pe_kbd = TRUE;
    return TRUE;
}

/*
 *   SAMP
 */
int CHtmlTagSAMP::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if SAMP is already set, there's nothing to do */
    if (desc->pe_samp)
        return FALSE;

    /* set SAMP and indicate a change */
    desc->pe_samp = TRUE;
    return TRUE;
}

/*
 *   STRONG 
 */
int CHtmlTagSTRONG::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if STRONG is already set, there's nothing to do */
    if (desc->pe_strong)
        return FALSE;

    /* set STRONG and indicate a change */
    desc->pe_strong = TRUE;
    return TRUE;
}

/*
 *   BIG 
 */
int CHtmlTagBIG::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    int newsize;

    /* calculate the new size as +1 from the BASEFONT size */
    newsize = formatter->get_font_base_size() + 1;
    if (newsize > 7) newsize = 7;

    /* if we're already at this size, there's nothing to do */
    if (desc->htmlsize == newsize)
        return FALSE;

    /* set the new size and indicate a change */
    desc->htmlsize = newsize;
    return TRUE;
}

/*
 *   SMALL
 */
int CHtmlTagSMALL::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    int newsize;

    /* calculate the new size as -1 from the BASEFONT size */
    newsize = formatter->get_font_base_size() - 1;
    if (newsize < 1) newsize = 1;

    /* if we're already at this size, there's nothing to do */
    if (desc->htmlsize == newsize)
        return FALSE;

    /* set the new size and indicate a change */
    desc->htmlsize = newsize;
    return TRUE;
}

/*
 *   DFN
 */
int CHtmlTagDFN::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if DFN is already set, there's nothing to do */
    if (desc->pe_dfn)
        return FALSE;

    /* set DFN and indicate a change */
    desc->pe_dfn = TRUE;
    return TRUE;
}

/*
 *   VAR
 */
int CHtmlTagVAR::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if VAR is already set, there's nothing to do */
    if (desc->pe_var)
        return FALSE;

    /* set VAR and indicate a change */
    desc->pe_var = TRUE;
    return TRUE;
}

/*
 *   B - bold
 */
int CHtmlTagB::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if the font is already bold, there's nothing to do */
    if (desc->weight >= 700)
        return FALSE;

    /* set bold weight and indicate a change */
    desc->weight = 700;
    return TRUE;
}

/*
 *   I - italic
 */
int CHtmlTagI::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if I is already set, there's nothing to do */
    if (desc->italic)
        return FALSE;

    /* set italics and indicate a change */
    desc->italic = TRUE;
    return TRUE;
}

/*
 *   U - underline
 */
int CHtmlTagU::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if U is already set, there's nothing to do */
    if (desc->underline)
        return FALSE;

    /* set U and indicate a change */
    desc->underline = TRUE;
    return TRUE;
}

/*
 *   STRIKE
 */
int CHtmlTagSTRIKE::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if STRIKE is already set, there's nothing to do */
    if (desc->strikeout)
        return FALSE;

    /* set STRIKE and indicate a change */
    desc->strikeout = TRUE;
    return TRUE;
}

/*
 *   TT - typewriter font
 */
int CHtmlTagTT::add_attrs(CHtmlFormatter *, CHtmlFontDesc *desc)
{
    /* if TT is already set, there's nothing to do */
    if (desc->fixed_pitch)
        return FALSE;

    /* set TT and indicate a change */
    desc->fixed_pitch = TRUE;
    return TRUE;
}

/*
 *   SUB - subscript 
 */
int CHtmlTagSUB::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    int change;

    /* inherit SMALL attributes */
    change = CHtmlTagSMALL::add_attrs(formatter, desc);

    /* add the SUBSCRIPT attribute as well */
    if (!desc->subscript)
    {
        change |= TRUE;
        desc->subscript = TRUE;
    }

    /* return whether we made a change */
    return change;
}

/*
 *   SUP - superscript 
 */
int CHtmlTagSUP::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    int change;

    /* inherit SMALL attributes */
    change = CHtmlTagSMALL::add_attrs(formatter, desc);

    /* add the SUPERSCRIPT attribute as well */
    if (!desc->superscript)
    {
        change |= TRUE;
        desc->superscript = TRUE;
    }

    /* return whether we made a change */
    return change;
}



/* ------------------------------------------------------------------------ */
/*
 *   BASEFONT 
 */

HTML_attrerr CHtmlTagBASEFONT::set_attribute(CHtmlParser *parser,
                                             HTML_Attrib_id_t attr_id,
                                             const textchar_t *val,
                                             size_t vallen)
{
    HTML_attrerr err;

    /* see what ID we're setting */
    switch(attr_id)
    {
    case HTML_Attrib_size:
        /* read the SIZE attribute value */
        err = set_number_attr(&size_, val, vallen);

        /* make sure it's in range */
        if (err == HTML_attrerr_ok && (size_ < 1 || size_ > 7))
        {
            /* leave the size as the default */
            size_ = 3;

            /* indicate an error */
            err = HTML_attrerr_out_of_range;
        }

        /* return the error indication */
        return err;

    case HTML_Attrib_face:
        face_.set(val, vallen);
        return HTML_attrerr_ok;

    case HTML_Attrib_color:
        use_color_ = TRUE;
        return set_color_attr(parser, &color_, val, vallen);

    default:
        /* inherit default processing */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }
}

/*
 *   on formatting the tag, set the base font in the formatter 
 */
void CHtmlTagBASEFONT::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlSysFont *old_font;
    CHtmlFontDesc desc;
    int delta;
    
    /*
     *   get the current font size, and note its HTML size relative to
     *   the base font 
     */
    old_font = formatter->get_font();
    old_font->get_font_desc(&desc);
    delta = desc.htmlsize - formatter->get_font_base_size();
    
    /* tell the formatter to use the new base font */
    formatter->set_font_base_size(size_);

    /* use the face if one was provided */
    if (face_.get() != 0)
    {
        size_t facelen;

        /* limit the length to what will fit in the font descriptor */
        facelen = get_strlen(face_.get());
        if (facelen >= sizeof(desc.face)/sizeof(desc.face[0]))
            facelen = sizeof(desc.face)/sizeof(desc.face[0]) - 1;

        /* copy and null-terminate the value */
        memcpy(desc.face, face_.get(), facelen * sizeof(textchar_t));
        desc.face[facelen] = '\0';
    }

    /* apply the color setting if one was provided */
    if (use_color_)
    {
        desc.color = map_color(win, color_);
        desc.default_color = FALSE;
    }

    /* update the default font to conform to the new size and description */
    desc.htmlsize = size_ + delta;
    formatter->set_font(win->get_font(&desc));
}

/* ------------------------------------------------------------------------ */
/*
 *   IMG - images 
 */

CHtmlTagIMG::~CHtmlTagIMG()
{
    /* remove our reference on the image object, if we have one */
    if (image_ != 0)
        image_->remove_ref();

    /* likewise the hover/active images */
    if (h_image_ != 0)
        h_image_->remove_ref();
    if (a_image_ != 0)
        a_image_->remove_ref();
}

/*
 *   pre-delete for pruning the tree - delete my ALT text from the text
 *   array 
 */
void CHtmlTagIMG::prune_pre_delete(CHtmlTextArray *arr)
{
    /* tell the text array to delete my text */
    if (alt_.get() != 0)
        arr->delete_text(alt_txtofs_, get_strlen(alt_.get()));
}

HTML_attrerr CHtmlTagIMG::set_attribute(CHtmlParser *parser,
                                        HTML_Attrib_id_t attr_id,
                                        const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;

    /* see what ID we're setting */
    switch(attr_id)
    {
    case HTML_Attrib_src:
        src_.set_url(val, vallen);
        break;

    case HTML_Attrib_asrc:
        asrc_.set_url(val, vallen);
        break;

    case HTML_Attrib_hsrc:
        hsrc_.set_url(val, vallen);
        break;

    case HTML_Attrib_alt:
        /* set the text */
        alt_.set(val, vallen);

        /* insert it into our text array */
        alt_txtofs_ = parser->get_text_array()->append_text(val, vallen);
        break;

    case HTML_Attrib_border:
        return set_number_attr(&border_, val, vallen);

    case HTML_Attrib_hspace:
        return set_number_attr(&hspace_, val, vallen);

    case HTML_Attrib_vspace:
        return set_number_attr(&vspace_, val, vallen);

    case HTML_Attrib_usemap:
        usemap_.set_url(val, vallen);
        break;

    case HTML_Attrib_ismap:
        ismap_ = TRUE;
        break;

    case HTML_Attrib_width:
        width_set_ = TRUE;
        return set_number_attr(&width_, val, vallen);

    case HTML_Attrib_height:
        height_set_ = TRUE;
        return set_number_attr(&height_, val, vallen);

    case HTML_Attrib_align:
        /* look up the alignment setting */
        val_id = parser->attrval_to_id(val, vallen);

        /* make sure it's valid - return an error if not */
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_right:
        case HTML_Attrib_top:
        case HTML_Attrib_middle:
        case HTML_Attrib_bottom:
            align_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    default:
        /* inherit default processing */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* success */
    return HTML_attrerr_ok;
}

/*
 *   format the image 
 */
void CHtmlTagIMG::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDispImg *disp;

    /* if we haven't loaded the image yet, load it now */
    if (win->get_html_show_graphics())
    {
        /* set up the main image, which is required for all <IMG> tags */
        load_image(win, formatter, &image_, &src_, "SRC", TRUE);

        /* set up the optional active and hover images, if present */
        load_image(win, formatter, &a_image_, &asrc_, "ASRC", FALSE);
        load_image(win, formatter, &h_image_, &hsrc_, "HSRC", FALSE);
    }
    else
    {
        /* 
         *   if we have an image hanging around from a previous pass
         *   through the format list, delete it, since we aren't going to
         *   display it any more 
         */
        if (image_ != 0)
        {
            /* graphics aren't allowed - delete the image */
            image_->remove_ref();
            image_ = 0;
        }
        if (a_image_ != 0)
        {
            a_image_->remove_ref();
            a_image_ = 0;
        }
        if (h_image_ != 0)
        {
            h_image_->remove_ref();
            h_image_ = 0;
        }

        /* if we have an ALT attribute, insert its text */
        if (alt_.get() != 0)
            formatter->
                add_disp_item(formatter->get_disp_factory()->
                              new_disp_text(win, formatter->get_font(),
                                            alt_.get(),
                                            get_strlen(alt_.get()),
                                            alt_txtofs_));

        /* 
         *   don't insert anything further - we specifically don't want to
         *   insert an image display object 
         */
        return;
    }

    /* create the display item */
    disp = formatter->get_disp_factory()->
           new_disp_img(win, image_, &alt_, align_, hspace_, vspace_,
                        width_, width_set_, height_, height_set_,
                        border_, &usemap_, ismap_);

    /* if we have hover or active images, set them in the display item */
    if (h_image_ != 0 || a_image_ != 0)
        disp->set_extra_images(a_image_, h_image_);

    /*
     *   Add the display item to the formatter's display list.  If the
     *   item is to be inserted into the current line, add it as we would
     *   any other item; if instead we're going to float it to the left or
     *   right margin and flow the text around it, we need to add it to
     *   the deferred display list. 
     */
    switch(align_)
    {
    case HTML_Attrib_top:
    case HTML_Attrib_bottom:
    case HTML_Attrib_middle:
        /* insert the item in-line */
        formatter->add_disp_item(disp);
        break;
        
    case HTML_Attrib_left:
    case HTML_Attrib_right:
        /* 
         *   the item will float to the margin with text flowing around it
         *   -- defer inserting it into the display list until we start a
         *   new line 
         */
        formatter->add_disp_item_deferred(disp);
        break;

    default:
        /* other cases should never occur; ignore them if they do */
        break;
    }
}

/*
 *   Load an image resource associated with the tag. 
 */
void CHtmlTagIMG::load_image(CHtmlSysWin *win, CHtmlFormatter *formatter,
                             CHtmlResCacheObject **image,
                             const CHtmlUrl *url,
                             const char *attr_name,
                             int mandatory)
{
    /* if the image is optional and there's no URL, skip it */
    if (!mandatory && (url->get_url() == 0 || url->get_url()[0] == 0))
        return;

    /* if we don't already have an object for this image, try loading it */
    if (*image == 0)
    {
        /* 
         *   find the image in the cache if possible, or load it and add it
         *   to the cache 
         */
        *image = formatter->get_res_cache()->
                 find_or_create(win, formatter->get_res_finder(), url);
        
        /* if it's not a valid image, log an error */
        if (*image == 0 || (*image)->get_image() == 0)
            oshtml_dbg_printf("<IMG %s>: resource %s is not a valid image\n",
                              attr_name, url->get_url());

        /* count our reference to the cache object as long as we keep it */
        if (*image != 0)
            (*image)->add_ref();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   NOBR - don't allow line breaks within the text spanned by the
 *   <nobr>...</nobr> sequence.  
 */

void CHtmlTagNOBR::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* save old wrapping state, and set new state */
    old_wrap_ = formatter->get_line_wrapping();
    formatter->set_line_wrapping(FALSE);

    /* add a line-wrap tag to the display stream, so it knows about it */
    formatter->add_disp_item(new (formatter) CHtmlDispNOBR(TRUE));
}

void CHtmlTagNOBR::format_exit(CHtmlFormatter *formatter)
{
    /* restore old line wrapping state */
    formatter->set_line_wrapping(old_wrap_);

    /* add a line-wrap tag to the display stream to restore the old status */
    formatter->add_disp_item(new (formatter) CHtmlDispNOBR(!old_wrap_));
}

/* ------------------------------------------------------------------------ */
/*
 *   WRAP tag 
 */

/*
 *   parse an attribute 
 */
HTML_attrerr CHtmlTagWRAP::set_attribute(CHtmlParser *parser,
                                         HTML_Attrib_id_t attr_id,
                                         const textchar_t *val, size_t vallen)
{
    /* check the attribute */
    switch(attr_id)
    {
    case HTML_Attrib_char:
        /* they want character-wrapping mode */
        char_wrap_mode_ = TRUE;
        break;

    case HTML_Attrib_word:
        /* they want word-wrapping mode */
        char_wrap_mode_ = FALSE;
        break;

    default:
        /* it's not one of ourse, so inherit the base class processing */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* success */
    return HTML_attrerr_ok;
}

/*
 *   enter the tag during formatting 
 */
void CHtmlTagWRAP::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* 
     *   Set the new wrapping mode.  Note that we're an in-line mode switch,
     *   not a container, so there's no need to remember the previous mode. 
     */
    formatter->set_char_wrap_mode(char_wrap_mode_);
}

/* ------------------------------------------------------------------------ */
/*
 *   BANNER 
 */

CHtmlTagBANNER::~CHtmlTagBANNER()
{
    /* 
     *   if there's anything in my banner list, move it back into my
     *   regular contents list, so that it's deleted normally 
     */
    if (banner_first_ != 0)
    {
        contents_first_ = banner_first_;
        banner_first_ = 0;
    }
}

/*
 *   pre-delete on pruning the tree - notify any children, either in the
 *   regular list or in the banner list 
 */
void CHtmlTagBANNER::prune_pre_delete(CHtmlTextArray *arr)
{
    CHtmlTag *cur;
    
    /* if there's anything in my banner child list, notify it */
    for (cur = banner_first_ ; cur != 0 ; cur = cur->get_next_tag())
        cur->prune_pre_delete(arr);
    
    /* inherit default handling */
    CHtmlTagContainer::prune_pre_delete(arr);
}

/*
 *   Receive notification that the banner is now obsolete.  The formatter
 *   will call this routine whenever it finds that a subsequent BANNER tag
 *   in the same format list uses the same banner -- when this happens,
 *   the banner involved will always be overwritten by the subsequent tag.
 *   We'll discard our contents and skip doing any work to format the
 *   banner on future passes through the format list.  
 */
void CHtmlTagBANNER::notify_obsolete(CHtmlTextArray *arr)
{
    /* restore my usual child list */
    if (banner_first_ != 0)
    {
        contents_first_ = banner_first_;
        banner_first_ = 0;
    }

    /* remove references on any contained text items */
    prune_pre_delete(arr);

    /* dump my children -- we'll never need to format the contents again */
    delete_contents();
}

void CHtmlTagBANNER::on_parse(CHtmlParser *parser)
{
    /*
     *   If the REMOVE attribute was set, it means that this isn't a
     *   container tag after all -- in this case, don't add myself as a
     *   container, but just as an ordinary tag. 
     */
    if (remove_)
        CHtmlTag::on_parse(parser);
    else
        CHtmlTagContainer::on_parse(parser);

    /* 
     *   insert a newline before the banner, so lines in the main window
     *   before the banner don't run into the banner text for word and
     *   line breaking purposes 
     */
    parser->get_text_array()->append_text_noref("\n", 1);

    /* skip whitespace at the start of the banner's body */
    parser->begin_skip_sp();
}

/*
 *   close the BANNER tag 
 */
void CHtmlTagBANNER::on_close(CHtmlParser *parser)
{
    /* inherit default behavior, if we're a container */
    if (!remove_)
        CHtmlTagContainer::on_close(parser);

    /* 
     *   insert a newline after the banner, for the same reason we insert
     *   one before the banner 
     */
    parser->get_text_array()->append_text_noref("\n", 1);
}

HTML_attrerr CHtmlTagBANNER::
   set_attribute(CHtmlParser *parser, HTML_Attrib_id_t attr_id,
                 const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;
    
    /* see what ID we're setting */
    switch(attr_id)
    {
    case HTML_Attrib_id:
        /* store the ID */
        id_.set(val, vallen);
        break;

    case HTML_Attrib_remove:
        /* note that this is a REMOVE tag */
        remove_ = TRUE;
        break;

    case HTML_Attrib_removeall:
        /* this is a REMOVEALL tag */
        remove_ = TRUE;
        removeall_ = TRUE;

    case HTML_Attrib_height:
        /* note that the height has been explicitly set */
        height_set_ = TRUE;

        /* check for special values */
        if (parser->attrval_to_id(val, vallen) == HTML_Attrib_previous)
        {
            /* note that we're to use the existing banner height */
            height_prev_ = TRUE;
        }
        else
        {
            /* set the numeric or percentage height */
            return set_number_or_pct_attr(&height_, &height_pct_,
                                          val, vallen);
        }
        break;

    case HTML_Attrib_width:
        /* note that the width has been explicitly set */
        width_set_ = TRUE;

        /* check for special values */
        if (parser->attrval_to_id(val, vallen) == HTML_Attrib_previous)
        {
            /* note that we're to use the existing banner height */
            width_prev_ = TRUE;
        }
        else
        {
            /* set the numeric or percentage width */
            return set_number_or_pct_attr(&width_, &width_pct_, val, vallen);
        }
        break;

    case HTML_Attrib_border:
        border_ = TRUE;
        break;

    case HTML_Attrib_align:
        /* get the alignment setting */
        val_id = parser->attrval_to_id(val, vallen);

        /* map it to the appropriate setting */
        switch(val_id)
        {
        case HTML_Attrib_top:
            alignment_ = HTML_BANNERWIN_POS_TOP;
            break;
        case HTML_Attrib_bottom:
            alignment_ = HTML_BANNERWIN_POS_BOTTOM;
            break;
        case HTML_Attrib_left:
            alignment_ = HTML_BANNERWIN_POS_LEFT;
            break;
        case HTML_Attrib_right:
            alignment_ = HTML_BANNERWIN_POS_RIGHT;
            break;
        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    default:
        /* inherit default processing */
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* success */
    return HTML_attrerr_ok;
}

void CHtmlTagBANNER::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* 
     *   If this is a <BANNER REMOVE> tag, find the existing banner and
     *   delete it.  If the banner isn't finished yet, ignore it for now.  
     */
    if (remove_)
    {
        if (!obsolete_)
        {
            /* check to see if we're removing one banner or all of them */
            if (removeall_)
            {
                /* tell the formatter to remove all banners */
                formatter->remove_all_banners(TRUE);
            }
            else
            {
                /* tell the formatter to remove this banner */
                formatter->remove_banner(id_.get());
            }

            /* 
             *   we don't need to apply this again in future formatting
             *   passes, because the formatter will go back into the
             *   format list and remove the previous banner 
             */
            obsolete_ = TRUE;
        }
    }
    else
    {
        /*
         *   If I have any contents, remove them and move them into the
         *   banner sublist instead.  This will prevent the banner's
         *   contents from showing up in my main list.  If I've already
         *   set up my banner sublist, it means I've been formatted before
         *   and we don't need to repeat this step.  
         */
        if (banner_first_ == 0)
        {
            banner_first_ = contents_first_;
            contents_first_ = 0;
        }

        /* if I don't have any contents, there's nothing to do */
        if (banner_first_ == 0)
            return;

        /* tell the formatter to start formatting the banner */
        if (!formatter->format_banner(id_.get(),
                                      height_, height_set_, height_prev_,
                                      height_pct_,
                                      width_, width_set_, width_prev_,
                                      width_pct_,
                                      this))
        {
            /* 
             *   That failed -- we can't format a real banner.  Simply leave
             *   the contents in the separate list, so that all of our
             *   contents are simply ignored.  (We wouldn't want our
             *   contents showing up in our main window, so it's better to
             *   just leave them unshown.)  
             */
        }
    }
}

void CHtmlTagBANNER::format_exit(CHtmlFormatter *formatter)
{
    /*
     *   This will only be called from within the subformatter.  Tell our
     *   subformatter to stop formatting, since we are no longer within
     *   its contents. 
     */
    formatter->end_banner();
}

/* ------------------------------------------------------------------------ */
/*
 *   ABOUTBOX tag - the about box 
 */

HTML_attrerr CHtmlTagABOUTBOX::
   set_attribute(CHtmlParser *parser, HTML_Attrib_id_t attr_id,
                 const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_id:
    case HTML_Attrib_remove:
    case HTML_Attrib_height:
    case HTML_Attrib_border:
        /* 
         *   none of these BANNER attributes are accepted by ABOUTBOX, so
         *   inherit the container default instead 
         */
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);

    default:
        /* inherit default from BANNER */
        return CHtmlTagBANNER::set_attribute(parser, attr_id, val, vallen);
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   TAB 
 */

CHtmlTagTAB::CHtmlTagTAB(CHtmlParser *)
{
    /* set default attribute values */
    align_ = HTML_Attrib_invalid;
    define_ = FALSE;
    to_ = FALSE;
    dp_ = 0;
    use_indent_ = FALSE;
    use_multiple_ = FALSE;
    indent_ = 0;
    txtofs_ = 0;
}

/*
 *   when pruning the tree, delete my text from the text array 
 */
void CHtmlTagTAB::prune_pre_delete(CHtmlTextArray *arr)
{
    arr->delete_text(txtofs_, 1);
}

void CHtmlTagTAB::on_parse(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTag::on_parse(parser);

    /* add a space to the buffer, in case we need to add it */
    txtofs_ = parser->get_text_array()->append_text(" ", 1);
}

/*
 *   parse a TAB attribute 
 */
HTML_attrerr CHtmlTagTAB::
   set_attribute(CHtmlParser *parser, HTML_Attrib_id_t attr_id,
                 const textchar_t *val, size_t vallen)
{
    HTML_Attrib_id_t val_id;

    /* see what ID we're setting */
    switch(attr_id)
    {
    case HTML_Attrib_id:
        /* store the ID */
        id_.set(val, vallen);
        define_ = TRUE;
        break;

    case HTML_Attrib_to:
        /* remember where we're tabbing to */
        id_.set(val, vallen);
        to_ = TRUE;
        break;

    case HTML_Attrib_dp:
        /* remember the decimal point character */
        if (vallen > 0)
            dp_ = val[0];
        break;

    case HTML_Attrib_align:
        switch(val_id = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
        case HTML_Attrib_decimal:
            /* these are all valid */
            align_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_indent:
        use_indent_ = TRUE;
        return set_number_attr(&indent_, val, vallen);

    case HTML_Attrib_multiple:
        use_multiple_ = TRUE;
        return set_number_attr(&indent_, val, vallen);

    default:
        /* inherit default processing */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* success */
    return HTML_attrerr_ok;
}

/*
 *   format a tab 
 */
void CHtmlTagTAB::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* if this defines a tab, tell the formatter about it */
    if (define_)
    {
        formatter->set_tab(id_.get(), align_, dp_);
        return;
    }

    /* let the formatter deal with it */
    formatter->tab_to(this);
}

/* ------------------------------------------------------------------------ */
/*
 *   <Q> - quote 
 */

/*
 *   pre-delete for pruning the tree - delete my text from the text array 
 */
void CHtmlTagQ::prune_pre_delete(CHtmlTextArray *arr)
{
    /* tell the text array to delete my text */
    arr->delete_text(open_ofs_, 1);
    arr->delete_text(close_ofs_, 1);

    /* inherit the default behavior */
    CHtmlTagContainer::prune_pre_delete(arr);
}

/*
 *   open parsing - add my open quote to the text array 
 */
void CHtmlTagQ::on_parse(CHtmlParser *parser)
{
    int level;
    unsigned int html_open, html_close;
    textchar_t ascii_quote;
    oshtml_charset_id_t charset;
    int charset_changed;
    textchar_t result[10];
    size_t len;
    
    /* inherit default */
    CHtmlTagContainer::on_parse(parser);

    /* figure out what kind of quotes to use, based on our nesting level */
    level = get_quote_nesting_level();
    if ((level & 1) == 0)
    {
        /* 
         *   I'm the outermost quote, or the second quote in, or the
         *   fourth quote in, and so on - use double quotes 
         */
        html_open = 8220;
        html_close = 8221;
        ascii_quote = '"';
    }
    else
    {
        /* 
         *   I'm the first one in, or the third one in, and so on - use
         *   single quotes 
         */
        html_open = 8216;
        html_close = 8217;
        ascii_quote = '\'';
    }

    /* translate the HTML open quote */
    len = parser->get_sys_frame()->
          xlat_html4_entity(result, sizeof(result), html_open,
                            &charset, &charset_changed);
    if (charset_changed || len > sizeof(open_q_))
        open_q_ = ascii_quote;
    else
        open_q_ = result[0];

    /* translate the HTML close quote */
    len = parser->get_sys_frame()->
          xlat_html4_entity(result, sizeof(result), html_close,
                            &charset, &charset_changed);
    if (charset_changed || len > sizeof(close_q_))
        close_q_ = ascii_quote;
    else
        close_q_ = result[0];

    /* add my open quote */
    open_ofs_ = parser->get_text_array()->append_text(&open_q_, 1);

    /* we effectively insert text, so keep adjacent source whitespace */
    parser->end_skip_sp();
}

/*
 *   close parsing - add my close quote to the text array 
 */
void CHtmlTagQ::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagContainer::on_close(parser);
    
    /* add my close quote */
    close_ofs_ = parser->get_text_array()->append_text(&close_q_, 1);

    /* we effectively insert text, so keep adjacent source whitespace */
    parser->end_skip_sp();
}

/*
 *   format the tag - add my open quote to the display list 
 */
void CHtmlTagQ::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* add a display item for my quote */
    formatter->add_disp_item(formatter->get_disp_factory()->
                             new_disp_text(win, formatter->get_font(),
                                           &open_q_, sizeof(open_q_),
                                           open_ofs_));
}

/* 
 *   end formatting - add my close quote to the display list 
 */
void CHtmlTagQ::format_exit(CHtmlFormatter *formatter)
{
    /* add a display item for my quote */
    formatter->add_disp_item(formatter->get_disp_factory()->
                             new_disp_text(formatter->get_win(),
                                           formatter->get_font(),
                                           &close_q_, sizeof(close_q_),
                                           close_ofs_));
}

/* ------------------------------------------------------------------------ */
/*
 *   <A> (anchor) 
 */

HTML_attrerr CHtmlTagA::set_attribute(CHtmlParser *parser,
                                      HTML_Attrib_id_t attr_id,
                                      const textchar_t *val, size_t vallen)
{
    HTML_attrerr err;
    const textchar_t *p;
    size_t rem;
    
    switch(attr_id)
    {
    case HTML_Attrib_href:
        /* store the hyperlink reference */
        href_.set_url(val, vallen);
        break;

    case HTML_Attrib_title:
        /* store the title */
        title_.set(val, vallen);
        break;

    case HTML_Attrib_name:
    case HTML_Attrib_rel:
    case HTML_Attrib_rev:
        /* accept but ignore these attributes */
        break;

    case HTML_Attrib_plain:
        /* set rendering style to 'plain' */
        style_ = LINK_STYLE_PLAIN;
        break;

    case HTML_Attrib_hidden:
        /* set link style to 'hidden' */
        style_ = LINK_STYLE_HIDDEN;
        break;

    case HTML_Attrib_forced:
        /* 
         *   Set rendering style to 'forced'.  Note that this is, for now, an
         *   UNDOCUMENTED, UNSUPPORTED extension - it is specifically for
         *   internal use by the system itself, not for use by games.
         *   Support for this attribute is NOT universal, so games shouldn't
         *   use it.  
         */
        style_ = LINK_STYLE_FORCED;
        break;

    case HTML_Attrib_hover:
        /* 
         *   Set the hovering foreground color, and optionally the background
         *   color.  First, scan for a comma, to see if there's a background
         *   color specified.  
         */
        rem = vallen;
        p = find_attr_token(val, &rem, ",");

        /* if there's a color, use it */
        if (p - val != 0)
        {
            /* scan the foreground color */
            err = set_color_attr(parser, &hover_fg_, val, p - val);

            /* return failure on error */
            if (err != HTML_attrerr_ok)
                return err;

            /* note that we're using a foreground hover color */
            use_hover_fg_ = TRUE;
        }

        /* if there's a comma, skip it */
        if (rem != 0)
            ++p, --rem;

        /* scan for a comma, to see if there's a decoration flag */
        val = p;
        p = find_attr_token(p, &rem, ",");

        /* if there's a background color, scan it */
        if (p - val != 0)
        {
            /* scan the color */
            err = set_color_attr(parser, &hover_bg_, val, p - val);

            /* return failure on error */
            if (err != HTML_attrerr_ok)
                return err;

            /* note that we're using a background hover color */
            use_hover_bg_ = TRUE;
        }

        /* if there's a comma, skip it */
        if (rem != 0)
            ++p, --rem;

        /* if there's an added decoration, scan it */
        if (rem != 0)
        {
            /* scan the decoration name */
            if (rem == 9 && memicmp(p, "underline", 9) == 0)
                hover_underline_ = TRUE;
            else
                err = HTML_attrerr_inv_enum;
        }

        /* return the parsing result */
        return err;

    case HTML_Attrib_noenter:
        noenter_ = TRUE;
        break;

    case HTML_Attrib_append:
        append_ = TRUE;
        break;

    default:
        /* return inherited default behavior */
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* if we didn't balk already, we must have been successful */
    return HTML_attrerr_ok;
}

void CHtmlTagA::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlDispLink *link;
    
    /* handle the font container formatting */
    fontcont_format(win, formatter);

    /* create a link display item */
    link = new (formatter)
           CHtmlDispLink(style_, append_, noenter_, &href_, title_.get(),
                         title_.get() == 0 ? 0 : get_strlen(title_.get()));

    /* set the HOVER attributes, if they're used */
    if (use_hover_fg_)
        link->set_hover_fg(map_color(win, hover_fg_));
    if (use_hover_bg_)
        link->set_hover_bg(map_color(win, hover_bg_));
    if (hover_underline_)
        link->set_hover_underline();

    /* set it as the current link in the formatter */
    formatter->add_disp_item_link(link);
}

void CHtmlTagA::format_exit(CHtmlFormatter *formatter)
{
    /* tell the formatter that we're exiting the link */
    formatter->end_link();

    /* exit the font container */
    fontcont_format_exit(formatter);
}

/*
 *   add font container attributes 
 */
int CHtmlTagA::add_attrs(CHtmlFormatter *formatter, CHtmlFontDesc *desc)
{
    /* 
     *   Within a hyperlink, ignore any containing font color, because we
     *   want to use the appropriate link color by default.  So, explicitly
     *   set the default_color_ attribute in the font descriptor: an <A> is
     *   like an explicit <FONT COLOR> tag specifying the appropriate link
     *   color.
     *   
     *   In a HIDDEN or PLAIN hyperlink, though, we DO inherit the color from
     *   the surrounding text, since these aren't drawn as hyperlinks.  
     */
    if (href_.get_url() != 0 && get_strlen(href_.get_url())
        && !desc->default_color
        && style_ != LINK_STYLE_PLAIN && style_ != LINK_STYLE_HIDDEN)
    {
        /* explicitly set the default color */
        desc->default_color = TRUE;

        /* we changed the description */
        return TRUE;
    }

    /* no changes to the font */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   MAP tag and subtags
 */

HTML_attrerr CHtmlTagMAP::set_attribute(CHtmlParser *parser,
                                        HTML_Attrib_id_t attr_id,
                                        const textchar_t *val, size_t vallen)
{
    switch(attr_id)
    {
    case HTML_Attrib_name:
        /* set my name */
        name_.set(val, vallen);
        break;

    default:
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* no errors */
    return HTML_attrerr_ok;
}

void CHtmlTagMAP::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    formatter->begin_map(name_.get(), get_strlen(name_.get()));
}

void CHtmlTagMAP::format_exit(CHtmlFormatter *formatter)
{
    formatter->end_map();
}

HTML_attrerr CHtmlTagAREA::set_attribute(CHtmlParser *parser,
                                         HTML_Attrib_id_t attr_id,
                                         const textchar_t *val,
                                         size_t vallen)
{
    HTML_Attrib_id_t val_id;
    HTML_attrerr err;
    int i;
    int pct;
    
    switch(attr_id)
    {
    case HTML_Attrib_shape:
        switch(val_id = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_rect:
        case HTML_Attrib_circle:
        case HTML_Attrib_poly:
            /* these are all valid */
            shape_ = val_id;
            break;

        default:
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_coords:
        switch(shape_)
        {
        case HTML_Attrib_invalid:
        case HTML_Attrib_rect:
            shape_ = HTML_Attrib_rect;
            for (i = 0 ; i < 4 ; ++i)
            {
                /* set the next coordinate */
                err = set_coord_attr(&coords_[i].val_, &pct, &val, &vallen);
                if (err != HTML_attrerr_ok)
                    return err;

                /* set percentage indicator */
                coords_[i].pct_ = pct;
            }
            coord_cnt_ = 4;
            break;

        case HTML_Attrib_circle:
            for (i = 0 ; i < 3 ; ++i)
            {
                /* set the next coordinate */
                err = set_coord_attr(&coords_[i].val_, &pct, &val, &vallen);
                if (err != HTML_attrerr_ok)
                    return err;

                /* set percentage indicator */
                coords_[i].pct_ = pct;
            }
            coord_cnt_ = 3;
            break;

        case HTML_Attrib_poly:
            for (i = 0 ; i < CHtmlTagAREA_max_coords ; ++i)
            {
                /* set the next coordinate */
                err = set_coord_attr(&coords_[i].val_, &pct, &val, &vallen);
                if (err != HTML_attrerr_ok)
                    return err;

                /* set percentage indicator */
                coords_[i].pct_ = pct;

                /* if we've run out of coordinates, stop */
                if (vallen == 0)
                    break;
            }

            /* make sure we used everything in the list */
            if (vallen != 0)
                return HTML_attrerr_too_many_coords;

            /* make sure we got an even number of coordinates */
            if ((i & 1) != 0)
                return HTML_attrerr_odd_coords;

            /* store the count */
            coord_cnt_ = i;
            break;

        default:
            /* other cases should never occur; ignore them if they do */
            break;
        }
        break;

    case HTML_Attrib_href:
        href_.set_url(val, vallen);
        nohref_ = FALSE;
        break;

    case HTML_Attrib_nohref:
        nohref_ = TRUE;
        break;

    case HTML_Attrib_alt:
        alt_.set(val, vallen);
        break;

    case HTML_Attrib_append:
        append_ = TRUE;
        break;

    case HTML_Attrib_noenter:
        noenter_ = TRUE;
        break;

    default:
        return CHtmlTag::set_attribute(parser, attr_id, val, vallen);
    }

    /* success */
    return HTML_attrerr_ok;
}

void CHtmlTagAREA::format(CHtmlSysWin *, CHtmlFormatter *formatter)
{
    /* add the area to the formatter's current map */
    formatter->add_map_area(shape_, nohref_ ? 0 : &href_,
                            alt_.get(),
                            alt_.get() == 0 ? 0 : get_strlen(alt_.get()),
                            coords_, coord_cnt_, append_, noenter_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Column list entry for TABLE tag 
 */

CHtmlTag_column::CHtmlTag_column()
{
    /* set up an initial cell list */
    cells_used_ = 0;
    cells_alloced_ = 10;
    cells_ = (CHtmlTag_cell **)
             th_malloc(cells_alloced_ * sizeof(CHtmlTag_cell *));

    /* we have no width measurements yet */
    min_width_ = max_width_ = 0;
    pix_width_ = 0;
    pct_width_ = 0;

    /* width is not yet known */
    width_ = 0;
}

CHtmlTag_column::~CHtmlTag_column()
{
    size_t i;
    CHtmlTag_cell **cell;
    
    /* delete each allocated cell */
    for (i = 0, cell = cells_ ; i < cells_used_ ; ++i, ++cell)
        delete *cell;

    /* delete the cell array */
    th_free(cells_);
}

/*
 *   Get a cell 
 */
CHtmlTag_cell *CHtmlTag_column::get_cell(size_t rownum)
{
    /* if the row is allocated, return it directly */
    if (rownum < cells_used_)
        return cells_[rownum];

    /* expand the slot list if necessary to make room for the newrow */
    if (rownum >= cells_alloced_)
    {
        cells_alloced_ = rownum + 5;
        cells_ = (CHtmlTag_cell **)
                 th_realloc(cells_, cells_alloced_
                            * sizeof(CHtmlTag_cell *));
    }

    /* allocate entries up to the given point */
    while (rownum >= cells_used_)
        cells_[cells_used_++] = new CHtmlTag_cell();

    /* return the new column */
    return cells_[rownum];
}

/* ------------------------------------------------------------------------ */
/*
 *   Table - base table image mix-in 
 */

CHtmlTableImage::~CHtmlTableImage()
{
    /* remove our reference on the image object, if we have one */
    if (bg_image_ != 0)
        bg_image_->remove_ref();
}

/*
 *   load the background image during formatting as needed 
 */
void CHtmlTableImage::format_load_bg(CHtmlSysWin *win, CHtmlFormatter *fmt)
{
    /* if we're in graphics mode, load the image */
    if (win->get_html_show_graphics())
    {
        /* if we already have an image loaded, there's nothing to do */
        if (bg_image_ != 0)
            return;

        /* if we don't have an image at all, there's nothing to do */
        if (background_.get_url() == 0 || background_.get_url()[0] == '\0')
            return;

        /* try loading the image */
        bg_image_ = fmt->get_res_cache()->
                    find_or_create(win, fmt->get_res_finder(), &background_);

        /* if we didn't load it, log an error */
        if (bg_image_ == 0 || bg_image_->get_image() == 0)
            oshtml_dbg_printf("<%s background>: resource %s is not a valid "
                              "image\n", get_tag_name(),
                              background_.get_url());

        /* if we got a cache object, count our reference to it */
        if (bg_image_ != 0)
            bg_image_->add_ref();
    }
    else
    {
        /* we're not in graphics mode - release any image we're holding */
        if (bg_image_ != 0)
        {
            /* release and forget our reference on the image */
            bg_image_->remove_ref();
            bg_image_ = 0;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Tables - TABLE tag
 */

CHtmlTagTABLE::CHtmlTagTABLE(CHtmlParser *)
    : CHtmlTagContainer(), CHtmlTableImage()
{
    /* no alignment assigned */
    align_ = HTML_Attrib_invalid;
    
    /* no width or height specified yet */
    width_set_ = height_set_ = FALSE;
    width_pct_ = height_pct_ = FALSE;
    
    /* default to no border - so indicate with a size of zero pixels */
    border_ = 0;
    
    /* use default spacing of 2 and padding of 1 */
    cellpadding_ = 1;
    cellspacing_ = 2;

    /* set up some space for the column list */
    cols_used_ = 0;
    cols_alloced_ = 10;
    columns_ = (CHtmlTag_column **)
               th_malloc(cols_alloced_ * sizeof(CHtmlTag_column *));

    /* currently on first row */
    cur_rownum_ = 0;

    /* no caption yet */
    caption_tag_ = 0;

    /* widths are not yet known */
    min_width_ = max_width_ = 0;

    /* no display item yet */
    disp_ = 0;

    /* don't have an enclosing table yet */
    enclosing_table_ = 0;
}

CHtmlTagTABLE::~CHtmlTagTABLE()
{
    size_t i;
    CHtmlTag_column **col;
    
    /* delete the columns we've allocated */
    for (i = 0, col = columns_ ; i < cols_used_ ; ++i, ++col)
        delete *col;

    /* delete the column slot array itself */
    th_free(columns_);
}

HTML_attrerr CHtmlTagTABLE::set_attribute(CHtmlParser *parser,
                                          HTML_Attrib_id_t attr_id,
                                          const textchar_t *val,
                                          size_t vallen)
{
    HTML_Attrib_id_t val_id;
    HTML_attrerr err;
    
    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
            /* valid setting - remember it */
            align_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;
        
    case HTML_Attrib_width:
        /* set the horizontal size */
        width_set_ = TRUE;
        return set_number_or_pct_attr(&width_, &width_pct_, val, vallen);

    case HTML_Attrib_height:
        /* set the vertical size */
        height_set_ = TRUE;
        return set_number_or_pct_attr(&height_, &height_pct_, val, vallen);

    case HTML_Attrib_border:
        /* 
         *   if there's no value, use a border size of 1; otherwise, use
         *   the value as the border size 
         */
        if (parser->attrval_to_id(val, vallen) == HTML_Attrib_border)
            border_ = 1;
        else
            return set_number_attr(&border_, val, vallen);
        break;
        
    case HTML_Attrib_cellpadding:
        return set_number_attr(&cellpadding_, val, vallen);

    case HTML_Attrib_cellspacing:
        return set_number_attr(&cellspacing_, val, vallen);

    case HTML_Attrib_bgcolor:
        /* parse the color */
        err = set_color_attr(parser, &bgcolor_, val, vallen);

        /* if we got a valid color, note that we want to use it*/
        if (err == HTML_attrerr_ok)
            use_bgcolor_ = TRUE;

        /* return the result of parsing the color */
        return err;

    case HTML_Attrib_background:
        /* store the background image URL */
        background_.set_url(val, vallen);
        break;

    default:
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* no errors */
    return HTML_attrerr_ok;
}

void CHtmlTagTABLE::on_parse(CHtmlParser *parser)
{
    /* 
     *   Start a new paragraph, unless it's a floating item, in which case
     *   we treat it as an in-line item, so there's no paragraph break.  
     */
    if (!is_floating())
        break_paragraph(parser, FALSE);

    /* inherit default */
    CHtmlTagContainer::on_parse(parser);
}

void CHtmlTagTABLE::on_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagContainer::on_close(parser);

    /*
     *   If I have a caption, tell the caption that we've finished parsing
     *   the table.  The caption will need to re-insert its text into the
     *   text array if it's aligned below the table, because the caption
     *   will be inserted into the display list after the table.  
     */
    if (caption_tag_ != 0)
        caption_tag_->on_close_table(parser);
}

void CHtmlTagTABLE::post_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagContainer::post_close(parser);

    /* 
     *   append a 'relax' in case we're closing any other containers around
     *   the table (this is necessary to prevent processing immediately
     *   following closures on the way out when we're doing the first pass
     *   through the table at format time) 
     */
    parser->append_tag(new CHtmlTagRelax());

    /* 
     *   start a new paragraph if it's a non-floating item; we don't do this
     *   for floating tables because we effectively treat floating tables as
     *   in-line items 
     */
    if (!is_floating())
        break_paragraph(parser, FALSE);
}

void CHtmlTagTABLE::pre_close(CHtmlParser *parser)
{
    /* if there's an open cell, close it */
    parser->close_tag_if_open("TH");
    parser->close_tag_if_open("TD");

    /* if there's an open row, close it */
    parser->close_tag_if_open("TR");

    /* if there's an open caption, close it */
    parser->close_tag_if_open("CAPTION");
}

void CHtmlTagTABLE::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    long width, max_width;
    long max_height;
    HTML_Attrib_id_t align;

    /* load our background image if needed */
    format_load_bg(win, formatter);

    /* start at the first row */
    cur_rownum_ = 0;

    /* 
     *   if our alignment is not specified, use the enclosing division
     *   alignment; otherwise, use the specified alignment 
     */
    align = (align_ == HTML_Attrib_invalid ? formatter->get_div_alignment()
                                           : align_);

    /* set the block alignment to the table's alignment */
    formatter->set_blk_alignment(align);

    /* 
     *   Figure out the maximum space available horizontally and
     *   vertically in the window 
     */
    max_width = win->get_disp_width() - formatter->get_left_margin()
                - formatter->get_right_margin();
    max_height = win->get_disp_height() - formatter->get_top_margin()
                 - formatter->get_bottom_margin();
    
    /* 
     *   If width and height are set in percentages, calculate the
     *   available size for the percentages now.  If they're not set,
     *   default to zero, so that we end up using the minimum size that
     *   works.  
     */
    if (width_set_)
    {
        if (width_pct_)
            width = (long)((double)max_width * ((double)width_ / 100.0));
        else
            width = width_;
    }
    else
        width = 0;

    if (height_set_)
    {
        if (height_pct_)
            calc_height_ = (long)
                           ((double)max_height * ((double)height_ / 100.0));
        else
            calc_height_ = height_;
    }
    else
        calc_height_ = 0;
    
    /* create a TABLE display item, and remember it for later */
    disp_ = new (formatter)
            CHtmlDispTable(win, border_, align, is_floating(),
                           width, width_pct_, width_set_, max_width,
                           map_color(win, bgcolor_), use_bgcolor_, bg_image_);

    /* 
     *   Tell the display item about the min and max widths.  On pass 1,
     *   these are not valid until after we've finished traversing the table,
     *   because we don't have enough information to calculate these at our
     *   format_exit time (and indeed, we wait until then to calculate the
     *   values).  However, we keep these values from the previous pass
     *   through the table, so they're available throughout pass 2.  
     */
    disp_->set_table_size_limits(min_width_, max_width_);
    
    /* tell the formatter we're starting a table */
    enclosing_table_ = formatter->begin_table(this, disp_,
                                              &enclosing_table_pos_,
                                              caption_tag_);
}

void CHtmlTagTABLE::format_exit(CHtmlFormatter *formatter)
{
    size_t colnum;
    long extra;
    int pass;

    /* 
     *   Now that we've finished a pass through the table, we have a complete
     *   set of metrics for the table's cells, so we can now calculate the
     *   column metrics based on the cell metrics.
     *   
     *   Computing the column metrics takes two passes.  On the first pass,
     *   we figure the width contributions to column sizes for cells that
     *   span only one column each.  On the second pass, we use those
     *   single-column metrics to find the width contributions for cells that
     *   span multiple columns.  
     */
    for (pass = 1 ; pass <= 2 ; ++pass)
    {
        CHtmlTag *cur;

        /* loop through the rows within the table */
        for (cur = get_contents() ; cur != 0 ; cur = cur->get_next_tag())
        {
            CHtmlTagTR *tr;

            /* if this is a row, traverse its cells */
            if ((tr = cur->get_as_table_row()) != 0)
            {
                CHtmlTag *chi;
                
                /* traverse the current row's cells */
                for (chi = tr->get_contents() ; chi != 0 ;
                     chi = chi->get_next_tag())
                {
                    CHtmlTagTableCell *cell;
                    
                    /* if it's a cell, figure its column metrics */
                    if ((cell = chi->get_as_table_cell()) != 0)
                        cell->compute_column_metrics(pass);
                }
            }
        }
    }
    
    /*
     *   We can now calculate the size limits for the table, since each
     *   cell has made its contribution.  The minimum size for the table
     *   is the sum of the minimum sizes of the columns; the maximum size
     *   is the sum of the maximum sizes of the columns. 
     */
    for (min_width_ = max_width_ = 0, colnum = 0 ; colnum < cols_used_ ;
         ++colnum)
    {
        CHtmlTag_column *col;

        /* get this column */
        col = get_column(colnum);

        /* add its minimum and maximum to the aggregate min and max */
        min_width_ += col->get_min_width();
        max_width_ += col->get_max_width();
    }

    /* add the decoration widths to the total (borders, padding, spacing) */
    extra = calc_decor_width();
    min_width_ += extra;
    max_width_ += extra;

    /* 
     *   make sure the minimum width is at least the explicitly set width,
     *   if provided 
     */
    if (width_set_ && !width_pct_ && min_width_ < width_)
        min_width_ = width_;

    /* 
     *   tell our display item the newly calculated values - this will allow
     *   enclosing tables to get our horizontal size requirements during the
     *   first pass 
     */
    if (disp_ != 0)
        disp_->set_table_size_limits(min_width_, max_width_);

    /* 
     *   Tell the formatter that we're done with this table, and restore
     *   the enclosing table as the current table under construction.
     *   Note that we need to wait until after we've calculated the table
     *   size limits, because ending the table will go back and reformat
     *   it with the new known size.  
     */
    if (disp_ != 0)
        formatter->end_table(disp_, enclosing_table_, &enclosing_table_pos_,
                             caption_tag_);

    /* restore division alignment */
    formatter->set_blk_alignment(HTML_Attrib_invalid);
}

/*
 *   Get a column at a given column index 
 */
CHtmlTag_column *CHtmlTagTABLE::get_column(size_t colnum)
{
    /* if the column is allocated, return it directly */
    if (colnum < cols_used_)
        return columns_[colnum];

    /* expand the slot list if necessary to make room for the new column */
    if (colnum >= cols_alloced_)
    {
        cols_alloced_ = colnum + 5;
        columns_ = (CHtmlTag_column **)
                   th_realloc(columns_, cols_alloced_
                              * sizeof(CHtmlTag_column *));
    }

    /* allocate entries up to the given point */
    while (colnum >= cols_used_)
        columns_[cols_used_++] = new CHtmlTag_column();

    /* return the new column */
    return columns_[colnum];
}

/*
 *   Calculate the width of table decorations (borders, cell spacing, cell
 *   padding) 
 */
long CHtmlTagTABLE::calc_decor_width()
{
    /* 
     *   calculate the overhead for decorations: a border on each side of the
     *   table of width 'border_'; cell padding on each side of each cell;
     *   cell spacing between each cell and to the left and right of the
     *   table; and two pixels for the left/right borders of each cell, if
     *   any 
     */
    return ((2 * border_)
            + (cols_used_ * cellpadding_ * 2)
            + (cols_used_ * (border_ ? 2 : 0))
            + ((cols_used_ + 1) * cellspacing_));
}

/*
 *   Calculate the width of the table, based on the column contents.  
 */
long CHtmlTagTABLE::calc_table_width(long width_min, long width_max,
                                     long win_width)
{
    size_t colnum;
    int tot_pct;
    long non_pct_max;
    long content_max;

    /* 
     *   If our minimum width doesn't fit the available window width, then
     *   simply use our minimum width; this will require horizontal
     *   scrolling, but this is the smallest we can go.  
     */
    if (width_min >= win_width)
        return width_min;

    /* 
     *   calculate the maximum width of the content part (excluding
     *   decorations) 
     */
    content_max = width_max - calc_decor_width();

    /*
     *   Our minimum size fits the window, so we can expand beyond our
     *   minimum content size.  In the simplest case, we can just check to
     *   see if the maximum fits the window, and top out there if so.  If the
     *   maximum doesn't fit, top out at the window size instead; this will
     *   squeeze the table into a smaller space than it would fill if left to
     *   its own devices, but will minimize horizontal scrolling, which is
     *   more important than maximizing the width of the table.
     *   
     *   If we have any percentage columns, we need to do some extra work.
     *   For each percentage-based column, we need to check the minimum table
     *   size that would be necessary for the column to have its desired
     *   percentage at its maximum content size.  In addition, we must check
     *   how much space would be needed so that the columns *without*
     *   percentage widths would take the percentage of space left over after
     *   accounting for all of the percentage columns.
     *   
     *   Run through each column and check for percentage columns.  
     */
    for (tot_pct = 0, colnum = 0, non_pct_max = 0 ;
         colnum < cols_used_ ; ++colnum)
    {
        CHtmlTag_column *col;
        int cur_pct;

        /* get this column descriptor */
        col = get_column(colnum);

        /* if it has a percentage width, figure its needs */
        if ((cur_pct = col->get_pct_width()) != 0)
        {
            /* 
             *   if this column would take us over 100%, limit it so that the
             *   total doesn't exceed 100% 
             */
            if (tot_pct + cur_pct > 100)
            {
                /* limit the sum of all requests to 100% */
                cur_pct = 100 - tot_pct;

                /* 
                 *   if this leaves us with 0% or less, we've overcommitted;
                 *   simply use the maximum available window width 
                 */
                if (cur_pct <= 0)
                    return win_width;
            }

            /* add this percentage into the total */
            tot_pct += cur_pct;

            /* 
             *   Calculate how big the table has to be to accommodate our
             *   percentage request.  Don't bother if we're asking for 100%,
             *   as this will either overcommit the table (if there are any
             *   other non-empty columns) or provide no information (if we're
             *   the only column); if it overcommits the table, we'll catch
             *   this naturally in the rest of the algorithm, so no need for
             *   a special check here.  
             */
            if (cur_pct < 100)
            {
                long implied_max;

                /* 
                 *   calculate the implied size of the table; round up, since
                 *   we'll round down when calculating percentage during
                 *   column width layout 
                 */
                implied_max = (col->get_max_width()*100 + (cur_pct - 1))
                              / cur_pct;

                /* 
                 *   if this is greater than the maximum table size, increase
                 *   the required maximum table size to accommodate 
                 */
                if (implied_max > content_max)
                {
                    /* increase the table size */
                    width_max += implied_max - content_max;
                    content_max = implied_max;
                }
            }
        }
        else
        {
            /* 
             *   this column doesn't have a percentage width, so add its
             *   maximum width to the sum of the non-percentage column
             *   widths; we'll use this later to make sure the part of the
             *   table without percentage widths adds up to the leftover
             *   percentage after counting all of the percentage-width
             *   columns 
             */
            non_pct_max += col->get_max_width();
        }
    }

    /*
     *   If the total of the non-percentage columns is non-zero, and we've
     *   committed all of the space with percentage columns already, then we
     *   can't possibly satisfy the constraints, so simply make the table as
     *   wide as the available window space.  
     */
    if (non_pct_max != 0 && tot_pct >= 100)
        return win_width;

    /* 
     *   If the total of the non-percentage columns is between 0 and 100,
     *   calculate the table width that we'd need to accommodate the
     *   non-percentage columns at the left-over percentage.  This will
     *   ensure that we have space for all of the non-percentage columns at
     *   their maximum sizes, and can still have all of the percentage
     *   columns at their correct percentages.  
     */
    if (non_pct_max != 0 && tot_pct != 0)
    {
        long implied_max;

        /* 
         *   calculate the implied table width so that the non-percentage
         *   columns take up exactly the left-over percentage 
         */
        implied_max = (non_pct_max*100 + (100 - tot_pct - 1))
                      / (100 - tot_pct);

        /* 
         *   if this is greater than the maximum table size, increase the
         *   required maximum table size 
         */
        if (implied_max > content_max)
        {
            /* increase the table size */
            width_max += implied_max - content_max;
            content_max = implied_max;
        }
    }

    /*
     *   If our maximum size fits the window, top out at the maximum size.
     *   Otherwise, limit our size to the available window space. 
     */
    if (width_max < win_width)
        return width_max;
    else
        return win_width;
}

/*
 *   Set column widths and horizontal offsets.  Returns the final width of
 *   the table.  
 */
void CHtmlTagTABLE::set_column_widths(long table_x_offset, long table_width)
{
    long content_width;
    size_t colnum;
    long x_offset;
    long rem;
    long sum_of_pix;
    CHtmlTag_column *col;
    int unconstrained_cnt;
    long tot_added;
    int tot_pct;
    int force_alloc;

    /* 
     *   calculate the content width as the table width minus the size of
     *   the decorations 
     */
    content_width = table_width - calc_decor_width();

    /*
     *   First, calculate the amount of space we'd have left over if we were
     *   to set every column to its minimum size.  This will tell us how
     *   much extra space there is to distribute among the columns, which
     *   we'll do according to their size constraints and settings.  
     */
    for (rem = content_width, colnum = 0, unconstrained_cnt = 0,
         sum_of_pix = 0 ;
         colnum < cols_used_ ; ++colnum)
    {
        /* get this column descriptor */
        col = get_column(colnum);

        /* initially set this column to its minimum width */
        col->set_width(col->get_min_width());
        
        /* deduct this one's minimum from the remaining leftover space */
        rem -= col->get_min_width();

        /* 
         *   If this column is unconstrained (i.e., it has no WIDTH setting
         *   of any kind), note it.  We'll want to know later if there are
         *   any unconstrained columns, because if there's any leftover
         *   space after we've dealt with all of the constrained columns,
         *   we'll preferentially divide the leftover among the
         *   unconstrained columns.
         *   
         *   If this column is constrained with a pixel width, count the
         *   pixel width request in our sum of pixel width requests.  
         */
        if (col->get_pix_width() == 0 && col->get_pct_width() == 0)
            ++unconstrained_cnt;
        else if (col->get_pix_width() != 0)
            sum_of_pix += col->get_pix_width();
    }

    /*
     *   Next, allocate the percentage requests.  Run through the columns
     *   again looking for percentage width requests, and allocate space for
     *   each one.  
     */
    for (colnum = 0, tot_added = 0, tot_pct = 0 ;
         colnum < cols_used_ ; ++colnum)
    {
        int cur_pct;

        /* get this column descriptor */
        col = get_column(colnum);

        /* if this is a percentage-width column, apply the percentage */
        if ((cur_pct = col->get_pct_width()) != 0)
        {
            long req_width;

            /* limit the total percentage requests to 100% */
            if (tot_pct + cur_pct > 100)
                cur_pct = 100 - tot_pct;

            /* count this in the total */
            tot_pct += cur_pct;

            /* 
             *   calculate the requested width as a percentage of the total
             *   table width 
             */
            req_width = (cur_pct * content_width) / 100;

            /* 
             *   if the requested width is greater than the minimum width of
             *   the column, apply it; ignore it if it's less than the
             *   minimum, since we never go less than the minimum 
             */
            if (req_width > col->get_min_width())
            {
                long added_width;

                /* figure out how much width we're attempting to add */
                added_width = req_width - col->get_width();

                /* count the total added */
                tot_added += added_width;

                /* add the width to the column */
                col->set_width(col->get_width() + added_width);
            }
        }
    }

    /*
     *   If we added more than the extra space we had available to add, go
     *   back and trim off the excess.  
     */
    for (force_alloc = FALSE ; tot_added > rem ; )
    {
        long need_to_trim;
        long tot_trimmed;
        long prop_base;

        /* calculate how much we need to trim */
        need_to_trim = tot_added - rem;

        /* add up the percentage requests of the eligible columns */
        for (colnum = 0, prop_base = 0 ; colnum < cols_used_ ; ++colnum)
        {
            /* get this column descriptor */
            col = get_column(colnum);

            /* 
             *   If this column is eligible for trimming, note it.  It's
             *   eligible if it has a percentage-based width and it's above
             *   its minimum width.  
             */
            if (col->get_pct_width() != 0
                && col->get_width() > col->get_min_width())
            {
                /* add its current width to the proportion base */
                prop_base += col->get_width();
            }
        }

        /* run through the columns and trim space from eligible columns */
        for (colnum = 0, tot_trimmed = 0 ; colnum < cols_used_ ; ++colnum)
        {
            /* get this column descriptor */
            col = get_column(colnum);

            /* if it's eligible, trim some space from it */
            if (col->get_pct_width() != 0
                && col->get_width() > col->get_min_width())
            {
                long cur_trim;

                /* 
                 *   trim from this column in proportion to its size relative
                 *   to the total size of the columns we can trim 
                 */
                cur_trim = (col->get_width() * need_to_trim) / prop_base;

                /* 
                 *   if we're on the final forced-allocation pass, allocate
                 *   all remaining pixels to this column if possible 
                 */
                if (force_alloc)
                    cur_trim = need_to_trim - tot_trimmed;

                /* we can't go below the column's minimum width, though */
                if (col->get_width() - cur_trim < col->get_min_width())
                    cur_trim = col->get_width() - col->get_min_width();

                /* trim space from the column */
                col->set_width(col->get_width() - cur_trim);

                /* count it in the total trimmed */
                tot_trimmed += cur_trim;
            }
        }

        /* adjust the net added width to reflect the trimming we just did */
        tot_added -= tot_trimmed;

        /* 
         *   If we didn't trim anything on this round, then we must have
         *   reached a point where we have a last pixel or two that we can't
         *   de-allocate proportionally due to rounding.  Force allocation
         *   of pixels on the next round.  
         */
        if (tot_trimmed == 0)
            force_alloc = TRUE;
    }

    /* 
     *   deduct the amount added in the percentage pass from the total
     *   remaining space to be distributed 
     */
    rem -= tot_added;

    /*
     *   If anything's left over, and we have one or more columns with
     *   explicit pixel widths, allocate the remaining space among those
     *   explicitly sized columns up to their width settings.
     */
    if (rem != 0 && sum_of_pix != 0)
    {
        long total_req;
        long total_to_alloc;
        long total_alloced;

        /* 
         *   First, check to see if we have enough to bring every column
         *   with an explicit pixel width up to its requested width.
         */
        for (total_req = 0, colnum = 0 ; colnum < cols_used_ ; ++colnum)
        {
            /* get this column descriptor */
            col = get_column(colnum);

            /* 
             *   if this column is pixel-sized, and its requested size is
             *   greater than its current size, add the amount of additional
             *   space it requested to the running total 
             */
            if (col->get_pix_width() != 0
                && col->get_width() < col->get_pix_width())
            {
                /* add its requested additional space to the running sum */
                total_req += col->get_pix_width() - col->get_width();
            }
        }

        /* if there are any requests, service them */
        if (total_req != 0)
        {
            /* 
             *   if the sum of the requests exceeds the available size,
             *   limit the amount to be allocated to the amount available;
             *   otherwise, just allocate the full amount requested 
             */
            total_to_alloc = (total_req < rem ? total_req : rem);
            
            /* allocate the space proportionally to the requests */
            for (total_alloced = 0, colnum = 0 ;
                 colnum < cols_used_ ; ++colnum)
            {
                /* get this column descriptor */
                col = get_column(colnum);

                /* if this is an pixel-sized column, allocate space to it */
                if (col->get_pix_width() != 0)
                {
                    /* 
                     *   if this column isn't already at least at its
                     *   requested size, give it its share of the remaining
                     *   space 
                     */
                    if (col->get_width() < col->get_pix_width())
                    {
                        long share;
                        
                        /* 
                         *   Allocate from the available space
                         *   proportionally to this column's needs for
                         *   additional space.  This should allocate the
                         *   space in such a way that we get equally close
                         *   to the requested size for each column.  
                         */
                        share = ((col->get_pix_width() - col->get_width())
                                 * total_to_alloc)
                                / total_req;
                        
                        /* add the share to the column's width */
                        col->set_width(col->get_width() + share);
                        
                        /* count it in the total we've allocated */
                        total_alloced += share;
                    }
                }
            }

            /* 
             *   deduct the total we allocated from the remaining space to
             *   be allocated 
             */
            rem -= total_alloced;
        }
    }

    /* 
     *   if there are no columns, ignore any remaining space, as there are no
     *   columns into which to distribute the space 
     */
    if (cols_used_ == 0)
        rem = 0;

    /*
     *   We've now applied all percentages and we've expanded each column
     *   with a requested pixel size to the requested pixel size, or at
     *   least as close to it as we can get.  If we still have any space
     *   remaining, divide it up.  Keep going as long as we have more space
     *   available, since we might on any given pass throw back some space
     *   for other columns to use. 
     */
    for (force_alloc = FALSE ; rem != 0 ; )
    {
        long max_sum;
        int all_at_max;
        int base_cnt;
        long space_used;

        /* we haven't used any space on this pass yet */
        space_used = 0;

        /* 
         *   If there are any unconstrained columns, allocate space only to
         *   the unconstrained columns, because this will let us keep the
         *   constrained columns at their optimal sizes.  If there are no
         *   unconstrained columns, just divide the space among all of the
         *   columns: if there's extra space and no unconstrained columns,
         *   the table is over-constrained, so all we can do is add space.  
         */
        if (unconstrained_cnt != 0)
        {
            /* we have unconstrained columns - add space only to these */
            base_cnt = unconstrained_cnt;
        }
        else
        {
            /* no unconstrained columns - add space to all columns */
            base_cnt = cols_used_;
        }

        /* 
         *   Run through the columns to check to see if they're all at least
         *   at their maximum sizes.  If some columns are and some columns
         *   aren't, we'll give space preferentially to those that aren't yet
         *   at their maximum sizes.  
         */
        for (colnum = 0, all_at_max = TRUE ; colnum < cols_used_ ; ++colnum)
        {
            /* 
             *   If this column isn't at its maximum size, they're not all at
             *   their maximum size, obviously.  Only count the column if
             *   we're going to process it (i.e., it's unconstrained, or all
             *   columns are constrained).  
             */
            col = get_column(colnum);
            if (col->get_width() < col->get_max_width()
                && (unconstrained_cnt == 0
                    || (col->get_pix_width() == 0
                        && col->get_pct_width() == 0)))
            {
                /* note that they're not all at maximum width */
                all_at_max = FALSE;

                /* no need to keep looking - one is enough */
                break;
            }
        }

        /* calculate the proportionality base for distributing space */
        for (colnum = 0, max_sum = 0 ; colnum < cols_used_ ; ++colnum)
        {
            /* get this column descriptor */
            col = get_column(colnum);

            /* 
             *   If we're including this column, count it in the
             *   proportionality base.  If we have any unconstrained columns,
             *   include only unconstrained columns; otherwise, include all
             *   columns.  
             */
            if (unconstrained_cnt == 0
                || (col->get_pix_width() == 0 && col->get_pct_width() == 0))
            {
                /* 
                 *   Include this column in the running total.  If all
                 *   columns are at their maximum sizes, simply distribute
                 *   the remaining space proportionally to the maximum sizes.
                 *   Otherwise, distribute space proportionally to the
                 *   *excess* of maximum over minimum size, as this will
                 *   naturally scale all of the columns up to their maximum
                 *   sizes at equal paces.  
                 */
                if (all_at_max)
                    max_sum += col->get_max_width();
                else
                    max_sum += col->get_max_width() - col->get_min_width();
            }
        }

        /* loop over the columns again and hand out the space */
        for (colnum = 0 ; colnum < cols_used_ ; ++colnum)
        {
            /* get this column descriptor */
            col = get_column(colnum);

            /* if we're including this column, add its space */
            if (unconstrained_cnt == 0
                || (col->get_pix_width() == 0 && col->get_pct_width() == 0))
            {
                long added;

                /* 
                 *   Add the space to the column.  If our proportionality
                 *   base is non-zero, divide the space in proportion to our
                 *   contribution to the base.  If the base is zero, simply
                 *   divide the space equally among the unconstrained
                 *   columns.  
                 */
                if (force_alloc)
                {
                    /* 
                     *   we're forcing allocation to the earliest columns
                     *   possible, so give all of the remaining pixels to
                     *   this column 
                     */
                    added = rem - space_used;
                }
                else if (max_sum != 0)
                {
                    /* 
                     *   figure the proportion of space going to this column:
                     *   if all columns are at maximum size, distribute space
                     *   proportionally to the maximum size; otherwise,
                     *   distribute space proportionally to the excess of
                     *   maximum over minimum size 
                     */
                    if (all_at_max)
                        added = (rem * col->get_max_width()) / max_sum;
                    else
                        added = (rem * (col->get_max_width()
                                        - col->get_min_width())) / max_sum;
                }
                else
                {
                    /* there's no proportionality base, so divide evenly */
                    added = rem / base_cnt;
                }

                /* 
                 *   if this would take us over our maximum width, and not
                 *   all of the columns are at their maximum widths yet,
                 *   limit ourselves to our maximum for now 
                 */
                if (!all_at_max
                    && col->get_width() + added > col->get_max_width())
                {
                    /* limit the addition to our maximum width */
                    added = col->get_max_width() - col->get_width();
                    
                    /* but don't shrink */
                    if (added < 0)
                        added = 0;
                }
                
                /* add the space to the column */
                col->set_width(col->get_width() + added);
                
                /* count the added space */
                space_used += added;
            }
        }

        /* deduct the space we just allocated from the remaining space */
        rem -= space_used;

        /*
         *   If we allocated no space on this round, it must mean we've
         *   reached the point where we have a final pixel or two that we
         *   can't allocate proportionally due to rounding.  Force
         *   allocation on the next round.  
         */
        if (space_used == 0)
            force_alloc = TRUE;
    }

    /*
     *   We've now handed out all space.  Run through the columns one last
     *   time and assign the column positions. 
     */
    for (x_offset = table_x_offset + cellspacing_ + border_, colnum = 0 ;
         colnum < cols_used_ ; ++colnum)
    {
        /* get this column descriptor */
        col = get_column(colnum);
        
        /* assign the position */
        col->set_x_offset(x_offset);
        
        /* adjust the column's width for the border and cell spacing */
        col->set_width(col->get_width()
                       + 2 * cellpadding_
                       + (border_ ? 2 : 0));
        
        /* 
         *   advance to the next cell offset: include the width of this cell
         *   plus the inter-cell spacing 
         */
        x_offset += col->get_width() + cellspacing_;
    }
}

/*
 *   Compute the final height of the table.  We'll go through our rows and
 *   compute their final heights, which will tell us the final height of
 *   the table itself. 
 */
void CHtmlTagTABLE::compute_table_height()
{
    CHtmlTag *cur;
    size_t rowcnt;
    size_t rownum;
    int pass;

    /* count the number of rows in the table */
    for (rowcnt = 0, cur = get_contents() ; cur != 0 ;
         cur = cur->get_next_tag())
    {
        /* if it's a row, count it */
        if (cur->get_as_table_row() != 0)
            ++rowcnt;
    }

    /*
     *   Go through the rows and compute the maximum cell height in each row.
     *   Cells may span multiple rows, so we must consider cells beyond a
     *   row's own cells to determine the actual row height.
     *   
     *   Note that we need to do this in two passes.  On the first pass, we
     *   assign heights to all single-row cells (ROWSPAN == 1).  On the
     *   second, we allocate space from multi-row cells.  We need to compute
     *   all of the single-row cell heights before we start working on the
     *   multi-row cells because we allocate the multi-row space in
     *   proportion to the single-row heights.  
     */
    for (pass = 1 ; pass <= 2 ; ++pass)
    {
        /* go through the rows and compute each row's height */
        for (rownum = 0, cur = get_contents() ; cur != 0 ;
             cur = cur->get_next_tag())
        {
            CHtmlTagTR *tr;
            
            /* get this item as a TR tag; if it's not a TR tag, skip it */
            tr = cur->get_as_table_row();
            if (tr == 0)
                continue;
            
            /* have this row ask its cells for their height contributions */
            tr->compute_max_cell_height(rownum, rowcnt, pass);
            
            /* count the row */
            ++rownum;
        }
    }
}

/*
 *   Set row positions. 
 */
void CHtmlTagTABLE::set_row_positions(class CHtmlFormatter *formatter)
{
    CHtmlTag *cur;
    long y_offset;
    long total_height;
    long extra_height;
    long total_interior_height;
    int star_cnt = 0;

    /* clear the total interior height counter */
    total_interior_height = 0;

    /*
     *   Go through my rows and add up the heights.  With the exception of
     *   the <CAPTION> element, every direct child tag should be a <TR>
     *   (table row) tag.  
     */
    for (y_offset = cellspacing_ + border_, cur = get_contents() ; cur != 0 ;
         cur = cur->get_next_tag())
    {
        CHtmlTagTR *tr;
        long height;
        
        /* get this item as a TR tag; if it's not a TR tag, skip it */
        tr = cur->get_as_table_row();
        if (tr == 0)
            continue;

        /* ask this row for its maximum cell height */
        height = tr->get_max_cell_height();

        /* count this row's interior height in the total */
        total_interior_height += height;

        /* add in the internal padding twice (bottom and top) */
        height += 2 * cellpadding_;

        /* 
         *   add two pixels of border (one for bottom and one for top) if
         *   we have a border (the cell borders are always one pixel,
         *   regardless of hte size of the table border) 
         */
        height += (border_ != 0 ? 2 : 0);

        /* add the bottom inter-row spacing */
        height += cellspacing_;

        /* add this row's height into the running total */
        y_offset += height;

        /* if this is a 'height=*' row, note that we have one */
        if (tr->is_height_star())
            ++star_cnt;
    }

    /* the y offset is the total height of the table plus the bottom border */
    total_height = y_offset + border_;

    /*
     *   If we have an explicit height set, and the explicit height is
     *   greater than the actual height we've calculated, we'll need to
     *   expand all of the rows proportionally to the extra height we need
     *   to reach the explicit setting.  We treat the explicit setting as
     *   a minimum height, so if the actual table height exceeds the
     *   explicit setting, we won't change anything. 
     */
    if (height_set_ && calc_height_ > total_height)
        extra_height = calc_height_ - total_height;
    else
        extra_height = 0;

    /*
     *   Now go through again, and actually set all of the row positions. 
     */
    for (y_offset = cellspacing_ + border_, cur = get_contents() ; cur != 0 ;
         cur = cur->get_next_tag())
    {
        CHtmlTagTR *tr;
        long height;
        long interior_height;

        /* get this item as a TR tag; if it's not a TR tag, skip it */
        tr = cur->get_as_table_row();
        if (tr == 0)
            continue;

        /* ask this row for its maximum cell height */
        interior_height = tr->get_max_cell_height();

        /* start with the interior height of the cell */
        height = interior_height;

        /* add in the internal padding twice (bottom and top) */
        height += 2 * cellpadding_;

        /* 
         *   add two pixels of border (one for bottom and one for top) if
         *   we have a border (the cell borders are always one pixel,
         *   regardless of the size of the table border) 
         */
        height += (border_ != 0 ? 2 : 0);

        /* 
         *   If there's extra height, dole it out.  If we have a star row,
         *   we're going to give all of the extra height to the star row;
         *   otherwise, we'll apportion it among the rows. 
         */
        if (star_cnt != 0)
        {
            /* 
             *   If this is a 'height=*' row, give it a fraction of the
             *   extra height.  Otherwise, give it nothing.  'height=*'
             *   rows soak up all of the extra space, and split it evenly
             *   by count.  
             */
            if (tr->is_height_star())
                height += extra_height / star_cnt;
        }
        else
        {
            /* 
             *   Add in this row's portion of the extra height - the
             *   amount we add in is the total extra height times the
             *   ratio of this row's height to the total actual height,
             *   thus giving us a portion of the extra height proportional
             *   to the row's contribution to the overall actual height.
             *   Note that we count only interior heights for the
             *   proportion.  
             */
            height += (long)((double)extra_height
                             * ((double)interior_height
                                / (double)total_interior_height));
        }

        /* set the y offset of the items in the row */
        tr->set_row_y_pos(formatter, this, y_offset, height);

        /* add the height to the current position */
        y_offset += height;

        /* add the bottom inter-row spacing */
        y_offset += cellspacing_;
    }

    /* add the bottom border size */
    y_offset += border_;

    /* tell the formatter the overall size of hte table */
    formatter->end_table_set_size(enclosing_table_, y_offset,
                                  &enclosing_table_pos_);

    /* set the display item's height */
    disp_->set_table_height(y_offset);
}

/* ------------------------------------------------------------------------ */
/*
 *   Tables - CAPTION tag
 */

HTML_attrerr CHtmlTagCAPTION::set_attribute(CHtmlParser *parser,
                                            HTML_Attrib_id_t attr_id,
                                            const textchar_t *val,
                                            size_t vallen)
{
    HTML_Attrib_id_t val_id;

    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_top:
        case HTML_Attrib_bottom:
            /* 
             *   This is a vertical setting.  Use default horizontal
             *   alignment, and remember the desired vertical alignment.  
             */
            valign_ = val_id;
            align_ = HTML_Attrib_invalid;
            break;

        case HTML_Attrib_left:
        case HTML_Attrib_right:
        case HTML_Attrib_center:
            /* horizontal setting - remember it */
            align_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_valign:
        /* check the setting */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_top:
        case HTML_Attrib_bottom:
            /* valid setting - remember it */
            valign_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    default:
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* no errors */
    return HTML_attrerr_ok;
}

/*
 *   when we encounter a CAPTION tag, close any other table structure tags
 *   (rows and cells) that are open 
 */
void CHtmlTagCAPTION::on_parse(CHtmlParser *parser)
{
    /* if there's an open cell, close it */
    parser->close_tag_if_open("TH");
    parser->close_tag_if_open("TD");

    /* if there's an open row, close it */
    parser->close_tag_if_open("TR");

    /* inherit default */
    CHtmlTagContainer::on_parse(parser);
}

/*
 *   finish parsing the CAPTION tag 
 */
void CHtmlTagCAPTION::on_close(CHtmlParser *)
{
    CHtmlTagTABLE *table;
    
    /* find my table container, and tell it about us */
    if ((table = get_table_container()) != 0)
        table->set_caption_tag(this);
}

/*
 *   post-close processing 
 */
void CHtmlTagCAPTION::post_close(CHtmlParser *parser)
{
    /* inherit default */
    CHtmlTagContainer::post_close(parser);

    /* 
     *   add a 'relax' tag after me - this will ensure that whatever follows,
     *   we won't traverse out of the enclosing table if we try to traverse
     *   off the end of our sublist (such as we must do while formatting our
     *   sublist) 
     */
    parser->append_tag(new CHtmlTagRelax());
}

/*
 *   Receive notification that we've just finished parsing the enclosing
 *   table.  If we're aligned at the bottom of the table, we must now
 *   re-insert any text contents into the text array, because the text
 *   will appear in the display list after the table. 
 */
void CHtmlTagCAPTION::on_close_table(CHtmlParser *parser)
{
    /* if I'm aligned below the table, re-insert my text contents here */
    if (valign_ == HTML_Attrib_bottom)
    {
        CHtmlTag *cur;

        /* go through my contents, and re-insert any text */
        for (cur = get_contents() ; cur != 0 ; cur = cur->get_next_tag())
            cur->reinsert_text_array(parser->get_text_array());
    }
}

/*
 *   Format the caption's contents 
 */
void CHtmlTagCAPTION::format_caption(CHtmlSysWin *win,
                                     CHtmlFormatter *formatter)
{
    HTML_Attrib_id_t align;
    HTML_Attrib_id_t old_align;
    CHtmlTag *cur;
    CHtmlTag *last;
        
    /* set the appropriate horizontal alignment */
    align = (align_ == HTML_Attrib_invalid ? HTML_Attrib_center : align_);
    old_align = formatter->get_blk_alignment();
    formatter->set_blk_alignment(align);

    /* make sure we're on a new line */
    formatter->add_disp_item_new_line(new (formatter) CHtmlDispBreak(0));

    /* format our contents */
    last = get_next_fmt_tag(formatter);
    for (cur = get_contents() ; cur != 0 && cur != last ;
         cur = cur->get_next_fmt_tag(formatter))
    {
        /* format this tag */
        cur->format(win, formatter);
    }

    /* end the line */
    formatter->add_disp_item_new_line(new (formatter) CHtmlDispBreak(0));

    /* restore old alignment */
    formatter->set_blk_alignment(old_align);
}

/* ------------------------------------------------------------------------ */
/*
 *   Tables - TR (table row) tag
 */

HTML_attrerr CHtmlTagTR::set_attribute(CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val,
                                       size_t vallen)
{
    HTML_Attrib_id_t val_id;
    HTML_attrerr err;

    switch(attr_id)
    {
    case HTML_Attrib_align:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
            /* valid setting - remember it */
            align_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_valign:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_top:
        case HTML_Attrib_middle:
        case HTML_Attrib_bottom:
            /* valid setting - remember it */
            valign_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_bgcolor:
        /* parse the color */
        err = set_color_attr(parser, &bgcolor_, val, vallen);

        /* if we got a valid color, note that we want to use it*/
        if (err == HTML_attrerr_ok)
            use_bgcolor_ = TRUE;

        /* return the result of parsing the color */
        return err;

    case HTML_Attrib_background:
        /* store the background image URL */
        background_.set_url(val, vallen);
        break;

    case HTML_Attrib_height:
        /* check for a '*' attribute */
        if (vallen == 1 && *val == '*')
        {
            /* note that we have a '*' for the height */
            height_star_ = TRUE;
        }
        else
        {
            /* set the height attribute */
            height_set_ = TRUE;
            return set_number_attr(&height_, val, vallen);
        }
        break;

    default:
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* no errors */
    return HTML_attrerr_ok;
}

void CHtmlTagTR::on_parse(CHtmlParser *parser)
{
    /* close a CAPTION if it's open */
    parser->close_tag_if_open("CAPTION");

    /* if we're currently in a cell (TD or TH), close it */
    parser->close_tag_if_open("TD");
    parser->close_tag_if_open("TH");

    /* if we're currently in another row, close it */
    parser->close_tag_if_open("TR");

    /* add this tag as normal */
    CHtmlTagContainer::on_parse(parser);
}

void CHtmlTagTR::pre_close(CHtmlParser *parser)
{
    /* close any open cell */
    parser->close_tag_if_open("TD");
    parser->close_tag_if_open("TH");
}

void CHtmlTagTR::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlTagTABLE *table;

    /* get my table - if I don't have a table, I can't do anything */
    table = get_container()->get_table_container();
    if (table == 0)
        return;

    /* load our background image if needed */
    format_load_bg(win, formatter);

    /* get my row, by asking the table for its current row */
    rownum_ = table->get_cur_rownum();

    /* 
     *   reset my maximum height -- the maximum height is set at the end
     *   of pass 2 
     */
    max_cell_height_ = 0;

    /* start at the first column */
    cur_colnum_ = 0;
}

void CHtmlTagTR::format_exit(CHtmlFormatter *)
{
    CHtmlTagTABLE *table;

    /* get my table - if I don't have a table, I can't do anything */
    table = get_container()->get_table_container();
    if (table == 0) return;

    /* tell the table to move on to the next row */
    table->set_cur_rownum(rownum_ + 1);
}

/*
 *   Compute the maximum height of cells in this row, and add their
 *   contributions to this row and any other rows they span.
 *   
 *   Note that this routine has to be called twice - call it for each row in
 *   the table with 'pass' set to 1, then go back and call it again for each
 *   row with 'pass' set to 2.  On the first pass, we calculate the height
 *   contribution of each cell that spans only a single row.  On the second
 *   pass, we go back and figure how to apportion the height contributions of
 *   cells that span multiple rows.  We need to figure all of the single-row
 *   contributions first, because we allocate the multi-row contributions in
 *   proportion to the single-row heights.  
 */
void CHtmlTagTR::compute_max_cell_height(size_t rownum, size_t rowcnt,
                                         int pass)
{
    long max_height;
    CHtmlTag *cur;

    /*
     *   Loop through my cells.  Ask each cell for its height, and find
     *   the largest. 
     */
    for (max_height = 0, cur = get_contents() ; cur != 0 ;
         cur = cur->get_next_tag())
    {
        CHtmlTagTableCell *cell;
        CHtmlTag *row;
        long height;
        long span;
        long span_left;
        
        /* if this isn't a table cell, ignore it */
        cell = cur->get_as_table_cell();
        if (cell == 0)
            continue;

        /*
         *   Get the number of rows we should distribute the cell's height
         *   over.  We normally distribute over the rowspan, but if the
         *   rowspan exceeds the number of rows left in the table after
         *   this row, we limit the rowspan to the number of rows
         *   remaining. 
         */
        span = cell->get_rowspan();
        if (span > (long)(rowcnt - rownum))
            span = (long)(rowcnt - rownum);

        /* get our cell height */
        height = cell->get_table_cell_height();

        /* 
         *   If we span only one row, assign the full height to that row.
         *   
         *   If we span more than one row, assign the height to our several
         *   rows in proportion to the single-cell heights of the rows.
         *   Because we need to calculate the single-cell heights first, we
         *   can't allocate the multi-row heights until the second pass. 
         */
        if (span <= 1)
        {
            /* we occupy a single row, so assign the height to our row */
            include_cell_height(height);
        }
        else if (pass == 2)
        {
            CHtmlTagTR *tr;
            long cur_tot;
            long extra;
            long rem;

            /*
             *   We span multiple rows, and this is the second pass.
             *   Allocate our height to each of the rows we span.  Dole out
             *   the extra height in proportion to the current heights of the
             *   spanned rows.  So, first calculate the total heights of the
             *   spanned rows. 
             */
            for (span_left = span, row = this, cur_tot = 0 ;
                 row != 0 && span_left != 0 ; row = row->get_next_tag())
            {
                /* if this is a row, get its height */
                if ((tr = row->get_as_table_row()) != 0)
                {
                    /* add the height to the total */
                    cur_tot += tr->get_max_cell_height();

                    /* count it */
                    --span_left;
                }
            }

            /*
             *   We now have the total of the current heights of the spanned
             *   rows.  Calculate the excess height we need to distribute:
             *   this is the excess of our height over the total current
             *   height. 
             */
            extra = (height > cur_tot ? height - cur_tot : 0);

            /* remember the total remaining to distribute */
            rem = extra;

            /* distribute the extra height proportionally */
            for (span_left = span, row = this ;
                 row != 0 && span_left != 0 ; row = row->get_next_tag())
            {
                long delta;
                long ht;
                
                /* if this isn't a row, skip it */
                if ((tr = row->get_as_table_row()) == 0)
                    continue;

                /* count the row */
                --span_left;

                /* get this row's current height */
                ht = tr->get_max_cell_height();
                
                /* 
                 *   Calculate the share of the extra height to put here.  If
                 *   this is the last row, distribute the entire remaining
                 *   height here, to avoid losing any pixels due to
                 *   accumulated rounding errors.  Otherwise, if the total
                 *   height is non-zero, give this cell its share of the
                 *   height in proportion to its current share of the total
                 *   current height.  If the total height is zero, we can't
                 *   calculate proportions, so simply dole out the extra
                 *   height in equal shares to our spanned cells. 
                 */
                delta = (span_left == 0
                         ? rem
                         : (cur_tot != 0
                            ? (extra * ht) / cur_tot
                            : extra / span));

                /* add the extra share to the row height */
                tr->include_cell_height(ht + delta);

                /* deduct this allocation from the remainder */
                rem -= delta;
            }
        }
    }
}

/*
 *   Set the y positions of the items in this row. 
 */
void CHtmlTagTR::set_row_y_pos(CHtmlFormatter *formatter,
                               CHtmlTagTABLE *table,
                               long y_offset, long height)
{
    CHtmlTag *cur;

    /* loop through my cells and set each one's vertical position */
    for (cur = get_contents() ; cur != 0 ; cur = cur->get_next_tag())
    {
        CHtmlTagTableCell *cell;
        CHtmlTag *nxt;
        long cell_height;
        long span_left;

        /* if this isn't a table cell, ignore it */
        cell = cur->get_as_table_cell();
        if (cell == 0)
            continue;

        /* 
         *   compute this cell's height - if the cell spans only one row,
         *   it's the current row's height; otherwise, we need to add in
         *   the heights of the spanned rows 
         */
        cell_height = height;
        for (nxt = this, span_left = cell->get_rowspan() ; span_left > 1 ;
             --span_left)
        {
            CHtmlTagTR *tr;
            
            /* find the next TR element */
            for (tr = 0, nxt = nxt->get_next_tag() ; nxt != 0 ;
                 nxt = nxt->get_next_tag())
            {
                /* if this is a TR element, stop searching */
                if ((tr = nxt->get_as_table_row()) != 0)
                    break;
            }

            /* if we didn't find another row, we've spanned all we can */
            if (tr == 0)
                break;

            /* 
             *   add this row's height to the total height of the cell,
             *   including all of the spacing 
             */
            cell_height += tr->get_max_cell_height()
                           + table->get_cellspacing()
                           + 2 * table->get_cellpadding()
                           + (table->get_border() != 0 ? 2 : 0);
        }

        /* set this cell's vertical position */
        cell->set_cell_y_pos(formatter, y_offset, cell_height, valign_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Tables - basic cell tag - base class for TH (table heading) and TD
 *   (table cell data) 
 */

CHtmlTagTableCell::CHtmlTagTableCell(CHtmlParser *parser)
    : CHtmlTagContainer(), CHtmlTableImage()
{
    /* NOWRAP not yet specified */
    nowrap_ = FALSE;

    /* default to spanning one row and column */
    rowspan_ = colspan_ = 1;

    /* no alignment settings yet */
    align_ = valign_ = HTML_Attrib_invalid;

    /* no width or height explicitly set yet */
    width_set_ = height_set_ = FALSE;
    width_pct_ = FALSE;

    /* haven't set my row and column position yet */
    row_col_set_ = FALSE;
    rownum_ = 0;
    colnum_ = 0;

    /* no display item yet */
    disp_ = 0;
    disp_last_ = 0;

    /* no contents yet */
    content_height_ = 0;
    disp_y_base_ = 0;
}

HTML_attrerr CHtmlTagTableCell::set_attribute(CHtmlParser *parser,
                                              HTML_Attrib_id_t attr_id,
                                              const textchar_t *val,
                                              size_t vallen)
{
    HTML_Attrib_id_t val_id;
    HTML_attrerr err;

    switch(attr_id)
    {
    case HTML_Attrib_nowrap:
        /* note the presence of NOWRAP */
        nowrap_ = TRUE;
        break;
        
    case HTML_Attrib_align:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_left:
        case HTML_Attrib_center:
        case HTML_Attrib_right:
            /* valid setting - remember it */
            align_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_valign:
        /* see what we have */
        val_id = parser->attrval_to_id(val, vallen);
        switch(val_id)
        {
        case HTML_Attrib_top:
        case HTML_Attrib_middle:
        case HTML_Attrib_bottom:
            /* valid setting - remember it */
            valign_ = val_id;
            break;

        default:
            /* other settings are invalid */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_rowspan:
        return set_number_attr(&rowspan_, val, vallen);

    case HTML_Attrib_colspan:
        return set_number_attr(&colspan_, val, vallen);

    case HTML_Attrib_width:
        /* set the horizontal size, and note that it's been set */
        width_set_ = TRUE;
        return set_number_or_pct_attr(&width_, &width_pct_, val, vallen);

    case HTML_Attrib_height:
        /* set the horizontal size, and note that it's been set */
        height_set_ = TRUE;
        return set_number_attr(&height_, val, vallen);

    case HTML_Attrib_bgcolor:
        /* parse the color */
        err = set_color_attr(parser, &bgcolor_, val, vallen);

        /* if we got a valid color, note that we want to use it*/
        if (err == HTML_attrerr_ok)
            use_bgcolor_ = TRUE;

        /* return the result of parsing the color */
        return err;

    case HTML_Attrib_background:
        /* store the background image URL */
        background_.set_url(val, vallen);
        break;

    default:
        return CHtmlTagContainer::set_attribute(parser, attr_id, val, vallen);
    }

    /* no errors */
    return HTML_attrerr_ok;
}

/*
 *   Parse a cell tag.  Since a new table tag implicitly closes the
 *   previous cell, we will close the current TH or TD tag, if one is
 *   open.  
 */
void CHtmlTagTableCell::on_parse(CHtmlParser *parser)
{
    /* close a CAPTION if it's open */
    parser->close_tag_if_open("CAPTION");

    /* if there's an open cell, implicitly close it */
    parser->close_tag_if_open("TH");
    parser->close_tag_if_open("TD");
    
    /* 
     *   add a space between cells, so that we always find word boundaries
     *   between cells and don't get confused into thinking a word can
     *   span cells 
     */
    parser->get_text_array()->append_text_noref(" ", 1);

    /* parse this tag normally */
    CHtmlTagContainer::on_parse(parser);
}

/*
 *   format a table cell 
 */
void CHtmlTagTableCell::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    CHtmlTagTABLE *table;
    CHtmlTagTR *row;
    size_t rownum;
    size_t colnum;
    long x_offset, col_width;
    long padding;
    HTML_Attrib_id_t align;
    int use_bgcolor;
    HTML_color_t bgcolor;
    
    /* 
     *   get my table and row -- if I don't have both, we can't format
     *   this item as a table cell 
     */
    table = get_container()->get_table_container();
    row = get_container()->get_table_row_container();
    if (table == 0 || row == 0)
        return;

    /* load our background image if needed */
    format_load_bg(win, formatter);

    /* 
     *   Determine my row and column position.  We start with the current
     *   row and column, and look for a block of available cells big
     *   enough for our rowspan and colspan settings.  All cells in the
     *   block must be available.  We'll keep going right until we find a
     *   big enough block, but we'll always stay in the current row.
     *   
     *   Note that if we've formatted the table already, the row and
     *   column positions will already be known, in which case we don't
     *   have to figure them out again.  Reformatting the same page will
     *   never change the row and column positions, as these are derived
     *   from the table structure itself, not from the on-screen layout.  
     */
    if (!row_col_set_)
    {
        for (rownum = row->get_rownum(), colnum = row->get_cur_colnum() ;; )
        {
            size_t r, c;
            int ok;
            
            /*
             *   See if the current position has enough space available.
             *   All cells for our row and column spans must be
             *   unoccupied, so that we don't overlap the contents of any
             *   other cells.  
             */
            for (ok = TRUE, c = colnum ; ok && c < colnum + colspan_ ; ++c)
            {
                for (r = rownum ; ok && r < rownum + rowspan_ ; ++r)
                {
                    /* see if this position is occupied */
                    if (table->get_cell(r, c)->is_occupied())
                    {
                        /*
                         *   This position is occupied.  Clear the "ok"
                         *   flag to indicate that we can't use this
                         *   position.  In addition, move the proposed
                         *   column position to the next column after this
                         *   one, since we obviously can't use anything to
                         *   the left of this point - if we overlap at the
                         *   current position, we'd overlap at the next
                         *   position to the right, and so on until we're
                         *   all the way to the right of this column.  
                         */
                        ok = FALSE;
                        colnum = c + 1;
                    }
                }
            }
            
            /* if the position checked out, we can stop looking */
            if (ok)
                break;
        }
        
        /* we've determined our position - remember it */
        rownum_ = rownum;
        colnum_ = colnum;
        row_col_set_ = TRUE;
    }
        

    /*
     *   Claim our block of cells by setting the 'occupied' flag on each
     *   cell in our block. 
     */
    for (rownum = rownum_ ; rownum < rownum_ + rowspan_ ; ++rownum)
        for (colnum = colnum_ ; colnum < colnum_ + colspan_ ; ++colnum)
            table->get_cell(rownum, colnum)->set_occupied();

    /* 
     *   tell the row to start the next column at the next column after
     *   the last one we're using 
     */
    row->set_cur_colnum(colnum_ + colspan_);

    /* 
     *   Calculate our x offset and column width.  The x offset is the x
     *   offset of my column; the width is the sum of the widths of my
     *   spanned columns. 
     */
    x_offset = table->get_column(colnum_)->get_x_offset();
    for (colnum = colnum_, col_width = 0 ; colnum < colnum_ + colspan_ ;
         ++colnum)
        col_width += table->get_column(colnum)->get_width();

    /* add the inter-column padding for spanned columns */
    col_width += (colspan_ - 1) * table->get_cellspacing();

    /* get the background color from the row, if it's not set in the cell */
    if (use_bgcolor_)
    {
        /* it's set in the cell - this overrides any row setting */
        use_bgcolor = TRUE;
        bgcolor = map_color(win, bgcolor_);
    }
    else
    {
        /* not set in the cell - use the setting from the row */
        use_bgcolor = row->get_use_bgcolor();
        bgcolor = row->get_bgcolor();
    }

    /* create the display item for the cell */
    disp_ = new (formatter)
            CHtmlDispTableCell(win, table->get_border() != 0,
                               bgcolor, use_bgcolor, bg_image_,
                               width_, width_pct_, width_set_);

    /* 
     *   Set horizontal alignment for the cell's contents.  If we don't
     *   have an explicit setting, use the row setting; if the row doesn't
     *   have an explicit setting, use the appropriate default for this
     *   type of cell. 
     */
    align = align_;
    if (align == HTML_Attrib_invalid)
        align = row->get_align();
    if (align == HTML_Attrib_invalid)
        align = get_default_cell_alignment();

    /*
     *   Tell the formatter we're beginning a new cell.  We must tell the
     *   formatter our column offset and size; the formatter will ignore
     *   this on the first pass, since we won't have the final results in
     *   at that point, but it will use it on the second pass.  Note that
     *   we must take the internal cell padding into account, since the
     *   offset and width include the space taken by the padding, but the
     *   contents must fit inside the padding.  
     */
    padding = table->get_cellpadding() + (table->get_border() ? 1 : 0);
    formatter->begin_table_cell(disp_, x_offset, x_offset + padding,
                                col_width - 2*padding);

    /* 
     *   Tell the formatter to set our alignment.  Wait until after we've
     *   already started the cell with the formatter, because our
     *   alignment setting affects the cell's *contents*, not the cell
     *   itself.
     *   
     *   Set the DIVISION alignment, not just the block-level alignment --
     *   we want every block in the cell to be aligned the same way, if
     *   the table contains multiple blocks (it may, for example, contain
     *   multiple paragraphs); set the block alignment as well, so that
     *   the implicit first block in the cell gets this alignment.
     *   
     *   Remember the enclosing alignment so that we can restore it when
     *   we're done.  
     */
    old_align_ = formatter->get_div_alignment();
    formatter->set_div_alignment(align);
    formatter->set_blk_alignment(align);

    /* set the cell's width */
    disp_->set_cell_width(col_width);
}

/* 
 *   exit formatting of a table cell 
 */
void CHtmlTagTableCell::format_exit(CHtmlFormatter *formatter)
{
    CHtmlDisp_wsinfo wsinfo;

    /* finish up any floating items that have been deferred */
    formatter->pre_end_table_cell();

    /*
     *   Scan our contents for trailing whitespace, and remove any we find.
     *   Trailing whitespace in a table cell can disturb the layout, so we
     *   throw it away.  
     */
    wsinfo.remove_trailing_whitespace(formatter->get_win(), disp_, 0);

    /* 
     *   add a line break; this is not needed as an actual line break, but
     *   we do need it to mark the end of our contents in the display list 
     */
    disp_last_ = new (formatter) CHtmlDispBreak(0);
    formatter->add_disp_item_new_line(disp_last_);

    /* restore the enclosing alignment */
    formatter->set_div_alignment(old_align_);
    
    /* tell the formatter that we're done with this cell */
    formatter->end_table_cell(this, disp_);
}

/*
 *   Receive information on our metrics.  The formatter calls this as we're
 *   leaving each cell on pass 1.  We simply store the minimum and maximum
 *   widths of our contents for later here; we'll use this information in
 *   compute_column_metrics() to figure our contribution to the column
 *   layout.  
 */
void CHtmlTagTableCell::set_table_cell_metrics(CHtmlTableMetrics *metrics)
{
    /* remember our calculated metrics for later */
    cont_min_width_ = metrics->min_width_;
    cont_max_width_ = metrics->max_width_;
}

/*
 *   Compute our width contribution to the column or columns we occupy.  This
 *   is called from our table's format_exit() routine, when we're done with
 *   the table - we have to wait until the end of the table to compute our
 *   column widths because our allocations sometimes depend on the other
 *   cells in the table.
 *   
 *   This must be called twice - call it for each cell in the table with
 *   'pass' set to 1, then go back and call it again for each cell with
 *   'pass' set to 2.  On the first pass, we'll compute the column width
 *   contribution for each cell that occupies a single column.  On the second
 *   pass, we'll calculate the contribution for each cell that spans multiple
 *   columns (i.e., COLSPAN > 1).  We have to wait to do the multi-column
 *   cells until after doing all of the single-column cells because we
 *   allocate the multi-column space in proportion to the widths calculated
 *   for the single-column cells.  
 */
void CHtmlTagTableCell::compute_column_metrics(int pass)
{
    CHtmlTagTABLE *table;
    size_t c;
    long min_width;
    long max_width;

    /* get the settings from the saved metrics */
    min_width = cont_min_width_;
    max_width = cont_max_width_;

    /* 
     *   If we have an explicit WIDTH setting in pixels, then use this as our
     *   maximum rather than the actual maximum.  We use this as the maximum
     *   because an explicit preferred size setting overrides the implied
     *   preferred size, which is the content size.  
     */
    if (width_set_ && !width_pct_)
    {
        /* limit our maximum width to the requested size */
        max_width = width_;

        /* make sure we don't go under our minimum content width, though */
        if (max_width < min_width)
            max_width = min_width;
    }

    /* 
     *   get my table -- if I don't have one, we can't format this item as a
     *   table cell 
     */
    table = get_container()->get_table_container();
    if (table == 0)
        return;

    /* 
     *   If we have span only one column (COLSPAN == 1), we simply contribute
     *   our entire width to our one column.
     *   
     *   If we span multiple columns, we have to divide our width among our
     *   spanned columns.  Rather than distributing the width in equal parts
     *   to our columns, dole it out in proportion to the widths the columns
     *   would have without our contribution.  To do this, we have to wait
     *   until our second pass through the table, at which point the
     *   contributions from other rows will be known. 
     */
    if (colspan_ <= 1)
    {
        CHtmlTag_column *colp = table->get_column(colnum_);

        /* we span only one column, so give our full width to it */
        colp->add_cell_widths(min_width, max_width);

        /* if we have an explicit WIDTH setting, allocate it to the column */
        if (width_set_)
        {
            /* apply the pixel or percentage width, as appropriate */
            if (width_pct_)
                colp->add_cell_pct_width(width_);
            else
                colp->add_cell_pix_width(width_);
        }
    }
    else if (pass == 2)
    {
        long min_tot, max_tot, width_tot;
        long min_extra, max_extra, width_extra;
        long min_rem, max_rem, width_rem;
        CHtmlTag_column *colp;

        /* 
         *   We span multiple columns, and we're on the second pass.
         *   Distribute our column width in proportion to the widths our
         *   spanned columns would otherwise have, if we weren't part of the
         *   table.  First, calculate the total widths our columns have so
         *   far.  Since we're on the second pass, we will have already
         *   calculated the widths from single-span cells in the same columns
         *   in other rows. 
         */
        for (c = colnum_, min_tot = max_tot = width_tot = 0 ;
             c < colnum_ + colspan_ ; ++c)
        {
            /* sum the totals for this column */
            colp = table->get_column(c);
            min_tot += colp->get_min_width();
            max_tot += colp->get_max_width();
            width_tot += (width_set_ && width_pct_
                          ? colp->get_pct_width() : colp->get_pix_width());
        }

        /* calculate the excess space we have to dole out */
        min_extra = (min_width > min_tot ? min_width - min_tot : 0);
        max_extra = (max_width > max_tot ? max_width - max_tot : 0);

        /* if the WIDTH attribute is set, calculate its excess as well */
        width_extra = 0;
        if (width_set_)
        {
            /* 
             *   for percentages, dole out the entire percentage
             *   proportionally; for pixel widths, dole out only the excess
             *   pixel width proportionally 
             */
            if (width_pct_)
                width_extra = width_pct_;
            else
                width_extra = (width_ > width_tot ? width_ - width_tot : 0);
        }

        /* 
         *   keep track of how much we've actually doled out, to ensure that
         *   we don't lose any pixels through rounding errors 
         */
        min_rem = min_extra;
        max_rem = max_extra;
        width_rem = width_extra;

        /* dole out our excess min and max widths proportionally */
        for (c = colnum_ ; c < colnum_ + colspan_ ; ++c)
        {
            long min_cur, max_cur;
            long min_delta, max_delta;
            int is_last_col = (c == colnum_ + colspan_ - 1);
            
            /* get this column and its current metrics */
            colp = table->get_column(c);
            min_cur = colp->get_min_width();
            max_cur = colp->get_max_width();

            /* 
             *   Compute the proportion of the extra widths to hand out.  If
             *   the existing total is zero, it means that the columns are
             *   all otherwise empty (i.e., it has no COLSPAN=1 entries at
             *   all), so dole out our space evenly in this case.  In any
             *   case, if this is the last column, to avoid losing pixels
             *   through accumulated rounding errors, give the entire unused
             *   balance to this column.  
             */
            min_delta = (is_last_col
                         ? min_rem
                         : (min_tot != 0
                            ? (min_extra * min_cur) / min_tot
                            : min_extra / colspan_));
            max_delta = (is_last_col
                         ? max_rem
                         : (max_tot != 0
                            ? (max_extra * max_cur) / max_tot
                            : max_extra / colspan_));

            /* add in the deltas */
            colp->add_cell_widths(min_cur + min_delta, max_cur + max_delta);

            /* deduct these deltas from the total to be allocated */
            min_rem -= min_delta;
            max_rem -= max_delta;

            /* if the width is set, hand out the extra set width as well */
            if (width_set_)
            {
                long width_cur;
                long width_delta;
                
                /* get the appropriate current column width */
                if (width_pct_)
                    width_cur = colp->get_pct_width();
                else
                    width_cur = colp->get_pix_width();

                /* calculate the proportional share to hand out */
                width_delta = (is_last_col
                               ? width_rem
                               : (width_tot != 0
                                  ? (width_extra * width_cur) / width_tot
                                  : width_extra / colspan_));

                /* allocate the delta to the appropriate width counter */
                if (width_pct_)
                    colp->add_cell_pct_width(width_delta);
                else
                    colp->add_cell_pix_width(width_cur + width_delta);

                /* deduct the delta from the total to be allocated */
                width_rem -= width_delta;
            }
        }
    }
}

/*
 *   Receive information on the formatted height of the cell's contents.
 *   The formatter calls this after the cell's contents have been
 *   formatted at the cell's proper width in pass 2.  In addition, we'll
 *   receive information on the base y position of the cell's contents, so
 *   that we can later offset the cell's contents to their final y
 *   positions.  
 */
void CHtmlTagTableCell::set_table_cell_height(long height, long base_y_pos)
{
    /* save the y offset of the cell's contents */
    disp_y_base_ = base_y_pos;
    
    /* assume we'll use the actual height */
    content_height_ = height;
    
    /* if the HEIGHT attribute is greater, use it instead */
    if (height_set_ && height_ > content_height_)
        content_height_ = height_;
}

/*
 *   Set the cell's vertical position 
 */
void CHtmlTagTableCell::set_cell_y_pos(CHtmlFormatter *formatter,
                                       long row_y_offset, long row_height,
                                       HTML_Attrib_id_t row_valign)
{
    HTML_Attrib_id_t valign;
    long ypos;

    /* use the row's valign if we don't have our own */
    valign = (valign_ == HTML_Attrib_invalid ? row_valign : valign_);

    /* figure the position based on the alignment */
    switch(valign)
    {
    case HTML_Attrib_top:
        /* align at the top of the row */
        ypos = row_y_offset;
        break;

    case HTML_Attrib_middle:
    case HTML_Attrib_invalid:
        /* align centered in the row */
        ypos = row_y_offset + (row_height - content_height_) / 2;
        break;

    case HTML_Attrib_bottom:
        /* align at the bottom of the row */
        ypos = row_y_offset + (row_height - content_height_);
        break;

    default:
        /* other attributes should never occur; ignore them if they do */
        break;
    }
    
    /* tell the formatter to set y positions for items in the cell */
    formatter->set_table_cell_y_pos(disp_, disp_last_, ypos, disp_y_base_,
                                    row_y_offset);

    /* set my display item's position height */
    disp_->set_cell_height(row_height);
}

/* ------------------------------------------------------------------------ */
/*
 *   SOUND tag 
 */

CHtmlTagSOUND::~CHtmlTagSOUND()
{
    if (sound_ != 0)
        sound_->remove_ref();
}

HTML_attrerr CHtmlTagSOUND::set_attribute(CHtmlParser *parser,
                                          HTML_Attrib_id_t attr_id,
                                          const textchar_t *val,
                                          size_t vallen)
{
    int pct;

    switch(attr_id)
    {
    case HTML_Attrib_src:
        src_.set_url(val, vallen);
        break;

    case HTML_Attrib_layer:
        /* allow foreground, background, or ambient */
        switch(layer_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_foreground:
        case HTML_Attrib_background:
        case HTML_Attrib_ambient:
        case HTML_Attrib_bgambient:
            break;

        default:
            /* invalid LAYER setting */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_alt:
        alt_.set(val, vallen);
        break;

    case HTML_Attrib_repeat:
        /* see if we have a keyword value */
        has_repeat_ = TRUE;
        if (parser->attrval_to_id(val, vallen) == HTML_Attrib_loop)
        {
            /* loop indefinitely - set repeat_ to zero to so indicate */
            repeat_ = 0;
        }
        else
        {
            /* treat it as a number */
            return set_number_attr(&repeat_, val, vallen);
        }
        break;

    case HTML_Attrib_fadein:
        return set_number_attr(&fadein_, val, vallen);

    case HTML_Attrib_fadeout:
        return set_number_attr(&fadeout_, val, vallen);

    case HTML_Attrib_random:
        return set_number_or_pct_attr(&random_, &pct, val, vallen);

    case HTML_Attrib_cancel:
        /* 
         *   see if they're cancelling a layer; if not, they're cancelling
         *   all layers 
         */
        cancel_ = TRUE;
        switch(layer_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_foreground:
        case HTML_Attrib_background:
        case HTML_Attrib_ambient:
        case HTML_Attrib_bgambient:
            /* accept the layer setting */
            break;

        case HTML_Attrib_cancel:
            /* no layer setting - cancel all layers */
            layer_ = HTML_Attrib_invalid;
            break;

        default:
            /* invalid setting */
            return HTML_attrerr_inv_enum;
        }
        break;

    case HTML_Attrib_interrupt:
        /* note that we're interrupting any currently-playing sound */
        interrupt_ = TRUE;
        break;

    case HTML_Attrib_sequence:
        /* accept REPLACE, RANDOM, and CYCLE */
        switch(sequence_ = parser->attrval_to_id(val, vallen))
        {
        case HTML_Attrib_replace:
        case HTML_Attrib_random:
        case HTML_Attrib_cycle:
            /* valid answer */
            break;

        default:
            /* invalid answer */
            return HTML_attrerr_inv_enum;
        }
        break;

    default:
        /* ignore other attributes */
        break;
    }

    /* no problems */
    return HTML_attrerr_ok;
}

/*
 *   "Format" a sound tag.  This initiates playback of the sound. 
 */
void CHtmlTagSOUND::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    HTML_res_type_t restype;

    /* if the tag is obsolete, ignore it */
    if (obsolete_)
        return;

    /* do nothing if sound tags aren't allowed in this kind of window */
    if (!formatter->allow_sound_tags())
        return;

    /* remember the previous sound tag */
    prv_sound_ = formatter->get_last_sound_tag();

    /* if this is a CANCEL operation, cancel sounds */
    if (cancel_)
    {
        formatter->cancel_sound(layer_);

        /* 
         *   this tag doesn't need to be played back again, since
         *   everything up to this point is now obsolete 
         */
        obsolete_ = TRUE;

        /*
         *   Obsolete all prior sound tags in the same layer, since
         *   they'll never need to be played again. 
         */
        obsolete_prev_sounds(layer_);

        /* done */
        return;
    }

    /* if we don't have a source, there's nothing to do */
    if (src_.get_url() == 0)
    {
        oshtml_dbg_printf("<SOUND>: SRC must be specified\n");
        return;
    }

    /* get the type of the source and check it for validity */
    switch(restype = CHtmlResType::get_res_type(src_.get_url()))
    {
    case HTML_res_type_MIDI:
    case HTML_res_type_WAV:
    case HTML_res_type_MP123:
    case HTML_res_type_OGG:
        /* these are valid types for sound playback */
        break;

    default:
        /* other types can't be played as sounds */
        oshtml_dbg_printf("<SOUND>: invalid resource type\n");
        return;
    }

    /*
     *   If characteristics of the sound (such as its type) determine the
     *   layer in which it must play, force the correct layer.
     */
    if (restype == HTML_res_type_MIDI)
    {
        /* MIDI files always play in the background */
        layer_ = HTML_Attrib_background;
    }
    else if (restype == HTML_res_type_MP123 || restype == HTML_res_type_OGG)
    {
        /* 
         *   MPEG audio and Ogg Vorbis audio can go in any layer - these
         *   formats are equally suitable for background music or sound
         *   effects in any other layer.  We can't make any guesses about the
         *   layer for these types.  
         */
    }
    else if (restype == HTML_res_type_WAV)
    {
        /* 
         *   WAV resources can go in the foreground or ambient layers - if
         *   the layer is unspecified, try to guess based on whether we
         *   have a RANDOM setting or not 
         */
        if (layer_ == HTML_Attrib_invalid)
        {
            if (random_ == 0)
            {
                /* no RANDOM setting - it's a foreground sound */
                layer_ = HTML_Attrib_foreground;
            }
            else
            {
                /* RANDOM is set - it's an ambient sound */
                layer_ = HTML_Attrib_ambient;
            }
        }
    }

    /* if we don't know the layer, we can't play the sound */
    if (layer_ == HTML_Attrib_invalid)
    {
        oshtml_dbg_printf("<SOUND>: LAYER must be specified\n");
        return;
    }

    /* if we don't have the sound loaded, load it now */
    if (sound_ == 0)
    {
        /* find it in the cache or load it */
        sound_ = formatter->get_res_cache()->
                 find_or_create(win, formatter->get_res_finder(), &src_);

        /* if it's not a valid image, log an error */
        if (sound_ == 0 || sound_->get_sound() == 0)
            oshtml_dbg_printf("<SOUND>: resource %s is not a valid sound\n",
                              src_.get_url());

        /* count our reference to the cache object as long as we keep it */
        if (sound_ != 0)
            sound_->add_ref();
    }

    /* interrupt current sound in our layer if appropriate */
    if (interrupt_)
    {
        /* cancel the current sound in this layer */
        formatter->cancel_sound(layer_);

        /*
         *   Obsolete all prior sound tags in the same layer, since
         *   they'll never need to be played again.  
         */
        obsolete_prev_sounds(layer_);

        /*
         *   We don't need to do the interrupt again in the future, so
         *   turn the interrupt flag off. This is important for background
         *   and ambient sounds, since reformatting this tag in the future
         *   could cancel this sound itself or a sound after it. 
         */
        interrupt_ = FALSE;
    }

    /*
     *   If this is in the AMBIENT layer, and it has a non-zero RANDOM
     *   value, and no REPEAT value is specified, assume REPEAT=LOOP 
     */
    if (layer_ == HTML_Attrib_ambient && random_ != 0 && !has_repeat_)
        repeat_ = 0;

    /* tell the formatter to play the sound */
    formatter->play_sound(this, layer_, sound_, repeat_, random_, sequence_);

    /*
     *   If this is a foreground sound, make it obsolete now.  Foreground
     *   sounds are played in response to a change in game state;
     *   reformatting the current text in the future will not cause the
     *   game state change, hence it's not necessary to replay a
     *   foreground sound.  
     */
    if (layer_ == HTML_Attrib_foreground)
        make_obsolete();
}

/*
 *   Obsolete all prior sounds in a given layer 
 */
void CHtmlTagSOUND::obsolete_prev_sounds(HTML_Attrib_id_t layer)
{
    CHtmlTagSOUND *cur;
    CHtmlTagSOUND *prv;

    /* 
     *   start out with the sound just before me, and work back to the
     *   start of the list 
     */
    for (cur = this->prv_sound_ ; cur != 0 ; cur = prv)
    {
        /* remember the previous sound, in case we forget about it below */
        prv = cur->prv_sound_;
        
        /* 
         *   if we're obsoleting all layers, or if this tag is in the
         *   given layer, the tag is now obsolete 
         */
        if (layer == HTML_Attrib_invalid || layer == cur->layer_)
        {
            /* this tag is now obsolete */
            cur->make_obsolete();
        }

        /*
         *   If we're cancelling every layer, we'll end up obsoleting
         *   everything prior to me, so there will be no point in doing so
         *   again in the future.  Hence, we can forget about the previous
         *   sound immediately, to truncate any future list scans (which
         *   will keep things from slowing down as we add sounds).  
         */
        if (layer == HTML_Attrib_invalid)
            cur->prv_sound_ = 0;
    }
}

/*
 *   Mark this sound as obsolete 
 */
void CHtmlTagSOUND::make_obsolete()
{
    /* remember that I'm obsolete */
    obsolete_ = TRUE;

    /* I don't need to keep my cache object around any more */
    if (sound_ != 0)
    {
        sound_->remove_ref();
        sound_ = 0;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Special <?T2> tag 
 */

/*
 *   active on formatting 
 */
void CHtmlTagQT2::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* tell the formatter we're in T3 mode */
    formatter->set_t3_mode(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Special <?T3> tag 
 */

/*
 *   active on formatting
 */
void CHtmlTagQT3::format(CHtmlSysWin *win, CHtmlFormatter *formatter)
{
    /* tell the formatter we're in T3 mode */
    formatter->set_t3_mode(TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debug routines 
 */
#ifdef TADSHTML_DEBUG

#include <stdarg.h>
#include <stdio.h>

void CHtmlTag::debug_indent(int indent)
{
    char  buf[128];
    char *p;

    for (indent *= 3, p = buf ; indent && p - buf + 1 < sizeof(buf) ;
         --indent, *p++ = ' ');
    *p = '\0';
    oshtml_dbg_printf(buf);
}

void CHtmlTag::debug_printf(const char *msg, ...) const
{
    va_list argptr;

    va_start(argptr, msg);
    oshtml_dbg_vprintf(msg, argptr);
    va_end(argptr);
}

void CHtmlTagText::debug_dump(int indent, class CHtmlTextArray *arr)
{
    debug_dump_name(indent);
    if (len_)
        debug_printf("[%.*s]\n", len_, arr->get_text(txtofs_));
    else
        debug_printf("[]\n");
    debug_dump_next(indent, arr);
}

#endif /* TADSHTML_DEBUG */

