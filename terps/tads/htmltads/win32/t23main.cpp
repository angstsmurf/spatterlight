#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2001 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t23main.cpp - combined TADS 2 and 3 interpreter startup
Function
  Senses whether to invoke the TADS 2 or 3 engine based on the game file
  being opened, and passes control to the appropriate engine's entrypoint.
Notes
  
Modified
  03/06/01 MJRoberts  - Creation
*/

#include <Windows.h>

#include "t23main.h"
#include "t3main.h"
#include "htmlw32.h"

/* tads 2 headers */
#include "trd.h"

/* tads 3 headers */
#include "vmmain.h"

/*
 *   Determine which VM to use, and invoke its entrypoint 
 */
int t23main(int argc, char **argv, struct appctxdef *appctx, char *save_ext)
{
    int stat;
    char game_arg[OSFNMAX];
    CHtmlSys_mainwin *mainwin;
    char fname[OSFNMAX + 1];
    char *game_file;
    int engine_type;
    int engine_valid;
    int using_pending;
    char actual_game_file[OSFNMAX];
    char *new_argv[2];
    static const char *defexts[] =
    {
        "gam",
        "t3"
    };
    
    /* presume failure */
    stat = OSEXFAIL;

    /* presume we won't use a pending game name */
    using_pending = FALSE;

    /* if there are no arugments, check for a pending game */
    if (argc <= 1)
    {
        /* ask the main window for a pending game */
        if ((mainwin = CHtmlSys_mainwin::get_main_win()) != 0
            && mainwin->get_pending_new_game() != 0
            && strlen(mainwin->get_pending_new_game()) + 1 <= sizeof(fname))
        {
            /* copy the pending image file name */
            strcpy(fname, mainwin->get_pending_new_game());
            
            /* use this as the filename for sensing the engine version */
            game_file = fname;
            
            /* 
             *   note that we are using a pending game (so that we can
             *   cancel the pending game if don't recognize its type) 
             */
            using_pending = TRUE;
        }
        else if (appctx != 0 && appctx->get_game_name != 0)
        {
            /* 
             *   No arguments and no pending game, but we have a host
             *   callback that can provide a game name on demand.  Ask for a
             *   name.  
             */
            if ((*appctx->get_game_name)(appctx->get_game_name_ctx,
                                         fname, sizeof(fname)))
            {
                /* 
                 *   we got the name - use this for sensing the engine
                 *   version 
                 */
                game_file = fname;

                /* set up a new argument vector with the game name */
                new_argv[0] = argv[0];
                new_argv[1] = fname;
                argv = new_argv;
                argc = 2;
            }
            else
            {
                /* 
                 *   no game selected - simply return success so that the
                 *   caller can go into standby mode 
                 */
                os_printz("\n(No game has been selected.)\n");
                return OSEXSUCC;
            }
        }
        else
        {
            /* 
             *   we have no game name, and no way of asking the host system
             *   for a game - we must just be starting in standby mode, so
             *   indicate success so that the caller go stand by for the
             *   user to select a game explicitly 
             */
            return OSEXSUCC;
        }
    }
    else
    {
        /* parse arguments to figure out which engine type we're using */
        if (!vm_get_game_arg(argc, argv, game_arg, sizeof(game_arg)))
        {
            /* show an error */
            MessageBox(0, "Invalid command line arguments.  No valid "
                       "game file was specified, or the syntax of one "
                       "or more command line options is incorrect.",
                       "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

            /* indicate failure */
            return OSEXFAIL;
        }

        /* use this as the game filename */
        game_file = game_arg;
    }
    
    /* check the game file to determine the game's type */
    engine_type = vm_get_game_type(game_file, actual_game_file,
                                   sizeof(actual_game_file),
                                   defexts,
                                   sizeof(defexts)/sizeof(defexts[0]));

    /* presume the engine type is not valid or not one we recognize */
    engine_valid = FALSE;

    /* invoke the appropriate engine */
    switch(engine_type)
    {
    case VM_GGT_TADS2:
        /* we have a valid engine type that we can deal with */
        engine_valid = TRUE;

        /* invoke tads 2, and use the resulting status code */
        stat = trdmain(argc, argv, appctx, save_ext);
        break;
        
    case VM_GGT_TADS3:
        /* we have a valid engine type that we can deal with */
        engine_valid = TRUE;

        /* invoke tads 3, and use the resulting status code */
        stat = t3main(argc, argv, appctx, save_ext);
        break;
        
    case VM_GGT_AMBIG:
        /* ambiguous game name - show an error */
        MessageBox(0, "More than one game file with the given name "
                   "exists.  Please specify the exact name of the file "
                   "you wish to use, including the suffix (.gam or .t3).",
                   "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        break;
        
    case VM_GGT_INVALID:
        /* the file is not valid for any known engine */
        MessageBox(0, "The game file specified is not a valid compiled "
                   "TADS game.",
                   "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        break;

    case VM_GGT_NOT_FOUND:
    default:
        /* 
         *   we couldn't figure out which engine type to invoke - show an
         *   error 
         */
        MessageBox(0, "The game file specified does not exist.",
                   "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        break;
    }

    /* 
     *   if we didn't get a valid engine type, and we are using a pending
     *   game name, cancel the pending game, since we've now effectively
     *   tried running the game and failed 
     */
    if (using_pending && !engine_valid)
        mainwin->clear_pending_new_game();

    /* return the status */
    return stat;
}
