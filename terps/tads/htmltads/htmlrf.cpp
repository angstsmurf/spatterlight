#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlrf.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlrf.cpp - HTML resource finder
Function
  
Notes
  
Modified
  12/03/97 MJRoberts  - Creation
*/

/* include TADS os interface functions */
#include <os.h>

#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry subclass for a resource map entry
 */
class CHtmlHashEntryFileres: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryFileres(const textchar_t *str, size_t len,
                          unsigned long seekofs, unsigned long siz,
                          int fileno)
        : CHtmlHashEntryCI(str, len, TRUE)
    {
        seekofs_ = seekofs;
        siz_ = siz;
        fileno_ = fileno;
    }

    /* seek offset and size of the resource in the resource file */
    unsigned long seekofs_;
    unsigned long siz_;

    /* 
     *   File number - the resource finder keeps a list of resource files
     *   that we've mapped; this gives the ID of the file in the list.  
     */
    int fileno_;
};

/* ------------------------------------------------------------------------ */
/*
 *   File list entry.  The resource finder keeps a list of files that
 *   we've loaded.  
 */
class CHtmlRfFile
{
public:
    CHtmlRfFile()
    {
        /* assume the seek base is zero */
        res_seek_base_ = 0;
    }
    
    CHtmlRfFile(const textchar_t *fname)
    {
        /* set the filename */
        filename_.set(fname);

        /* we don't know the seek base yet - assume it will be zero */
        res_seek_base_ = 0;
    }

    /* get the filename */
    const textchar_t *get_fname() { return filename_.get(); }

    /* change the filename */
    void set_fname(const textchar_t *fname) { filename_.set(fname); }

    /* get/set the seek base position */
    unsigned long get_seek_base() const { return res_seek_base_; }
    void set_seek_base(unsigned long val) { res_seek_base_ = val; }

private:
    /* name of the file */
    CStringBuf filename_;

