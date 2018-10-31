/* $Header: d:/cvsroot/tads/html/htmldbg.h,v 1.4 1999/07/11 00:46:40 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldbg.h - HTML TADS debugger interface
Function
  This class implements a set of portable operations intended to simplify
  implementation of the HTML TADS debugger on each platform.  Most of
  the debugger user interface is port-specific code; this class contains
  portable helper routines that the user interface code will usually
  need.

  This class is meant to be instantiated once, so it's probably easiest
  for the the main application frame or the main debugger window to
  create and own an instance of this object.
Notes
  
Modified
  02/04/98 MJRoberts  - Creation
*/

#ifndef HTMLDBG_H
#define HTMLDBG_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

#include <os.h>


/* size of buffer we need for building a breakpoint location string */
const int HTMLDBG_MAXBPNAME = OSFNMAX + 30;

/* maximum length of a condition string */
const int HTMLDBG_MAXCOND = 256;

/* maximum length of a breakpoint description */
const int HTMLDBG_MAXBPDESC = 512;


/* ------------------------------------------------------------------------ */
/*
 *   System services interfaces for debugger helper functions.  Certain
 *   debugger helper functions require certain operations to be performed
 *   by the user interface.  When calling such functions, the debugger's
 *   system-specific code must provide implementations of some of these
 *   interfaces.  
 */

/* window type specifier */
enum HtmlDbg_wintype_t
{
    HTMLDBG_WINTYPE_SRCFILE,                          /* source file window */
    HTMLDBG_WINTYPE_STACK,                                  /* stack window */
    HTMLDBG_WINTYPE_HISTORY,                   /* call history trace window */
    HTMLDBG_WINTYPE_DEBUGLOG                            /* debug log window */
};

/* status code for breakpoint loading */
enum htmldbg_loadbp_stat
{
    /* success */
    HTMLDBG_LBP_OK,

    /* one or more breakpoints moved to a new location */
    HTMLDBG_LBP_MOVED,

    /* 
     *   one or more breakpoints could not be set (some may have moved as
     *   well, but failure to set is the more serious condition) 
     */
    HTMLDBG_LBP_NOT_SET
};

/*
 *   Window creation interface.  The debugger system-specific code must
 *   provide an implementation of this window creation interface when
 *   calling helper routines that need to open new debugger windows.  
 */
class CHtmlDebugSysIfc_win
{
public:
    /*
     *   Create a new system window using the given parser and formatter
     *   instances.  'win_type' specifies the type of the window to
     *   create; if the system code wants to use different subclasses for
     *   the different types, it can use this type specifier to determine
     *   the type to create.  
     */
    virtual class CHtmlSysWin
        *dbgsys_create_win(class CHtmlParser *parser,
                           class CHtmlFormatter *formatter,
                           const textchar_t *win_title,
                           const textchar_t *full_path,
                           HtmlDbg_wintype_t win_type) = 0;

    /*
     *   Adjust a font descriptor to select the appropriate font for a
     *   source window.  The font descriptor will be set up with for a
     *   default fixed-pitch font; if the system code wants to make any
     *   further adjustments, such as setting a new font size, it can do
     *   so here.  
     */
    virtual void dbgsys_set_srcfont(class CHtmlFontDesc *font_desc) = 0;

    /*
     *   Compare two filenames to see if they actually refer to the same
     *   file.  This should consider system-specific factors such as case
     *   sensitivity, and should also consider relative paths.  If the two
     *   filenames refer to the same file after resolving relative paths
     *   and other factors, this should return true, otherwise false. 
     */
    virtual int dbgsys_fnames_eq(const char *fname1, const char *fname2) = 0;

    /*
     *   Compare a filename to a path to see if the path is in fact the
     *   fully-qualified form of the filename.  This should return true if
     *   the filename, along with any relative path in the filename,
     *   matches the path.
     *   
     *   For example, for a Unix-style file system, the following values
     *   for path and filename should yield the given results:
     *   
     *.  "/home/mjr/games/mygame.t", "mygame.t" -> true
     *.  "/home/mjr/games/mygame.t", "games/mygame.t" -> true
     *.  "/home/mjr/games/mygame.t", "mjr/games/mygame.t" -> true
     *.  "/home/mjr/games/mygame.t", "yourgame.t" -> false
     *.  "/home/mjr/games/mygame.t", "test/mygame.t" -> false 
     */
    virtual int dbgsys_fname_eq_path(const char *path,
                                     const char *fname) = 0;

