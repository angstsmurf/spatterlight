#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlprs.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlprs - HTML parser
Function
  
Notes
  The parser assumes that the client will parse HTML source in chunks of
  a reasonable size.  In particular, the client must write a well-formed
  chunk of HTML to a buffer, then submit the buffer to the parser; the
  caller can't break a single tag or other syntactic unit across chunks.
Modified
  08/23/97 MJRoberts  - Creation
*/

#include <assert.h>
#include <memory.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif

#ifdef TADSHTML_DEBUG
#include <stdio.h>
#include <stdarg.h>
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry subclass for an ampersand sequence name.  Entity
 *   names are case-sensitive, thus the xxxCS base class.
 */
class CHtmlHashEntryAmp: public CHtmlHashEntryCS
{
public:
    CHtmlHashEntryAmp(const textchar_t *str, size_t len, int copy,
                      unsigned int charval)
        : CHtmlHashEntryCS(str, len, copy)
    {
        charval_ = charval;
    }
    
    /* 
     *   Character value.  We store the standard HTML code value, which is
     *   the same as the Unicode value of the character.  (Whenever we use
     *   a character entity value, we'll translate the character value to
     *   the appropriate local character set code point.)  
     */
    unsigned int charval_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry subclass for a tag name.  Entity names are
 *   case-insensitive, hence the xxxCI base class.
 */
class CHtmlHashEntryTag: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryTag(const textchar_t *str, size_t len, int copy,
                      class CHtmlTag *(*create_func)(class CHtmlParser *),
                      void (*end_func)(class CHtmlParser *,
                                       const textchar_t *, size_t))
        : CHtmlHashEntryCI(str, len, copy)
    {
        create_func_ = create_func;
        end_func_ = end_func;
    }

    /* invoke the creator function and return the result */
    CHtmlTag *create_new_tag(class CHtmlParser *parser)
        { return (*create_func_)(parser); }

    /* process the closing form of this tag (the </xxx> tag) */
    void process_end_tag(class CHtmlParser *parser,
                         const textchar_t *tag, size_t len)
        { (*end_func_)(parser, tag, len); }

private:
    /* tag object creation function */
    CHtmlTag *(*create_func_)(class CHtmlParser *);

    /* end tag function */
    void (*end_func_)(class CHtmlParser *, const textchar_t *, size_t);
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for attribute names 
 */
class CHtmlHashEntryAttr: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryAttr(const textchar_t *str, size_t len, int copy,
                       HTML_Attrib_id_t id)
        : CHtmlHashEntryCI(str, len, copy)
    {
        id_ = id;
    }

    /* hash ID value */
    HTML_Attrib_id_t id_;
};


/* ------------------------------------------------------------------------ */
/*
 *   HTML parser.  The client first writes HTML source code to a
 *   CHtmlTextBuffer object, then submits the buffer object to the parser
 *   to turn into a parsed tag list.  The parser can then in turn be
 *   submitted to a renderer for display.  
 */

/*
 *   Initialize 
 */
