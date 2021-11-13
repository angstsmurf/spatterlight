/* $Header: d:/cvsroot/tads/html/htmldcfg.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldcfg.h - debugger configuration reader/writer
Function
  This is a simple utility object that allows saving and restoring
  debugger state information.

  Information is stored as a set of name/value pairs.  Each value can
  be a string, list of strings, rectangle, or boolean value.  Each name
  can have a single associated value, or can have an array of associated
  values.  Array items are addressed by an element name (rather than
  an index number).

  The configuration information is kept in memory while running.  The
  information can be loaded from a file or saved to a file at any time;
  normally, a configuration file would be loaded at startup and written
  at program exit.
Notes
  
Modified
  03/05/98 MJRoberts  - Creation
*/

#ifndef HTMLDCFG_H
#define HTMLDCFG_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

#include "htmlhash.h"


/* maximum length of a key name */
const int HTML_DCFG_KEY_MAX = 512;

class CHtmlDebugConfig
{
public:
    CHtmlDebugConfig();
    ~CHtmlDebugConfig();

    /* 
     *   Load a file; replaces all current configuration information.
     *   Returns zero on success, non-zero no failure. 
     */
    int load_file(const textchar_t *fname);

    /* 
     *   Write a file.  Returns zero on success, non-zero on failure. 
     */
    int save_file(const textchar_t *fname);

    /*
     *   Load a TADS 3 makefile (a .t3m file).  This loads a configuration
     *   that was stored using TADS 3 compiler makefile format rather than
     *   our private binary format.  'sys_lib_path' is the path to the system
     *   libraries, which we need in order to determine whether source files
     *   and library files come from the system library directory or from the
     *   user directory.  
     */
    int load_t3m_file(const textchar_t *fname,
                      const char *sys_lib_path);

    /*
     *   Save a TADS 3 makefile (a .t3m file).  This saves the configuration
     *   data in the TADS 3 compiler makefile format rather than our private
     *   binary format. 
     */
    int save_t3m_file(const textchar_t *fname);

    /*
     *   Store values.  The variable name and the element ID together form
     *   the key; the element ID can be null.  If no variable of the given
     *   name exists, one is created.  If a variable of this name already
     *   exists, its value is replaced.  
     */
    void setval(const textchar_t *varname, const textchar_t *element,
                int val);
    void setval(const textchar_t *varname, const textchar_t *element,
                int stridx, const textchar_t *val);
    void setval(const textchar_t *varname, const textchar_t *element,
                int stridx, const textchar_t *val, size_t vallen);
    void setval(const textchar_t *varname, const textchar_t *element,
                const CHtmlRect *val);
    void setval_color(const textchar_t *varname, const textchar_t *element,
                      HTML_color_t color);
    void setval_bytes(const textchar_t *varname, const textchar_t *element,
                      const void *buf, size_t len);

    /* clear a string list - removes all elements from the list */
    void clear_strlist(const textchar_t *varname, const textchar_t *element);

    /* get the number of elements in a string list */
    int get_strlist_cnt(const textchar_t *varname, const textchar_t *element);

    /* 
     *   Retrieve values.  These routines return zero on success, non-zero
     *   on failure.  The call fails if the named value does not exist or
     *   is not of the requested type.  
     */
    int getval(const textchar_t *varname, const textchar_t *element,
               int *val);
    int getval(const textchar_t *varname, const textchar_t *element,
               int stridx, textchar_t *dstbuf, size_t dstbuflen)
    {
        return getval(varname, element, stridx, dstbuf, dstbuflen, 0);
    }
    
    int getval(const textchar_t *varname, const textchar_t *element,
               int stridx, textchar_t *dstbuf, size_t dstbuflen,
               size_t *result_len);
    int getval(const textchar_t *varname, const textchar_t *element,
               CHtmlRect *val);
    int getval_color(const textchar_t *varname, const textchar_t *element,
                     HTML_color_t *color);
    
    int getval_bytes(const textchar_t *varname, const textchar_t *element,
                     void *buf, size_t buflen);
    int getval_bytes_len(const textchar_t *varname,
                         const textchar_t *element, size_t *len);
    int getval_bytes_buf(const textchar_t *varname,
                         const textchar_t *element, void **bufptr);

    /* delete a value */
    void delete_val(const textchar_t *varname, const textchar_t *element);

protected:
    /* find the extension in a URL-style filename */
    char *CHtmlDebugConfig::find_url_ext(const char *url);

    /* hash table entry enumeration callback for saving to a file */
    static void save_enumcb(void *ctx, class CHtmlHashEntry *entry);

