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
  htmldb2.cpp - debugger helper class - TADS 2 Interface
Function
  This module contains the helper class interfaces to the TADS 2 debugger
  internal engine API.

  The code in this module is separated from the main debugger helper class
  so that the main helper class is independent of the underlying VM engine
  and its debugger API.  This allows the main debugger helper to be shared
  by different engines; in particular, this allows the main debugger helper
  class to be used for TADS 2 and T3 debuggers.  Because the UI code uses
  the helper class to access the VM engine (the UI code never calls the
  engine directly - it always uses the helper class instead), the UI code
  is also independent of the underlying engine.
Notes
  
Modified
  10/11/99 MJRoberts  - Creation
*/

#include <string.h>

/* include necessary TADS interfaces */
#include <dbg.h>
#include <err.h>
#include <lin.h>
#include <linf.h>
#include <dat.h>
#include <os.h>
#include <bif.h>
#include <obj.h>


#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Debugger entrypoints 
 */

/*
 *   initialize 
 */
void dbguini(dbgcxdef *ctx, const char *game_filename)
{
    void *ui_obj;
    
    /* call the UI implementation */
    ui_obj = html_dbgui_ini(ctx, game_filename);

    /* store the UI object in the engine context */
    ctx->dbgcxui = ui_obj;
}

/*
 *   initialize, phase two 
 */
void dbguini2(dbgcxdef *ctx)
{
    html_dbgui_ini2(ctx, ctx->dbgcxui);
}

/*
 *   determine if we can resume from an error 
 */
int dbgu_err_resume(dbgcxdef *ctx)
{
    return html_dbgui_err_resume(ctx, ctx->dbgcxui);
}

/*
 *   find a source file 
 */
int dbgu_find_src(const char *origname, int origlen,
                  char *fullname, size_t full_len, int must_find_file)
{
    return html_dbgui_find_src(origname, origlen, fullname, full_len,
                               must_find_file);
}

/*
 *   main command loop entrypoint 
 */
void dbgucmd(struct dbgcxdef *ctx,
             int bphit, int err, unsigned int *exec_ofs)
{
    html_dbgui_cmd(ctx, ctx->dbgcxui, bphit, err, exec_ofs);
}

/*
 *   quitting game 
 */
void dbguquitting(struct dbgcxdef *ctx)
{
    html_dbgui_quitting(ctx, ctx->dbgcxui);
}

/*
 *   terminating 
 */
void dbguterm(struct dbgcxdef *ctx)
{
    /* tell the UI we're shutting down */
    html_dbgui_term(ctx, ctx->dbgcxui);

    /* clear the UI object in the engine debug context */
    ctx->dbgcxui = 0;
}

/*
 *   display an error
 */
void dbguerr(struct dbgcxdef *ctx, int errno, char *msg)
{
    html_dbgui_err(ctx, ctx->dbgcxui, errno, msg);
}


/* ------------------------------------------------------------------------ */
/*
 *   Helper class extension.  This class encapsulates VM-specific private
 *   functions.  These functions are called by the vm_xxx code and are
 *   private to the vm_xxx code - the generic debugger helper class
 *   doesn't call any of these functions.
 *   
 *   This class isn't ever instantiated; all of the methods are static.
 *   The only real reason we need this class at all is so that we can make
 *   it a friend of CHtmlDebugHelper, so that our private vm_xxx
 *   implementation functions can call back into the CHtmlDebugHelper
 *   private parts; this is a valid need because CHtmlDebugHelperVM is
 *   effectively an extension of CHtmlDebugHelper, but couldn't be part of
 *   CHtmlDebugHelper because of the need to keep the VM-specific
 *   functions out of the public interface.  (In particular, each VM
 *   implementation will have its own set of functions in
 *   CHtmlDebugHelperVM, so we can't come up with a set in
 *   CHtmlDebugHelper that will serve everyone.)  
 */
class CHtmlDebugHelperVM
{
public:
    /* toggle the enabled status of the breakpoint at the given location */
    static void toggle_bp_disable(CHtmlDebugHelper *helper,
                                  struct dbgcxdef *ctx,
                                  int source_id, unsigned long linenum,
                                  objnum line_objn, int line_ofs);

    /* set or clear a breakpoint at the given source location */
    static int toggle_breakpoint(CHtmlDebugHelper *helper,
                                 struct dbgcxdef *ctx,
                                 int source_id, unsigned long linenum,
                                 objnum line_objn, uint line_ofs,
                                 const textchar_t *cond, int set_only);

    /* given a source location, get the code address */
    static int srcofs_to_code(CHtmlDebugHelper *helper,
                              dbgcxdef *ctx, CHtmlSysWin *win,
                              unsigned long *linenum,
                              CHtmlDbg_win_link **win_link,
                              objnum *line_objn, uint *line_ofs);

    /* given a source location, get the code address */
    static int srcofs_to_code(dbgcxdef *ctx, int source_id,
                              unsigned long *linenum,
                              objnum *line_objn, uint *line_ofs);
};