void CHtmlParser::init(int literal_mode)
{
    /* set up a table of '&' character name sequences */
    struct amp_tbl_t
    {
        const textchar_t *cname;
        unsigned int cval;
    };
    static const struct amp_tbl_t amp_tbl[] =
    {
        { "amp", '&' },
        { "quot", '"' },
        { "gt", '>' },
        { "lt", '<' },
        { "nbsp", 160 },
        { "iexcl", 161 },
        { "cent", 162 },
        { "pound", 163 },
        { "curren", 164 },
        { "yen", 165 },
        { "brvbar", 166 },
        { "sect", 167 },
        { "uml", 168 },
        { "copy", 169 },
        { "ordf", 170 },
        { "laquo", 171 },
        { "not", 172 },
        { "shy", 173 },
        { "reg", 174 },
        { "macr", 175 },
        { "deg", 176 },
        { "plusmn", 177 },
        { "sup2", 178 },
        { "sup3", 179 },
        { "acute", 180 },
        { "micro", 181 },
        { "para", 182 },
        { "middot", 183 },
        { "cedil", 184 },
        { "sup1", 185 },
        { "ordm", 186 },
        { "raquo", 187 },
        { "frac14", 188 },
        { "frac12", 189 },
        { "frac34", 190 },
        { "iquest", 191 },
        { "times", 215 },
        { "divide", 247 },
        { "AElig", 198 },
        { "Aacute", 193 },
        { "Acirc", 194 },
        { "Agrave", 192 },
        { "Aring", 197 },
        { "Atilde", 195 },
        { "Auml", 196 },
        { "Ccedil", 199 },
        { "ETH", 208 },
        { "Eacute", 201 },
        { "Ecirc", 202 },
        { "Egrave", 200 },
        { "Euml", 203 },
        { "Iacute", 205 },
        { "Icirc", 206 },
        { "Igrave", 204 },
        { "Iuml", 207 },
        { "Ntilde", 209 },
        { "Oacute", 211 },
        { "Ocirc", 212 },
        { "Ograve", 210 },
        { "Oslash", 216 },
        { "Otilde", 213 },
        { "Ouml", 214 },
        { "THORN", 222 },
        { "Uacute", 218 },
        { "Ucirc", 219 },
        { "Ugrave", 217 },
        { "Uuml", 220 },
        { "Yacute", 221 },
        { "aacute", 225 },
        { "acirc", 226 },
        { "aelig", 230 },
        { "agrave", 224 },
        { "aring", 229 },
        { "atilde", 227 },
        { "auml", 228 },
        { "ccedil", 231 },
        { "eacute", 233 },
        { "ecirc", 234 },
        { "egrave", 232 },
        { "eth", 240 },
        { "euml", 235 },
        { "iacute", 237 },
        { "icirc", 238 },
        { "igrave", 236 },
        { "iuml", 239 },
        { "ntilde", 241 },
        { "oacute", 243 },
        { "ocirc", 244 },
        { "ograve", 242 },
        { "oslash", 248 },
        { "otilde", 245 },
        { "ouml", 246 },
        { "szlig", 223 },
        { "thorn", 254 },
        { "uacute", 250 },
        { "ucirc", 251 },
        { "ugrave", 249 },
        { "uuml", 252 },
        { "yacute", 253 },
        { "thorn", 254 },
        { "yuml", 255 },

        /* TADS extensions to the standard characters */
        { "lsq", 8216 },
        { "rsq", 8217 },
        { "ldq", 8220 },
        { "rdq", 8221 },
        { "endash", 8211 },
        { "emdash", 8212 },
        { "trade", 8482 },
        { "zwsp", 0x200b },
        { "zwnbsp", 0xfeff },
        { "tpmsp", 0x2004 },
        { "fpmsp", 0x2005 },
        { "spmsp", 0x2006 },
        { "figsp", 0x2007 },
        { "puncsp", 0x2008 },
        { "hairsp", 0x200a },

        /* HTML 4.0 character extensions */
        { "ndash", 8211 },
        { "mdash", 8212 },
        { "lsquo", 8216 },
        { "rsquo", 8217 },
        { "ldquo", 8220 },
        { "rdquo", 8221 },
        { "sbquo", 8218 },
        { "bdquo", 8222 },
        { "lsaquo", 8249 },
        { "rsaquo", 8250 },
        { "dagger", 8224 },
        { "Dagger", 8225 },
        { "OElig", 338 },
        { "oelig", 339 },
        { "permil", 8240 },
        { "Yuml", 376 },
        { "scaron", 353 },
        { "Scaron", 352 },
        { "circ", 710 },
        { "tilde", 732 },
        { "ensp", 0x2002 },
        { "emsp", 0x2003 },
        { "thinsp", 0x2009 },

        /* Greek letters */
        { "Alpha", 913 },
        { "Beta", 914 },
        { "Gamma", 915 },
        { "Delta", 916 },
        { "Epsilon", 917 },
        { "Zeta", 918 },
        { "Eta", 919 },
        { "Theta", 920 },
        { "Iota", 921 },
        { "Kappa", 922 },
        { "Lambda", 923 },
        { "Mu", 924 },
        { "Nu", 925 },
        { "Xi", 926 },
        { "Omicron", 927 },
        { "Pi", 928 },
        { "Rho", 929 },
        { "Sigma", 931 },
        { "Tau", 932 },
        { "Upsilon", 933 },
        { "Phi", 934 },
        { "Chi", 935 },
        { "Psi", 936 },
        { "Omega", 937 },
        { "alpha", 945 },
        { "beta", 946 },
        { "gamma", 947 },
        { "delta", 948 },
        { "epsilon", 949 },
        { "zeta", 950 },
        { "eta", 951 },
        { "theta", 952 },
        { "iota", 953 },
        { "kappa", 954 },
        { "lambda", 955 },
        { "mu", 956 },
        { "nu", 957 },
        { "xi", 958 },
        { "omicron", 959 },
        { "pi", 960 },
        { "rho", 961 },
        { "sigmaf", 962 },
        { "sigma", 963 },
        { "tau", 964 },
        { "upsilon", 965 },
        { "phi", 966 },
        { "chi", 967 },
        { "psi", 968 },
        { "omega", 969 },
        { "thetasym", 977 },
        { "upsih", 978 },
        { "piv", 982 },

        /* general punctuation marks */
        { "bull", 8226 },
        { "hellip", 8230 },
        { "prime", 8242 },
        { "Prime", 8243 },
        { "oline", 8254 },
        { "frasl", 8260 },

        /* letter-like symbols */
        { "weierp", 8472 },
        { "image", 8465 },
        { "real", 8476 },
        { "alefsym", 8501 },

        /* arrows */
        { "larr", 8592 },
        { "uarr", 8593 },
        { "rarr", 8594 },
        { "darr", 8595 },
        { "harr", 8596 },
        { "crarr", 8629 },
        { "lArr", 8656 },
        { "uArr", 8657 },
        { "rArr", 8658 },
        { "dArr", 8659 },
        { "hArr", 8660 },

        /* mathematical operators */
        { "forall", 8704 },
        { "part", 8706 },
        { "exist", 8707 },
        { "empty", 8709 },
        { "nabla", 8711 },
        { "isin", 8712 },
        { "notin", 8713 },
        { "ni", 8715 },
        { "prod", 8719 },
        { "sum", 8721 },
        { "minus", 8722 },
        { "lowast", 8727 },
        { "radic", 8730 },
        { "prop", 8733 },
        { "infin", 8734 },
        { "ang", 8736 },
        { "and", 8743 },
        { "or", 8744 },
        { "cap", 8745 },
        { "cup", 8746 },
        { "int", 8747 },
        { "there4", 8756 },
        { "sim", 8764 },
        { "cong", 8773 },
        { "asymp", 8776 },
        { "ne", 8800 },
        { "equiv", 8801 },
        { "le", 8804 },
        { "ge", 8805 },
        { "sub", 8834 },
        { "sup", 8835 },
        { "nsub", 8836 },
        { "sube", 8838 },
        { "supe", 8839 },
        { "oplus", 8853 },
        { "otimes", 8855 },
        { "perp", 8869 },
        { "sdot", 8901 },
        { "lceil", 8968 },
        { "rceil", 8969 },
        { "lfloor", 8970 },
        { "rfloor", 8971 },
        { "lang", 9001 },
        { "rang", 9002 },

        /* geometric shapes */
        { "loz", 9674 },

        /* miscellaneous symbols */
        { "spades", 9824 },
        { "clubs", 9827 },
        { "hearts", 9829 },
        { "diams", 9830 },

        /* Latin-extended B */
        { "fnof", 402 },

        /* Latin-2 characters */
        { "Aogon", 260 },
        { "breve", 728 },
        { "Lstrok", 321 },
        { "Lcaron", 317 },
        { "Sacute", 346 },
        { "Scedil", 350 },
        { "Tcaron", 356 },
        { "Zacute", 377 },
        { "Zcaron", 381 },
        { "Zdot", 379 },
        { "aogon", 261 },
        { "ogon", 731 },
        { "lstrok", 322 },
        { "lcaron", 318 },
        { "sacute", 347 },
        { "caron", 711 },
        { "scedil", 351 },
        { "tcaron", 357 },
        { "zacute", 378 },
        { "dblac", 733 },
        { "zcaron", 382 },
        { "zdot", 380 },
        { "Racute", 340 },
        { "Abreve", 258 },
        { "Lacute", 313 },
        { "Cacute", 262 },
        { "Ccaron", 268 },
        { "Eogon", 280 },
        { "Ecaron", 282 },
        { "Dcaron", 270 },
        { "Dstrok", 272 },
        { "Nacute", 323 },
        { "Ncaron", 327 },
        { "Odblac", 336 },
        { "Rcaron", 344 },
        { "Uring", 366 },
        { "Udblac", 368 },
        { "Tcedil", 354 },
        { "racute", 341 },
        { "abreve", 259 },
        { "lacute", 314 },
        { "cacute", 263 },
        { "ccaron", 269 },
        { "eogon", 281 },
        { "ecaron", 283 },
        { "dcaron", 271 },
        { "dstrok", 273 },
        { "nacute", 324 },
        { "ncaron", 328 },
        { "odblac", 337 },
        { "rcaron", 345 },
        { "uring", 367 },
        { "udblac", 369 },
        { "tcedil", 355 },
        { "dot", 729 }
    };
    const struct amp_tbl_t *ampptr;
    size_t i;

    struct tag_tbl_t
    {
        const textchar_t *(*name_func)();
        CHtmlTag *(*create_func)(class CHtmlParser *);
        void (*end_func)(class CHtmlParser *, const textchar_t *, size_t);
    };
    static const struct tag_tbl_t tag_tbl[] =
    {
#define HTML_REG_TAG(clsname) \
        { &clsname::get_tag_name_st, \
          &clsname::create_tag_instance, \
          &clsname::process_end_tag },

#include "htmlreg.h"
        { 0, 0 }
    };
    const struct tag_tbl_t *tagptr;

    /* structure for the attribute name list */
    struct attr_t
    {
        const textchar_t *nm;
        HTML_Attrib_id_t id;
    };
    struct attr_t *attr;

    /* list of identifiers that can appear as attribute names */
    static struct attr_t attr_list[] =
    {
        { "background", HTML_Attrib_background },
        { "bgcolor", HTML_Attrib_bgcolor },
        { "text", HTML_Attrib_text },
        { "link", HTML_Attrib_link },
        { "vlink", HTML_Attrib_vlink },
        { "alink", HTML_Attrib_alink },
        { "hlink", HTML_Attrib_hlink },
        { "align", HTML_Attrib_align },
        { "compact", HTML_Attrib_compact },
        { "type", HTML_Attrib_type },
        { "start", HTML_Attrib_start },
        { "value", HTML_Attrib_value },
        { "width", HTML_Attrib_width },
        { "noshade", HTML_Attrib_noshade },
        { "size", HTML_Attrib_size },
        { "valign", HTML_Attrib_valign },
        { "border", HTML_Attrib_border },
        { "cellspacing", HTML_Attrib_cellspacing },
        { "cellpadding", HTML_Attrib_cellpadding },
        { "nowrap", HTML_Attrib_nowrap },
        { "rowspan", HTML_Attrib_rowspan },
        { "colspan", HTML_Attrib_colspan },
        { "height", HTML_Attrib_height },
        { "name", HTML_Attrib_name },
        { "checked", HTML_Attrib_checked },
        { "maxlength", HTML_Attrib_maxlength },
        { "src", HTML_Attrib_src },
        { "asrc", HTML_Attrib_asrc },
        { "hsrc", HTML_Attrib_hsrc },
        { "multiple", HTML_Attrib_multiple },
        { "selected", HTML_Attrib_selected },
        { "rows", HTML_Attrib_rows },
        { "cols", HTML_Attrib_cols },
        { "href", HTML_Attrib_href },
        { "rel", HTML_Attrib_rel },
        { "rev", HTML_Attrib_rev },
        { "title", HTML_Attrib_title },
        { "alt", HTML_Attrib_alt },
        { "hspace", HTML_Attrib_hspace },
        { "vspace", HTML_Attrib_vspace },
        { "usemap", HTML_Attrib_usemap },
        { "ismap", HTML_Attrib_ismap },
        { "codebase", HTML_Attrib_codebase },
        { "code", HTML_Attrib_code },
        { "face", HTML_Attrib_face },
        { "color", HTML_Attrib_color },
        { "clear", HTML_Attrib_clear },
        { "shape", HTML_Attrib_shape },
        { "coords", HTML_Attrib_coords },
        { "nohref", HTML_Attrib_nohref },
        { "id", HTML_Attrib_id },
        { "remove", HTML_Attrib_remove },
        { "to", HTML_Attrib_to },
        { "indent", HTML_Attrib_indent },
        { "dp", HTML_Attrib_dp },
        { "plain", HTML_Attrib_plain },
        { "forced", HTML_Attrib_forced },
        { "continue", HTML_Attrib_continue },
        { "seqnum", HTML_Attrib_seqnum },
        { "layer", HTML_Attrib_layer },
        { "cancel", HTML_Attrib_cancel },
        { "repeat", HTML_Attrib_repeat },
        { "random", HTML_Attrib_random },
        { "fadein", HTML_Attrib_fadein },
        { "fadeout", HTML_Attrib_fadeout },
        { "interrupt", HTML_Attrib_interrupt },
        { "sequence", HTML_Attrib_sequence },
        { "removeall", HTML_Attrib_removeall },
        { "append", HTML_Attrib_append },
        { "noenter", HTML_Attrib_noenter },
        { "char", HTML_Attrib_char },
        { "word", HTML_Attrib_word },
        { "hidden", HTML_Attrib_hidden },
        { "hover", HTML_Attrib_hover },
        { 0, HTML_Attrib_invalid }
    };

    /* list of identifiers that can appear as attribute values */
    static struct attr_t val_list[] =
    {
        { "black", HTML_Attrib_black },
        { "silver", HTML_Attrib_silver },
        { "gray", HTML_Attrib_gray },
        { "white", HTML_Attrib_white },
        { "maroon", HTML_Attrib_maroon },
        { "red", HTML_Attrib_red },
        { "purple", HTML_Attrib_purple },
        { "fuchsia", HTML_Attrib_fuchsia },
        { "green", HTML_Attrib_green },
        { "lime", HTML_Attrib_lime },
        { "olive", HTML_Attrib_olive },
        { "yellow", HTML_Attrib_yellow },
        { "navy", HTML_Attrib_navy },
        { "blue", HTML_Attrib_blue },
        { "teal", HTML_Attrib_teal },
        { "aqua", HTML_Attrib_aqua },
        { "left", HTML_Attrib_left },
        { "right", HTML_Attrib_right },
        { "center", HTML_Attrib_center },
        { "disc", HTML_Attrib_disc },
        { "square", HTML_Attrib_square },
        { "circle", HTML_Attrib_circle },
        { "top", HTML_Attrib_top },
        { "middle", HTML_Attrib_middle },
        { "bottom", HTML_Attrib_bottom },
        { "password", HTML_Attrib_password },
        { "checkbox", HTML_Attrib_checkbox },
        { "radio", HTML_Attrib_radio },
        { "submit", HTML_Attrib_submit },
        { "reset", HTML_Attrib_reset },
        { "file", HTML_Attrib_file },
        { "hidden", HTML_Attrib_hidden },
        { "image", HTML_Attrib_image },
        { "all", HTML_Attrib_all },
        { "rect", HTML_Attrib_rect },
        { "poly", HTML_Attrib_poly },
        { "justify", HTML_Attrib_justify },
        { "decimal", HTML_Attrib_decimal },
        { "previous", HTML_Attrib_previous },
        { "border", HTML_Attrib_border },
        { "foreground", HTML_Attrib_foreground },
        { "ambient", HTML_Attrib_ambient },
        { "bgambient", HTML_Attrib_bgambient },
        { "background", HTML_Attrib_background },
        { "loop", HTML_Attrib_loop },
        { "random", HTML_Attrib_random },
        { "replace", HTML_Attrib_replace },
        { "cycle", HTML_Attrib_cycle },
        { "cancel", HTML_Attrib_cancel },
        { "statusbg", HTML_Attrib_statusbg },
        { "statustext", HTML_Attrib_statustext },
        { "link", HTML_Attrib_link },
        { "alink", HTML_Attrib_alink },
        { "hlink", HTML_Attrib_hlink },
        { "bgcolor", HTML_Attrib_bgcolor },
        { "text", HTML_Attrib_text },
        { "char", HTML_Attrib_char },
        { "word", HTML_Attrib_word },
        { 0, HTML_Attrib_invalid }
    };

    /* no system frame object yet */
    frame_ = 0;

    /* create a hash table for the '&' sequence names, and fill it up */
    amp_table_ = new CHtmlHashTable(256, new CHtmlHashFuncCS);
    for (i = 0, ampptr = amp_tbl ; i < sizeof(amp_tbl)/sizeof(amp_tbl[0]) ;
         ++i, ++ampptr)
    {
        CHtmlHashEntryAmp *entry;

        /* create a new hash table entry and add it to the hash table */
        entry = new CHtmlHashEntryAmp(ampptr->cname,
                                      get_strlen(ampptr->cname), FALSE,
                                      ampptr->cval);
        amp_table_->add(entry);
    }

    /* create a hash table for the tags, and fill it up */
    tag_table_ = new CHtmlHashTable(256, new CHtmlHashFuncCI);
    for (tagptr = tag_tbl ; tagptr->name_func != 0 ; ++tagptr)
    {
        CHtmlHashEntryTag *entry;
        const textchar_t *tagname;

        /*
         *   get the tag name from the class's static member function for
         *   naming 
         */
        tagname = (*tagptr->name_func)();

        /* create a new hash table entry and add it to the hash table */
        entry = new CHtmlHashEntryTag(tagname, get_strlen(tagname),
                                      FALSE, tagptr->create_func,
                                      tagptr->end_func);
        tag_table_->add(entry);
    }

    /* create and populate the hash table for attribute names */
    attr_table_ = new CHtmlHashTable(256, new CHtmlHashFuncCI);
    for (attr = attr_list ; attr->nm != 0 ; ++attr)
    {
        CHtmlHashEntryAttr *entry;

        entry = new CHtmlHashEntryAttr(attr->nm, get_strlen(attr->nm),
                                       FALSE, attr->id);
        attr_table_->add(entry);
    }

    /* create and populate the hash table for attribute value names */
    attr_val_table_ = new CHtmlHashTable(256, new CHtmlHashFuncCI);
    for (attr = val_list ; attr->nm != 0 ; ++attr)
    {
        CHtmlHashEntryAttr *entry;

        entry = new CHtmlHashEntryAttr(attr->nm, get_strlen(attr->nm),
                                       FALSE, attr->id);
        attr_val_table_->add(entry);
    }

    /* allocate the text array for storing the text stream */
    text_array_ = new CHtmlTextArray;

    /* create special outermost container */
    container_depth_ = 1;
    container_ = new CHtmlTagOuter;

    /* start off obeying markups, unless in literal mode */
    obey_markups_ = !literal_mode;
    obey_end_markups_ = !literal_mode;

    /* 
     *   start off treating whitespace characters as mere separators,
     *   unless in literal mode 
     */
    obey_whitespace_ = (literal_mode != 0);
    break_long_lines_ = TRUE;

    /* start off eating whitespace characters */
    eat_whitespace_ = TRUE;
}


