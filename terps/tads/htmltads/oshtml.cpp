#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/oshtml.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oshtml.c - interface layer from TADS to TADS-HTML front-end
Function
  This layer takes the place of the OS layer normally used in the TADS
  runtime.  Rather than providing an OS interface, this layer provides
  an interface the HTML front-end.
Notes
  
Modified
  11/02/97 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* include TADS interfaces that we'll need */
#include <os.h>
#include <run.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
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

/* status mode flag */
static int status_mode_ = 0;

/* HTML mode flag */
static int html_mode_ = 0;

/* input-in-progress flag (for os_gets_timeout) */
static int gets_in_progress_ = FALSE;

/* hilite mode - this flags our synthesized hilite attribute */
static int hilite_mode_ = FALSE;


/*
 *   Buffer containing score information for status line 
 */
static textchar_t status_score_buf_[256];

/* 
 *   The system frame object.  The OS code MUST create an object implementing
 *   the CHtmlSysFrame interface during program initialization, and call
 *   CHtmlSysFrame::set_frame_obj() to give us a reference to the object. 
 */
CHtmlSysFrame *CHtmlSysFrame::app_frame_ = 0;

/*
 *   Enter HTML mode 
 */
void os_start_html()
{
    CHtmlParser *parser;
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();
    
    /* if there's no app frame, there's nothing more to do */
    if (app_frame == 0)
        return;

    /* 
     *   Flush buffered text to the parser.  We don't need to format it
     *   right now, but we do need to parse it, since we're going to
     *   change the parsing mode. 
     */
    app_frame->flush_txtbuf(FALSE, FALSE);

    /* note that we're in HTML mode */
    html_mode_ = 1;

    /* turn on HTML parsing */
    parser = app_frame->get_parser();
    parser->obey_whitespace(FALSE, TRUE);
    parser->obey_markups(TRUE);
}

/*
 *   exit HTML mode 
 */
void os_end_html()
{
    CHtmlParser *parser;
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if there's no app frame, there's nothing to do */
    if (app_frame == 0)
        return;

    /* flush buffered text to the parser */
    app_frame->flush_txtbuf(FALSE, FALSE);

    /* note that we're not in HTML mode */
    html_mode_ = 0;

    /* turn off HTML parsing */
    parser = app_frame->get_parser();
    parser->obey_whitespace(TRUE, TRUE);
    parser->obey_markups(FALSE);
    parser->obey_end_markups(FALSE);
}

/*
 *   set non-stop mode 
 */
void os_nonstop_mode(int flag)
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* tell the app frame to set the mode */
    if (app_frame != 0)
        app_frame->set_nonstop_mode(flag);
}

/*
 *   check for user break (control-C, etc) 
 */
int os_break()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if there's no app frame, there can't be a break */
    if (app_frame == 0)
        return FALSE;

    /* ask the window to check for a break */
    return app_frame->check_break_key();
}

/* 
 *   display a string in the score area in the status line 
 */
void os_strsc(const char *p)
{
    size_t len;

    /* 
     *   remember the score string - it will be used the next time we
     *   rebuild the status line 
     */
    len = get_strlen(p) * sizeof(textchar_t);
    if (len > sizeof(status_score_buf_))
        len = sizeof(status_score_buf_) - sizeof(status_score_buf_[0]);
    memcpy(status_score_buf_, p, len);
    status_score_buf_[len / sizeof(status_score_buf_[0])] = '\0';
}

/*
 *   internal routine to display HTML tags, regardless of our current
 *   markup obedience mode 
 */
static void oshtml_display_html_tags(const textchar_t *txt)
{
    CHtmlParser *parser;
    int old_obey, old_obey_end;
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if there's no app frame, there's nothing to do */
    if (app_frame == 0)
        return;

    /* get the parser */
    parser = app_frame->get_parser();

    /* 
     *   Flush any buffered text to the parser.  We don't need to format
     *   it yet, but we do need to make sure it's fully parsed, since
     *   we're may be changing the parsing mode.  
     */
    app_frame->flush_txtbuf(FALSE, FALSE);

    /* obey markups long enough to parse our HTML markup text */
    old_obey = parser->get_obey_markups();
    old_obey_end = parser->get_obey_end_markups();
    parser->obey_markups(TRUE);

    /* display the output */
    app_frame->display_output(txt, strlen(txt));

    /* flush the result to the parser, since we may be changing modes */
    app_frame->flush_txtbuf(FALSE, FALSE);

    /* restore markup obedience mode */
    parser->obey_markups(old_obey);
    parser->obey_end_markups(old_obey_end);
}

