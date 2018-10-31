#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldc3.cpp - HTML TADS Debugger Configuration reader for TADS 3 makefiles
Function
  Reads TADS 3 makefiles into debugger configuration objects.  Parses
  the compiler options from the makefile and stores them as debugger
  configuration variables.
Notes
  
Modified
  07/11/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "tadshtml.h"
#include "htmldcfg.h"
#include "htmlhash.h"

/* tads 3 headers */
#include "tccmdutl.h"


/* ------------------------------------------------------------------------ */
/*
 *   Option file comment parsing states 
 */
enum comment_state_t
{
    /* start of file - haven't parsed anything yet */
    COMMENT_START,

    /* parsing file header comments */
    COMMENT_HEADER,

    /* parsing source file list header comments */
    COMMENT_SOURCES,

    /* parsing a discarded comment */
    COMMENT_DISCARD
};

/*
 *   Option file helper object
 */
class CConfigOptHelper: public CTcOptFileHelper
{
public:
    CConfigOptHelper(CHtmlDebugConfig *config)
    {
        /* remember my debugger configuration object */
        config_ = config;

        /* we haven't found the first comment yet */
        cur_comment_ = COMMENT_START;
        cur_idx_ = 0;

        /* we've found no foreign configuration data yet */
        fc_idx_ = 0;

        /* we haven't found any configuration data yet */
        found_config_ = FALSE;
    }

    /* allocate an option string */
    char *alloc_opt_file_str(size_t len)
        { return (char *)th_malloc(len); }

    /* free an option string previously allocated */
    void free_opt_file_str(char *str)
        { th_free(str); }

    /* note a non-comment line */
    void process_non_comment_line(const char *)
    {
        /* if we're parsing a comment, mark it as no longer in progress */
        switch(cur_comment_)
        {
        case COMMENT_DISCARD:
            /* we've been discarding a comment; keep doing so */
            break;
            
        case COMMENT_START:
            /* 
             *   we're still waiting for the file header comment, but we
             *   found a non-comment first - there's no file header, so
             *   simply ignore any comments we find until we find something
             *   we explicitly recognize 
             */
            cur_comment_ = COMMENT_DISCARD;
            break;

        default:
            /* 
             *   for any other type of comment we've been processing, we're
             *   done with that block of comment lines, so discard any other
             *   comments we encounter until we find something we recognize 
             */
            cur_comment_ = COMMENT_DISCARD;
            break;
        }
    }

    /* process a comment */
    void process_comment_line(const char *str)
    {
        /* check for a special comment section flag */
        if (strcmp(str, "##sources") == 0)
        {
            /* enter the source files block comment */
            cur_comment_ = COMMENT_SOURCES;
            cur_idx_ = 0;

            /* don't store the ##sources line with the block */
            return;
        }

        /* check our state */
        switch(cur_comment_)
        {
        case COMMENT_DISCARD:
            /* discarding - ignore the comment */
            break;

        case COMMENT_START:
            /* we're waiting for the file header comment block - enter it */
            cur_comment_ = COMMENT_HEADER;
            cur_idx_ = 0;

            /* FALL THROUGH to process the header comment line */

        case COMMENT_HEADER:
            /* add this to the header comment list */
            config_->setval("makefile", "header_comment", cur_idx_++, str);
            break;

        case COMMENT_SOURCES:
            /* add this to the sources comment list */
            config_->setval("makefile", "sources_comment", cur_idx_++, str);
            break;
        }
    }

