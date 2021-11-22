/* $Header: d:/cvsroot/tads/html/htmlrc.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlrc.h - resource cache
Function
  Implements a cache for resources, such as images and sounds.
  Because these binary resources can be large objects, it's desirable
  to re-use them whenever possible, rather than load a separate copy
  of the resource for each use.

  Resources are identified by URL's, and contain associated binary
  data loaded from external files.  The details of loading and using
  the binary data associated with the resource are not part of the
  cache's responsibilities; the cache merely provides the reference
  tracking mechanism.

  This cache:

    - Allows a single resource to be referenced multiple times, and tracks
      the references, so that the resource is deleted from memory when and
      only when nothing is referencing it.  (Of course, this requires
      that the objects doing the referencing are cooperating by adding
      and removing references when necessary, but apart from that,
      client objects don't need to worry about the operations of the
      cache.)

    - Finds a resource that's already in memory, given an URL to the
      resource.
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#ifndef HTMLRC_H
#define HTMLRC_H

#ifndef HTMLURL_H
#include "htmlurl.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Resource cache object.  The cache object implements referencing
 *   counting, and automatically deletes itself when its reference count
 *   drops to zero.  The cache object owns the associated resource, and
 *   deletes the resource when the cache object is deleted. 
 */
class CHtmlResCacheObject
{
public:
    /* create a cache object */
    CHtmlResCacheObject(const CHtmlUrl *url,
                        class CHtmlSysResource *resource,
                        class CHtmlResCache *cache)
    {
        /* remember the resource */
        resource_ = resource;

        /* remember the URL */
        url_.set_url(url);

        /* no references yet -- all references must be explicitly added */
        refcnt_ = 0;

        /* remember the cache */
        cache_ = cache;
    }

    /* get the URL of the resource */
    const textchar_t *get_url() const { return url_.get_url(); }

    /* get the resource */
    class CHtmlSysResource *get_resource() { return resource_; }

    /*
     *   Get the resource as specific resource subtypes.  These downcast
     *   routines use the typesafe downcast routines in the
     *   CHtmlSysResource interface, so these methods return null when
     *   called on inappropriate resource types. 
     */
    class CHtmlSysImage *get_image();
    class CHtmlSysSound *get_sound();

    /* 
     *   Add/remove a reference.  When the last reference is removed, the
     *   object is automatically deleted. 
     */
    void add_ref() { ++refcnt_; }
    void remove_ref()
    {
        --refcnt_;
        if (refcnt_ == 0)
            delete this;
    }

private:
    /* 
     *   the constructor is never called explicitly by a client; instead,
     *   the client simply removes its reference, and we'll automatically
     *   delete ourselves when our reference count drops to zero 
     */
    ~CHtmlResCacheObject();

    /* URL of the resource */
    CHtmlUrl url_;

    /* the resource object - contains the resource's binary data */
    class CHtmlSysResource *resource_;

    /* number of references */
    long refcnt_;

    /* cache containing the resource */
    class CHtmlResCache *cache_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Resource cache 
 */

class CHtmlResCache
{
    friend class CHtmlResCacheObject;
    
public:
    CHtmlResCache();
    ~CHtmlResCache();

    /* 
     *   Add an object to the cache.  Note that although the cache has a
     *   reference to the object, it is a "weak" reference, in that it
     *   doesn't add to the reference count for the object and the
     *   presence of this reference doesn't keep the object in memory.
     *   Instead, the cache object is responsible for notifying us if it's
     *   deleted, and we'll remove our reference to the object.  
     */
    void add(class CHtmlResCacheObject *obj);

    /*
     *   Find an existing cache object matching an URL.  Returns null if
     *   there is no entry in the cache matching the URL.  
     */
    class CHtmlResCacheObject *find(const CHtmlUrl *url);

    /*
     *   Find an existing cache object matching an URL, or create a new
     *   one if one is not already in the cache. 
     */
    class CHtmlResCacheObject *find_or_create(class CHtmlSysWin *win,
                                              class CHtmlResFinder *resfinder,
                                              const CHtmlUrl *url);

private:
    /* 
     *   Remove an object from the cache.  This should never be called
     *   except by the cache object itself as it's about to be deleted. 
     */
    void remove(class CHtmlResCacheObject *obj);

    /* hash table containing the cache objects */
    class CHtmlHashTable *hashtable_;
};

#endif /* HTMLRC_H */

