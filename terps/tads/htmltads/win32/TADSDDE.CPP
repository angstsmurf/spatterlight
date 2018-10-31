#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsdde.cpp,v 1.1 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsdde.cpp - DDE support for TADS installer
Function
  
Notes
  
Modified
  05/17/98 MJRoberts  - Creation
*/

#include <Windows.h>
#include "tadsdde.h"


/*
 *   Generic DDE callback 
 */
static HDDEDATA CALLBACK dde_cb(UINT ttype, UINT fmt, HCONV hconv,
                                HSZ hsz1, HSZ hsz2, HDDEDATA hdata,
                                DWORD data1, DWORD data2)
{
    switch(ttype)
    {
    case XTYP_ADVDATA:
        return (HDDEDATA)DDE_FACK;

    default:
        return (HDDEDATA)0;
    }
}

/*
 *   Initialize a DDE conversation with a given service and topic.
 *   Returns zero on success, non-zero on failure.  If successful, the
 *   dde_info_t structure will be initialized for future dde operations.  
 */
unsigned dde_open_conv(dde_info_t *info, const char *service,
                       const char *topic)
{
    unsigned err;

    /* clear out the information structure */
    info->ddeinst_ = 0;
    info->hconv_ = 0;
    info->hservice_ = info->htopic_ = info->hitem_ = 0;

    /* initialize DDE */
    err = DdeInitialize(&info->ddeinst_, (PFNCALLBACK)&dde_cb,
                        APPCMD_CLIENTONLY | CBF_FAIL_EXECUTES
                        | CBF_SKIP_ALLNOTIFICATIONS, 0);

    /* if that failed, return the DDE error code */
    if (err != DMLERR_NO_ERROR)
        return err;

    /* create string handles for the service and topic names */
    info->hservice_ =
        DdeCreateStringHandle(info->ddeinst_, service, CP_WINANSI);
    info->htopic_ = DdeCreateStringHandle(info->ddeinst_, topic, CP_WINANSI);

    /* set up a conversation with the given service and topic */
    info->hconv_ = DdeConnect(info->ddeinst_, info->hservice_,
                              info->htopic_, 0);

    /* if the conversation startup succeeded, we're done */
    if (info->hconv_ != 0)
        return 0;

    /* 
     *   an error occurred - get the error code, uninitialize DDE, and
     *   return the error 
     */
    err = DdeGetLastError(info->ddeinst_);
    DdeUninitialize(info->ddeinst_);
    return err;
}

/*
 *   Close a DDE conversation.  Deallocates DDE resources.  Each
 *   successful call to dde_open_conv should be matched by a call to this
 *   routine.  
 */
void dde_close_conv(dde_info_t *info)
{
    /* release our item handle, if we allocated it */
    if (info->hitem_ != 0)
        DdeFreeStringHandle(info->ddeinst_, info->hitem_);

    /* release our service and topic handles */
    DdeFreeStringHandle(info->ddeinst_, info->hservice_);
    DdeFreeStringHandle(info->ddeinst_, info->htopic_);

    /* release DDEML resources */
    DdeUninitialize(info->ddeinst_);
}

/*
 *   Issue a REQUEST transaction to obtain data for a given item.  Returns
 *   the handle to the item data.  Returns null if the data could not be
 *   read.  'fmt' is the clipboard format in which the item is requested
 *   (for example, CF_TEXT for a text reply).  If 'err' is non-null, we'll
 *   fill it in with the DDE error code if an error occurs.
 *   
 *   On success, we'll fill in *pdata and *psiz with a pointer to the
 *   returned data block and the size of the data block.
 */
HDDEDATA dde_request(dde_info_t *info, const char *item, int fmt,
                     unsigned *err, BYTE **pdata, DWORD *psiz)
{
    HDDEDATA hdata;
    
    /* if we already have an item name handle, free it */
    if (info->hitem_ != 0)
        DdeFreeStringHandle(info->ddeinst_, info->hitem_);
    
    /* create a handle for the item name */
    info->hitem_ = DdeCreateStringHandle(info->ddeinst_, item, CP_WINANSI);

    /* get the data */
    hdata = DdeClientTransaction(0, 0, info->hconv_, info->hitem_, fmt,
                                 XTYP_REQUEST, 5000, 0);

    /* if that failed, free the item name handle and return failure */
    if (hdata == 0)
    {
        /* get the error code if the caller wants it */
        if (err != 0)
            *err = DdeGetLastError(info->ddeinst_);

        /* release the item name handle */
        DdeFreeStringHandle(info->ddeinst_, info->hitem_);
        info->hitem_ = 0;

        /* return failure */
        return 0;
    }

    /* get a pointer to the data */
    *pdata = DdeAccessData(hdata, psiz);

    /* return the data handle */
    return hdata;
}

/*
 *   Free the data handle received from a dde_request().  Each successful
 *   call to dde_request() should call this routine to free the data
 *   handle received. 
 */
void dde_request_free(dde_info_t *, HDDEDATA hdata)
{
    DdeFreeDataHandle(hdata);
}

/*
 *   Execute a DDE command, building the command with string
 *   substitutions.  The 'cmd' string can contain '%s' sequences to
 *   indicate that additional (const char *) arguments (provided in
 *   varargs form) should be substituted textually.  Use '%%' to indicate
 *   a single '%' in the string.  Returns 0 on success, a non-zero DDE
 *   error code on failure.  
 */
unsigned dde_exec(dde_info_t *info, const char *cmd, ...)
{
    char buf[512];
    const char *src;
    char *dst;
    va_list argptr;

    /* start reading the varargs */
    va_start(argptr, cmd);
    
    /* build the command string */
    for (src = cmd, dst = buf ;
         *src != '\0' && dst < buf + sizeof(buf) - 1 ; ++src)
    {
        /* check for substitutions */
        if (*src == '%')
        {
            /* check what we're substituting */
            ++src;
            switch(*src)
            {
            case 's':
                {
                    const char *arg;
                    
                    /* get the next argument */
                    arg = va_arg(argptr, const char *);

                    /* copy it into the destination */
                    while (*arg != 0 && dst < buf + sizeof(buf) - 1)
                        *dst++ = *arg++;
                }
                break;

            default:
                /* copy anything else directly */
                *dst++ = *src;
                break;
            }
        }
        else
        {
            /* simply copy this character directly */
            *dst++ = *src;
        }
    }

    /* null-terminate the formatted command string */
    *dst++ = '\0';

    /* done with varargs */
    va_end(argptr);

    /* execute the transaction, and return the result */
    if (DdeClientTransaction((BYTE *)buf, dst - buf, info->hconv_, 0, 0,
                             XTYP_EXECUTE, 5000, 0) == 0)
        return DdeGetLastError(info->ddeinst_);

    /* success */
    return 0;
}