    /* 
     *   Find a source file.  This has the same interface and same
     *   operation as the TADS 2 dbgu_find_src() function.  
     */
    virtual int dbgsys_find_src(const char *origname, int origlen,
                                char *fullname, size_t full_len,
                                int must_find_file) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger entrypoint functions.  The UI is responsible for
 *   implementing these functions.  The debugger engine calls these
 *   functions (indirectly via our engine-specific interfaces).
 *   
 *   These functions pass in a debugger context of type (struct dbgcxdef
 *   *).  This is an opaque pointer; the UI should never use this context
 *   for anything except passing back to CHtmlDebugHelper functions.  This
 *   is very important -- by avoiding any interpretation of the dbgcxdef
 *   structure, the UI is independent of the underlying engine, so the UI
 *   will be reusable without any changes with alternative engines that
 *   implement a CHtmlDebugHelper interface.  In particular, the UI will
 *   work with both TADS 2 and T3 VM's without any changes.
 *   
 *   These functions also pass in a (void *) "UI object"; this is a
 *   context pointer entirely for the use of the UI code.  For example, on
 *   Windows, the UI object is the main window instance, since the main
 *   window class encapsulates the top-level functionality of the debugger
 *   UI on Windows.  This is the context that the html_dbgui_ini()
 *   function returns.  
 */

/* 
 *   Debugger user interface initialization, phase one.  TADS calls this
 *   routine during startup, before reading the .GAM file, to let the user
 *   interface perform any initialization it requires before the .GAM file
 *   is loaded.
 *   
 *   This function returns the "UI object," creating a new UI object if
 *   necessary.  TADS retains the UI object and passes it to most of the
 *   other html_dbgui_xxx functions.  
 */
void *html_dbgui_ini(struct dbgcxdef *ctx, const char *game_filename);

/*
 *   Debugger user interface initialization, phase two.  TADS calls this
 *   routine during startup, after read the .GAM file.  The debugger user
 *   interface code can perform any required initialization that depends
 *   on the .GAM file having been read.  
 */
void html_dbgui_ini2(struct dbgcxdef *ctx, void *ui_object);

/*
 *   Determine if the debugger can resume from a run-time error.  This
 *   reflects the capabilities of the user interface of the debugger.  In
 *   particular, if the UI provides a way to change the instruction
 *   pointer, then the debugger can resume from an error, since the user
 *   can always move past the run-time error and continue execution.  If
 *   the UI doesn't let the user change the instruction pointer, resuming
 *   from an error won't work, since the program will keep hitting the
 *   same error and re-entering the debugger.  If this returns false, the
 *   run-time will trap to the debugger on an error, but will simply abort
 *   the current command when the debugger returns.  If this returns true,
 *   the run-time will trap to the debugger on an error with the
 *   instruction pointer set back to the start of the line containing the
 *   error, and will thus re-try the same line of code when the debugger
 *   returns, unless the debugger explicitly moves the instruction pointer
 *   before returning.  
 */
int html_dbgui_err_resume(struct dbgcxdef *ctx, void *ui_object);

/*
 *   Find a source file.  origname is the name of the source file as it
 *   appears in the game's debugging information; this routine should
 *   figure out where the file actually is, and put the fully-qualified
 *   path to the file in fullname.  The debugger calls this after it
 *   exhausts all of its other methods of finding a source file (such as
 *   searching the include path).
 *   
 *   Return true if the source file should be considered valid, false if
 *   not.  Most implementations will simply return true if the file was
 *   found, false if not; however, this approach will cause the debugger
 *   to terminate with an error at start-up if the user hasn't set up the
 *   debugger's include path correctly before running the debugger.  Some
 *   implementations, in particular GUI implementations, may wish to wait
 *   to find a file until the file is actually needed, rather than pester
 *   the user with file search dialogs repeatedly at start-up.
 *   
 *   must_find_file specifies how to respond if we can't find the file.
 *   If must_find_file is true, we should always return false if we can't
 *   find the file.  If must_find_file is false, however, we can
 *   optionally return true even if we can't find the file.  Doing so
 *   indicates that the debugger UI will defer locating the file until it
 *   is actually needed.
 *   
 *   If this routine returns true without actually finding the file, it
 *   should set fullname[0] to '\0' to indicate that fullname doesn't
 *   contain a valid filename.  
 */
int html_dbgui_find_src(const char *origname, int origlen,
                        char *fullname, size_t full_len, int must_find_file);

/* 
 *   Debugger user interface main command loop.  If err is non-zero, the
 *   debugger was entered because a run-time error occurred; otherwise, if
 *   bphit is non-zero, it's the number of the breakpoint that was
 *   encountered; otherwise, the debugger was entered through a
 *   single-step of some kind.  exec_ofs is the byte offset within the
 *   target object of the next instruction to be executed.  This can be
 *   changed upon return, in which case execution will continue from the
 *   new offset, but the offset must be within the same method of the same
 *   object (or within the same function) as it was upon entry.  
 */
void html_dbgui_cmd(struct dbgcxdef *ctx, void *ui_object,
                    int bphit, int err, unsigned int *exec_ofs);

/*
 *   Debugger UI - quitting game.  The runtime calls this routine just
 *   before the play loop is about to terminate after the game code has
 *   called the "quit" built-in function.  If the debugger wants, it can
 *   take control here (just as with dbgucmd()) for as long as it wants.
 *   If the debugger wants to restart the game, it should call bifrst().
 *   If this routine returns without signalling a RUN_RESTART error, TADS
 *   will terminate.  If a RUN_RESTART error is signalled, TADS will
 *   resume the play loop.  
 */
void html_dbgui_quitting(struct dbgcxdef *ctx, void *ui_object);

/* 
 *   debugger user interface termination - this routine is called when the
 *   debugger is about to terminate, so that the user interface can close
 *   itself down (close windows, release memory, etc) 
 */
void html_dbgui_term(struct dbgcxdef *ctx, void *ui_object);

/*
 *   Debugger user interface: display an error.  This is called mainly so
 *   that the debugger can display an error using special output
 *   formatting if the error occurs while debugging. 
 */
void html_dbgui_err(struct dbgcxdef *ctx, void *ui_object,
                    int errno, char *msg);


/* ------------------------------------------------------------------------ */
/*
 *   Debugger helper class
 */

class CHtmlDebugHelper
{
    /*
     *   Allow functions in the VM-specific extension class to access our
     *   private members, since the VM extension class is effectively a
     *   part of this class (it's a separate class only to allow different
     *   implementations to be plugged in) 
     */
    friend class CHtmlDebugHelperVM;
    
public:
    /* 
     *   Create the helper object.  See get_need_srcfiles() below for
     *   information on the meaning of 'need_srcfiles'.  
     */
    CHtmlDebugHelper(int need_srcfiles);

    /* delete */
    ~CHtmlDebugHelper();

    /* 
     *   Add/remove a reference.  Upon removing the last reference, we'll
     *   delete the object 
     */
    void add_ref() { ++refcnt_; }
    void remove_ref()
    {
        if (--refcnt_ == 0)
            delete this;
    }

    /* 
     *   Initialize after loading.
     *   
     *   If keep_bp_config is true, we'll save the current breakpoint
     *   configuration into the config object, then reload the
     *   configuration into the new TADS context; otherwise, we'll simply
     *   discard the current breakpoints and load the configuration from
     *   the config object.  Normally, if we're reloading the same game as
     *   was most recently loaded, we'll want to keep the current
     *   breakpoint configuration, since the user could have edited the
     *   configuration while the game was unloaded and would presumably
     *   want to keep the changes.  When loading a new game, we'll
     *   normally want to discard the old configuration, since it's not
     *   relevant to the new game.  
     */
    htmldbg_loadbp_stat init_after_load(struct dbgcxdef *ctx,
                                        class CHtmlDebugSysIfc_win *syswinifc,
                                        class CHtmlDebugConfig *config,
                                        int keep_bp_config);

    /* delete line links */
    void delete_line_status(int redraw);

    /* delete internal breakpoint list */
    void delete_internal_bps();

    /* delete internal source file list */
    void delete_internal_sources();

    /* clear the line source list from a configuration object */
    void clear_internal_source_config(class CHtmlDebugConfig *config);

    /* save our internal source list */
    void save_internal_source_config(class CHtmlDebugConfig *config);

    /* add a line source to our internal list */
    class CHtmlDbg_src
        *add_internal_line_source(const textchar_t *fname,
                                  const textchar_t *path,
                                  int line_source_id);

    /* 
     *   add a fake internal line source - this doesn't correspond to a
     *   file in the compiled game, but merely some arbitrary text file 
     */
    class CHtmlDbg_src
        *add_fake_internal_line_source(const textchar_t *fname);

    /*
     *   Update the debugger for a change in the execution status.  This
     *   should be called whenever we re-enter the debugger after
     *   executing game code.  We'll show the current line, reset the
     *   evaluation context, and update the stack window.  Returns the
     *   activated window.  
     */
    class CHtmlSysWin
        *update_for_entry(struct dbgcxdef *ctx,
                          class CHtmlDebugSysIfc_win *syswinifc);

