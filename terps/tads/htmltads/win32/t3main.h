/* $Header$ */

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3main.h - interface to T3 startup routine
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#ifndef T3MAIN_H
#define T3MAIN_H

/*
 *   Invoke the T3 main entrypoint 
 */
int t3main(int argc, char **argv,
           struct appctxdef *app_ctx, char *config_file);

/* create/delete a host interface */
class CVmHostIfc *t3main_create_hostifc(const char *exefile,
                                        struct appctxdef *appctx);
void t3main_delete_hostifc(class CVmHostIfc *hostifc);

#endif /* T3MAIN_H */