    /* process a configuration line */
    void process_config_line(const char *id, const char *str, int is_id_line)
    {
        const char *start;
        CStringBuf varname;
        CStringBuf elename;
        const char *typ;
        size_t typ_len;
        const char *val;
        const char *p;

        /* 
         *   If we haven't found any configuration data yet, make a note that
         *   we have now found configuration data.  If we save an updated
         *   configuration back to this file, we might want to know whether
         *   or not any tool-generated configuration data were ever present,
         *   because if not, the file was probably created manually, so we
         *   might want to warn about overwriting it. 
         */
        if (!found_config_)
        {
            /* note it in the configuration records */
            config_->setval("makefile", "found_auto_config_data", TRUE);

            /* 
             *   note that we've found config data, so we don't flag it again
             *   in the configuration 
             */
            found_config_ = TRUE;
        }

        /* 
         *   if this isn't part of our configuration section, simply save it
         *   verbatim, so that we can store it when we save the file 
         */
        if (stricmp(id, "devsys") != 0)
        {
            /* save this line in the foreign config list */
            config_->setval("makefile", "foreign_config", fc_idx_++, str);

            /* we're done with this line */
            return;
        }

        /* 
         *   it's ours - if this is the ID line itself, ignore it, since we
         *   have no need to store it explicitly 
         */
        if (is_id_line)
            return;

        /* 
         *   This is one of our lines.  There are two possible storage
         *   formats:
         *   
         *   varname:elename:type:val
         *.  *len varname-of-length-len:type:val
         *   
         *   Where 'varname' is the variable name, 'elename' is the element
         *   name (essentially a second-level portion of the variable name),
         *   'type' is a type name, and 'val' is the value, encoded
         *   according to the type.
         *   
         *   If the line starts with '*len', where 'len' is a decimal
         *   number, then 'len' gives the number of bytes in the variable
         *   name; we ignore any embedded colons in that stretch of
         *   characters.  This allows for variable/element names that
         *   contain multiple colons.
         *   
         *   The valid types are:
         *   
         *   S: the value is in the form "idx:val", where 'idx' is the
         *   numeric index of the string in the string array, and 'val' is
         *   the string itself.
         *   
         *   I: the value is an integer, encoded as a decimal number.
         *   
         *   R: the value is a rectangle's coordinates in the form
         *   "x1,y1,x2,y2", where each subvalue is an integer encoded as a
         *   decimal number.
         *   
         *   C: the value is a color in the form "r,g,b", where each subvalue
         *   is an integer encoded as a decimal number.
         *   
         *   B: binary data encoded as a series of hex pairs, with no spaces
         *   (for example, the string "\001\002\003\n" would be encoded as
         *   0102030A).  
         */

        /* skip leading spaces */
        for (p = str ; is_space(*p) ; ++p) ;

        /* check for a length prefix */
        if (*p == '*')
        {
            int len;

            /* scan the length */
            ++p;
            len = atoi(p);

            /* skip to the next space, then skip intervening whitespace */
            for ( ; *p != '\0' && !is_space(*p) ; ++p) ;
            for ( ; is_space(*p) ; ++p) ;

            /* scan and save the variable name */
            for (start = p ; *p != ':' && *p != '\0' && len != 0 ;
                 ++p, --len) ;
            varname.set(start, p - start);

            /* skip the colon */
            if (*p == ':' && len != 0)
                ++p, --len;

            /* 
             *   scan the element name - this name is opaque, so take
             *   everything for the remaining length, even if we find an
             *   embedded colon 
             */
            for (start = p ; *p != '\0' && len != 0 ; ++p, --len) ;

            /* if the element name is non-null, remember it */
            if (p != start)
                elename.set(start, p - start);

            /* 
             *   we should be looking at a colon now - if not, then the line
             *   isn't well-formed, so ignore it 
             */
            if (*p != ':')
                return;
        }
        else
        {
            /* scan and save the variable name */
            for (start = p ; *p != ':' && *p != '\0' ; ++p) ;
            varname.set(start, p - start);
            
            /* skip the colon */
            if (*p == ':')
                ++p;
            
            /* scan the element name */
            for (start = p ; *p != ':' && *p != '\0' ; ++p) ;
            
            /* if the element name is non-null, remember it */
            if (p != start)
                elename.set(start, p - start);
        }

        /* skip the colon */
        if (*p == ':')
            ++p;

        /* find the end of the type name */
        for (typ = p ; *p != ':' && *p != '\0' ; ++p) ;
        typ_len = p - typ;

        /* skip the colon */
        if (*p == ':')
            ++p;

        /* the value follows */
        val = p;

        /* 
         *   if the line doesn't look well-formed, ignore it - we must have a
         *   non-empty variable name, and a one-character type code 
         */
        if (varname.get() == 0
            || get_strlen(varname.get()) == 0
            || typ_len != 1)
            return;

        /* check the type code */
        switch(typ[0])
        {
        case 'S':
            /* string */
            {
                int idx;

                /* get the index value */
                idx = atoi(val);

                /* skip to the colon */
                for ( ; *val != ':' && *val != '\0' ; ++val) ;

                /* if we didn't find the colon, it's not well-formed */
                if (*val != ':')
                    return;

                /* skip the colon */
                ++val;

                /* store the value */
                config_->setval(varname.get(), elename.get(), idx, val);
            }
            break;

        case 'I':
            /* integer */
            config_->setval(varname.get(), elename.get(), atoi(val));
            break;

        case 'R':
            /* rectangle */
            {
                long x1, y1, x2, y2;
                CHtmlRect rc;

                /* scan the four coordinates */
                sscanf(val, "%ld,%ld,%ld,%ld", &x1, &y1, &x2, &y2);

                /* set up the rectangle structure */
                rc.set(x1, y1, x2, y2);

                /* store the value */
                config_->setval(varname.get(), elename.get(), &rc);
            }
            break;

        case 'C':
            /* color */
            {
                int r, g, b;

                /* scan the red, green, and blue values */
                sscanf(val, "%d,%d,%d", &r, &g, &b);

                /* store the color */
                config_->setval_color(varname.get(), elename.get(),
                                      HTML_make_color(r, g, b));
            }
            break;

        case 'B':
            /* binary data */
            {
                unsigned char *binval;
                unsigned char *dst;
                size_t len;

                /* if the value length isn't even, it's not well-formed */
                len = strlen(val);
                if ((len & 1) != 0)
                    return;

                /* 
                 *   allocate a buffer for room for the converted data - we
                 *   need one byte in the buffer for each two characters in
                 *   the value string, since each two characters in the value
                 *   encode one byte 
                 */
                binval = (unsigned char *)th_malloc(len / 2);

                /* convert the value to bytes */
                for (dst = binval ; *val != '\0' ; val += 2)
                {
                    /* 
                     *   if we don't have two hex digits, it's not
                     *   well-formed, so ignore it 
                     */
                    if (!is_hex_digit(*val) || !is_hex_digit(*(val+1)))
                    {
                        /* it's not well-formed - ignore the string */
                        th_free(binval);
                        return;
                    }

                    /* add this byte to the buffer */
                    *dst++ = ((ascii_hex_to_int(*val) << 4)
                              + ascii_hex_to_int(*val));
                }

                /* store the value */
                config_->setval_bytes(varname.get(), elename.get(),
                                      binval, len/2);

                /* we're done with the conversion buffer */
                th_free(binval);
            }
            break;
        }
    }

protected:
    /* my debugger configuration object */
    CHtmlDebugConfig *config_;

    /* current comment state */
    comment_state_t cur_comment_;

    /* string index of next line of current comment to be stored */
    int cur_idx_;

    /* string index of next line of foreign configuration data */
    int fc_idx_;

