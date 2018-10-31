/* $Header: d:/cvsroot/tads/html/htmltags.h,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmltags.h - tag classes
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#ifndef HTMLTAGS_H
#define HTMLTAGS_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLURL_H
#include "htmlurl.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Error codes for setting attributes (CHtmlTag::set_attribute)
 */
enum HTML_attrerr
{
    HTML_attrerr_ok,                                             /* success */
    HTML_attrerr_inv_attr,           /* attribute is not valid for this tag */
    HTML_attrerr_inv_type,          /* value is not of the appropriate type */
    HTML_attrerr_inv_enum,           /* value doesn't match allowed choices */
    HTML_attrerr_out_of_range,                     /* value is out of range */
    HTML_attrerr_too_many_coords,                   /* too many coordinates */
    HTML_attrerr_odd_coords,        /* odd number of coordinates in polygon */
    HTML_attrerr_missing_delim                         /* missing delimiter */
};


/* ------------------------------------------------------------------------ */
/*
 *   Basic tag class
 */
class CHtmlTag
{
public:
    CHtmlTag() { nxt_ = 0; container_ = 0; }
    virtual ~CHtmlTag();

    /* 
     *   Process the end-tag.  This is a static function invoked directly
     *   from the parser to begin processing the </xxx> closing tag.  Almost
     *   all tags are handled the same way here, but a few tag classes need
     *   to "override" this; for example, the </BODY> tag isn't a real
     *   container and simply ignores closing tags, so it overrides this to
     *   do nothing at all.
     *   
     *   Note that this is a static, but we can still override it as though
     *   it were virtual, because the registration table (in htmlreg.h)
     *   automatically associates each tag name with the proper class's
     *   static.  If this isn't overridden, then the class static will simply
     *   "inherit" the base class static; otherwise, the class static will be
     *   the one defined in the specific overriding class.  
     */
    static void process_end_tag(class CHtmlParser *parser,
                                const textchar_t *tag, size_t len);

    /* format this tag */
    virtual void format(class CHtmlSysWin *, class CHtmlFormatter *) { }

    /* 
     *   Determine if we can prune this tag.  Certain types of tags cannot
     *   be pruned because they have global effects.  By default, we'll
     *   return true; any tag that cannot be pruned due to global effects
     *   should return false.  
     */
    virtual int can_prune_tag() const { return TRUE; }

    /*
     *   If this tag can't be pruned (i.e., can_prune_tag() returns
     *   false), this method should try to delete any contents of the tag
     *   that can be safely deleted.  In some cases, a container tag (such
     *   as TITLE) can't be pruned, but the contents of the tag can be
     *   safely removed.  By default, we won't do anything here.
     */
    virtual void try_prune_contents(class CHtmlTextArray *) { }

    /*
     *   Perform any work necessary just before deleting this tag as part
     *   of pruning a tree.  Tags with children should call this method on
     *   all of their children.  Tags with text should delete their text
     *   from the text array.  
     */
    virtual void prune_pre_delete(class CHtmlTextArray *arr) { }

    /*
     *   Set an attribute value.  Each tag must override this routine,
     *   since by default we don't recognize any attributes.  Returns an
     *   HTML_attr_err error code.  
     */
    virtual HTML_attrerr set_attribute(class CHtmlParser *parser,
                                       HTML_Attrib_id_t attr_id,
                                       const textchar_t *val, size_t vallen);
    
    /*
     *   Immediately after the parser finishes scanning the tag's
     *   attribute list, it will create a new instance of the tag object,
     *   then call this method to let the tag take any parse-time action
     *   necessary.  Container start tags, for example, will place
     *   themselves on the parser tag stack.  By default, we'll simply add
     *   this tag to the current container.  
     */
    virtual void on_parse(CHtmlParser *parser);

    /* get my tag name - must be overridden by each subclass */
    virtual const textchar_t *get_tag_name() const = 0;

    /* check if my tag name matches a given name */
    int tag_name_matches(const textchar_t *nm, size_t nmlen) const;

    /* check if two tag names match */
    static int tag_names_match(const textchar_t *nm1, size_t len1,
                               const textchar_t *nm2, size_t len2);

    /*
     *   Determine if a particular tag is allowed while within this
     *   container.  Each time a tag is parsed, the parser will check with
     *   each enclosing container to make sure that a particular tag is
     *   allowed by the container.  By default, this routine returns true
     *   to indicate that the tag is allowed.  Some container tags will
     *   want to override this; for example, the <TITLE> tag does not
     *   allow any markups, and the <PRE> tag does not allow block
     *   markups.  
     */
    virtual int allow_tag(CHtmlTag *) const { return TRUE; }

    /*
     *   certain tags are not allowed in lists - these should ovveride
     *   this and return false 
     */
    virtual int allowed_in_list() const { return TRUE; }

    /*
     *   Get the list nesting level.  Since most elements are not list
     *   containers, this by default returns the list nesting level of my
     *   container, or zero if I have no container. 
     */
    virtual int get_list_nesting_level() const;

    /* tags are arranged into lists */
    CHtmlTag *get_next_tag() const { return nxt_; }

    /* determine if it's okay to proceed into formatting this tag */
    virtual int ready_to_format() const { return TRUE; }

    /* 
     *   set the next tag to the given tag (used by the parser when
     *   unlinking tags from trees) 
     */
    void set_next_tag(CHtmlTag *nxt) { nxt_ = nxt; }

    /* each tag has a container */
    class CHtmlTagContainer *get_container() const { return container_; }

    /* am I a container? */
    virtual int is_container_tag() const { return FALSE; }

    /* add a new item following this tag */
    void append(CHtmlTag *tag) { nxt_ = tag; }

    /* set my container */
    void set_container(class CHtmlTagContainer *container)
    {
        container_ = container;
    }

    /*
     *   Get this item as a table cell.  If it's not a table cell, this
     *   routine returns null. 
     */
    virtual class CHtmlTagTableCell *get_as_table_cell() { return 0; }

    /*
     *   Get this item as a table row.  If it's not a table row, this
     *   routine returns null. 
     */
    virtual class CHtmlTagTR *get_as_table_row() { return 0; }

    /*
     *   Get the next tag in format order.  When formatting, we start
     *   with the top container, then go to the first child of the
     *   container, then to it's first child, and so on; then to the next
     *   sibling of that tag, and so on; then to the next sibling of its
     *   container, then to its first child, and so on.  In effect, we do
     *   a depth-first traversal, but we process the container nodes on
     *   the way down.  
     */
    virtual CHtmlTag *get_next_fmt_tag(class CHtmlFormatter *formatter);

    /*
     *   Re-insert this tag's contents into the text array.  This is used
     *   when a tag's display position is different than its position in
     *   the parsed tag list.  Text must appear in the same order in the
     *   text array that it appears in the display list, so when a tag's
     *   display items are displayed out of order with respect to the tag,
     *   we must use this routine to re-insert the tag's contents at the
     *   proper point.  This is used, for example, to put a table's
     *   caption's text after the table's text when the caption is aligned
     *   below the table.
     *   
     *   The default implementation does nothing, because default tags
     *   have no contents and no text.  
     */
    virtual void reinsert_text_array(class CHtmlTextArray *) { }


    /* -------------------------------------------------------------------- */
    /*
     *   Debugging methods.  These methods are conditionally compiled; if
     *   debugging is turned off at compile time, these methods will not
     *   be defined.  
     */

    HTML_IF_DEBUG(virtual void debug_dump(int indent,
                                          class CHtmlTextArray *arr)
        { debug_dump_name(indent); debug_dump_next(indent, arr); })
    HTML_IF_DEBUG(void debug_dump_name(int indent)
        { debug_indent(indent); debug_printf("%s\n", get_tag_name());
          debug_dump_attrib(indent); })
    HTML_IF_DEBUG(void debug_dump_next(int indent,
                                       class CHtmlTextArray *arr)
        { if (nxt_) nxt_->debug_dump(indent, arr); } )
    HTML_IF_DEBUG(virtual void debug_dump_attrib(int /*indent*/) { })

    HTML_IF_DEBUG(void debug_indent(int indent);)
    HTML_IF_DEBUG(void debug_printf(const char *msg, ...) const;)

protected:
    /*
     *   Most block tags cause a paragraph break.  This service routine
     *   should be called by the on_parse() routine of each tag that
     *   causes a paragraph break just before themselves, and in the
     *   on_close() of container tags that cause a paragraph break after
     *   themselves.  This routine tells the parser to insert a paragraph
     *   break into the text stream, and also has the effect of supplying
     *   an implicit </P> tag if a <P> tag is on the stack.  
     */
    void break_paragraph(CHtmlParser *parser, int isexplicit);

    /* process a CLEAR attribute */
    void format_clear(class CHtmlFormatter *formatter,
                      HTML_Attrib_id_t clear);

    /* service routine to parse a color attribute value */
    HTML_attrerr set_color_attr(class CHtmlParser *parser,
                                HTML_color_t *color_var,
                                const textchar_t *val, size_t vallen);

    /*
     *   Map a color.  If the color is a special parameterized color
     *   values, we'll map it to the corresponding system RGB color here.
     *   If the color is already an RGB color, we'll simply return the
     *   value unchanged.  This routine always returns a fully-resolved
     *   RGB setting.  
     */
    HTML_color_t map_color(class CHtmlSysWin *win, HTML_color_t color);

    /* parse a CLEAR attribute */
    HTML_attrerr set_clear_attr(HTML_Attrib_id_t *clear,
                                class CHtmlParser *parser,
                                const textchar_t *val, size_t vallen);

    /*
     *   Service routines to parse a numeric decimal/hex attribute value
     */
    HTML_attrerr set_number_attr(long *num_var,
                                 const textchar_t *val, size_t vallen);
    HTML_attrerr set_hex_attr(long *num_var,
                              const textchar_t *val, size_t vallen);

    /*
     *   Service routine to parse a numeric value that may be either an
     *   absolute value or a percentage.  If the last character is a
     *   percent sign, it's a percentage, otherwise it's an absolute
     *   value. 
     */
    HTML_attrerr set_number_or_pct_attr(long *num_var, int *is_pct,
                                        const textchar_t *val, size_t vallen);

    /*
     *   Set a coordinate attribute.  If pct is not null, we'll allow a
     *   percentage sign, and return in *pct whether or not a percentage
     *   sign was found.  Advances the string pointer to the next comma.  
     */
    HTML_attrerr set_coord_attr(long *num_var, int *is_pct,
                                const textchar_t **val, size_t *vallen);

    /* find the end of a token in a delimited attribute list */
    const textchar_t *find_attr_token(const textchar_t *val, size_t *vallen,
                                      const textchar_t *delim_chars);

private:
    /* next tag in my list */
    CHtmlTag *nxt_;

    /* my container */
    class CHtmlTagContainer *container_;
};

/*
 *   Each final tag subclass must define a static method that returns a
 *   string giving the name of the tag, and must also define a static
 *   method that returns a new instance of the class.  This macro can be
 *   used to provide these definitions.  This macro also defines the
 *   virtual tag name retrieval function.  
 */
#define HTML_TAG_MAP_NOCONSTRUCTOR(tag_class_name, tag_name_string) \
    static const textchar_t *get_tag_name_st() { return tag_name_string; } \
    const textchar_t *get_tag_name() const { return tag_name_string; } \
    static CHtmlTag *create_tag_instance(class CHtmlParser *prs) \
        { return new tag_class_name(prs); }

#define HTML_TAG_MAP(tag_class_name, tag_name_string) \
    tag_class_name(class CHtmlParser *) { } \
    HTML_TAG_MAP_NOCONSTRUCTOR(tag_class_name, tag_name_string)

