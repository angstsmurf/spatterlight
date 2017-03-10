#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmldcfg.cpp,v 1.3 1999/07/11 00:46:40 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldcfg.cpp - debugger configuration manager
Function
  
Notes
  
Modified
  03/05/98 MJRoberts  - Creation
*/

/* use TADS I/O routines */
#include <std.h>
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for a configuration variable
 */

/*
 *   delete 
 */
CHtmlHashEntryConfig::~CHtmlHashEntryConfig()
{
    /* delete any memory associated with the item */
    delete_value();
}

/*
 *   delete the current value 
 */
void CHtmlHashEntryConfig::delete_value()
{
    switch(type_)
    {
    case HTML_DCFG_TYPE_STRLIST:
        {
            CHtmlDcfgString *cur;
            CHtmlDcfgString *nxt;

            /* delete the string list */
            for (cur = val_.strlist_ ; cur != 0 ; cur = nxt)
            {
                /* remember the next entry */
                nxt = cur->next_;

                /* delete this entry */
                delete cur;
            }
        }
        val_.strlist_ = 0;
        break;

    case HTML_DCFG_TYPE_RECT:
        if (val_.rect_ != 0)
        {
            delete val_.rect_;
            val_.rect_ = 0;
        }
        break;

    case HTML_DCFG_TYPE_BYTES:
        if (val_.bytes_.buf_ != 0)
        {
            th_free(val_.bytes_.buf_);
            val_.bytes_.buf_ = 0;
        }
        break;

    default:
        /* other types allocate no memory */
        break;
    }
}

/*
 *   change the type 
 */
void CHtmlHashEntryConfig::change_type(html_dcfg_type_t new_type)
{
    /* see if we're actually changing anything */
    if (type_ != new_type)
    {
        /* delete the existing value */
        delete_value();
        
        /* remember the new type */
        type_ = new_type;

        /* initialize memory */
        switch(type_)
        {
        case HTML_DCFG_TYPE_STRLIST:
            val_.strlist_ = 0;
            break;

        case HTML_DCFG_TYPE_RECT:
            val_.rect_ = new CHtmlRect();
            break;

        case HTML_DCFG_TYPE_BYTES:
            break;

        default:
            /* other types don't need any memory initialization */
            break;
        }
    }
}

/*
 *   write the entry to a file 
 */
int CHtmlHashEntryConfig::write_to_file(osfildef *fp)
{
    uchar buf[16];
    
    /* write the datatype */
    buf[0] = type_;
    if (osfwb(fp, buf, 1))
        return 1;

    /* write the key's length and name */
    oswp2(buf, getlen());
    if (osfwb(fp, buf, 2)
        || osfwb(fp, getstr(), getlen() * sizeof(textchar_t)))
        return 2;

    /* write the value */
    switch(type_)
    {
    case HTML_DCFG_TYPE_STRLIST:
        {
            CHtmlDcfgString *cur;

            /* write each entry in the list */
            for (cur = val_.strlist_ ; cur != 0 ; cur = cur->next_)
            {
                size_t len;

                /* check for a null entry */
                if (cur->str_.get() == 0)
                {
                    /* it's a null entry - write it as a zero-length string */
                    oswp4(buf, 1);
                    buf[4] = '\0';
                    if (osfwb(fp, buf, 5))
                        return 3;
                }
                else
                {
                    /* write the length marker, including null terminator */
                    len = get_strlen(cur->str_.get()) + 1;
                    oswp4(buf, len);
                    if (osfwb(fp, buf, 4))
                        return 3;
                    
                    /* write the string */
                    if (osfwb(fp, cur->str_.get(), len * sizeof(textchar_t)))
                        return 4;
                }
            }

            /* write a marker for the end of the list */
            oswp4(buf, 0);
            if (osfwb(fp, buf, 4))
                return 5;
        }
        break;

    case HTML_DCFG_TYPE_INT:
        oswp4(buf, val_.int_);
        if (osfwb(fp, buf, 4))
            return 6;
        break;

    case HTML_DCFG_TYPE_RECT:
        oswp4(buf, val_.rect_->left);
        oswp4(buf + 4, val_.rect_->top);
        oswp4(buf + 8, val_.rect_->right);
        oswp4(buf + 12, val_.rect_->bottom);
        if (osfwb(fp, buf, 16))
            return 7;
        break;

    case HTML_DCFG_TYPE_COLOR:
        oswp2(buf, HTML_color_red(val_.color_));
        oswp2(buf+2, HTML_color_green(val_.color_));
        oswp2(buf+4, HTML_color_blue(val_.color_));
        if (osfwb(fp, buf, 6))
            return 8;
        break;

    case HTML_DCFG_TYPE_BYTES:
        oswp4(buf, val_.bytes_.len_);
        if (osfwb(fp, buf, 4)
            || osfwb(fp, val_.bytes_.buf_, val_.bytes_.len_))
            return 9;
        break;

    case HTML_DCFG_TYPE_EOF:
    case HTML_DCFG_TYPE_NULL:
        /* these types don't have any extra data */
        break;
    }

    /* success */
    return 0;
}

