/* $Header: d:/cvsroot/tads/html/htmlprs.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlprs.h - HTML parser
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#ifndef HTMLPRS_H
#define HTMLPRS_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLATTR_H
#include "htmlattr.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   HTML parser.  The client first writes HTML source code to a
 *   CHtmlTextBuffer object, then submits the buffer object to the parser
 *   to turn into a parsed tag list.  The parser can then in turn be
 *   submitted to a renderer for display.  
 */
class CHtmlParser
{
    friend class CHtmlParserState;
    
public:
    /* set up the parser */
    CHtmlParser() { init(FALSE); }

    /* 
     *   Set up the parser, optionally initializing in "literal" mode --
     *   if 'literal' is true, we won't interpret HTML markups, and we'll
     *   treat whitespace and newlines as significant. 
     */
    CHtmlParser(int literal_mode) { init(literal_mode); }

    /* delete the parser */
    ~CHtmlParser();

    /* process a buffer containing HTML source */
    void parse(const CHtmlTextBuffer *src, class CHtmlSysWinGroup *frame);

    /*
     *   clear the page, deleting all tags in all lists and sublists
     *   (apart from the special outermost container, which we keep as
     *   long as the parser itself is around) 
     */
    void clear_page();

    /*
     *   Add a new input tag.  The input tag is special in that allows
     *   editing of the contents of the contents in the parse tree; this
     *   tag provides for input buffer editing in the user interface.  
     */
    class CHtmlTagTextInput *append_input_tag(const textchar_t *buf,
                                              size_t len);

    /* add a tag to the current innermost container's tag list */
    void append_tag(class CHtmlTag *tag);

    /* close a tag if it's open */
    void close_tag_if_open(const textchar_t *nm);

    /* close the current tag */
    void close_current_tag();

    /* 
     *   pre-close the current tag - call upon recognizing a close tag,
     *   before we've actually closed anything 
     */
    void pre_close_tag(const textchar_t *nm, size_t nmlen);

    /* parse the directive we're looking at */
    void parse_directive();

    /*
     *   Begin skipping whitespace.  A tag's on_parse() method can call
     *   this whenever it wants to skip all whitespace separating the tag
     *   from the next non-blank text.  The P and BR tags use this to
     *   ensure that there isn't any stray whitespace at the start of the
     *   first line following the tag. 
     */
    void begin_skip_sp() { eat_whitespace_ = TRUE; }

    /*
     *   Stop skipping whitespace.  A tag's on_parse() or on_close() method
     *   can call this if the tag inserts text into the stream (such as <Q>).
     *   Most tags don't do any such thing, but the rare ones that do should
     *   always call this to ensure that adjacent whitespace is considered
     *   significant.  
     */
    void end_skip_sp() { eat_whitespace_ = FALSE; }

    /*
     *   Turn whitespace obedience on (flag=true) or off (flag=false).
     *   Whitespace is obeyed only within the special preformatted text
     *   containers.  If break_long_lines is true, we'll continue to allow
     *   breaking long lines within the block; generally, when whitespace
     *   is obeyed, implicit line breaks are not allowed.  Note that the
     *   break_long_lines setting is ignored when not obeying whitespace.  
     */
    void obey_whitespace(int flag, int break_long_lines);
    int get_obey_whitespace() const { return obey_whitespace_; }
    int get_break_long_lines() const { return break_long_lines_; }

    /*
     *   Turn markup translation on (flag=true) or off(flag=false).
     *   Markups are translated except within the special listing-style
     *   preformatted text containers. 
     */
    void obey_markups(int flag) { obey_markups_ = flag; }
    int get_obey_markups() const { return obey_markups_; }

    /*
     *   If in obey_markups(FALSE) mode, you can set this additional flag
     *   to determine whether or not an *end* markup of the current type
     *   will be obeyed.  Normally, end markups of markups that start a
     *   verbatim mode (such as </PRE>) should be obeyed.  However, if the
     *   caller wants markups ignored for some reason other than an
     *   opening markup, it can set obey_end_markups(FALSE) mode, in which
     *   case we'll never obey any markup of any kind. 
     */
    void obey_end_markups(int flag) { obey_end_markups_ = flag; }
    int get_obey_end_markups() const { return obey_end_markups_; }

    /* push a container onto the container stack */
    void push_container(class CHtmlTagContainer *tag);

    /* pop the innermost container */
    void pop_inner_container();

    /*
     *   Fix up a trailing container.  If the last tag we formatted was a
     *   container end tag, this will add a "relax" tag at the end of the
     *   current open container's sublist so that we have a non-container to
     *   land on at the end of the formatting cycle.  This is important when
     *   we're traversing the list for formatting, because this helps us
     *   ensure we don't repeatedly call format_exit on a closing tag by
     *   ensuring we always have a place to go after traversing out of a
     *   container.  
     */
    void fix_trailing_cont();

