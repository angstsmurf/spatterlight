/* $Header: d:/cvsroot/tads/html/win32/tadsdde.h,v 1.1 1999/07/11 00:46:47 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsdde.h - TADS DDE support routines
Function
  
Notes
  
Modified
  05/17/98 MJRoberts  - Creation
*/

#ifndef TADSDDE_H
#define TADSDDE_H


/* ------------------------------------------------------------------------ */
/*
 *   DDE support routines.  We provide a few routines to make access to
 *   DDE operations simpler.  
 */

/*
 *   Initialize a DDE conversation with a given service and topic.
 *   Returns zero on success, non-zero on failure.  If successful, the
 *   dde_info_t structure will be initialized for future dde operations.  
 */
unsigned dde_open_conv(struct dde_info_t *info,
                       const char *service, const char *topic);

/*
 *   Close a DDE conversation.  Deallocates DDE resources.  Each
 *   successful call to dde_open_conv should be matched by a call to this
 *   routine.  
 */
void dde_close_conv(struct dde_info_t *info);

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
HDDEDATA dde_request(struct dde_info_t *info, const char *item,
                     int fmt, unsigned *err, BYTE **pdata,
                     DWORD *psiz);

/*
 *   Free the data handle received from a dde_request().  Each successful
 *   call to dde_request() should call this routine to free the data
 *   handle received.  
 */
void dde_request_free(struct dde_info_t *, HDDEDATA hdata);

/*
 *   Execute a DDE command, building the command with string
 *   substitutions.  The 'cmd' string can contain '%s' sequences to
 *   indicate that additional (const char *) arguments (provided in
 *   varargs form) should be substituted textually.  Use '%%' to indicate
 *   a single '%' in the string.  Returns 0 on success, a non-zero DDE
 *   error code on failure.  
 */
unsigned dde_exec(struct dde_info_t *info, const char *cmd, ...);


/* ------------------------------------------------------------------------ */
/*
 *   DDE information structure - clients can use this to set up for DDE
 *   transactions 
 */
struct dde_info_t
{
    /* DDEML instance handle */
    DWORD ddeinst_;

    /* conversation handle */
    HCONV hconv_;

    /* service and topic string handles */
    HSZ hservice_;
    HSZ htopic_;

    /* item string handle */
    HSZ hitem_;
};


#endif /* TADSDDE_H */
