#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsreg.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsreg.cpp - registry
Function
  
Notes
  
Modified
  10/28/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif

/*
 *   open a key 
 */
HKEY CTadsRegistry::open_key(HKEY base_key, const textchar_t *key_name,
                             DWORD *disposition, int create)
{
    LONG err;
    HKEY ret_key;
    
    if (create)
    {
        err = RegCreateKeyEx(base_key, key_name, 0, 0, 0, KEY_ALL_ACCESS,
                             0, &ret_key, disposition);
    }
    else
    {
        err = RegOpenKeyEx(base_key, key_name, 0, KEY_ALL_ACCESS, &ret_key);
    }

    /* return the key if we were successful */
    return (err == ERROR_SUCCESS) ? ret_key : 0;
}

/*
 *   close a key 
 */
void CTadsRegistry::close_key(HKEY key)
{
    RegCloseKey(key);
}

/*
 *   Get a key's value 
 */
int CTadsRegistry::query_key(HKEY key, const textchar_t *valname,
                             textchar_t *buf, size_t *bufsiz)
{
    DWORD typ;
    DWORD datasiz;

    /* query the value into the buffer */
    datasiz = *bufsiz;
    if (RegQueryValueEx(key, valname, 0, &typ, (BYTE *)buf, &datasiz)
        != ERROR_SUCCESS || typ != REG_SZ)
        return 1;

    /* set the size that we read */
    *bufsiz = (size_t)datasiz;

    /* success */
    return 0;
}

/*
 *   Get a key's value as a long 
 */
long CTadsRegistry::query_key_long(HKEY key, const textchar_t *valname)
{
    char buf[128];
    size_t bufsiz = sizeof(buf);

    /* get the key text */
    if (query_key(key, valname, buf, &bufsiz))
        return 0;

    /* parse the string into a long */
    return get_atol(buf);
}

/*
 *   Get a key's value as a boolean 
 */
int CTadsRegistry::query_key_bool(HKEY key, const textchar_t *valname)
{
    char buf[128];
    size_t bufsiz = sizeof(buf);

    /* get the key text */
    if (query_key(key, valname, buf, &bufsiz))
        return 0;

    /* if it starts with 'y' or 'Y', it's true, otherwise it's false */
    return (buf[0] == 'y' || buf[0] == 'Y');
}

/*
 *   Get a key as a string 
 */
size_t CTadsRegistry::query_key_str(HKEY key, const textchar_t *valname,
                                    textchar_t *buf, size_t bufsiz)
{
    /* get the key text */
    if (query_key(key, valname, buf, &bufsiz))
    {
        /* no such value - clear the buffer and indicate an empty string */
        buf[0] = '\0';
        return 0;
    }

    /* return the length, minus the null terminator */
    return bufsiz - 1;
}

/*
 *   Get a key value as binary data 
 */
size_t CTadsRegistry::query_key_binary(HKEY key, const textchar_t *valname,
                                       void *buf, size_t bufsiz)
{
    DWORD typ;
    DWORD datasiz;

    datasiz = bufsiz;
    if (RegQueryValueEx(key, valname, 0, &typ, (BYTE *)buf, &datasiz)
        != ERROR_SUCCESS || typ != REG_BINARY)
        return 0;

    /* return the length of the buffer read */
    return (size_t)datasiz;
}

void CTadsRegistry::set_key_long(HKEY key, const textchar_t *valname,
                                 long val)
{
    textchar_t buf[128];

    /* generate a string for the number and save the string */
    sprintf(buf, "%ld", val);
    RegSetValueEx(key, valname, 0, REG_SZ,
                  (BYTE *)buf, get_strlen(buf) + sizeof(textchar_t));
}

void CTadsRegistry::set_key_str(HKEY key, const textchar_t *valname,
                                const textchar_t *str, size_t len)
{
    CStringBuf buf(str, len);
    RegSetValueEx(key, valname, 0, REG_SZ,
                  (BYTE *)buf.get(), len + sizeof(textchar_t));
}

void CTadsRegistry::set_key_bool(HKEY key, const textchar_t *valname, int val)
{
    textchar_t *valstr;
    size_t vallen;

    if (val)
    {
        valstr = "Yes";
        vallen = 4 * sizeof(textchar_t);
    }
    else
    {
        valstr = "No";
        vallen = 3 * sizeof(textchar_t);
    }

    RegSetValueEx(key, valname, 0, REG_SZ, (BYTE *)valstr, vallen);
}

void CTadsRegistry::set_key_binary(HKEY key, const textchar_t *valname,
                                   void *buf, size_t bufsiz)
{
    RegSetValueEx(key, valname, 0, REG_BINARY, (BYTE *)buf, bufsiz);
}


/*
 *   Check to see if a value is set in the registry 
 */
int CTadsRegistry::value_exists(HKEY key, const textchar_t *valname)
{
    DWORD datasize;
    DWORD datatype;

    return (RegQueryValueEx(key, valname, 0, &datatype, 0, &datasize)
            == ERROR_SUCCESS);
}

/* ------------------------------------------------------------------------ */
/*
 *   Rename a key 
 */
