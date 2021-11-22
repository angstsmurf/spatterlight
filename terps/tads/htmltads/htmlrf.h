#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlrf.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlrf.h - HTML resource finder
Function
  Finds HTML resources.  An HTML resource can be in an external file,
  or it can be part of the .GAM file, stored in the HTMLRES resource
  structure.
Notes
  
Modified
  12/03/97 MJRoberts  - Creation
*/

#ifndef HTMLRF_H
#define HTMLRF_H

#include <stdlib.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

/* include the TADS main runtime header */
#ifndef TRD_H
#include "trd.h"
#endif

class CHtmlResFinder
{
public:
    CHtmlResFinder();
    ~CHtmlResFinder();

    /* reset - delete any old resource finder information */
    void reset();

    /*
     *   Set up a TADS appctxdef structure for use with the TADS loader.
     *   The TADS loader makes calls through function pointers in this
     *   structure; we'll set up the structure to refer to our methods, so
     *   that we can learn about the resources in the .GAM file. 
     */
    void init_appctx(appctxdef *appctx)
    {
        /* load up the callback functions to point to my callbacks */
        appctx->set_game_name = &cb_set_game_name;
        appctx->set_game_name_ctx = this;
        appctx->set_res_dir = &cb_set_res_dir;
        appctx->set_res_dir_ctx = this;
        appctx->set_resmap_seek = &cb_set_resmap_seek;
        appctx->set_resmap_seek_ctx = this;
        appctx->add_resource = &cb_add_resource;
        appctx->add_resource_ctx = this;
        appctx->add_res_path = &cb_add_res_path;
        appctx->add_res_path_ctx = this;
        appctx->add_resfile = &cb_add_resfile;
        appctx->add_resfile_ctx = this;
        appctx->resfile_exists = &cb_resfile_exists;
        appctx->resfile_exists_ctx = this;
        appctx->find_resource = &cb_find_resource;
        appctx->find_resource_ctx = this;
    }

    /*
     *   Get the filename and seek offset to use to read a resource.  If
     *   we can find the resource in our .GAM file resource map, we'll
     *   return the .GAM filename and the offset of the resource in the
     *.  GAM file; otherwise, we'll return the name of the resource
     *   itself and a seek offset of zero.
     */
    void get_file_info(CStringBuf *outbuf,
                       const textchar_t *resource_name, size_t resnamelen,
                       unsigned long *start_seekpos,
                       unsigned long *file_size);

    /* determine if a resource file exists */
    int resfile_exists(const textchar_t *resource_name, size_t resnamelen);

private:
    /* find a resource, optionally returning a handle to the file */
    int find_resource(CStringBuf *filenamebuf,
                      const char *resname, size_t resnamelen,
                      unsigned long *start_seekpos, unsigned long *res_size,
                      osfildef **fp_ret);
    
    /* initialize our data */
    void init_data();
    
    /* delete all of our data */
    void delete_data();
    
    /* set the .GAM file name */
    void set_game_name(const char *fname);

    /* set the root path for individual resources */
    void set_res_dir(const char *dir);

    /* set the resource base seek position */
    void set_resmap_seek(unsigned long seekpos, int fileno);

    /* add a resource in the .GAM resource map */
    void add_resource(unsigned long ofs, unsigned long siz,
                      const char *nm, size_t nmlen, int fileno);

    /* add a stand-alone resource file search path */
    void add_res_path(const char *path, size_t len);

    /* add a resource file */
    int add_resfile(const char *fname);

    /* 
     *   given a resource name, construct the name of the external file
     *   containing the resource 
     */
    void resname_to_filename(char *fname_buf, size_t fname_buf_size,
                             const char *resname, size_t resname_len);

    /*
     *   Static functions used as function pointers in the appctx
     *   structure.  These functions invoke our methods.
     */
    static void cb_set_game_name(void *ctx, const char *fname);
    static void cb_set_res_dir(void *ctx, const char *dir);
    static void cb_set_resmap_seek(void *ctx, unsigned long seekpos,
                                   int fileno);
    static void cb_add_resource(void *ctx, unsigned long ofs,
                                unsigned long siz,
                                const char *nm, size_t nmlen, int fileno);
    static void cb_add_res_path(void *ctx, const char *path, size_t len);
    static osfildef *cb_find_resource(void *ctx, const char *resname,
                                      size_t resnamelen,
                                      unsigned long *res_size);
    static int cb_add_resfile(void *ctx, const char *fname);
    static int cb_resfile_exists(void *ctx, const char *res_name,
                                 size_t res_name_len);

    /* hash table containing resource information */
    class CHtmlHashTable *restab_;

    /* 
     *   array of file objects, number of items used in the list, and
     *   number of entries allocated in the array 
     */
    class CHtmlRfFile **files_;
    int files_used_;
    int files_alloced_;

    /* root path for resource files, if one was specified */
    CStringBuf res_dir_;

    /* head and tail of stand-alone resource file search path */
    class CHtmlResFinderPathEntry *search_path_head_;
    class CHtmlResFinderPathEntry *search_path_tail_;
};

/*
 *   Search path entry.  We keep a linked list of these entries, giving the
 *   list of directories to search when we're looking for a stand-alone
 *   external resource. 
 */
class CHtmlResFinderPathEntry
{
public:
    /* construct */
    CHtmlResFinderPathEntry(const char *path, size_t len)
        : path_(path, len)
    {
        /* no 'next' link yet */
        nxt_ = 0;
    }

    /* get/set the 'next' pointer */
    CHtmlResFinderPathEntry *get_next() const { return nxt_; }
    void set_next(CHtmlResFinderPathEntry *nxt) { nxt_ = nxt; }

    /* get a pointer to the null-terminated string giving the path */
    const char *get_path() const { return path_.get(); }

private:
    /* our directory path string */
    CStringBuf path_;

    /* the next entry in the list */
    CHtmlResFinderPathEntry *nxt_;
};


#endif /* HTMLRF_H */