/*
 *   Destroy the parser 
 */
CHtmlParser::~CHtmlParser()
{
    /* delete the hash tables */
    delete amp_table_;
    delete tag_table_;
    delete attr_table_;
    delete attr_val_table_;

    /* clear the page, deleting any tag lists we have accumulated */
    clear_page();

    /* delete the text array */
    delete text_array_;

    /*
     *   delete the special outermost container object, which stays
     *   around until the bitter end (which would be now) 
     */
    delete container_;
}

/*
 *   Clear the page 
 */
void CHtmlParser::clear_page()
{
    /* find the outermost container and make it current */
    while (container_->get_container() != 0)
        container_ = container_->get_container();

    /* delete everything within the outermost container */
    container_->delete_contents();

    /* delete all of the text in the text array */
    text_array_->clear();

    /* clear out the text accumulator */
    curtext_.clear();

    /* reset container depth */
    container_depth_ = 1;

    /* start off eating whitespace, unless we're in obedient mode */
    if (!obey_whitespace_)
        eat_whitespace_ = TRUE;
}

/*
 *   Turn whitespace obedience on or off.  When we first turn on
 *   whitespace obedience, we check to see if we're at the end of a line;
 *   if so, we skip up to the first character past the line break.
 */
void CHtmlParser::obey_whitespace(int flag, int break_long_lines)
{
    /* note the break-long-lines setting */
    break_long_lines_ = break_long_lines;

    /* ignore if this isn't a change from the status quo */
    if (flag == obey_whitespace_)
        return;

    /* set the new mode */
    obey_whitespace_ = flag;

    /* see if we're turning whitespace obedience on or off */
    if (!flag)
    {
        const textchar_t *p = curtext_.getbuf();
        size_t len = curtext_.getlen();
        
        /*
         *   Return to normal mode.  If we added a newline to our
         *   verbatim text just prior to this directive, take it back out.
         */
        if (len > 0 && *(p + len - 1) == '\n')
            curtext_.setlen(len - 1);
    }
}

/*
 *   Parse a character 
 */
size_t CHtmlParser::parse_char(textchar_t *result, size_t result_buf_size,
                               oshtml_charset_id_t *charset,
                               int *changed_charset, unsigned int *special)
{
    size_t char_bytes;

    /* presume no special unicode meaning */
    if (special != 0)
        *special = 0;

    /* presume we won't need to change the character set */
    if (changed_charset != 0)
        *changed_charset = FALSE;

    /* 
     *   note the length of the character in bytes (in case we're using a
     *   multi-byte character set) 
     */
    char_bytes = p_.char_bytes(frame_->get_default_win_charset());
    if (char_bytes > 1)
    {
        size_t i;
        
        /* 
         *   It's a multi-byte character.  Assume that this means that it's
         *   not a markup, since only ASCII characters can be involved in
         *   markups, and we assume that our multi-byte character sets are
         *   arranged in such a way that ASCII characters are all
         *   single-byte.  Simply copy all of the bytes of the character, to
         *   ensure that we don't separate the bytes of a single character
         *   into separate buffer runs, and return the character without
         *   further interpretation.  
         */
        for (i = 0 ; i < char_bytes && i < result_buf_size ; p_.inc(), ++i)
            result[i] = p_.curchar();

        /* null-terminate the result buffer */
        result[i] = '\0';

        /* return the result length */
        return char_bytes;
    }

    /* start with the character as-is */
    result[0] = p_.curchar();
    result[1] = '\0';
    
    /* if it's an ampersand, we need to parse the entity */
    if (p_.curchar() == '&')
        return parse_char_entity(result, result_buf_size,
                                 charset, changed_charset, special);
    
    /* if it's a newline character, skip the newline sequence */
    if (is_newline(result[0]))
    {
        /* skip the entire newline sequence */
        p_.skip_newline();
        
        /* 
         *   always use \n as the parsed text stream result, regardless of
         *   what kind of newline sequence was in the source text 
         */
        result[0] = '\n';
    }
    else
        p_.inc();
    
    /* it's a single character */
    return 1;
}

/*
 *   Parse the character entity we're looking at.  This is a subroutine for
 *   parse_char(); this routine shouldn't be called directly by other code.
 *   This will only be called when we find a '&' in the input stream.
 *   
 *   '*special' is a Unicode character code that we treat specially, such as
 *   the special space characters.  If this is provided, we'll set it to a
 *   non-zero value on return if we find one of these Unicode values.  We'll
 *   also fill in the result buffer with an approximation, which case be
 *   used if the caller isn't interested in the special Unicode character
 *   but just wants a textual approximation.
 *   
 *   The caller will already have initialized '*charset' to false, and will
 *   have set up the result buffer with a null character at the second
 *   element (which assumes that we'll return a one-character string).  
 */