/*
 *   Note that each final tag subclass that corresponds to a tag name
 *   must be registered so that the parser can include the tag in its tag
 *   name table.  See htmlreg.h for information.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Container tag class.  This subclass of the basic tag can be used as
 *   the base class for any tag that acts as a container.  
 */
class CHtmlTagContainer: public CHtmlTag
{
public:
    CHtmlTagContainer()
    {
        contents_first_ = 0;
        contents_last_ = 0;
        closed_ = FALSE;
    }
    ~CHtmlTagContainer();

    /* am I a container? */
    virtual int is_container_tag() const { return TRUE; }

    /*
     *   Internal processing to exit the formatting of the container.  We
     *   must undo any formatter state changes (such as margin sizes or font
     *   settings) that we made on entry; by default, we don't do anything,
     *   but container subclasses that modify formatter state on entry must
     *   undo those changes on exit.  
     */
    virtual void format_exit(class CHtmlFormatter *) { }

    /* perform pre-deletion work on all of my children */
    void prune_pre_delete(class CHtmlTextArray *arr);

    /*
     *   A container tag pushes itself onto the container stack upon
     *   being parsed 
     */
    virtual void on_parse(CHtmlParser *parser);

    /*
     *   When we parse a close tag for an open container tag, the parser
     *   will call this routine before actually closing the tag.  This
     *   gives us a chance to do any implicit closing of other tags that
     *   may be nested within this tag.  The default doesn't do anything.  
     */
    virtual void pre_close(CHtmlParser *) { }

    /*
     *   When we reach the end of a container, the parser will call on_close
     *   on the current container to let it know that it's been closed.
     *   Note that we don't need to pop the container off the parser stack,
     *   as the parser will always do this automatically.  Note also that
     *   this is called while we're still the active container.  
     */
    virtual void on_close(CHtmlParser *)
    {
        /* note that the tag is closed */
        closed_ = TRUE;
    }

    /* 
     *   receive notification that we've just finished closing the tag; the
     *   immediate container is now re-established as the active container 
     */
    virtual void post_close(CHtmlParser *) { }

    /* determine whether the close tag has been parsed yet */
    int is_closed() const { return closed_; }

    /*
     *   Container tags can have sublists, containing the list of data
     *   and markups within the container.  This returns the head of the
     *   sublist.  
     */
    CHtmlTag *get_contents() const { return contents_first_; }

    /* get the lats item in my sublist */
    CHtmlTag *get_last_contents() const { return contents_last_; }

    /*
     *   Set the first child (this is used by the parser when unlinking
     *   tags from trees) 
     */
    void set_contents(CHtmlTag *cont) { contents_first_ = cont; }

    /* Add a new item at the end of my sublist */
    void append_to_sublist(CHtmlTag *tag);

    /* Delete all of my contents */
    void delete_contents();

    /*
     *   Get the next tag in format order.  The next tag after formatting
     *   a container is the first child of the container. 
     */
    virtual CHtmlTag *get_next_fmt_tag(class CHtmlFormatter *formatter);

    /* re-insert my contents into the text array */
    virtual void reinsert_text_array(class CHtmlTextArray *arr);

    /*
     *   If this item is a list container, return this item, otherwise
     *   return the enclosing container that is a list container.  Returns
     *   null if there is no enclosing list container. 
     */
    virtual class CHtmlTagListContainer *get_list_container()
    {
        /* 
         *   default items are not list containers; return my container
         *   cast to a list container, or null if I have no container
         */
        if (get_container() == 0)
            return 0;
        else
            return get_container()->get_list_container();
    }

    /*
     *   If this item is a quote tag, return this item, otherwise return
     *   the enclosing container that is a quote item.  Returns null if
     *   there is no enclosing quote container.  
     */
    virtual class CHtmlTagQ *get_quote_container()
    {
        if (get_container() == 0)
            return 0;
        else
            return get_container()->get_quote_container();
    }

    /*
     *   If this item is a table/row/cell container, return this item,
     *   otehrwise return the enclosing container that is a table/row
     *   container.  Returns null if there is no enclosing table/row
     *   container.  
     */
    virtual class CHtmlTagTABLE *get_table_container()
    {
        if (get_container() == 0)
            return 0;
        else
            return get_container()->get_table_container();
    }
    virtual class CHtmlTagTR *get_table_row_container()
    {
        if (get_container() == 0)
            return 0;
        else
            return get_container()->get_table_row_container();
    }
    virtual class CHtmlTagTableCell *get_table_cell_container()
    {
        if (get_container() == 0)
            return 0;
        else
            return get_container()->get_table_cell_container();
    }

    HTML_IF_DEBUG(virtual void debug_dump(int indent,
                                          class CHtmlTextArray *arr)
        { debug_dump_name(indent);
          if (contents_first_) contents_first_->debug_dump(indent + 1, arr);
          debug_dump_next(indent, arr); })

protected:
    /* head of sublist of contents of this container */
    CHtmlTag *contents_first_;
    CHtmlTag *contents_last_;

    /* flag indicating that the close tag has been encountered */
    int closed_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Special "relax" tag - this tag doesn't do anything, but is sometimes
 *   useful to pad the list. 
 */
class CHtmlTagRelax: public CHtmlTag
{
public:
    /* this class does not correspond to a named tag */
    const textchar_t *get_tag_name() const { return "<!-- Relax -->"; }

    /* we have no effect on formatting */
    void format(class CHtmlSysWin *, class CHtmlFormatter *) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Special outermost container class.  This is the container that wraps
 *   the entire document. 
 */
class CHtmlTagOuter: public CHtmlTagContainer
{
public:
    /* this class does not correspond to a named tag */
    const textchar_t *get_tag_name() const
    {
        return "<!-- Outer Container -->";
    }

    /* format the tag */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
};

/* ------------------------------------------------------------------------ */
/*
 *   Special text tag class.  This tag is used to wrap a block of text. 
 */
class CHtmlTagText: public CHtmlTag
{
public:
    CHtmlTagText(class CHtmlTextArray *arr, class CHtmlTextBuffer *buf);
    CHtmlTagText(class CHtmlTextArray *arr, const textchar_t *buf,
                 size_t buflen);
    ~CHtmlTagText();

    /* delete my text from the text array when pruning the tree */
    void prune_pre_delete(class CHtmlTextArray *arr);

    const textchar_t *get_tag_name() const { return "<!-- Text -->"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    HTML_IF_DEBUG(virtual void debug_dump(int indent,
                                          class CHtmlTextArray *arr);)

    /* get this item's text offset */
    unsigned long get_text_ofs() const { return txtofs_; }

    /* re-insert my text into the text array */
    virtual void reinsert_text_array(class CHtmlTextArray *arr);

protected:
    CHtmlTagText() { }

    /* memory containing the text */
    unsigned long txtofs_;
    size_t len_;
};


/*
 *   Text tag subclass for preformatted text 
 */
class CHtmlTagTextPre: public CHtmlTagText
{
public:
    CHtmlTagTextPre(int tab_size, class CHtmlTextArray *arr,
                    class CHtmlTextBuffer *buf);
    CHtmlTagTextPre(int tab_size, class CHtmlTextArray *arr,
                    const textchar_t *buf, size_t buflen);
    const textchar_t *get_tag_name() const
        { return "<!-- Preformatted Text -->"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* initialize the text buffer - used by the constructors */
    void init_text(int tab_size, class CHtmlTextArray *arr,
                   const textchar_t *buf, size_t buflen);
};


/*
 *   Text tag subclass for command line input.  This class behaves
 *   largely like a normal text tag, except for some special behavior.
 *   First, it can be edited through the UI.  Second, the text in the
 *   buffer isn't stored in the text array, but rather in a buffer managed
 *   by the UI that's handling the input; the UI makes changes to its
 *   buffer, and notifies the formatter of changes so that the formatter
 *   can update the display.  Once the command is completed, it can be
 *   committed, which will store it in the text array as usual; after
 *   being committed, the command can no longer be edited, and behaves
 *   like a normal text tag.  
 */
class CHtmlTagTextInput: public CHtmlTagText
{
public:
    CHtmlTagTextInput(class CHtmlTextArray *arr, const textchar_t *buf,
                      size_t len);

    /* format the tag */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* commit the command to the text array */
    void commit(class CHtmlTextArray *arr);

    /*
     *   Update the length of the text in the buffer.  Since the UI
     *   manages the buffer, it must keep us apprised of changes to the
     *   buffer's length with this function. 
     */
    void setlen(class CHtmlFormatterInput *fmt, size_t len);

    /* re-insert my text into the text array */
    virtual void reinsert_text_array(class CHtmlTextArray *arr);

