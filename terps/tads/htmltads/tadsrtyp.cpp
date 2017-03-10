#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/tadsrtyp.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsrtyp.cpp - TADS Resource Type implementation
Function
  
Notes
  
Modified
  11/17/98 MJRoberts  - Creation
*/

#include <stdio.h>
#include <memory.h>
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Resource table entry.  This table is used to load resources based on
 *   the resource type that appears as the resource URL suffix.  
 */
struct ext_map_t
{
    /* 
     *   The resource suffix ("extension") - this is a string of up to
     *   eight characters that appears at the end of an URL after a
     *   period; this identifies the type of the resource.  
     */
    textchar_t ext[9];

    /*
     *   Resource type ID.  This is the internal content type identifier
     *   that we use for the resource.  
     */
    HTML_res_type_t res_type;

    /*
     *   Loader.  This is a function that we invoke to load the resource
     *   from the file and create the C++ object that we use to manipulate
     *   the resource.  
     */
    htmlres_loader_func_t create_res_obj;
};

/* 
 *   the static resource table, the number of entries it contains, and the
 *   total number of slots in the table 
 */
ext_map_t *CHtmlResType::ext_table_ = 0;
size_t CHtmlResType::ext_table_cnt_ = 0;
size_t CHtmlResType::ext_table_size_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Add the basic set of built-in resource type mappings 
 */
void CHtmlResType::add_basic_types()
{
    add_res_type("JPG",  HTML_res_type_JPEG, &CHtmlSysImageJpeg::create_jpeg);
    add_res_type("JPE",  HTML_res_type_JPEG, &CHtmlSysImageJpeg::create_jpeg);
    add_res_type("JPEG", HTML_res_type_JPEG, &CHtmlSysImageJpeg::create_jpeg);
    add_res_type("PNG",  HTML_res_type_PNG,  &CHtmlSysImagePng::create_png);
    add_res_type("MNG",  HTML_res_type_MNG,  &CHtmlSysImageMng::create_mng);
    add_res_type("WAV",  HTML_res_type_WAV,  &CHtmlSysSoundWav::create_wav);
    add_res_type("MID",  HTML_res_type_MIDI, &CHtmlSysSoundMidi::create_midi);
    add_res_type("MIDI", HTML_res_type_MIDI, &CHtmlSysSoundMidi::create_midi);
    add_res_type("MPG",  HTML_res_type_MP123,&CHtmlSysSoundMpeg::create_mpeg);
    add_res_type("MP2",  HTML_res_type_MP123,&CHtmlSysSoundMpeg::create_mpeg);
    add_res_type("MP3",  HTML_res_type_MP123,&CHtmlSysSoundMpeg::create_mpeg);
    add_res_type("OGG",  HTML_res_type_OGG,  &CHtmlSysSoundOgg::create_ogg);
}


/* ------------------------------------------------------------------------ */
/*
 *   Add a resource type 
 */
void CHtmlResType::add_res_type(const textchar_t *suffix,
                                HTML_res_type_t res_type,
                                htmlres_loader_func_t create_res_obj)
{
    ext_map_t *entry;

    /* 
     *   if we don't have a mapping table yet, allocate one; if we have
     *   one but it's full, expand it 
     */
    if (ext_table_cnt_ == 0)
    {
        /* 
         *   no table yet - allocate a table with space for several
         *   initial entries 
         */
        ext_table_size_ = 10;
        ext_table_ = (ext_map_t *)th_malloc(ext_table_size_
                                            * sizeof(ext_table_[0]));
    }
    else if (ext_table_cnt_ == ext_table_size_)
    {
        /* the table is full - expand it a bit */
        ext_table_size_ += 10;
        ext_table_ = (ext_map_t *)th_realloc(ext_table_,
                                             ext_table_size_
                                             * sizeof(ext_table_[0]));
    }

    /* add this entry */
    entry = &ext_table_[ext_table_cnt_];
    do_strcpy(entry->ext, suffix);
    entry->res_type = res_type;
    entry->create_res_obj = create_res_obj;

    /* consume the slot */
    ++ext_table_cnt_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Determine the type of a resource given its URL 
 */
HTML_res_type_t CHtmlResType::get_res_type(const textchar_t *url)
{
    return get_res_mapping(url, 0);
}

/*
 *   Get the mapping information for a resource 
 */
HTML_res_type_t CHtmlResType::
   get_res_mapping(const textchar_t *url,
                   htmlres_loader_func_t *loader_func)

{
    const textchar_t *p;
    const textchar_t *last_dot;
    const ext_map_t *pmap;
    size_t i;

    /* if there's no URL, there's no mapping */
    if (url == 0)
        return HTML_res_type_unknown;

    /*
     *   Look for the last dot that follows the last path separator
     *   character (colon, slash, or backslash) 
     */
    for (p = url, last_dot = 0 ; *p ; ++p)
    {
        switch(*p)
        {
        case '/':
        case '\\':
        case ':':
            /* it's a path separator, so the last dot is invalid */
            last_dot = 0;
            break;

        case '.':
            /* this is the last dot we've seen so far */
            last_dot = p;
            break;
        }
    }

    /* if we didn't find a dot, we can't determine the resource type */
    if (last_dot == 0)
        return HTML_res_type_unknown;

    /* skip the dot and find the extension */
    ++last_dot;

    /* look up the extension in our table */
    for (pmap = ext_table_, i = 0 ; i < ext_table_cnt_ ; ++pmap, ++i)
    {
        /* see if this one matches */
        if (get_stricmp(last_dot, pmap->ext) == 0)
        {
            /* if they wanted to know about the loader, return it */
            if (loader_func != 0)
                *loader_func = pmap->create_res_obj;

            /* found a match */
            return pmap->res_type;
        }
    }

    /* we didn't find a match */
    return HTML_res_type_unknown;
}

/*
 *   Delete the resource table 
 */
void CHtmlResType::delete_res_table()
{
    if (ext_table_ != 0)
    {
        th_free(ext_table_);
        ext_table_ = 0;
        ext_table_cnt_ = ext_table_size_ = 0;
    }
}