    /* 
     *   Get the entry with a given hash key.  If create is true, we'll
     *   create a new entry if one doesn't exist.
     */
    class CHtmlHashEntryConfig *get_entry(const textchar_t *varname,
                                          const textchar_t *element,
                                          int create);

    /* get an entry from a given hash table */
    class CHtmlHashEntryConfig *get_entry_from(class CHtmlHashTable *hashtab,
                                               const textchar_t *varname,
                                               const textchar_t *element,
                                               int create);

    /* get a variable's entry for saving in a .t3m file, marking it saved */
    class CHtmlHashEntryConfig *t3m_get_entry_for_save(
        const char *var, const char *ele);

    /* copy a configuration variable's string list to a .t3m file */
    void t3m_copy_to_file(osfildef *fp, const char *var, const char *ele);

    /* save a .t3m string list option */
    void t3m_save_option_strs(osfildef *fp, const char *var, const char *ele,
                              const char *opt, int cvt_to_url,
                              const char *base_path_url);

    /* save a .t3m boolean option */
    void t3m_save_option_bool(osfildef *fp, const char *var, const char *ele,
                              const char *opt_on, const char *opt_off);

    /* save a .t3m integer option */
    void t3m_save_option_int(osfildef *fp, const char *var, const char *ele,
                             const char *opt);

    /* write a .t3m option argument, quoting it if necessary */
    void t3m_save_write_arg(osfildef *fp, const char *arg);

    /* 
     *   set a string variable in local notation given a portable URL-style
     *   path; if the result isn't an absolute path, expand it relative to
     *   the given local base path 
     */
    void setval_rel_url(const char *var, const char *subvar, int idx,
                        const char *val, const char *base_path);

    /* hash table containing the values */
    class CHtmlHashTable *hashtab_;

    /* file signature */
    static const textchar_t filesig[];
};

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry subclass for debugger configuration entries.  These
 *   types are private to the configuration manager class.  
 */

/* datatypes for table entries */
enum html_dcfg_type_t
{
    HTML_DCFG_TYPE_EOF,              /* special type marker for end of file */
    HTML_DCFG_TYPE_NULL,                               /* no value assigned */
    HTML_DCFG_TYPE_STRLIST,                              /* list of strings */
    HTML_DCFG_TYPE_INT,                                          /* integer */
    HTML_DCFG_TYPE_RECT,                                       /* rectangle */
    HTML_DCFG_TYPE_COLOR,                                          /* color */
    HTML_DCFG_TYPE_BYTES                                  /* raw byte array */
};

/* linked list entry for a string list member */
class CHtmlDcfgString
{
public:
    CHtmlDcfgString()
    {
        next_ = 0;
    }

    /* contents of the string */
    CStringBuf str_;

    /* next string in list */
    CHtmlDcfgString *next_;
};

/*
 *   Hash table entry for a configuration variable 
 */
class CHtmlHashEntryConfig: public CHtmlHashEntryCS
{
public:
    CHtmlHashEntryConfig(const textchar_t *str, size_t len, int copy)
        : CHtmlHashEntryCS(str, len, copy)
    {
        init(HTML_DCFG_TYPE_NULL);
    }

    CHtmlHashEntryConfig(const textchar_t *str, size_t len, int copy,
                         CHtmlDcfgString *strlist)
        : CHtmlHashEntryCS(str, len, copy)
    {
        init(HTML_DCFG_TYPE_STRLIST);
        val_.strlist_ = strlist;
    }

    CHtmlHashEntryConfig(const textchar_t *str, size_t len, int copy,
                         int intval)
        : CHtmlHashEntryCS(str, len, copy)
    {
        init(HTML_DCFG_TYPE_INT);
        val_.int_ = intval;
    }

    CHtmlHashEntryConfig(const textchar_t *str, size_t len, int copy,
                         const CHtmlRect *rectval)
        : CHtmlHashEntryCS(str, len, copy)
    {
        init(HTML_DCFG_TYPE_RECT);
        val_.rect_ = new CHtmlRect();
        val_.rect_->set(rectval);
    }

    ~CHtmlHashEntryConfig();

    /*
     *   Basic initialization.  Set the type and initialize variables.  
     */
    void init(html_dcfg_type_t typ)
    {
        /* remember the type */
        type_ = typ;

        /* initialize our status to zero arbitrarily */
        status_ = 0;
    }

    /*
     *   Read an entry from a file.  Creates a new entry object based on the
     *   data in the file.  Returns zero on success, non-zero on error.  When
     *   we reach the end-of-file marker, we'll return success, but the new
     *   entry will be null.  
     */
    static int read_from_file(osfildef *fp, CHtmlHashEntryConfig **new_entry);

    /* 
     *   Write the entry to a file.  Returns zero on success, non-zero on
     *   failure.  
     */
    int write_to_file(osfildef *fp);