    /* change the underlying UI buffer */
    void change_buf(CHtmlFormatterInput *fmt, const textchar_t *buf);

private:
    /* the text pointer, when it's still in the UI's buffer */
    const textchar_t *buf_;
};

/*
 *   Text tag subclass for debugger source file lines.  In addition to the
 *   normal information we store for a text item, we store the seek
 *   location in the source file of the line; this information is used to
 *   associate the line in memory with the debug information in the .GAM
 *   file.
 *   
 *   This class is used only for the debugger.  
 */
class CHtmlTagTextDebugsrc: public CHtmlTagTextPre
{
public:
    CHtmlTagTextDebugsrc(unsigned long linenum, int tab_size,
                         class CHtmlTextArray *arr,
                         const textchar_t *buf, size_t buflen);
    const textchar_t *get_tag_name() const
        { return "<!-- tdb source text -->"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    unsigned long linenum_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Special tag for a soft hyphen.  This indicates a point at which we can
 *   optionally break the line (to fit the margins) by adding a hyphen.
 */
class CHtmlTagSoftHyphen: public CHtmlTagText
{
public:
    CHtmlTagSoftHyphen(class CHtmlTextArray *arr)
        : CHtmlTagText(arr, "", 0) { }
    const textchar_t *get_tag_name() const { return "&shy;"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

protected:
};

/* ------------------------------------------------------------------------ */
/*
 *   Special tag for non-breaking spaces.  
 */
class CHtmlTagNBSP: public CHtmlTagText
{
public:
    CHtmlTagNBSP(class CHtmlTextArray *arr, const textchar_t *buf)
        : CHtmlTagText(arr, buf, get_strlen(buf))
    {
        /* remember my width in spaces */
        wid_ = get_strlen(buf);
    }
    const textchar_t *get_tag_name() const { return "&nbsp;"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

protected:
    /* our width, in ordinary space widths */
    int wid_;
};

/*
 *   Special width values for spaces whose sizes depend on font attributes
 *   other than the em size.  We normally represent space widths as a number
 *   of spaces per em (i.e., as the denominator in the fraction of an em).
 *   Some space widths are not tied to the em size, though; for these, we
 *   define some manifest constants that are absurdly outside the range that
 *   we need for any of the pre-defined fractional-em spaces.  
 */
#define HTMLTAG_SPWID_ORDINARY  10000   /* width of an ordinary space (' ') */
#define HTMLTAG_SPWID_FIGURE    10001             /* width of the digit '0' */
#define HTMLTAG_SPWID_PUNCT     10002            /* width of a period ('.') */

/*
 *   Special tag for explicitly-sized spaces. 
 */
class CHtmlTagSpace: public CHtmlTagText
{
public:
    /*
     *   Construct.  Note that we store a single space in the text array as
     *   our plain text representation, unless we're a zero-width space, in
     *   which case we store nothing at all in the text array.  
     */
    CHtmlTagSpace(class CHtmlTextArray *arr, int wid)
        : CHtmlTagText(arr, wid != 0 ? " " : "", wid != 0 ? 1 : 0)
    {
        /* remember my width */
        wid_ = wid;
    }
    const textchar_t *get_tag_name() const
        { return "<!-- special space -->"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

protected:
    /* 
     *   our width, expressed as the denominator of the fractional em (or as
     *   one of the special HTMLTAG_SPWID_xxx widths, or as zero for a
     *   zero-width space) 
     */
    int wid_;
};


/* ------------------------------------------------------------------------ */
/*
 *   BODY tag 
 */

/* default color settings */
const HTML_color_t HTML_bgcolor_default = 0xffffff;                /* white */
const HTML_color_t HTML_textcolor_default = 0x000000;              /* black */

class CHtmlTagBODY: public CHtmlTag
{
public:
    CHtmlTagBODY(class CHtmlParser *)
    {
        use_bgcolor_ = FALSE;
        use_textcolor_ = FALSE;
        use_link_color_ = FALSE;
        use_vlink_color_ = FALSE;
        use_alink_color_ = FALSE;
        use_hlink_color_ = FALSE;
        bg_image_ = 0;
    }

    ~CHtmlTagBODY();

    /* process the end tag */
    static void process_end_tag(class CHtmlParser *,
                                const textchar_t *, size_t)
    {
        /*
         *   Unlike in standard HTML, our <BODY> tags aren't containers, so
         *   we don't need or want an ending </BODY> tag.  However, since
         *   people familiar with HTML often expect <BODY> to require a
         *   closing </BODY> tag, allow it by simply ignoring it.  
         */
    }

    /* BODY tags can't be pruned due to global effects */
    virtual int can_prune_tag() const { return FALSE; }

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagBODY, "BODY");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* URL for background image */
    class CHtmlUrl background_;

    /* the image cache object for the background image */
    class CHtmlResCacheObject *bg_image_;

    /* background color - ignored if use_bgcolor_ is false */
    int use_bgcolor_ : 1;
    HTML_color_t bgcolor_;

    /* text color - ignored if use_textcolor_ is false */
    int use_textcolor_ : 1;
    HTML_color_t textcolor_;

    /* 
     *   link, vlink, alink, and hlink colors - the colors are ignored if the
     *   corresponding use_xxx_'s are false 
     */
    int use_link_color_ : 1;
    HTML_color_t link_color_;

    int use_alink_color_ : 1;
    HTML_color_t alink_color_;

    int use_vlink_color_ : 1;
    HTML_color_t vlink_color_;

    int use_hlink_color_ : 1;
    HTML_color_t hlink_color_;
};

/* ------------------------------------------------------------------------ */
/*
 *   TITLE tag 
 */
class CHtmlTagTITLE: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagTITLE, "TITLE");

    /* TITLE tags can't be pruned due to global effects */
    virtual int can_prune_tag() const { return FALSE; }

    /* 
     *   prune the contents - we can always delete the contents of a
     *   TITLE, even though we can't delete the TITLE itself 
     */
    virtual void try_prune_contents(class CHtmlTextArray *txtarr);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    virtual void format_exit(class CHtmlFormatter *formatter);

    /* 
     *   set my title text - the formatter calls this when we close our
     *   tag to inform us of the text of the title 
     */
    void set_title_text(const textchar_t *txt)
    {
        /* 
         *   Record the title only if it's non-empty. If the caller is
         *   attempting to set an empty title string, it's probably
         *   because we've been pruned; in this case, keep our original
         *   title string.  Note that, in the case we actually have an
         *   empty title string, this will be harmless, because we'll
         *   never record a non-empty string internally in the first place
         *   in this case.  
         */
        if (txt != 0 && get_strlen(txt) != 0)
            title_.set(txt);
    }

private:
    /* the text of our title */
    CStringBuf title_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Block-level container tag.  The block-level tags all have some common
 *   attributes, such as CLEAR and NOWRAP.  
 */
class CHtmlTagBlock: public CHtmlTagContainer
{
public:
    CHtmlTagBlock()
    {
        /* set default attributes */
        clear_ = HTML_Attrib_invalid;
        nowrap_ = FALSE;
    }

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

protected:
    /* CLEAR setting - LEFT, RIGHT, or ALL, if anything is set */
    HTML_Attrib_id_t clear_;

    /* NOWRAP specified */
    int nowrap_ : 1;

    /* old line wrap setting */
    int old_wrap_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Paragraph tag.  The paragraph tag is not a container, but simply a
 *   marker in the text stream.  
 */

class CHtmlTagP: public CHtmlTag
{
public:
    CHtmlTagP(CHtmlParser *prs) { init(TRUE, prs); }
    CHtmlTagP(int isexplicit, CHtmlParser *prs) { init(isexplicit, prs); }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagP, "P");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void on_parse(class CHtmlParser *parser);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* process the end tag */
    static void process_end_tag(class CHtmlParser *parser,
                                const textchar_t *, size_t);

private:
    /* initialize */
    void init(int isexplicit, CHtmlParser *prs);
    
    /* alignment setting */
    HTML_Attrib_id_t  align_;

    /*
     *   flag indicating whether this paragraph break was inserted by an
     *   explicit <P> tag in the source text, or implicitly by another tag
     *   that causes a paragraph to end 
     */
    int explicit_ : 1;

    /* NOWRAP setting - if true, we won't wrap within the paragraph */
    int nowrap_ : 1;

    /* CLEAR setting - LEFT, RIGHT, or ALL, if anything is set */
    HTML_Attrib_id_t clear_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Special internal hilite-on and hilite-off tags.  These are
 *   non-container font attribute tags; they act essentially like <B> and
 *   </B>, but are simply in-line tags that don't contain any other tags:
 *   they establish the new font settings until the next similar tag. 
 */
class CHtmlTagHILITE: public CHtmlTag
{
public:
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

protected:
    /* 
     *   set my font attribute in the given font descriptor; returns true if
     *   anything changed about the font 
     */
    virtual int set_attr(class CHtmlFontDesc *desc) = 0;
};

class CHtmlTagHILITEON: public CHtmlTagHILITE
{
public:
    HTML_TAG_MAP(CHtmlTagHILITEON, "HILITEON");

protected:
    virtual int set_attr(class CHtmlFontDesc *desc);
};

class CHtmlTagHILITEOFF: public CHtmlTagHILITE
{
public:
    HTML_TAG_MAP(CHtmlTagHILITEOFF, "HILITEOFF");

protected:
    virtual int set_attr(class CHtmlFontDesc *desc);
};


/* ------------------------------------------------------------------------ */
/*
 *   Division tags 
 */

/*
 *   basic DIV-style tag - shared by DIV and CENTER
 */
class CHtmlTagDIV_base: public CHtmlTagBlock
{
public:
    /* do a paragraph break on opening and closing a heading */
    void on_parse(CHtmlParser *parser);
    void on_close(CHtmlParser *parser);

    /* do general DIV-style formatting with a given alignment type */
    void div_format(class CHtmlFormatter *formatter,
                    HTML_Attrib_id_t alignment);

    /* leave the division - restore old alignment settings */
    void format_exit(class CHtmlFormatter *formatter);

private:
    /* alignment in effect immediately before this division started */
    HTML_Attrib_id_t old_align_;
};


/*
 *   DIV 
 */
class CHtmlTagDIV: public CHtmlTagDIV_base
{
public:
    CHtmlTagDIV(CHtmlParser *)
    {
        align_ = HTML_Attrib_invalid;
        start_txtofs_ = 0;
        end_txtofs_ = 0;
        hover_link_ = FALSE;
        use_hover_fg_ = use_hover_fg_ = hover_underline_ = FALSE;
    }

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagDIV, "DIV");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void on_parse(CHtmlParser *parser);
    void on_close(CHtmlParser *parser);
    void post_close(class CHtmlParser *parser);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);
    void prune_pre_delete(class CHtmlTextArray *arr);

private:
    /* alignment setting */
    HTML_Attrib_id_t  align_;

    /* range of text offsets that this DIV covers */
    unsigned long start_txtofs_;
    unsigned long end_txtofs_;

    /* is there a HOVER=LINK attribute? */
    int hover_link_ : 1;

    /* HOVER=LINK(fgcolor,bgcolor,decoration) values */
    HTML_color_t hover_fg_;
    HTML_color_t hover_bg_;
    int use_hover_fg_ : 1;
    int use_hover_bg_ : 1;
    int hover_underline_ : 1;
};


/*
 *   CENTER 
 */
class CHtmlTagCENTER: public CHtmlTagDIV_base
{
public:
    HTML_TAG_MAP(CHtmlTagCENTER, "CENTER");

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter)
    {
        /* do division formatting with center alignment */
        div_format(formatter, HTML_Attrib_center);

        /* inherit default */
        CHtmlTagDIV_base::format(win, formatter);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Font container add-in.  This class can be multiply inherited by a
 *   tag class to provide font container attributes.  Each final subclass
 *   that inherits this must implement add_attrs() to add the appropriate
 *   attributes for the font. 
 */
class CHtmlFontContAddin
{
public:
    virtual ~CHtmlFontContAddin() {}

protected:
    void fontcont_init() { old_font_ = 0; }

    /*
     *   Do font formatting entry and exit.  The subclass's format() and
     *   format_exit() routines, respectively, should call these in the
     *   course of its processing.  
     */
    void fontcont_format(class CHtmlSysWin *win,
                         class CHtmlFormatter *formatter);
    void fontcont_format_exit(class CHtmlFormatter *formatter);
    
    /*
     *   Add my attributes to a font description.  Returns true if the
     *   description was changed, false if not.  
     */
    virtual int add_attrs(class CHtmlFormatter *,
                          class CHtmlFontDesc *) = 0;

    /* font in effect before we added our attributes */
    class CHtmlSysFont *old_font_;
};

/* ------------------------------------------------------------------------ */

class CHtmlTagADDRESS: public CHtmlTagBlock, public CHtmlFontContAddin
{
public:
    HTML_TAG_MAP(CHtmlTagADDRESS, "ADDRESS");

    /* address tags are not allowed in lists */
    int allowed_in_list() const { return FALSE; }

    /* do a paragraph break on opening and closing a heading */
    void on_parse(CHtmlParser *parser);
    void on_close(CHtmlParser *parser);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter)
        { fontcont_format(win, formatter); }
    void format_exit(class CHtmlFormatter *formatter)
        { fontcont_format_exit(formatter); }

    /* add font attributes */
    int add_attrs(class CHtmlFormatter *fmt,
                  class CHtmlFontDesc *desc);
};

class CHtmlTagBLOCKQUOTE: public CHtmlTagBlock
{
public:
    HTML_TAG_MAP(CHtmlTagBLOCKQUOTE, "BLOCKQUOTE");

    int allowed_in_list() const { return FALSE; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

private:
    /* spacing before and after quoted area - calculated in format() */
    int break_ht_;
};

/*
 *   BQ - HTML 3.0 synonym for BLOCKQUOTE 
 */
class CHtmlTagBQ: public CHtmlTagBLOCKQUOTE
{
public:
    CHtmlTagBQ(class CHtmlParser *prs) : CHtmlTagBLOCKQUOTE(prs) { }

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagBQ, "BQ");
};


/* ------------------------------------------------------------------------ */
/*
 *   Basic heading tag class 
 */
class CHtmlTagHeading: public CHtmlTagBlock, public CHtmlFontContAddin
{
public:
    CHtmlTagHeading()
    {
        fontcont_init();
        align_ = HTML_Attrib_invalid;
    }

    /* heading tags are not allowed in lists */
    int allowed_in_list() const { return FALSE; }

    /* do a paragraph break on opening and closing a heading */
    void on_parse(CHtmlParser *parser);
    void on_close(CHtmlParser *parser);

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format and exit formatting for these fonts */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* add font attributes */
    int add_attrs(class CHtmlFormatter *fmt,
                  class CHtmlFontDesc *desc);

    /*
     *   Get my heading size.  All of the heading types behave the same
     *   way, except that they each have a distinctive size.  This routine
     *   must be overridden by each heading subclass to return the
     *   appropriate HTML size (1-7) for the heading.  
     */
    virtual int get_heading_fontsize() = 0;

private:
    /*
     *   size of vertical whitespace around heading - set during
     *   format(), used again during format_exit() 
     */
    int break_ht_;

    /* alignment */
    HTML_Attrib_id_t align_;
};

class CHtmlTagH1: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH1, "H1");

