#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3main.cpp - interface to T3 engine main entrypoint
Function
  The startup configuration code uses this routine as a bridge to the
  T3 engine's main entrypoint.  When the HTML user interface is ready
  to call the underlying engine to start running the program, it calls
  us; we provide the interface glue necessary to call the T3 engine.
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#include <Windows.h>

/* T3 includes */
#include "t3std.h"
#include "vmglob.h"
#include "vmmain.h"
#include "vmmaincn.h"
#include "vmhost.h"
#include "resload.h"
#include "vmconsol.h"


/* TADS 2 includes */
#include "appctx.h"

/* HTML TADS includes */
#include "t3main.h"
#include "w32main.h"
#include "htmlw32.h"
#include "htmlpref.h"
#include "tadsapp.h"


/*
 *   Client services interface.  Base our implementation on the standard
 *   console-based implementation, since we use a regular system console to
 *   implement the main HTML output stream.  
 */
class MyClientIfc: public CVmMainClientConsole
{
public:
    /* set "plain" mode - do nothing, since we only run in GUI mode */
    void set_plain_mode() { }

    /* 
     *   Display an error.  Show the error in a message box rather than
     *   writing it on the console, the way our base class normally would.  
     */
    void display_error(struct vm_globals *, const char *msg,
                       int /*add_blank_line*/)
    {
        /* display the message on the console */
        os_printz(msg);

        /* also display the message in a message box */
        MessageBox(0, msg, "TADS",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
};

/*
 *   Host application interface.  This provides a bridge between the T3 VM
 *   host interface (class CVmHostIfc) and the TADS 2 application context
 *   (struct appctxdef) mechanism.  
 */
class MyHostIfc: public CVmHostIfc
{
public:
    MyHostIfc(appctxdef *appctx, const char *exe_dir, const char *exe_file)
    {
        /* remember the executable file */
        exe_file_ = lib_copy_str(exe_file);

        /* remember the TADS 2 host application context */
        appctx_ = appctx;

        /* set up a resource loader in the executable directory */
        cmap_res_loader_ = new CResLoader(exe_dir);

        /* set the name of the executable file */
        cmap_res_loader_->set_exe_filename(exe_file);

        /* by default, allow read/write in current directory only */
        io_safety_ = VM_IO_SAFETY_READWRITE_CUR;
    }

    ~MyHostIfc()
    {
        /* delete the resource loader */
        delete cmap_res_loader_;

        /* delete the saved executable file name */
        lib_free_str(exe_file_);
    }

    /* get the I/O safety level */
    int get_io_safety()
    {
        if (appctx_ != 0 && appctx_->get_io_safety_level != 0)
        {
            /* ask the app context to handle it */
            return (*appctx_->get_io_safety_level)
                (appctx_->io_safety_level_ctx);
        }
        else
        {
            /* the app context doesn't care - use our own level memory */
            return io_safety_;
        }
    }

    /* set the I/O safety level */
    void set_io_safety(int level)
    {
        if (appctx_ != 0 && appctx_->set_io_safety_level != 0)
        {
            /* let the app context handle it */
            (*appctx_->set_io_safety_level)(appctx_->io_safety_level_ctx,
                                            level);
        }
        else
        {
            /* the app doesn't care - set our own level memory */
            io_safety_ = level;
        }
    }

    /* get the resource loader */
    CResLoader *get_cmap_res_loader() { return cmap_res_loader_; }

    /* set the game name */
    void set_image_name(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->set_game_name != 0)
            (*appctx_->set_game_name)(appctx_->set_game_name_ctx, fname);
    }

    /* set the resource root directory */
    void set_res_dir(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->set_res_dir != 0)
            (*appctx_->set_res_dir)(appctx_->set_res_dir_ctx, fname);
    }