size_t CHtmlParser::parse_char_entity(textchar_t *result, size_t result_size,
                                      oshtml_charset_id_t *charset,
                                      int *changed_charset,
                                      unsigned int *special)
{
    size_t len;
    size_t skipcnt;
    
    /* presume we'll return a single character */
    len = 1;

    /* if we're not obeying markups, parse the ampersand literally */
    if (!obey_markups_)
    {
        /* skip the '&', and return the '&' literally */
        p_.inc();
        result[0] = '&';
        return 1;
    }

    /* 
     *   make sure the entity is complete - if not, defer parsing it until we
     *   get more text 
     */
    if (!check_entity_complete())
        return 0;
        
    /*
     *   check the next character - if it's not followed by a letter or '#'
     *   and a digit, it's not a markup 
     */
    p_.inc();
    if (p_.getlen() >= 2 && p_.curchar() == '#')
    {
        int hex = FALSE;
        
        /* skip the '#' */
        skipcnt = 1;
        p_.inc();
        
        /* if it's hex, accumulate the value in hex */
        if (p_.curchar() == 'x' || p_.curchar() == 'X')
        {
            hex = TRUE;
            p_.inc();
            ++skipcnt;
        }
        
        /* if the next character isn't a digit, it's not a markup */
        if (is_digit(p_.curchar())
            || (hex && is_hex_digit(p_.curchar())))
        {
            unsigned int acc;
            
            /* read the digits */
            for (acc = 0 ;
                 p_.getlen() != 0
                     && (is_digit(p_.curchar())
                         || (hex && is_hex_digit(p_.curchar()))) ;
                 p_.inc(), ++skipcnt)
            {
                acc *= (hex ? 16 : 10);
                acc += ascii_hex_to_int(p_.curchar());
            }
            
            /*
             *   if we stopped at a semicolon, the semicolon is part of the
             *   markup, so skip it 
             */
            if (p_.getlen() != 0 && p_.curchar() == ';')
            {
                p_.inc();
                ++skipcnt;
            }
            
            /* translate the character */
            if (!special_entity(result, result_size, &len, acc, special))
            {
                if (frame_ != 0)
                    len = frame_->xlat_html4_entity(result, result_size,
                        acc, charset, changed_charset);
                else
                    result[0] = (textchar_t)acc;
            }
        }
        else
        {
            /* it's not a digit sequence, so it's not a markup */
            p_.dec(skipcnt);
            result[0] = '&';
        }
    }
    else if (p_.getlen() != 0 && is_alpha(p_.curchar()))
    {
        const int markup_maxlen = 12;
        textchar_t markup[markup_maxlen + 1];
        textchar_t *dst;
        CHtmlHashEntryAmp *entry;
        
        /* accumulate the markup name */
        for (skipcnt = 0, dst = markup ;
             p_.getlen() != 0
                 && (is_alpha(p_.curchar()) || is_digit(p_.curchar()))
                 && dst - markup < markup_maxlen ;
             p_.inc(), ++skipcnt)
        {
            /* add this character */
            *dst++ = p_.curchar();
        }
        
        /* if we found a semicolon, it's part of the markup, so skip it */
        if (p_.getlen() != 0 && p_.curchar() == ';')
        {
            p_.inc();
            ++skipcnt;
        }
        
        /* find the markup in the markup table */
        entry = (CHtmlHashEntryAmp *)amp_table_->find(markup, dst - markup);
        
        /* 
         *   if we didn't find an exact match, search for any sequence that
         *   matches the leading substring 
         */
        if (entry == 0)
        {
            /* find the entry by leading substring */
            entry = (CHtmlHashEntryAmp *)amp_table_->
                    find_leading_substr(markup, dst - markup);
            
            /* if we found it, skip the appropriate length */
            if (entry != 0)
                p_.dec(skipcnt - entry->getlen());
        }
        
        /* see if we found it */
        if (entry != 0)
        {
            /* 
             *   found it - translate the character value to the current
             *   character set 
             */
            if (!special_entity(result, result_size, &len,
                                entry->charval_, special))
            {
                if (frame_ != 0)
                    len = frame_->xlat_html4_entity(result, result_size,
                        entry->charval_, charset, changed_charset);
                else
                    result[0] = (textchar_t)entry->charval_;
            }
        }
        else
        {
            /* we didn't find a match -- copy the whole thing as-is */
            p_.dec(skipcnt);
            result[0] = '&';
        }
    }
    else
    {
        /* it's not a markup */
        result[0] = '&';
    }

    /* return the length */
    return len;
}

/*
 *   Check for a special Unicode entity.  This checks a Unicode character
 *   value to determine if it's one of the special codes we interpret
 *   directly, rather than mapping to the local character set.
 *   
 *   We give special meanings to the following characters:
 *   
 *   Note that this is used specifically to check entities given as '&'
 *   markups before any local character set translation, which means the
 *   character value we're looking up is always given in Unicode, regardless
 *   of the local character set.  This code is thus universal - it doesn't
 *   matter what kind of local character set we're using, because we're
 *   operating purely in the Unicode domain at this point.
 *   
 *   Returns true if we find a special character, false if not.  Our
 *   approximations are always simple ASCII, so there's no need for the
 *   caller to perform further character set translation on the result when
 *   we provide one.  
 */
int CHtmlParser::special_entity(textchar_t *result, size_t result_size,
                                size_t *outlen,
                                unsigned int ch, unsigned int *special)
{
    int found;

    /* presume we won't find a special character */
    found = FALSE;

    /* check the input character */
    switch (ch)
    {
    case 0x00AD:                                             /* soft hyphen */
    case 0xFEFF:                           /* zero-width non-breaking space */
    case 0x200B:                                        /* zero-width space */
        /* 
         *   Approximate these as an empty string, since none of these has
         *   any visible presence under normal conditions.  (The soft hyphen
         *   does appear visually as a hyphen when it's actually used as a
         *   line breaking point, but if the caller is only interested in
         *   the approximation, then they won't know to break the line here,
         *   so they'll never need to render the hyphen.)  
         */
        *outlen = 0;
        result[0] = '\0';
        found = TRUE;
        break;

    case 0x0015:                                    /* our own quoted space */
    case 0x00A0:                                      /* non-breaking space */
    case 0x2002:                                                /* en space */
    case 0x2003:                                                /* em space */
    case 0x2004:                                      /* three-per-em space */
    case 0x2005:                                       /* four-per-em space */
    case 0x2006:                                        /* six-per-em space */
    case 0x2007:                                            /* figure space */
    case 0x2008:                                       /* punctuation space */
    case 0x2009:                                              /* thin space */
    case 0x200A:                                              /* hair space */
        /* these all look like an ordinary space in text approximations */
        *outlen = 1;
        result[0] = ' ';
        found = TRUE;
        break;
    }

    /* 
     *   if we found a special character, and the caller is interested in
     *   the special meaning, provide it - the special meaning is simply the
     *   unicode value for the special character itself 
     */
    if (found && special != 0)
        *special = ch;

    /* indicate whether or not we found anything special */
    return found;
}

/*
 *   Parse an identifier 
 */
textchar_t *CHtmlParser::scan_ident(textchar_t *buf, size_t buflen)
{
    textchar_t *endp;

    /* scan through the identifier until we find a space, '>', or '=' */
    for (endp = buf ; p_.getlen() != 0 ; p_.inc())
    {
        /* if we're at a space or the closing '>' delimiter, we're done */
        if (is_space(p_.curchar())
            || p_.curchar() == '>'
            || p_.curchar() == '=')
            break;

        /* accumulate this character, unless we're out of buffer space */
        if ((size_t)(endp - buf) < buflen - 1)
            *endp++ = p_.curchar();
    }

    /* null-terminate it */
    *endp = '\0';

    /* return a pointer to the end of the buffer */
    return endp;
}

/*
 *   Determine if a tag matches the current container tag.  Returns true if
 *   so, false if not.  If it doesn't match, and "log" is true, we'll log the
 *   mismatch as an error; otherwise, we'll silently ignore it.
 *   
 *   If 'find' is true, we'll search the stack for the matching open tag.  If
 *   we find a matching tag somewhere in the stack, we'll close all of the
 *   tags nested within it.  This makes us more tolerant of ill-formed HTML,
 *   where an end-tag is omitted, by allowing us to match up the close tag
 *   even if there was an unclosed tag nested within it.  
 */
int CHtmlParser::end_tag_matches(const textchar_t *end_tag_name,
                                 size_t end_tag_len, int log, int find)
{
    const textchar_t *start_tag_name;

    /* get the name of the starting tag */
    start_tag_name = get_inner_container()->get_tag_name();

    /* see if it matches the ending tag */
    if (!get_inner_container()->tag_name_matches(end_tag_name, end_tag_len))
    {
        /* log the error if desired */
        if (log)
        {
            /*
             *   Error - end tag doesn't match current start tag.  Log
             *   the error, but proceed as though it had matched.  
             */
            log_error("end tag </%.*s> doesn't match start tag <%s>",
                      (int)end_tag_len, end_tag_name, start_tag_name);
        }

        /*
         *   If 'find' is true, search the stack for the corresponding open
         *   tag.  If we find it, close the nested tags.  This makes us
         *   tolerant of ill-formed HTML that omits an end tag for a nested
         *   structure. 
         */
        if (find)
        {
            CHtmlTagContainer *open_tag;
            
            /* scan the stack */
            for (open_tag = get_inner_container() ; open_tag != 0 ;
                 open_tag = open_tag->get_container())
            {
                /* if this one matches our end tag, it's the one */
                if (open_tag->tag_name_matches(end_tag_name, end_tag_len))
                    break;
            }

            /* 
             *   if we found the matching open tag, close everything nested
             *   within it 
             */
            if (open_tag != 0)
            {
                /* found it - close everything within our open tag */
                while (get_inner_container() != 0
                       && get_inner_container() != open_tag)
                {
                    /* close the current innermost container */
                    close_current_tag();
                }
            }
        }

        /* return mismatch indication */
        return FALSE;
    }
    else
    {
        /* it matches */
        return TRUE;
    }
}

/*
 *   Skip whitespace and newlines following a tag 
 */
void CHtmlParser::skip_posttag_whitespace()
{
    /*
     *   If we have whitespace followed by a newline, skip up through the
     *   newline -- a newline after a directive doesn't count as
     *   whitespace.  Likewise, keep skipping until we find a line that
     *   contains something other than whitespace.  
     */
    for (;;)
    {
        CCntlenStrPtrSaver oldpos;

        /* save the current position */
        p_.save_position(&oldpos);

        /* skip whitespace */
        while (p_.getlen() != 0)
        {
            /* if it's a newline, stop looking */
            if (p_.curchar() == '\n')
                break;

            /* if this is a space, skip it and keep going */
            if (is_space(p_.curchar()))
                p_.inc();
            else
                break;
        }

        /*
         *   if we're at a newline, eat it and keep going, skipping any
         *   runs of whitespace and newlines that follow; otherwise, go
         *   back to where we were and resume parsing 
         */
        if (p_.getlen() == 0 || p_.curchar() != '\n')
        {
            /* restore the old position and resume parsing */
            p_.restore_position(&oldpos);
            break;
        }

        /* skip the newline and keep going */
        p_.inc();
    }
}

/*
 *   Check that a directive is lexically complete.  If it's not, we'll just
 *   throw it in our pending buffer and return false, so that we can wait for
 *   more text to show up before we attempt to parse the directive.  If it's
 *   complete, we'll return true with the parsing position unchanged.  
 */