    /*
     *   Set the debugging execution state.  These calls adjust the TADS
     *   interpreter state to the selected debugging state.  Since these
     *   state changes are usually made from within the user interface
     *   while the debugger has control, the debugger UI should normally
     *   exit its event loop (which will return control to the interpreter
     *   and thus to the game being executed) after making one of these
     *   calls. 
     */

    /* set execution state to GO */
    void set_exec_state_go(struct dbgcxdef *ctx);

    /* set execution state to STEP OVER - steps without following calls */
    void set_exec_state_step_over(struct dbgcxdef *ctx);

    /* set execution state to STEP INTO - steps and follows calls */
    void set_exec_state_step_into(struct dbgcxdef *ctx);

    /* 
     *   set execution state to STEP OUT - steps until current function
     *   has returned 
     */
    void set_exec_state_step_out(struct dbgcxdef *ctx);

    /*
     *   Move execution to the current selection in the given window.
     *   Returns zero on success, non-zero on error.  If this is
     *   successful, the debugger should immediately execute a single
     *   step, which will simply break back into the debugger immediately
     *   at this line; the single-step execution will ensure that we set
     *   up the debugger frame correctly at the new statement.
     *   
     *   On return, if successful, *need_single_step will be set to true
     *   if a single-step execution is needed as a follow-up to this
     *   operation.  Some engines require a single-step to establish the
     *   execution point in the new line of code, while others do not.  If
     *   *need_single_step is set to true on a successful return, the
     *   caller should act as though the user interactively entered a
     *   single-step command.  
     */
    int set_next_statement(struct dbgcxdef *ctx, unsigned int *exec_ofs,
                           class CHtmlSysWin *win, int *need_single_step);

    /*
     *   Check if the current selection in the given window is within the
     *   same function or method as the current execution point.  Returns
     *   true if so, false if not.  This is a condition of setting the
     *   next statement; this function is provided so that the UI code can
     *   check to make sure that this condition is met when the user
     *   attempts to move the execution point, and generate an appropriate
     *   error message if not.  
     */
    int is_in_same_fn(struct dbgcxdef *ctx, class CHtmlSysWin *win)
        { return vm_is_in_same_fn(ctx, win); }

    /*
     *   Signal a QUIT condition.  Because this uses the TADS error
     *   signalling mechanism, the caller should take care only to call
     *   this from the point at which its own event loop would normally be
     *   about to return, because this will invoke longjmp() to jump out
     *   of the caller's stack context. 
     */
    static void signal_quit(struct dbgcxdef *ctx);

    /*
     *   Signal a RESTART condition, restarting the game from the
     *   beginning.  The same restrictions that apply to signal_quit()
     *   apply here.  
     */
    static void signal_restart(struct dbgcxdef *ctx);

    /*
     *   Signal an ABORT condition.  This will abort the current command,
     *   returning control to the game's command line for a new command.
     *   The same restrictions that apply to signal_quit() apply here.  
     */
    static void signal_abort(struct dbgcxdef *ctx);

    /*
     *   Get/set the call tracing status 
     */
    int is_call_trace_active(struct dbgcxdef *ctx) const;
    void set_call_trace_active(struct dbgcxdef *ctx, int flag);

    /*
     *   Clear the call trace history log 
     */
    void clear_call_trace_log(struct dbgcxdef *ctx);

    /*
     *   Get a pointer to the history buffer and the number of bytes of
     *   data currently in the log 
     */
    const textchar_t *get_call_trace_buf(struct dbgcxdef *ctx) const;
    unsigned long get_call_trace_len(struct dbgcxdef *ctx) const;

    /*
     *   Turn profiling on and off. 
     */
    void start_profiling(struct dbgcxdef *ctx);
    void end_profiling(struct dbgcxdef *ctx);

    /*
     *   Retrieve profiling information from the last profiling session.
     *   This invokes the callback for each entrypoint, passing the name of
     *   the entrypoint in a human-readable format, the time spent directly
     *   in the function or method, the total time spent in children of the
     *   function or method (i.e., other functions or methods called from
     *   this one), and the number of times the entrypoint was called.  The
     *   times are given in microseconds.  
     */
    void get_profiling_data(struct dbgcxdef *ctx,
                            void (*cb)(void *ctx,
                                       const char *func_name,
                                       unsigned long time_direct,
                                       unsigned long time_in_children,
                                       unsigned long call_cnt),
                            void *cb_ctx);

    /*
     *   Set or clear a breakpoint at the current selection in the given
     *   window 
     */
    void toggle_breakpoint(struct dbgcxdef *ctx, class CHtmlSysWin *win);

    /*
     *   Set a temporary breakpoint at the current selection in the given
     *   window.  If there's already a breakpoint at this location, we
     *   won't add a new breakpoint, and we'll return false.  Otherwise,
     *   we'll set a new breakpoint and return true.
     *   
     *   The purpose of a temporary breakpoint is to allow "run to
     *   cursor"-type operations, where the debugger must secretly set its
     *   own internal breakpoint to facilitate the operation but where the
     *   user doesn't intend to add a new breakpoint to the persistent
     *   breakpoint list.  Because the breakpoint is temporary, we won't
     *   bother giving it a meaningful name for the display list, and we
     *   also won't show the breakpoint in the line status displayed in
     *   the source window; this breakpoint should thus be removed the
     *   next time the debugger is entered.
     *   
     *   Only one temporary breakpoint is allowed at a time.  If there's
     *   already a temporary breakpoint set, we'll remove the old one
     *   before adding the new one.  
     */
    int set_temp_bp(struct dbgcxdef *ctx, class CHtmlSysWin *win);

    /*
     *   Clear the temporary breakpoint.  Has no effect if there isn't a
     *   temporary breakpoint.  This should be called each time the
     *   debugger is entered. 
     */
    void clear_temp_bp(struct dbgcxdef *ctx);

    /*
     *   Add a new global breakpoint.  Returns a non-zero error code if
     *   the condition is invalid or any other error occurs; returns zero
     *   on success, and fills in *bpnum with the identifier of the new
     *   breakpoint.  If an error occurs, fills in errbuf with a message
     *   desribing the error.  
     */
    int set_global_breakpoint(struct dbgcxdef *ctx,
                              const textchar_t *cond, int change,
                              int *bpnum, char *errbuf, size_t errbuflen);

    /*
     *   Enable or disable the breakpoint at the current selection in the
     *   given window.  Does nothing if there's no breakpoint at the given
     *   location. 
     */
    void toggle_bp_disable(struct dbgcxdef *ctx, class CHtmlSysWin *win);

    /*
     *   Delete a breakpoint given the breakpoint number.  The breakpoint
     *   number is obtained from the TADS debugger core; for example, this
     *   is the identifier passed to the enumerator callback function.
     *   Returns zero on success, non-zero if an error occurs.  
     */
    int delete_breakpoint(struct dbgcxdef *ctx, int bpnum);