/*
 *   internal routine to set the status mode 
 */
static void oshtml_status_mode(int flag)
{
    static const textchar_t banner_start[] =
        "<banner id=statusline height=previous border>"
        "<body bgcolor=statusbg text=statustext><b>";
    static const textchar_t banner_score[] = "</b><tab align=right><i>";
    static const textchar_t banner_end[] = "</i><br height=0></banner>";

    /* start or end the status line banner */
    if (flag)
    {
        /* start the banner */
        oshtml_display_html_tags(banner_start);
    }
    else
    {
        /* add the score to the banner */
        oshtml_display_html_tags(banner_score);
        oshtml_display_html_tags(status_score_buf_);
        oshtml_display_html_tags(banner_end);
    }
}

/* 
 *   set the status line mode 
 */
void os_status(int stat)
{
    /* 
     *   If we're in HTML mode, don't do any automatic status line
     *   generation -- let the game do the status line the way it wants,
     *   without any predefined handling from the run-time system 
     */
    if (html_mode_)
        return;

    /* see what mode we're setting */
    switch(stat)
    {
    case 0:
    default:
        /* turn off status line mode */
        status_mode_ = 0;
        oshtml_status_mode(0);
        break;

    case 1:
        /* turn on status line mode */
        status_mode_ = 1;
        oshtml_status_mode(1);
        break;
    }
}

/*
 *   get the status line mode 
 */
int os_get_status()
{
    return status_mode_;
}

/* 
 *   set the score value 
 */
void os_score(int cur, int turncount)
{
    char buf[128];
    
    /* build the default status line score format */
    sprintf(buf, "%d/%d", cur, turncount);

    /* set the score string */
    os_strsc(buf);
}


/* 
 *   read a line of input from the keyboard 
 */
unsigned char *os_gets(unsigned char *buf, size_t bufl)
{
    int evt;
    
    /* cancel any previous interrupted input */
    os_gets_cancel(TRUE);

    /* call the timeout version with no timeout specified */
    evt = os_gets_timeout(buf, bufl, 0, FALSE);

    /* translate the event code for our caller */
    return (evt == OS_EVT_EOF ? 0 : buf);
}

/*
 *   Finish an input that we previously started.  
 */
static void oss_gets_done()
{
    /* if an input line is in progress, end it */
    if (gets_in_progress_)
    {
        /* if we're not in HTML mode, close the input-font tag */
        if (!html_mode_)
            oshtml_display_html_tags("</font>");

        /* the input is no longer in progress */
        gets_in_progress_ = FALSE;
    }
}

/*
 *   read a line of input from the keyboard, with optional timeout 
 */
int os_gets_timeout(unsigned char *buf, size_t bufl,
                    unsigned long timeout, int use_timeout)
{
    int evt;
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();
    
    /* if app frame, we can't get input */
    if (app_frame == 0)
        return OS_EVT_EOF;

    /* 
     *   If we're in non-HTML mode, and we don't have a prior input already
     *   in progress, establish the input font by displaying the proper
     *   <font> tag.  
     */
    if (!html_mode_ && !gets_in_progress_)
        oshtml_display_html_tags("<font face='TADS-Input'>");

    /* note that we now have an input in progress */
    gets_in_progress_ = TRUE;

    /* 
     *   get input from the app frame - return null if it returns end of
     *   file (i.e., application terminating) 
     */
    evt = app_frame->get_input_timeout(
        (textchar_t *)buf, bufl, timeout, use_timeout);

    /* if that returned end-of-file, pass that along to our caller */
    if (evt == OS_EVT_EOF)
        return evt;

    /* if we didn't time out, the gets is now completed */
    if (evt != OS_EVT_TIMEOUT)
        oss_gets_done();

    /* return the event code */
    return evt;
}