    int get_heading_fontsize() { return 6; }
};

class CHtmlTagH2: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH2, "H2");

    int get_heading_fontsize() { return 5; }
};

class CHtmlTagH3: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH3, "H3");

    int get_heading_fontsize() { return 4; }
};

class CHtmlTagH4: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH4, "H4");

    int get_heading_fontsize() { return 3; }
};

class CHtmlTagH5: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH5, "H5");

    int get_heading_fontsize() { return 2; }
};

class CHtmlTagH6: public CHtmlTagHeading
{
public:
    HTML_TAG_MAP(CHtmlTagH6, "H6");

    int get_heading_fontsize() { return 1; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Preformatted text tags.  All of these tags cause entry into verbatim
 *   mode, in which whitespace and line breaks in the source text are
 *   obeyed.  
 */

class CHtmlTagVerbatim: public CHtmlTagContainer, public CHtmlFontContAddin
{
public:
    CHtmlTagVerbatim() { fontcont_init(); }

    void on_parse(CHtmlParser *parser);
    void on_close(CHtmlParser *parser);

    /* do we translate tags within this type of preformatted block? */
    virtual int translate_tags() const = 0;

    /* format and exit formatting for these fonts */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* add font attributes */
    virtual int add_attrs(class CHtmlFormatter *fmt,
                          class CHtmlFontDesc *desc);

    /* get the font size decrement for this listing type */
    virtual int get_font_size_decrement() const { return 1; }

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);
private:
    /*
     *   size of vertical whitespace above and below - calculated during
     *   format(), and used again during format_exit()
     */
    int break_ht_;

    /*
     *   WIDTH attribute setting; we ignore this, but read it anyway, for
     *   compatibility with other renderers and in case we might want to
     *   use it in the future 
     */
    long width_;
};

class CHtmlTagPRE: public CHtmlTagVerbatim
{
public:
    HTML_TAG_MAP(CHtmlTagPRE, "PRE");

    /* we do translate tags within PRE blocks */
    int translate_tags() const { return TRUE; }
};

class CHtmlTagLISTING: public CHtmlTagVerbatim
{
public:
    HTML_TAG_MAP(CHtmlTagLISTING, "LISTING");

    /* we do not translate tags within LISTING blocks */
    int translate_tags() const { return FALSE; }

    /* use a smaller font size than the default listing font */
    int get_font_size_decrement() const { return 2; }
};

class CHtmlTagXMP: public CHtmlTagVerbatim
{
public:
    HTML_TAG_MAP(CHtmlTagXMP, "XMP");

    /* we do not translate tags within XMP blocks */
    int translate_tags() const { return FALSE; }
};


/* ------------------------------------------------------------------------ */
/*
 *   List elements 
 */

class CHtmlTagListContainer: public CHtmlTagBlock, public CHtmlFontContAddin
{
public:
    CHtmlTagListContainer()
    {
        compact_ = FALSE;
        list_level_ = 0;
        list_started_ = FALSE;
        fontcont_init();
    }

    /* I'm a list container */
    CHtmlTagListContainer *get_list_container() { return this; }

    /* return the name of the list item tag making up this list */
    virtual const textchar_t *get_list_tag_name() const { return "LI"; }

    /* pre-close the tag - close any open list item before closing the list */
    void pre_close(CHtmlParser *parser);

    /* certain tags are not allowed within lists */
    int allow_tag(CHtmlTag *tag) const { return tag->allowed_in_list(); }

    /* get my list nesting level */
    int get_list_nesting_level() const { return list_level_; }

    /* 
     *   determine if the list is indented overall; by default, lists are
     *   indented overall, but some list subtypes (such as definition
     *   lists) indent some items but not the overall list 
     */
    virtual int is_list_indented() const { return TRUE; }

    /* get the amount to indent the list */
    virtual long get_left_indent(class CHtmlSysWin *win,
                                 class CHtmlFormatter *formatter) const;

    /* set the bullet for an item */
    virtual void set_item_bullet(class CHtmlTagLI *,
                                 class CHtmlFormatter *) { }

    /* add the display item for the bullet for a list item */
    virtual void add_bullet_disp(class CHtmlSysWin *, class CHtmlFormatter *,
                                 class CHtmlTagLI *, long /*bullet_width*/)
        { }

    /* parse the list tag */
    void on_parse(CHtmlParser *parser);

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the tag */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* 
     *   begin a list item - each list item should call this before
     *   formatting itself 
     */
    void begin_list_item(class CHtmlSysWin *win,
                         class CHtmlFormatter *formatter);

    /* add font container attributes */
    int add_attrs(class CHtmlFormatter *, class CHtmlFontDesc *);

private:
    /* list nesting level */
    int list_level_;

    /* compact flag */
    int compact_ : 1;

    /* line spacing - set in format(), used in format_exit() */
    int break_ht_;

    /* flag: first list item encountered */
    int list_started_ : 1;
};

class CHtmlTagUL: public CHtmlTagListContainer
{
public:
    CHtmlTagUL(class CHtmlParser *)
    {
        /* use default bullet type */
        type_ = HTML_Attrib_invalid;
    }

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagUL, "UL");

    /* set the bullet for an item */
    void set_item_bullet(class CHtmlTagLI *item,
                         class CHtmlFormatter *formatter);

    /* add the bullet display item */
    void add_bullet_disp(class CHtmlSysWin *win,
                         class CHtmlFormatter *formatter,
                         class CHtmlTagLI *item, long bullet_width);

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the item */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* type of bullet to use - disc, square, circle */
    HTML_Attrib_id_t type_;

    /* source for bullet when given as an image */
    CHtmlUrl src_;
};

class CHtmlTagOL: public CHtmlTagListContainer
{
public:
    CHtmlTagOL(class CHtmlParser *)
    {
        /* use default bullet type */
        style_ = '\0';

        /* start at number 1 unless otherwise specified */
        start_ = 1;

        /* 
         *   presume we'll start a new list, rather than continue
         *   numbering from previous ordered list 
         */
        continue_ = FALSE;
    }

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagOL, "OL");

    /* set the bullet for an item */
    void set_item_bullet(class CHtmlTagLI *item,
                         class CHtmlFormatter *formatter);

    /* add the bullet display item */
    void add_bullet_disp(class CHtmlSysWin *win,
                         class CHtmlFormatter *formatter,
                         class CHtmlTagLI *item, long bullet_width);

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the item */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* style specifier - '1', 'a', 'A', 'i', 'I' */
    textchar_t style_;

    /* starting number */
    long start_;

    /* current value */
    long cur_value_;

    /* flag: continue numbering from end of previous list */
    int continue_ : 1;
};

class CHtmlTagDL: public CHtmlTagListContainer
{
public:
    HTML_TAG_MAP(CHtmlTagDL, "DL");

    /* definition lists are made up of DD items */
    const textchar_t *get_list_tag_name() const { return "DD"; }

    /* do not indent the overall list for a definition list */
    int is_list_indented() const { return FALSE; }
};

/*
 *   DIR - treat this as a synonym for UL 
 */
class CHtmlTagDIR: public CHtmlTagUL
{
public:
    CHtmlTagDIR(class CHtmlParser *parser) : CHtmlTagUL(parser) { }
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagDIR, "DIR");
};

/*
 *   MENU - treat this as a synonym for UL 
 */
class CHtmlTagMENU: public CHtmlTagUL
{
public:
    CHtmlTagMENU(class CHtmlParser *parser) : CHtmlTagUL(parser) { }
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagMENU, "MENU");
};

class CHtmlTagLH: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagLH, "LH");
};

class CHtmlTagLI: public CHtmlTagContainer
{
public:
    CHtmlTagLI(class CHtmlParser *)
    {
        /* no value has been set yet, so use the default */
        value_set_ = FALSE;

        /* presume we'll inherit bullet type from the list */
        type_ = HTML_Attrib_invalid;

        /* presume we'll inherit numbering style from the list */
        style_ = '\0';

        /* no image yet */
        image_ = 0;
    }

    ~CHtmlTagLI();
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagLI, "LI");

    /* parse the item */
    void on_parse(CHtmlParser *parser);

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the item */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* 
     *   set the default bullet - if a bullet has already been specified,
     *   leave the specified bullet as it was 
     */
    void set_default_bullet(HTML_Attrib_id_t type,
                            const CHtmlUrl *url)
    {
        /* if we don't have a type set, use the default type for the list */
        if (type_ == HTML_Attrib_invalid)
            type_ = type;

        /* if we don't have a source image, use the default for the list */
        if (src_.get_url() == 0 && url != 0 && url->get_url() != 0)
            src_.set_url(url);
    }

    /*
     *   set the default value - if a value has already been specified,
     *   leave the value as it was 
     */
    void set_default_value(long value)
    {
        if (!value_set_)
        {
            value_ = value;
            value_set_ = TRUE;
        }
    }

    /* set the default number display style */
    void set_default_number_style(textchar_t style)
    {
        if (style_ == '\0')
            style_ = style;
    }

    /* 
     *   add a numeric list header display item, using my current value
     *   and display style 
     */
    void add_listhdr_number(class CHtmlSysWin *win,
                            class CHtmlFormatter *formatter,
                            long bullet_width);

    /* 
     *   add a bullet list header display item, using the bullet type
     *   previously established for this item 
     */
    void add_listhdr_bullet(class CHtmlSysWin *win,
                            class CHtmlFormatter *formatter,
                            long bullet_width);

    /* get my value */
    long get_value() const { return value_; }

private:
    /* current list item value, and flag indicating it's been set */
    long value_;
    int value_set_ : 1;

    /* list item style */
    textchar_t style_;

    /* bullet type, if specified */
    HTML_Attrib_id_t type_;

    /* source for image for use as bullet */
    CHtmlUrl src_;

    /* image cache object for bullet image */
    class CHtmlResCacheObject *image_;
};

class CHtmlTagDT: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagDT, "DT");

    /* parse this item */
    void on_parse(CHtmlParser *parser);

    /* format it */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
};

class CHtmlTagDD: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagDD, "DD");

    /* parse this item */
    void on_parse(CHtmlParser *parser);

    /* format it */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);
};


/* ------------------------------------------------------------------------ */
/*
 *   phrase markup 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Font containers.  These items all set a font attribute on entry, and
 *   remove the font attribute on exit. 
 */
class CHtmlTagFontCont: public CHtmlTagContainer, public CHtmlFontContAddin
{
public:
    CHtmlTagFontCont() { fontcont_init(); }
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter)
        { fontcont_format(win, formatter); }
    void format_exit(class CHtmlFormatter *formatter)
        { fontcont_format_exit(formatter); }
};

/*
 *   Character set tag - this is a special font tag that the parser
 *   generates when it needs to switch to a new character set.  This
 *   otherwise leaves the font unchanged. 
 */
class CHtmlTagCharset: public CHtmlTagFontCont
{
public:
    CHtmlTagCharset(oshtml_charset_id_t charset)
    {
        charset_ = charset;
    }

    const textchar_t *get_tag_name() const { return "<!-- Charset -->"; }
    
    /* add font attributes */
    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);

private:
    /* character set identifier */
    oshtml_charset_id_t charset_;
};

/*
 *   CREDIT - HTML 3.0 
 */
class CHtmlTagCREDIT: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagCREDIT, "CREDIT");

    /* delete my text when pruning the tree */
    void prune_pre_delete(class CHtmlTextArray *arr);

    CHtmlTagCREDIT(class CHtmlParser *parser);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* add font attributes */
    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);