    /*
     *   Delete all breakpoints 
     */
    void delete_all_bps(struct dbgcxdef *ctx);

    /*
     *   Enable or disable a breakpoint given the breakpoint number. 
     */
    int enable_breakpoint(struct dbgcxdef *ctx, int bpnum, int enable);

    /*
     *   Determine if the given breakpoint number is associated with code.
     *   Returns true if so, false if not.  (Global breakpoints are not
     *   associated with code.) 
     */
    int is_bp_at_line(int bpnum);

    /*
     *   Determine if the given breakpoint is a global breakpoint.  Any
     *   breakpoint that doesn't have associated source code is a global
     *   breakpoint. 
     */
    int is_bp_global(int bpnum)
        { return !is_bp_at_line(bpnum); }

    /*
     *   Show the code associated with a given breakpoint.  Returns the
     *   window containing the given code, or null if the breakpoint
     *   doesn't have code associated with it.  
     */
    class CHtmlSysWin *show_bp_code(int bpnum,
                                    class CHtmlDebugSysIfc_win *syswinifc);

    /* show a source line, given a line number */
    void show_source_line(struct CHtmlDbg_win_link *win_link,
                          unsigned long linenum);

    /*
     *   Enumerate breakpoints in a displayable format
     */
    void enum_breakpoints(struct dbgcxdef *ctx,
                          void (*cbfunc)(void *cbctx, int bpnum,
                                         const textchar_t *desc,
                                         int disabled),
                          void *cbctx);

    /*
     *   Get a breakpoint's description in a format suitable for display.
     *   Returns zero on success, non-zero on failure. 
     */
    int get_bp_desc(int bpnum, textchar_t *buf, size_t buflen);

    /*
     *   Determine if a breakpoint is enabled or disabled.  Returns true
     *   if the breakpoint is enabled, false if it's disabled.  
     */
    int is_bp_enabled(int bpnum);

    /*
     *   Get the condition associated with a breakpoint.  Returns zero on
     *   success, non-zero on failure. 
     */
    int get_bp_cond(int bpnum, textchar_t *condbuf, size_t condbuflen,
                    int *stop_on_change);

    /*
     *   Change the condition associated with a breakpoint.  Returns zero
     *   on success.  If an error occurs, fills in errbuf with a message
     *   describing the error, and returns a non-zero error code.
     */
    int change_bp_cond(dbgcxdef *ctx, int bpnum,
                       const textchar_t *cond, int change,
                       char *errbuf, size_t errbuflen);

    /* 
     *   clear all breakpoints from a configuration; this doesn't affect
     *   any active breakpoints, but merely clears out the saved
     *   configuration data 
     */
    void clear_bp_config(class CHtmlDebugConfig *config) const;

    /*
     *   Save the breakpoint configuration to a configuration object, and
     *   reload a saved configuration.
     */
    void save_bp_config(class CHtmlDebugConfig *config) const;
    htmldbg_loadbp_stat load_bp_config(struct dbgcxdef *ctx,
                                       CHtmlDebugSysIfc_win *syswinifc,
                                       class CHtmlDebugConfig *config);

    /*
     *   Get the text of an error message.  This routine doesn't require
     *   an error stack, so it's suitable for retrieving the text of error
     *   messages returned from the debugger API.  
     */
    void get_error_text(struct dbgcxdef *ctx, int err,
                        textchar_t *buf, size_t buflen);

    /*
     *   Get the text of an error message, formatting the message with
     *   arguments from the error stack.  This should only be used when a
     *   valid error stack is present, such as when entering the debugger
     *   due to a run-time error.  
     */
    void format_error(struct dbgcxdef *ctx, int err, textchar_t *buf,
                      size_t buflen);

    /*
     *   Evaluate an expression in the current stack context.  Fills in
     *   the buffer with the expression result.  If an error occurs, we'll
     *   write an error message to the buffer instead of the expression
     *   value, and we'll also return the error code.  If is_lval is not
     *   null, we'll determine whether the expression can be assigned
     *   into, and set *is_lval to true if so, false if not.  If
     *   is_openable is not null, we'll determine whether the expression
     *   refers to an aggregate value (such as an object or a list), and
     *   set *is_openable to true if so, false if not.
     *   
     *   If aggcb is not null, it refers to an aggregation callback that
     *   we'll invoke for each subitem in the value, if the value refers
     *   to an aggregate object.  The 'relationship' parameter to the
     *   callback gives an operator that can be put between the parent
     *   expression (i.e., the one we're evaluating here) and the subitem
     *   string to yield an expression for the subitem itself.
     *   
     *   If 'speculative' is true, we'll perform speculative evaluation:
     *   we won't allow any assignments, function calls, or method calls.
     *   The idea is to prohibit game state changes, thereby allowing
     *   evaluation of an expression that the user *might* be interested
     *   in before the user actually asks us to evaluate it.  Since the
     *   user hasn't expressly requested evaluation, we don't want to
     *   change anything, thus the prohibitions.  
     */
    int eval_expr(struct dbgcxdef *ctx, textchar_t *buf, size_t buflen,
                  const textchar_t *expr, int *is_lval, int *is_openable,
                  void (*aggcb)(void *aggctx, const char *subitemname,
                                int subitemnamelen, const char *relationship),
                  void *aggctx, int speculative);

    /*
     *   Execute an assignment expression.  The result of evaluating the
     *   rvalue should be assigned to the lvalue.  The lvalue and rvalue
     *   are given as expression source code text.  The evaluation is
     *   purely for the assignment side effect, so the result is not
     *   returned.  
     */
    int eval_asi_expr(struct dbgcxdef *ctx, const textchar_t *lvalue,
                      const textchar_t *rvalue);

    /*
     *   Enumerate local variables in the current stack context.
     */
    void enum_locals(struct dbgcxdef *ctx,
                     void (*cbfunc)(void *ctx, const textchar_t *lclnam,
                                    size_t lclnamlen), void *cbctx);

    /*
     *   Receive notification that a source window is being closed.  This
     *   must be called by the system-specific code; we'll delete the
     *   structures that we allocated to track the window, including its
     *   formatter and parser instances.  Note that the window object
     *   itself must be deleted by the system-specific code. 
     */
    void on_close_srcwin(class CHtmlSysWin *win);

    /*
     *   Static callback version of on_close_srcwin - this can be used to
     *   set up a callback for objects that aren't aware of the helper
     *   class. 
     */
    static void on_close_srcwin_cb(void *ctx, class CHtmlSysWin *win)
    {
        ((CHtmlDebugHelper *)ctx)->on_close_srcwin(win);
    }

    /* get the line link record for a given source line */
    struct CHtmlDbg_line_link *find_line_link(int line_source_id,
                                              unsigned long linenum) const;