/*
 *   Cancel a previously interrupted input 
 */
void os_gets_cancel(int reset)
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* cancel the input in the app window */
    if (app_frame != 0)
        app_frame->get_input_cancel(reset);

    /* do our own cleanup on the pending input */
    oss_gets_done();
}

/* 
 *   read a character from the keyboard 
 */
int os_getc()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if the app frame is missing, we can't get input - indicate EOF */
    if (app_frame == 0)
    {
        static int eof_cmd;

        /* 
         *   return (0 + CMD_EOF) sequences as we're called; alternate
         *   between the '\000' character and the CMD_EOF character 
         */
        eof_cmd = !eof_cmd;
        return (eof_cmd ? 0 : CMD_EOF);
    }

    /* ask the app frame to get a key */
    return textchar_to_int(app_frame->wait_for_keystroke(FALSE));
}

/*
 *   Read a character from the keyboard in raw mode.  Because the OS code
 *   handles command line editing itself, we presume that os_getc() is
 *   already returning the low-level raw keystroke information, so we'll
 *   just return that same information. 
 */
int os_getc_raw()
{
    return os_getc();
}


/* wait for a character to become available from the keyboard */
void os_waitc()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* tell the app frame to wait for a keystroke */
    if (app_frame != 0)
        app_frame->wait_for_keystroke(TRUE);
}

/*
 *   get an event 
 */
int os_get_event(unsigned long timeout_in_milliseconds, int use_timeout,
                 os_event_info_t *info)
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if there's no app frame, we can't get an event */
    if (app_frame == 0)
        return OS_EVT_EOF;

    /* ask the app frame for an event and return the result */
    return app_frame->get_input_event(timeout_in_milliseconds, use_timeout,
                                      info);
}

/* 
 *   clear the screen 
 */
void oscls()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if app frame, we can't do this */
    if (app_frame == 0)
        return;

    /* tell the app frame to start a new page */
    app_frame->start_new_page();
}

/* 
 *   flush any buffered display output 
 */
void os_flush()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* tell the app frame to flush its buffers */
    if (app_frame != 0)
        app_frame->flush_txtbuf(TRUE, FALSE);
}

/*
 *   immediately update the display 
 */
void os_update_display()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* 
     *   tell the app frame to flush its buffers and immediately update the
     *   display 
     */
    if (app_frame != 0)
        app_frame->flush_txtbuf(TRUE, TRUE);
}

/*
 *   Set text attributes.  We can ignore these if we're in HTML mode,
 *   because we can count on the caller sending us HTML markups directly.
 *   When we're not in HTML mode, though, we need to supply the appropriate
 *   formatting.  
 */
void os_set_text_attr(int attrs)
{
    /* 
     *   ignore attribute settings in HTML mode - calls must use appropriate
     *   HTML tags instead 
     */
    if (html_mode_)
        return;

    /* check for highlighting */
    if (((attrs & OS_ATTR_HILITE) != 0) != (hilite_mode_ != 0))
    {
        /* remember the new mode */
        hilite_mode_ = ((attrs & OS_ATTR_HILITE) != 0);

        /* add an in-line highlighting mode switch */
        oshtml_display_html_tags(hilite_mode_ ? "<hiliteOn>" : "<hiliteOff>");
    }
}

/*
 *   Set the text color. 
 */
void os_set_text_color(os_color_t, os_color_t)
{
    /* we ignore this - callers must use HTML tags to set colors */
}

/* 
 *   Set the screen color.  
 */
void os_set_screen_color(os_color_t color)
{
    /* we ignore this - callers must use HTML tags to set colors */
}

/* 
 *   use plain ascii mode for the display 
 */
void os_plain()
{
    /* ignore this -- we can only use the HTML mode */
}

/*
 *   write a string
 */
void os_printz(const char *str)
{
    /* use our base counted-length writer */
    os_print(str, strlen(str));
}