int CHtmlParser::check_directive_complete()
{
    CCntlenStrPtr p(p_);

    /*
     *   If we have at least 4k of text remaining in the incoming stream,
     *   then assume it's complete.  Even if it's not actually complete, this
     *   will set a limit on lexically incomplete tags, to avoid waiting
     *   forever if someone forgets a close bracket.  
     */
    if (p.getlen() > 4096)
        return TRUE;

    /* skip the '<' */
    p.inc();

    /* check for a close tag */
    if (p.nextchar() == '/' || p.nextchar() == '\\')
    {
        /* close tag - make sure we have a '>' following */
        while (p.getlen() != 0 && p.curchar() != '>')
            p.inc();

        /* if we found the '>', we're good - tell the caller it's complete */
        if (p.getlen() != 0 && p.curchar() == '>')
            return TRUE;
    }
    else
    {
        textchar_t qu;

        /* scan for the close bracket, noting quoted sequences */
        for (qu = '\0' ; p.getlen() != 0 ; p.inc())
        {
            /* 
             *   if we're in a quoted string, check for the end of the quoted
             *   string; otherwise, check for an open quote or close bracket 
             */
            if (qu != '\0')
            {
                /* in quoted text - check for the matching quote */
                if (p.curchar() == qu)
                {
                    /* 
                     *   it's the close quote - note we're no longer in
                     *   quoted text 
                     */
                    qu = '\0';
                }
            }
            else
            {
                /* check for lexically significant characters */
                switch(p.curchar())
                {
                case '>':
                    /* 
                     *   close bracket - this is what we're looking for, so
                     *   we can simply return success 
                     */
                    return TRUE;

                case '"':
                case '\'':
                    /* it's an open quote - note we're in quoted text */
                    qu = p.curchar();
                    break;
                }
            }
        }
    }

    /*
     *   If we got this far, it means we ran out of source text without
     *   finding the end of the directive.  Move the entire remaining string
     *   to the pending buffer, so that we can try parsing it again when we
     *   get more text.  
     */
    pending_.append(p_.gettext(), p_.getlen());

    /* 
     *   we've made a copy of the source in the pending buffer, so we're
     *   finished for now with the incoming source - skip it all 
     */
    p_.inc(p_.getlen());

    /* tell the caller that the tag is not complete */
    return FALSE;
}

/*
 *   Check that an entity (a '&' sequence) is complete.  If it's not, we'll
 *   move it to our pending buffer and return false, so that we can wait for
 *   more text to show up before we attempt to parse the directive.  If it's
 *   complete, we'll return true with the parsing position unchanged.  
 */
int CHtmlParser::check_entity_complete()
{
    CCntlenStrPtr p(p_);

    /*
     *   Cap entities at 16 characters.  This is bigger than any entity, so
     *   if we reach this limit, something is ill-formed.  The cap ensures
     *   that we don't go on forever waiting for an ill-formed sequence to
     *   end.  
     */
    if (p.getlen() >= 16)
        return TRUE;

    /* skip the '&' */
    p.inc();
    if (p.getlen() == 0)
        goto incomplete;

    /* check for a '#' */
    if (p.curchar() == '#')
    {
        /* it's a numeric entity - skip the '#' */
        p.inc();
        if (p.getlen() == 0)
            goto incomplete;

        /* check for hex or decimal */
        if (p.curchar() == 'x')
        {
            /* it's hex - skip the 'x' */
            p.inc();
            if (p.getlen() == 0)
                goto incomplete;

            /* skip any hex digits */
            while (p.getlen() != 0 && is_hex_digit(p.curchar()))
                p.inc();
        }
        else
        {
            /* it's decimal - skip any decimal digits */
            while (p.getlen() != 0 && is_digit(p.curchar()))
                p.inc();
        }

        /* if we have anything after the digits, the entity is complete */
        if (p.getlen() != 0)
            return TRUE;
    }
    else if (is_alpha(p.curchar()))
    {
        /* 
         *   It's a named entity.  The name is a sequence of alphanumerics,
         *   optionally terminated with a semicolon.  Skip alphanumerics.
         */
        while (p.getlen() != 0
               && (is_alpha(p.curchar()) || is_digit(p.curchar())))
            p.inc();

        /* if we have anything left after the name, the entity is complete */
        if (p.getlen() != 0)
            return TRUE;
    }
    else
    {
        /* it's not a valid entity, so the '&' is complete all by itself */
        return TRUE;
    }

incomplete:
    /*
     *   If we got here, it means we ran out of source text without finding
     *   the end of the entity.  Move the entire remaining string to the
     *   pending buffer, so that we can try parsing it again when we get more
     *   text.  
     */
    pending_.append(p_.gettext(), p_.getlen());

    /* 
     *   we've made a copy of the source in the pending buffer, so we're
     *   finished for now with the incoming source - skip it all 
     */
    p_.inc(p_.getlen());

    /* tell the caller that the tag is not complete */
    return FALSE;
}

/*
 *   Parse the directive we're looking at.  We'll advance past the
 *   closing '>' of the directive.  
 */
