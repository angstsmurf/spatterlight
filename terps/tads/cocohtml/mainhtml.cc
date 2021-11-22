/*
 * mainhtml.cc -- entrypoint for combined HTML-TADS 2/3 interpreter
 */

#include "cocohtml.h"

/* tads 2 headers */
#include "trd.h"

/* tads 3 headers */
#include "t3std.h"
#include "vmmain.h"
#include "vmvsn.h"
#include "vmmaincn.h"
#include "vmhostsi.h"

static appctxdef appctx;

extern "C" int os_init(int *argc, char *argv[], const char *prompt, char *buf, int bufsiz)
{
    return 0;
}

extern "C" void os_uninit(void)
{
}


static int coco_init_html(char *fname)
{
    CocoHtmlSysFrame *frame;
    char dname[1024];
    char *p;

    strcpy(dname, fname);
    p = strrchr(dname, '/');
    if (p)
        *p = '0';

    CHtmlResType::add_basic_types();

    memset(&appctx, 0, sizeof appctx);
    appctx.usage_app_name = "tadsr";

    frame = new CocoHtmlSysFrame();

    frame->resfinder->init_appctx(&appctx);
}

/*
 *   Invoke the tads 2 main entrypoint with the given command-line arguments 
 */
static int main_t2(int argc, char **argv)
{
    int stat;

    /* initialize the OS layer */
    os_init(&argc, argv, 0, 0, 0);

    /* invoke the tads 2 main entrypoint */
    stat = os0main2(argc, argv, trdmain, "", 0, 0);

    /* uninitialize the OS layer */
    os_uninit();

    /* exit with status code */
    os_term(stat);

    /* we shouldn't get here, but in case os_term doesn't really exit... */
    return stat;
}

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

/*
 *   Main entrypoint 
 */

int main(int argc, char **argv)
{
    int stat;
    static const char *defexts[] = { "gam", "t3" };
    char prog_arg[OSFNMAX];
    char fname[OSFNMAX];
    int engine_ver;

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

        printf( "TADS Interpreter - "
                "Copyright (c) 1993, 2004 Michael J. Roberts\n"
                "TADS 2 VM version " TADS_RUNTIME_VERSION " / "
                "T3 VM version " T3VM_VSN_STRING "\n\n");

        printf("You have not selected a game to run.\n");

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
	coco_init_html(fname);
        stat = main_t2(argc, argv);
        break;

    case VM_GGT_TADS3:
        /* run the game using the TADS 3 engine */
	coco_init_html(fname);
        stat = main_t3(argc, argv);
	break;

    case VM_GGT_INVALID:
        /* invalid file type */
        printf("The file you have selected (%s) is not a valid game file.\n", fname);
        break;

    case VM_GGT_NOT_FOUND:
        /* file not found */
        printf("The game file (%s) cannot be found.\n", prog_arg);
        break;

    case VM_GGT_AMBIG:
        /* ambiguous file */
        printf("The game file (%s) cannot be found exactly as given, "
               "but multiple game "
               "files with this name and different default suffixes "
               "(.gam, .t3) exist. "
               "Please specify the full name of the file, including the "
               "suffix, that you "
               "wish to use.\n",
               prog_arg);
        break;
    }

    /* pause (if desired by OS layer) and terminate with our status code */
    os_expause();
    os_term(stat);
}