    /* flag: we've found a configuration section */
    int found_config_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Service routine - append an argument string to an option.  If the
 *   argument contains any spaces, we'll put it in quotes.  
 */
static void append_option_arg(CStringBuf *buf, const char *arg)
{
    const char *p;

    /* scan the argument for spaces */
    for (p = arg ; *p != '\0' ; ++p)
    {
        /* if it's a space or a quote, stop scanning */
        if (is_space(*p) || *p == '"')
            break;
    }

    /* if we found no spaces or quotes, append it without quotes */
    if (*p == '\0')
    {
        /* the string doesn't need quoting, so append it and we're done */
        buf->append(arg);
        return;
    }

    /* append the open quote */
    buf->append_inc("\"", 128);

    /* append each character, stuttering each quote */
    for (p = arg ; *p != '\0' ; ++p)
    {
        /* add this character */
        buf->append_inc(p, 1, 128);

        /* if it's a quote within the string, stutter it */
        if (*p == '"')
            buf->append_inc("\"", 128);
    }

    /* append the closing quote */
    buf->append("\"");
}

/* ------------------------------------------------------------------------ */
/*
 *   Set a string index value, converting it from URL to local notation, and
 *   expanding it relative to a given base path. 
 */
void CHtmlDebugConfig::setval_rel_url(const char *var, const char *subvar,
                                      int idx, const char *val,
                                      const char *base_path)
{
    char buf[OSFNMAX];

    /* convert from URL notation to local conventions */
    os_cvt_url_dir(buf, sizeof(buf), val, FALSE);

    /* 
     *   if the result is a relative filename or path, build the full path
     *   using the given base path 
     */
    if (!os_is_file_absolute(buf) && base_path != 0 && base_path[0] != '\0')
    {
        char buf2[OSFNMAX];
        
        /* copy it into another buffer for a moment */
        strcpy(buf2, buf);

        /* combine it with the base path */
        os_build_full_path(buf, sizeof(buf), base_path, buf2);
    }

    /* store the local form internally */
    setval(var, subvar, idx, buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   Read a .t3m file (T3 makefile)
 */
int CHtmlDebugConfig::load_t3m_file(const textchar_t *fname,
                                    const char *sys_lib_path)
{
    osfildef *fp;
    CConfigOptHelper opt_helper(this);
    char **argv;
    int argc;
    int i;
    char *p;
    int def_idx;
    int undef_idx;
    int inc_idx;
    int src_idx;
    int res_idx;
    int symdir_idx;
    int objdir_idx;
    int srcdir_idx;
    int extra_idx;
    CStringBuf optbuf;
    char proj_path[OSFNMAX];
    int res_recurse = TRUE;

    /* extract the project file path */
    os_get_path_name(proj_path, sizeof(proj_path), fname);

    /* we don't have any -I, -D, or -U options yet */
    inc_idx = def_idx = undef_idx = 0;

    /* we don't have any -F options yet */
    symdir_idx = objdir_idx = srcdir_idx = 0;

    /* we have no extra options yet */
    extra_idx = 0;

    /* we have no source files yet */
    src_idx = 0;

    /* we have no resource files yet */
    res_idx = 0;

    /* open the file - give up immediately if we can't */
    fp = osfoprt(fname, OSFTTEXT);
    if (fp == 0)
        return 1;

    /* get rid of all of the existing option table entries */
    hashtab_->delete_all_entries();

    /* parse the file once, to get the argument count */
    argc = CTcCommandUtil::parse_opt_file(fp, 0, 0);

    /* allocate space for the argument list */
    argv = (char **)th_malloc(argc * sizeof(argv[0]));

    /* 
     *   Seek back to the start of the file and parse it again now that we
     *   have a place to store the argument vector.  
     */
    osfseek(fp, 0, OSFSK_SET);
    CTcCommandUtil::parse_opt_file(fp, argv, &opt_helper);

    /* done with the file */
    osfcls(fp);

    /* parse the options */
    for (i = 0 ; i < argc && argv[i][0] == '-' ; ++i)
    {
        /* 
         *   if we have a "-source" or "-lib" argument, we're done with
         *   options, and we're on to the module list 
         */
        if (strcmp(argv[i], "-source") == 0
            || strcmp(argv[i], "-lib") == 0)
            break;

        /* find out which option we have */
        switch(argv[i][1])
        {
        case 'd':
            /* debug mode - note it */
            setval("makefile", "debug", TRUE);
            break;

        case 'D':
            /* add preprocessor symbol definition */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 1);
            if (p != 0)
                setval("build", "defines", def_idx++, p);
            break;

        case 'U':
            p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 1);
            if (p != 0)
                setval("build", "undefs", undef_idx++, p);
            break;

        case 'c':
            /* see what follows */
            switch (argv[i][2])
            {
            case 's':
                /* it's a character set specification */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 2);
                if (p != 0)
                {
                    setval("build", "use_charmap", TRUE);
                    setval("build", "charmap", 0, p);
                }
                break;

            case 'l':
                /* check for "-clean" */
                if (strcmp(argv[i], "-clean") == 0)
                {
                    /* set "clean" mode */
                    setval("makefile", "clean_mode", TRUE);
                }
                break;

            case '\0':
                /* set compile-only (no link) mode */
                setval("makefile", "compile_only", TRUE);
                break;
            }
            break;

        case 'a':
            /* see what follows */
            switch(argv[i][2])
            {
            case 'l':
                /* force only the link phase */
                setval("makefile", "force_link", TRUE);
                break;

            case '\0':
                setval("makefile", "force_build", TRUE);
                break;
            }
            break;

        case 'v':
            /* set verbose mode */
            setval("build", "verbose errors", TRUE);
            break;

        case 'I':
            /* add a #include path */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 1);
            if (p != 0)
                setval_rel_url("build", "includes", inc_idx++, p, proj_path);
            break;

        case 'o':
            /* set the image file name */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 1);
            if (p != 0)
                setval_rel_url("makefile", "image_file", 0, p, proj_path);
            break;

