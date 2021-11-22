#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmldbg.cpp,v 1.4 1999/07/11 00:46:39 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmldbg.cpp - debugger helper class implementation
Function
  
Notes
  
Modified
  02/04/98 MJRoberts  - Creation
*/

#include <string.h>

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
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif

/*
 *   initialize 
 */
CHtmlDebugHelper::CHtmlDebugHelper(int need_srcfiles)
{
    /* remember whether or not we need to maintain the "srcfiles" list */
    need_srcfiles_ = need_srcfiles;

    /* there are no windows in the source window list yet */
    first_srcwin_ = last_srcwin_ = 0;

    /* no current or context line yet */
    cur_is_valid_ = FALSE;
    ctx_is_valid_ = FALSE;
    ctx_stackwin_disp_ = 0;

    /* no stack window yet */
    stack_win_ = 0;

    /* no debug log window yet */
    debuglog_win_ = 0;

    /* no history window yet */
    hist_win_ = 0;

    /* set default tab expansion size of 8 */
    tab_size_ = 8;

    /* no temporary breakpoint yet */
    tmp_bp_valid_ = FALSE;

    /* no breakpoints or source files yet */
    bp_ = 0;
    bp_tail_ = 0;
    src_ = 0;

    /* start out with the implicit reference for our caller */
    refcnt_ = 1;

    /* we haven't seen any compiled line source ID's yet */
    last_compiled_line_source_id_ = -1;
    next_line_source_id_ = 0;

    /* no lines yet */
    first_line_ = last_line_ = 0;
}

/*
 *   delete 
 */
CHtmlDebugHelper::~CHtmlDebugHelper()
{
    /* close any remaining source windows */
    while (first_srcwin_ != 0)
        first_srcwin_->win_->close_window(TRUE);

    /* delete any remaining special line links */
    delete_line_status(FALSE);

    /* delete internal breakpoint and source lists */
    delete_internal_bps();
    delete_internal_sources();
}

/*
 *   Delete all line links 
 */
void CHtmlDebugHelper::delete_line_status(int redraw)
{
    /* loop through all links, deleting each */
    while (first_line_ != 0)
    {
        CHtmlDbg_line_link *cur;

        /* remember this item */
        cur = first_line_;

        /* remove it from the list */
        first_line_ = first_line_->next_;
        if (first_line_ != 0)
            first_line_->prev_ = 0;

        /* if we're redrawing, update the line to show no status */
        if (redraw)
        {
            /* clear the link's flags */
            cur->stat_ = 0;
            
            /* update it on-screen */
            update_line_on_screen(cur);
        }
        
        /* delete the link */
        delete cur;
    }

    /* there's no more last line either */
    last_line_ = 0;
}

/*
 *   Delete internal breakpoint records 
 */
void CHtmlDebugHelper::delete_internal_bps()
{
    /* delete each breakpoint */
    while (bp_ != 0)
    {
        CHtmlDbg_bp *nxt;

        /* remember the next one */
        nxt = bp_->nxt_;

        /* delete this one */
        delete bp_;

        /* move on */
        bp_ = nxt;
    }

    /* clear the list tail pointer */
    bp_tail_ = 0;
}

/*
 *   delete internal source file records 
 */
void CHtmlDebugHelper::delete_internal_sources()
{
    while (src_ != 0)
    {
        CHtmlDbg_src *nxt;

        /* remember the next one */
        nxt = src_->nxt_;

        /* delete this one */
        delete src_;

        /* move on */
        src_ = nxt;
    }

    /* forget all compiled line source ID's */
    last_compiled_line_source_id_ = -1;
    next_line_source_id_ = 0;
}

/*
 *   initialize after loading 
 */
htmldbg_loadbp_stat CHtmlDebugHelper::
   init_after_load(dbgcxdef *ctx,
                   CHtmlDebugSysIfc_win *syswinifc,
                   CHtmlDebugConfig *config, int keep_bp_config)
{
    CHtmlDbg_win_link *link;
    htmldbg_loadbp_stat ret;

    /* presume success */
    ret = HTMLDBG_LBP_OK;

    /* we haven't seen any compiled line source ID's yet */
    last_compiled_line_source_id_ = -1;

    /* 
     *   if they want to keep the current breakpoint configuration, save
     *   the current breakpoint configuration so that we can reload it
     *   into the new TADS engine debug context 
     */
    if (keep_bp_config && config != 0)
        save_bp_config(config);

    /* perform engine-specific checks on the newly-loaded game */
    vm_check_loaded_program(ctx);

    /* 
     *   discard all of the old breakpoints - we'll reload them if the
     *   caller wanted to keep them, otherwise they're no longer relevant 
     */
    delete_internal_bps();

    /* 
     *   delete the old line status markers - we'll refresh the display
     *   with a current set when we finish loading 
     */
    delete_line_status(TRUE);

    /* delete the old line source trackers */
    delete_internal_sources();

    /* 
     *   Load the new line sources - if we loaded a program (in which case
     *   we'll have a non-null engine context), load sources from the
     *   program; otherwise load from the saved configuration.  Note that we
     *   don't bother loading from the saved configuration if we don't need a
     *   separate source files list.  
     */
    if (ctx != 0)
    {
        /* read line sources from the compiled game */
        vm_load_sources_from_program(ctx);

        /* 
         *   we now know the highest line source ID in the compiled file;
         *   start assigning ID's to external files after this value 
         */
        next_line_source_id_ = last_compiled_line_source_id_ + 1;
    }
    else if (need_srcfiles_)
    {
        int i;
        
        /* 
         *   We don't have a compiled game available, so read line sources
         *   from the saved configuration. 
         */
        i = config->get_strlist_cnt("srcfiles", "fname");
        while (i > 0)
        {
            textchar_t fname[OSFNMAX];
            textchar_t path[OSFNMAX];

            /* move on to the next one */
            --i;

            /* 
             *   Read this filename, and create the line source.  We saved
             *   each line source in the string array at the index value
             *   equal to its source ID, so the source ID is simply the
             *   current index value ('i').  
             */ 
            if (!config->getval("srcfiles", "fname", i, fname, sizeof(fname))
                && !config->getval("srcfiles", "path", i, path, sizeof(path)))
                add_internal_line_source(fname, path, i);
        }
    }

    /*
     *   Go through all of the windows and figure out which ones now
     *   contain files that are in our source file list.  For each one
     *   that's in the list, set its source ID.  Even if the game was
     *   previously loaded, the source ID's can change on a recompile, so
     *   we need to fix up all of the ID's each time we reload.  
     */
    for (link = first_srcwin_ ; link != 0 ; link = link = link->next_)
    {
        CHtmlDbg_src *src;

        /* 
         *   if this window isn't tied to a line source, don't bother
         *   trying to tie it to a new line source 
         */
        if (link->source_id_ == -1)
            continue;

        /* see if there's a source file matching this window's file */
        src = find_internal_src(syswinifc, link->fname_.get(),
                                link->path_.get(), TRUE);

        /* 
         *   if it matches, set the source ID to the matching source
         *   file's ID; otherwise, set the source ID to invalid, since
         *   this window contains some random source file that isn't part
         *   of the source code for this game 
         */
        link->source_id_ = (src == 0 ? -1 : src->source_id_);
    }

    /* if they provided a configuration, load breakpoints */
    if (config != 0)
        ret = load_bp_config(ctx, syswinifc, config);

    /* return the result */
    return ret;
}

/*
 *   clear the internal line source list from a configuration object 
 */
void CHtmlDebugHelper::clear_internal_source_config(CHtmlDebugConfig *config)
{
    /* clear out our string lists */
    config->clear_strlist("srcfiles", "fname");
    config->clear_strlist("srcfiles", "path");
}

/*
 *   save the internal line source list 
 */
void CHtmlDebugHelper::save_internal_source_config(CHtmlDebugConfig *config)
{
    CHtmlDbg_src *src;
    
    /* clear out any existing source file list */
    clear_internal_source_config(config);

    /* if we don't need to save the "srcfiles" list, don't bother */
    if (!need_srcfiles_)
        return;

    /* loop through our internal line sources, saving each one */
    for (src = src_ ; src != 0 ; src = src->nxt_)
    {
        /* save this item; the line source ID is the string list index */
        config->setval("srcfiles", "fname", src->source_id_,
                       src->fname_.get());
        config->setval("srcfiles", "path", src->source_id_,
                       src->path_.get() != 0 ? src->path_.get() : "");
    }
}

/*
 *   add a line source to our internal list
 */
CHtmlDbg_src *CHtmlDebugHelper::
   add_internal_line_source(const textchar_t *fname,
                            const textchar_t *path, int line_source_id)
{
    CHtmlDbg_src *src;

    /* create the new line source object */
    src = new CHtmlDbg_src(fname, path, line_source_id);

    /* 
     *   if this has an assigned ID greater than or equal to the next ID
     *   that we would normally assign a new fake internal line source,
     *   increase our internal assignment counter to ensure we don't
     *   reassign the same ID later
     */
    if (line_source_id >= next_line_source_id_)
        next_line_source_id_ = line_source_id + 1;
    
    /* link it into our list */
    src->nxt_ = src_;
    src_ = src;

    /* return the new line source */
    return src;
}

/*
 *   add an internal line source that doesn't correspond to an actual file
 *   in the compiled game 
 */
CHtmlDbg_src *CHtmlDebugHelper::
   add_fake_internal_line_source(const textchar_t *fname)
{
    int id;
    
    /* assign a new line source ID */
    id = next_line_source_id_++;
    
    /* add the line source */
    return add_internal_line_source(fname, fname, id);
}


/* 
 *   set execution state to GO 
 */
void CHtmlDebugHelper::set_exec_state_go(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* set the execution state in the engine */
    vm_set_exec_state_go(ctx);

    /* remove the old current line pointer while executing */
    forget_current_line();
}

/* 
 *   set execution state to STEP OVER - steps without following calls 
 */
void CHtmlDebugHelper::set_exec_state_step_over(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* set the state in the engine */
    vm_set_exec_state_step_over(ctx);

    /* remove the old current line pointer while exeucting */
    forget_current_line();
}

/*
 *   set execution state to STEP OUT - steps until the current function
 *   has returned 
 */
void CHtmlDebugHelper::set_exec_state_step_out(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* set the state in the engine */
    vm_set_exec_state_step_out(ctx);

    /* remove the old current line pointer while exeucting */
    forget_current_line();
}

/* 
 *   set execution state to STEP INTO - steps and follows calls 
 */
void CHtmlDebugHelper::set_exec_state_step_into(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* set engine state */
    vm_set_exec_state_step_into(ctx);
    
    /* remove the old current line pointer while exeucting */
    forget_current_line();
}