void os_print(const char *str, size_t len)
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* check our status mode */
    switch(status_mode_)
    {
    case 0:
        /* display the entire string normally */
        break;

    case 1:
        /* skip any leading newlines */
        for ( ; len != 0 && *str == '\n' ; ++str, --len) ;

        /* stop at the first newline after any other characters */
        if (len != 0)
        {
            const char *p;
            size_t rem;

            /* scan for a newline */
            for (p = str, rem = len ; rem != 0 && *p != '\n' ; ++p, --rem) ;

            /* if we found one, note it */
            if (rem != 0 && *p == '\n')
            {
                /* switch to status mode 2 for subsequent output */
                status_mode_ = 2;

                /* display only the part before the newline */
                len = p - str;
            }
        }
        break;

    case 2:
    default:
        /* 
         *   suppress everything in status mode 2 - this is the part after
         *   the initial line of status text, which is hidden until we
         *   explicitly return to the main text area by switching to status
         *   mode 0 
         */
        return;
    }

    /* if app frame, we can't display anything */
    if (app_frame == 0)
        return;

    /* display the string */
    app_frame->display_output(str, len);
}

/*
 *   show the MORE prompt and wait for the user to respond
 */
void os_more_prompt()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if app frame, we can't do this */
    if (app_frame == 0)
        return;

    /* flush the output, and make sure it's displayed */
    app_frame->flush_txtbuf(TRUE, FALSE);

    /* display the MORE prompt */
    app_frame->pause_for_more();
}

/* 
 *   pause prior to exit 
 */
void os_expause()
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* if app frame, we can't do this */
    if (app_frame == 0)
        return;

    /* flush the output, and make sure it's displayed */
    app_frame->flush_txtbuf(TRUE, FALSE);

    /* display an exit message in the status line, and wait for a key */
    app_frame->pause_for_exit();
}

/*
 *   printf to debug log window
 */
void oshtml_dbg_printf(const char *fmt, ...)
{
    va_list argptr;

    va_start(argptr, fmt);
    oshtml_dbg_vprintf(fmt, argptr);
    va_end(argptr);
}

void oshtml_dbg_vprintf(const char *fmt, va_list argptr)
{
    char buf[1024];
    char *p;
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();

    /* format the message */
    vsprintf(buf, fmt, argptr);

    /* 
     *   Remove any bold on/off sequences from the buffer.  Bold sequences
     *   are interpreted by the HTML parser as tag open/close sequences,
     *   so they can cause weird problems. 
     */
    for (p = buf ; *p != '\0' ; ++p)
    {
        /* if it's a bold on/off sequence, convert it to a space */
        if (*p == 1 || *p == 2)
            *p = ' ';
    }

    /* display it */
    if (app_frame != 0)
    {
        app_frame->dbg_print(buf);
    }
#ifdef TADSHTML_DEBUG
    else
    {
        /* no app frame - use the system debug console instead */
        os_dbg_sys_msg(buf);
    }
#endif /* TADSHTML_DEBUG */
}

/*
 *   We can ignore title settings from the high-level output formatter
 *   layer, since we parse the title tag ourselves in the underlying HTML
 *   layer.  
 */
void os_set_title(const char *)
{
    /* nothing to do */
}


/*
 *   HTML 4 character set translation.  This translates to the plain ASCII
 *   7-bit character set.  For the HTML interpreter, we only need this for
 *   translations to log files, since we handle the display in the HTML
 *   layer.  
 */