        case 'O':
            switch(argv[i][2])
            {
            case 's':
                /* string capture file - set up a string with the option */
                optbuf.set("-Os ");

                /* get the argument */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 2);
                if (p != 0)
                {
                    /* add the "-Os file" option to the extra options list */
                    append_option_arg(&optbuf, p);
                    setval("build", "extra options",
                           extra_idx++, optbuf.get());
                }
                break;
            }
            break;

        case 'F':
            /* output path settings - see which one we're setting */
            switch(argv[i][2])
            {
                char sw;
                    
            case 's':
            case 'o':
            case 'y':
            case 'a':
                /* remember the sub-option */
                sw = argv[i][2];

                /* read the path argument */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &i, 2);
                if (p != 0)
                {
                    /* set the appropriate build variable */
                    switch(sw)
                    {
                    case 's':
                        setval_rel_url("build", "srcfile path", srcdir_idx++,
                                       p, proj_path);
                        break;

                    case 'o':
                        setval_rel_url("build", "objfile path", objdir_idx++,
                                       p, proj_path);
                        break;

                    case 'y':
                        setval_rel_url("build", "symfile path", symdir_idx++,
                                       p, proj_path);
                        break;

                    case 'a':
                        setval_rel_url("build", "asmfile", 0, p, proj_path);
                        break;
                    }
                }
                break;

            default:
                /* unrecognize option - ignore it */
                break;
            }
            break;

        case 'n':
            /* check what we have */
            if (strcmp(argv[i], "-nopre") == 0)
            {
                /* explicitly turn off preinit mode */
                setval("build", "run preinit", FALSE);
            }
            else if (strcmp(argv[i], "-nobanner") == 0)
            {
                /* add this to the extra options */
                setval("build", "extra options", extra_idx++, argv[i]);
            }
            else if (strcmp(argv[i], "-nodef") == 0)
            {
                /* don't add the default modules */
                setval("build", "keep default modules", FALSE);
            }
            break;

        case 'p':
            /* check what we have */
            if (strcmp(argv[i], "-pre") == 0)
            {
                /* explicitly turn on preinit mode */
                setval("build", "run preinit", TRUE);
            }
            break;

        case 'P':
            /* 
             *   A preprocess-only mode.  The plain -P generates
             *   preprocessed source to standard output; -Pi generates only
             *   a list of names of #included files to standard output.  
             */
            if (strcmp(argv[i], "-P") == 0)
            {
                /* set preprocess-only mode */
                setval("makefile", "pp_only", TRUE);
            }
            else if (strcmp(argv[i], "-Pi") == 0)
            {
                /* set list-includes mode */
                setval("makefile", "list_includes", TRUE);
            }
            break;

        case 'q':
            /* quiet mode - add this to the extra options */
            setval("build", "extra options", extra_idx++, argv[i]);
            break;

        case 't':
            /* add this to the extra options */
            if (strcmp(argv[i], "-test") == 0)
                setval("build", "extra options", extra_idx++, argv[i]);
            break;

        case 'w':
            /* warning level - see what level they're setting */
            if (argv[i][2] != '\0')
                setval("build", "warning level", argv[i][2] - '0');
            break;

        default:
            /* invalid argument - ignore it */
            break;
        }
    }

    /* scan the modules */
    for ( ; i < argc ; ++i)
    {
        int is_source;
        int is_lib;
        char fname[OSFNMAX];
        const char *file_loc;
        char full_fname[OSFNMAX];

        /* we don't know the file type yet */
        is_source = FALSE;
        is_lib = FALSE;

        /* check to see if we have a module type option */
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-source") == 0)
            {
                /* note that we have a source file, and skip the option */
                is_source = TRUE;
                ++i;
            }
            else if (strcmp(argv[i], "-lib") == 0)
            {
                /* note that we have a library, and skip the option */
                is_lib = TRUE;
                ++i;
            }
            else if (strcmp(argv[i], "-res") == 0)
            {
                /* 
                 *   we're starting the resource options - skip the "-res"
                 *   and exit the source module loop 
                 */
                ++i;
                break;
            }
            else
            {
                /* invalid option - skip it */
                continue;
            }

            /* 
             *   if we have no filename argument following the type
             *   specifier, it's an error; skip it
             */
            if (i == argc)
                break;
        }

        /* 
         *   if we didn't find an explicit type specifier, infer the type
         *   from the filename suffix, if one is present 
         */
        if (!is_source && !is_lib)
        {
            size_t len;

            /* 
             *   if we have a ".tl" suffix, assume it's a library file;
             *   otherwise, assume it's a source file 
             */
            if ((len = strlen(argv[i])) > 3
                && stricmp(argv[i] + len - 3, ".tl") == 0)
            {
                /* ".tl" - it's a library file */
                is_lib = TRUE;
            }
            else
            {
                /* something else - assume it's a source file */
                is_source = TRUE;
            }
        }

        /* convert the name from URL format to local format */
        os_cvt_url_dir(fname, sizeof(fname), argv[i], FALSE);

        /* add a default extension according to the file type */
        os_defext(fname, is_source ? "t" : "tl");

        /* 
         *   Check to see if this is a user or system file: if we can find it
         *   in the project file directory, it's definitely a user file.
         *   Otherwise, if we can find the file in the system directory, it's
         *   a system file.  If we can't find it in either place, assume it's
         *   a user file that doesn't exist yet.  
         */
        os_build_full_path(full_fname, sizeof(full_fname),
                           proj_path, fname);
        if (!osfacc(full_fname))
        {
            /* it exists in the project directory, so it's a user file */
            file_loc = "user";
        }
        else
        {
            /* 
             *   it doesn't exist in the project directory, so check to see
             *   if we can find it in the system directory; if so, it's a
             *   system file 
             */
            os_build_full_path(full_fname, sizeof(full_fname),
                               sys_lib_path, fname);
            if (!osfacc(full_fname))
            {
                /* it exists in the system library directory */
                file_loc = "system";
            }
            else
            {
                /* we can't find it anywhere, so assume it's a user file */
                file_loc = "user";
            }
        }
            
        /* note the file's user/system status */
        setval("build", "source_files-sys", src_idx, file_loc);

        /* process the file according to its type */
        if (is_source)
        {
            /* add the file to the module list and mark it as a source file */
            setval("build", "source_files", src_idx, fname);
            setval("build", "source_file_types", src_idx, "source");

            /* count it */
            ++src_idx;
        }
        else
        {
            int exc_idx;
            char exc_var[OSFNMAX + 50];

            /* add this file to the module list */
            setval("build", "source_files", src_idx, fname);
            setval("build", "source_file_types", src_idx, "library");

            /* no exclusions yet for this library */
            exc_idx = 0;

            /* build the exclusion variable name */
            sprintf(exc_var, "lib_exclude:%s", fname);

            /* process any exclusion options */
            while (i + 1 < argc && strcmp(argv[i+1], "-x") == 0)
            {
                const char *url;

                /* get the argument of the "-x" */
                url = argv[i + 2];

                /* skip the "-x" and its argument */
                i += 2;

                /* if the filename is missing, ignore it */
                if (i + 1 >= argc)
                    break;

                /* note this exclusion item */
                setval("build", exc_var, exc_idx++, argv[i]);
            }

            /* count it */
            ++src_idx;
        }
    }

    /* scan resource options */
    for ( ; i < argc ; ++i)
    {
        char fname[OSFNMAX];

        /* check for options */
        if (argv[i][0] == '-')
        {
            /* it's an option - check which one we have */
            if (strcmp(argv[i], "-recurse") == 0)
            {
                /* set recursive mode for subsequent directory items */
                res_recurse = TRUE;
            }
            else if (strcmp(argv[i], "-norecurse") == 0)
            {
                /* set non-recursive mode for subsequent directory items */
                res_recurse = FALSE;
            }
            else
            {
                /* unknown option - just ignore it and continue */
                continue;
            }
        }
        else
        {
            /*
             *   it's a filename - convert it from URL notation to local file
             *   naming conventions 
             */
            os_cvt_url_dir(fname, sizeof(fname), argv[i], FALSE);

            /* add the resource */
            setval("build", "image_resources", res_idx, fname);

            /* set its recursion status */
            setval("build", "image_resources_recurse", res_idx,
                   res_recurse ? "Y" : "N");

            /* count it */
            ++res_idx;
        }
    }

    /* delete the argv strings we previously allocated */
    for (i = 0 ; i < argc ; ++i)
        opt_helper.free_opt_file_str(argv[i]);

    /* delete the argv vector itself */
    th_free(argv);

    /* success */
    return 0;
}
    