/*
 *   Signal a QUIT condition 
 */
void CHtmlDebugHelper::signal_quit(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* tell the engine about it */
    vm_signal_quit(ctx);
}

/*
 *   Signal a RESTART condition 
 */
void CHtmlDebugHelper::signal_restart(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* signal the change in the engine */
    vm_signal_restart(ctx);
}

/*
 *   Signal an ABORT condition 
 */
void CHtmlDebugHelper::signal_abort(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* signal the chnage in the engine */
    vm_signal_abort(ctx);
}

/*
 *   Move execution to the current selection in the given window.  Returns
 *   zero on success, non-zero on error.  
 */
int CHtmlDebugHelper::set_next_statement(dbgcxdef *ctx,
                                         unsigned int *exec_ofs,
                                         CHtmlSysWin *win,
                                         int *need_single_step)
{
    CHtmlDbg_win_link *win_link;
    CHtmlDbg_line_link *line_link;
    unsigned long linenum;

    /* if there's no context, we can't do anything */
    if (ctx == 0)
        return 1;

    /* make sure it's valid */
    if (!vm_is_in_same_fn(ctx, win))
        return 1;

    /* get information on the current source file position */
    linenum = find_line_info(win, &win_link, &line_link);
    if (linenum == 0)
        return 2;

    /* move the execution position in the engine */
    if (vm_set_next_statement(ctx, exec_ofs, win, &linenum, need_single_step))
        return 3;

    /* forget the old current line */
    forget_current_line();

    /* set the new current line */
    set_current_line(win_link->source_id_, linenum);

    /* success */
    return 0;
}


/*
 *   get the current call trace status 
 */
int CHtmlDebugHelper::is_call_trace_active(dbgcxdef *ctx) const
{
    return (ctx == 0 ? FALSE : vm_is_call_trace_active(ctx));
}

/*
 *   turn call trace on or off 
 */
void CHtmlDebugHelper::set_call_trace_active(dbgcxdef *ctx, int flag)
{
    /* if there's no context, we can't do anything here */
    if (ctx == 0)
        return;

    /* set the status in the underlying engine */
    vm_set_call_trace_active(ctx, flag);
}

/*
 *   clear the call trace log 
 */
void CHtmlDebugHelper::clear_call_trace_log(dbgcxdef *ctx)
{
    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* clear the trace log in the engine */
    vm_clear_call_trace_log(ctx);

    /* make sure we reflect this in the display window, if open */
    update_hist_window(ctx);
}

/*
 *   get a pointer to the history log buffer 
 */
const textchar_t *CHtmlDebugHelper::get_call_trace_buf(dbgcxdef *ctx) const
{
    return ctx == 0 ? 0 : vm_get_call_trace_buf(ctx);
}

/*
 *   get the size of the data accumulated in the history log 
 */
unsigned long CHtmlDebugHelper::get_call_trace_len(dbgcxdef *ctx) const
{
    return ctx == 0 ? 0 : vm_get_call_trace_len(ctx);
}

/*
 *   Set or clear a breakpoint at the current selection in the given
 *   window 
 */
void CHtmlDebugHelper::toggle_breakpoint(dbgcxdef *ctx,
                                         CHtmlSysWin *win)
{
    /* toggle the breakpoint in the VM */
    vm_toggle_breakpoint(ctx, win);
}

/*
 *   Set a temporary breakpoint at the current selection in the given
 *   window.  If there's already a breakpoint at this location, we won't
 *   add a new breakpoint, and we'll return false.  Otherwise, we'll set a
 *   new breakpoint and return true.
 *   
 *   The purpose of a temporary breakpoint is to allow "run to
 *   cursor"-type operations, where the debugger must secretly set its own
 *   internal breakpoint to facilitate the operation but where the user
 *   doesn't intend to add a new breakpoint to the persistent breakpoint
 *   list.  Because the breakpoint is temporary, we won't bother giving it
 *   a meaningful name for the display list, and we also won't show the
 *   breakpoint in the line status displayed in the source window; this
 *   breakpoint should thus be removed the next time the debugger is
 *   entered.
 *   
 *   Only one temporary breakpoint is allowed at a time.  If there's
 *   already a temporary breakpoint set, we'll remove the old one before
 *   adding the new one.  
 */
int CHtmlDebugHelper::set_temp_bp(dbgcxdef *ctx, CHtmlSysWin *win)
{
    int err;
    
    /* if there's no context, ignore it */
    if (ctx == 0)
        return FALSE;

    /* if we already have a temporary breakpoint, clear it */
    clear_temp_bp(ctx);
    
    /* set the breakpoint in the VM */
    err = vm_set_temp_bp(ctx, win);

    /* if we were successful, note that we now have a temporary breakpoint */
    tmp_bp_valid_ = TRUE;

    /* return status */
    return err;
}

/*
 *   Clear the temporary breakpoint, if there is one.  If no temporary
 *   breakpoint is currently set, this will have no effect. 
 */
void CHtmlDebugHelper::clear_temp_bp(dbgcxdef *ctx)
{
    /* if we don't have a temporary breakpoint, there's nothing to do */
    if (!tmp_bp_valid_)
        return;

    /* clear the breakpoint in the VM */
    vm_clear_temp_bp(ctx);

    /* forget the breakpoint */
    tmp_bp_valid_ = FALSE;
}

/*
 *   Add a new global breakpoint.  Returns a non-zero error code if the
 *   condition is invalid or any other error occurs; returns zero on
 *   success, and fills in *bpnum with the identifier of the new
 *   breakpoint.  
 */
int CHtmlDebugHelper::set_global_breakpoint(dbgcxdef *ctx,
                                            const textchar_t *cond,
                                            int change,
                                            int *bpnum,
                                            char *errbuf, size_t errbuflen)
{
    /* set the global breakpoint in the engine */
    return vm_set_global_breakpoint(ctx, cond, change, bpnum,
                                    errbuf, errbuflen);
}

/*
 *   Enable/disable a breakpoint at the current selection point in the
 *   given window.  Reverses current enabling state.  
 */
void CHtmlDebugHelper::toggle_bp_disable(dbgcxdef *ctx,
                                         CHtmlSysWin *win)
{
    /* ask the VM to do the work */
    vm_toggle_bp_disable(ctx, win);
}

/*
 *   Delete a breakpoint given a breakpoint number 
 */
int CHtmlDebugHelper::delete_breakpoint(dbgcxdef *ctx, int bpnum)
{
    /* delete the breakpoint in the VM */
    return vm_delete_breakpoint(ctx, bpnum);
}

/*
 *   Delete all breakpoints 
 */
void CHtmlDebugHelper::delete_all_bps(dbgcxdef *ctx)
{
    CHtmlDbg_bp *nxt;
    
    /* loop through our breakpoint trackers */
    while (bp_ != 0)
    {
        /* remember the next one, since we're going to delete this one */
        nxt = bp_->nxt_;

        /* delete this one */
        delete_breakpoint(ctx, bp_->bpnum_);

        /* move on */
        bp_ = nxt;
    }

    /* clear the tail pointer */
    bp_tail_ = 0;
}

/*
 *   Enable or disable a breakpoint, given a breakpoint number 
 */
int CHtmlDebugHelper::enable_breakpoint(dbgcxdef *ctx, int bpnum, int enable)
{
    int err;
    CHtmlDbg_bp *bp;

    /* presume no error */
    err = 0;

    /* find the tracker */
    bp = find_internal_bp(bpnum);

    /* if there's no tracker, it's an error */
    if (bp == 0)
        return 1;

    /* if TADS is running, tell TADS to enable or disable the breakpoint */
    if (ctx != 0 && (err = vm_enable_breakpoint(ctx, bpnum, enable)))
        return err;

    /* update our tracker */
    bp->enabled_ = enable;

    /* update the line status if appropriate */
    if (!bp->global_)
        change_line_status(bp->source_id_, bp->linenum_,
                           HTMLTDB_STAT_BP_DIS, !enable);

    /* return the result */
    return err;
}

/*
 *   Determine if a given breakpoint is associated with code.  Returns
 *   true if so, false if not. 
 */
int CHtmlDebugHelper::is_bp_at_line(int bpnum)
{
    CHtmlDbg_bp *bp;

    /* find the tracker */
    bp = find_internal_bp(bpnum);

    /* if there's no tracker, it's not at a line */
    if (bp == 0)
        return FALSE;

    /* it's at a line if it's not global */
    return !bp->global_;
}

/*
 *   Go to the code associated with a given breakpoint.  Simply shows the
 *   code; doesn't establish the code's stack context.  Returns the window
 *   associated with the code; returns null if the breakpoint isn't
 *   associated with code.  
 */
CHtmlSysWin *CHtmlDebugHelper::show_bp_code(int bpnum,
                                            CHtmlDebugSysIfc_win *syswinifc)
{
    CHtmlDbg_src *src;
    CHtmlDbg_win_link *link;
    int source_id;
    unsigned long linenum;
    
    /* 
     *   get the line information associated with the breakpoint; if there
     *   isn't any line information available, return failure 
     */
    if (!get_bp_line_info(bpnum, &source_id, &linenum))
        return 0;

    /* get the source file */
    src = find_internal_src(source_id);

    /* if we didn't find the line source, give up */
    if (src == 0)
        return 0;

    /* load or find the source window for this line source */
    link = load_source_file(syswinifc, src);
    if (link == 0)
        return 0;

    /* go to the source line */
    show_source_line(link, linenum);

    /* return the window we found */
    return link->win_;
}

/*
 *   Go to a given line in a source file 
 */
void CHtmlDebugHelper::show_source_line(CHtmlDbg_win_link *link,
                                        unsigned long linenum)
{
    CHtmlDisp *disp;

    /* find the line at this position */
    disp = link->formatter_->find_by_debugsrcpos(linenum);

    /* scroll the source window to show this location */
    if (disp != 0)
    {
        CHtmlRect pos;

        /* get the position of this line, and scroll it into view */
        pos = disp->get_pos();
        link->win_->scroll_to_doc_coords(&pos);

        /* put the selection at the start of the line */
        link->formatter_->set_sel_range(disp->get_text_ofs(),
                                        disp->get_text_ofs());
    }
}


/*
 *   Enumerate breakpoints in a displayable format 
 */
