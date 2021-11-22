#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlrc.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlrc.cpp - resource cache
Function
  
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Resource cache object 
 */

/*
 *   Delete an resource cache object.  Note that we're only deleted by the
 *   remove_ref() method.  
 */
CHtmlResCacheObject::~CHtmlResCacheObject()
{
    /* if I'm in a cache, remove myself from the cache */
    if (cache_ != 0)
        cache_->remove(this);

    /* we own the resource, so delete it */
    if (resource_ != 0)
        delete resource_;
}

/*
 *   Downcast routines 
 */

/* get the resource as an image */
CHtmlSysImage *CHtmlResCacheObject::get_image()
{
    return (resource_ != 0 ? resource_->cast_image() : 0);
}

/* get the resource as a sound */
CHtmlSysSound *CHtmlResCacheObject::get_sound()
{
    return (resource_ != 0 ? resource_->cast_sound() : 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash entry subclass for resource cache objects 
 */
class CHtmlHashEntryRes: public CHtmlHashEntryCS
{
public:
    CHtmlHashEntryRes(CHtmlResCacheObject *obj)
        : CHtmlHashEntryCS(obj->get_url(), get_strlen(obj->get_url()), FALSE)
    {
        cache_obj_ = obj;
    }

    ~CHtmlHashEntryRes()
    {
        /* delete our associated cache object */
        // $$$ delete cache_obj_;
    }

    /* cache object */
    CHtmlResCacheObject *cache_obj_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Resource cache
 */

CHtmlResCache::CHtmlResCache()
{
    /* create my hash table with a case-sensitive hash function */
    hashtable_ = new CHtmlHashTable(512, new CHtmlHashFuncCS);
}

CHtmlResCache::~CHtmlResCache()
{
    /* delete the hash table */
    delete hashtable_;
}

/*
 *   Find an existing resource in the cache 
 */
CHtmlResCacheObject *CHtmlResCache::find(const CHtmlUrl *url)
{
    CHtmlHashEntryRes *entry;

    /* if the url is empty, there's no object */
    if (url->get_url() == 0)
        return 0;
    
    /* look in the hash table for the object */
    entry = (CHtmlHashEntryRes *)
            hashtable_->find(url->get_url(), get_strlen(url->get_url()));

    /* if we found a hash table entry, return the cache object */
    return (entry == 0 ? 0 : entry->cache_obj_);
}

/*
 *   Find an existing resource, or create a new one if there isn't one in
 *   the cache matching the given URL.  
 */
CHtmlResCacheObject *CHtmlResCache::find_or_create(
    CHtmlSysWin *win, CHtmlResFinder *res_finder, const CHtmlUrl *url)
{
    CHtmlResCacheObject *entry;
    CHtmlSysResource *resource;
    htmlres_loader_func_t loader_func;
    unsigned long seekpos;
    unsigned long filesize;
    CStringBuf fname;
    int cacheable;
    
    /* search for an existing entry, and return it if we can find it */
    if ((entry = find(url)) != 0)
        return entry;

    /* presume we're not going to be able to load the resource */
    resource = 0;

    /* 
     *   We didn't find one - load the resource from the URL.  First
     *   determine the resource type, and see if we can find the resource
     *   in the .GAM file.  
     */
    if (CHtmlResType::get_res_mapping(url->get_url(), &loader_func)
        != HTML_res_type_unknown)
    {
        /* find the resource in the .GAM file or externally */
        res_finder->get_file_info(&fname, url->get_url(),
                                  get_strlen(url->get_url()),
                                  &seekpos, &filesize);

        /* load it */
        resource = (*loader_func)(url, fname.get(), seekpos, filesize, win);
    }

    /* note if the entry is cacheable */
    cacheable = (resource == 0 || resource->is_cacheable());

    /* create a new cache object for the resource */
    entry = new CHtmlResCacheObject(url, resource, cacheable ? this : 0);

    /* if the resource is cacheable, add it to our cache */
    if (cacheable)
        add(entry);

    /* return the new entry */
    return entry;
}

/*
 *   Add an object to the cache.
 */
void CHtmlResCache::add(CHtmlResCacheObject *obj)
{
    CHtmlHashEntryRes *entry;

    /* create an entry for the object */
    entry = new CHtmlHashEntryRes(obj);

    /* add it to the hash table */
    hashtable_->add(entry);
}

/*
 *   Remove an object from the cache.
 */
void CHtmlResCache::remove(CHtmlResCacheObject *obj)
{
    CHtmlHashEntry *entry;
    
    /* find the hash table entry for the object */
    entry = hashtable_->find(obj->get_url(), get_strlen(obj->get_url()));

    /* 
     *   If we found it, remove it from the hash table, and delete the
     *   hash entry object, since its only purpose is to fill the slot in
     *   the hash table.  
     */
    if (entry != 0)
    {
        hashtable_->remove(entry);
        delete entry;
    }
}