void CHtmlParser::parse_directive()
{
    const int   dir_buf_siz = 128;
    textchar_t  directive[dir_buf_siz];
    textchar_t *dir_end;
    int         end_tag;

    /*
     *   if we're not translating markups, parse it literally unless it's
     *   the end tag for the section we're in 
     */
    if (!obey_markups_)
    {
        int parse_as_text;

        /* presume we're going to parse it as text */
        parse_as_text = TRUE;
        
        /* check for end tag if we are obeying end markups */
        if (obey_end_markups_
            && (p_.nextchar() == '/' || p_.nextchar() == '\\'))
        {
            CCntlenStrPtrSaver oldpos;

            /* save the current position */
            p_.save_position(&oldpos);

            /* skip the '</' sequence and read the directive */
            p_.inc(2);
            dir_end = scan_ident(directive, dir_buf_siz);

            /*
             *   restore the old position regardless of what happens - if
             *   we're going to treat it literally, we want to go back to
             *   the start of the tag, and if we're going to parse it,
             *   we're going to go through the full normal parsing, so
             *   we'll still want to be back at the beginning of the tag 
             */
            p_.restore_position(&oldpos);

            /*
             *   see if it's our end tag - if so, we'll want to parse
             *   this as an ordinary tag after all 
             */
            if (end_tag_matches(directive, dir_end - directive, FALSE, FALSE))
                parse_as_text = FALSE;
        }

        /* see if we're going to parse the tag as text */
        if (parse_as_text)
        {
            /* treat it as a literal */
            parse_text();

            /* we're done with it */
            return;
        }
    }

    /* 
     *   check for lexical completeness - if it's not complete, just throw it
     *   in the pending buffer and consider it done 
     */
    if (!check_directive_complete())
        return;

    /* skip the opening '<' */
    p_.inc();

    /* check for comments */
    if (p_.getlen() != 0 && p_.curchar() == '!')
    {
        /* skip the '!' */
        p_.inc();
        
        /* process up to the closing '>' */
        for (;;)
        {
            /* if we're out of text, stop scanning */
            if (p_.getlen() == 0)
                return;

            /* if we're at the closing '>', skip it and we're done */
            if (p_.curchar() == '>')
            {
                /* skip the closing '>' */
                p_.inc();

                /* skip following whitespace */
                skip_posttag_whitespace();

                /* done */
                return;
            }
            
            /* see what we have */
            if (p_.curchar() == '-' && p_.nextchar() == '-')
            {
                oshtml_charset_id_t cs = frame_->get_default_win_charset();

                /* it's a comment - skip up to the next '--' sequence */
                p_.inc(2);
                while (p_.getlen() != 0
                       && p_.curchar() != '>'
                       && (p_.curchar() != '-' || p_.nextchar() != '-'))
                    p_.inc_char(cs);

                /* skip any additional consecutive hyphens */
                while (p_.getlen() != 0 && p_.curchar() == '-')
                    p_.inc();
            }
            else
            {
                /* we have a comment directive */
                // $$$ TBD: process comment directives properly
                //          for now, we'll just ignore them
                p_.inc();
            }

            /* skip any trailing whitespace */
            while (p_.getlen() != 0 && is_space(p_.curchar()))
                p_.inc();
        }
    }

    /* check for an end tag */
    if (p_.getlen() != 0 && (p_.curchar() == '/' || p_.curchar() == '\\'))
    {
        /* note that we're looking at an end tag and skip the slash */
        end_tag = TRUE;
        p_.inc();
    }
    else
    {
        /* not an end tag */
        end_tag = FALSE;
    }
    
    /* scan the directive */
    dir_end = scan_ident(directive, dir_buf_siz);

    /* null-terminate the directive */
    *dir_end = '\0';

    /*
     *   If we're at an end-tag, there can't be any attributes, so skip
     *   up to the closing '>' 
     */
    if (end_tag)
    {
        size_t end_tag_len;
        CHtmlHashEntryTag *entry;

        /* skip whitespace */
        while (p_.getlen() != 0 && is_space(p_.curchar()))
            p_.inc();

        /*
         *   if we're not at a closing '>', it's an error - log it, and
         *   skip up to the closing '>' 
         */
        if (p_.getlen() == 0 || p_.curchar() != '>')
        {
            log_error("closing '>' of end-tag </%s> not found", directive);
            while (p_.getlen() != 0 && p_.curchar() != '>')
                p_.inc();
        }

        /* figure the length of the tag name */
        end_tag_len = dir_end - directive;

        /* find the tag */
        entry = (CHtmlHashEntryTag *)tag_table_->find(directive, end_tag_len);
        if (entry != 0)
        {
            /* process the end tag */
            entry->process_end_tag(this, directive, end_tag_len);
        }
        else
        {
            /*
             *   We don't recognize this tag, so don't close anything -- just
             *   ignore it, as we probably did the opening tag.  Note that
             *   this could be an accidental mismatch with the opening tag,
             *   in which case we'll leave the container open incorrectly,
             *   but this behavior is generally desirable because it causes
             *   unrecognized <tag>...</tag> sequences to be ignored
             *   silently.  
             */
            log_error("unrecognized end tag - </%s> - ignored", directive);
        }
    }
    else
    {
        CHtmlHashEntryTag *entry;
        CHtmlTagContainer *cont;
        CHtmlTag *new_tag;

        /*
         *   start or non-container tag - find the directive in the table
         *   of known directives 
         */
        entry = (CHtmlHashEntryTag *)tag_table_->
                find(directive, dir_end - directive);

        /* if we found it, create a new tag; otherwise, ignore it entirely */
        if (entry)
        {
            /* create a tag object */
            new_tag = entry->create_new_tag(this);
        }
        else
        {
            /* log the error, and otherwise ignore the entire markup */
            log_error("unknown tag <%s>", directive);

            /* skip up to the closing '>' */
            while (p_.getlen() != 0 && p_.curchar() != '>')
                p_.inc();

            /* ...and skip the closing '>' if we found it */
            if (p_.getlen() != 0 && p_.curchar() == '>')
                p_.inc();

            /* done */
            return;
        }

        /*
         *   check with each enclosing container to make sure that this
         *   tag is allowed 
         */
        for (cont = container_ ; cont ; cont = cont->get_container())
        {
            if (!cont->allow_tag(new_tag))
            {
                log_error("tag <%s> not allowed by container", directive);
                delete new_tag;
                return;
            }
        }

        /*
         *   Parse attributes, if any are present.  For each attribute,
         *   we'll scan its name and value, and call the tag to let it
         *   know about the new attribute.  
         */
        for (;;)
        {
            const int     attrib_buf_siz = 128;
            textchar_t    attrib[attrib_buf_siz];
            textchar_t   *attrib_end;
            HTML_attrerr  err;
            CHtmlHashEntryAttr *attr_entry;

            /* skip whitespace */
            while (p_.getlen() && is_space(p_.curchar()))
                p_.inc();

            /* if we're out of text, there aren't any more attributes */
            if (p_.getlen() == 0)
                break;

            /* if we're out of text or at the closing '>', we're done */
            if (p_.getlen() == 0 || p_.curchar() == '>')
                break;

            /* make sure we have an attribute */
            if (!is_alpha(p_.curchar()))
            {
                /* log it */
                log_error("invalid attribute name - must start with a letter");

                /* skip to next whitespace character */
                while (p_.getlen() && !is_space(p_.curchar())
                       && p_.curchar() != '>')
                    p_.inc();

                /* move on to the next attribute */
                continue;
            }

            /* scan the attribute name */
            attrib_end = scan_ident(attrib, attrib_buf_siz);

            /*
             *   Look up the attribute name in the hash table.  If we
             *   find it, we can use its ID to refer to the attribute; if
             *   we don't find it, it's an error.  
             */
            attr_entry = (CHtmlHashEntryAttr *)attr_table_->
                find(attrib, attrib_end - attrib);
            if (attr_entry == 0)
            {
                /* log the error, but go ahead and parse the value part */
                log_error("attribute \"%s\" not valid in tag <%s>",
                          attrib, directive);
            }

            /*
             *   if we're at a '=', we have a value, otherwise we have
             *   just a flag 
             */
            curattr_.clear();
            if (p_.getlen() && p_.curchar() == '=')
            {
                textchar_t  quote;

                /* skip the '=' */
                p_.inc();

                /* see if we have an open quote */
                if (p_.getlen()
                    && (p_.curchar() == '"' || p_.curchar() == '\''))
                {
                    /* remember the quote */
                    quote = p_.curchar();

                    /* skip it */
                    p_.inc();
                }
                else
                {
                    /* no quote */
                    quote = 0;
                }

                /*
                 *   read characters until we find the closing quote, if
                 *   we had an open quote, or a space or '>' 
                 */
                for ( ; p_.getlen() ; )
                {
                    textchar_t c;
                    textchar_t buf[50];
                    size_t len;

                    /* get the next literal character of input */
                    c = p_.curchar();

                    /* see if we're done */
                    if (quote != 0)
                    {
                        /* quoted - done if we're at the closing quote */
                        if (c == quote)
                        {
                            /* skip the quote in the input stream */
                            p_.inc();

                            /* we're done */
                            break;
                        }
                    }
                    else
                    {
                        /* not quoted - done if we're at a space or '>' */
                        if (c == '>' || is_space(c))
                            break;
                    }

                    /* parse the character, which can contain '&' markups */
                    len = parse_char(buf, sizeof(buf), 0, 0, 0);

                    /* not done - add this character to the value buffer */
                    curattr_.append(buf, len);
                }
            }
            else
            {
                /* set the attribute value to the name of the attribute */
                curattr_.append(attrib, attrib_end - attrib);
            }

            /* if the attribute name was invalid, skip this attribute */
            if (attr_entry == 0)
                continue;

            /* Set this attribute in the tag */
            err = new_tag->set_attribute(this, attr_entry->id_,
                                         curattr_.getbuf(), curattr_.getlen());
            switch(err)
            {
            case HTML_attrerr_ok:
                /* no problem */
                break;

            case HTML_attrerr_inv_attr:
                /* invalid attribute for this tag */
                log_error("attribute \"%s\" not valid for tag <%s>",
                          attrib, directive);
                break;

            case HTML_attrerr_inv_type:
                /* invalid type for attribute value */
                log_error("invalid value type for attribute \"%s\" in "
                          "tag <%s>", attrib, directive);
                break;

            case HTML_attrerr_inv_enum:
                /* value doesn't match allowed choices */
                log_error("value for attribute \"%s\" in tag <%s> doesn't "
                          "match any of the allowed values",
                          attrib, directive);
                break;

            case HTML_attrerr_out_of_range:
                /* value is out of range */
                log_error("value for attribute \"%s\" in tag <%s> "
                          "is out of range", attrib, directive);
                break;

            case HTML_attrerr_too_many_coords:
                log_error("too many coordinates given in POLY COORDS");
                break;

            case HTML_attrerr_odd_coords:
                log_error("even number of coordinates required in POLY");
                break;

            case HTML_attrerr_missing_delim:
                log_error("missing delimiter in value of attribute \"%s\" "
                          "in tag <%s>", attrib, directive);
                break;
            }
        }

        /*
         *   The tag looks good.  Add the current block of text to its
         *   container as a new text tag, then inform the new tag that
         *   it's been parsed.  
         */
        add_text_tag();
        new_tag->on_parse(this);
    }

    /*
     *   if we're at the closing '>', skip it; otherwise, we have an
     *   incomplete tag at the end of the text, which is an error 
     */
    if (p_.curchar() == '>')
    {
        /* well-formed tag - skip the closing '>' */
        p_.inc();

        /* skip whitespace after the tag */
        skip_posttag_whitespace();
    }
    else
    {
        /* incomplete tag at the end of the document - ignore it entirely */
        log_error("incomplete tag at end of document");
    }
}

/*
 *   Process a normal end tag.  
 */
void CHtmlParser::end_normal_tag(const textchar_t *tag, size_t len)
{
    /*
     *   Find the matching container and call its pre-close method.  This
     *   allows close tags that implicitly close other tags to do the
     *   implicit closing before we decide whether we're at the matching
     *   level.  
     */
    pre_close_tag(tag, len);

    /*
     *   Make sure it matches the most recent start tag on the container
     *   stack. If so, pop the start tag; otherwise, it's an error.  
     */
    end_tag_matches(tag, len, TRUE, TRUE);

    /*
     *   If we only have one container in the stack, it's the special outer
     *   container, which we can't pop - they must have specified more end
     *   tags than start tags.  Log it as an error and ignore the end tag.  
     */
    if (get_container_depth() == 1)
    {
        /* log the error and proceed */
        log_error("too many end tags - </%s> ignored", tag);
    }
    else
    {
        /* close the current tag */
        close_current_tag();
    }
}

/*
 *   Process a closing </P> tag.  We don't treat <P> as a true container, so
 *   we will never have an outstanding container for a </P> tag to match.
 *   So, whenever we see </P>, simply treat it as a paragraph break and
 *   otherwise ignore it.
 *   
 *   Also treat the </BODY> tag specially.  Our <BODY> tag isn't a container
 *   as it is in standard HTML, so we don't need a matching end tag.
 *   However, allow the end tag by simply ignoring it.  
 */
void CHtmlParser::end_p_tag()
{
    /* add the current text block as a new text tag */
    add_text_tag();
    
    /* end the current paragraph, and otherwise ignore the </P> */
    end_paragraph(FALSE);
}

/*
 *   Pre-close a tag.  Find a matching container, and call its pre-close
 *   method.  This allows close tags that implicitly close other tags to
 *   do the implicit closing before we decide whether we have a matching
 *   tag.  
 */
void CHtmlParser::pre_close_tag(const textchar_t *nm, size_t nmlen)
{
    CHtmlTagContainer *tag;
    
    /* look up the container list for a matching tag */
    for (tag = get_inner_container() ; tag != 0 ;
         tag = tag->get_container())
    {
        /* check if this is a matching tag */
        if (tag->tag_name_matches(nm, nmlen))
        {
            /* let this tag do its close recognition work */
            tag->pre_close(this);

            /* no need to look further */
            return;
        }
    }
}

/*
 *   Close a tag if it's open 
 */
void CHtmlParser::close_tag_if_open(const textchar_t *nm)
{
    size_t nmlen = get_strlen(nm);

    /* if the immediate container matches this tag, close it */
    if (get_inner_container()->tag_name_matches(nm, nmlen))
    {
        /* this is the one we want - close it */
        close_current_tag();
    }
}

/*
 *   Close the current tag 
 */
void CHtmlParser::close_current_tag()
{
    CHtmlTagContainer *closing_tag;
    
    /* add the current text block as a new text tag */
    add_text_tag();

    /* tell the current container it's closing */
    closing_tag = get_inner_container();
    closing_tag->on_close(this);

    /* pop the innermost container */
    pop_inner_container();

    /* tell the container we just closed it */
    closing_tag->post_close(this);
}

/*
 *   Look up an attribute value in the enumerated attribute value list.
 *   Returns an attribute ID if the value matches one of the enumerated
 *   values.
 */
HTML_Attrib_id_t CHtmlParser::attrval_to_id(const textchar_t *val,
                                            size_t vallen)
{
    CHtmlHashEntryAttr *entry;

    entry = (CHtmlHashEntryAttr *)attr_val_table_->find(val, vallen);
    return (entry ? entry->id_ : HTML_Attrib_invalid);
}


/*
 *   Add the current text stream to the parse tree as a text tag 
 */