void CHtmlDebugHelper::
   enum_breakpoints(dbgcxdef *ctx,
                    void (*cbfunc)(void *cbctx, int bpnum,
                                   const textchar_t *desc, int disabled),
                    void *cbctx)
{
    CHtmlDbg_bp *bp;
    CHtmlDbg_bp *nxt;
    
    /* loop over the breakpoints */
    for (bp = bp_ ; bp != 0 ; bp = nxt)
    {
        textchar_t desc[HTMLDBG_MAXBPDESC];

        /* 
         *   TADS can change the enabled status of global breakpoints
         *   without consulting us.  If TADS is running, always update the
         *   status of our breakpoints from the engine.  
         */
        if (ctx != 0)
            bp->enabled_ = vm_is_bp_enabled(ctx, bp->bpnum_);
        
        /* 
         *   remember the next one now, before invoking the callback, in
         *   case the callback deletes the current breakpoint (in which
         *   case we will no longer be able to access the structure after
         *   the callback returns) 
         */
        nxt = bp->nxt_;

        /* build the description */
        format_bp_for_display(bp, desc, sizeof(desc));

        /* invoke the callback */
        (*cbfunc)(cbctx, bp->bpnum_, desc, !bp->enabled_);
    }
}

/*
 *   Format a breakpoint description from our internal format to a format
 *   suitable for display to the user.  
 */
void CHtmlDebugHelper::format_bp_for_display(const CHtmlDbg_bp *bp,
                                             textchar_t *buf,
                                             size_t /*buflen*/) const
{
    /* check the breakpoint type */
    if (bp->global_)
    {
        /* it's a global breakpoint - format as "when <condition>" */
        sprintf(buf, "when %s %s%s", bp->cond_.get(),
                bp->stop_on_change_ ? "changes" : "is true",
                bp->enabled_ ? "" : " [disabled]");
    }
    else
    {
        CHtmlDbg_src *src;
        const textchar_t *cond;
        int has_cond;
        
        /* it's at a source line - find the source tracker */
        src = find_internal_src(bp->source_id_);

        /* get the condition string */
        cond = bp->cond_.get();
        has_cond = (cond != 0 && cond[0] != '\0');
        
        /* format it as "at '<filename>' line <linenum> when <condition>" */
        sprintf(buf, "at '%s' line %ld%s%s%s%s",
                (src != 0 ? src->fname_.get() : "<unknown source file>"),
                bp->linenum_,
                has_cond ? " when " : "",
                has_cond ? cond : "",
                has_cond ? (bp->stop_on_change_ ? " changes" : " is true")
                         : "",
                bp->enabled_ ? "" : " [disabled]");
    }
}


/*
 *   Get a breakpoint's description in a format suitable for display.
 *   Returns zero on success, non-zero on failure.  
 */
int CHtmlDebugHelper::get_bp_desc(int bpnum,
                                  textchar_t *buf, size_t buflen)
{
    CHtmlDbg_bp *bp;

    /* find the breakpoint */
    bp = find_internal_bp(bpnum);

    /* if we didn't find it, return failure */
    if (bp == 0)
        return 1;

    /* format the text into the caller's buffer */
    format_bp_for_display(bp, buf, buflen);

    /* success */
    return 0;
}

/*
 *   Determine if a breakpoint is enabled or disabled.  Returns true if
 *   the breakpoint is enabled, false if it's disabled.  
 */
int CHtmlDebugHelper::is_bp_enabled(int bpnum)
{
    CHtmlDbg_bp *bp;

    /* find the breakpoint and return its status */
    bp = find_internal_bp(bpnum);
    return (bp == 0 ? FALSE : bp->enabled_);
}

/*
 *   Get a breakpoint's condition.  Returns zero on success, non-zero on
 *   failure.  If this is a valid breakpoint without a condition, we'll
 *   return success and an empty condbuf string.  
 */
int CHtmlDebugHelper::get_bp_cond(int bpnum,
                                  textchar_t *condbuf, size_t condbuflen,
                                  int *stop_on_change)
{
    CHtmlDbg_bp *bp;
    size_t len;

    /* find the breakpoint */
    bp = find_internal_bp(bpnum);
    if (bp == 0)
        return 1;

    /* if the condition doesn't fit, return failure */
    len = (bp->cond_.get() != 0 ? get_strlen(bp->cond_.get()) : 0);

    /* if it doesn't fit, return an error */
    if (len + 1 > condbuflen)
        return 2;

    /* copy the condition */
    if (len != 0)
        do_strcpy(condbuf, bp->cond_.get());
    else
        condbuf[0] = '\0';

    /* set the stop-on-change flag */
    *stop_on_change = bp->stop_on_change_;

    /* success */
    return 0;
}

/*
 *   Change a breakpoint's condition 
 */
int CHtmlDebugHelper::change_bp_cond(dbgcxdef *ctx, int bpnum,
                                     const textchar_t *cond, int change,
                                     char *errbuf, size_t errbuflen)
{
    CHtmlDbg_bp *bp;
    int err;
    
    /* if TADS is running, update its condition text */
    if (ctx != 0
        && (err = vm_set_bp_condition(ctx, bpnum, cond, change,
                                      errbuf, errbuflen)) != 0)
        return err;

    /* update our internal tracker's condition string */
    bp = find_internal_bp(bpnum);
    if (bp != 0)
    {
        bp->cond_.set(cond);
        bp->stop_on_change_ = change;
    }

    /* success */
    return 0;
}

/*
 *   Clear the breakpoint configuration 
 */
void CHtmlDebugHelper::clear_bp_config(CHtmlDebugConfig *config) const
{
    /* delete any existing configuration information for the breakpoints */
    config->clear_strlist("breakpoints", "global");
    config->clear_strlist("breakpoints", "file");
    config->clear_strlist("breakpoints", "line");
    config->clear_strlist("breakpoints", "status");
    config->clear_strlist("breakpoints", "cond");
    config->clear_strlist("breakpoints", "mode");
}

/*
 *   Save breakpoint configuration 
 */
void CHtmlDebugHelper::save_bp_config(CHtmlDebugConfig *config) const
{
    CHtmlDbg_bp *bp;
    int i;

    /* clear any existing configuration */
    clear_bp_config(config);

    /* loop over breakpoints */
    for (i = 0, bp = bp_ ; bp != 0 ; bp = bp->nxt_, ++i)
    {
        char buf[20];
        CHtmlDbg_src *src;

        /* if this is a source breakpoint, get the line source */
        src = (bp->global_ ? 0 : find_internal_src(bp->source_id_));

        /* save this breakpoint's global/local status */
        config->setval("breakpoints", "global", i,
                       bp->global_ ? "g" : "l");
        
        /* save the filename */
        config->setval("breakpoints", "file", i,
                       src != 0 ? src->fname_.get() : "");

        /* save the line number */
        sprintf(buf, "%lu", bp->linenum_);
        config->setval("breakpoints", "line", i, buf);

        /* save it's enabled/disabled status */
        config->setval("breakpoints", "status", i,
                       bp->enabled_ ? "e" : "d");
        
        /* if there's a condition, save it */
        config->setval("breakpoints", "cond", i,
                       bp->cond_.get() != 0 ? bp->cond_.get() : "");

        /* set the condition mode (stop-on-change or stop-on-true) */
        config->setval("breakpoints", "cond-mode", i,
                       bp->stop_on_change_ ? "change" : "true");
    }
}

/*
 *   Load breakpoint configuration 
 */
htmldbg_loadbp_stat CHtmlDebugHelper::
   load_bp_config(dbgcxdef *dbgctx,
                  CHtmlDebugSysIfc_win *syswinifc,
                  CHtmlDebugConfig *config)
{
    int cnt;
    int i;
    int not_set_cnt;
    int moved_cnt;

    /* we haven't failed to set any yet */
    not_set_cnt = 0;

    /* none of the breakpoints have moved yet */
    moved_cnt = 0;
    
    /* run through the breakpoints in the configuration information */
    cnt = config->get_strlist_cnt("breakpoints", "global");
    for (i = 0 ; i < cnt ; ++i)
    {
        textchar_t mode[10];
        textchar_t glob[5];
        textchar_t fname[HTMLDBG_MAXBPNAME];
        textchar_t linebuf[20];
        textchar_t stat[5];
        textchar_t cond[HTMLDBG_MAXCOND];

        /* get the stop-on-change setting */
        if (config->getval("breakpoints", "cond-mode", i, mode, sizeof(mode)))
            mode[0] = 't';

        /* load the rest of this breakpoint's information */
        if (config->getval("breakpoints", "global", i, glob, sizeof(glob))
            || config->getval("breakpoints", "file", i, fname, sizeof(fname))
            || config->getval("breakpoints", "line",
                              i, linebuf, sizeof(linebuf))
            || config->getval("breakpoints", "status", i, stat, sizeof(stat))
            || config->getval("breakpoints", "cond", i, cond, sizeof(cond)))
        {
            ++not_set_cnt;
            continue;
        }

        /* create the appropriate type of breakpoint */
        if (glob[0] == 'l')
        {
            unsigned long linenum;
            unsigned long orig_linenum;
            CHtmlDbg_src *src;
            
            /* it's a breakpoint at a source line - get the line number */
            orig_linenum = linenum = (unsigned long)atol(linebuf);

            /* find the source file */
            src = find_internal_src(syswinifc, fname, 0, FALSE);
            if (src == 0)
            {
                /* 
                 *   if we have a debugger context, note that this one
                 *   can't actually be set; if not, the game isn't yet
                 *   running, so we're not actually trying to set any
                 *   breakpoints, hence we can't fail to set any
                 *   breakpoints 
                 */
                if (dbgctx != 0)
                    ++not_set_cnt;

                /* create an internal line source for the file */
                src = add_fake_internal_line_source(fname);
            }

            /* if TADS is running, set the breakpoint in the engine */
            if (dbgctx != 0)
            {
                /* set the breakpoint in the engine */
                if (vm_set_loaded_bp(dbgctx, src->source_id_,
                                     orig_linenum, &linenum,
                                     cond, mode[0] == 'c',
                                     stat[0] == 'd'))
                {
                    /* unsuccessful - count the failure */
                    ++not_set_cnt;

                    /* skip the rest of the processing for this breakpoint */
                    continue;
                }

                /* 
                 *   if the line location changed (which can happen if the
                 *   game was recompiled since the configuration was last
                 *   saved, since some code could have moved around in the
                 *   source file), note it 
                 */
                if (linenum != orig_linenum)
                    ++moved_cnt;
            }
            else
            {
                int bpnum;

                /* synthesize an internal breakpoint ID */
                bpnum = synthesize_bp_num();
                
                /* 
                 *   the engine isn't running - just set the internal
                 *   breakpoint record 
                 */
                toggle_internal_bp(dbgctx, src->source_id_, orig_linenum,
                                   cond, mode[0] == 'c', bpnum, TRUE, FALSE);

                /* disable it if necessary */
                if (stat[0] == 'd')
                    enable_breakpoint(dbgctx, bpnum, FALSE);
            }
        }
        else
        {
            int bpnum;
            char errbuf[256];
            
            /* it's a global breakpoint - set it */
            if (set_global_breakpoint(dbgctx, cond, mode[0] == 'c',
                                      &bpnum, errbuf, sizeof(errbuf)) == 0)
            {
                /* disable the breakpoint if appropriate */
                if (stat[0] == 'd')
                    enable_breakpoint(dbgctx, bpnum, FALSE);
            }
        }
    }

    /* return the result */
    if (not_set_cnt != 0)
        return HTMLDBG_LBP_NOT_SET;
    else if (moved_cnt != 0)
        return HTMLDBG_LBP_MOVED;
    else
        return HTMLDBG_LBP_OK;
}