/* ------------------------------------------------------------------------ */
/*
 *   context structure for our hashtable enumeration callbacks 
 */
struct save_file_enum_ctx_t
{
    /* the file we're writing */
    osfildef *fp;
};

/*
 *   hashtable enumerator: clear the 'status_' field of a variable 
 */
static void clear_status_cb(void * /*ctx*/, CHtmlHashEntry *entry)
{
    /* initialize the status to zero */
    ((CHtmlHashEntryConfig *)entry)->status_ = 0;
}

/*
 *   save a variable name to a config file 
 */
static void save_var_name(osfildef *fp, CHtmlHashEntryConfig *entry,
                          int colons)
{
    char buf[256];
    
    /*  
     *   The hashtable entry already stores the name and element names
     *   separated by a colon, so we just need to write the whole string as
     *   one part.
     *   
     *   If the name consists of only one part (i.e., no element name), add
     *   an extra colon after the name so we'll know on loading this file
     *   that we have a null element name for this variable.
     *   
     *   If the name has more than one colon, we have to write it in a
     *   special format, since we normally indicate the end of the name with
     *   a colon.  If there are extra colons in the name, then write it with
     *   a length prefix rather than a delimiter, so that we can take the
     *   entire contents of the name as opaque.  
     */
    if (colons > 1)
    {
        /* multiple colons - write with the special length-prefix notation */
        sprintf(buf, "*%d %.*s:", (int)entry->getlen(),
                (int)entry->getlen(), entry->getstr());
    }
    else
    {
        /* 
         *   standard zero- or one-colon name - write it out normally, adding
         *   an extra colon at the end if there is no embedded colon 
         */
        sprintf(buf, "%.*s%s:",
                (int)entry->getlen(), entry->getstr(),
                colons == 0 ? ":" : "");
    }
    os_fprintz(fp, buf);
}

/*
 *   hashtable enumerator: save all generic variables 
 */
static void save_generic_vars_cb(void *ctx0, CHtmlHashEntry *entry0)
{
    save_file_enum_ctx_t *ctx = (save_file_enum_ctx_t *)ctx0;
    CHtmlHashEntryConfig *entry = (CHtmlHashEntryConfig *)entry0;
    const char *p;
    size_t rem;
    int colons;
    char fbuf[128];

    /*
     *   If this is a "makefile" variable, it's metadata about the makefile,
     *   so it doesn't have to be stored in the makefile.  If it has a
     *   non-zero status_ field, it means we've mapped it to an explicit
     *   makefile option, so it doesn't have to be written generically.  So,
     *   simply ignore the variable and return if either of these conditions
     *   is true. 
     */
    if (entry->status_ != 0
        || (entry->getlen() >= 9
            && memcmp(entry->getstr(), "makefile:", 9) == 0))
        return;

    /* 
     *   If it's an empty string list, skip it.
     *   
     *   We need to check this as a special case because we would otherwise
     *   write the "var:ele" prefix line, as we do for all variables before
     *   writing the type-specific part, even though we have no value to
     *   write.  We must check for this case in particular to avoid even
     *   writing the prefix part, because we don't want to write anything at
     *   all in this case.  
     */
    if (entry->type_ == HTML_DCFG_TYPE_STRLIST
        && entry->get_strlist_cnt() == 0)
    {
        /* it's an empty string list, so there's nothing to write */
        return;
    }

    /* count the number of colons in the entry's variable name string */
    for (p = entry->getstr(), rem = entry->getlen(), colons = 0 ;
         rem != 0 ; ++p, --rem)
    {
        /* if this is a colon, count it */
        if (*p == ':')
            ++colons;
    }

    /* write the variable name */
    save_var_name(ctx->fp, entry, colons);

    /* get the type code for writing to the file */
    switch (entry->type_)
    {
    case HTML_DCFG_TYPE_STRLIST:
        /* 
         *   it's a string list - write each string from the list as a
         *   separate entry 
         */
        {
            CHtmlDcfgString *str;
            int idx;

            for (idx = 0, str = entry->val_.strlist_ ; str != 0 ;
                 ++idx, str = str->next_)
            {
                /* if this entry is null, skip it */
                if (str->str_.get() == 0)
                    continue;
                
                /* if this isn't the first entry, write the name again */
                if (idx != 0)
                    save_var_name(ctx->fp, entry, colons);
                
                /* write the type, index, and value */
                sprintf(fbuf, "S:%d:", idx);
                os_fprintz(ctx->fp, fbuf);
                os_fprintz(ctx->fp, str->str_.get());
                os_fprintz(ctx->fp, "\n");
            }
        }
        break;

    case HTML_DCFG_TYPE_INT:
        /* write the integer value */
        sprintf(fbuf, "I:%d\n", entry->val_.int_);
        os_fprintz(ctx->fp, fbuf);
        break;

    case HTML_DCFG_TYPE_RECT:
        /* write the rectangle value */
        sprintf(fbuf, "R:%ld,%ld,%ld,%ld\n",
                entry->val_.rect_->left,
                entry->val_.rect_->top,
                entry->val_.rect_->right,
                entry->val_.rect_->bottom);
        os_fprintz(ctx->fp, fbuf);
        break;

    case HTML_DCFG_TYPE_COLOR:
        /* write the RGB value */
        sprintf(fbuf, "C:%d,%d,%d\n",
                HTML_color_red(entry->val_.color_),
                HTML_color_green(entry->val_.color_),
                HTML_color_blue(entry->val_.color_));
        os_fprintz(ctx->fp, fbuf);
        break;

    case HTML_DCFG_TYPE_BYTES:
        /* write the type code */
        os_fprintz(ctx->fp, "B:");

        /* write the bytes, encoding each as a hex digit pair */
        {
            unsigned char *p;
            size_t rem;

            /* scan the buffer */
            for (p = (unsigned char *)entry->val_.bytes_.buf_,
                 rem = entry->val_.bytes_.len_ ;
                 rem != 0 ;
                 --rem, ++p)
            {
                /* write this byte */
                sprintf(fbuf, "%02x", *p);
                os_fprintz(ctx->fp, fbuf);
            }
        }
        break;

    default:
        /* unknown type - mark it in the file as an unknown type */
        os_fprintz(ctx->fp, "?\n");
        break;
    }

}

