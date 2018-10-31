/* $Header: d:/cvsroot/tads/html/html_os.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  html_os.h - OS definitions for TADS HTML
Function
  
Notes
  Include this file to define portable interfaces to non-portable
  functionality.  This file (indirectly, via a separate #include file)
  defines types, macros, and functions that can be called from portable code
  but which are implemented in a system-specific manner.

  This file is the "switchboard" for OS-specific header inclusion.  To
  avoid cluttering the code (in this file, in the portable files, and in the
  system-specific files) with a confusing maze of ifdef's, this file doesn't
  do anything except include another file based on pre-defined macros that
  specify the target system.

  To add a new operating system, create a new file in the subdirectory
  for system-specific code for the new OS, named "xxx/hos_xxx.h".  (The
  "xxx" specifies the system name.  For 32-bit Windows, it's "w32"; for
  Macintosh, it might be "mac".).  Then add an ifdef to this file that
  includes your new file.  Place the required definitions in your new file.

  See win32/hos_w32.h for information on what each system must implement
  in its own xxx/hos_xxx.h file.

  Do NOT include your hos_xxx.h file directly from any other code.
  Simply include html_os.h from any code that needs the definitions in
  hos_xxx.h.  In addition, you should not put any interfaces in your
  hos_xxx.h file that are used exclusively by your system-specific code;
  hos_xxx.h is meant only for defining portable interfaces to non-portable
  functionality.  For any interfaces that you use only from your
  system-specific code, create a system-specific header file of your own.

  If you're porting this system to a new platform, you should send me
  your #include addition to this file for inclusion in the generic
  distribution.  This will make it easier for you to update to new versions
  in the future - the generic copy will already have your system-specific
  #include, so you won't need to make any edits to this file.
Modified
  10/24/97 MJRoberts  - Creation
*/

/* ------------------------------------------------------------------------ */
/* 
 *   Include definitions for 32-bit Windows
 */
#ifdef __WIN32__
#include "hos_w32.h"
#endif