/*
 *   Get the text of an error message.  This routine doesn't require an
 *   error stack, so it's suitable for retrieving the text of error
 *   messages returned from the debugger API. 
 */
void CHtmlDebugHelper::get_error_text(struct dbgcxdef *ctx, int err,
                                      textchar_t *buf, size_t buflen)
{
    /* if there's no context, we can't do anything */
    if (ctx == 0)
    {
        buf[0] = '\0';
        return;
    }
    
    /* get the text of the message from the VM */
    vm_get_error_text(ctx, err, buf, buflen);
}

/*
 *   Get the text of an error message, formatting the message with
 *   arguments from the error stack.  This should only be used when a
 *   valid error stack is present, such as when entering the debugger due
 *   to a run-time error.  
 */
void CHtmlDebugHelper::format_error(struct dbgcxdef *ctx, int err,
                                    textchar_t *buf, size_t buflen)
{
    /* get the text from the engine */
    vm_format_error(ctx, err, buf, buflen);
}

/*
 *   Evaluate an expression 
 */
int CHtmlDebugHelper::eval_expr(struct dbgcxdef *ctx,
                                textchar_t *buf, size_t buflen,
                                const textchar_t *expr,
                                int *is_lval, int *is_openable,
                                void (*aggcb)(void *, const char *, int,
                                              const char *),
                                void *aggctx, int speculative)
{
    int level;
    
    /* 
     *   get the stack level at which to evaluate the expression - use the
     *   current context level from the stack window 
     */
    level = (ctx_stackwin_disp_ != 0
             ? ctx_stackwin_disp_->get_debugsrcpos()
             : 0);
    
    /* let the engine do the work */
    return vm_eval_expr(ctx, buf, buflen, expr, level,
                        is_lval, is_openable, aggcb, aggctx, speculative);
}

/*
 *   Evaluate an assignment expression
 */
int CHtmlDebugHelper::eval_asi_expr(dbgcxdef *ctx, const textchar_t *lvalue,
                                    const textchar_t *rvalue)
{
    int level;

    /* 
     *   get the stack level at which to evaluate the expression - use the
     *   current context level from the stack window 
     */
    level = (ctx_stackwin_disp_ != 0
             ? ctx_stackwin_disp_->get_debugsrcpos()
             : 0);
    
    /* let the VM handle it */
    return vm_eval_asi_expr(ctx, level, lvalue, rvalue);
}

/*
 *   Enumerate local variables in the current stack context.  
 */
void CHtmlDebugHelper::
   enum_locals(dbgcxdef *ctx,
               void (*cbfunc)(void *, const textchar_t *, size_t),
               void *cbctx)
{
    int level;

    /* if the debugger isn't running, there are no locals */
    if (ctx == 0)
        return;

    /* get the frame level - 0 is the currently active method */
    level = (ctx_stackwin_disp_ != 0
             ? ctx_stackwin_disp_->get_debugsrcpos()
             : 0);
    
    /* let the VM do the work */
    vm_enum_locals(ctx, level, cbfunc, cbctx);
}

/*
 *   Set the current line 
 */
void CHtmlDebugHelper::set_current_line(int source_id, unsigned long linenum)
{
    /* forget any previous current line */
    forget_current_line();
    
    /* remember the new line location */
    cur_source_id_ = source_id;
    cur_linenum_ = linenum;
    
    /* the current line record is now valid */
    cur_is_valid_ = TRUE;

    /* update the visual status */
    set_line_status(cur_source_id_, cur_linenum_, HTMLTDB_STAT_CURLINE);
}

/*
 *   Set the current context line 
 */
void CHtmlDebugHelper::set_current_ctx_line(int source_id,
                                            unsigned long linenum)
{
    /* forget any old context line */
    forget_ctx_line();

    /* remember the new context line */
    ctx_source_id_ = source_id;
    ctx_linenum_ = linenum;

    /* the current context line record is now valid */
    ctx_is_valid_ = TRUE;

    /* update the visual status */
    set_line_status(ctx_source_id_, ctx_linenum_, HTMLTDB_STAT_CTXLINE);
}

/*
 *   Forget the current line.  This should be called just prior to
 *   resuming execution to ensure that the current line doesn't hang
 *   around while the game is executing for an extended period.  
 */
void CHtmlDebugHelper::forget_current_line()
{
    /* forget the current line if we have one */
    if (cur_is_valid_)
    {
        /* drop its current-line status flag */
        clear_line_status(cur_source_id_, cur_linenum_, HTMLTDB_STAT_CURLINE);

        /* forget we have a current line */
        cur_is_valid_ = FALSE;
    }

    /* if we have a context line, drop it as well */
    forget_ctx_line();
}

/*
 *   forget the context line 
 */
void CHtmlDebugHelper::forget_ctx_line()
{
    if (ctx_is_valid_)
    {
        /* drop the context-line status flag from the source line */
        clear_line_status(ctx_source_id_, ctx_linenum_, HTMLTDB_STAT_CTXLINE);

        /* forget about it */
        ctx_is_valid_ = FALSE;
    }

    /* clear the status flag in the stack window */
    if (ctx_stackwin_disp_ != 0 && stack_win_ != 0)
    {
        /* remove the flag */
        ctx_stackwin_disp_->set_dbg_status(stack_win_->win_, 0);
        
        /* forget the display item */
        ctx_stackwin_disp_ = 0;
    }
}

/*
 *   Update the debugger for a change in the execution status.  This
 *   should be called whenever we re-enter the debugger after executing
 *   game code.  We'll show the current line, reset the evaluation
 *   context, and update the stack window.  
 */
CHtmlSysWin *CHtmlDebugHelper::
   update_for_entry(dbgcxdef *ctx, CHtmlDebugSysIfc_win *syswinifc)
{
    CHtmlSysWin *win;

    /* if we have no context, there's nothing to do */
    if (ctx == 0)
        return 0;

    /* load the current source line */
    win = load_current_source(ctx, syswinifc);

    /* update the stack window */
    update_stack_window(ctx, TRUE);

    /* update the call history window, if we're keeping call history */
    if (vm_is_call_trace_active(ctx))
        update_hist_window(ctx);

    /* return the current source line window */
    return win;
}

/*
 *   Load the source file at a particular stack level.  Opens the window,
 *   scrolls to the current line, and sets the appropriate line status at
 *   that line.  
 */
CHtmlSysWin *CHtmlDebugHelper::
   load_source_at_level(dbgcxdef *ctx, CHtmlDebugSysIfc_win *syswinifc,
                        int level)
{
    unsigned long linenum;
    CHtmlDbg_win_link *link;
    CHtmlDbg_src *src;

    /* if we previously had a current line showing on-screen, remove it */
    if (level == 0)
    {
        /* forget the current and context lines */
        forget_current_line();
    }
    else
    {
        /* forget only the old context line */
        forget_ctx_line();
    }

    /* if we have no context, we can't do anything here */
    if (ctx == 0)
        return 0;

    /* get information on the current line at the given stack level */
    if (vm_get_source_info_at_level(ctx, &src, &linenum, level, syswinifc))
        return 0;

    /* load or find the source window for this line source */
    link = load_source_file(syswinifc, src);

    /* go to the current line if we got a window */
    if (link != 0 && link->win_ != 0)
        show_source_line(link, linenum);

    /* set the current or the context line, as appropriate */
    if (level == 0)
    {
        /* set the new current line */
        set_current_line(src->source_id_, linenum);
    }
    else
    {
        /* set the new context line */
        set_current_ctx_line(src->source_id_, linenum);
    }

    /* return the source window we found */
    return link != 0 ? link->win_ : 0;
}

/*
 *   Enumerate open windows 
 */
void CHtmlDebugHelper::
   enum_source_windows(void (*func)(void *ctx, int idx,
                                    class CHtmlSysWin *win,
                                    int line_source_id,
                                    HtmlDbg_wintype_t win_type),
                       void *cbctx)
{
    CHtmlDbg_win_link *link;
    int idx;
    CHtmlDbg_win_link *next_link;

    /* go through the list of windows */
    for (link = first_srcwin_, idx = 0 ; link != 0 ; link = next_link, ++idx)
    {
        /* 
         *   remember the next window in the list - the callback could
         *   destroy this window before it returns, in which case we'd
         *   lose the window structure 
         */
        next_link = link->next_;
        
        /* invoke the callback for this window */
        (*func)(cbctx, idx, link->win_, link->source_id_, link->win_type_);
    }
}

/*
 *   Get a source window by index 
 */
CHtmlSysWin *CHtmlDebugHelper::get_source_window_by_index(int idx) const
{
    CHtmlDbg_win_link *link;

    /* go through the list of windows */
    for (link = first_srcwin_ ; link != 0 && idx != 0 ;
         link = link->next_, --idx) ;

    /* return the window we found */
    return (link == 0 ? 0 : link->win_);
}

/*
 *   Open a window on a line source, given the line source ID 
 */
CHtmlSysWin *CHtmlDebugHelper::
   open_line_source(CHtmlDebugSysIfc_win *syswinifc,
                    int line_source_id)
{
    CHtmlDbg_src *src;
    CHtmlDbg_win_link *link;

    /* find the line source */
    src = find_internal_src(line_source_id);

    /* if we didn't find it, we can't open a window */
    if (src == 0)
        return 0;
            
    /* open it */
    link = load_source_file(syswinifc, src);

    /* return the window */
    return (link == 0 ? 0 : link->win_);
}

/*
 *   find the window for a text file
 */