/*
 *   Find the filename extension in a URL-style filename.  Returns null if no
 *   extension was found, otherwise returns a pointer to the first character
 *   of the extension (i.e., the character following the '.' that introduces
 *   the extension).
 *   
 *   For convenience, we take a pointer to a const string, since we don't
 *   need to modify the string, but we return a non-const result, in case the
 *   caller does need to modify the result.  Callers should actually treat
 *   the return value as though it had the same const-ness as the 'url'
 *   parameter.  
 */
char *CHtmlDebugConfig::find_url_ext(const char *url)
{
    const char *p;
    const char *ext;

    /* scan the URL */
    for (ext = 0, p = url ; *p != '\0' ; ++p)
    {
        /* check what we have */
        switch(*p)
        {
        case '/':
        case ':':
            /* 
             *   it's a path separator character - any dot we've previously
             *   seen isn't part of the extension after all, since the
             *   extension has to be part of the last path element 
             */
            ext = 0;
            break;

        case '.':
            /* 
             *   it's a dot - this might be the extension (we can't be sure
             *   until we ensure that no path separator characters and no
             *   other dots follow, but this is the best candidate so far, so
             *   note it and keep looking) 
             */
            ext = p + 1;
            break;
        }
    }

    /* return the extension */
    return (char *)ext;
}


/*
 *   Save a .t3m file (T3 makefile) 
 */