private:
    unsigned long txtofs_;
};


class CHtmlTagCITE: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagCITE, "CITE");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagCODE: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagCODE, "CODE");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagEM: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagEM, "EM");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagKBD: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagKBD, "KBD");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagSAMP: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagSAMP, "SAMP");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagSTRONG: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagSTRONG, "STRONG");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagBIG: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagBIG, "BIG");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagSMALL: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagSMALL, "SMALL");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagDFN: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagDFN, "DFN");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagVAR: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagVAR, "VAR");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagB: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagB, "B");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagI: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagI, "I");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagU: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagU, "U");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

class CHtmlTagSTRIKE: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagSTRIKE, "STRIKE");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

/* S is synonymous with STRIKE */
class CHtmlTagS: public CHtmlTagSTRIKE
{
public:
    CHtmlTagS(CHtmlParser *prs) : CHtmlTagSTRIKE(prs) { }
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagS, "S");
};

class CHtmlTagTT: public CHtmlTagFontCont
{
public:
    HTML_TAG_MAP(CHtmlTagTT, "TT");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

/*
 *   SUP - superscript - subclass this from SMALL, since we'll use the
 *   same font as with SMALL, but set it a little differently
 */
class CHtmlTagSUP: public CHtmlTagSMALL
{
public:
    CHtmlTagSUP(class CHtmlParser *parser) : CHtmlTagSMALL(parser) { }
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagSUP, "SUP");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};

/*
 *   SUB - subscript - subclass this from SMALL 
 */
class CHtmlTagSUB: public CHtmlTagSMALL
{
public:
    CHtmlTagSUB(class CHtmlParser *parser) : CHtmlTagSMALL(parser) { }
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagSUB, "SUB");

    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);
};


/*
 *   FONT tag 
 */
class CHtmlTagFONT: public CHtmlTagFontCont
{
public:
    CHtmlTagFONT(class CHtmlParser *)
    {
        /* by default, leave the size alone */
        relative_ = '+';
        size_ = 0;

        /* by default, do not change the color */
        use_color_ = FALSE;
        use_bgcolor_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagFONT, "FONT");

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* add my font attributes to a font descriptor */
    int add_attrs(class CHtmlFormatter *fmt, class CHtmlFontDesc *desc);

private:
    /* font size */
    long size_;

    /*
     *   relative setting - if this is zero, the size is absolute; if
     *   it's '+', the size is a positive relative setting; if it's '-',
     *   the size is a negative relative setting 
     */
    textchar_t relative_;

    /* face name list */
    CStringBuf face_;

    /* color - if use_color_ is false, color_ is ignored */
    int use_color_ : 1;
    HTML_color_t color_;

    /* background color */
    int use_bgcolor_ : 1;
    HTML_color_t bgcolor_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Explicit font tag. 
 */
class CHtmlTagFontExplicit: public CHtmlTagContainer
{
public:
    CHtmlTagFontExplicit(const class CHtmlFontDesc *font)
    {
        font_ = *font;
    }

    const textchar_t *get_tag_name() const
        { return "<!-- Explicit Font -->"; }

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

protected:
    /* our font */
    class CHtmlFontDesc font_;

    /* original font in effect in enclosing scope */
    class CHtmlSysFont *old_font_;
};


/* ------------------------------------------------------------------------ */
/*
 *   NOBR - non-breaking text.  This encloses a run of pre-formatted text
 *   where no automatic line breaking is allowed.  
 */
class CHtmlTagNOBR: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagNOBR, "NOBR");

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

private:
    /* the enclosing break/nobreak status */
    int old_wrap_;
};


/* ------------------------------------------------------------------------ */
/*
 *   WRAP - control the text-wrapping mode.  This is an in-line
 *   non-container tag that lets the document switch between word-wrapping
 *   mode and character-wrapping mode.  In word-wrapping mode, we break
 *   lines only at defined word boundaries (at spaces, after hyphens, at
 *   soft hyphens).  In character-wrapping mode, we can break lines anywhere
 *   except at explicit non-breaking boundaries (as marked by the zero-width
 *   non-breaking space character, \uFEFF).  
 */
class CHtmlTagWRAP: public CHtmlTag
{
public:
    CHtmlTagWRAP(CHtmlParser *)
    {
        /* assume word-wrap mode */
        char_wrap_mode_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagWRAP, "WRAP");

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format this tag */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* our wrapping mode */
    int char_wrap_mode_;
};


/* ------------------------------------------------------------------------ */
/*
 *   NOLOG - text hidden from log files.  This encloses a run of text that we
 *   want displayed but not copied to any transcript file.  Since we handle
 *   only regular display streams, this tag has no effect; we simply parse it
 *   and otherwise ignore it.  
 */
class CHtmlTagNOLOG: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagNOLOG, "NOLOG");
};

/*
 *   LOG - text hidden *except* in log files.  This encloses a run of text
 *   that we want displayed only in a transcript file, not in the UI.  Since
 *   we handle only regular display streams, this tag effectively hides its
 *   contents; we simply parse it, but skip its contents during formatting. 
 */
class CHtmlTagLOG: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagLOG, "LOG");

    /* 
     *   get the next tag in format order - since we hide our contents, this
     *   is simply the next tag after us, not our first child 
     */
    virtual CHtmlTag *get_next_fmt_tag(class CHtmlFormatter *formatter)
    {
        return CHtmlTag::get_next_fmt_tag(formatter);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   TAB markup
 */
class CHtmlTagTAB: public CHtmlTag
{
public:
    CHtmlTagTAB(CHtmlParser *);
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagTAB, "TAB");

    /* delete my text on deleting the tag */
    void prune_pre_delete(class CHtmlTextArray *arr);

    /* parse the tag */
    void on_parse(class CHtmlParser *parser);

    /* parse an attribute */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* set the MULTIPLE attribute to the specified value */
    void set_multiple_val(int val)
    {
        indent_ = val;
        use_multiple_ = TRUE;
    }

    /* format the tab */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* name of the tab */
    CStringBuf id_;

    /* true -> define my ID */
    int define_ : 1;

    /* true -> tab to my ID */
    int to_ : 1;

    /* alignment */
    HTML_Attrib_id_t align_;

    /* alignment character (decimal point) */
    textchar_t dp_;

    /* 
     *   indent amount, and flag specifying if it's to be used as direct
     *   spacing (use_indent_) or indentation to a multiple of a given tab
     *   size (use_multiple_) 
     */
    long indent_;
    int use_indent_ : 1;
    int use_multiple_ : 1;

    /* text offset of our space character */
    unsigned long txtofs_;
};


/* ------------------------------------------------------------------------ */

class CHtmlTagA: public CHtmlTagContainer, public CHtmlFontContAddin
{
public:
    CHtmlTagA(class CHtmlParser *prs)
    {
        /* presume we'll render with the normal link style */
        style_ = LINK_STYLE_NORMAL;
        noenter_ = FALSE;
        append_ = FALSE;
        use_hover_fg_ = use_hover_bg_ = FALSE;
        hover_underline_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagA, "A");

    /* don't allow <A> tags to be nested */
    int allow_tag(CHtmlTag *tag) const
    {
        /* allow anything but a nested <A> */
        return !tag->tag_name_matches("A", 1);
    }

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* add font-cont attributes */
    virtual int add_attrs(class CHtmlFormatter *,
                          class CHtmlFontDesc *);

private:
    /* link information */
    CHtmlUrl href_;
    CStringBuf title_;

    /* HOVER foreground and background colors */
    HTML_color_t hover_fg_;
    HTML_color_t hover_bg_;

    /* link style (LINK_STYLE_xxx) */
    unsigned int style_ : 2;

    /* true -> do not automatically enter the command */
    int noenter_ : 1;

    /* 
     *   true -> append the href to any existing command line, rather than
     *   clearing the previous command 
     */
    int append_ : 1;

    /* true -> use hover foreground/background colors */
    int use_hover_fg_ : 1;
    int use_hover_bg_ : 1;

    /* 
     *   Hover decoration: true->underline, false->none.  Note that this
     *   specifies an *added* decoration - so if this is true, we add an
     *   underline, but if it's false, we don't remove an underline if we'd
     *   otherwise use one.  
     */
    int hover_underline_ : 1;
};

class CHtmlTagQ: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagQ, "Q");

    /* delete my quote text when pruning the tree */
    void prune_pre_delete(class CHtmlTextArray *arr);

    void on_parse(class CHtmlParser *parser);
    void on_close(class CHtmlParser *parser);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* I'm a quote container */
    CHtmlTagQ *get_quote_container() { return this; }

    /* get my quote nesting level */
    int get_quote_nesting_level()
    {
        CHtmlTagQ *qcont;

        /* get my enclosing quote container */
        qcont = (get_container() == 0
                 ? 0 : get_container()->get_quote_container());

        /* I'm one level deeper than it is */
        return (qcont == 0 ? 0 : qcont->get_quote_nesting_level() + 1);
    }

private:
    /* offset in text array of open and close quotes */
    unsigned long open_ofs_;
    unsigned long close_ofs_;

    /* characters to use for open and close quotes */
    textchar_t open_q_;
    textchar_t close_q_;
};

class CHtmlTagBR: public CHtmlTag
{
public:
    CHtmlTagBR(class CHtmlParser *prs);
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagBR, "BR");

    void on_parse(class CHtmlParser *parser);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

private:
    /* CLEAR setting - LEFT, RIGHT, or ALL, if anything is set */
    HTML_Attrib_id_t clear_;

    /* height of the break */
    long height_;
};

class CHtmlTagHR: public CHtmlTag
{
public:
    CHtmlTagHR(class CHtmlParser *)
    {
        align_ = HTML_Attrib_center;
        shade_ = TRUE;
        size_ = 0;
        width_ = 100;
        width_pct_ = TRUE;
        image_ = 0;
    }

    ~CHtmlTagHR();
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagHR, "HR");

    void on_parse(CHtmlParser *parser);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

private:
    /* alignment */
    HTML_Attrib_id_t align_;

    /* shading on or off */
    int shade_ : 1;

    /* vertical size in pixels */
    long size_;

    /* width as a percentage or as a pixel size (depending on width_pct_) */
    long width_;

    /* flag: true -> width is given as a percentage; false -> pixel size */
    int width_pct_;

    /* image source, if an image is used */
    CHtmlUrl src_;

    /* the image cache object */
    class CHtmlResCacheObject *image_;
};

/* ------------------------------------------------------------------------ */
/*
 *   IMAGE tag 
 */
class CHtmlTagIMG: public CHtmlTag
{
public:
    CHtmlTagIMG(class CHtmlParser *)
    {
        /* initialize all attributes to default values */
        image_ = 0;
        h_image_ = 0;
        a_image_ = 0;
        border_ = 1;
        ismap_ = FALSE;
        align_ = HTML_Attrib_middle;
        width_ = height_ = 0;
        width_set_ = height_set_ = FALSE;
        hspace_ = vspace_ = 0;
        alt_txtofs_ = 0;
    }

    ~CHtmlTagIMG();

    /* on pruning the tree, delete my ALT text */
    void prune_pre_delete(class CHtmlTextArray *arr);

    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagIMG, "IMG");

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the image */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* load one of our image resources */
    void load_image(class CHtmlSysWin *win, class CHtmlFormatter *formatter,
                    class CHtmlResCacheObject **image,
                    const CHtmlUrl *url, const char *attr_name,
                    int mandatory);

    /* the image cache object for the main image */
    class CHtmlResCacheObject *image_;

    /* for hyperlinked images, the hover/active images */
    class CHtmlResCacheObject *h_image_;
    class CHtmlResCacheObject *a_image_;
    
    /* image source */
    CHtmlUrl src_;

    /* for hyperlinked images, the hover/active sources */
    CHtmlUrl asrc_;
    CHtmlUrl hsrc_;

    /* alternate textual description */
    CStringBuf alt_;

    /* text offset of alt_ text */
    unsigned long alt_txtofs_;

    /* border size */
    long border_;

    /* horizontal/vertical whitespace settings */
    long hspace_;
    long vspace_;

    /* width and height settings */
    long width_;
    long height_;

    /* flags indicating whether width and height were explicitly set */
    int width_set_ : 1;
    int height_set_ : 1;

    /* map setting */
    CHtmlUrl usemap_;

    /* ISMAP setting */
    int ismap_ : 1;

    /* alignment */
    HTML_Attrib_id_t align_;
};