CHtmlSysWin *CHtmlDebugHelper::
   find_text_file(CHtmlDebugSysIfc_win *syswinifc,
                  const char *fname, const char *path)
{
    CHtmlDbg_src *src;
    CHtmlDbg_win_link *link;
    
    /* find the line source */
    src = find_internal_src(syswinifc, fname, path, FALSE);

    /* if we didn't find a line source, there's definitely no window */
    if (src == 0)
        return 0;

    /* search the open file list for the line source */
    for (link = first_srcwin_ ; link != 0 ; link = link->next_)
    {
        /* if this is the one, return it */
        if (syswinifc->dbgsys_fnames_eq(path, link->path_.get()))
            return link->win_;
    }

    /* we didn't find it */
    return 0;
}

/*
 *   Load an arbitrary text file into a window.
 */
CHtmlSysWin *CHtmlDebugHelper::
   open_text_file_at_line(CHtmlDebugSysIfc_win *syswinifc,
                          const char *fname, const char *path,
                          unsigned long linenum)
{
    CHtmlDbg_src *src;
    CHtmlDbg_win_link *link;

    /*
     *   Scan the source file line sources to see if we're actually
     *   opening one of the game's source files.  If so, just open the
     *   line source directly. 
     */
    src = find_internal_src(syswinifc, fname, path, TRUE);

    /* open it */
//$$$    link = load_source_file(syswinifc, src);
    link = load_source_file(syswinifc, fname, path, src->source_id_);

    /* 
     *   if we got a link, and we want to show a line other than the first
     *   line, go to the line number 
     */
    if (link != 0 && link->win_ != 0 && linenum != 1)
        show_source_line(link, linenum);
        
    /* return the line source's window */
    return (link == 0 ? 0 : link->win_);
}

/*
 *   Load a line source into a window 
 */
CHtmlDbg_win_link *CHtmlDebugHelper::
   load_source_file(CHtmlDebugSysIfc_win *syswinifc, CHtmlDbg_src *src)
{
    char path[OSFNMAX];
    char *rootname;

    /*
     *   Ignore the path in the source tracker and ask the UI to help us
     *   find the file.  In the past, we tried to be clever and use the
     *   stored path, but this can create problems when moving a
     *   configuration between directories or copying it to another
     *   machine, so overall it's better to use a simple approach and just
     *   search the UI's source path for the filename.  
     */
    rootname = os_get_root_name((char *)src->fname_.get());
    if (!syswinifc->dbgsys_find_src(rootname, strlen(rootname),
                                    path, sizeof(path), TRUE))
    {
        /* the UI couldn't find it; give up */
        return 0;
    }

    /* save the path */
    src->path_.set(path);

    /* load the line source */
    return load_source_file(syswinifc, src->fname_.get(),
                            src->path_.get(), src->source_id_);
}

/*
 *   Load a source file 
 */
CHtmlDbg_win_link *CHtmlDebugHelper::
   load_source_file(CHtmlDebugSysIfc_win *syswinifc,
                    const textchar_t *fname, const textchar_t *path,
                    int source_id)
{
    CHtmlDbg_win_link *link;
    int err;

    /*
     *   Search for the file in our open file list.  If we find it, simply
     *   return the existing window.  
     */
    for (link = first_srcwin_ ; link != 0 ; link = link->next_)
    {
        /* if this window's path matches the one we want, stop looking */
        if (syswinifc->dbgsys_fnames_eq(path, link->path_.get()))
            return link;
    }

    /* 
     *   we didn't find it in our open file list, so we'll have to load it
     *   for real - create a new window, and load the file into the new
     *   window 
     */
    link = create_source_win(syswinifc, fname, path, source_id);
    err = vm_load_file_into_win(link, path);

    /* if we successfully loaded the file, format the window */
    if (err == 0)
    {
        CHtmlRect rc;

        /* do the initial formatting of the window */
        link->win_->do_formatting(TRUE, TRUE, TRUE);

        /* update the scrollbars */
        link->win_->fmt_adjust_vscroll();

        /* update special-line indicators for this window */
        update_lines_on_screen(link);
    
        /* scroll the window to the top */
        rc.set(0, 0, 0, 0);
        link->win_->scroll_to_doc_coords(&rc);
    }

    /* return the new window */
    return link;
}
    
/*
 *   Go through our special-line records and update the display so that a
 *   window's settings reflect the current special-line status flags.
 *   Note that we only have to do this for line sources with valid (i.e.,
 *   other than -1) id's, since these are the only sources that can have
 *   special line status flags.  
 */
void CHtmlDebugHelper::update_lines_on_screen(CHtmlDbg_win_link *win_link)
{
    CHtmlDbg_line_link *line_link;

    if (win_link->source_id_ != -1)
    {
        /* look through our list of special lines */
        for (line_link = first_line_ ; line_link != 0 ;
             line_link = line_link->next_)
        {
            /* if this line is from the source we just loaded, update it */
            if (line_link->source_id_ == win_link->source_id_)
            {
                /* update this line on-screen */
                update_line_on_screen(win_link, line_link);
            }
        }
    }
}


/*
 *   Create a new window for displaying a source file
 */
CHtmlDbg_win_link *CHtmlDebugHelper::create_source_win(
    CHtmlDebugSysIfc_win *syswinifc, const textchar_t *fname,
    const textchar_t *path, int source_id)
{
    /* create the window */
    return create_win(syswinifc, fname, fname, path, source_id,
                      HTMLDBG_WINTYPE_SRCFILE);
}

/*
 *   Set the tab expansion size to use in source windows.  If this changes
 *   while windows are open, we'll need to reload files into all open
 *   windows, since we translate tabs while loading files. 
 */
void CHtmlDebugHelper::set_tab_size(class CHtmlDebugSysIfc_win *syswinifc,
                                    int n)
{
    if (tab_size_ != n && tab_size_ != 0)
    {
        /* remember the new tab size */
        tab_size_ = n;
        
        /* reload all source windows */
        reload_source_windows(syswinifc);
    }
}

/*
 *   Reformat all source windows 
 */
void CHtmlDebugHelper::
   reformat_source_windows(dbgcxdef *ctx, CHtmlDebugSysIfc_win *syswinifc)
{
    CHtmlDbg_win_link *link;

    /* go through the list of windows, and reformat each */
    for (link = first_srcwin_ ; link != 0 ; link = link->next_)
        reformat_source_window(ctx, syswinifc, link, 0);
}

/*
 *   Reformat a source file 
 */
void CHtmlDebugHelper::
   reformat_source_window(dbgcxdef *ctx,
                          CHtmlDebugSysIfc_win *syswinifc,
                          CHtmlDbg_win_link *link,
                          const CHtmlRect *orig_scroll_pos)
{
    CHtmlFontDesc font_desc;
    CHtmlRect rc;

    /* 
     *   If the caller passed in the original scrolling position, use that
     *   position; otherwise, save the current scrolling position.  
     */
    if (orig_scroll_pos != 0)
        rc = *orig_scroll_pos;
    else
        link->win_->get_scroll_doc_coords(&rc);
        
    /* reset the window */
    link->formatter_->start_at_top(TRUE);

    /* get the font, in case it's changed */
    font_desc.fixed_pitch = TRUE;
    font_desc.htmlsize = 3;
    syswinifc->dbgsys_set_srcfont(&font_desc);
    link->font_ = link->win_->get_font(&font_desc);
    link->formatter_->set_font(link->font_);

    /* update the window according to what type of window we have */
    switch(link->win_type_)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
    case HTMLDBG_WINTYPE_HISTORY:
    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* do a complete reformat of this window */
        link->win_->do_formatting(TRUE, TRUE, TRUE);
        link->win_->fmt_adjust_vscroll();

        /* make sure we've updated line indicators */
        update_lines_on_screen(link);

        /* scroll back to the original position in the window */
        link->win_->scroll_to_doc_coords(&rc);

        /* done */
        break;

    case HTMLDBG_WINTYPE_STACK:
        /* 
         *   for the stack window, just rebuild it from scratch - it's
         *   easier than duplicating the code that figures out where all
         *   the special indicators go 
         */
        update_stack_window(ctx, FALSE);
        break;
    }
}

/*
 *   Reload a single source window.  If win == 0, reloads all source
 *   windows.  
 */
void CHtmlDebugHelper::reload_source_window(CHtmlDebugSysIfc_win *syswinifc,
                                            CHtmlSysWin *win)
{
    CHtmlDbg_win_link *link;

    /* go through the list of windows */
    for (link = first_srcwin_ ; link != 0 ; link = link->next_)
    {
        /* 
         *   if it's a source file, and either it matches the target
         *   window or we're reloading all windows (win == 0), reload this
         *   window 
         */
        if (link->win_type_ == HTMLDBG_WINTYPE_SRCFILE
            && (win == link->win_ || win == 0))
        {
            const textchar_t *path;
            CHtmlRect orig_scroll_pos;

            /* 
             *   open the file - use the full path name if present,
             *   otherwise use the filename 
             */
            if (link->path_.get() != 0 && link->path_.get()[0] != '\0')
                path = link->path_.get();
            else
                path = link->fname_.get();

            /* make sure we can access the file */
            if (osfacc(path))
                continue;

            /* 
             *   before re-loading the file, remember the original
             *   scrolling position in the window, so that we can restore
             *   the original position after we finish reloading the file 
             */
            link->win_->get_scroll_doc_coords(&orig_scroll_pos);

            /* clear the page */
            link->formatter_->start_at_top(FALSE);

            /* load it */
            vm_load_file_into_win(link, path);

            /* reformat it */
            reformat_source_window(0, syswinifc, link, &orig_scroll_pos);

            /* 
             *   if we're reloading only one window, there's no need to
             *   look any further 
             */
            if (win != 0)
                break;
        }
        else
        {
            /* 
             *   if we're doing all windows, reformat this one even though
             *   it's not a source window 
             */
            if (win == 0)
                reformat_source_window(0, syswinifc, link, 0);
        }
    }
}

/*
 *   get the stack window 
 */
CHtmlSysWin *CHtmlDebugHelper::get_stack_win() const
{
    return (stack_win_ != 0 ? stack_win_->win_ : 0);
}

/*
 *   get the history window 
 */
CHtmlSysWin *CHtmlDebugHelper::get_hist_win() const
{
    return (hist_win_ != 0 ? hist_win_->win_ : 0);
}

/*
 *   get the debug log window 
 */
CHtmlSysWin *CHtmlDebugHelper::get_debuglog_win() const
{
    return (debuglog_win_ != 0 ? debuglog_win_->win_ : 0);
}

/*
 *   clear the debug window 
 */
void CHtmlDebugHelper::clear_debuglog_win()
{
    if (debuglog_win_ != 0)
    {
        /* clear out the window */
        debuglog_win_->formatter_->start_at_top(TRUE);
        debuglog_win_->formatter_->set_font(debuglog_win_->font_);
        debuglog_win_->parser_->clear_page();
    }
}