int CTadsRegistry::rename_key(HKEY from_base_key, const textchar_t *from_key,
                              HKEY to_base_key, const textchar_t *to_key)
{
    HKEY key;
    DWORD disp;
    
    /* if the new key already exists, return failure */
    key = open_key(to_base_key, to_key, &disp, FALSE);
    if (key != 0)
    {
        /* it exists - close the key and return failure */
        close_key(key);
        return 1;
    }

    /* copy the tree and its subkeys to the new key */
    if (copy_key(from_base_key, from_key, to_base_key, to_key))
        return 2;

    /* delete the original key */
    if (delete_key(from_base_key, from_key))
        return 3;

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete a key and its subkeys 
 */
int CTadsRegistry::delete_key(HKEY base_key, const textchar_t *key_name)
{
    HKEY key;
    DWORD disp;
    DWORD idx;

    /* open the key to be deleted */
    key = open_key(base_key, key_name, &disp, FALSE);
    if (key == 0)
        return 1;

    /* enumerate its subkeys and recursively delete them */
    for (idx = 0 ; ; ++idx)
    {
        DWORD len;
        char subkey[128];
        FILETIME ft;
        DWORD err;
        
        /* get the next subkey */
        len = sizeof(subkey);
        err = RegEnumKeyEx(key, idx, subkey, &len, 0, 0, 0, &ft);

        /* if we're out of keys, we can stop our scan */
        if (err == ERROR_NO_MORE_ITEMS)
            break;

        /* if any other error occurred, return failure */
        if (err != ERROR_SUCCESS)
        {
            /* error - close our base key and return failure */
            close_key(key);
            return 2;
        }

        /* delete this subkey */
        if (delete_key(key, subkey))
        {
            /* error - close our base key and return failure */
            close_key(key);
            return 3;
        }
    }

    /* 
     *   we've deleted all of the subkeys, so delete our main key - note that
     *   we still have it open from the enumeration, so close it first 
     */
    close_key(key);
    if (RegDeleteKey(base_key, key_name) != ERROR_SUCCESS)
        return 4;

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy a key, including all of its values and subkeys, to a new key. 
 */
int CTadsRegistry::copy_key(HKEY from_base_key, const textchar_t *from_name,
                            HKEY to_base_key, const textchar_t *to_name)
{
    HKEY from_key;
    HKEY to_key;
    DWORD disp;
    DWORD idx;

    /* if the destination key already exists, return failure */
    to_key = open_key(to_base_key, to_name, &disp, FALSE);
    if (to_key != 0)
    {
        /* it exists - close the key and return failure */
        close_key(to_key);
        return 1;
    }

    /* open the 'from' key so that we can enumerate its values and subkeys */
    from_key = open_key(from_base_key, from_name, &disp, FALSE);
    if (from_key == 0)
        return 2;

    /* 
     *   create the 'to' key, so we can write the copied subkeys and values
     *   to the new key 
     */
    to_key = open_key(to_base_key, to_name, &disp, TRUE);
    if (to_key == 0)
        return 3;

    /* recursively copy the subkeys */
    for (idx = 0 ; ; ++idx)
    {
        DWORD len;
        char subkey[128];
        FILETIME ft;
        DWORD err;

        /* get the next subkey */
        len = sizeof(subkey);
        err = RegEnumKeyEx(from_key, idx, subkey, &len, 0, 0, 0, &ft);

        /* if we're out of keys, stop scanning */
        if (err == ERROR_NO_MORE_ITEMS)
            break;

        /* on any other error, return failure */
        if (err != ERROR_SUCCESS)
        {
            /* error - close our base keys and return failure */
            close_key(from_key);
            close_key(to_key);
            return 4;
        }

        /* copy this subkey */
        if (copy_key(from_key, subkey, to_key, subkey))
        {
            /* error - close our base keys and return failure */
            close_key(from_key);
            close_key(to_key);
            return 5;
        }
    }

    /* copy the values */
    for (idx = 0 ; ; ++idx)
    {
        char val_name[1024];
        BYTE val[4096];
        DWORD val_name_len;
        DWORD val_len;
        DWORD typ;
        DWORD err;
        
        /* get the next value */
        val_name_len = sizeof(val_name);
        val_len = sizeof(val);
        err = RegEnumValue(from_key, idx, val_name, &val_name_len, 0,
                           &typ, val, &val_len);

        /* if we're out of values, stop scanning */
        if (err == ERROR_NO_MORE_ITEMS)
            break;

        /* on any other error, return failure */
        if (err != ERROR_SUCCESS)
        {
            /* couldn't write the value - return failure */
            close_key(from_key);
            close_key(to_key);
            return 6;
        }

        /* store the value in the destination key */
        if (RegSetValueEx(to_key, val_name, 0, typ, val, val_len)
            != ERROR_SUCCESS)
        {
            /* couldn't write the value - return failure */
            close_key(from_key);
            close_key(to_key);
            return 7;
        }
    }

    /* all done - close the keys */
    close_key(from_key);
    close_key(to_key);

    /* success */
    return 0;
}