/* ------------------------------------------------------------------------ */
/* 
 *   hidden output status - this is a global variable defined by the TADS
 *   2 debugger engine 
 */
extern "C" int dbghid;


/* ------------------------------------------------------------------------ */
/*
 *   Hidden output tracking.  The TADS 2 engine requires that the debugger
 *   UI implement these functions.  
 */

/*
 *   Turn hidden output tracking on/off 
 */
void trchid()
{
    outflushn(1);
    os_printz("---Hidden output---");
    outflushn(1);
}

void trcsho()
{
    outflushn(1);
    os_printz("---End of hidden output, normal output resumes---");
    outflushn(1);
}


/* ------------------------------------------------------------------------ */
/*
 *   Flush output to the main game text window 
 */
void CHtmlDebugHelper::vm_flush_main_output(struct dbgcxdef *)
{
    outflushn(0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the object and p-code offset for the current selection in the
 *   given window.  Returns zero on success, non-zero on failure.  On
 *   return, *fpos will be changed to reflect the actual source file
 *   location closest to the input fpos value that contains a line start.
 *   *win_link is filled in with a pointer to the window link structure
 *   for the given window.  
 */
int CHtmlDebugHelperVM::srcofs_to_code(CHtmlDebugHelper *helper,
                                       dbgcxdef *ctx, CHtmlSysWin *win,
                                       unsigned long *linenum,
                                       CHtmlDbg_win_link **win_link,
                                       objnum *line_objn, uint *line_ofs)
{
    CHtmlDbg_line_link *line_link;

    /* find information the current line in the window */
    *linenum = helper->find_line_info(win, win_link, &line_link);

    /* if we didn't find the window, return failure */
    if (*win_link == 0)
        return 1;

    /* get information on this position */
    return srcofs_to_code(ctx, (*win_link)->source_id_, linenum,
                          line_objn, line_ofs);
}

/*
 *   Find the line source for a given line source ID 
 */
static lindef *find_lindef(dbgcxdef *ctx, int id)
{
    lindef *lin;

    /* scan the list of line sources */
    for (lin = ctx->dbgcxlin ; lin != 0 ; lin = lin->linnxt)
    {
        /* if this is the one, return it */
        if (lin->linid == id)
            return lin;
    }

    /* not found */
    return 0;
}

/*
 *   Find the nearest executable line to a given source location.  If TADS
 *   isn't running, we won't make any adjustment; otherwise, we'll fix up
 *   the line number to the nearest line with executable code, and we'll
 *   fill in *line_objn and *line_ofs with the p-code location of the
 *   code.  
 */
int CHtmlDebugHelperVM::srcofs_to_code(dbgcxdef *ctx, int source_id,
                                       unsigned long *linenum,
                                       objnum *line_objn, uint *line_ofs)
{
    lindef *lin;
    uchar posbuf[4];

    /* 
     *   if there's no context, the actual source location is the best we
     *   can do for now - we have no game file to tell us the location of
     *   the next executable line 
     */
    if (ctx == 0)
    {
        *line_objn = MCMONINV;
        *line_ofs = 0;
        return 0;
    }

    /* get the line source */
    lin = find_lindef(ctx, source_id);
    if (lin == 0)
        return 2;

    /* find the source line at this location */
    oswp4(posbuf, *linenum);
    linfind(lin, (char *)posbuf, line_objn, line_ofs);

    /* 
     *   adjust the file position to the actual value -- this may be
     *   different than the text the user was pointing at, since not every
     *   line contains executable code 
     */
    *linenum = osrp4(posbuf);

    /* make sure we have a valid location */
    if (*line_objn == MCMONINV)
        return 3;

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get information on the current source line at the given stack level.
 *   Returns zero on success, non-zero on failure.  
 */
int CHtmlDebugHelper::
   vm_get_source_info_at_level(dbgcxdef *ctx, CHtmlDbg_src **src,
                               unsigned long *linenum, int level,
                               class CHtmlDebugSysIfc_win *)
{
    uchar infobuf[128];

    /* get information on the current line */
    if (dbglgetlvl(ctx, infobuf, level))
        return 1;

    /* search for a line source with the given ID */
    *src = find_internal_src((int)infobuf[0]);

    /* if we didn't find a line source with the current line, give up */
    if (*src == 0)
        return 2;

    /* get the current line number */
    *linenum = osrp4(infobuf + 1);

    /* success */
    return 0;
}
                               

/* ------------------------------------------------------------------------ */
/*
 *   Perform engine-specific checks on a newly-loaded a game 
 */
void CHtmlDebugHelper::vm_check_loaded_program(dbgcxdef *ctx)
{
    /* 
     *   check to make sure that the correct debug info is available if we
     *   have a game loaded (we might only have a config loaded, in which
     *   case it's too early to determine if the game has the correct
     *   debug information) 
     */
    if (ctx != 0 && !(ctx->dbgcxflg & DBGCXFLIN2))
        errsig(ctx->dbgcxerr, ERR_NEEDLIN2);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load line sources from a compiled game program 
 */
void CHtmlDebugHelper::vm_load_sources_from_program(dbgcxdef *ctx)
{
    lindef *lin;

    /* read line sources from the compiled game */
    for (lin = ctx->dbgcxlin ; lin != 0 ; lin = lin->linnxt)
    {
        linfdef *linf;

        /* create and add a new source tracker for this line */
        linf = (linfdef *)lin;
        add_internal_line_source(linf->linfnam,
                                 linf->linfnam + strlen(linf->linfnam)+1,
                                 lin->linid);
        
        /* if this is the highest ID yet, note it */
        if (lin->linid > last_compiled_line_source_id_)
            last_compiled_line_source_id_ = lin->linid;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set engine execution state to GO 
 */
void CHtmlDebugHelper::vm_set_exec_state_go(dbgcxdef *ctx)
{
    /* clear the stepping flags */
    ctx->dbgcxflg &= ~(DBGCXFSS + DBGCXFSO);
}

/*
 *   Set engine execution state to STEP OVER 
 */
void CHtmlDebugHelper::vm_set_exec_state_step_over(dbgcxdef *ctx)
{
    /* set the step and step-over-calls flags */
    ctx->dbgcxflg |= (DBGCXFSS + DBGCXFSO);

    /* keep stepping until we're back at the current depth again */
    ctx->dbgcxsof = ctx->dbgcxdep;

}

/*
 *   Set engine execution state to STEP OUT 
 */
void CHtmlDebugHelper::vm_set_exec_state_step_out(dbgcxdef *ctx)
{
    /* set the step and step-over-calls flags */
    ctx->dbgcxflg |= (DBGCXFSS + DBGCXFSO);

    /* keep stepping until we're back at the *enclosing* depth */
    ctx->dbgcxsof = (ctx->dbgcxdep >= 1 ? ctx->dbgcxdep - 1 : 0);
}

/*
 *   Set engine execution state to STEP INTO 
 */
void CHtmlDebugHelper::vm_set_exec_state_step_into(dbgcxdef *ctx)
{
    /* set the stepping flag, but clear the step-over-calls flag */
    ctx->dbgcxflg |= DBGCXFSS;
    ctx->dbgcxflg &= ~DBGCXFSO;
}

/*
 *   Signal a QUIT condition in the engine
 */
void CHtmlDebugHelper::vm_signal_quit(dbgcxdef *ctx)
{
    /* signal the TADS error condition for quitting the game */
    errsig(ctx->dbgcxerr, ERR_RUNQUIT);
}

/*
 *   Signal a RESTART condition in the engine 
 */
void CHtmlDebugHelper::vm_signal_restart(dbgcxdef *ctx)
{
    /* 
     *   clear the in-debugger flag - we're leaving the debugger via the
     *   signal, so we don't need to keep this flag set 
     */
    ctx->dbgcxflg &= ~DBGCXFIND;

    /* clear the debugger stack */
    ctx->dbgcxdep = 0;
    ctx->dbgcxfcn = 0;

    /* 
     *   go restart via the built-in restart() function, which takes care
     *   of resetting all the run-time context information 
     */
    bifres((bifcxdef *)ctx->dbgcxrun->runcxbcx, 0);
}

/*
 *   Signal abort in the engine 
 */
void CHtmlDebugHelper::vm_signal_abort(dbgcxdef *ctx)
{
    /* signal the TADS error condition for quitting the game */
    errsig(ctx->dbgcxerr, ERR_RUNABRT);
}

/* ------------------------------------------------------------------------ */
/*
 *   Move the execution position in the engine to the current selection in
 *   the given window.  Returns zero on success, non-zero on failure.  
 */
int CHtmlDebugHelper::vm_set_next_statement(dbgcxdef *ctx,
                                            unsigned int *exec_ofs,
                                            CHtmlSysWin *win,
                                            unsigned long *linenum,
                                            int *need_single_step)
{
    CHtmlDbg_win_link *win_link;
    objnum line_objn;
    uint line_ofs;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, linenum, &win_link,
                                           &line_objn, &line_ofs))
        return 2;

    /* move the execution point to the new offset */
    *exec_ofs = line_ofs;

    /* 
     *   TADS 2 requires a single step after changing the execution
     *   position, to set up the new location context in the new line
     *   (specifically, we need to execute the OPCLINE instruction at the
     *   start of the new source line) 
     */
    *need_single_step = TRUE;

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check if the current selection in the given window is within the same
 *   function or method as the current execution point.  Returns true if
 *   so, false if not.  This is a condition of setting the next statement;
 *   this function is provided so that the UI code can check to make sure
 *   that this condition is met when the user attempts to move the
 *   execution point, and generate an appropriate error message if not.  
 */
int CHtmlDebugHelper::vm_is_in_same_fn(dbgcxdef *ctx,
                                       CHtmlSysWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long fpos;
    objnum line_objn;
    uint line_ofs;
    uint start_ofs, end_ofs;
    dbgfdef *fr;

    /* if there's no context, there's no way to compare them */
    if (ctx == 0)
        return FALSE;

    /* get the current frame - if there isn't one, return failure */
    if (ctx->dbgcxfcn == 0)
        return FALSE;
    fr = &ctx->dbgcxfrm[ctx->dbgcxfcn - 1];

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &fpos, &win_link,
                                           &line_objn, &line_ofs))
        return FALSE;

    /* 
     *   if this line is not in the same object as the current target
     *   object, we can disallow it 
     */
    if (line_objn == MCMONINV || line_objn != fr->dbgftarg)
        return FALSE;

    /* 
     *   if there's no property, it's a function, so it's valid as long as
     *   it's in the current object (which we've just determined that it
     *   is) 
     */
    if (fr->dbgfprop == (prpnum)0)
        return TRUE;

    /* find the limits of the property's data range */
    start_ofs = objgetp(ctx->dbgcxmem, line_objn, fr->dbgfprop, 0);
    end_ofs = objgetp_end(ctx->dbgcxmem, line_objn, fr->dbgfprop);

    /* if it's within the range, it's valid, otherwise it's not */
    return (line_ofs >= start_ofs && line_ofs < end_ofs);
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if call tracing is active in the engine
 */
int CHtmlDebugHelper::vm_is_call_trace_active(dbgcxdef *ctx) const
{
    return ((ctx->dbgcxflg & DBGCXFTRC) != 0);
}

/*
 *   turn call trace on or off in the engine
 */
void CHtmlDebugHelper::vm_set_call_trace_active(dbgcxdef *ctx, int flag)
{
    /* set or clear the trace flag as appropriate */
    if (flag)
        ctx->dbgcxflg |= DBGCXFTRC;
    else
        ctx->dbgcxflg &= ~DBGCXFTRC;
}

/*
 *   Clear the call trace log in the engine 
 */
void CHtmlDebugHelper::vm_clear_call_trace_log(dbgcxdef *ctx)
{
    /* forget everything in the buffer */
    ctx->dbgcxhstf = 0;

}

/*
 *   Get a pointer to the history log buffer maintained by the engine 
 */
const textchar_t *CHtmlDebugHelper::vm_get_call_trace_buf(dbgcxdef *ctx) const
{
    return ctx->dbgcxhstp;
}

/*
 *   get the size of the data accumulated in the history log 
 */
unsigned long CHtmlDebugHelper::vm_get_call_trace_len(dbgcxdef *ctx) const
{
    return ctx->dbgcxhstf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set or clear a breakpoint at a given location.  Returns zero on
 *   success, non-zero on error.  
 */
int CHtmlDebugHelperVM::toggle_breakpoint(CHtmlDebugHelper *helper,
                                          dbgcxdef *ctx,
                                          int source_id,
                                          unsigned long linenum,
                                          objnum line_objn, uint line_ofs,
                                          const textchar_t *cond,
                                          int set_only)
{
    int bpnum;
    int did_set;
    char bpname[HTMLDBG_MAXBPNAME];
    CHtmlDbg_src *src;

    /* find the internal source file tracker */
    src = helper->find_internal_src(source_id);
    if (src == 0)
        return 1;

    /*
     *   Build the name of the breakpoint for display and configuration
     *   saving purposes.  The name consists of the name of the line
     *   source and the source offset.  
     */
    sprintf(bpname, "#%lu %s", linenum, src->fname_.get());

    /*
     *   If we can only set a new breakpoint, check to see if there's
     *   already a breakpoint at this location.  If there is, disallow it.
     */
    if (set_only && helper->find_internal_bp(source_id, linenum) != 0)
        return 2;

    /* set the breakpoint in the engine, if we're running */
    if (ctx != 0)
    {
        /* set the breakpoint in TADS */
        if (dbgbpat(ctx, line_objn, MCMONINV, line_ofs, &bpnum,
                    bpname, TRUE, (char *)cond, &did_set))
            return 3;
    }

    /* toggle the internal breakpoint record */
    helper->toggle_internal_bp(ctx, source_id, linenum, 0,
                               bpnum, FALSE, did_set, FALSE);

    /* success */
    return 0;
}

/*
 *   Toggle a breakpoint at the current selection in the given window 
 */
void CHtmlDebugHelper::vm_toggle_breakpoint(dbgcxdef *ctx,
                                            CHtmlSysWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long linenum;
    objnum line_objn;
    uint line_ofs;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win,
                                           &linenum, &win_link,
                                           &line_objn, &line_ofs))
        return;

    /* toggle the breakpoint at the given location */
    CHtmlDebugHelperVM::toggle_breakpoint(this, ctx, win_link->source_id_,
                                          linenum, line_objn, line_ofs,
                                          0, FALSE);
}

/*
 *   Set a temporary breakpoint 
 */
int CHtmlDebugHelper::vm_set_temp_bp(dbgcxdef *ctx, CHtmlSysWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long fpos;
    objnum line_objn;
    uint line_ofs;
    int did_set;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &fpos, &win_link,
                                           &line_objn, &line_ofs))
        return FALSE;

    /* if there's already a breakpoint there, there's nothing to do */
    if (dbgisbp(ctx, line_objn, MCMONINV, line_ofs, &tmp_bpnum_))
        return FALSE;

    /* set the breakpoint */
    if (dbgbpat(ctx, line_objn, MCMONINV, line_ofs, &tmp_bpnum_,
                "Temp BP", FALSE, 0, &did_set) || !did_set)
        return FALSE;

    /* success */
    return TRUE;
}

/*
 *   Clear a temporary breakpoint 
 */
void CHtmlDebugHelper::vm_clear_temp_bp(dbgcxdef *ctx)
{
    /* delete the temporary breakpoint in the VM */
    dbgbpdel(ctx, tmp_bpnum_);
}

/*
 *   Set a global breakpoint 
 */
int CHtmlDebugHelper::vm_set_global_breakpoint(dbgcxdef *ctx,
                                               const textchar_t *cond,
                                               int change,
                                               int *bpnum,
                                               char *errbuf, size_t errbuflen)
{
    /* 
     *   Add the breakpoint in the engine, if we're running.  Use the name
     *   "g" for all global breakpoints; since the breakpoint is based
     *   entirely on the condition string, we don't need to remember
     *   anything about it but that it has a condition string.  
     */
    if (ctx != 0)
    {
        int err;
        
        /* set it in the engine */
        err = dbgbpat(ctx, MCMONINV, MCMONINV, 0, bpnum, "g", TRUE,
                      (textchar_t *)cond, 0);

        /* if an error occurred, get the message and return the error */
        if (err != 0)
        {
            char msg[256];
            
            /* get the message into the caller's error buffer */
            errmsg(ctx->dbgcxerr, msg, sizeof(msg), err);
            errfmt(errbuf, errbuflen, msg, 0, 0);
            
            /* return failure */
            return err;
        }
    }
    else
    {
        /* the engine isn't running; synthesize a breakpoint ID */
        *bpnum = synthesize_bp_num();
    }

    /* add an entry to our breakpoint tracker list */
    toggle_internal_bp(ctx, 0, 0, cond, change, *bpnum, TRUE, TRUE);

    /* success */
    return 0;
}

/*
 *   Enable or disable a breakpoint at the current selection in the given
 *   window 
 */
void CHtmlDebugHelper::vm_toggle_bp_disable(dbgcxdef *ctx,
                                            CHtmlSysWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long linenum;
    objnum line_objn;
    uint line_ofs;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &linenum,
                                           &win_link, &line_objn, &line_ofs))
        return;

    /* toggle the status */
    CHtmlDebugHelperVM::toggle_bp_disable(this, ctx, win_link->source_id_,
                                          linenum, line_objn, line_ofs);
}