/*
 *   Create a window to use for stack display 
 */
CHtmlSysWin *CHtmlDebugHelper::
   create_stack_win(CHtmlDebugSysIfc_win *syswinifc)
{
    /* create a new stack window if we don't already have one */
    if (stack_win_ == 0)
        stack_win_ = create_win(syswinifc, "Stack", 0, 0, -1,
                                HTMLDBG_WINTYPE_STACK);

    /* return the system window */
    return stack_win_->win_;
}

/*
 *   Create the debug log window 
 */
CHtmlSysWin *CHtmlDebugHelper::
   create_debuglog_win(CHtmlDebugSysIfc_win *syswinifc)
{
    /* create a new debug log window if we don't have one yet */
    if (debuglog_win_ == 0)
        debuglog_win_ = create_win(syswinifc, "Debug Log", 0, 0, -1,
                                   HTMLDBG_WINTYPE_DEBUGLOG);

    /* return the system window */
    return debuglog_win_->win_;
}

/*
 *   Create a window to use for history display 
 */
CHtmlSysWin *CHtmlDebugHelper::
   create_hist_win(CHtmlDebugSysIfc_win *syswinifc)
{
    /* create a new stack window if we don't already have one */
    if (hist_win_ == 0)
        hist_win_ = create_win(syswinifc, "Call History", 0, 0, -1,
                               HTMLDBG_WINTYPE_HISTORY);

    /* return the system window */
    return hist_win_->win_;
}


/*
 *   Create a debugger window.  linf can be null (this will be the case
 *   for system windows, such as the stack window, not tied to a line
 *   source).  
 */
CHtmlDbg_win_link *CHtmlDebugHelper::
   create_win(CHtmlDebugSysIfc_win *syswinifc,
              const textchar_t *win_title,
              const textchar_t *fname, const textchar_t *path,
              int source_id, HtmlDbg_wintype_t win_type)
{
    CHtmlParser *parser;
    CHtmlFormatter *formatter;
    CHtmlSysWin *win;
    CHtmlFontDesc font_desc;
    CHtmlSysFont *font;

    /* 
     *   if there's an underlying file, make sure it exists - if not, fail
     *   to create the window 
     */
    if (path != 0 && osfacc(path))
        return 0;

    /* create a parser for the window */
    parser = new CHtmlParser(TRUE);

    /* create a formatter for the window */
    formatter = new CHtmlFormatterSrcwin(parser);

    /* if there's no path, use the filename instead */
    if (path == 0)
        path = fname;

    /* create the system window using the system interface */
    win = syswinifc
          ->dbgsys_create_win(parser, formatter, win_title, path,
                              win_type);

    /* set up the formatter with the default monospaced font */
    font_desc.fixed_pitch = TRUE;
    font_desc.htmlsize = 3;
    syswinifc->dbgsys_set_srcfont(&font_desc);
    font = win->get_font(&font_desc);
    formatter->set_font(font);

    /* 
     *   add the window to our list of source windows, and return the new
     *   link structure 
     */
    return add_src_win(win, parser, formatter,  win_title,
                       fname, path, source_id, font, win_type);
}

/*
 *   Add a window to the source window list 
 */
CHtmlDbg_win_link *CHtmlDebugHelper::add_src_win(
    CHtmlSysWin *win, CHtmlParser *parser, CHtmlFormatter *formatter,
    const textchar_t *win_title, const textchar_t *fname,
    const textchar_t *path, int source_id, CHtmlSysFont *font,
    HtmlDbg_wintype_t win_type)
{
    CHtmlDbg_win_link *link;
    
    /* create the new link */
    link = new CHtmlDbg_win_link(win, parser, formatter, win_title,
                                 fname, path, source_id, font, win_type);

    /* link it in at the end of our list of source windows */
    if (last_srcwin_ != 0)
        last_srcwin_->next_ = link;
    else
        first_srcwin_ = link;
    link->prev_ = last_srcwin_;
    last_srcwin_ = link;

    /* return the new link */
    return link;
}

/*
 *   Delete a window link from the source window list.  Removes the link
 *   from the list and deletes the link object.  
 */
void CHtmlDebugHelper::del_src_win(CHtmlDbg_win_link *link)
{
    /* unlink it from its previous item */
    if (link->prev_ != 0)
        link->prev_->next_ = link->next_;
    else
        first_srcwin_ = link->next_;

    /* unlink it from its next item */
    if (link->next_ != 0)
        link->next_->prev_ = link->prev_;
    else
        last_srcwin_ = link->prev_;

    /* delete the link */
    delete link;
}

/*
 *   Receive notification that a source window is being closed 
 */
void CHtmlDebugHelper::on_close_srcwin(CHtmlSysWin *win)
{
    CHtmlDbg_win_link *link;
    
    /* find the window in our list */
    for (link = first_srcwin_ ; link != 0 ; link = link->next_)
    {
        /* if this is the one, delete it */
        if (link->win_ == win)
        {
            /* this is it - delete the formatter and parser */
            link->formatter_->release_parser();
            delete link->parser_;
            delete link->formatter_;

            /* if this is the stack window, forget that as well */
            if (link == stack_win_)
                stack_win_ = 0;

            /* if it's the debug log window, forget it */
            if (link == debuglog_win_)
                debuglog_win_ = 0;

            /* if it's the history window, forget it, too */
            if (link == hist_win_)
                hist_win_ = 0;

            /* now delete the link itself */
            del_src_win(link);

            /* we're done */
            return;
        }
    }

    /* we didn't find it; ignore the request */
}

/*
 *   Get the line and window link records for the current selection in a
 *   given window.  Returns the line number of the start of the current
 *   selection in the window.  
 */
unsigned long CHtmlDebugHelper::
   find_line_info(CHtmlSysWin *win, CHtmlDbg_win_link **win_linkp,
                  CHtmlDbg_line_link **line_linkp) const
{
    CHtmlDbg_win_link *wlink;
    
    /* presume we won't find anything */
    *win_linkp = 0;
    *line_linkp = 0;

    /* find the window in our list of source windows */
    for (wlink = first_srcwin_ ; wlink != 0 ; wlink = wlink->next_)
    {
        /* if this is the one, find the current selection in it */
        if (wlink->win_ == win)
        {
            unsigned long sel_start, sel_end;
            unsigned long linenum;
            CHtmlDispTextDebugsrc *disp;

            /* get the start of the current selection */
            wlink->formatter_->get_sel_range(&sel_start, &sel_end);

            /* find the display item at the start of the selection */
            disp = (CHtmlDispTextDebugsrc *)
                   wlink->formatter_->find_by_txtofs(sel_start, TRUE, FALSE);
            if (disp == 0)
                return 0;

            /* get the file position of the display item */
            linenum = disp->get_debugsrcpos();

            /* tell the caller about the window */
            *win_linkp = wlink;

            /* find the line record */
            *line_linkp = find_line_link(wlink->source_id_, linenum);

            /* return the file position */
            return linenum;
        }
    }

    /* didn't find anything */
    return 0;
}


/*
 *   Get the line link record for a given source line 
 */
CHtmlDbg_line_link *CHtmlDebugHelper::find_line_link(
    int line_source_id, unsigned long linenum) const
{
    CHtmlDbg_line_link *link;

    /* find the line in our list */
    for (link = first_line_ ; link != 0 ; link = link->next_)
    {
        /* see if this is the one we're looking for */
        if (link->equals(line_source_id, linenum))
        {
            /* this is it - return it */
            return link;
        }
    }

    /* didn't find it */
    return 0;
}


/*
 *   Get information on the source line containing the given breakpoint 
 */
int CHtmlDebugHelper::get_bp_line_info(int bpnum, int *source_id,
                                       unsigned long *linenum)
{
    CHtmlDbg_bp *bp;
    
    /* find the breakpoint */
    bp = find_internal_bp(bpnum);
    if (bp == 0)
        return FALSE;

    /* check the breakpoint type */
    if (bp->global_)
    {
        /* there's no line information for a global breakpoint */
        *source_id = -1;
        *linenum = 0;
        return FALSE;
    }
    else
    {
        /* it's a source line - return the information */
        *source_id = bp->source_id_;
        *linenum = bp->linenum_;
        return TRUE;
    }
}

/*
 *   Set or clear a source line status bit
 */
void CHtmlDebugHelper::change_line_status(int line_source_id,
                                          unsigned long seekpos,
                                          unsigned int statflag, int set)
{
    CHtmlDbg_line_link *link;

    /* find the link */
    link = find_line_link(line_source_id, seekpos);

    /* if we found it, adjust the current flags as requested */
    if (link != 0)
    {
        /* set or clear the new flags as needed */
        if (set)
            link->stat_ |= statflag;
        else
            link->stat_ &= ~statflag;
        
        /* update the line on-screen */
        update_line_on_screen(link);
        
        /* 
         *   if the flags are now all cleared in this line, drop the line
         *   from the special lines list, since there's nothing special to
         *   track about it any more 
         */
        if (link->stat_ == 0)
        {
            /* unlink it from the list */
            if (link->prev_ == 0)
                first_line_ = link->next_;
            else
                link->prev_->next_ = link->next_;
            if (link->next_ == 0)
                last_line_ = link->prev_;
            else
                link->next_->prev_ = link->prev_;
            
            /* delete it */
            delete link;
        }
    }
    else
    {
        /*
         *   We didn't find the line we were looking for, so the line
         *   doesn't currently have any special properties.  If we're
         *   clearing bits, there's nothing to do, since the line already
         *   has all its status bits cleared.  
         */
        if (!set)
            return;
        
        /* create a new special-line link and add it to the list */
        link = new CHtmlDbg_line_link(line_source_id, seekpos, statflag);
        link->prev_ = last_line_;
        link->next_ = 0;
        if (last_line_ != 0)
            last_line_->next_ = link;
        else
            first_line_ = link;
        last_line_ = link;
        
        /* update the line on-screen */
        update_line_on_screen(link);
    }
}

/*
 *   Update a line on-screen 
 */
void CHtmlDebugHelper::update_line_on_screen(CHtmlDbg_line_link *link)
{
    CHtmlDbg_win_link *win;
    
    /*
     *   Find the line's file in our file list.  If the line isn't in our
     *   file list anywhere, it's not displayed anywhere on screen, so
     *   there's nothing to update just now. 
     */
    for (win = first_srcwin_ ; win != 0 ; win = win->next_)
    {
        /* 
         *   See if this window's line source ID matches the line's source
         *   ID.  Note that we'll keep going even if we find a match,
         *   since we could conceivably have the same file open in
         *   multiple windows.  
         */
        if (win->source_id_ == link->source_id_)
            update_line_on_screen(win, link);
    }
}

