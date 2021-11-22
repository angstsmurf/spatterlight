#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlhash.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlhash.cpp - hash table implementation
Function
  
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#include <assert.h>
#include <memory.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Simple case-insensitive hash function implementation.
 */
unsigned int CHtmlHashFuncCI::compute_hash(const textchar_t *s, size_t l)
{
    unsigned int acc;

    /*
     *   add up all the character values in the string, converting all
     *   characters to upper-case 
     */
    for (acc = 0 ; l != 0 ; ++s, --l)
        acc += is_lower(*s) ? to_upper(*s) : *s;

    /* return the accumulated value */
    return acc;
}

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-sensitive hash function implementation 
 */
unsigned int CHtmlHashFuncCS::compute_hash(const textchar_t *s, size_t l)
{
    unsigned int acc;

    /*
     *   add up all the character values in the string, treating case as
     *   significant 
     */
    for (acc = 0 ; l != 0 ; ++s, --l)
        acc += *s;

    /* return the accumulated value */
    return acc;
}


/* ------------------------------------------------------------------------ */
/*
 *   Hash table symbol entry.  This is an abstract class; subclasses must
 *   provide a symbol-matching method.  
 */
CHtmlHashEntry::CHtmlHashEntry(const textchar_t *str, size_t len, int copy)
{
    /* not linked into a list yet */
    nxt_ = 0;

    /* see if we can use the original string or need to make a private copy */
    if (copy)
    {
        textchar_t *buf;

        /* allocate space for a copy */
        buf = new textchar_t[len];

        /* copy it into our buffer */
        memcpy(buf, str, len * sizeof(*buf));

        /* remember it */
        str_ = buf;
    }
    else
    {
        /* we can use the original */
        str_ = str;
    }

    /* remember the length */
    len_ = len;

    /* remember whether or not we own the string */
    copy_ = copy;
}