/* ------------------------------------------------------------------------ */
/*
 *   SOUND tag 
 */

class CHtmlTagSOUND: public CHtmlTag
{
public:
    CHtmlTagSOUND(class CHtmlParser *)
    {
        sound_ = 0;
        layer_ = HTML_Attrib_invalid;
        repeat_ = 1;
        has_repeat_ = FALSE;
        fadein_ = fadeout_ = 0;
        random_ = 0;
        cancel_ = FALSE;
        interrupt_ = FALSE;
        sequence_ = HTML_Attrib_invalid;
        obsolete_ = FALSE;
        prv_sound_ = 0;
        gen_id_ = 0;
    }

    ~CHtmlTagSOUND();
        
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagSOUND, "SOUND");
    
    /* don't allow pruning SOUND tags except when obsolete */
    virtual int can_prune_tag() const { return obsolete_; }

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* format the sound */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

    /* get/set the sound generation ID */
    unsigned int get_gen_id() const { return gen_id_; }
    void set_gen_id(unsigned int gen_id) { gen_id_ = gen_id; }

private:
    /*
     *   Obsolete all previous sounds in the given layer (but not this
     *   sound itself).  This is called for all sounds prior to a CANCEL
     *   tag in a given layer.  
     */
    void obsolete_prev_sounds(HTML_Attrib_id_t layer);

    /* mark this sound as obsolete */
    void make_obsolete();
    
    /* the resource cache object */
    class CHtmlResCacheObject *sound_;

    /* source */
    CHtmlUrl src_;

    /* layer (foreground, bgambient, ambient, background) */
    HTML_Attrib_id_t layer_;

    /* alternate textual description */
    CStringBuf alt_;

    /* number of times to repeat -- zero indicates looping */
    long repeat_;

    /* flag indicating that REPEAT was specified */
    int has_repeat_ : 1;

    /* time in seconds to fade in/out */
    long fadein_;
    long fadeout_;

    /* 
     *   probability of starting at any given second; zero indicates that
     *   the RANDOM attribute is not present 
     */
    long random_;

    /* cancellation - true means that we're cancelling sounds in a layer */
    int cancel_ : 1;

    /* 
     *   interrupt - true means we start immediately, stopping any sound
     *   currently playing in the same layer 
     */
    int interrupt_ : 1;

    /* sequence code (replace, random, cycle) */
    HTML_Attrib_id_t sequence_;

    /* 
     *   Obsolete flag.  Once a sound has been removed from its queue by a
     *   CANCEL, it's unnecessary to ever play that sound again, since
     *   there's a tag that follows that will cancel the sound.  So, we'll
     *   set this flag whenever we cancel a sound from its queue, and
     *   we'll ignore any sounds in the playback stream that have this
     *   flag set. 
     */
    int obsolete_ : 1;

    /*
     *   Previous sound tag in format list.  We use this to keep a chain
     *   of the sound tags in reverse order.  Each time we encounter a
     *   CANCEL tag, we can go back and obsolete all of the tags in the
     *   chain prior to the CANCEL tag, since they'll never need to be
     *   played again.  
     */
    CHtmlTagSOUND *prv_sound_;

    /*
     *   Sound generation ID.  Each time we play a sound, the formatter
     *   marks it with its current generation ID.  If we've played a sound
     *   before on the current generation, it means that we're just
     *   reformatting the current page and should leave the current sounds
     *   playing, so we won't play the same sound again. 
     */
    unsigned int gen_id_;
};

/* ------------------------------------------------------------------------ */
/*
 *   MAP and subtags 
 */

class CHtmlTagMAP: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP(CHtmlTagMAP, "MAP");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

private:
    CStringBuf name_;
};

class CHtmlTagAREA: public CHtmlTag
{
public:
    CHtmlTagAREA(CHtmlParser *)
    {
        /* no shape yet */
        shape_ = HTML_Attrib_invalid;
        
        /* no coordinates yet */
        coord_cnt_ = 0;

        /* NOHREF not yet seen */
        nohref_ = FALSE;

        /* APPEND, NOENTER not yet seen */
        append_ = FALSE;
        noenter_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagAREA, "AREA");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
private:
    /* shape - rect, circle, poly */
    HTML_Attrib_id_t shape_;

    /* 
     *   Coordinate list - interpretation depends on the type of shape,
     *   but we'll always store a flat list of coordinate values. 
     */
    CHtmlTagAREA_coords_t coords_[CHtmlTagAREA_max_coords];

    /* number of coordinates in use */
    size_t coord_cnt_;

    /* href of this map setting */
    CHtmlUrl href_;

    /* flag indicating that NOHREF is specified */
    int nohref_ : 1;

    /* alternate name */
    CStringBuf alt_;

    /* 
     *   APPEND attribute setting - appends the HREF to the command rather
     *   than clearing out the old command 
     */
    int append_ : 1;

    /* 
     *   NOENTER - allows the player to continue editing the command after
     *   adding our HREF 
     */
    int noenter_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   BASEFONT 
 */

class CHtmlTagBASEFONT: public CHtmlTag
{
public:
    CHtmlTagBASEFONT(class CHtmlParser *)
    {
        size_ = 3;
        use_color_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagBASEFONT, "BASEFONT");

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);

private:
    /* SIZE setting */
    long size_;

    /* face name list */
    CStringBuf face_;

    /* color - if use_color_ is false, color_ is ignored */
    int use_color_ : 1;
    HTML_color_t color_;
};

/* ------------------------------------------------------------------------ */
/*
 *   BANNER tag 
 */
class CHtmlTagBANNER: public CHtmlTagContainer
{
public:
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagBANNER, "BANNER");

    CHtmlTagBANNER(class CHtmlParser *)
    {
        /* initialize attributes with defaults */
        remove_ = FALSE;
        removeall_ = FALSE;
        height_set_ = width_set_ = FALSE;
        height_pct_ = width_pct_ = FALSE;
        height_prev_ = width_prev_ = FALSE;
        border_ = FALSE;
        alignment_ = HTML_BANNERWIN_POS_TOP;
        obsolete_ = FALSE;

        /* if ID isn't specified, use an empty name as the default banner */
        id_.set("");

        /* nothing in the banner sublist yet */
        banner_first_ = 0;

    }

    ~CHtmlTagBANNER();
        
    /* pre-delete on pruning the tree */
    void prune_pre_delete(class CHtmlTextArray *arr);

    /* only allow pruning BANNER tags when they're obsolete */
    virtual int can_prune_tag() const { return obsolete_; }

    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    void on_parse(class CHtmlParser *parser);
    void on_close(class CHtmlParser *parser);

    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* 
     *   We can't format a banner until it's finished.  (However, note
     *   that a 'remove' element has no contents, so it's always ready to
     *   format.) 
     */
    virtual int ready_to_format() const { return remove_ || is_closed(); }

    /*
     *   Get the first tag in the banner contents.  During formatting of
     *   the banner, the banner's normal contents list is empty, so the
     *   contents of the banner must instead be obtained with this special
     *   call. 
     */
    class CHtmlTag *get_banner_contents() const { return banner_first_; }

    /*
     *   Receive notification that the banner is now obsolete.  The
     *   formatter will call this routine whenever it finds that a
     *   subsequent BANNER tag in the same format list uses the same
     *   banner -- when this happens, the banner involved will always be
     *   overwritten by the subsequent tag.  We'll discard our contents
     *   and skip doing any work to format the banner on future passes
     *   through the format list. 
     */
    void notify_obsolete(class CHtmlTextArray *arr);

    /*
     *   Special flag that identifies this tag as an about box rather than
     *   a regular banner.  Since about boxes and banners work almost the
     *   same way, we share most of the code for the two classes, and use
     *   this flag to let the formatter know which is which.  
     */
    virtual int is_about_box() const { return FALSE; }

    /*
     *   Determine if the banner has a border 
     */
    int has_border() const { return border_; }

    /*
     *   Get the alignment setting
     */
    HTML_BannerWin_Pos_t get_alignment() const { return alignment_; }

protected:
    /* banner identifier */
    CStringBuf id_;

    /* 
     *   Height - if not set, we'll use the height of the contents.  If
     *   height_pct_ is set, it indicates that the height is a percentage
     *   of the overall window height. 
     */
    long height_;
    int height_set_ : 1;
    int height_pct_;

    /* likewise with width */
    long width_;
    int width_set_ : 1;
    int width_pct_;

    /* flag: keep height/width of previous instance of this banner */
    int height_prev_ : 1;
    int width_prev_ : 1;

    /* REMOVE flag - set when REMOVE attribute is present */
    int remove_ : 1;

    /* REMOVEALL flag - set when REMOVEALL attribute is present */
    int removeall_ : 1;

    /* 
     *   obsolete flag - we'll set this immediately after we apply any
     *   REMOVE tag, because doing so will remove previous banners not
     *   only from the display but also from the formatting list, hence we
     *   only need to apply one of these a single time 
     */
    int obsolete_ : 1;

    /* BORDER flag */
    int border_ : 1;

    /* alignment setting */
    HTML_BannerWin_Pos_t alignment_;

    /* 
     *   banner contents -- we move the normal contents list into this
     *   special banner contents list when we're first formatted; this
     *   prevents the banner contents from being formatted into my own
     *   display list, but still lets us get at the sublist in the banner
     *   subformatter 
     */
    CHtmlTag *banner_first_;
};

/* ------------------------------------------------------------------------ */
/*
 *   The ABOUTBOX tag is a simple subclass of BANNER.  It works almost the
 *   same as BANNER, but has a special flag to the formatter that lets the
 *   formatter know this is actually an about box.  
 */