int CHtmlDebugConfig::save_t3m_file(const textchar_t *fname)
{
    osfildef *fp;
    save_file_enum_ctx_t cbctx;
    CHtmlHashEntryConfig *entry;
    char proj_path[OSFNMAX];
    char proj_url[OSFNMAX];

    /* open the file - give up immediately if we can't */
    fp = osfopwt(fname, OSFTTEXT);
    if (fp == 0)
        return 1;

    /* 
     *   extract the project's local filename path, and convert it to URL
     *   notation
     */
    os_get_path_name(proj_path, sizeof(proj_path), fname);
    os_cvt_dir_url(proj_url, sizeof(proj_url), proj_path, TRUE);

    /* set up our callback context */
    cbctx.fp = fp;

    /*
     *   First, run through the table of variables and set everything's
     *   status to zero.  We use the status field to keep track of which
     *   variables we've written to the makefile so far, so we want to make
     *   sure we have everything in a clean initial state.  
     */
    hashtab_->enum_entries(clear_status_cb, &cbctx);

    /*
     *   If this makefile contains no [Config:xxx] sections, it is likely
     *   that it was originally created by hand or simply did not previously
     *   exist.  If this is the case, generate some header comments
     *   explaining that the makefile is mechanically generated.  
     */
    if (get_entry("makefile", "found_auto_config_data", FALSE) == 0)
        os_fprintz(fp, "# TADS 3 makefile\n"
                   "#\n"
                   "# Warning: this file was mechanically generated.  You "
                   "may edit this file\n"
                   "# manually, but your changes might be modified or "
                   "discarded when\n"
                   "# you load this file into a TADS development tool.  "
                   "The TADS tools\n"
                   "# generally retain the comment at the start of the "
                   "file and the\n"
                   "# comment marked \"##sources\" below, but other comments "
                   "might be\n"
                   "# discarded, the formatting might be changed, and "
                   "some option\n"
                   "# settings might be modified.\n\n");

    /* copy the original header comment to the file */
    t3m_copy_to_file(fp, "makefile", "header_comment");

    /* save the -o option */
    t3m_save_option_strs(fp, "makefile", "image_file", "-o", TRUE, proj_url);

    /* save the debug mode setting */
    t3m_save_option_bool(fp, "makefile", "debug", "-d", 0);

    /* save the -P and -Pi options */
    t3m_save_option_bool(fp, "makefile", "pp_only", "-P", 0);
    t3m_save_option_bool(fp, "makefile", "list_includes", "-Pi", 0);

    /* save the -pre or -nopre option */
    t3m_save_option_bool(fp, "build", "run preinit", "-pre", "-nopre");

    /* save the -nodef option */
    t3m_save_option_bool(fp, "build", "keep default modules", 0, "-nodef");

    /* save the -I options */
    t3m_save_option_strs(fp, "build", "includes", "-I", TRUE, proj_url);

    /* save the -D options */
    t3m_save_option_strs(fp, "build", "defines", "-D", FALSE, 0);

    /* save the -U options */
    t3m_save_option_strs(fp, "build", "undefs", "-U", FALSE, 0);

    /* save the -cs option */
    if ((entry = t3m_get_entry_for_save("build", "use_charmap")) != 0
        && entry->type_ == HTML_DCFG_TYPE_INT
        && entry->val_.int_)
    {
        /* use_charmap is set, so write the character mapping name */
        t3m_save_option_strs(fp, "build", "charmap", "-cs", FALSE, 0);
    }

    /* save the -Fs, -Fo, -Fy options */
    t3m_save_option_strs(fp, "build", "srcfile path", "-Fs", TRUE, proj_url);
    t3m_save_option_strs(fp, "build", "symfile path", "-Fy", TRUE, proj_url);
    t3m_save_option_strs(fp, "build", "objfile path", "-Fo", TRUE, proj_url);
    t3m_save_option_strs(fp, "build", "asmfile", "-Fa", TRUE, proj_url);

    /* save the -clean option */
    t3m_save_option_bool(fp, "makefile", "clean_mode", "-clean", 0);

    /* save the -c option */
    t3m_save_option_bool(fp, "makefile", "compile_only", "-c", 0);

    /* save the -a option */
    t3m_save_option_bool(fp, "makefile", "force_build", "-a", 0);

    /* save the -al option */
    t3m_save_option_bool(fp, "makefile", "force_link", "-al", 0);

    /* save the -v option */
    t3m_save_option_bool(fp, "build", "verbose errors", "-v", 0);

    /* save the -w option */
    t3m_save_option_int(fp, "build", "warning level", "-w");

    /* save the extra build options - just copy these verbatim */
    t3m_copy_to_file(fp, "makefile", "extra_options");

    /* 
     *   We're done with options, so we're ready to add the source files.
     *   Before the source file list itself, copy the ##sources comment
     *   block, if there is one.  If there isn't, simply write the ##sources
     *   comment flag.  
     */
    os_fprintz(fp, "\n##sources\n");
    if (get_entry("makefile", "sources_comment", FALSE) != 0)
    {
        /* we found the ##sources comment block - copy it out unchanged */
        t3m_copy_to_file(fp, "makefile", "sources_comment");
    }

    /* write the source file list */
    if ((entry = t3m_get_entry_for_save("build", "source_files")) != 0
        && entry->type_ == HTML_DCFG_TYPE_STRLIST)
    {
        CHtmlDcfgString *str;
        int idx;

        /* mark the source_file_types variable as saved */
        t3m_get_entry_for_save("build", "source_file_types");

        /* make the source_files-sys variable as saved */
        t3m_get_entry_for_save("build", "source_files-sys");

        /* scan the string list */
        for (idx = 0, str = entry->val_.strlist_ ; str != 0 ;
             str = str->next_, ++idx)
        {
            char tbuf[64];
            char url[OSFNMAX];
            char *defext;
            char *ext;

            /* if this is a null entry, skip it */
            if (str->str_.get() == 0)
                continue;
            
            /* get the corresponding file type marker */
            getval("build", "source_file_types", idx, tbuf, sizeof(tbuf));

            /* write the type flag */
            os_fprintz(fp, tbuf[0] == 's' ? "-source " : "-lib ");

            /* convert the filename to URL notation */
            os_cvt_dir_url(url, sizeof(url), str->str_.get(), FALSE);

            /* note the extension implied for the type */
            defext = (tbuf[0] == 's' ? "t" : "tl");

            /* find the existing extension on the filename */
            ext = find_url_ext(url);

            /* 
             *   If the extension matches the default extension, remove it.
             *   It's better to store source and library filenames without
             *   extensions if possible, because this makes the .t3m file
             *   more readily portable to systems that use different
             *   extension conventions.  Since we found the default extension
             *   for the type, and the extension conforms to local
             *   conventions, we'll add the extension back in by default when
             *   we reload the file, so we can drop it.  
             */
            if (stricmp(ext, defext) == 0)
                *(ext - 1) = '\0';

            /* write the filename as a URL, adding quotes if needed */
            t3m_save_write_arg(fp, url);

            /* add a newline */
            os_fprintz(fp, "\n");

            /* if this is a library, check for its exclusion list */
            if (tbuf[0] == 'l')
            {
                char exc_var[OSFNMAX + 50];
                
                /* build the exclusion variable name */
                sprintf(exc_var, "lib_exclude:%s", str->str_.get());

                /* save the exclusion list */
                t3m_save_option_strs(fp, "build", exc_var, "  -x", FALSE, 0);
            }
        }
    }

    /* write the resource list, if any */
    if ((entry = t3m_get_entry_for_save("build", "image_resources")) != 0
        && entry->type_ == HTML_DCFG_TYPE_STRLIST
        && entry->val_.strlist_ != 0)
    {
        CHtmlDcfgString *str;
        int idx;
        int recurse = TRUE;

        /* 
         *   make the image_resources-sys variable as saved, even though we
         *   actually don't have any use for it 
         */
        t3m_get_entry_for_save("build", "image_resources-sys");

        /* mark the 'recursive' flag as saved */
        t3m_get_entry_for_save("build", "image_resources_recurse");

        /* add the "-res" option to the file */
        os_fprintz(fp, "\n-res\n");

        /* add the resources */
        for (idx = 0, str = entry->val_.strlist_ ; str != 0 ;
             str = str->next_, ++idx)
        {
            char rbuf[5];
            int new_recurse;

            /* if the entry is empty, skip it */
            if (str->str_.get() == 0)
                continue;

            /* 
             *   get the item's recursion setting (assuming recursive mode by
             *   default) 
             */
            if (getval("build", "image_resources_recurse", idx,
                       rbuf, sizeof(rbuf)))
                rbuf[0] = 'Y';

            /* if the mode is changing, add the appropriate option */
            new_recurse = (rbuf[0] == 'Y');
            if (recurse != new_recurse)
            {
                /* write the new setting */
                os_fprintz(fp, new_recurse ? "-recurse\n" : "-norecurse\n");

                /* remember the new setting for next time */
                recurse = new_recurse;
            }

            /* write this file entry */
            os_fprintz(fp, str->str_.get());
            os_fprintz(fp, "\n");
        }
    }

    /*
     *   Create our [Config:xxx] section, and write out everything that we
     *   haven't specifically mapped to a regular option setting or used for
     *   some purpose of our own.  Everything with status == 0 and with a
     *   primary variable name other than "makefile" should be written.
     *   (Items with status != 0 are not written because they're the
     *   variables we mapped to makefile options.  Items with variable name
     *   "makefile" are not written because they're configuration data
     *   related to the makefile itself, which we use to construct the
     *   makefile.)  
     */
    os_fprintz(fp, "\n# The following is machine-generated - "
               "DO NOT EDIT\n"
               "[Config:devsys]\n");
    cbctx.fp = fp;
    hashtab_->enum_entries(&save_generic_vars_cb, &cbctx);
    os_fprintz(fp, "\n");

    /* 
     *   Finally, write out all of the foreign [Config:xxx] sections we found
     *   in the file.  When we loaded the file, we saved the original raw
     *   text of all of the foreign configuration sections (including their
     *   "[Config:xxx]" marker lines), so we simply need to dump out the
     *   saved strings verbatim.  
     */
    t3m_copy_to_file(fp, "makefile", "foreign_config");

    /* done with the file */
    osfcls(fp);

    /* success */
    return 0;
}

/*
 *   Get a variable's hash table record for saving to the .t3m file.  If we
 *   find the variable, we'll mark it as explicitly saved, so that we don't
 *   have to save it with our raw [Config:devsys] data.  
 */
CHtmlHashEntryConfig *CHtmlDebugConfig::t3m_get_entry_for_save(
    const char *var, const char *ele)
{
    CHtmlHashEntryConfig *entry;

    /* get the entry */
    entry = get_entry(var, ele, FALSE);

    /* if we found it, mark it as explicitly saved */
    if (entry != 0)
        entry->status_ = 1;

    /* return what we found */
    return entry;
}
    

