/* $Header: d:/cvsroot/tads/html/win32/tadsreg.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsreg.h - registry operations
Function
  
Notes
  
Modified
  10/28/97 MJRoberts  - Creation
*/

#ifndef TADSREG_H
#define TADSREG_H

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

class CTadsRegistry
{
public:
    /* 
     *   open a registry key; if 'create' is true, we'll create the key if
     *   it's not already present 
     */
    static HKEY open_key(HKEY base_key, const textchar_t *key_name,
                         DWORD *disposition, int create);

    /* close a key */
    static void close_key(HKEY key);

    /* check if a value is set in the registry */
    static int value_exists(HKEY key, const textchar_t *valname);

    /* get a key's value */
    static long query_key_long(HKEY key, const textchar_t *valname);
    static size_t query_key_str(HKEY key, const textchar_t *valname,
                                textchar_t *buf, size_t bufsiz);
    static int query_key_bool(HKEY key, const textchar_t *valname);
    static size_t query_key_binary(HKEY key, const textchar_t *valname,
                                   void *buf, size_t bufsiz);

    /* set a key's value */
    static void set_key_long(HKEY key, const textchar_t *valname, long val);
    static void set_key_str(HKEY key, const textchar_t *valname,
                            const textchar_t *str, size_t len);
    static void set_key_bool(HKEY key, const textchar_t *valname, int val);
    static void set_key_binary(HKEY key, const textchar_t *valname,
                               void *buf, size_t bufsiz);

    /*
     *   Rename a key.  The registry API doesn't have a native way of doing
     *   this, so we make a deep copy of the key and then delete the existing
     *   key.  Returns zero on success, non-zero on failure.  
     */
    static int rename_key(HKEY from_base_key, const textchar_t *from_key,
                          HKEY to_base_key, const textchar_t *to_key);

    /*
     *   Delete a key and its subkeys.  On NT/2000/XP, we must manually
     *   delete all subkeys (recursively) before deleting a key.  Returns
     *   zero on success, non-zero on failure.  
     */
    static int delete_key(HKEY base_key, const textchar_t *key_name);

    /*
     *   Copy a key, recursively copying all subkeys. 
     */
    static int copy_key(HKEY from_base_key, const textchar_t *from_key,
                        HKEY to_base_key, const textchar_t *to_key);

private:
    static int query_key(HKEY key, const textchar_t *valname,
                         textchar_t *buf, size_t *bufsiz);
};

#endif /* TADSREG_H */