/*
 *   write the end-of-file marker 
 */
int CHtmlHashEntryConfig::write_eof_marker(osfildef *fp)
{
    uchar buf[1];

    /* write the special type marker for end of file */
    buf[0] = HTML_DCFG_TYPE_EOF;
    if (osfwb(fp, buf, 1))
        return 1;

    /* success */
    return 0;
}

/*
 *   read a variable from a file 
 */
int CHtmlHashEntryConfig::read_from_file(osfildef *fp,
                                         CHtmlHashEntryConfig **new_entry)
{
    size_t keylen;
    textchar_t keyname[HTML_DCFG_KEY_MAX];
    uchar buf[16];

    /* read the type */
    if (osfrb(fp, buf, 1))
        return 1;

    /* if that's the end of the file, return success with a null entry */
    if (buf[0] == HTML_DCFG_TYPE_EOF)
    {
        *new_entry = 0;
        return 0;
    }

    /* read the key's length and the key string */
    if (osfrb(fp, buf + 1, 2)
        || (keylen = (size_t)osrp2(buf + 1)) > sizeof(keyname)
        || osfrb(fp, keyname, keylen))
        return 2;

    /* read the value based on the type */
    switch(buf[0])
    {
    case HTML_DCFG_TYPE_INT:
        if (osfrb(fp, buf, 4))
            return 3;
        *new_entry = new CHtmlHashEntryConfig(keyname, keylen, TRUE,
                                              (int)osrp4(buf));
        break;

    case HTML_DCFG_TYPE_RECT:
        {
            CHtmlRect rect;

            /* read the left,top,right,bottom values */
            if (osfrb(fp, buf, 16))
                return 4;

            /* set up the rectangle */
            rect.set(osrp4(buf), osrp4(buf+4), osrp4(buf+8), osrp4(buf+12));

            /* create the entry */
            *new_entry =
                new CHtmlHashEntryConfig(keyname, keylen, TRUE, &rect);
        }
        break;

    case HTML_DCFG_TYPE_STRLIST:
        {
            CHtmlDcfgString *head;
            CHtmlDcfgString *tail;
            
            for (head = tail = 0 ;; )
            {
                size_t len;
                size_t rem;
                CHtmlDcfgString *cur;
                
                /* 
                 *   read the length of the next string - this length
                 *   includes the null terminator, so if the length is
                 *   zero, we're done 
                 */
                if (osfrb(fp, buf, 4))
                    return 5;
                len = (size_t)osrp4(buf);
                if (len == 0)
                    break;

                /* create a new string list entry */
                cur = new CHtmlDcfgString();

                /* link it in to the list */
                if (tail == 0)
                    head = cur;
                else
                    tail->next_ = cur;
                tail = cur;

                /* read the value */
                for (rem = len ; rem != 0 ; )
                {
                    textchar_t valbuf[512];
                    size_t curlen;
                    
                    /* read as much as possible, up to the buffer size */
                    curlen = rem;
                    if (curlen > sizeof(valbuf))
                        curlen = sizeof(valbuf);
                    if (osfrb(fp, valbuf, curlen))
                        return 6;

                    /* deduct the amount read from the total */
                    rem -= curlen;

                    /* append this to the string */
                    cur->str_.append(valbuf, curlen);
                }
            }

            /* create the entry */
            *new_entry =
                new CHtmlHashEntryConfig(keyname, keylen, TRUE, head);
        }
        break;

    case HTML_DCFG_TYPE_COLOR:
        if (osfrb(fp, buf, 6))
            return 7;
        *new_entry = new CHtmlHashEntryConfig(keyname, keylen, TRUE);
        (*new_entry)->setval_color(HTML_make_color(
            osrp2(buf), osrp2(buf+2), osrp2(buf+4)));
        break;

    case HTML_DCFG_TYPE_BYTES:
        {
            size_t len;
            void *bufptr;

            /* read the length */
            if (osfrb(fp, buf, 4))
                return 8;
            len = (size_t)osrp4(buf);

            /* create the entry and allocate buffer space */
            *new_entry = new CHtmlHashEntryConfig(keyname, keylen, TRUE);
            (*new_entry)->setval_bytes((size_t)osrp4(buf));

            /* get the entry's buffer */
            if ((*new_entry)->getval_bytes_buf(&bufptr))
                return 9;

            /* read directly into the entry's buffer */
            if (osfrb(fp, bufptr, len))
                return 10;
        }
        break;

    default:
        /* invalid type */
        return 10;
    }

    /* success */
    return 0;
}