    /* base seek offset of resources in .GAM file */
    unsigned long res_seek_base_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Resource finder implementation 
 */

CHtmlResFinder::CHtmlResFinder()
{
    /* initialize */
    init_data();
}

CHtmlResFinder::~CHtmlResFinder()
{
    /* delete the old data */
    delete_data();
}

/*
 *   initialize our data
 */
void CHtmlResFinder::init_data()
{
    /* create a hash table for the resource map */
    restab_ = new CHtmlHashTable(256, new CHtmlHashFuncCI);

    /* 
     *   Initialize the file list.  We always allocate the first entry,
     *   since this is the dedicated entry for the .GAM file.  
     */
    files_alloced_ = 5;
    files_ = (CHtmlRfFile **)
             th_malloc(files_alloced_ * sizeof(CHtmlRfFile *));
    files_used_ = 1;
    files_[0] = new CHtmlRfFile();

    /* we have no external resource file search path directories yet */
    search_path_head_ = search_path_tail_ = 0;
}

/*
 *   Delete our data
 */
void CHtmlResFinder::delete_data()
{
    int i;

    /* delete the hash table */
    delete restab_;

    /* delete the list of files */
    for (i = 0 ; i < files_used_ ; ++i)
        delete files_[i];
    delete files_;

    /* delete the external file search path */
    while (search_path_head_ != 0)
    {
        /* remember the next one */
        CHtmlResFinderPathEntry *nxt = search_path_head_->get_next();

        /* delete this entry */
        delete search_path_head_;

        /* unlink it */
        search_path_head_ = nxt;
    }
}

/*
 *   Reset 
 */
void CHtmlResFinder::reset()
{
    /* delete the old data */
    delete_data();

    /* re-initialize */
    init_data();
}

/*
 *   Determine if an entry exists 
 */
int CHtmlResFinder::resfile_exists(const textchar_t *resname,
                                   size_t resnamelen)
{
    CHtmlHashEntryFileres *entry;
    char cvtbuf[OSFNMAX];

    /* 
     *   see if we can find an entry in the resource map - if so, it
     *   definitely exists 
     */
    entry = (CHtmlHashEntryFileres *)restab_->find(resname, resnamelen);
    if (entry != 0)
        return TRUE;

    /* check with the local system to see if it's an embedded resource */
    if (CHtmlSysFrame::get_frame_obj() != 0)
    {
        unsigned long pos;
        unsigned long siz;

        /* ask the system for the resource */
        if (CHtmlSysFrame::get_frame_obj()->get_exe_resource(
            resname, resnamelen, cvtbuf, sizeof(cvtbuf), &pos, &siz))
            return TRUE;
    }

    /*
     *   There's no entry in the resource map matching this name - map the
     *   resource name to a stand-alone external filename.  
     */
    resname_to_filename(cvtbuf, sizeof(cvtbuf), resname, resnamelen);

    /* 
     *   if the file exists, the resource exists - so, if we don't get an
     *   error code back from osfacc, return true 
     */
    return !osfacc(cvtbuf);
}

/*
 *   Get the filename and seek offset to use to read a resource.  If we
 *   can find the resource in our .GAM file resource map, we'll return the
 *   GAM filename and the offset of the resource in the GAM file;
 *   otherwise, we'll return the name of the resource itself and a seek
 *   offset of zero.  
 */
void CHtmlResFinder::get_file_info(CStringBuf *outbuf,
                                   const textchar_t *resname,
                                   size_t resnamelen,
                                   unsigned long *start_seekpos,
                                   unsigned long *file_size)
{
    /* find the resource */
    if (!find_resource(outbuf, resname, resnamelen,
                       start_seekpos, file_size, 0))
    {
        /* failed - log a debug message */
        oshtml_dbg_printf("resource loader error: resource not found: %s\n",
                          resname);
    }
}

/*
 *   Find a resource.  Returns true if we find the resource, false if not.
 *   
 *   If we find the resource, and fp_ret is not null, we'll fill in *fp_ret
 *   with an osfildef* handle to the file, with its seek position set to the
 *   start of the resource.
 *   
 *   If *filenamebuf is non-null, we'll fill it in with the name of the file
 *   containing the resource.  
 */
int CHtmlResFinder::find_resource(CStringBuf *filenamebuf,
                                  const char *resname,
                                  size_t resnamelen,
                                  unsigned long *start_seekpos,
                                  unsigned long *res_size,
                                  osfildef **fp_ret)
{
    CHtmlHashEntryFileres *entry;

    /* see if we can find an entry in the resource map */
    entry = (CHtmlHashEntryFileres *)restab_->find(resname, resnamelen);
    if (entry != 0)
    {
        /* 
         *   Found an entry - return the seek position of the entry in the
         *   file (calculate the actual seek position as the resource map
         *   base seek address plus the offset of this resource within the
         *   resource map).  Get the filename from the files_[] list entry
         *   for the file number stored in the resource entry.  
         */
        *start_seekpos = files_[entry->fileno_]->get_seek_base()
                         + entry->seekofs_;
        *res_size = entry->siz_;

        /* tell the caller the name, if desired */
        if (filenamebuf != 0)
            filenamebuf->set(files_[entry->fileno_]->get_fname());

        /* if the caller wants the file handle, open the file */
        if (fp_ret != 0)
        {
            /* open the file */
            *fp_ret = osfoprb(files_[entry->fileno_]->get_fname(), OSFTBIN);

            /* seek to the start of the file */
            if (*fp_ret != 0)
                osfseek(*fp_ret, *start_seekpos, OSFSK_SET);
        }

        /* indicate that we found the resource */
        return TRUE;
    }
    else
    {
        char cvtbuf[OSFNMAX];
        osfildef *fp;

        /* check with the local system to see if it's an embedded resource */
        if (CHtmlSysFrame::get_frame_obj() != 0)
        {
            /* ask the system for the resource */
            if (CHtmlSysFrame::get_frame_obj()->get_exe_resource(
                resname, resnamelen, cvtbuf, sizeof(cvtbuf),
                start_seekpos, res_size))
            {
                /* it's an embedded resource - tell them the filename */
                if (filenamebuf != 0)
                    filenamebuf->set(cvtbuf);

                /* open the file if the caller is interested */
                if (fp_ret != 0)
                {
                    /* open the file */
                    *fp_ret = osfoprb(cvtbuf, OSFTBIN);

                    /* seek to the start of the resource */
                    if (*fp_ret != 0)
                        osfseek(*fp_ret, *start_seekpos, OSFSK_SET);
                }

                /* indicate that we found the resource */
                return TRUE;
            }
        }

        /*
         *   There's no entry in the resource map matching this name - map
         *   the resource name to a filename, and start at offset zero in
         *   the file.  
         */
        resname_to_filename(cvtbuf, sizeof(cvtbuf), resname, resnamelen);
        *start_seekpos = 0;

        /* store the name of the stand-alone external file */
        if (filenamebuf != 0)
            filenamebuf->set(cvtbuf);

        /* try opening the file */
        fp = osfoprb(cvtbuf, OSFTBIN);

        /* 
         *   if we failed, and the path looks like an "absolute" local path,
         *   try opening the resource name directly, without any url-to-local
         *   translations 
         */
        if (fp == 0)
        {
            /* make a null-terminated copy of the original resource name */
            if (resnamelen > sizeof(cvtbuf) - 1)
                resnamelen = sizeof(cvtbuf) - 1;
            memcpy(cvtbuf, resname, resnamelen);
            cvtbuf[resnamelen] = '\0';

            /* if the path looks absolute, try opening it as a local file */
            if (os_is_file_absolute(cvtbuf))
            {
                /* store the name of the external file */
                if (filenamebuf != 0)
                    filenamebuf->set(cvtbuf);

                /* try opening the file */
                fp = osfoprb(cvtbuf, OSFTBIN);
            }
        }

        /* check to see if we got a file */
        if (fp == 0)
        {
            /* no file - indicate failure */
            *res_size = 0;
            return FALSE;
        }

        /* seek to the end of the file to determine its size */
        osfseek(fp, 0, OSFSK_END);
        *res_size = (unsigned long)osfpos(fp);
        
        /* if the caller wants the file, return it; otherwise, close it */
        if (fp_ret != 0)
        {
            /* 
             *   position the file back at the start of the resource data
             *   (which is simply the start of the file, since the entire
             *   file is the resource's data) 
             */
            osfseek(fp, 0, OSFSK_SET);
            
            /* give the handle to the caller */
            *fp_ret = fp;
        }
        else
        {
            /* they don't want it - close the file */
            osfcls(fp);
        }
        
        /* indicate success */
        return TRUE;
    }
}

/*
 *   Given a resource name, generate the external filename for the
 *   resource 
 */
void CHtmlResFinder::resname_to_filename(char *outbuf,
                                         size_t outbufsize,
                                         const char *resname,
                                         size_t resname_len)
{
    char buf[OSFNMAX];
    char rel_fname[OSFNMAX];

    /* generate a null-terminated version of the resource name */
    if (resname_len > sizeof(buf) - 1)
        resname_len = sizeof(buf) - 1;
    memcpy(buf, resname, resname_len);
    buf[resname_len] = '\0';

    /* convert the URL to a filename */
    os_cvt_url_dir(rel_fname, sizeof(rel_fname), buf, FALSE);

    /*
     *   Get the resource path prefix.  If we have an explicit resource root
     *   directory, use that.  Otherwise, use the path portion of the game
     *   filename, so that we look for individual resources in the same
     *   directory containing the game file.  
     */
    if (res_dir_.get() != 0 && res_dir_.get()[0] != '\0')
    {
        /* use the resource directory */
        os_build_full_path(outbuf, outbufsize, res_dir_.get(), rel_fname);
    }
    else if (files_[0]->get_fname() != 0)
    {
        /* get the path name from the game filename */
        os_get_path_name(buf, sizeof(buf), files_[0]->get_fname());

        /* build the full path */
        os_build_full_path(outbuf, outbufsize, buf, rel_fname);
    }
    else
    {
        size_t len;

        /* 
         *   we have no root path; just use the relative filename as it is;
         *   limit the copy length to the buffer size 
         */
        len = strlen(rel_fname);
        if (len > outbufsize)
            len = outbufsize - 1;

        /* copy and null-terminate the result */
        memcpy(outbuf, rel_fname, len);
        outbuf[len] = '\0';
    }

    /* if the file doesn't exist, check the search path */
    if (osfacc(outbuf))
    {
        const CHtmlResFinderPathEntry *entry;
        
        /* check each search path entry */
        for (entry = search_path_head_ ; entry != 0 ;
             entry = entry->get_next())
        {
            /* try building this path into a fuill filename */
            os_build_full_path(outbuf, outbufsize,
                               entry->get_path(), rel_fname);

            /* if that exists, stop and return this path */
            if (!osfacc(outbuf))
                break;
        }
    }
}

/*
 *   Set the name of the game file 
 */
void CHtmlResFinder::set_game_name(const char *fname)
{
    /* 
     *   Remember the name of the game file - we'll use this to look up
     *   resources.  The first entry in our file list is dedicated to the
     *   GAM file, so set this entry's filename.  In addition, we'll use
     *   the path to the game when looking for resources, since we look in
     *   the game's directory for external resources.  
     */
    files_[0]->set_fname(fname);
}

/*
 *   Set the root directory path for individual resources 
 */
void CHtmlResFinder::set_res_dir(const char *dir)
{
    /* remember the name */
    res_dir_.set(dir);
}

/*
 *   Add a resoure.  We'll add an entry to our hash table with the
 *   resource information.
 */
void CHtmlResFinder::add_resource(unsigned long ofs, unsigned long siz,
                                  const char *nm, size_t nmlen, int fileno)
{
    /* create and add a new hash table entry */
    restab_->add(new CHtmlHashEntryFileres(nm, nmlen, ofs, siz, fileno));
}

/*
 *   Add a resource file search path. 
 */
void CHtmlResFinder::add_res_path(const char *path, size_t len)
{
    CHtmlResFinderPathEntry *entry;
    
    /* create a search path entry */
    entry = new CHtmlResFinderPathEntry(path, len);

    /* link it in at the end of our list */
    if (search_path_tail_ != 0)
        search_path_tail_->set_next(entry);
    else
        search_path_head_ = entry;
    search_path_tail_ = entry;
}

/*
 *   Set the resource seek base for a resource file 
 */
void CHtmlResFinder::set_resmap_seek(unsigned long seekpos, int fileno)
{
    files_[fileno]->set_seek_base(seekpos);
}

/*
 *   Add a resource file, returning the file number for the new resource
 *   file.  
 */
int CHtmlResFinder::add_resfile(const char *fname)
{
    int fileno;
    
    /* note the new file's index */
    fileno = files_used_;
    
    /*
     *   If there isn't room in our array of resource files, expand the
     *   array.  
     */
    if (files_used_ == files_alloced_)
    {
        files_alloced_ += 5;
        files_ = (CHtmlRfFile **)
                 th_realloc(files_, files_alloced_ * sizeof(CHtmlRfFile *));
    }

    /* add the new entry */
    files_[files_used_] = new CHtmlRfFile(fname);
    ++files_used_;

    /* return the new file number */
    return fileno;
}


/* ------------------------------------------------------------------------ */
/*
 *   Static functions invoked from TADS loader.  We'll refer these to an
 *   instance of a CHtmlResFinder, which is stored in the callback
 *   context. 
 */

void CHtmlResFinder::cb_set_game_name(void *ctx, const char *fname)
{
    ((CHtmlResFinder *)ctx)->set_game_name(fname);
}

void CHtmlResFinder::cb_set_res_dir(void *ctx, const char *dir)
{
    ((CHtmlResFinder *)ctx)->set_res_dir(dir);
}

void CHtmlResFinder::cb_set_resmap_seek(void *ctx, unsigned long seekpos,
                                        int fileno)
{
    ((CHtmlResFinder *)ctx)->set_resmap_seek(seekpos, fileno);
}

void CHtmlResFinder::cb_add_resource(void *ctx, unsigned long ofs,
                                     unsigned long siz,
                                     const char *nm, size_t nmlen,
                                     int fileno)
{
    ((CHtmlResFinder *)ctx)->add_resource(ofs, siz, nm, nmlen, fileno);
}

int CHtmlResFinder::cb_add_resfile(void *ctx, const char *fname)
{
    return ((CHtmlResFinder *)ctx)->add_resfile(fname);
}

void CHtmlResFinder::cb_add_res_path(void *ctx, const char *path, size_t len)
{
    ((CHtmlResFinder *)ctx)->add_res_path(path, len);
}

/*
 *   Determine if a resource exists 
 */
int CHtmlResFinder::cb_resfile_exists(void *ctx, const char *res_name,
                                      size_t res_name_len)
{
    return ((CHtmlResFinder *)ctx)->resfile_exists(res_name, res_name_len);
}

/*
 *   Find a resource 
 */
osfildef *CHtmlResFinder::cb_find_resource(void *ctx, const char *resname,
                                           size_t resnamelen,
                                           unsigned long *res_size)
{
    unsigned long start_pos;
    osfildef *fp;

    /* try finding the resource */
    if (((CHtmlResFinder *)ctx)->find_resource(
        0, resname, resnamelen, &start_pos, res_size, &fp))
    {
        /* success - return the file handle */
        return fp;
    }
    else
    {
        /* failed - return a null file handle */
        return 0;
    }
}