    /*
     *   Set a new value, replacing any existing value 
     */

    void setval(int intval)
    {
        change_type(HTML_DCFG_TYPE_INT);
        val_.int_ = intval;
    }

    void setval(int stridx, const textchar_t *val)
    {
        setval(stridx, val, get_strlen(val));
    }

    void setval(int stridx, const textchar_t *val, size_t vallen);

    void setval(const CHtmlRect *rectval)
    {
        change_type(HTML_DCFG_TYPE_RECT);
        val_.rect_->set(rectval);
    }

    void setval_color(HTML_color_t color)
    {
        change_type(HTML_DCFG_TYPE_COLOR);
        val_.color_ = color;
    }

    void setval_bytes(const void *buf, size_t buflen)
    {
        setval_bytes(buflen);
        memcpy(val_.bytes_.buf_, buf, buflen);
    }

    void setval_bytes(size_t buflen)
    {
        delete_value();
        change_type(HTML_DCFG_TYPE_BYTES);
        val_.bytes_.buf_ = th_malloc(buflen);
        val_.bytes_.len_ = buflen;
    }

    /* clear all list elements out of a string list item */
    void clear_strlist()
    {
        if (type_ == HTML_DCFG_TYPE_STRLIST)
            delete_value();
    }

    /* get the number of elements in a string list */
    int get_strlist_cnt();

    /*
     *   Get the current value.  Returns zero on success, non-zero on
     *   failure.  Fails if the value is not of the requested type.  
     */
    int getval(int *intval)
    {
        /* make sure the type's correct */
        if (type_ != HTML_DCFG_TYPE_INT)
            return 1;

        /* set the value and return success */
        *intval = val_.int_;
        return 0;
    }

    int getval(int stridx, textchar_t *dstbuf, size_t dstbuflen)
    {
        return getval(stridx, dstbuf, dstbuflen, 0);
    }

    int getval(int stridx, textchar_t *dstbuf, size_t dstbuflen,
               size_t *result_len);

    int getval(CHtmlRect *rectval)
    {
        /* make sure the type's correct */
        if (type_ != HTML_DCFG_TYPE_RECT)
            return 1;

        /* set the value and return success */
        *rectval = *val_.rect_;
        return 0;
    }

    int getval_color(HTML_color_t *colorval)
    {
        /* make sure it's the correct type */
        if (type_ != HTML_DCFG_TYPE_COLOR)
            return 1;

        /* set the value and return success */
        *colorval = val_.color_;
        return 0;
    }

    int getval_bytes_len(size_t *len)
    {
        /* make sure it's the correct type */
        if (type_ != HTML_DCFG_TYPE_BYTES)
            return 1;

        /* set the length and return success */
        *len = val_.bytes_.len_;
        return 0;
    }

    int getval_bytes_buf(void **bufptr)
    {
        /* make sure it's the correct type */
        if (type_ != HTML_DCFG_TYPE_BYTES)
            return 1;

        /* return the pointer to the buffer */
        *bufptr = val_.bytes_.buf_;
        return 0;
    }

    int getval_bytes(void *buf, size_t len)
    {
        /* make sure it's the correct type */
        if (type_ != HTML_DCFG_TYPE_BYTES)
            return 1;

        /* 
         *   copy the actual length or the capacity of the buffer, whichever
         *   is smaller 
         */
        if (val_.bytes_.len_ < len)
            len = val_.bytes_.len_;

        /* copy the buffer */
        memcpy(buf, val_.bytes_.buf_, len);
        return 0;
    }


    /*
     *   Write the end-of-file marker to a file.  We use this during reading
     *   to detect when we've finished reading all of the table entries in a
     *   file.  
     */
    static int write_eof_marker(osfildef *fp);

    /* datatype of this entry */
    html_dcfg_type_t type_;

    /* the value of this entry */
    union
    {
        /* a string array is stored as a linked list of string objects */
        CHtmlDcfgString *strlist_;

        /* integer value */
        int int_;

        /* rectangle */
        CHtmlRect *rect_;

        /* color */
        HTML_color_t color_;

        struct
        {
            void *buf_;
            size_t len_;
        } bytes_;
    } val_;

    /*
     *   Status of the variable.  We use this internally as a scratch-pad to
     *   keep track of the status of the variable during certain operations
     *   on all variables.  For example, the write-to-t3-makefile operation
     *   uses this to keep track of variables that map to makefile options.  
     */
    int status_;

private:
    /* delete the current value */
    void delete_value();

    /* change to a new type */
    void change_type(html_dcfg_type_t new_type);
};


#endif /* HTMLDCFG_H */