void CHtmlParser::add_text_tag()
{
    CHtmlTagText *text_tag;
    
    /* if there's no text, don't bother adding a new tag */
    if (curtext_.getlen() == 0)
        return;

    /* make a new tag for the text */
    text_tag = (obey_whitespace_ && !break_long_lines_)
               ? new CHtmlTagTextPre(8, text_array_, &curtext_)
               : new CHtmlTagText(text_array_, &curtext_);

    /* add it to the parse tree */
    append_tag(text_tag);

    /* clear the text buffer */
    curtext_.clear();
}

/*
 *   Parse a hard tab character 
 */
void CHtmlParser::parse_tab()
{
    if (obey_whitespace_)
    {
        CHtmlTagTAB *tab;
        
        /* skip the tab character in the input */
        p_.inc();

        /* flush the current text */
        add_text_tag();

        /* create a <TAB MULTIPLE=4> tag */
        tab = new CHtmlTagTAB(this);
        tab->set_multiple_val(4);
        tab->on_parse(this);
    }
    else
    {
        /* not in preformatted text - treat as ordinary whitespace */
        parse_whitespace();
    }
}

/*
 *   Parse whitespace 
 */
void CHtmlParser::parse_whitespace()
{
    /*
     *   if we're obeying whitespace literally, simply add this character
     *   to the text stream directly 
     */
    if (obey_whitespace_)
    {
        parse_text();
        return;
    }

    /* skip past any additional contiguous whitespace */
    while (p_.getlen() > 0 && is_space(p_.curchar()))
        p_.inc();

    /*
     *   If the last character of the text stream was already whitespace,
     *   ignore this one; otherwise, add it. 
     */
    if (!eat_whitespace_)
    {
        /* add a whitespace character to the text */
        append_to_text(' ');

        /* eat whitespace until we get something else */
        eat_whitespace_ = TRUE;
    }
}

/*
 *   Parse a newline character.  For the most part, newlines are treated
 *   as whitespace.  In preformatted text, newlines count as line breaks.
 */
void CHtmlParser::parse_newline()
{
    /* if obeying whitespace literally, treat this as a line break */
    if (obey_whitespace_)
    {
        /* skip the newline */
        p_.inc();
        
        /* add current text block as a new text tag */
        add_text_tag();

        /* add a line break tag */
        append_tag(new CHtmlTagBR(this));

        /* done */
        return;
    }

    /* otherwise, treat it as any other whitespace */
    parse_whitespace();
}

/*
 *   Parse text 
 */
void CHtmlParser::parse_text()
{
    textchar_t buf[50];
    size_t len;
    oshtml_charset_id_t charset;
    int changed_charset;
    unsigned int special;

    /* 
     *   If our text buffer is getting big, flush it - this will ensure that
     *   we won't create gigantic text display items, and that we won't
     *   overflow the 16-bit length limit of an individual text display
     *   item. 
     */
    if (curtext_.getlen() > 30000)
        add_text_tag();

    /* get the current character */
    len = parse_char(buf, sizeof(buf), &charset, &changed_charset, &special);

    /* if it's a special unicode character, handle it specially */
    if (special != 0)
    {
        size_t len;
        int spwid;
        
        /*
         *   Most of the special space characters subsume any ordinary
         *   whitespace that precedes them.  If we have a trailing space in
         *   our pending text buffer, and we have one of these special space
         *   characters, remove the trailing space.  
         */
        if ((len = curtext_.getlen()) != 0
            && curtext_.getbuf()[len - 1] == ' ')
        {
            /* check for a special space character */
            switch(special)
            {
            case 0x0015:                            /* our own quoted space */
            case 0x2002:                                        /* en space */
            case 0x2003:                                        /* em space */
            case 0x2004:                              /* three-per-em space */
            case 0x2005:                               /* four-per-em space */
            case 0x2006:                                /* six-per-em space */
            case 0x2007:                                    /* figure space */
            case 0x2008:                               /* punctuation space */
            case 0x2009:                                      /* thin space */
            case 0x200A:                                      /* hair space */
                /* 
                 *   These all subsume adjacent space - remove the trailing
                 *   space from the pending text.  Note that we can assume
                 *   that we will have at most one trailing space, since we
                 *   always collapse runs of whitespace as we build the
                 *   pending text buffer in the first place.  
                 */
                curtext_.setlen(len - 1);
                break;
            }
        }

        /*
         *   Whatever we have next will require a separate element to
         *   represent, so send the pending text to the tag list as a
         *   text-run element.  
         */
        add_text_tag();

        /* 
         *   presume that this special character doesn't subsume any
         *   immediately following whitespace 
         */
        eat_whitespace_ = FALSE;

        /* now check the special character and add the appropriate tag */
        switch(special)
        {
        case 0x00AD:                                         /* soft hyphen */
            /* add a soft-hyphen tag */
            append_tag(new CHtmlTagSoftHyphen(text_array_));
            break;
            
        case 0x00A0:                                  /* non-breaking space */
            /* add a non-breaking space tag with a width of one space */
            append_tag(new CHtmlTagNBSP(text_array_, " "));
            break;

        case 0xFEFF:                       /* zero-width non-breaking space */
            /* add a non-breaking space tag with a zero width */
            append_tag(new CHtmlTagNBSP(text_array_, ""));
            break;

        case 0x200B:                                    /* zero-width space */
            /* add a zero-width space */
            spwid = 0;
            goto add_space_no_eat;

        case 0x0015:                                /* our own quoted space */
            /* add a space with a width of one ordinary space */
            spwid = HTMLTAG_SPWID_ORDINARY;
            goto add_space;
            
        case 0x2002:                                            /* en space */
            /* 
             *   an en space is 1/2 em (we represent fractions of an em by
             *   using the denominator of the fractional em size: 2
             *   indicates 1/2 em, 3 is 1/3 em, and so on) 
             */
            spwid = 2;
            goto add_space;
            
        case 0x2003:                                            /* em space */
            /* use a full em space */
            spwid = 1;
            goto add_space;
            
        case 0x2004:                                  /* three-per-em space */
            /* use a 1/3 em space */
            spwid = 3;
            goto add_space;

        case 0x2005:                                   /* four-per-em space */
            /* use a 1/4 em space */
            spwid = 4;
            goto add_space;
            
        case 0x2006:                                    /* six-per-em space */
            /* use a 1/6 em space */
            spwid = 6;
            goto add_space;
            
        case 0x2007:                                        /* figure space */
            /* use the special figure space indicator */
            spwid = HTMLTAG_SPWID_FIGURE;
            goto add_space;

        case 0x2008:                                   /* punctuation space */
            /* use the special punctuation space indicator */
            spwid = HTMLTAG_SPWID_PUNCT;
            goto add_space;
            
        case 0x2009:                                          /* thin space */
            /* use a 1/5 em space */
            spwid = 5;
            goto add_space;
            
        case 0x200A:                                          /* hair space */
            /* use a 1/8 em space */
            spwid = 8;
            goto add_space;

        add_space:
            /* 
             *   consolidate any subsequent whitespace with this space - most
             *   of the typographical spaces (other than the zero-width
             *   spaces) subsume adjacent ordinary whitespace 
             */
            eat_whitespace_ = TRUE;

        add_space_no_eat:
            /* add the space tag */
            append_tag(new CHtmlTagSpace(text_array_, spwid));

            /* done */
            break;
        }
    }
    else
    {
        /* 
         *   if necessary, generate a FONT tag to switch to the correct
         *   character set - we only need to do this if the character set of
         *   the character is different than the default frame character set 
         */
        if (changed_charset)
        {
            /* flush the text output */
            add_text_tag();
            
            /* generate a character set tag for the switch */
            push_container(new CHtmlTagCharset(charset));
        }
        
        /* add this character to our current text stream */
        curtext_.append(buf, len);
        
        /* switch back to our old character set if necessary */
        if (changed_charset)
        {
            /* flush the text output */
            add_text_tag();
            
            /* close the character set tag */
            close_current_tag();
        }
    
        /*
         *   if this isn't a whitespace character, we'll want to include the
         *   next whitespace character we see into the text 
         */
        if (len != 0 && !is_space(buf[len-1]))
            eat_whitespace_ = FALSE;
    }
}

/*
 *   Add text to the current buffer
 */
void CHtmlParser::append_to_text(textchar_t c)
{
    /* flush the text buffer if it's getting big */
    if (curtext_.getlen() > 30000)
        add_text_tag();

    /* add the text */
    curtext_.append(&c, 1);
}


/*
 *   Break the current paragraph 
 */
void CHtmlParser::end_paragraph(int isexplicit)
{
    /* add the current text to the output stream */
    add_text_tag();

    /* add a plain paragraph tag */
    append_tag(new CHtmlTagP(isexplicit, this));
}


/*
 *   Process a piece of text, appending it to the end of the HTML stream
 *   processed so far. 
 */
void CHtmlParser::parse(const CHtmlTextBuffer *src, CHtmlSysWinGroup *frame)
{
    CHtmlTextBuffer new_src;

    /* 
     *   If we have pending buffered text, add this text to the pending
     *   buffer, and parse from the pending buffer.  
     */
    if (pending_.getlen() != 0)
    {
        /* make a working copy of the pending source */
        new_src.append(pending_.getbuf(), pending_.getlen());

        /* add the new text to the working copy */
        new_src.append(src->getbuf(), src->getlen());

        /* 
         *   the pending text is now our working text, so it's no longer
         *   pending: clear the pending buffer 
         */
        pending_.clear();

        /* parse from the new working buffer */
        src = &new_src;
    }

    /* start at the beginning of the buffer */
    p_.set(src->getbuf(), src->getlen());
    p_start_.set(src->getbuf(), src->getlen());

    /* remember the system frame (for use in lower-level routines) */
    frame_ = frame;
    
    /* keep going until we run out of text in the buffer */
    for (;;)
    {
        /* if we're out of text, we're done */
        if (p_.getlen() == 0)
            break;

        /* see what we have */
        switch(p_.curchar())
        {
        case '<':
            /* start of a directive */
            parse_directive();
            break;

        case ' ':
            /* regular whitespace */
            parse_whitespace();
            break;

        case '\t':
            /* hard tab character */
            parse_tab();
            break;

        case '\n':
        case '\r':
            parse_newline();
            break;
            
        default:
            /* anything else is to be added literally */
            parse_text();
            break;
        }
    }

    /* flush any text that we've gathered up */
    add_text_tag();
}