/*
 *   enable/disable a breakpoint at the given location 
 */
void CHtmlDebugHelperVM::toggle_bp_disable(CHtmlDebugHelper *helper,
                                           dbgcxdef *ctx,
                                           int source_id,
                                           unsigned long linenum,
                                           objnum line_objn, int line_ofs)
{
    int bpnum;
    int disabled;

    /* 
     *   if the engine is running, ask it if there's a breakpoint there -
     *   if not, there's nothing to do 
     */
    if (ctx != 0)
    {
        if (!dbgisbp(ctx, line_objn, MCMONINV, line_ofs, &bpnum))
            return;

        /* toggle the enabled/disabled state of the breakpoint */
        disabled = !dbgisbpena(ctx, bpnum);
        dbgbpdis(ctx, bpnum, disabled);
    }

    /* toggle the status of the internal breakpoint record */
    helper->toggle_internal_bp_disable(ctx, source_id, linenum);
}

/*
 *   Enable or disable a breakpoint 
 */
int CHtmlDebugHelper::vm_enable_breakpoint(dbgcxdef *ctx, int bpnum,
                                           int enable)
{
    /* enable/disable the breakpoint in the debugger */
    return dbgbpdis(ctx, bpnum, !enable);
}

/*
 *   Determine if a breakpoint is enabled 
 */