    /* add a resource file */
    int add_resfile(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->add_resfile != 0)
            return (*appctx_->add_resfile)(appctx_->add_resfile_ctx, fname);
        else
            return 0;
    }

    /* we suport add_resfile() if the application context does */
    virtual int can_add_resfiles()
    {
        /* 
         *   if the add_resfile function is defined in the application
         *   context, we support adding resource files 
         */
        return (appctx_ != 0 && appctx_->add_resfile != 0);
    }

    /* add a resource */
    void add_resource(unsigned long ofs, unsigned long siz,
                      const char *res_name, size_t res_name_len, int fileno)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->add_resource != 0)
            (*appctx_->add_resource)(appctx_->add_resource_ctx, ofs, siz,
                                     res_name, res_name_len, fileno);
    }

    /* get the external resource path */
    const char *get_res_path()
    {
        /* get the path from the app context if possible */
        return (appctx_ != 0 ? appctx_->ext_res_path : 0);
    }

    /* determine if a resource exists */
    int resfile_exists(const char *res_name, size_t res_name_len)
    {
        /* 
         *   let the application context handle it if possible; if not,
         *   just return false, since we can't otherwise provide resource
         *   operations 
         */
        if (appctx_ != 0 && appctx_->resfile_exists != 0)
            return (*appctx_->resfile_exists)(appctx_->resfile_exists_ctx,
                                              res_name, res_name_len);
        else
            return FALSE;
    }

    /* find a resource */
    osfildef *find_resource(const char *res_name, size_t res_name_len,
                            unsigned long *res_size)
    {
        /* 
         *   let the application context handle it; if we don't have an
         *   application context, we don't provide resource operation, so
         *   simply return failure 
         */
        if (appctx_ != 0 && appctx_->find_resource != 0)
            return (*appctx_->find_resource)(appctx_->find_resource_ctx,
                                             res_name, res_name_len,
                                             res_size);
        else
            return 0;
    }

    /* get the image file name */
    vmhost_gin_t get_image_name(char *buf, size_t buflen)
    {
        /* 
         *   let the application context handle it if possible; otherwise,
         *   return false, since we can't otherwise ask for an image name 
         */
        if (appctx_ != 0 && appctx_->get_game_name != 0)
        {
            int ret;
            
            /* ask the host system to get a name */
            ret = (*appctx_->get_game_name)(appctx_->get_game_name_ctx,
                                            buf, buflen);

            /* 
             *   if that failed, the user must have chosen to cancel;
             *   otherwise, we were successful
             */
            return (ret ? VMHOST_GIN_SUCCESS : VMHOST_GIN_CANCEL);
        }
        else
        {
            /* we can't ask for a name */
            return VMHOST_GIN_IGNORED;
        }
    }

    /* get a special file system path */
    virtual void get_special_file_path(char *buf, size_t buflen, int id)
        { return os_get_special_path(buf, buflen, exe_file_, id); }

private:
    /* TADS 2 host application context */
    appctxdef *appctx_;

    /* character map resource loader */
    CResLoader *cmap_res_loader_;

    /* 
     *   our internal I/O safety level memory (in case the host app
     *   context doesn't support I/O safety level operations) 
     */
    int io_safety_;

    /* the main executable filename */
    char *exe_file_;
};

/* 
 *   create a host interface 
 */
class CVmHostIfc *t3main_create_hostifc(const char *exe_file,
                                        struct appctxdef *appctx)
{
    char exe_dir[OSFNMAX];

    /* get the directory containing the executable */
    os_get_path_name(exe_dir, sizeof(exe_dir), exe_file);

    /* create and return a new host interface */
    return new MyHostIfc(appctx, exe_dir, exe_file);
}

/*
 *   delete a host interface 
 */
void t3main_delete_hostifc(class CVmHostIfc *hostifc)
{
    /* delete the interface implementation */
    delete hostifc;
}

/*
 *   Invoke the main T3 VM entrypoint 
 */
int t3main(int argc, char **argv, struct appctxdef *appctx, char *)
{
    CHtmlSys_mainwin *mainwin;
    char *my_argv[2];
    char my_image[OSFNMAX + 1];
    MyClientIfc clientifc;
    CVmHostIfc *hostifc;
    char exe_file[OSFNMAX];
    int stat;

    /* get the name of the executable file */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      exe_file, sizeof(exe_file));

    /* set up the host context */
    hostifc = t3main_create_hostifc(exe_file, appctx);

    /* 
     *   if we didn't get a filename argument, see if the main window has
     *   a pending game, and supply that as the argument if so 
     */
    if (argc <= 1
        && (mainwin = CHtmlSys_mainwin::get_main_win()) != 0
        && mainwin->get_pending_new_game() != 0
        && strlen(mainwin->get_pending_new_game()) + 1 <= sizeof(my_image))
    {
        /* copy the pending image file name */
        strcpy(my_image, mainwin->get_pending_new_game());

        /* 
         *   the pending game name has now been consumed - tell the window
         *   to forget about it 
         */
        mainwin->clear_pending_new_game();

        /* synthesize our new argv */
        my_argv[0] = argv[0];
        my_argv[1] = my_image;

        /* use the synthesized argv instead of the caller's */
        argc = 2;
        argv = my_argv;
    }

    /* run the program and return the result code */
    stat = vm_run_image_main(&clientifc, w32_usage_app_name,
                             argc, argv, TRUE, FALSE, hostifc);

    /* delete the host interface */
    t3main_delete_hostifc(hostifc);

    /* return the status */
    return stat;
}