/*
 *   update a particular line in a particular window on-screen 
 */
void CHtmlDebugHelper::update_line_on_screen(CHtmlDbg_win_link *win,
                                             CHtmlDbg_line_link *link)
{
    CHtmlDispTextDebugsrc *disp;

    /* find the display item for this line */
    disp = (CHtmlDispTextDebugsrc *)win->formatter_->
           find_by_debugsrcpos(link->linenum_);

    /* if we found it, update its status */
    if (disp != 0)
        disp->set_dbg_status(win->win_, link->stat_);
}


/*
 *   Context structure for the stack iterator callback 
 */
struct HtmlDbg_stkdisp_t
{
    HtmlDbg_stkdisp_t(CHtmlDebugHelper *helper)
    {
        /* remember the helper object */
        helper_ = helper;

        /* start out at level 0 */
        level_ = 0;
    }
    
    /* the helper object */
    CHtmlDebugHelper *helper_;

    /* stack level - we increment this for each line in the stack trace */
    int level_;

    /* buffer for batching up each line of text */
    CStringBuf buf_;
};

/*
 *   Update the stack window 
 */
void CHtmlDebugHelper::update_stack_window(dbgcxdef *ctx,
                                           int reset_level)
{
    HtmlDbg_stkdisp_t cbctx(this);
    static textchar_t emptymsg[] = "[stack trace unavailable]";
    CHtmlDispTextDebugsrc *disp;
    int level;
    
    /* if we don't have a stack window, there's nothing to update */
    if (stack_win_ == 0)
        return;

    /* delete everything currently in the stack window */
    stack_win_->formatter_->start_at_top(TRUE);
    stack_win_->formatter_->set_font(stack_win_->font_);
    stack_win_->parser_->clear_page();

    /* determine what level we'll be at when we're done */
    if (reset_level)
    {
        /* we're resetting the level to zero */
        level = 0;
    }
    else
    {
        /* remember the current level */
        level = (ctx_stackwin_disp_ == 0
                 ? 0 : ctx_stackwin_disp_->get_debugsrcpos());
    }
    
    /* 
     *   make sure we're not holding on to a context display item, since
     *   we're about to delete all of the display items in the stack
     *   window 
     */
    ctx_stackwin_disp_ = 0;
    
    /* build the stack */
    if (ctx != 0)
        vm_build_stack_listing(ctx, &cbctx);

    /*
     *   If there's nothing on the stack at all, it means the game is
     *   running.  Put an indication of this in the stack listing so that
     *   it's not completely empty. 
     */
    if (cbctx.level_ == 0)
        stack_win_->parser_->append_tag(new CHtmlTagTextDebugsrc(
            0, tab_size_, stack_win_->parser_->get_text_array(),
            emptymsg, get_strlen(emptymsg)));

    /* add a blank line at the end */
    stack_win_->parser_->append_tag(new CHtmlTagTextDebugsrc(
        0, tab_size_, stack_win_->parser_->get_text_array(), "", 0));

    /* format the stack into the stack window */
    if (!stack_win_->win_->do_formatting(TRUE, TRUE, TRUE))
    {
        CHtmlRect area(0, 0, HTMLSYSWIN_MAX_RIGHT, HTMLSYSWIN_MAX_BOTTOM);
        stack_win_->win_->inval_doc_coords(&area);
    }

    /* update the scrollbars */
    stack_win_->win_->fmt_adjust_vscroll();

    /* 
     *   Turn on the current-line status flag for the first item - the
     *   first item in the list is always the current line, so display it
     *   thus.  Don't do this if there's no stack traceback available.  
     */
    disp = (CHtmlDispTextDebugsrc *)
           stack_win_->formatter_->find_by_debugsrcpos(0);
    if (disp != 0)
        disp->set_dbg_status(stack_win_->win_,
                             cbctx.level_ == 0 ? 0 :
                             HTMLTDB_STAT_CURLINE);

    /* set the context line, if we're at a nested context */
    if (level != 0)
    {
        /* get the correct level */
        ctx_stackwin_disp_ = (CHtmlDispTextDebugsrc *)
            stack_win_->formatter_->find_by_debugsrcpos(level);

        /* set its status */
        if (ctx_stackwin_disp_ != 0)
            ctx_stackwin_disp_->set_dbg_status(stack_win_->win_,
                                               HTMLTDB_STAT_CTXLINE);
    }
}

/*
 *   Display callback for for update_stack_window 
 */
void CHtmlDebugHelper::update_stack_disp_cb(void *ctx0,
                                            const char *str, int strl)
{
    HtmlDbg_stkdisp_t *ctx = (HtmlDbg_stkdisp_t *)ctx0;
    
    /* invoke the non-static callback handler */
    ctx->helper_->update_stack_disp(ctx, str, strl);
}

void CHtmlDebugHelper::update_stack_disp(HtmlDbg_stkdisp_t *ctx,
                                         const char *str, int strl)
{
    const char *p;
    const char *p_end;

    /* look for a newline */
    for (p = str, p_end = str + strl ; p < p_end ; ++p)
    {
        /* if this is a newline, add the line to the window */
        if (*p == '\n')
        {
            CHtmlTag *tag;

            /* if we have anything prior to the newline, add to the window */
            if (p != str)
            {
                /* append up to the newline to our line buffer */
                ctx->buf_.append(str, p - str);
            }

            /* 
             *   Add the line in the buffer to our list.  Note that we use
             *   the line number (i.e., the stack nesting level) as the
             *   position. 
             */
            tag = new CHtmlTagTextDebugsrc
                  (ctx->level_, tab_size_,
                   stack_win_->parser_->get_text_array(),
                   ctx->buf_.get(), get_strlen(ctx->buf_.get()));
            stack_win_->parser_->append_tag(tag);

            /* count the line */
            ctx->level_++;

            /* the buffer has been used, so clear it */
            ctx->buf_.set("");
            
            /* drop everything prior to the newline */
            str = p + 1;
        }
    }
    
    /* add the remainder of the line to the buffer */
    if (p > str)
        ctx->buf_.append(str, p - str);
}

/*
 *   Activate a stack window item 
 */
CHtmlSysWin *CHtmlDebugHelper::
   activate_stack_win_item(CHtmlDisp *disp, dbgcxdef *ctx,
                           CHtmlDebugSysIfc_win *syswinifc)
{
    CHtmlDispTextDebugsrc *dbgdisp = (CHtmlDispTextDebugsrc *)disp;
    unsigned long level;

    /* if there's no context, there's nothing to do */
    if (ctx == 0)
        return 0;
    
    /* forget any existing context line we have */
    forget_ctx_line();

    /* 
     *   get the level from the item - we store this as the file position
     *   of the item 
     */
    level = dbgdisp->get_debugsrcpos();

    /* activate the level */
    return activate_stack_win_line(level, ctx, syswinifc);
}

/*
 *   Activate a stack window line 
 */
CHtmlSysWin *CHtmlDebugHelper::
   activate_stack_win_line(int level, dbgcxdef *ctx,
                           CHtmlDebugSysIfc_win *syswinifc)
{
    /* 
     *   if it's level 0, select the current line; otherwise, activate the
     *   enclosing stack context level 
     */
    if (level == 0)
    {
        /* no new context line - activate the current line */
        return load_current_source(ctx, syswinifc);
    }
    else
    {
        CHtmlSysWin *win;
        CHtmlDispTextDebugsrc *dbgdisp;

        /* load the source at this level */
        win = load_source_at_level(ctx, syswinifc, level);

        /* update the stack window context display if possible */
        if (stack_win_ != 0)
        {
            /* find the display item for this line */
            dbgdisp = (CHtmlDispTextDebugsrc *)
                      stack_win_->formatter_->get_disp_by_linenum(level);
            
            /* set the stack window to show that the new context line */
            dbgdisp->set_dbg_status(stack_win_->win_, HTMLTDB_STAT_CTXLINE);
            ctx_stackwin_disp_ = dbgdisp;
        }
            
        /* return the window */
        return win;
    }
}

/*
 *   Update the history window 
 */
void CHtmlDebugHelper::update_hist_window(dbgcxdef *ctx)
{
    const textchar_t *startp;
    const textchar_t *endp;
    const textchar_t *p;

    /* if there's no context, ignore it */
    if (ctx == 0)
        return;

    /* if there's no history window, there's nothing to update */
    if (hist_win_ == 0)
        return;

    /* delete everything currently in the window */
    hist_win_->formatter_->start_at_top(TRUE);
    hist_win_->formatter_->set_font(hist_win_->font_);
    hist_win_->parser_->clear_page();
    
    /* run through the history buffer */
    startp = get_call_trace_buf(ctx);
    endp = startp + get_call_trace_len(ctx);
    for (p = startp ; p < endp ; p += get_strlen(p) + 1)
    {
        CHtmlTag *tag;
        
        /* store this line */
        tag = new CHtmlTagTextDebugsrc
              (0, tab_size_, hist_win_->parser_->get_text_array(),
               p, get_strlen(p));
        hist_win_->parser_->append_tag(tag);
    }

    /* add a blank line at the end */
    hist_win_->parser_->append_tag(new CHtmlTagTextDebugsrc(
        0, tab_size_, hist_win_->parser_->get_text_array(), "", 0));

    /* format it into the window */
    if (!hist_win_->win_->do_formatting(TRUE, TRUE, TRUE))
    {
        CHtmlRect area(0, 0, HTMLSYSWIN_MAX_RIGHT, HTMLSYSWIN_MAX_BOTTOM);
        hist_win_->win_->inval_doc_coords(&area);
    }

    /* update the scrollbars */
    hist_win_->win_->fmt_adjust_vscroll();
}

/*
 *   Enumerate line sources 
 */
void CHtmlDebugHelper::
   enum_source_files(void (*func)(void *cbctx, const char *fname,
                                  int line_source_id), void *cbctx)
{
    CHtmlDbg_src *src;
    CHtmlDbg_src *nxt;
    
    /* run through the line sources, and invoke the callback for each one */
    for (src = src_ ; src != 0 ; src = nxt)
    {
        /* remember the next one, in case the callback deletes this one */
        nxt = src->nxt_;

        /* invoke the callback */
        (*func)(cbctx, src->fname_.get(), src->source_id_);
    }
}

/*
 *   Find an internal source tracker 
 */