/*
 *   Push a container tag
 */
void CHtmlParser::push_container(CHtmlTagContainer *tag)
{
    /* add this object to the current container's list */
    container_->append_to_sublist(tag);

    /* this is the new innermost container */
    container_ = tag;

    /* increase the depth counter */
    ++container_depth_;
}

/*
 *   Pop the innermost container off the stack 
 */
void CHtmlParser::pop_inner_container()
{
    /*
     *   if there's no container, or the container doesn't have a
     *   container, we can't pop anything -- we can never pop the special
     *   outermost container 
     */
    if (container_ == 0 || container_->get_container() == 0)
        return;
    
    /* make the current object's container the new innermost container */
    container_ = container_->get_container();

    /* count the change in depth */
    --container_depth_;
}

/*
 *   Fix up a trailing container close 
 */
void CHtmlParser::fix_trailing_cont()
{
    /*
     *   If the last tag we added is a container, add a "relax" tag, so that
     *   we have a non-container to land on after traversing out of the
     *   container. 
     */
    if (get_inner_container() != 0
        && get_inner_container()->get_last_contents() != 0
        && get_inner_container()->get_last_contents()->is_container_tag())
    {
        /* we need a "relax" tag - create a new one and append it */
        get_inner_container()->append_to_sublist(new CHtmlTagRelax());
    }
}

/*
 *   Append an item to the current innermost container's sublist 
 */
void CHtmlParser::append_tag(CHtmlTag *tag)
{
    get_inner_container()->append_to_sublist(tag);
}

/*
 *   Append a new input tag 
 */
CHtmlTagTextInput *CHtmlParser::append_input_tag(const textchar_t *buf,
                                                 size_t len)
{
    CHtmlTagTextInput *new_tag;

    /* create the input tag */
    new_tag = new CHtmlTagTextInput(text_array_, buf, len);

    /* add it to our list */
    get_inner_container()->append_to_sublist(new_tag);

    /* return the new tag */
    return new_tag;
}

/*
 *   Find the outermost container 
 */
CHtmlTagContainer *CHtmlParser::get_outer_container() const
{
    CHtmlTagContainer *cont;

    /*
     *   scan outward from the inner container until we find the
     *   outermost one 
     */
    for (cont = container_ ; cont->get_container() != 0 ;
         cont = cont->get_container()) ;

    /* return what we found */
    return cont;
}

/*
 *   Export the parse tree 
 */
void CHtmlParser::export_parse_tree(CHtmlParserState *state)
{
    /* save my state in the state object */
    state->text_array_ = text_array_;
    state->container_ = container_;
    state->outer_container_ = get_outer_container();
    state->container_depth_ = container_depth_;

    /* forget my state information */
    text_array_ = 0;
    container_ = 0;

    /* create a new text array */
    text_array_ = new CHtmlTextArray;

    /* create a new outermost container */
    container_ = new CHtmlTagOuter;
    container_depth_ = 1;

    /* reset internal variables to start a new page */
    clear_page();
}

/*
 *   Import a parse tree 
 */
void CHtmlParser::import_parse_tree(CHtmlParserState *state)
{
    /* discard my current tags */
    clear_page();
    if (container_ != 0)
        delete container_;

    /* discard my current text array */
    if (text_array_ != 0)
        delete text_array_;

    /* load the information from the state structure */
    text_array_ = state->text_array_;
    container_ = state->container_;
    container_depth_ = state->container_depth_;

    /* clear out the state structure - we own the state now */
    state->text_array_ = 0;
    state->container_ = 0;
    state->outer_container_ = 0;
}

/*
 *   Destroy a parse tree.
 */
void CHtmlParser::destroy_parse_tree(CHtmlParserState *state)
{
    /* delete the text array */
    if (state->text_array_ != 0)
        delete state->text_array_;

    /* delete the tags */
    if (state->outer_container_ != 0)
    {
        /* delete the top container's children */
        state->outer_container_->delete_contents();

        /* delete the top container */
        delete state->outer_container_;
    }

    /* clear the state structure */
    state->text_array_ = 0;
    state->outer_container_ = 0;
    state->container_ = 0;
}

/*
 *   Prune the parse tree by deleting the given percentage of top-level
 *   nodes.  
 */
void CHtmlParser::prune_tree(unsigned long max_text_array_size)
{
    CHtmlTag *cur;
    CHtmlTag *prv;
    CHtmlTag *nxt;
    long top_cnt;

    /*
     *   Count the top-level tags.  Always try to keep around at least a
     *   few of them, so that we don't leave the window entirely blank. 
     */
    for (cur = get_outer_container()->get_contents(), top_cnt = 0 ;
         cur != 0 ; cur = cur->get_next_tag(), ++top_cnt) ;

    /* 
     *   Run through the list and delete each top-level node that we can
     *   safely delete.  Keep deleting until we run out of things to
     *   delete, or the text array is no larger than the given size.  
     */
    for (cur = get_outer_container()->get_contents(), prv = 0 ;
         cur != 0 ; cur = nxt)
    {
        int can_delete;
        int can_prune_contents;
        
        /* presume we can delete this tag and prune its contents */
        can_delete = can_prune_contents = TRUE;
        
        /* 
         *   check to see if we've pruned enough space yet - if so, don't
         *   bother deleting this one or pruning its contents
         */
        if (get_text_array()->get_mem_in_use() <= max_text_array_size)
            can_delete = can_prune_contents = FALSE;

        /* stop if we're running too low on top-level tags */
        if (top_cnt < 125)
            can_delete = can_prune_contents = FALSE;
        
        /* remember the next node in line, in case we delete this one */
        nxt = cur->get_next_tag();

        /* 
         *   if this is our last tag, always keep it and its contents, so
         *   that we have something still around after we're done 
         */
        if (nxt == 0)
            can_delete = can_prune_contents = FALSE;

        /* 
         *   If we still think we can delete the tag, check with the tag
         *   itself to see if it can be deleted.  If it can't be deleted,
         *   so note; but we might still allow pruning its contents.  
         */
        if (can_delete && !cur->can_prune_tag())
            can_delete = FALSE;

        /* 
         *   If we can delete this one, do so.  Otherwise, go through its
         *   contents and try pruning the contents, if desired.  
         */
        if (can_delete)
        {
            /* adjust the previous link */
            if (prv != 0)
                prv->set_next_tag(nxt);
            else
                get_outer_container()->set_contents(nxt);

            /* notify the tag of the pruning */
            cur->prune_pre_delete(get_text_array());

            /* delete this tag */
            delete cur;

            /* that's one less top-level tag remaining */
            --top_cnt;
        }
        else
        {
            /* we can't delete this one, so it'll be the next 'prv' */
            prv = cur;

            /* tell the tag to prune its contents, if necessary */
            if (can_prune_contents)
                cur->try_prune_contents(get_text_array());
        }
    }
}


/*
 *   Log an error 
 */
void CHtmlParser::log_error(const textchar_t *msg, ...)
{
    va_list argptr;
    size_t curofs;
    size_t startofs;
    CCntlenStrPtr ctxp;
    textchar_t buf[80];
    textchar_t *dst;

    /* 
     *   get some context - go back from the current point by 50
     *   characters if possible, and display from that point to the
     *   current point plus 30 characters 
     */
    curofs = p_start_.getlen() - p_.getlen();
    startofs = (curofs > 50 ? curofs - 50 : 0);
    ctxp.set(&p_start_);
    ctxp.inc(startofs);
    for (dst = buf ; dst < buf + sizeof(buf) - 1 && ctxp.getlen() > 0 ;
         ctxp.inc())
        *dst++ = ctxp.curchar();
    *dst = '\0';

    /* display the context */
    oshtml_dbg_printf("==============================\n%s\n", buf);

    /* display a pointer to the current position */
    for (dst = buf ; dst < buf + sizeof(buf) - 1 && startofs < curofs ;
         *dst++ = ' ', ++startofs) ;
    *dst = '\0';
    oshtml_dbg_printf("%s^\n", buf);

    /* display the error message */
    va_start(argptr, msg);
    oshtml_dbg_printf("Error: ");
    oshtml_dbg_vprintf(msg, argptr);
    oshtml_dbg_printf("\n");
    va_end(argptr);
}

#ifdef TADSHTML_DEBUG
/*
 *   dump tree to stdout for debugging purposes 
 */
void CHtmlParser::debug_dump()
{
    /* dump the outermost container */
    get_outer_container()->debug_dump(0, text_array_);
}
#endif


#if 0
/* ------------------------------------------------------------------------ */
/*
 *   TEST SECTION 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "htmlfmt.h"
#include "htmlsys_w32.h"

int main(int argc, char **argv)
{
    FILE *fp;
    CHtmlTextBuffer *buf;
    CHtmlParser *parser;
    CHtmlFormatter *formatter;
    CHtmlSysWin *win;

    /* ensure they specified a file */
    if (argc < 2)
    {
        printf("usage: %s filename\n", argv[0]);
        exit(1);
    }

    /* open the file */
    fp = fopen(argv[1], "r");
    if (fp == 0)
    {
        printf("can't open input file\n");
        exit(2);
    }

    /* create a text buffer */
    buf = new CHtmlTextBuffer;

    /* read the file and load it into our text buffer */
    for (;;)
    {
        char lin[256];

        /* read the next line */
        if (!fgets(lin, sizeof(lin), fp))
            break;

        /* load the line into the text buffer */
        buf->append(lin, strlen(lin));
    }

    /* run it through the parser */
    parser = new CHtmlParser;
    parser->parse(buf, 0);

    /* dump the parse tree */
    parser->debug_dump();

    /* run it through the formatter */
    win = new CHtmlSysWin_win32();
    formatter = new CHtmlFormatter(parser, win);
    while (formatter->more_to_do())
        formatter->do_formatting();
    formatter->start_new_line(0);

    /* done with the parser */
    delete parser;

    /* done with the formatter */
    delete formatter;

    /* done with the buffer */
    delete buf;
    return 0;
}

#endif