/*
 *   set a string value 
 */
void CHtmlHashEntryConfig::setval(int stridx, const textchar_t *val,
                                  size_t vallen)
{
    CHtmlDcfgString *cur;
    CHtmlDcfgString *prv;

    /* make sure we have the correct type */
    change_type(HTML_DCFG_TYPE_STRLIST);
    
    /* look up the existing string at the given index */
    for (prv = 0, cur = val_.strlist_ ; cur != 0 && stridx != 0 ;
         --stridx, prv = cur, cur = cur->next_) ;

    /* 
     *   if we don't have any entry at the given index yet, add entries
     *   until we have one at the given index 
     */
    while (cur == 0)
    {
        /* allocate a new string and add it to the end of the list */
        cur = new CHtmlDcfgString();
        if (prv != 0)
            prv->next_ = cur;
        else
            val_.strlist_ = cur;
        prv = cur;

        /* if this is at the desired index, we're done */
        if (stridx == 0)
            break;

        /* move on to the next entry */
        cur = 0;
        --stridx;
    }

    /* set this string's value */
    cur->str_.set(val, vallen);
}

/* 
 *   get the number of elements in a string list 
 */
int CHtmlHashEntryConfig::get_strlist_cnt()
{
    int cnt;
    CHtmlDcfgString *cur;
    
    /* if it's not a string list, there aren't any elements */
    if (type_ != HTML_DCFG_TYPE_STRLIST)
        return 0;

    /* loop through my list and count the elements */
    for (cnt = 0, cur = val_.strlist_ ; cur != 0 ; cur = cur->next_, ++cnt) ;

    /* return the count */
    return cnt;
}

/*
 *   get a string value 
 */