    /* get the innermost container on the container stack */
    class CHtmlTagContainer *get_inner_container() const
    {
        return container_;
    }

    /* get the outermost container */
    class CHtmlTagContainer *get_outer_container() const;

    /* get the depth of the container stack */
    int get_container_depth() const { return container_depth_; }

    /*
     *   End the current paragraph.  If explicit is true, it means that
     *   this is a real paragraph break, so paragraph spacing should be
     *   inserted.  Otherwise, it means that the paragraph was ended
     *   implicitly, so we shouldn't add paragraph spacing. 
     */
    void end_paragraph(int isexplicit);

    HTML_IF_DEBUG(void debug_dump();)

    /*
     *   Log an error.  This generally does nothing, but the user
     *   interface may provide a mechanism that allows the user to see the
     *   errors produced when parsing a document.  
     */
    void log_error(const textchar_t *errmsg, ...);

    /*
     *   Look up an attribute value in the enumerated attribute value
     *   list.  Returns an attribute ID if the value matches one of the
     *   enumerated values.  
     */
    HTML_Attrib_id_t attrval_to_id(const textchar_t *val, size_t vallen);

    /* get the text array */
    class CHtmlTextArray *get_text_array() const { return text_array_; }

    /* 
     *   Export a parse tree.  This should only be used after the source
     *   has been completely parsed.  After exporting the parse tree, the
     *   parser forgets all information about the parse tree -- the parser
     *   no longer references the parse tree once it has been exported.  
     */
    void export_parse_tree(class CHtmlParserState *state);

    /*
     *   Import a parse tree.  This restores a parse tree saved with
     *   save_parse_tree().  Any existing parse tree is destroyed.  
     */
    void import_parse_tree(class CHtmlParserState *state);

    /*
     *   Get the system window frame object - this is valid during
     *   parsing, so tags can use it if necessary (the main reason a tag
     *   would need this object is to translate an HTML entity value to a
     *   character value). 
     */
    class CHtmlSysWinGroup *get_sys_frame() const { return frame_; }

    /*
     *   Prune the parse tree.  Attempts to reduce the memory allocated to
     *   the text array to the given size; we'll delete top-level nodes in
     *   the parse tree, starting with the oldest nodes, until we run out
     *   of nodes that can be deleted or the text array size is no larger
     *   than the given size.
     *   
     *   Note that the actual amount of memory in use after this call will
     *   be greater than the given size, since the parse nodes themselves
     *   take up space.  In a typical document, where most of the
     *   information in the document is text, the text array size will
     *   dominate; documents that contain extensive mark-up information
     *   will naturally need more space for parse nodes for a given amount
     *   of text.  
     */
    void prune_tree(unsigned long max_text_array_size);

    /* process a closing tag for most kinds of tags */
    void end_normal_tag(const textchar_t *tag, size_t len);

    /* process a closing </P> tag */
    void end_p_tag();

private:
    /* internal initialization */
    void init(int literal_mode);

    /* destroy an externalized parse tree */
    static void destroy_parse_tree(class CHtmlParserState *state);

    /* check that we have a lexically complete tag to parse */
    int check_directive_complete();

    /* check that we have a lexically complete entity to parse */
    int check_entity_complete();

    /* determine if a given string matches the start tag name */
    int end_tag_matches(const textchar_t *end_tag_name,
                        size_t end_tag_len, int log, int find);
    
    /*
     *   Parse and return a single character, turning an '&' sequence into
     *   the corresponding single character.  Increments the pointer to
     *   point to the next character.  Fills in the result buffer with the
     *   result of the translation.  Returns the length (excluding null
     *   termination) of the result.
     *   
     *   If charset is not null, it indicates that we're in a context where
     *   we can change character sets; in this case, we'll fill in *charset
     *   with the character set to use for this character.  If charset is
     *   null, it means that we can't change the character set, so we'll
     *   attempt to map any '&' entities to the current character set.  If
     *   we need to change to a new character set, we'll set changed_charset
     *   (if the pointer isn't null) to true, otherwise we'll set it to
     *   false.
     *   
     *   '*special' returns with a non-zero value if the character is one of
     *   the Unicode characters which carry a special meaning for us.  In
     *   this case, we'll still fill in the result buffer with a text-only
     *   approximation, in case the caller doesn't care about the special
     *   meaning.  
     */
    size_t parse_char(textchar_t *result, size_t result_buf_size,
                      oshtml_charset_id_t *charset, int *changed_charset,
                      unsigned int *special);

