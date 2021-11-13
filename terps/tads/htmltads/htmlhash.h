/* $Header: d:/cvsroot/tads/html/htmlhash.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlhash.h - hash table implementation
Function
  
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#ifndef HTMLHASH_H
#define HTMLHASH_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Hash function interface class.  Hash table clients must implement an
 *   appropriate hash function to use with the hash table; this abstract
 *   class provides the necessary interface.  
 */
class CHtmlHashFunc
{
public:
    virtual ~CHtmlHashFunc() { }

    virtual unsigned int compute_hash(const textchar_t *str, size_t len) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table symbol entry.  This is an abstract class; subclasses must
 *   provide a symbol-matching method.  
 */
class CHtmlHashEntry
{
public:
    /*
     *   Construct the hash entry.  'copy' indicates whether we should
     *   make a private copy of the value; if not, the caller must keep
     *   the original string around as long as this hash entry is around.
     *   If 'copy' is true, we'll make a private copy of the string
     *   immediately, so the caller need not keep it around after
     *   constructing the entry.  
     */
    CHtmlHashEntry(const textchar_t *str, size_t len, int copy);
    virtual ~CHtmlHashEntry();

    /* determine if this entry matches a given string */
    virtual int matches(const textchar_t *str, size_t len) = 0;

    /* list link */
    CHtmlHashEntry *nxt_;

    /* get the string pointer and length */
    const textchar_t *getstr() const { return str_; }
    size_t getlen() const { return len_; }

protected:
    const textchar_t *str_;
    size_t len_;
    int copy_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table 
 */
class CHtmlHashTable
{
public:
    /*
     *   Construct a hash table.  IMPORTANT: the hash table object takes
     *   ownership of the hash function object, so the hash table object
     *   will delete the hash function object when the table is deleted.
     */
    CHtmlHashTable(int hash_table_size, CHtmlHashFunc *hash_function);

    /* delete the hash table */
    ~CHtmlHashTable();

    /*
     *   Add a symbol.  If 'copy' is true, it means that we need to make
     *   a private copy of the string; otherwise, the caller must ensure
     *   that the string remains valid as long as the hash table entry
     *   remains valid, since we'll just store a pointer to the original
     *   string.  IMPORTANT: the hash table takes over ownership of the
     *   hash table entry; the hash table will delete this object when the
     *   hash table is deleted, so the client must not delete the entry
     *   once it's been added to the table.  
     */
    void add(CHtmlHashEntry *entry);

    /*
     *   Remove an object from the cache.  This routine does not delete
     *   the object. 
     */
    void remove(CHtmlHashEntry *entry);

    /*
     *   Delete all entries in the table 
     */
    void delete_all_entries();

    /*
     *   Find an entry in the table matching the given string 
     */
    CHtmlHashEntry *find(const textchar_t *str, size_t len);

    /* 
     *   find an entry that matches the longest leading substring of the
     *   given string 
     */
    CHtmlHashEntry *find_leading_substr(const textchar_t *str, size_t len);

    /*
     *   Enumerate all entries, invoking a callback for each entry in the
     *   table 
     */
    void enum_entries(void (*func)(void *ctx, class CHtmlHashEntry *entry),
                      void *ctx);

private:
    /* internal service routine for checking hash table sizes for validity */
    int is_power_of_two(int n);

    /* compute the hash value for an entry/a string */
    unsigned int compute_hash(CHtmlHashEntry *entry);
    unsigned int compute_hash(const textchar_t *str, size_t len);

    /* the table of hash entries */
    CHtmlHashEntry **table_;
    size_t table_size_;

    /* hash function */
    CHtmlHashFunc *hash_function_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-insensitive hash function 
 */
class CHtmlHashFuncCI: public CHtmlHashFunc
{
public:
    unsigned int compute_hash(const textchar_t *str, size_t len);
};

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-sensitive hash function implementation 
 */
class CHtmlHashFuncCS: public CHtmlHashFunc
{
public:
    unsigned int compute_hash(const textchar_t *str, size_t len);
};

/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CHtmlHashEntry providing a case-insensitive
 *   symbol match implementation 
 */
class CHtmlHashEntryCI: public CHtmlHashEntry
{
public:
    CHtmlHashEntryCI(const textchar_t *str, size_t len, int copy)
        : CHtmlHashEntry(str, len, copy) { }

    virtual int matches(const textchar_t *str, size_t len);
};

/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CHtmlHashEntry providing a case-sensitive symbol
 *   match implementation 
 */
class CHtmlHashEntryCS: public CHtmlHashEntry
{
public:
    CHtmlHashEntryCS(const textchar_t *str, size_t len, int copy)
        : CHtmlHashEntry(str, len, copy) { }

    virtual int matches(const textchar_t *str, size_t len);
};


#endif /* HTMLHASH_H */