int CHtmlDebugHelper::vm_is_bp_enabled(dbgcxdef *ctx, int bpnum)
{
    return dbgisbpena(ctx, bpnum);
}

/*
 *   Set a breakpoint's condidtion text 
 */
int CHtmlDebugHelper::vm_set_bp_condition(dbgcxdef *ctx, int bpnum,
                                          const textchar_t *cond, int change,
                                          char *errbuf, size_t errbuflen)
{
    int err;

    /* try setting the condition */
    err = dbgbpsetcond(ctx, bpnum, (textchar_t *)cond);

    /* if that failed, get the error text into the message buffer */
    if (err != 0)
    {
        char msg[256];
        
        errmsg(ctx->dbgcxerr, msg, sizeof(msg), err);
        errfmt(errbuf, errbuflen, msg, 0, 0);
    }

    /* return the result code */
    return err;
}


/*
 *   Delete a breakpoint given a breakpoint number 
 */
int CHtmlDebugHelper::vm_delete_breakpoint(dbgcxdef *ctx, int bpnum)
{
    int err;

    /* if TADS is running, tell TADS to delete the breakpoint */
    if (ctx != 0 && (err = dbgbpdel(ctx, bpnum)) != 0)
        return err;

    /* delete the internal breakpoint and update the display */
    delete_internal_bp(ctx, bpnum);

    /* success */
    return 0;
}

