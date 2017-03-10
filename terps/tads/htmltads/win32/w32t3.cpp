#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32t3.cpp - TADS 3 version-specific implementation
Function
  
Notes
  
Modified
  01/20/00 MJRoberts  - Creation
*/

#include <Windows.h>

#include "t3main.h"
#include "w32main.h"
#include "hos_w32.h"

/* include the TADS 2 VM version information */
#include "trd.h"

/* include the TADS 3 VM version information */
#include "vmvsn.h"

/* include some needed T3 headers */
#include "vmerr.h"
#include "vmimage.h"


/* ------------------------------------------------------------------------ */
/*
 *   list memory blocks in the T3 VM subsystem 
 */
#ifdef TADSHTML_DEBUG

void th_list_subsys_memory_blocks()
{
    t3_list_memory_blocks(&os_dbg_sys_msg);
}

#endif /* TADSHTML_DEBUG */