    /* parse a special Unicode character to see if it has a special meaning */
    int special_entity(textchar_t *result, size_t result_size,
                       size_t *outlen,
                       unsigned int ch, unsigned int *special);

    /* 
     *   Parse a character entity.  This is a subroutine for parse_char(),
     *   and shouldn't be called directly by other code. 
     */
    size_t parse_char_entity(textchar_t *result, size_t result_buf_size,
                             oshtml_charset_id_t *charset,
                             int *changed_charset, unsigned int *special);

    /* parse whitespace */
    void parse_whitespace();

    /* 
     *   parse a hard tab character - we'll treat this as whitespace in
     *   most cases, but in pre-formatted text we'll insert spacing to the
     *   next tab stop 
     */
    void parse_tab();

    /* parse a newline */
    void parse_newline();

    /* parse text */
    void parse_text();

    /* append a character to our text */
    void append_to_text(textchar_t c);

    /*
     *   make a text tag out of the current text stream and add it to the
     *   current container 
     */
    void add_text_tag();

    /*
     *   Scan an identifier (tag name, attribute name) within a tag.
     *   Returns a pointer to the next character after the last character
     *   of the identifier in the buffer. 
     */
    textchar_t *scan_ident(textchar_t *buf, size_t buflen);

    /* skip runs of blank lines following a tag */
    void skip_posttag_whitespace();

    /* text array - we store the stream of text for the document here */
    class CHtmlTextArray *text_array_;

    /* current buffer position */
    CCntlenStrPtr p_;

    /* 
     *   Pending buffer.  Whenever we find an incomplete tag in the input
     *   stream, we'll add what we have so far to this buffer, and defer
     *   parsing it until more data arrive.  
     */
    CHtmlTextBuffer pending_;

    /* 
     *   starting buffer position (we keep track of this so that we can
     *   display some context before the current position when an error
     *   occurs) 
     */
    CCntlenStrPtr p_start_;

    /* Hash table for ampersand sequence names */
    class CHtmlHashTable *amp_table_;

    /* Hash table for the tag names */
    class CHtmlHashTable *tag_table_;

    /* hash table for attribute names */
    class CHtmlHashTable *attr_table_;

    /* hash table for attribute value names */
    class CHtmlHashTable *attr_val_table_;

    /* current container */
    class CHtmlTagContainer *container_;

    /*
     *   Text buffer containing current output stream.  The output stream
     *   accumulates until we encounter a new tag, at which point we build
     *   a text-container tag out of the current buffer and insert it into
     *   the tag stream. 
     */
    CHtmlTextBuffer curtext_;

    /* text buffer containing an attribute value being scanned */
    CHtmlTextBuffer curattr_;

    /* Depth of containment */
    int container_depth_;

    /* Flag: true -> obeying whitespace literally */
    int obey_whitespace_ : 1;

    /* Flag: if obey_whitespace_ is true, allow breaking long lines */
    int break_long_lines_ : 1;

    /* Flag: true -> translating markups normally */
    int obey_markups_ : 1;

    /* Flag: true -> translating end markups normally when in verbatim mode */
    int obey_end_markups_ : 1;

    /*
     *   Flag: eat any whitespace characters.  This flag is set whenever
     *   we start off a new paragraph or add a new whitespace character to
     *   the text during normal formatting. 
     */
    int eat_whitespace_ : 1;

    /*
     *   System application frame object - whenever we enter the parser,
     *   we remember the frame object passed in to the parse() call here.
     *   We need the system frame to perform certain work for us, such as
     *   translating unicode characters to the current system character
     *   set.  
     */
    class CHtmlSysWinGroup *frame_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Parser state saver.  This object can be used to save a parse tree for
 *   later use.  Note that we don't save any state involved in parsing --
 *   we only save a completely parsed tag tree.  
 */

class CHtmlParserState
{
    friend class CHtmlParser;

public:
    CHtmlParserState()
    {
        text_array_ = 0;
        container_ = 0;
        outer_container_ = 0;
        container_depth_ = 0;
    }
    
    ~CHtmlParserState()
    {
        /* ask the parser to destroy my contents */
        CHtmlParser::destroy_parse_tree(this);
    }

    /* get the text array */
    class CHtmlTextArray *get_text_array() const { return text_array_; }

private:
    /* text array */
    class CHtmlTextArray *text_array_;

    /* current container */
    class CHtmlTagContainer *container_;

    /* outermost container */
    class CHtmlTagContainer *outer_container_;

    /* container nesting depth */
    int container_depth_;
};

#endif /* HTMLPRS_H */