/*
 *   Set a breakpoint loaded from a saved configuration.  Returns zero on
 *   success, non-zero on failure.  We'll fill in *actual_linenum with the
 *   line number at which we actually set the breakpoint; this might
 *   differ from the proposed line number because the source might have
 *   been modified and recompiled since the configuration was saved, hence
 *   the lines at which breakpoints are valid might have changed.  
 */
int CHtmlDebugHelper::vm_set_loaded_bp(dbgcxdef *dbgctx, int source_id,
                                       unsigned long orig_linenum,
                                       unsigned long *actual_linenum,
                                       const char *cond, int change,
                                       int disabled)
{
    objnum line_objn;
    uint line_ofs;

    /* try putting the breakpoint at the requested location */
    *actual_linenum = orig_linenum;
    
    /* get the exact location of the breakpoint */
    if (CHtmlDebugHelperVM::srcofs_to_code(dbgctx, source_id, actual_linenum,
                                           &line_objn, &line_ofs))
        return 1;

    /* set the breakpoint */
    if (CHtmlDebugHelperVM::toggle_breakpoint(this, dbgctx, source_id,
                                              *actual_linenum,
                                              line_objn, line_ofs,
                                              cond, TRUE))
        return 2;

    /* if it's disabled, disable it */
    if (disabled)
        CHtmlDebugHelperVM::toggle_bp_disable(this, dbgctx, source_id,
                                              *actual_linenum,
                                              line_objn, line_ofs);

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the text of an error message 
 */
void CHtmlDebugHelper::vm_get_error_text(struct dbgcxdef *ctx, int err,
                                         textchar_t *buf, size_t buflen)
{
    /* get the text */
    errmsg(ctx->dbgcxerr, buf, buflen, err);
}

/*
 *   Format an error message with arguments from the error stack 
 */
void CHtmlDebugHelper::vm_format_error(struct dbgcxdef *ctx, int err,
                                       textchar_t *buf, size_t buflen)
{
    errcxdef *errctx = ctx->dbgcxerr;
    char msgtxt[256];

    /* get the text of the message */
    errmsg(errctx, msgtxt, sizeof(msgtxt), err);

    /* format it with arguments from the error stack */
    errfmt(buf, buflen, msgtxt,
           errctx->errcxptr->erraac, errctx->errcxptr->erraav);
}

/* ------------------------------------------------------------------------ */
/*
 *   callback structure for eval_expr_cb 
 */
struct eval_expr_cb_ctx_t
{
    /* debugger context */
    struct dbgcxdef *dbgctx;

    /* pointer to start of buffer and original length */
    textchar_t *buf;
    size_t buflen;

    /* pointer to the current position in the output buffer */
    textchar_t *outp;

    /* number of characters remaining in output buffer */
    size_t rem;

    /* logged error number */
    int err;

    /* flag indicating whether we want to keep logged error messages */
    int log_errors;
};

/*
 *   Display callback for eval_expr - simply adds text to the output
 *   buffer 
 */
static void eval_expr_cb(void *ctx0, const char *txt, int len)
{
    eval_expr_cb_ctx_t *ctx = (eval_expr_cb_ctx_t *)ctx0;

    /* if an error has been logged, don't record any more output */
    if (ctx->err != 0)
        return;

    /* if this is a single newline, omit it */
    if (len == 1 && *txt == '\n')
        return;

    /* limit the copy to the amount of room we have */
    if ((size_t)len > ctx->rem)
        len = (int)ctx->rem;

    /* copy the characters, turning newlines into spaces */
    while (len > 0)
    {
        *(ctx->outp++) = (*txt == '\n' ? ' ' : *txt);
        ++txt;
        --(ctx->rem);
        --len;
    }
}

/*
 *   Error logging callback for expression evaluator.  We'll put the error
 *   into our buffer and note the error.  
 */
static void eval_expr_errlog(void *ctx0, char * /*fac*/, int err,
                             int argc, erradef *argv)
{
    char msg[256];
    eval_expr_cb_ctx_t *ctx = (eval_expr_cb_ctx_t *)ctx0;

    /* keep the message only if we're currently logging errors */
    if (ctx->log_errors)
    {
        /* format the message into the buffer at the start of the buffer */
        errmsg(ctx->dbgctx->dbgcxerr, msg, sizeof(msg), err);
        errfmt(ctx->buf, ctx->buflen, msg, argc, argv);

        /* position the write pointer after the message */
        ctx->outp = ctx->buf + get_strlen(ctx->buf);
        ctx->rem = ctx->buflen - get_strlen(ctx->buf);
    }

    /* record the error */
    ctx->err = err;
}

/*
 *   Evaluate an assignment 
 */
int CHtmlDebugHelper::vm_eval_asi_expr(dbgcxdef *ctx, int level,
                                       const textchar_t *lvalue,
                                       const textchar_t *rvalue)
{
    textchar_t asi_expr[2048];
    textchar_t result[10];

    /* build the assignment expression */
    sprintf(asi_expr, "(%s):=%s", lvalue, rvalue);

    /* evaluate the expression */
    return vm_eval_expr(ctx, result, sizeof(result), asi_expr, level,
                        0, 0, 0, 0, FALSE);
}

/*
 *   Evaluate an expression 
 */
int CHtmlDebugHelper::
   vm_eval_expr(struct dbgcxdef *ctx, textchar_t *buf, size_t buflen,
                const textchar_t *expr, int level,
                int *is_lval, int *is_openable,
                void (*aggcb)(void *, const char *, int,
                              const char *), void *aggctx, int speculative)
{
    int err;
    eval_expr_cb_ctx_t cbctx;
    void (*old_errcxlog)(void *, char *, int, int, erradef *);
    void  *old_errcxlgc;
    dattyp dat;

    /* we can't proceed if there's no context */
    if (ctx == 0)
    {
        buf[0] = '\0';
        return 1;
    }

    /* 
     *   Set up the callback context to point to the output buffer.
     *   Reserve one character of the buffer for null termination.  
     */
    cbctx.dbgctx = ctx;
    cbctx.outp = cbctx.buf = buf;
    cbctx.rem = cbctx.buflen = buflen - 1;
    cbctx.err = 0;
    cbctx.log_errors = TRUE;

    /* if we're not in the debugger, we can't evaluate the expression */
    if (!(ctx->dbgcxflg & DBGCXFIND))
    {
        /* put the message in the buffer */
        err = ERR_DBGINACT;
        errmsg(ctx->dbgcxerr, buf, buflen, err);

        /* return the error */
        return err;
    }

    /* 
     *   get the frame level - note that the UI stack level counter has
     *   level 0 as the innermost (current) level, which is the opposite
     *   of the way that dbgeval() counts levels, so we need to invert our
     *   counter to get the one to use in the dbgeval() call 
     */
    level = ctx->dbgcxfcn - 1 - level;

    /* make sure we have a valid level, in case we have no stack frames */
    if (level < 0)
    {
        err = ERR_INACTFR;
        errmsg(ctx->dbgcxerr, buf, buflen, err);
        return err;
    }

    /*
     *   While we're evaluating, set up to receive error logging messages
     *   -- we'll write them to our output buffer and note the errors.
     *   Certain expression evaluation errors result in logged (rather
     *   than signalled) errors, so we need to trap any logged errors.  
     */
    old_errcxlog = ctx->dbgcxerr->errcxlog;
    old_errcxlgc = ctx->dbgcxerr->errcxlgc;
    ctx->dbgcxerr->errcxlog = eval_expr_errlog;
    ctx->dbgcxerr->errcxlgc = &cbctx;

    /* evaluate the expression */
    err = dbgevalext(ctx, (char *)expr, eval_expr_cb, &cbctx, level, FALSE,
                     &dat, aggcb, aggctx, speculative);

    /*
     *   determine whether the type is openable, and inform the caller if
     *   desired 
     */
    if (is_openable != 0)
    {
        switch(dat)
        {
        case DAT_LIST:
        case DAT_OBJECT:
            /* these types are openable */
            *is_openable = TRUE;
            break;

        default:
            /* other types are not openable */
            *is_openable = FALSE;
            break;
        }
    }

    /* null-terminate the value buffer */
    *(cbctx.outp) = '\0';

    /* check for error */
    if (err != 0)
    {
        char msg[256];

        /* an error occurred - put its text into the buffer */
        errmsg(ctx->dbgcxerr, msg, sizeof(msg), err);
        errfmt(buf, buflen, msg, 0, 0);
    }
    else if (cbctx.err != 0)
    {
        /* a logged error occurred; it's already in the buffer */
        err = cbctx.err;
    }
    else
    {
        /*
         *   If the caller wants to know, check the expression to
         *   determine if it's an lvalue.  To do this, try parsing "(expr)
         *   := 0"; if the expression is an lvalue, the compiler will
         *   accept this, otherwise a parsing error will occur.  
         */
        if (is_lval != 0)
        {
            char asibuf[1024];
            objnum objn;

            /* 
             *   this parse will be speculative, so don't keep any error
             *   messages that get logged 
             */
            cbctx.log_errors = FALSE;

            /* build the assignment and evaluate it */
            sprintf(asibuf, "(%s):=0", expr);
            objn = MCMONINV;
            if (dbgcompile(ctx, asibuf, &ctx->dbgcxfrm[level], &objn,
                           speculative) != 0
                || cbctx.err != 0)
            {
                /* it's not an lvalue, so it can't be assigned into */
                *is_lval = FALSE;
            }
            else
            {
                /* it can be used as an lvalue */
                *is_lval = TRUE;
            }

            /*   
             *   free the compiled code - we didn't need it except for
             *   parsing it 
             */
            if (objn != MCMONINV)
                mcmfre(ctx->dbgcxmem, objn);
        }
    }

    /* if an error occurred, it's definitely not an lvalue */
    if (err != 0 && is_lval != 0)
        *is_lval = FALSE;

    /* restore error context */
    ctx->dbgcxerr->errcxlog = old_errcxlog ;
    ctx->dbgcxerr->errcxlgc = old_errcxlgc;

    /* return the result */
    return err;
}

/*
 *   Enumerate local variables in the current stack context.  
 */
void CHtmlDebugHelper::
   vm_enum_locals(dbgcxdef *ctx, int level,
                  void (*cbfunc)(void *, const textchar_t *, size_t),
                  void *cbctx)
{
    /* if we're not in the debugger, there's no local variable context */
    if (!(ctx->dbgcxflg & DBGCXFIND))
        return;

    /* 
     *   adjust the level to the format that the TADS 2 engine requires -
     *   for us, 0 is the current level; for the engine, the current level
     *   is (ctx->dbgcxfcn - 1) 
     */
    level = ctx->dbgcxfcn - 1 - level;

    /* ask the debugger to do the enumeration */
    dbgenumlcl(ctx, level, cbfunc, cbctx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a source file into a given window 
 */
int CHtmlDebugHelper::vm_load_file_into_win(CHtmlDbg_win_link *win_link,
                                            const textchar_t *fname)
{
    osfildef *fp;
    struct
    {
        linfdef linf;
        char fname[256];
    } linf;
    lindef *lin;
    uchar posbuf[4];
    ulong linenum;

    /* get the base class line source */
    lin = &linf.linf.linflin;

    /* open the file */
    fp = osfoprs(fname, OSFTTEXT);

    /* if we couldn't open the file, return failure */
    if (fp == 0)
        return 1;

    /* set up a line source to read the file */
    linfini2(0, &linf.linf, (char *)fname, get_strlen(fname), fp, TRUE);
    linf.linf.linflin.linid = win_link->source_id_;

    /* seek to the start of the line source */
    oswp4(posbuf, 0);
    linseek(lin, posbuf);

    /* make sure there's nothing on the page */
    win_link->parser_->clear_page();

    /* 
     *   read lines from the line source, and add each line to the
     *   formatter list for the window 
     */
    for (linenum = 0 ; ; )
    {
        textchar_t buf[1024];
        CHtmlTagText *txt_tag;
        textchar_t *p;

        /* get the next line */
        if (!lingets(lin, (uchar *)buf, sizeof(buf)))
            break;

        /* count the line */
        ++linenum;

        /* remove any newline characters at the end of the line */
        for (p = buf + get_strlen(buf) ; p > buf ; )
        {
            /* move to the previous character */
            --p;

            /* if it's not a \r or \n, we're done */
            if (*p != '\n' && *p != '\r')
                break;

            /* remove it and continue scanning */
            *p = '\0';
        }

        /* add a text tag for the line source */
        txt_tag =
            new CHtmlTagTextDebugsrc(linenum, tab_size_,
                                     win_link->parser_->get_text_array(),
                                     buf, get_strlen(buf));
        win_link->parser_->append_tag(txt_tag);
    }

    /* 
     *   Add an extra blank line at the end, to ensure that the final line
     *   gets formatted properly.  The formatter gets confused without a
     *   final empty line.  
     */
    win_link->parser_->append_tag(
        new CHtmlTagTextDebugsrc(linenum + 1, tab_size_,
                                 win_link->parser_->get_text_array(),
                                 "", 0));

    /* done with the source file - close it */
    osfcls(fp);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Toggle hidden output 
 */
void CHtmlDebugHelper::vm_toggle_dbg_hid()
{
    dbghid = !dbghid;
}

/*
 *   Get hidden output display status 
 */
int CHtmlDebugHelper::vm_get_dbg_hid()
{
    return dbghid;
}

/*
 *   Build a stack listing 
 */
void CHtmlDebugHelper::vm_build_stack_listing(dbgcxdef *ctx, void *cbctx)
{
    /* build the listing, invoking update_stack_disp_cb() for each line */
    dbgstktr(ctx, &update_stack_disp_cb, cbctx, 0, FALSE, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Turn profiling on 
 */
void CHtmlDebugHelper::start_profiling(dbgcxdef *)
{
    /* there's no profiler in TADS 2 - ignore this */
}

/*
 *   Turn profiling off 
 */
void CHtmlDebugHelper::end_profiling(dbgcxdef *)
{
    /* there's no profiler in TADS 2 - ignore this */
}

void CHtmlDebugHelper::get_profiling_data(
    struct dbgcxdef *,
    void (*)(void *, const char *, unsigned long,
             unsigned long, unsigned long),
    void *)
{
    /* we don't support profiling - ignore it */
}

