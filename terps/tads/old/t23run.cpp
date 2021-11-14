/*
  t23run.cpp - "command line" entrypoint for combined TADS 2/3 interpreter
*/

#include <stdlib.h>
#include <stdio.h>

#include "os.h"

/* tads 2 headers */
#include "trd.h"

/* tads 3 headers */
#include "t3std.h"
#include "vmmain.h"
#include "vmvsn.h"
#include "vmmaincn.h"
#include "vmhostsi.h"

/* ------------------------------------------------------------------------ */

extern "C"
{
#include "glk.h"
#include "glkstart.h"
}

int tads_argc;
char **tads_argv;

glkunix_argumentlist_t glkunix_arguments[] =
{
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

extern "C" int glkunix_startup_code(glkunix_startup_t *data)
{
	tads_argc = data->argc;
	tads_argv = data->argv;
	return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Invoke the tads 2 main entrypoint with the given command-line arguments 
 */
static int main_t2(int argc, char **argv)
{
    int stat;

    /* initialize the OS layer */
    os_init(&argc, argv, 0, 0, 0);

    /* install the break handler */
    os_instbrk(1);

    /* invoke the tads 2 main entrypoint */
    stat = os0main2(argc, argv, trdmain, "", 0, 0);

    /* done with the break handler */
    os_instbrk(0);

    /* uninitialize the OS layer */
    os_uninit();

    /* return the status */
    return stat;
}

/* ------------------------------------------------------------------------ */
/*
 *   Invoke the T3 VM with the given command-line arguments
 */
static int main_t3(int argc, char **argv)
{
    CVmMainClientConsole clientifc;
    int stat;
    CVmHostIfc *hostifc = new CVmHostIfcStdio(argv[0]);

    /* 
     *   Initialize the OS layer.  Since this is a command-line-only
     *   implementation, there's no need to ask the OS layer to try to get
     *   us a filename to run, so pass in null for the prompt and filename
     *   buffer.  
     */
    os_init(&argc, argv, 0, 0, 0);

    /* invoke the basic entrypoint */
    stat = vm_run_image_main(&clientifc, "t3run", argc, argv,
                             TRUE, FALSE, hostifc);

    /* uninitialize the OS layer */
    os_uninit();

    /* done with the host interface */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* exit with status code */
    os_term(stat);

    /* we shouldn't get here, but in case os_term doesn't really exit... */
    return stat;
}

/* ------------------------------------------------------------------------ */
/*
 *   Main entrypoint 
 */

void glk_main(void)
{
    int stat;
    static const char *defexts[] = { "gam", "t3" };
    char prog_arg[OSFNMAX];
    char fname[OSFNMAX];
    int engine_ver;

    int argc = tads_argc;
    char **argv = tads_argv;

    winid_t mainwin;
    char buf[256];

    /* 
     *   if one of our special usage message arguments was given, show the
     *   usage 
     */
    if (argc == 2 && stricmp(argv[1], "-help2") == 0)
    {
        /* invoke the tads 2 main entrypoint with no arguments */
        main_t2(1, argv);
        
        /* that's all we need to do with this option */
        os_term(OSEXSUCC);
    }
    else if (argc == 2 && stricmp(argv[1], "-help3") == 0)
    {
        /* invoke the tads 3 main entrypoint with no arguments */
        main_t3(1, argv);
        
        /* that's all we need to do with this option */
        os_term(OSEXSUCC);
    }

    /* look at the arguments and try to find the program name */
    if (!vm_get_game_arg(argc, argv, prog_arg, sizeof(prog_arg)))
    {
        /* 
         *   there's no game file name specified or implied - show the
         *   generic combined v2/v3 usage message 
         */
        mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
        glk_set_window(mainwin);

        /* copyright-date-string */
        glk_put_string(
                "TADS Interpreter - "
                "Copyright (c) 1993, 2004 Michael J. Roberts\n"
                "TADS 2 VM version " TADS_RUNTIME_VERSION " / "
                "T3 VM version " T3VM_VSN_STRING "\n\n");

        glk_put_string(
                "Error: you didn't specify a game file name on the command "
                "line, or the command "
                "options are incorrect. You must specify the name of "
                "the game file you would "
                "like to run.\n"
                "\n"
                "If you'd like a list of command-line options for TADS 2 "
                "games, specify -help2 "
                "instead of giving a game file name. Or, if you'd like a "
                "list of command-line "
                "options for TADS 3, specify -help3.\n");

        /* pause (if desired by OS layer) and exit */
        os_expause();
        os_term(OSEXFAIL);
    }

    /* determine the type of the game we have */
    engine_ver = vm_get_game_type(prog_arg, fname, sizeof(fname),
                                  defexts,
                                  sizeof(defexts)/sizeof(defexts[0]));

    /* presume failure */
    stat = OSEXFAIL;

    /* see what we have */
    switch(engine_ver)
    {
    case VM_GGT_TADS2:
        /* run the game using the TADS 2 engine */
        stat = main_t2(argc, argv);
        break;

    case VM_GGT_TADS3:
        /* run the game using the TADS 3 engine */
        stat = main_t3(argc, argv);
        break;

    case VM_GGT_INVALID:
        /* invalid file type */
        sprintf(buf,
            "The file you have selected (%s) is not a valid game file.\n",
            fname);
        mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
        glk_set_window(mainwin);
        glk_put_string(buf);
        break;

    case VM_GGT_NOT_FOUND:
        /* file not found */
        sprintf(buf, "The game file (%s) cannot be found.\n", prog_arg);
        mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
        glk_set_window(mainwin);
        glk_put_string(buf);
        break;

    case VM_GGT_AMBIG:
        /* ambiguous file */
        sprintf(buf,"The game file (%s) cannot be found exactly as given, "
               "but multiple game "
               "files with this name and different default suffixes "
               "(.gam, .t3) exist. "
               "Please specify the full name of the file, including the "
               "suffix, that you "
               "wish to use.\n",
               prog_arg);
        mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
        glk_set_window(mainwin);
        glk_put_string(buf);
        break;
    }

    /* pause (if desired by OS layer) and terminate with our status code */
    os_expause();
    os_term(stat);
}