class CHtmlTagABOUTBOX: public CHtmlTagBANNER
{
public:
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagABOUTBOX, "ABOUTBOX");

    CHtmlTagABOUTBOX(class CHtmlParser *parser)
        : CHtmlTagBANNER(parser)
    {
    }

    /* set an attribute - we suppress certain BANNER attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* this is the ABOUTBOX tag */
    virtual int is_about_box() const { return TRUE; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Tables 
 */

/*
 *   Table column cell list entry 
 */
class CHtmlTag_cell
{
public:
    CHtmlTag_cell()
    {
        /* the cell is not yet occupied */
        occupied_ = FALSE;
    }

    /* determine if the cell is occupied */
    int is_occupied() const { return occupied_; }

    /* mark the cell as occupied */
    void set_occupied() { occupied_ = TRUE; }

private:
    /* flag indicating whether the cell has been occupied yet */
    int occupied_ : 1;
};

/*
 *   Table column list entry 
 */
class CHtmlTag_column
{
public:
    CHtmlTag_column();
    ~CHtmlTag_column();

    /*
     *   Get the pointer to a cell at a given row (row numbers start at
     *   zero).  If the cell isn't yet allocated, this automatically
     *   allocates the cell, so this routine always returns a valid cell
     *   pointer.  
     */
    CHtmlTag_cell *get_cell(size_t rownum);

    /*
     *   Add a cell's widths to my measurements.  This is used during pass
     *   1 to determine the sizes needed for the column (and, in turn, for
     *   the enclosing table).  The minimum width is the narrowest that we
     *   can make the column before we start clipping; in other words,
     *   it's the width of the largest single item that can't be broken
     *   across lines.  The maximum width is the width of the longest
     *   line; this is the narrowest that we can make the table before we
     *   need to start wrapping lines.  If either of the parameters to
     *   this function are greater than the respective values we've
     *   stored, we'll update the stored values with the new values.  
     */
    void add_cell_widths(long min_width, long max_width)
    {
        if (min_width > min_width_)
            min_width_ = min_width;
        if (max_width > max_width_)
            max_width_ = max_width;
    }

    /*
     *   Add a cell's explicit pixel width to my measurements.  We'll
     *   accumulate the minimum explicit pixel width that each column wants,
     *   and then apply this if it exceeds the width we'd otherwise choose
     *   for the column.  
     */
    void add_cell_pix_width(long pix_width)
    {
        if (pix_width > pix_width_)
            pix_width_ = pix_width;
    }

    /*
     *   Add a cell's percentage-based width to my measurements.  We'll
     *   accumulate the minimum percentage width that each column wants,
     *   and then apply this if it exceeds the width we'd otherwise choose
     *   for the column.  
     */
    void add_cell_pct_width(long pct_width)
    {
        if (pct_width > pct_width_)
            pct_width_ = pct_width;
    }

    /* get the minimum and maximum sizes for the column */
    long get_min_width() const { return min_width_; }
    long get_max_width() const { return max_width_; }

    /* get the percentage width for the column */
    long get_pct_width() const { return pct_width_; }

    /* get the explicitly set pixel width for the column */
    long get_pix_width() const { return pix_width_; }

    /* 
     *   Set the final width of the column.  This is called when the table
     *   is formatted during pass 2, after all of the contents have been
     *   measured.
     */
    void set_width(long wid) { width_ = wid; }

    /* get the final width of the column */
    long get_width() const { return width_; }

    /* 
     *   Get/set the horizontal offset of the column.  This is the offset
     *   from the left edge of the table, and is simply the offset of the
     *   previous column plus the width of the previous column.  
     */
    long get_x_offset() const { return x_offset_; }
    void set_x_offset(long x_offset) { x_offset_ = x_offset; }
    

private:
    /* cell list - array of cells in the column */
    CHtmlTag_cell **cells_;

    /* number of cells used in the list */
    size_t cells_used_;

    /* number of slots for cells allocated in list */
    size_t cells_alloced_;

    /*
     *   Minimum width of the column: this is the width of the largest
     *   item that can't be broken across multiple lines.  This is the
     *   narrowest we can go before we start clipping items within the
     *   column. 
     */
    long min_width_;

    /* 
     *   Maximum width needed for the column: this is the width of the
     *   longest line in the column.  This is the narrowest that we can go
     *   before we start wrapping lines within the column.
     */
    long max_width_;

    /*
     *   Pixel width.  When a cell's WIDTH attribute is given as a pixel
     *   width, we'll track it here.  We record the highest pixel width of
     *   the cells in this column.  
     */
    long pix_width_;

    /*
     *   Percentage width.  When a cell's width is expressed as a
     *   percentage of the table width, we'll track it here.  We record
     *   the highest of the percentage widths of the cells in this column.
     */
    long pct_width_;

    /* 
     *   final width of the column - set during pass 2, after all contents
     *   are known 
     */
    long width_;

    /* horizontal offset of the column from the left edge of the table */
    long x_offset_;
};

/*
 *   Mix-in class for table background image management.  
 */
class CHtmlTableImage
{
public:
    CHtmlTableImage()
    {
        /* no background color setting yet */
        use_bgcolor_ = FALSE;

        /* no background image yet */
        bg_image_ = 0;
    }
    virtual ~CHtmlTableImage();

    /* load the background image during formatting as needed */
    void format_load_bg(class CHtmlSysWin *win, class CHtmlFormatter *fmt);

protected:
    virtual const textchar_t *get_tag_name() const = 0;

    /* our background color */
    HTML_color_t bgcolor_;
    int use_bgcolor_ : 1;

    /* the URL and image cache object for our background image */
    class CHtmlUrl background_;
    class CHtmlResCacheObject *bg_image_;
};

/*
 *   TABLE tag 
 */
class CHtmlTagTABLE: public CHtmlTagContainer, public CHtmlTableImage
{
public:
    CHtmlTagTABLE(class CHtmlParser *);
    ~CHtmlTagTABLE();
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagTABLE, "TABLE");

    /* put paragraph breaks around the table */
    void on_parse(class CHtmlParser *parser);
    void on_close(class CHtmlParser *parser);
    void post_close(class CHtmlParser *parser);

    /* before closing the table, close any open cell and row */
    void pre_close(class CHtmlParser *parser);

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* I'm a TABLE tag */
    CHtmlTagTABLE *get_table_container() { return this; }

    /* get my display item */
    class CHtmlDispTable *get_disp() const { return disp_; }

    /* is this a margin-floating item? */
    int is_floating() const
    {
        /* it's a floating item if it's left- or right-aligned */
        return (align_ == HTML_Attrib_left
                || align_ == HTML_Attrib_right);
    }

    /* get the alignment attribute */
    HTML_Attrib_id_t get_align() const { return align_; }

    /* get/set my current row number */
    size_t get_cur_rownum() const { return cur_rownum_; }
    void set_cur_rownum(size_t rownum) { cur_rownum_ = rownum; }

    /* format the table */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* get the number of columns */
    size_t get_column_count() const { return cols_used_; }
    
    /*
     *   Get the column at a particular column index.  Allocates a new
     *   column entry if this column is not yet allocated, so this routine
     *   always returns a valid cell pointer. 
     */
    CHtmlTag_column *get_column(size_t colnum);

    /*
     *   Get a cell at a particular row and column position. 
     */
    CHtmlTag_cell *get_cell(size_t rownum, size_t colnum)
    {
        return get_column(colnum)->get_cell(rownum);
    }

    /*
     *   Calculate the table width based on the size constraints of the
     *   columns.  width_min and width_max are the minimum and maximum sizes
     *   calculated for the contents, and win_width is the available space in
     *   the window.  
     */
    long calc_table_width(long width_min, long width_max, long win_width);

    /* 
     *   calculate the width of the table decorations (borders, cell
     *   spacing, cell padding) 
     */
    long calc_decor_width();

    /*
     *   Set column widths.
     *   
     *   x_offset is the x position of the left edge of the table.
     *   table_width is the desired width of the table, which includes all
     *   decorations (borders, cell spacing, cell padding). 
     */
    void set_column_widths(long x_offset, long table_width);

    /*
     *   Set row positions.  This distributes the vertical space in the
     *   table according to the row sizes.  Each row's size depends on the
     *   sizes of its member cells; each row's position depends on the
     *   sizes of the rows above it.  When the table is finished after
     *   pass 2, everything about the cells is determined except the
     *   vertical sizes of the rows; this routine finishes the job by
     *   determining row positions and setting everything's final vertical
     *   location. 
     */
    void set_row_positions(class CHtmlFormatter *formatter);

    /*
     *   Compute the final height of the table.  We'll go through our rows
     *   and compute their final heights. 
     */
    void compute_table_height();

    /* get the cell padding and spacing settings */
    long get_cellpadding() const { return cellpadding_; }
    long get_cellspacing() const { return cellspacing_; }

    /* get the border size */
    long get_border() const { return border_; }

    /* 
     *   Set the CAPTION tag.  A table may have one CAPTION tag, or no
     *   caption at all.  The CAPTION tag tells us about itself when the
     *   parser finishes with it.  
     */
    void set_caption_tag(class CHtmlTagCAPTION *caption_tag)
    {
        caption_tag_ = caption_tag;
    }

    /* get my enclosing table */
    CHtmlTagTABLE *get_enclosing_table() const { return enclosing_table_; }

private:
    /* my display item */
    class CHtmlDispTable *disp_;
    
    /* alignment */
    HTML_Attrib_id_t align_;

    /* 
     *   width; if width_pct_ is true, width_ is a percentage of the space
     *   between the margins, otherwise it's a pixel size 
     */
    long width_;
    int width_pct_;

    /* 
     *   flag indicating if width has been set at all; if not, it defaults
     *   to the minimum size needed to show the table with all cells at
     *   maximum, but no more than the greater of 100% of the available
     *   space between the margins and the minimum table width 
     */
    int width_set_ : 1;

    /* height; works like width */
    long height_;
    int height_pct_;

    /* flag indicating whether height has been specified */
    int height_set_ : 1;

    /* 
     *   calculated minimum height - we calculate this based on the height
     *   parameter value when we start formatting the table, and then use
     *   it when we know all of the row heights to set the minimum table
     *   height 
     */
    long calc_height_;

    /* border size in pixels */
    long border_;

    /* cell spacing and padding, in pixels */
    long cellspacing_;
    long cellpadding_;

    /* the enclosing table display item and position */
    class CHtmlTagTABLE *enclosing_table_;
    CHtmlPoint enclosing_table_pos_;

    /* my CAPTION tag, if I have one */
    class CHtmlTagCAPTION *caption_tag_;

    /* 
     *   Current row number.  This starts at zero; each time we finish a
     *   row, we'll increase this. 
     */
    size_t cur_rownum_;

    /*
     *   Column list.  The table keeps an array of column entries; each
     *   column entry keeps track of the size metrics for the column, and
     *   keeps a list of the cells (by row) that are actually occupied in
     *   that column.  
     */
    CHtmlTag_column **columns_;

    /* number of columns used in column list */
    size_t cols_used_;

    /* number of column slots allocated in column list */
    size_t cols_alloced_;

    /*
     *   Minimum and maximum widths.  These aren't known until after we
     *   complete the first pass.  The minimum width is the size of the
     *   largest single item that can't be broken across lines; the
     *   maximum width is the width of the longest single line. 
     */
    long min_width_;
    long max_width_;
};

class CHtmlTagCAPTION: public CHtmlTagContainer
{
public:
    CHtmlTagCAPTION(class CHtmlParser *)
    {
        /* no alignment specified yet */
        valign_ = HTML_Attrib_invalid;
        align_ = HTML_Attrib_invalid;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagCAPTION, "CAPTION");

    /*
     *   Get the next tag in format order.  We don't format CAPTION tags
     *   at the point they're encountered in the tag list; instead, the
     *   table formats its caption at the appropriate point in laying out
     *   the table.  Hence, when asked for the next tag after the CAPTION
     *   in format order, we pretend we have no contents. 
     */
    virtual CHtmlTag *get_next_fmt_tag(class CHtmlFormatter *formatter)
    {
        /* return the next tag after us, as though we had no contents */
        return CHtmlTag::get_next_fmt_tag(formatter);
    }

    /* parse the tag */
    void on_parse(CHtmlParser *parser);

    /* close parsing of the tag */
    void on_close(class CHtmlParser *parser);

    /* after-close processing */
    void post_close(class CHtmlParser *parser);

    /*
     *   Format the caption's contents.  Normal formatting will skip the
     *   caption's contents; at the appropriate time, the formatter will
     *   explicitly call this routine to format the caption. 
     */
    void format_caption(class CHtmlSysWin *win,
                        class CHtmlFormatter *formatter);

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /*
     *   Determine if the caption is at the top of the table.  Returns
     *   true if the caption is at the top, false if the caption is at the
     *   bottom. 
     */
    int is_at_top_of_table() const
    {
        /* it's at the top if it hasn't been set explicitly to the bottom */
        return (valign_ != HTML_Attrib_bottom);
    }

    /*
     *   Receive notification that we've finished parsing the table.  If
     *   we're aligned at the bottom of the table, we'll need to re-insert
     *   our text into the text array here, since the display items for
     *   our contents will be in the display list after the table. 
     */
    void on_close_table(class CHtmlParser *parser);

private:
    /* horizontal and vertical alignment */
    HTML_Attrib_id_t align_;
    HTML_Attrib_id_t valign_;
};