int CHtmlHashEntryConfig::getval(int stridx, textchar_t *dstbuf,
                                 size_t dstbuflen, size_t *result_len)
{
    CHtmlDcfgString *cur;
    size_t len;

    /* if we don't have a string list, we can't do this */
    if (type_ != HTML_DCFG_TYPE_STRLIST)
        return 1;

    /* look up the existing string at the given index */
    for (cur = val_.strlist_ ; cur != 0 && stridx != 0 ;
         --stridx, cur = cur->next_) ;

    /* if we don't have a string at this index, it's an error */
    if (cur == 0 || cur->str_.get() == 0)
        return 1;

    /* set the value's length if desired */
    len = get_strlen(cur->str_.get());
    if (result_len != 0)
        *result_len = len;

    /* return the value if desired */
    if (dstbuf != 0 && dstbuflen > 0)
    {
        /* limit the copy to the size of the return buffer */
        len *= sizeof(textchar_t);
        if (len > dstbuflen)
            len = dstbuflen - sizeof(textchar_t);

        /* copy the value */
        memcpy(dstbuf, cur->str_.get(), len);

        /* set the null terminator */
        dstbuf[len] = '\0';
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger configuration manager implementation 
 */

/* file signature */
const textchar_t CHtmlDebugConfig::filesig[] =
    "TADS Debugger Config File v2.2.3\n\r\032";

CHtmlDebugConfig::CHtmlDebugConfig()
{
    /* create the hash table */
    hashtab_ = new CHtmlHashTable(256, new CHtmlHashFuncCS());
}

CHtmlDebugConfig::~CHtmlDebugConfig()
{
    /* delete the hash table */
    delete hashtab_;
}

/*
 *   load from a file 
 */
int CHtmlDebugConfig::load_file(const textchar_t *fname)
{
    osfildef *fp;
    uchar buf[128];
    
    /* open the file - give up immediately if we can't */
    fp = osfoprb(fname, OSFTBIN);
    if (fp == 0)
        return 1;

    /* 
     *   check the signature; if it doesn't look like one of our files,
     *   don't bother proceeding 
     */
    if (osfrb(fp, buf, get_strlen(filesig))
        || memcmp(buf, filesig, get_strlen(filesig)) != 0)
    {
        osfcls(fp);
        return 2;
    }

    /* the file looks okay - get rid of all existing entries */
    hashtab_->delete_all_entries();

    /* load everything in the file */
    for (;;)
    {
        CHtmlHashEntryConfig *new_entry;
        
        /* read the next item */
        if (CHtmlHashEntryConfig::read_from_file(fp, &new_entry))
        {
            osfcls(fp);
            return 3;
        }

        /* 
         *   if we didn't get an entry, we've reached the end of the file
         *   successfully 
         */
        if (new_entry == 0)
            break;

        /* add this new element to the hash table */
        hashtab_->add(new_entry);
    }

    /* done with the file */
    osfcls(fp);

    /* success */
    return 0;
}

/* callback context structure for enumerating entries during file save */
struct save_file_enum_ctx_t
{
    /* file we're writing to */
    osfildef *fp_;

    /* flag indicating that an error occurred during writing */
    int error_;
};

/*
 *   save to a file 
 */
int CHtmlDebugConfig::save_file(const textchar_t *fname)
{
    osfildef *fp;
    save_file_enum_ctx_t cbctx;

    /* open the file - give up immediately if we can't */
    fp = osfopwb(fname, OSFTBIN);
    if (fp == 0)
        return 1;

    /* write the signature */
    if (osfwb(fp, filesig, get_strlen(filesig)))
    {
        osfcls(fp);
        return 2;
    }

    /* save everything to the file */
    cbctx.fp_ = fp;
    cbctx.error_ = 0;
    hashtab_->enum_entries(&save_enumcb, &cbctx);

    /* write the terminating entry, so we can detect the end of the file */
    if (CHtmlHashEntryConfig::write_eof_marker(fp))
    {
        osfcls(fp);
        return 3;
    }

    /* done with the file */
    osfcls(fp);

    /* return error indication from callback */
    return cbctx.error_;
}

/*
 *   Callback for enumerating entries and saving them to a file 
 */
void CHtmlDebugConfig::save_enumcb(void *ctx0, CHtmlHashEntry *entry)
{
    save_file_enum_ctx_t *ctx = (save_file_enum_ctx_t *)ctx0;
    int err;

    /* tell the entry to write itself out */
    err = ((CHtmlHashEntryConfig *)entry)->write_to_file(ctx->fp_);

    /* if an error occurred, note it in the context */
    if (err != 0)
        ctx->error_ = err;
}


/*
 *   set an integer value 
 */
void CHtmlDebugConfig::setval(const textchar_t *varname,
                              const textchar_t *element,
                              int val)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval(val);
}

/*
 *   set a string value 
 */
void CHtmlDebugConfig::setval(const textchar_t *varname,
                              const textchar_t *element,
                              int stridx, const textchar_t *val)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval(stridx, val);
}

/*
 *   set a string value 
 */
void CHtmlDebugConfig::setval(const textchar_t *varname,
                              const textchar_t *element,
                              int stridx, const textchar_t *val,
                              size_t vallen)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval(stridx, val, vallen);
}

/*
 *   set a rectangle value 
 */
void CHtmlDebugConfig::setval(const textchar_t *varname,
                              const textchar_t *element,
                              const CHtmlRect *val)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval(val);
}

void CHtmlDebugConfig::setval_color(const textchar_t *varname,
                                    const textchar_t *element,
                                    HTML_color_t color)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval_color(color);
}

void CHtmlDebugConfig::setval_bytes(const textchar_t *varname,
                                    const textchar_t *element,
                                    const void *buf, size_t len)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, TRUE)) != 0)
        entry->setval_bytes(buf, len);
}