CHtmlHashEntry::~CHtmlHashEntry()
{
    /* if we made a private copy of the string, we own it, so delete it */
    if (copy_)
        delete [] str_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CHtmlHashEntry providing a case-insensitive
 *   symbol match implementation 
 */
int CHtmlHashEntryCI::matches(const textchar_t *str, size_t len)
{
    /*
     *   it's a match if the strings are the same length and all
     *   characters match, ignoring case 
     */
    return (len == len_
            && memicmp(str, str_, len * sizeof(*str)) == 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CHtmlHashEntry providing a case-sensitive symbol
 *   match implementation 
 */
int CHtmlHashEntryCS::matches(const textchar_t *str, size_t len)
{
    /*
     *   it's a match if the strings are the same length and all
     *   characters match, treating case as significant 
     */
    return (len == len_
            && memcmp(str, str_, len * sizeof(*str)) == 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash table 
 */

CHtmlHashTable::CHtmlHashTable(int hash_table_size,
                               CHtmlHashFunc *hash_function)
{
    CHtmlHashEntry **entry;
    size_t i;

    /* make sure it's a power of two */
    assert(is_power_of_two(hash_table_size));

    /* make sure we got a hash function */
    assert(hash_function != 0);

    /* save the hash function */
    hash_function_ = hash_function;

    /* allocate the table */
    table_ = new CHtmlHashEntry *[hash_table_size];
    table_size_ = hash_table_size;

    /* clear the table */
    for (entry = table_, i = 0 ; i < table_size_ ; ++i, ++entry)
        *entry = 0;
}

CHtmlHashTable::~CHtmlHashTable()
{
    /* delete the hash function object */
    delete hash_function_;

    /* delete each entry in the hash table */
    delete_all_entries();

    /* delete the hash table */
    delete [] table_;
}

/*
 *   delete all entries in the hash table, but keep the table itself 
 */
void CHtmlHashTable::delete_all_entries()
{
    CHtmlHashEntry **tableptr;
    size_t i;

    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        CHtmlHashEntry *entry;
        CHtmlHashEntry *nxt;

        /* delete each entry in the list at this element */
        for (entry = *tableptr ; entry ; entry = nxt)
        {
            /* remember the next entry */
            nxt = entry->nxt_;

            /* delete this entry */
            delete entry;
        }

        /* there's nothing at this table entry now */
        *tableptr = 0;
    }
}

/*
 *   Verify that a value is a power of two.  Hash table sizes must be
 *   powers of two. 
 */
int CHtmlHashTable::is_power_of_two(int n)
{
    /* divide by two until we have an odd number */
    while ((n & 1) == 0) n >>= 1;

    /* make sure the result is 1 */
    return (n == 1);
}

/*
 *   Compute the hash value for an entry 
 */
unsigned int CHtmlHashTable::compute_hash(CHtmlHashEntry *entry)
{
    return compute_hash(entry->getstr(), entry->getlen());
}

/*
 *   Compute the hash value for a string 
 */
unsigned int CHtmlHashTable::compute_hash(const textchar_t *str, size_t len)
{
    return (hash_function_->compute_hash(str, len) & (table_size_ - 1));
}

/*
 *   Add an object to the table
 */
void CHtmlHashTable::add(CHtmlHashEntry *entry)
{
    unsigned int hash;

    /* compute the hash value for this entry */
    hash = compute_hash(entry);

    /* link it into the slot for this hash value */
    entry->nxt_ = table_[hash];
    table_[hash] = entry;
}

/*
 *   Remove an object 
 */
void CHtmlHashTable::remove(CHtmlHashEntry *entry)
{
    unsigned int hash;

    /* compute the hash value for this entry */
    hash = compute_hash(entry);

    /* 
     *   if it's the first item in the chain, advance the head over it;
     *   otherwise, we'll need to find the previous item to unlink it 
     */
    if (table_[hash] == entry)
    {
        /* it's the first item - simply advance the head to the next item */
        table_[hash] = entry->nxt_;
    }
    else
    {
        CHtmlHashEntry *prv;
        
        /* find the previous item in the list for this hash value */
        for (prv = table_[hash] ; prv != 0 && prv->nxt_ != entry ;
             prv = prv->nxt_) ;

        /* if we found it, unlink this item */
        if (prv != 0)
            prv->nxt_ = entry->nxt_;
    }
}

/*
 *   Find an object in the table matching a given string. 
 */
CHtmlHashEntry *CHtmlHashTable::find(const textchar_t *str, size_t len)
{
    unsigned int hash;
    CHtmlHashEntry *entry;

    /* compute the hash value for this entry */
    hash = compute_hash(str, len);

    /* scan the list at this hash value looking for a match */
    for (entry = table_[hash] ; entry ; entry = entry->nxt_)
    {
        /* if this one matches, return it */
        if (entry->matches(str, len))
            return entry;
    }

    /* didn't find anything */
    return 0;
}

/*
 *   Find an object in the table matching a given leading substring.
 *   We'll return the longest-named entry that matches a leading substring
 *   of the given string.  For example, if there's are entires A, AB, ABC,
 *   and ABCE, and this routine is called to find something matching
 *   ABCDEFGH, we'll return ABC as the match (not ABCE, since it doesn't
 *   match any leading substring of the given string, and not A or AB,
 *   even though they match, since ABC also matches and it's longer).  
 */
CHtmlHashEntry *CHtmlHashTable::find_leading_substr(
    const textchar_t *str, size_t len)
{
    size_t sublen;
    CHtmlHashEntry *entry;

    /* 
     *   try to find each leading substring, starting with the longest,
     *   decreasing by one character on each iteration, until we've used
     *   the whole string 
     */
    for (sublen = len ; sublen > 0 ; --sublen)
    {
        /* if this substring matches, use it */
        if ((entry = find(str, sublen)) != 0)
            return entry;
    }

    /* we didn't find it */
    return 0;
}

/*
 *   Enumerate all entries 
 */
void CHtmlHashTable::enum_entries(void (*func)(void *, CHtmlHashEntry *),
                                  void *ctx)
{
    CHtmlHashEntry **tableptr;
    size_t i;

    /* go through each hash value */
    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        CHtmlHashEntry *entry;

        /* go through each entry at this hash value */
        for (entry = *tableptr ; entry ; entry = entry->nxt_)
        {
            /* invoke the callback on this entry */
            (*func)(ctx, entry);
        }
    }
}