class CHtmlTagTR: public CHtmlTagContainer, public CHtmlTableImage
{
public:
    CHtmlTagTR(class CHtmlParser *)
        : CHtmlTagContainer(), CHtmlTableImage()
    {
        /* no horizontal alignment set */
        align_ = HTML_Attrib_invalid;

        /* default to middle vertical alignment */
        valign_ = HTML_Attrib_middle;

        /* no cells yet, so the maximum height is zero */
        max_cell_height_ = 0;

        /* no height set yet */
        height_set_ = FALSE;
        height_star_ = FALSE;
    }
    
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagTR, "TR");

    /* 
     *   on opening and closing parsing of tag, implicitly close any open
     *   cell within this row
     */
    void on_parse(class CHtmlParser *parser);
    void pre_close(class CHtmlParser *parser);

    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* I'm a table row (TR) tag */
    CHtmlTagTR *get_as_table_row() { return this; }
    CHtmlTagTR *get_table_row_container() { return this; }

    /* 
     *   Get my row number.  The row number is fixed for all cells in the
     *   row; this is determined by the table when we start this row.  
     */
    size_t get_rownum() const { return rownum_; }

    /*
     *   Get/set the current column number.  We start this at zero when we
     *   start the row; each cell sets the column number to the next
     *   available column when it determines where it goes. 
     */
    size_t get_cur_colnum() const { return cur_colnum_; }
    void set_cur_colnum(size_t colnum) { cur_colnum_ = colnum; }

    /* format the row */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

    /* get the maximum height of any of my cells */
    long get_max_cell_height() const
    {
        /* 
         *   if we have an explicit minimum height, and it's larger than
         *   the maximum cell height, use that; otherwise use the
         *   calculated maximum cell height 
         */
        if (height_set_ && height_ > max_cell_height_)
            return height_;
        else
            return max_cell_height_;
    }

    /* 
     *   Compute the maximum cell height.  Adds the height contribution
     *   from each of the cells in this row to all rows the cell spans.  
     */
    void compute_max_cell_height(size_t rownum, size_t rowcnt, int pass);

    /* 
     *   Include the height of a cell in the computation of the row's
     *   maximum cell height. 
     */
    void include_cell_height(long height)
    {
        /* if this is the largest so far, remember it */
        if (height > max_cell_height_)
            max_cell_height_ = height;
    }

    /*
     *   Set the y offset of items in the row.  We'll use this after we've
     *   completed pass 2 to set the final vertical positions of the
     *   contents of the row.  
     */
    void set_row_y_pos(class CHtmlFormatter *formatter,
                       class CHtmlTagTABLE *table,
                       long y_offset, long height);

    /* get my alignment setting */
    HTML_Attrib_id_t get_align() const { return align_; }

    /* get my background color setting (and whether it's valid) */
    HTML_color_t get_bgcolor() const { return bgcolor_; }
    int get_use_bgcolor() const { return use_bgcolor_; }

    /* 
     *   determine if I my height attribute is '*', which means that I
     *   should soak up any available extra space in the vertical height
     *   of the enclosing table 
     */
    int is_height_star() const { return height_star_; }

private:
    /* horizontal and vertical alignment settings */
    HTML_Attrib_id_t align_;
    HTML_Attrib_id_t valign_;

    /* height attribute */
    long height_;
    unsigned int height_set_ : 1;
    unsigned int height_star_ : 1;

    /* my row number in the table */
    size_t rownum_;

    /* current column number */
    size_t cur_colnum_;

    /* maximum cell height found so far */
    long max_cell_height_;
};

/*
 *   base class for table cells (TD and TH)
 */
class CHtmlTagTableCell: public CHtmlTagContainer, public CHtmlTableImage
{
public:
    CHtmlTagTableCell(class CHtmlParser *parser);

    /* 
     *   get this item as a table cell - I'm definitely a table cell, so
     *   return a pointer to myself 
     */
    class CHtmlTagTableCell *get_table_cell_container() { return this; }
    class CHtmlTagTableCell *get_as_table_cell() { return this; }

    /*
     *   Set the cell's metrics.  The formatter calls this after it's
     *   computed the minimum and maximum sizes of our contents during
     *   pass 1 through our table.
     */
    void set_table_cell_metrics(class CHtmlTableMetrics *metrics);

    /* compute the column metrics from the previously stored cell metrics */
    void compute_column_metrics(int pass);

    /*
     *   Set the cell's height.  The formatter calls this after it's
     *   computed the cell's formatted vertical size during pass 2.  If
     *   the content height is less than the height attribute, we'll use
     *   the height attribute.  base_y_pos is the tentative y position of
     *   the top of the cell's contents; we'll save this position so that,
     *   when we later set the final position of each item in the cell's
     *   contents, we'll be able to set its offset from the final cell
     *   base position to be the same as its original offset from the
     *   tentative base position.  
     */
    void set_table_cell_height(long height, long base_y_pos);

    /* get my formatted height */
    long get_table_cell_height() const { return content_height_; }

    /* get my ROWSPAN setting */
    long get_rowspan() const { return rowspan_; }

    /*
     *   Set the cell's vertical position.  This is used at the end of the
     *   second pass, after we know the vertical size and position of the
     *   row containing this cell, to set the final positions of the items
     *   in the cell.  'y_offset' is the offset from the top of the table
     *   to the top of this cell's row.  
     */
    void set_cell_y_pos(class CHtmlFormatter *formatter,
                        long y_offset, long height,
                        HTML_Attrib_id_t row_valign);
    
    /* parse attributes */
    HTML_attrerr set_attribute(class CHtmlParser *parser,
                               HTML_Attrib_id_t attr_id,
                               const textchar_t *val, size_t vallen);

    /* on starting a cell, close any previous cell that's still open */
    void on_parse(class CHtmlParser *parser);

    /* format the cell */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
    void format_exit(class CHtmlFormatter *formatter);

protected:
    /*
     *   Get my default horizontal alignment.  This setting is used if
     *   neither the row nor the cell has an explicit ALIGN setting.  The
     *   value differs for each type of cell, thus this method is pure
     *   virtual.  
     */
    virtual HTML_Attrib_id_t get_default_cell_alignment() const = 0;

private:
    /* my row and column numbers */
    size_t rownum_;
    size_t colnum_;

    /* flag indicating that we've figured out my row and column position */
    int row_col_set_ : 1;

    /* true -> don't wrap long lines */
    int nowrap_ : 1;

    /* number of rows and columns spanned by the cell */
    long rowspan_;
    long colspan_;

    /* horizontal and vertial alignment settings */
    HTML_Attrib_id_t align_;
    HTML_Attrib_id_t valign_;

    /* alignment in effect immediately before this cell started */
    HTML_Attrib_id_t old_align_;

    /* width and height attributes in pixels */
    long width_;
    long height_;

    /* flag indicating that the width is a percentage of the table width */
    int width_pct_;

    /* 
     *   Content height in pixels; this is the actual height of the
     *   contents after formatting the cell on the second pass. 
     */
    long content_height_;

    /* 
     *   Tentative vertical position of the top of the cell's contents.
     *   This is the position that the formatter puts the contents at
     *   before the final positions are known; during the final
     *   positioning pass, we'll use this as the base, so that we know how
     *   far to put each item from the final vertical position of the
     *   cell. 
     */
    long disp_y_base_;

    /* flags indicating whether width and height have been specified */
    int width_set_ : 1;
    int height_set_ : 1;

    /* 
     *   My display item.  This is only meaningful inside a single table
     *   formatting pass; we need to keep track of it so that we can
     *   calculate the cell's metrics when we finish formatting its
     *   contents in pass 1, and so that we can set the vertical positions
     *   of the cell's contents in pass 2.  
     */
    class CHtmlDispTableCell *disp_;

    /* 
     *   Last display item in the cell.  This is used as a marker so that
     *   we know the range of the display items contained within the cell. 
     */
    class CHtmlDisp *disp_last_;

    /* min/max width calculated from contents */
    long cont_min_width_;
    long cont_max_width_;
};

class CHtmlTagTH: public CHtmlTagTableCell
{
public:
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagTH, "TH");
    CHtmlTagTH(class CHtmlParser *parser) : CHtmlTagTableCell(parser) { }

protected:
    /* header cells are centered by default */
    HTML_Attrib_id_t get_default_cell_alignment() const
    {
        return HTML_Attrib_center;
    }

private:
};

class CHtmlTagTD: public CHtmlTagTableCell
{
public:
    HTML_TAG_MAP_NOCONSTRUCTOR(CHtmlTagTD, "TD");
    CHtmlTagTD(class CHtmlParser *parser) : CHtmlTagTableCell(parser) { }

protected:
    /* data cells are aligned left by default */
    HTML_Attrib_id_t get_default_cell_alignment() const
    {
        return HTML_Attrib_left;
    }

private:
};

/* ------------------------------------------------------------------------ */
/*
 *   Special internal processing directive tag for T3 callers.  This tells us
 *   to adjust our parsing rules for T3 programs.  When we see this tag, we
 *   disable the <BANNER> tag, because that tag isn't allowed in T3.  
 */
class CHtmlTagQT3: public CHtmlTag
{
public:
    HTML_TAG_MAP(CHtmlTagQT3, "?T3");

    /* activate our special handling on formatting */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
};

/*
 *   Special internal processing directive tag for TADS 2 callers.  This
 *   tells us to adjust our parsing rules for TADS 2 programs.
 */
class CHtmlTagQT2: public CHtmlTag
{
public:
    HTML_TAG_MAP(CHtmlTagQT2, "?T2");

    /* activate our special handling on formatting */
    void format(class CHtmlSysWin *win, class CHtmlFormatter *formatter);
};


/* ------------------------------------------------------------------------ */
/*
 *   Special parameterized color values.  These color values are used to
 *   store references to color settings that are mapped through a
 *   system-dependent mechanism to an actual system color.  The color
 *   mapping mechanism may use a player-controlled preference mechanism to
 *   map the colors, or may simply choose colors that are suitable for the
 *   system color scheme or display characteristics.
 *   
 *   These values are used to store colors after parsing and until
 *   formatting, so they're stored only in tag objects.  Upon rendering a
 *   tag into a display list item, we'll translate these colors into the
 *   corresponding RGB values.  This deferred translation allows us to
 *   update the displayed color scheme by reformatting the document, which
 *   is useful if the color mapping mechanism allows the player to control
 *   the colors through preference settings.
 *   
 *   Note that we can distinguish these color values from actual RGB
 *   values by checking the high-order byte of the 32-bit value.  Actual
 *   RGB values always have zero in the high order byte; these special
 *   values have a non-zero value in the high-order byte. 
 */

/* status line background and text colors */
const unsigned long HTML_COLOR_STATUSBG    = 0x01000001;
const unsigned long HTML_COLOR_STATUSTEXT  = 0x01000002;

/* link colors - regular, active (i.e., clicked) */
const unsigned long HTML_COLOR_LINK        = 0x01000003;
const unsigned long HTML_COLOR_ALINK       = 0x01000004;

/* text color */
const unsigned long HTML_COLOR_TEXT        = 0x01000005;

/* background color */
const unsigned long HTML_COLOR_BGCOLOR     = 0x01000006;

/* color of command-line input text */
const unsigned long HTML_COLOR_INPUT       = 0x01000007;

/* link color - hovering */
const unsigned long HTML_COLOR_HLINK       = 0x01000008;

/*
 *   determine if a color is a special color; returns true if the color is
 *   a special parameterized color value, false if it's an RGB value 
 */
#define html_is_color_special(color) (((color) & 0xff000000) != 0)

#endif /* HTMLTAGS_H */