void os_xlat_html4(unsigned int html4_char, char *result, size_t result_len)
{
    /* default character to use for unknown charaters */
#define INV_CHAR "?"

    /* 
     *   Translation table - provides mappings for characters the ISO
     *   Latin-1 subset of the HTML 4 character map (values 128-255).
     *   
     *   Characters marked "(approx)" are approximations where the actual
     *   desired character is not available in the DOS console character
     *   set, but a reasonable approximation is used.  Characters marked
     *   "(approx unaccented)" are accented characters that are not
     *   available; these use the unaccented equivalent as an
     *   approximation, since this will presumably convey more meaning
     *   than a blank.
     *   
     *   Characters marked "(n/a)" have no equivalent (even approximating)
     *   in the DOS character set, and are mapped to spaces.
     *   
     *   Characters marked "(not used)" are not used by HTML '&' markups.  
     */
    static const char *xlat_tbl[] =
    {
        INV_CHAR,                                         /* 128 (not used) */
        INV_CHAR,                                         /* 129 (not used) */
        "'",                                        /* 130 - sbquo (approx) */
        INV_CHAR,                                         /* 131 (not used) */
        "\"",                                       /* 132 - bdquo (approx) */
        INV_CHAR,                                         /* 133 (not used) */
        INV_CHAR,                                     /* 134 - dagger (n/a) */
        INV_CHAR,                                     /* 135 - Dagger (n/a) */
        INV_CHAR,                                         /* 136 (not used) */
        INV_CHAR,                                     /* 137 - permil (n/a) */
        INV_CHAR,                                         /* 138 (not used) */
        "<",                                       /* 139 - lsaquo (approx) */
        INV_CHAR,                                      /* 140 - OElig (n/a) */
        INV_CHAR,                                         /* 141 (not used) */
        INV_CHAR,                                         /* 142 (not used) */
        INV_CHAR,                                         /* 143 (not used) */
        INV_CHAR,                                         /* 144 (not used) */
        "'",                                        /* 145 - lsquo (approx) */
        "'",                                        /* 146 - rsquo (approx) */
        "\"",                                       /* 147 - ldquo (approx) */
        "\"",                                       /* 148 - rdquo (approx) */
        INV_CHAR,                                         /* 149 (not used) */
        "-",                                                /* 150 - endash */
        "--",                                               /* 151 - emdash */
        INV_CHAR,                                         /* 152 (not used) */
        "(tm)",                                     /* 153 - trade (approx) */
        INV_CHAR,                                         /* 154 (not used) */
        ">",                                       /* 155 - rsaquo (approx) */
        INV_CHAR,                                      /* 156 - oelig (n/a) */
        INV_CHAR,                                         /* 157 (not used) */
        INV_CHAR,                                         /* 158 (not used) */
        "Y",                              /* 159 - Yuml (approx unaccented) */
        INV_CHAR,                                         /* 160 (not used) */
        "!",                                                 /* 161 - iexcl */
        "c",                                                  /* 162 - cent */
        "#",                                                 /* 163 - pound */
        INV_CHAR,                                     /* 164 - curren (n/a) */
        INV_CHAR,                                              /* 165 - yen */
        "|",                                                /* 166 - brvbar */
        INV_CHAR,                                       /* 167 - sect (n/a) */
        INV_CHAR,                                        /* 168 - uml (n/a) */
        "(c)",                                       /* 169 - copy (approx) */
        INV_CHAR,                                             /* 170 - ordf */
        "<<",                                                /* 171 - laquo */
        INV_CHAR,                                              /* 172 - not */
        " ",                                             /* 173 - shy (n/a) */
        "(R)",                                        /* 174 - reg (approx) */
        INV_CHAR,                                       /* 175 - macr (n/a) */
        INV_CHAR,                                              /* 176 - deg */
        "+/-",                                              /* 177 - plusmn */
        INV_CHAR,                                             /* 178 - sup2 */
        INV_CHAR,                                       /* 179 - sup3 (n/a) */
        "'",                                        /* 180 - acute (approx) */
        INV_CHAR,                                            /* 181 - micro */
        INV_CHAR,                                       /* 182 - para (n/a) */
        INV_CHAR,                                           /* 183 - middot */
        ",",                                        /* 184 - cedil (approx) */
        INV_CHAR,                                       /* 185 - sup1 (n/a) */
        INV_CHAR,                                             /* 186 - ordm */
        ">>",                                                /* 187 - raquo */
        "1/4",                                             /* 188 - frac14 */
        "1/2",                                             /* 189 - frac12 */
        "3/4",                                     /* 190 - frac34 (approx) */
        "?",                                                /* 191 - iquest */
        "A",                            /* 192 - Agrave (approx unaccented) */
        "A",                            /* 193 - Aacute (approx unaccented) */
        "A",                             /* 194 - Acirc (approx unaccented) */
        "A",                            /* 195 - Atilde (approx unaccented) */
        "A",                                                  /* 196 - Auml */
        "A",                                                 /* 197 - Aring */
        "AE",                                                /* 198 - AElig */
        "C",                                                /* 199 - Ccedil */
        "E",                            /* 200 - Egrave (approx unaccented) */
        "E",                                                /* 201 - Eacute */
        "E",                             /* 202 - Ecirc (approx unaccented) */
        "E",                              /* 203 - Euml (approx unaccented) */
        "I",                            /* 204 - Igrave (approx unaccented) */
        "I",                            /* 205 - Iacute (approx unaccented) */
        "I",                             /* 206 - Icirc (approx unaccented) */
        "I",                              /* 207 - Iuml (approx unaccented) */
        INV_CHAR,                                        /* 208 - ETH (n/a) */
        "N",                                                /* 209 - Ntilde */
        "O",                            /* 210 - Ograve (approx unaccented) */
        "O",                            /* 211 - Oacute (approx unaccented) */
        "O",                             /* 212 - Ocirc (approx unaccented) */
        "O",                            /* 213 - Otilde (approx unaccented) */
        "O",                                                  /* 214 - Ouml */
        "x",                                        /* 215 - times (approx) */
        "O",                            /* 216 - Oslash (approx unaccented) */
        "U",                            /* 217 - Ugrave (approx unaccented) */
        "U",                            /* 218 - Uacute (approx unaccented) */
        "U",                             /* 219 - Ucirc (approx unaccented) */
        "U",                                                  /* 220 - Uuml */
        "Y",                            /* 221 - Yacute (approx unaccented) */
        INV_CHAR,                                      /* 222 - THORN (n/a) */
        INV_CHAR,                                   /* 223 - szlig (approx) */
        "a",                                                /* 224 - agrave */
        "a",                                                /* 225 - aacute */
        "a",                                                 /* 226 - acirc */
        "a",                            /* 227 - atilde (approx unaccented) */
        "a",                                                  /* 228 - auml */
        "a",                                                 /* 229 - aring */
        "ae",                                                /* 230 - aelig */
        "c",                                                /* 231 - ccedil */
        "e",                                                /* 232 - egrave */
        "e",                                                /* 233 - eacute */
        "e",                                                 /* 234 - ecirc */
        "e",                                                  /* 235 - euml */
        "i",                                                /* 236 - igrave */
        "i",                                                /* 237 - iacute */
        "i",                                                 /* 238 - icirc */
        "i",                                                  /* 239 - iuml */
        INV_CHAR,                                        /* 240 - eth (n/a) */
        "n",                                                /* 241 - ntilde */
        "o",                                                /* 242 - ograve */
        "o",                                                /* 243 - oacute */
        "o",                                                 /* 244 - ocirc */
        "o",                            /* 245 - otilde (approx unaccented) */
        "o",                                                  /* 246 - ouml */
        INV_CHAR,                                           /* 247 - divide */
        "o",                            /* 248 - oslash (approx unaccented) */
        "u",                                                /* 249 - ugrave */
        "u",                                                /* 250 - uacute */
        "u",                                                 /* 251 - ucirc */
        "u",                                                  /* 252 - uuml */
        "y",                            /* 253 - yacute (approx unaccented) */
        INV_CHAR,                                      /* 254 - thorn (n/a) */
        "y"                                                   /* 255 - yuml */
    };

    /*
     *   Map certain extended characters into our 128-255 range.  If we
     *   don't recognize the character, return the default invalid
     *   charater value.  
     */
    if (html4_char > 255)
    {
        switch(html4_char)
        {
        case 338: html4_char = 140; break;
        case 339: html4_char = 156; break;
        case 376: html4_char = 159; break;
        case 352: html4_char = 154; break;
        case 353: html4_char = 138; break;
        case 8211: html4_char = 150; break;
        case 8212: html4_char = 151; break;
        case 8216: html4_char = 145; break;
        case 8217: html4_char = 146; break;
        case 8218: html4_char = 130; break;
        case 8220: html4_char = 147; break;
        case 8221: html4_char = 148; break;
        case 8222: html4_char = 132; break;
        case 8224: html4_char = 134; break;
        case 8225: html4_char = 135; break;
        case 8240: html4_char = 137; break;
        case 8249: html4_char = 139; break;
        case 8250: html4_char = 155; break;
        case 8482: html4_char = 153; break;

        default:
            /* unmappable character */
            result[0] = INV_CHAR[0];
            result[1] = '\0';
            return;
        }
    }
    
    /* 
     *   if the character is in the regular ASCII zone, return it
     *   untranslated 
     */
    if (html4_char < 128)
    {
        result[0] = (unsigned char)html4_char;
        result[1] = '\0';
        return;
    }

    /* look up the character in our table and return the translation */
    strcpy(result, xlat_tbl[html4_char - 128]);
}