    /* 
     *   Get the line link record and window link record for the current
     *   selection in a window.  Returns the line number of the start of
     *   the line containing the start of the selection range in the
     *   window.  
     */
    unsigned long find_line_info(class CHtmlSysWin *win,
                                 struct CHtmlDbg_win_link **win_link,
                                 struct CHtmlDbg_line_link **line_link) const;

    /*
     *   Add/remove a status flag to/from a source line.  This will update
     *   the window containing the line to show the new status visually,
     *   if the line is showing in a window.  
     */
    void set_line_status(int line_source_id, unsigned long linenum,
                         unsigned int statflag)
    {
        change_line_status(line_source_id, linenum, statflag, TRUE);
    }
    
    void clear_line_status(int line_source_id, unsigned long linenum,
                           unsigned int statflag)
    {
        change_line_status(line_source_id, linenum, statflag, FALSE);
    }

    /* set a new current line */
    void set_current_line(int source_id, unsigned long linenum);

    /* set a new context line */
    void set_current_ctx_line(int source_id, unsigned long linenum);

    /*
     *   Forget the current line, updating the display to remove the
     *   current line marker. 
     */
    void forget_current_line();

    /* forget the context line */
    void forget_ctx_line();

    /* 
     *   Create a window to use as the stack window.  The helper class
     *   will keep track of the window, and will update it whenever
     *   update_for_entry() is called. 
     */
    class CHtmlSysWin
        *create_stack_win(class CHtmlDebugSysIfc_win *syswinifc);

    /*
     *   Create a window to use as the debug log window. 
     */
    class CHtmlSysWin
        *create_debuglog_win(class CHtmlDebugSysIfc_win *syswinifc);

    /*
     *   Create a window to use as the history trace window.  The helper
     *   class will keep track of the window, and will update it whenever
     *   update_for_entry() is called. 
     */
    class CHtmlSysWin
        *create_hist_win(class CHtmlDebugSysIfc_win *syswinifc);

    /* get the stack window */
    class CHtmlSysWin *get_stack_win() const;

    /* get the debug log window */
    class CHtmlSysWin *get_debuglog_win() const;

    /* clear the debug log window */
    void clear_debuglog_win();

    /* get the history window */
    class CHtmlSysWin *get_hist_win() const;

    /* 
     *   Update the stack window.  If reset_level is true, we'll reset the
     *   context level to the current line; otherwise, we'll refresh the
     *   window and leave the current context line intact.  
     */
    void update_stack_window(struct dbgcxdef *ctx, int reset_level);

    /* update the history window */
    void update_hist_window(struct dbgcxdef *ctx);

    /*
     *   Activate a stack window item.  The stack window should call this
     *   when the user selects a display item in the stack window (in the
     *   manner appropriate to the UI; for example, by double-clicking on
     *   the item).  Returns the source window to be activated.  
     */
    class CHtmlSysWin
        *activate_stack_win_item(class CHtmlDisp *disp, struct dbgcxdef *ctx,
                                 class CHtmlDebugSysIfc_win *syswinifc);

    /*
     *   Activate a stack window item, given the line number.  
     */
    class CHtmlSysWin
        *activate_stack_win_line(int linenum, struct dbgcxdef *ctx,
                                 class CHtmlDebugSysIfc_win *syswinifc);

    /*
     *   Toggle hidden display, and get the current hidden display status
     *   (true -> hidden output is being displayed).
     */
    void toggle_dbg_hid() { vm_toggle_dbg_hid(); }
    int get_dbg_hid() { return vm_get_dbg_hid(); }

    /*
     *   Enumerate open source windows.  Invokes the callback for each
     *   open window displaying either a line source or a text file.  The
     *   'idx' value can be used to identify the window in a subsequent
     *   call to get_source_window_by_index().  
     */
    void enum_source_windows(void (*func)(void *ctx, int idx,
                                          class CHtmlSysWin *win,
                                          int line_source_id,
                                          HtmlDbg_wintype_t win_type),
                             void *cbctx);

    /*
     *   Retrieve a particular source window, given its index.  The index
     *   reflects the same ordering as enum_source_windows() uses; the
     *   first window that enum_source_windows() returns is window 0, the
     *   second is 1, and so on.  
     */
    class CHtmlSysWin *get_source_window_by_index(int idx) const;

    /*
     *   Set the tab expansion size - this sets the number of spaces
     *   between tab stops.  We'll use this setting when loading source
     *   files when we encounter tab (\t, ascii 9) characters.  If there
     *   are open source windows, we'll reload them, since we translate
     *   tabs at load time.  
     */
    void set_tab_size(class CHtmlDebugSysIfc_win *win, int n);
    int get_tab_size() const { return tab_size_; }

    /*
     *   Reformat all source windows.  This should be called when there's
     *   a visible change in the display preferences, such as the text
     *   color or font.  
     */
    void reformat_source_windows(struct dbgcxdef *dbgctx,
                                 class CHtmlDebugSysIfc_win *syswinifc);

    /* 
     *   find the window containing a text file; returns null if there is
     *   no window currently open for this file
     */
    class CHtmlSysWin *find_text_file(class CHtmlDebugSysIfc_win *syswinifc,
                                      const textchar_t *fname,
                                      const textchar_t *path);

    /*
     *   Open a line source, given the line source ID.  Returns the window
     *   containing the line source.  
     */
    class CHtmlSysWin
        *open_line_source(class CHtmlDebugSysIfc_win *syswinifc,
                          int line_source_id);

    /*
     *   Open a text file.  We'll try to find an existing window with
     *   showing the same file, and show it if we can find it.  
     */
    class CHtmlSysWin
        *open_text_file(class CHtmlDebugSysIfc_win *syswinifc,
                        const textchar_t *fname)
    {
        return open_text_file(syswinifc, fname, fname);
    }

    /*
     *   Open a text file, using a possibly different string for the
     *   display filename than for the full filename path.  
     */
    class CHtmlSysWin
        *open_text_file(class CHtmlDebugSysIfc_win *syswinifc,
                        const textchar_t *display_fname,
                        const textchar_t *path)
    {
        /* open the text file at the first line */
        return open_text_file_at_line(syswinifc, display_fname, path, 1);
    }

    /*
     *   Open a text file and display a particular line number 
     */
    class CHtmlSysWin
        *open_text_file_at_line(class CHtmlDebugSysIfc_win *syswinifc,
                                const textchar_t *display_fname,
                                const textchar_t *path,
                                unsigned long linenum);

    /*
     *   Enumerate the source files making up the game.  Invokes the
     *   callback for each file.  The line source ID is a number that can
     *   be used to identify the line source in operations such as opening
     *   the file; the line sources are numbered consecutively starting at
     *   zero.  
     */
    void enum_source_files(void (*func)(void *cbctx, const textchar_t *fname,
                                        int line_source_id), void *cbctx);