CHtmlDbg_src *CHtmlDebugHelper::find_internal_src(int source_id) const
{
    CHtmlDbg_src *src;

    /* scan our list */
    for (src = src_ ; src != 0 ; src = src->nxt_)
    {
        /* if this one matches, return it */
        if (src->source_id_ == source_id)
            return src;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Find an internal source tracker by filename 
 */
CHtmlDbg_src *CHtmlDebugHelper::
   find_internal_src(CHtmlDebugSysIfc_win *syswinifc,
                     const textchar_t *fname, const textchar_t *path,
                     int add_if_not_found)
{
    CHtmlDbg_src *src;

    /* 
     *   if the given filename is absolute, and there's no path, consider
     *   the filename to be the path 
     */
    if ((path == 0 || path[0] == '\0') && os_is_file_absolute(fname))
        path = fname;
    
    /* scan our list */
    for (src = src_ ; src != 0 ; src = src->nxt_)
    {
        int match;
        
        /* 
         *   Compare the filenames.  If we have a path and the source has
         *   a path, compare the paths.  Otherwise, compare just the
         *   display filenames.  
         */
        if (path == 0 || path[0] == '\0'
            || src->path_.get() == 0 || src->path_.get()[0] == '\0')
        {
            /* 
             *   if the file we're looking for has a path and the line
             *   source doesn't, check to see if the line source's
             *   filename matches the given path 
             */
            if (path != 0 && path[0] != '\0')
            {
                /* 
                 *   the file has a path, even though the line source
                 *   doesn't - compare the filename to see if it belongs
                 *   in the path 
                 */
                match = syswinifc
                        ->dbgsys_fname_eq_path(path, src->fname_.get());

                /* 
                 *   if we matched, set the line source's path to the full
                 *   path of this file, so that we look for this line
                 *   source in this location from now on 
                 */
                if (match)
                    src->path_.set(path);
            }
            else if (src->path_.get() != 0 && src->path_.get()[0] != '\0')
            {
                /* 
                 *   the line source has a path, even though the file
                 *   doesn't - compare the filename to see if it belongs
                 *   in the path 
                 */
                match = syswinifc
                        ->dbgsys_fname_eq_path(src->path_.get(), fname);
            }
            else
            {
                /* we don't have any paths - compare only the filenames */
                match = syswinifc
                        ->dbgsys_fnames_eq(fname, src->fname_.get());
            }
        }
        else
        {
            /* we have full paths - compare them */
            match = syswinifc->dbgsys_fnames_eq(path, src->path_.get());
        }

        /* if they match, this is the source we want */
        if (match)
            return src;
    }

    /* 
     *   we didn't find it - if they want to create a new one in this
     *   case, do so now 
     */
    if (add_if_not_found)
        return add_fake_internal_line_source(fname);

    /* not found */
    return 0;
}

/*
 *   Find an internal breakpoint tracker 
 */
CHtmlDbg_bp *CHtmlDebugHelper::find_internal_bp(int bpnum) const
{
    CHtmlDbg_bp *bp;

    /* scan our list */
    for (bp = bp_ ; bp != 0 ; bp = bp->nxt_)
    {
        /* if this is the one, return it */
        if (bp->bpnum_ == bpnum)
            return bp;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Toggle an internal breakpoint tracker 
 */
void CHtmlDebugHelper::toggle_internal_bp(dbgcxdef *ctx, int source_id,
                                          unsigned long linenum,
                                          const textchar_t *cond, int change,
                                          int bpnum, int did_set, int global)
{
    CHtmlDbg_bp *bp;

    /* 
     *   if it's not global, try to find an internal breakpoint record for
     *   the same code location (if it's global, don't bother, since
     *   there's no code location to check for a breakpoint) 
     */
    if (!global)
        bp = find_internal_bp(source_id, linenum);
    else
        bp = 0;

    /* 
     *   We're setting a new breakpoint if we didn't find an existing one.
     *   (Ignore the indication as to whether the underlying engine set
     *   the breakpoint; trust our own records instead.) 
     */
    did_set = (bp == 0);

    /* add or remove our internal record */
    if (did_set)
    {
        /* if the engine isn't running, synthesize a breakpoint ID */
        if (ctx == 0)
            bpnum = synthesize_bp_num();

        /* create a new breakpoint record */
        bp = new CHtmlDbg_bp(global, source_id, linenum, bpnum, cond, change);

        /* link it into our list at the end */
        if (bp_tail_ != 0)
            bp_tail_->nxt_ = bp;
        else
            bp_ = bp;
        bp_tail_ = bp;
        bp->nxt_ = 0;
    }
    else
    {
        /* we're clearing the breakpoint - delete our tracker */
        delete_internal_bp(bp);
    }

    /* 
     *   Update the visual status of the source location, so that it shows a
     *   breakpoint marker.  (But not for globals, for obvious reasons.)  
     */
    if (!global)
        change_line_status(source_id, linenum, HTMLTDB_STAT_BP, did_set);

    /* 
     *   if we cleared the breakpoint, make sure we clear any
     *   breakpoint-related flags on the line 
     */
    if (!did_set)
        change_line_status(source_id, linenum,
                           (HTMLTDB_STAT_BP_DIS | HTMLTDB_STAT_BP_COND),
                           FALSE);
}

/*
 *   Toggle the disabled status of an internal breakpoint tracker 
 */
void CHtmlDebugHelper::toggle_internal_bp_disable(
    dbgcxdef *ctx, int source_id, unsigned long linenum)
{
    CHtmlDbg_bp *bp;
    
    /* find our internal tracker */
    bp = find_internal_bp(source_id, linenum);

    /* if we found it, toggle its state */
    if (bp != 0)
    {
        int disabled;

        /* toggle the status */
        bp->enabled_ = !bp->enabled_;

        /* note the new status */
        disabled = !bp->enabled_;

        /* update the special status of the line */
        change_line_status(bp->source_id_, bp->linenum_,
                           HTMLTDB_STAT_BP_DIS, disabled);
    }
}

/*
 *   Find an internal breakpoint tracker 
 */
CHtmlDbg_bp *CHtmlDebugHelper::find_internal_bp(int source_id,
                                                unsigned long linenum) const
{
    CHtmlDbg_bp *bp;

    /* scan our list */
    for (bp = bp_ ; bp != 0 ; bp = bp->nxt_)
    {
        /* if this is the one, return it */
        if (!bp->global_
            && bp->source_id_ == source_id
            && bp->linenum_ == linenum)
            return bp;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Delete an internal breakpoint tracker and update the visual
 *   representation of the breakpoint, if any
 */
void CHtmlDebugHelper::delete_internal_bp(dbgcxdef *ctx, int bpnum)
{
    CHtmlDbg_bp *bp;

    /* find the breakpoint */
    bp = find_internal_bp(bpnum);

    /* if we don't have an internal tracker for the breakpoint, ignore it */
    if (bp == 0)
        return;

    /* update the visual status, if it's not a global breakpoint */
    if (!bp->global_)
        change_line_status(bp->source_id_, bp->linenum_,
                           HTMLTDB_STAT_BP, FALSE);

    /* delete our internal tracker */
    delete_internal_bp(bp);
}

/*
 *   delete a breakpoint tracker 
 */
void CHtmlDebugHelper::delete_internal_bp(CHtmlDbg_bp *bp_to_del)
{
    CHtmlDbg_bp *bp;
    CHtmlDbg_bp *prv;

    /* find the entry in the list */
    for (prv = 0, bp = bp_ ; bp != 0 ; prv = bp, bp = bp->nxt_)
    {
        /* if this is the one, delete it from the list */
        if (bp == bp_to_del)
        {
            /* unlink it from the list */
            if (prv == 0)
                bp_ = bp->nxt_;
            else
                prv->nxt_ = bp->nxt_;

            /* if it's the last one, move the tail pointer back */
            if (bp == bp_tail_)
                bp_tail_ = prv;

            /* delete it */
            delete bp;

            /* done */
            return;
        }
    }
}

/*
 *   Synthesize a breakpoint ID 
 */
int CHtmlDebugHelper::synthesize_bp_num() const
{
    int max_bpnum;
    CHtmlDbg_bp *bp;
    
    /* find the maximum breakpoint number currently in use */
    for (bp = bp_, max_bpnum = 0 ; bp != 0 ; bp = bp->nxt_)
    {
        /* if this is higher than the highest we've seen yet, record it */
        if (bp->bpnum_ > max_bpnum)
            max_bpnum = bp->bpnum_;
    }
    
    /* 
     *   return one higher than the maximum currently in use - this is
     *   guaranteed to be unique, since it's not equal to anything in the
     *   list 
     */
    return max_bpnum + 1;
}


/* ------------------------------------------------------------------------ */
/*
 *   CHtmlDbg_win_link implementation 
 */

CHtmlDbg_win_link::CHtmlDbg_win_link(CHtmlSysWin *win, CHtmlParser *parser,
                                     CHtmlFormatter *formatter,
                                     const textchar_t *win_title,
                                     const textchar_t *fname,
                                     const textchar_t *path,
                                     int source_id,
                                     CHtmlSysFont *font,
                                     HtmlDbg_wintype_t win_type)
{
    /* remember window and parser */
    win_ = win;
    parser_ = parser;
    formatter_ = formatter;

    /* not in a list yet, so clear link pointers */
    next_ = prev_ = 0;

    /* remember the name of the file */
    fname_.set(fname != 0 ? fname : win_title);

    /* remember the path to the file */
    path_.set(path);

    /* remember the line source ID */
    source_id_ = source_id;

    /* remember the default font */
    font_ = font;

    /* remember the type of the window */
    win_type_ = win_type;
}

/* ------------------------------------------------------------------------ */
/*
 *   Special-line record 
 */

CHtmlDbg_line_link::CHtmlDbg_line_link(int source_id, unsigned long linenum,
                                       unsigned int init_stat)
{
    /* remember the information */
    source_id_ = source_id;
    linenum_ = linenum;
    stat_ = init_stat;

    /* not in a list yet */
    next_ = prev_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   internal line source tracker 
 */
CHtmlDbg_src::CHtmlDbg_src(const textchar_t *fname, const textchar_t *path,
                           int source_id)
{
    /* remember the information */
    fname_.set(fname);
    source_id_ = source_id;
    
    /* store the path only if it's not empty */
    if (path != 0 && path[0] != '\0')
    {
        /* a non-empty path is provided - store it */
        path_.set(path);
    }
    else
    {
        /* 
         *   no path is provided - if the filename appears to be an
         *   absolute path, store it as the path 
         */
        if (os_is_file_absolute(fname))
            path_.set(fname);
    }
    
    /* not in a list yet */
    nxt_ = 0;
}