/* ------------------------------------------------------------------------ */
/*
 *   External banner interface 
 */

/*
 *   create an external banner window 
 */
void *os_banner_create(void *parent_handle, int where, void *other_handle,
                       int wintype, int align, int siz, int siz_units,
                       unsigned long style)
{
    CHtmlFormatterBannerExt *parent_fmt;
    CHtmlFormatterBannerExt *other_fmt;

    /* 
     *   the 'parent' and 'other' handles are simply CHtmlFormatterBannerExt
     *   pointers 
     */
    parent_fmt = (CHtmlFormatterBannerExt *)parent_handle;
    other_fmt = (CHtmlFormatterBannerExt *)other_handle;

    /* 
     *   Create and return the new formatter.  Simply use the pointer to the
     *   CHtmlFormatterBannerExt object as the handle.  
     */
    return CHtmlFormatterBannerExt::create_extern_banner(
        parent_fmt, where, other_fmt, wintype,
        (HTML_BannerWin_Pos_t)align, siz, siz_units, style);
}

/*
 *   delete an external banner window 
 */
void os_banner_delete(void *banner_handle)
{
    CHtmlFormatterBannerExt *banner_fmt;

    /* the banner handle is the CHtmlFormatterBannerExt object */
    banner_fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* delete the banner */
    CHtmlFormatterBannerExt::delete_extern_banner(banner_fmt);
}