    /* 
     *   Reload a single source window.  If win == 0, we'll reload and
     *   reformat all windows; otherwise, we'll reload only the given
     *   window.  
     */
    void reload_source_window(class CHtmlDebugSysIfc_win *syswinifc,
                              class CHtmlSysWin *win);

    /* flush output in the main window */
    static void flush_main_output(struct dbgcxdef *ctx)
        { vm_flush_main_output(ctx); }

    /*
     *   Get/set the "srcfiles" list status.
     *   
     *   For some GUI implementations, we must manage our own list of source
     *   files when a game isn't running, because the GUI doesn't have a user
     *   interface for managing the list of source files associated with the
     *   project and thus depends upon the compiled game's debug records for
     *   this information.  We can optionally maintain our own copy of the
     *   source list from the last time we loaded the compiled program.
     *   
     *   On Windows, the TADS 2 version of Workbench requires this separate
     *   source file list, because there's no UI to maintain a list of files
     *   in the project.  The Windows T3 Workbench does NOT need this
     *   separate source list, since it has its own "project" UI that allows
     *   the user to control the list of source files associated with the
     *   project.
     *   
     *   The GUI is responsible for setting this when constructing the helper
     *   object.  
     */
    int get_need_srcfiles() const { return need_srcfiles_; }

private:
    /* find a source file tracker */
    class CHtmlDbg_src *find_internal_src(int source_id) const;
    class CHtmlDbg_src *find_internal_src(CHtmlDebugSysIfc_win *syswinifc,
                                          const textchar_t *fname,
                                          const textchar_t *path,
                                          int add_if_not_found);
    class CHtmlDbg_src *find_internal_src(CHtmlDebugSysIfc_win *syswinifc,
                                          const textchar_t *path,
                                          int add_if_not_found)
    {
        return find_internal_src(syswinifc, path, path, add_if_not_found);
    }

    /* find a breakpoint tracker */
    class CHtmlDbg_bp *find_internal_bp(int bpnum) const;
    class CHtmlDbg_bp *find_internal_bp(int source_id,
                                        unsigned long linenum) const;

    /* toggle an internal breakpoint tracker */
    void toggle_internal_bp(dbgcxdef *ctx, int source_id,
                            unsigned long linenum,
                            const textchar_t *cond, int change,
                            int bpnum, int did_set, int global);

    /* toggle disabled status for an internal breakpoint tracker */
    void toggle_internal_bp_disable(dbgcxdef *ctx, int source_id,
                                    unsigned long linenum);

    /* delete an internal breakpoint tracker and update the display */
    void delete_internal_bp(dbgcxdef *ctx, int bpnum);

    /* delete a breakpoint tracker */
    void delete_internal_bp(CHtmlDbg_bp *bp);

    /* 
     *   synthesize a breakpoint number - this can be used when setting a
     *   breakpoint when the TADS engine isn't running 
     */
    int synthesize_bp_num() const;
    
    /*
     *   Reformat a source file 
     */
    void reformat_source_window(struct dbgcxdef *ctx,
                                class CHtmlDebugSysIfc_win *syswinifc,
                                struct CHtmlDbg_win_link *link,
                                const class CHtmlRect *orig_scroll_pos);
    
    /* 
     *   Reload all source windows -- this must be used when there's a
     *   change in the preferences that affects file loading, such as the
     *   tab expansion size.
     */
    void reload_source_windows(class CHtmlDebugSysIfc_win *syswinifc)
    {
        reload_source_window(syswinifc, 0);
    }

    /* change a line status bit */
    void change_line_status(int line_source_id, unsigned long linenum,
                            unsigned int statflag, int set);

    /* 
     *   Get the line information for a given breakpoint number.  Returns
     *   true if we found a line for the breakpoint, false if the
     *   breakpoint doesn't have a valid line (which is the case for a
     *   global breakpoint). 
     */
    int get_bp_line_info(int bpnum,
                         int *line_source_id, unsigned long *linenum);

    /* update all lines on screen for a particular source window */
    void update_lines_on_screen(struct CHtmlDbg_win_link *win_link);

    /* update a line's status on-screen */
    void update_line_on_screen(struct CHtmlDbg_line_link *line_link);

    /* update a line's status in a particular window */
    void update_line_on_screen(struct CHtmlDbg_win_link *win_link,
                               struct CHtmlDbg_line_link *line_link);

    /* display callback for updating the stack window */
    static void update_stack_disp_cb(void *ctx, const char *str, int strl);
    void update_stack_disp(struct HtmlDbg_stkdisp_t *ctx,
                           const char *str, int strl);

    /*
     *   Format a breakpoint description from our internal format to a
     *   format suitable for display to the user.  
     */
    void format_bp_for_display(const class CHtmlDbg_bp *bp,
                               textchar_t *buf, size_t buflen) const;

    /*
     *   Load the source file for the current line (i.e., the point at
     *   which execution is currently suspended), if it's not already
     *   loaded, and return the (possibly new) window showing that source
     *   file.  
     */
    class CHtmlSysWin
        *load_current_source(struct dbgcxdef *ctx,
                             class CHtmlDebugSysIfc_win *sysifc)
    {
        /* load the source at stack level 0 */
        return load_source_at_level(ctx, sysifc, 0);
    }

    /*
     *   Load the source file for the current line at a given stack level.
     *   Level 0 is the current frame, 1 is the first enclosing frame, and
     *   so on. 
     */
    class CHtmlSysWin
        *load_source_at_level(struct dbgcxdef *ctx,
                              class CHtmlDebugSysIfc_win *sysifc, int level);

    /*
     *   Load a source file into a new window and return the window.  If
     *   the file has already been loaded into a window, this simply
     *   return the existing window.  
     */
    struct CHtmlDbg_win_link *
        load_source_file(class CHtmlDebugSysIfc_win *sysifc,
                         class CHtmlDbg_src *src);

    /* 
     *   Load a source file from a given filename.  display_fname is the
     *   filename to display for the new window; path is the full name of
     *   the file 
     */
    struct CHtmlDbg_win_link *
        load_source_file(class CHtmlDebugSysIfc_win *sysifc,
                         const textchar_t *display_fname,
                         const textchar_t *path, int source_id);

    /*
     *   create a new source window; returns the tracking structure for
     *   the new window 
     */
    struct CHtmlDbg_win_link
        *create_source_win(class CHtmlDebugSysIfc_win *syswinifc,
                           const textchar_t *fname, const textchar_t *path,
                           int source_id);

    /* create a debugger window (source or other) */
    struct CHtmlDbg_win_link
        *create_win(class CHtmlDebugSysIfc_win *syswinifc,
                    const textchar_t *win_title,
                    const textchar_t *fname, const textchar_t *path,
                    int source_id, HtmlDbg_wintype_t win_type);

