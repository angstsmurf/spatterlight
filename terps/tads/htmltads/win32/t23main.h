/* $Header$ */

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3main.h - interface to startup routine for combined TADS 2 and interpreters
Function

Notes

Modified
  03/06/01 MJRoberts  - creation
*/

#ifndef T23MAIN_H
#define T23MAIN_H

/*
 *   Invoke the combined entrypoint, which senses which engine to run based
 *   on the file being opened 
 */
int t23main(int argc, char **argv,
           struct appctxdef *app_ctx, char *config_file);

#endif /* T23MAIN_H */