/*
 *   orphan an external banner window 
 */
void os_banner_orphan(void *banner_handle)
{
    CHtmlSysFrame *app_frame = CHtmlSysFrame::get_frame_obj();
    CHtmlFormatterBannerExt *banner_fmt;

    /* if there's no app frame, simply delete the banner */
    if (app_frame == 0)
    {
        os_banner_delete(banner_handle);
        return;
    }

    /* the banner handle is the CHtmlFormatterBannerExt object */
    banner_fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* let the application frame object handle it */
    app_frame->orphan_banner_window(banner_fmt);
}

/*
 *   display output on a banner 
 */
void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* ask the formatter to handle it */
    fmt->add_source_text(txt, len);
}

/*
 *   flush output on a banner 
 */
void os_banner_flush(void *banner_handle)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* ask the formatter to handle it */
    fmt->flush_txtbuf(TRUE);
}

/*
 *   set the banner size 
 */
void os_banner_set_size(void *banner_handle, int siz, int siz_units,
                        int is_advisory)
{
    CHtmlFormatterBannerExt *fmt;

    /* 
     *   If this is advisory only, ignore it - we fully support
     *   size-to-contents, which the caller is indicating they will
     *   eventually call, so there's no need to set this estimated size.
     *   It's actually better for us not to set the estimated size, because
     *   doing so could redundantly resize the window, making for excessive
     *   redrawing. 
     */
    if (is_advisory)
        return;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* set the size */
    fmt->set_banner_size(siz, siz_units);
}

/*
 *   set the banner to the size of its current contents in one or both
 *   dimension 
 */