    /* add a window to/remove a window from our source window list */
    struct CHtmlDbg_win_link *add_src_win(class CHtmlSysWin *win,
                                          class CHtmlParser *parser,
                                          class CHtmlFormatter *formatter,
                                          const textchar_t *win_title,
                                          const textchar_t *fname,
                                          const textchar_t *path,
                                          int source_id,
                                          class CHtmlSysFont *font,
                                          HtmlDbg_wintype_t win_type);
    void del_src_win(struct CHtmlDbg_win_link *link);


    /* -------------------------------------------------------------------- */
    /*
     *   Private VM-specific interface.  These functions (whose names are
     *   of the form vm_xxx() to make it clear that they're part of this
     *   group) are specific to a particular underlying engine.
     *   
     *   We separate these functions so that the helper class can be
     *   completely independent of the underlying engine.  The helper
     *   class interacts with the VM only through these vm_xxx functions.
     *   The implementations of the vm_xxx functions are in a separate
     *   source file.  This allows the rest of the helper code (i.e., all
     *   of it outside the vm_xxx function group) to be used unchanged
     *   with multiple underlying engines.  In particular, the same helper
     *   code can be used with TADS 2 and with T3.  Note that, because the
     *   UI accesses the underlying engine only through the helper, the UI
     *   is also completely independent of the VM and can be shared
     *   identically among different engines.
     *   
     *   At link time, an executable is built with a particular
     *   implementation of the vm_xxx functions.  Choose the vm_xxx
     *   implementation module that is tailored to the underlying VM.  
     */

    /* 
     *   determine if the current selection in the given window is within
     *   the same function or method as the current execution point 
     */
    int vm_is_in_same_fn(struct dbgcxdef *ctx, class CHtmlSysWin *win);

    /* flush the main output */
    static void vm_flush_main_output(struct dbgcxdef *ctx);

    /* 
     *   get information on the current source line at the given stack
     *   level; returns zero on success, non-zero on failure 
     */
    int vm_get_source_info_at_level(struct dbgcxdef *ctx, CHtmlDbg_src **src,
                                    unsigned long *linenum, int level,
                                    class CHtmlDebugSysIfc_win *win);

    /* perform engine-specific checks on a newly-loaded program */
    void vm_check_loaded_program(struct dbgcxdef *ctx);

    /* load line sources from a compile game program */
    void vm_load_sources_from_program(struct dbgcxdef *ctx);

    /* set execution state */
    void vm_set_exec_state_go(struct dbgcxdef *ctx);
    void vm_set_exec_state_step_over(struct dbgcxdef *ctx);
    void vm_set_exec_state_step_out(struct dbgcxdef *ctx);
    void vm_set_exec_state_step_into(struct dbgcxdef *ctx);

    /* signal an ABORT condition */
    static void vm_signal_abort(struct dbgcxdef *ctx);

    /* signal a QUIT condition */
    static void vm_signal_quit(struct dbgcxdef *ctx);

    /* signal a RESTART condition */
    static void vm_signal_restart(struct dbgcxdef *ctx);

    /* 
     *   move the execution position; fill in *linenum with the actual
     *   line number where we moved the execution point 
     */
    int vm_set_next_statement(struct dbgcxdef *ctx,
                              unsigned int *exec_ofs,
                              CHtmlSysWin *win, unsigned long *linenum,
                              int *need_single_step);

    /* call tracing */
    int vm_is_call_trace_active(struct dbgcxdef *ctx) const;
    void vm_set_call_trace_active(struct dbgcxdef *ctx, int flag);
    void vm_clear_call_trace_log(struct dbgcxdef *ctx);
    const textchar_t *vm_get_call_trace_buf(struct dbgcxdef *ctx) const;
    unsigned long vm_get_call_trace_len(struct dbgcxdef *ctx) const;

    /* toggle a breakpoint at the current selection in the given window */
    void vm_toggle_breakpoint(struct dbgcxdef *ctx,
                              class CHtmlSysWin *win);

    /* set/clear a temporary breakpoint */
    int vm_set_temp_bp(struct dbgcxdef *ctx, class CHtmlSysWin *win);
    void vm_clear_temp_bp(struct dbgcxdef *ctx);

    /* set a global breakpoint */
    int vm_set_global_breakpoint(struct dbgcxdef *ctx,
                                 const textchar_t *cond, int change,
                                 int *bpnum, char *errbuf, size_t errbuflen);

    /* 
     *   enable/disable a breapoint at the current selection in the given
     *   window 
     */
    void vm_toggle_bp_disable(struct dbgcxdef *ctx,
                              class CHtmlSysWin *win);

    /* 
     *   enable or disable a breakpoint; returns zero on success, non-zero
     *   on failure 
     */
    int vm_enable_breakpoint(struct dbgcxdef *ctx, int bpnum, int enable);

    /* determine if a breakpoint is enabled */
    int vm_is_bp_enabled(struct dbgcxdef *ctx, int bpnum);

    /* set a breakpoint's condition */
    int vm_set_bp_condition(struct dbgcxdef *ctx, int bpnum,
                            const textchar_t *cond, int change,
                            char *errbuf, size_t errbuflen);

    /* delete a breakpoint */
    int vm_delete_breakpoint(struct dbgcxdef *ctx, int bpnum);

    /* set a breakpoint loaded from a saved configuration */
    int vm_set_loaded_bp(struct dbgcxdef *dbgctx, int source_id,
                         unsigned long orig_linenum,
                         unsigned long *actual_linenum,
                         const char *cond, int change, int disabled);

    /* get the text of an error message */
    void vm_get_error_text(struct dbgcxdef *ctx, int err,
                           textchar_t *buf, size_t buflen);

    /* format an error message from error stack parameters */
    void vm_format_error(struct dbgcxdef *ctx, int err,
                         textchar_t *buf, size_t buflen);

    /* evaluate an expression */
    int vm_eval_expr(struct dbgcxdef *ctx,
                     textchar_t *buf, size_t buflen,
                     const textchar_t *expr, int level,
                     int *is_lval, int *is_openable,
                     void (*aggcb)(void *, const char *, int,
                                   const char *),
                     void *aggctx, int speculative);

    /* execute an assignment */
    int vm_eval_asi_expr(struct dbgcxdef *ctx, int stack_level,
                         const textchar_t *lvalue, const textchar_t *rvalue);


    /* 
     *   Enumerate local variables at the given stack level.  Level 0 is
     *   the currently executing method, level 1 is the caller of the
     *   current method, level 2 is the caller of level 1, and so on. 
     */
    void vm_enum_locals(struct dbgcxdef *ctx, int level,
                        void (*cbfunc)(void *, const textchar_t *, size_t),
                        void *cbctx);