/*
 *   Copy a configuration variable's string list to the .t3m file verbatim.  
 */
void CHtmlDebugConfig::t3m_copy_to_file(osfildef *fp, const char *var,
                                        const char *ele)
{
    CHtmlHashEntryConfig *entry;
    CHtmlDcfgString *str;

    /* get the variable's hash entry */
    entry = t3m_get_entry_for_save(var, ele);

    /* if it doesn't exist, or it's not a string list, ignore it */
    if (entry == 0 || entry->type_ != HTML_DCFG_TYPE_STRLIST)
        return;

    /* write out each string in the list */
    for (str = entry->val_.strlist_ ; str != 0 ; str = str->next_)
    {
        /* write out this string and a newline */
        if (str->str_.get() != 0)
        {
            os_fprintz(fp, str->str_.get());
            os_fprintz(fp, "\n");
        }
    }
}

/*
 *   Save to a .t3m file a set of options given by a string list, using the
 *   given option specifier.  
 */
void CHtmlDebugConfig::t3m_save_option_strs(osfildef *fp, const char *var,
                                            const char *ele, const char *opt,
                                            int cvt_to_url,
                                            const char *base_path_url)
{
    CHtmlHashEntryConfig *entry;
    CHtmlDcfgString *str;

    /* get the entry, and ensure it's the expected type */
    if ((entry = t3m_get_entry_for_save(var, ele)) == 0
        || entry->type_ != HTML_DCFG_TYPE_STRLIST)
        return;

    /* write out each string in the list */
    for (str = entry->val_.strlist_ ; str != 0 ; str = str->next_)
    {
        char url[OSFNMAX];
        const char *p;

        /* if the string is set and is non-empty, write it out */
        if ((p = str->str_.get()) != 0 && *p != '\0')
        {
            /* if they want it converted to a URL, do so */
            if (cvt_to_url)
            {
                size_t base_len;
                
                /* convert it into URL format */
                os_cvt_dir_url(url, sizeof(url), p, FALSE);

                /* use the URL version as the string to write */
                p = url;

                /* 
                 *   if we have a base path URL, and it matches this value
                 *   URL as a prefix, then remove the prefix and just store
                 *   the relative portion 
                 */
                if (base_path_url != 0
                    && (base_len = strlen(base_path_url)) < strlen(url)
                    && memcmp(base_path_url, url, base_len) == 0)
                {
                    /* use only the part after the path prefix */
                    p = url + base_len;
                }
            }
            
            /* write the option string and a space */
            os_fprintz(fp, opt);
            os_fprintz(fp, " ");

            /* write the argument, quoting it if necessary */
            t3m_save_write_arg(fp, p);

            /* add a newline after the option */
            os_fprintz(fp, "\n");
        }
    }
}

/*
 *   Save a boolean option to a .t3m file.  If the option is set, write the
 *   'opt_on' string; otherwise, write the 'opt_off' string.  
 */
void CHtmlDebugConfig::t3m_save_option_bool(osfildef *fp, const char *var,
                                            const char *ele,
                                            const char *opt_on,
                                            const char *opt_off)
{
    CHtmlHashEntryConfig *entry;

    /* get the entry, and ensure it's the expected type */
    if ((entry = t3m_get_entry_for_save(var, ele)) == 0
        || entry->type_ != HTML_DCFG_TYPE_INT)
        return;

    /* write the on or off option string, as appropriate */
    if (entry->val_.int_)
    {
        /* the option is set - if there's an 'on' string, write it */
        if (opt_on != 0)
        {
            os_fprintz(fp, opt_on);
            os_fprintz(fp, "\n");
        }
    }
    else
    {
        /* the option is unset - if there's an 'off' string, write it */
        if (opt_off != 0)
        {
            os_fprintz(fp, opt_off);
            os_fprintz(fp, "\n");
        }
    }
}

/*
 *   Save an integer option to a .t3m file.  Writes the integer setting
 *   directly appended to the option string.  
 */
void CHtmlDebugConfig::t3m_save_option_int(osfildef *fp, const char *var,
                                           const char *ele,
                                           const char *opt)
{
    CHtmlHashEntryConfig *entry;
    char fbuf[30];

    /* get the entry, and ensure it's the expected type */
    if ((entry = t3m_get_entry_for_save(var, ele)) == 0
        || entry->type_ != HTML_DCFG_TYPE_INT)
        return;

    /* write the option */
    os_fprintz(fp, opt);
    sprintf(fbuf, "%d\n", entry->val_.int_);
    os_fprintz(fp, fbuf);
}

/*
 *   Save an argument string to a .t3m file.  If the argument contains any
 *   spaces or quote marks, we'll enclose the entire string in double quotes,
 *   stuttering any quotes contained within the value.  
 */
void CHtmlDebugConfig::t3m_save_write_arg(osfildef *fp, const char *arg)
{
    const char *p;
    const char *start;

    /* scan the argument for spaces */
    for (p = arg ; *p != '\0' ; ++p)
    {
        /* if it's a space or a quote, stop scanning */
        if (is_space(*p) || *p == '"')
            break;
    }

    /* if we found no spaces or quotes, write it without quotes */
    if (*p == '\0')
    {
        /* the string doesn't need quoting, so write it directly */
        os_fprintz(fp, arg);
        return;
    }

    /* we do need quotes - write the string, stuttering each quote */
    for (start = p = arg ; *p != '\0' ; ++p)
    {
        /* 
         *   if we're at a quote, write the part up to here, plus an extra
         *   quote to stutter this quote 
         */
        if (*p == '"')
        {
            /* 
             *   write the part up to here plus an extra quote; if this is
             *   the first chunk, add the open quote as well 
             */
            os_fprintz(fp, start == arg ? "\"" : "");
            os_fprint(fp, start, p - start);
            os_fprintz(fp, "\"");

            /* start the next chunk with the quote */
            start = p;
        }
    }

    /* 
     *   add the final chunk and the close quote; if we're still on the
     *   initial chunk, we need the open quote as well 
     */
    os_fprintz(fp, start == arg ? "\"" : "");
    os_fprint(fp, start, p - start);
    os_fprintz(fp, "\"");
}