void os_banner_size_to_contents(void *banner_handle)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* make sure we've flushed any buffered text */
    fmt->flush_txtbuf(TRUE);

    /* size the window to its current contents */
    fmt->size_to_contents(TRUE, TRUE);
}

/*
 *   turn HTML mode on in the banner 
 */
void os_banner_start_html(void *banner_handle)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* flush buffered text */
    os_banner_flush(banner_handle);

    /* turn on HTML parsing in the banner */
    fmt->set_html_mode(TRUE);
}

/*
 *   turn HTML mode off in the banner 
 */
void os_banner_end_html(void *banner_handle)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* flush buffered text */
    os_banner_flush(banner_handle);

    /* turn off HTML parsing in the banner */
    fmt->set_html_mode(FALSE);
}

/*
 *   Set an attribute in the banner 
 */
void os_banner_set_attr(void *banner_handle, int attrs)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* set the attribute in the banner */
    fmt->set_banner_text_attr(attrs);
}

/* 
 *   Set the text color in a banner, for subsequent text displays.  We'll
 *   ignore this in HTML mode, as HTML must be used instead.  
 */
void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* set the color in the banner */
    fmt->set_banner_text_color(fg, bg);
}

/* 
 *   Set the screen color in the banner.  Ignore this in the HTML
 *   interpreter: HTML formatting must be used.  
 */
void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* set the screen color in the banner */
    fmt->set_banner_screen_color(color);
}

/*
 *   Clear a banner 
 */
void os_banner_clear(void *banner_handle)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* flush any buffered text */
    os_banner_flush(banner_handle);

    /* clear the formatter */
    fmt->clear_contents();
}

/*
 *   Get the width and height of the banner, in character cells.  This is not
 *   meaningful for HTML mode, because the HTML interpreters can use
 *   proportional fonts and different fonts of different rendered sizes.  
 */
int os_banner_get_charwidth(void *) { return 0; }
int os_banner_get_charheight(void *) { return 0; }

/*
 *   Get information on the banner 
 */
int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    CHtmlFormatterBannerExt *fmt;
    HTML_BannerWin_Pos_t pos;
    CHtmlSysWin *win;
    CHtmlPoint siz;
    CHtmlRect margins;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* if there's no window, there's no information */
    if ((win = fmt->get_win()) == 0)
        return FALSE;

    /* get the information from the window */
    win->get_banner_info(&pos, &info->style);

    /* get the pixel width and height of the window */
    info->pix_width = win->get_disp_width();
    info->pix_height = win->get_disp_height();

    /* measure the size of a "0" character in the default font */
    siz = win->measure_text(fmt->get_font(), "0", 1, 0);

    /* get the formatter's margins */
    margins = fmt->get_phys_margins();

    /* 
     *   Approximate the character size of the window using the "0" size.
     *   Don't count the formatter's margins in the row/column size, since
     *   what we really want to know is how much text we could fit in the
     *   window without any scrolling.  
     */
    info->rows = (info->pix_height - margins.top - margins.bottom) / siz.y;
    info->columns = (info->pix_width - margins.left - margins.right) / siz.x;

    /* 
     *   perform the trivial type conversion from HTML_BannerWin_Pos_t to
     *   the OS_BANNER_ALIGN_xxx system (the identifiers correspond directly
     *   between the two systems; the HTML subsystem's 'enum' is purely
     *   cosmetic) 
     */
    info->align = (int)pos;

    /* 
     *   it's an HTML window, so it does its own line wrapping at the OS
     *   level; the caller should not do line wrapping on text sent our way 
     */
    info->os_line_wrap = TRUE;

    /* indicate that we filled in the information */
    return TRUE;
}

/*
 *   Set the output coordinates in a text grid banner 
 */
void os_banner_goto(void *banner_handle, int row, int col)
{
    CHtmlFormatterBannerExt *fmt;

    /* the banner handle is simply the CHtmlFormatterBannerExt pointer */
    fmt = (CHtmlFormatterBannerExt *)banner_handle;

    /* flush any buffered text */
    os_banner_flush(banner_handle);

    /* move the cursor */
    fmt->set_cursor_pos(row, col);
}