    /* 
     *   Build a stack listing.  Invoke static member function
     *   update_stack_disp_cb() for each line of text in the stack
     *   listing, passing cbctx to the function as the callback context on
     *   each call.  
     */
    void vm_build_stack_listing(struct dbgcxdef *ctx, void *cbctx);


    /*
     *   Load a source file into a window.  Returns zero on success,
     *   non-zero on failure.  
     */
    int vm_load_file_into_win(struct CHtmlDbg_win_link *win_link,
                              const textchar_t *path);

    /* toggle/get hidden output status */
    void vm_toggle_dbg_hid();
    int vm_get_dbg_hid();

    /* -------------------------------------------------------------------- */
    /*
     *   Member variables 
     */
    
    /* head and tail of source window list */
    struct CHtmlDbg_win_link *first_srcwin_;
    struct CHtmlDbg_win_link *last_srcwin_;

    /* head and tail of special line list */
    struct CHtmlDbg_line_link *first_line_;
    struct CHtmlDbg_line_link *last_line_;

    /* currently-executing line */
    int cur_is_valid_ : 1;
    int cur_source_id_;
    unsigned long cur_linenum_;

    /* 
     *   Current context line - this is the line that's selected as the
     *   current evaluation context from the stack list.  If this is not
     *   valid, the current line is used instead. 
     */
    int ctx_is_valid_ : 1;
    int ctx_source_id_;
    unsigned long ctx_linenum_;

    /* display item in stack window for current evaluation context level */
    class CHtmlDispTextDebugsrc *ctx_stackwin_disp_;

    /* the stack window */
    struct CHtmlDbg_win_link *stack_win_;

    /* the debug log window */
    struct CHtmlDbg_win_link *debuglog_win_;

    /* the history window */
    struct CHtmlDbg_win_link *hist_win_;

    /* tab expansion size setting */
    int tab_size_;

    /* temporary breakpoint number, and flag indicating if it's set */
    int tmp_bpnum_;
    int tmp_bp_valid_ : 1;

    /* head of internal list of breakpoints */
    class CHtmlDbg_bp *bp_;

    /* tail of internal list of breakpoints */
    class CHtmlDbg_bp *bp_tail_;

    /* internal list of source files */
    class CHtmlDbg_src *src_;

    /* reference count */
    int refcnt_;

    /*
     *   Next available line source ID.  After the game file is loaded,
     *   we'll assign ID's with numbers higher than any in the compiled
     *   game.  We need to assign such ID's to arbitrary text files that
     *   are opened.  
     */
    int next_line_source_id_;

    /* highest line source ID in the game file */
    int last_compiled_line_source_id_;

    /* flag: we need to maintain the "srcfiles" list */
    int need_srcfiles_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Breakpoint tracker 
 */
class CHtmlDbg_bp
{
public:
    CHtmlDbg_bp(int global, int source_id, unsigned long linenum,
                int bpnum, const textchar_t *cond, int change)
    {
        /* remember the description */
        global_ = global;
        source_id_ = source_id;
        linenum_ = linenum;
        bpnum_ = bpnum;
        cond_.set(cond);
        stop_on_change_ = change;

        /* initially enable the breakpoint */
        enabled_ = TRUE;

        /* we're not in a list yet */
        nxt_ = 0;
    }

    /* flag: it's a global breakpoint (not at a source line) */
    unsigned int global_ : 1;

    /* flag: breakpoint is enabled */
    unsigned int enabled_ : 1;

    /* 
     *   if there's a condition, do we stop when the condition changes or
     *   when it's true? 
     */
    unsigned int stop_on_change_ : 1;

    /* source file ID and line number */
    int source_id_;
    unsigned long linenum_;

    /* TADS internal breakpoint ID */
    int bpnum_;

    /* condition text */
    CStringBuf cond_;

    /* next in list */
    CHtmlDbg_bp *nxt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Source file tracker 
 */
class CHtmlDbg_src
{
public:
    CHtmlDbg_src(const textchar_t *fname, const textchar_t *path,
                 int source_id);

    ~CHtmlDbg_src()
    {
    }

    /* display filename */
    CStringBuf fname_;

    /* full file path */
    CStringBuf path_;

    /* TADS internal source ID */
    int source_id_;

    /* next in list */
    CHtmlDbg_src *nxt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   System window tracker.  We keep a list of the source windows we've
 *   opened using a linked list of these structures. 
 */
struct CHtmlDbg_win_link
{
    CHtmlDbg_win_link(class CHtmlSysWin *win, class CHtmlParser *parser,
                      class CHtmlFormatter *formatter,
                      const textchar_t *win_title,
                      const textchar_t *fname, const textchar_t *path,
                      int source_id,
                      class CHtmlSysFont *font, HtmlDbg_wintype_t win_type);

    /* next and previous link in the list */
    CHtmlDbg_win_link *next_;
    CHtmlDbg_win_link *prev_;

    /* our system window */
    class CHtmlSysWin *win_;

    /* the parser associated with the window */
    class CHtmlParser *parser_;

    /* formatter associated with the window */
    class CHtmlFormatter *formatter_;

    /* name of the file (not including directory path) */
    CStringBuf fname_;

    /* full name and path of the file */
    CStringBuf path_;

    /* line source ID */
    int source_id_;

    /* default font */
    class CHtmlSysFont *font_;

    /* type of window */
    HtmlDbg_wintype_t win_type_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Special-line tracker.  We keep a list of lines that have a special
 *   characteristic, such as being the current line executing or having a
 *   breakpoint.  These characteristics show up visually in source
 *   windows, so we need to update the corresponding source window
 *   whenever a line's status changes.  Since a line might not always be
 *   loaded into a window when its status changes, this structure lets us
 *   update a window when it's initially loaded, as well as keeping it up
 *   to date while it's loaded. 
 */

struct CHtmlDbg_line_link
{
    CHtmlDbg_line_link(int source_id, unsigned long linenum,
                       unsigned int init_stat);

    /* determine if this line matches a given line */
    int equals(int source_id, unsigned long linenum)
    {
        return (source_id_ == source_id && linenum_ == linenum);
    }
    
    /* next/previous link in the list */
    CHtmlDbg_line_link *next_;
    CHtmlDbg_line_link *prev_;
    
    /* 
     *   Line source ID (linid) of the file containing the line.  Since
     *   special lines are always source files referenced by the .GAM file
     *   (we can't have breakpoints or execution in any files not in the
     *   debug records, after all), special lines always have valid line
     *   source ID's. 
     */
    int source_id_;

    /* line number in the source file */
    unsigned long linenum_;

    /* 
     *   The current set of status flags associated with the line.  If a
     *   line's status flags become zero, we'll drop the link from the
     *   list (after updating the screen accordingly), since there's
     *   nothing to keep track of any more. 
     */
    unsigned int stat_;
};


#endif /* HTMLDBG_H */