/*
 *   clear a string list 
 */
void CHtmlDebugConfig::clear_strlist(const textchar_t *varname,
                                     const textchar_t *element)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        entry->clear_strlist();
}

/* 
 *   get the number of elements in a string list 
 */
int CHtmlDebugConfig::get_strlist_cnt(const textchar_t *varname,
                                      const textchar_t *element)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->get_strlist_cnt();
    else
        return 0;
}

/* 
 *   get an integer value 
 */
int CHtmlDebugConfig::getval(const textchar_t *varname,
                             const textchar_t *element, int *val)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval(val);
    else
        return 1;
}

/* 
 *   get a string value
 */
int CHtmlDebugConfig::getval(const textchar_t *varname,
                             const textchar_t *element,
                             int stridx, textchar_t *dstbuf,
                             size_t dstbuflen, size_t *resultlen)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval(stridx, dstbuf, dstbuflen, resultlen);
    else
        return 1;
}

/* 
 *   get a rectangle value 
 */
int CHtmlDebugConfig::getval(const textchar_t *varname,
                             const textchar_t *element, CHtmlRect *val)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval(val);
    else
        return 1;
}

/* 
 *   get a color value 
 */
int CHtmlDebugConfig::getval_color(const textchar_t *varname,
                                   const textchar_t *element,
                                   HTML_color_t *color)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval_color(color);
    else
        return 1;
}

/* 
 *   get a byte array
 */
int CHtmlDebugConfig::getval_bytes(const textchar_t *varname,
                                   const textchar_t *element,
                                   void *buf, size_t buflen)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval_bytes(buf, buflen);
    else
        return 1;
}

/* 
 *   get a byte array's length
 */
int CHtmlDebugConfig::getval_bytes_len(const textchar_t *varname,
                                       const textchar_t *element,
                                       size_t *len)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval_bytes_len(len);
    else
        return 1;
}

/* 
 *   get a byte array's buffer pointer
 */
int CHtmlDebugConfig::getval_bytes_buf(const textchar_t *varname,
                                       const textchar_t *element,
                                       void **bufptr)
{
    CHtmlHashEntryConfig *entry;

    if ((entry = get_entry(varname, element, FALSE)) != 0)
        return entry->getval_bytes_buf(bufptr);
    else
        return 1;
}

/*
 *   Get an entry 
 */
CHtmlHashEntryConfig *CHtmlDebugConfig::get_entry(
    const textchar_t *varname, const textchar_t *element, int create)
{
    /* get the entry from our hash table */
    return get_entry_from(hashtab_, varname, element, create);
}

/*
 *   Get an entry from a given hash table 
 */
CHtmlHashEntryConfig *CHtmlDebugConfig::get_entry_from(
    CHtmlHashTable *hashtab, const textchar_t *varname,
    const textchar_t *element, int create)
{
    CHtmlHashEntryConfig *entry;
    textchar_t key[HTML_DCFG_KEY_MAX];
    
    /* build the composite key from the variable name and element ID */
    do_strcpy(key, varname);
    if (element != 0)
    {
        textchar_t *p;

        /* add a colon and the element ID */
        p = key + get_strlen(key);
        *p++ = ':';
        do_strcpy(p, element);
    }

    /* look it up in the hash table */
    entry = (CHtmlHashEntryConfig *)hashtab->find(key, get_strlen(key));

    /* if we didn't find it, create a new entry if desired */
    if (entry == 0 && create)
    {
        /* create the new entry */
        entry = new CHtmlHashEntryConfig(key, get_strlen(key), TRUE);

        /* add it to the hash table */
        hashtab->add(entry);
    }

    /* return the result */
    return entry;
}

/* 
 *   delete an entry 
 */
void CHtmlDebugConfig::delete_val(const textchar_t *varname,
                                  const textchar_t *element)
{
    CHtmlHashEntryConfig *entry;

    /* 
     *   Look up the entry; we obviously have no reason to create it if it
     *   doesn't exist, since we're just going to delete it.  If we do find
     *   the entry, remove it and delete it.  
     */
    if ((entry = get_entry(varname, element, FALSE)) != 0)
    {
        /* drop the entry from the table */
        hashtab_->remove(entry);

        /* delete the entry */
        delete entry;
    }
}
