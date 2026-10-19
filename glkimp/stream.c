#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "glkimp.h"
#include "gi_blorb.h"

static stream_t *gli_streamlist = NULL; /* linked list of all streams */
static stream_t *gli_currentstr = NULL; /* the current output stream */

stream_t *gli_new_stream(int type, int readable, int writable,
                         glui32 rock)
{
    stream_t *str = (stream_t *)malloc(sizeof(stream_t));
    if (!str)
        return NULL;

    str->tag = generate_tag(); /* For serialization */


    str->type = type;
    str->rock = rock;

    str->unicode = FALSE;
    str->isbinary = FALSE;
    str->resfilenum= 0;

    str->win = NULL;
    str->file = NULL;
    str->filename = NULL;
    str->lastop = 0;
    str->buf = NULL;
    str->bufptr = NULL;
    str->bufend = NULL;
    str->bufeof = NULL;
    str->ubuf = NULL;
    str->ubufptr = NULL;
    str->ubufend = NULL;
    str->ubufeof = NULL;
    str->buflen = 0;
    str->arrayrock.num = 0;

    str->readcount = 0;
    str->writecount = 0;
    str->readable = readable;
    str->writable = writable;

    str->prev = NULL;
    str->next = gli_streamlist;
    gli_streamlist = str;
    if (str->next) {
        str->next->prev = str;
    }

    if (gli_register_obj)
        str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
    else
        str->disprock.ptr = NULL;

    return str;
}

void gli_delete_stream(stream_t *str)
{
    stream_t *prev, *next;

    if (str == gli_currentstr) {
        gli_currentstr = NULL;
    }

    gli_windows_unechostream(str);


    switch (str->type) {
        case strtype_Window:
            /* nothing necessary; the window is already being closed */
            break;
        case strtype_Memory:
            if (gli_unregister_arr) {
                /* This could be a char array or a glui32 array. */
                char *typedesc = (str->unicode ? "&+#!Iu" : "&+#!Cn");
                void *buf = (str->unicode ? (void*)str->ubuf : (void*)str->buf);
                (*gli_unregister_arr)(buf, str->buflen, typedesc,
                                      str->arrayrock);
            }
            break;
        case strtype_Resource:
            /* nothing necessary; the array belongs to gi_blorb.c. */
            break;
        case strtype_File:
            /* close the FILE */
            if (str->file)
                fclose(str->file);
            str->file = NULL;
            if (str->filename)
                free(str->filename);
            str->filename = NULL;
            str->lastop = 0;
            break;
    }

    if (gli_unregister_obj) {
        (*gli_unregister_obj)(str, gidisp_Class_Stream, str->disprock);
        str->disprock.ptr = NULL;
    }

    prev = str->prev;
    next = str->next;
    str->prev = NULL;
    str->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_streamlist = next;
    if (next)
        next->prev = prev;

    free(str);
}

void gli_stream_fill_result(stream_t *str, stream_result_t *result)
{
    if (!result)
        return;

    result->readcount = str->readcount;
    result->writecount = str->writecount;
}

void glk_stream_close(stream_t *str, stream_result_t *result)
{
    if (!str) {
        gli_strict_warning("stream_close: invalid ref.");
        return;
    }

    if (str->type == strtype_Window) {
        gli_strict_warning("stream_close: cannot close window stream");
        return;
    }

    gli_stream_fill_result(str, result);
    gli_delete_stream(str);
}

void gli_close_all_file_streams(void)
{
    stream_t *stream;

    for (stream = glk_stream_iterate(NULL, NULL); stream; stream = glk_stream_iterate(stream, NULL))
    {
        if (stream->type == strtype_File && stream->file) {
            fclose(stream->file);
        }
    }
}

strid_t glk_stream_open_memory(char *buf, glui32 buflen, glui32 fmode,
                               glui32 rock)
{
    stream_t *str;

    if (fmode != filemode_Read
        && fmode != filemode_Write
        && fmode != filemode_ReadWrite) {
        gli_strict_warning("stream_open_memory: illegal filemode");
        return NULL;
    }

    str = gli_new_stream(strtype_Memory,
                         (fmode != filemode_Write),
                         (fmode != filemode_Read),
                         rock);
    if (!str) {
        gli_strict_warning("stream_open_memory: unable to create stream.");
        return NULL;
    }

    if (buf && buflen) {
        str->buf = (unsigned char *)buf;
        str->bufptr = (unsigned char *)buf;
        str->buflen = buflen;
        str->bufend = str->buf + str->buflen;
        if (fmode == filemode_Write)
            str->bufeof = str->buf;
        else
            str->bufeof = str->bufend;
        if (gli_register_arr) {
            str->arrayrock = (*gli_register_arr)(buf, buflen, "&+#!Cn");
        }
    }

    return str;
}

static strid_t gli_stream_open_file(fileref_t *fref, glui32 fmode,
                                   glui32 rock, int unicode)
{
    stream_t *str;
    char modestr[16];
    FILE *fl;

    if (!fref) {
        gli_strict_warning("stream_open_file: invalid fileref id.");
        return NULL;
    }

    /* The spec says that Write, ReadWrite, and WriteAppend create the
     file if necessary. However, fopen(filename, "r+") doesn't create
     a file. So we have to pre-create it in the ReadWrite and
     WriteAppend cases. (We use "a" so as not to truncate, and "b"
     because we're going to close it immediately, so it doesn't matter.) */

    /* Another Unix quirk: in r+ mode, you're not supposed to flip from
     reading to writing or vice versa without doing an fseek. We will
     track the most recent operation (as lastop) -- Write, Read, or
     0 if either is legal next. */

    if (fmode == filemode_ReadWrite || fmode == filemode_WriteAppend) {
        fl = fopen(fref->filename, "ab");
        if (!fl) {
            gli_strict_warning("stream_open_file: unable to open file.");
            return NULL;
        }

        fclose(fl);
    }

    switch (fmode) {
        case filemode_Write:
            strcpy(modestr, "w");
            break;
        case filemode_Read:
            strcpy(modestr, "r");
            break;
        case filemode_ReadWrite:
            strcpy(modestr, "r+");
            break;
        case filemode_WriteAppend:
            /* Can't use "a" here, because then fseek wouldn't work.
             Instead we use "r+" and then fseek to the end. */
            strcpy(modestr, "r+");
            break;
        default:
            gli_strict_warning("stream_open_file: illegal filemode");
            return NULL;
    }

    if (!fref->textmode)
        strcat(modestr, "b");

    fl = fopen(fref->filename, modestr);
    if (!fl) {
        gli_strict_warning("stream_open_file: unable to open file.");
        return NULL;
    }

    if (fmode == filemode_WriteAppend) {
        fseek(fl, 0, SEEK_END);
    }

    str = gli_new_stream(strtype_File,
                         (fmode == filemode_Read || fmode == filemode_ReadWrite),
                         !(fmode == filemode_Read),
                         rock);
    if (!str) {
        gli_strict_warning("stream_open_file: unable to create stream.");
        fclose(fl);
        return NULL;
    }

    str->unicode = unicode;
    str->isbinary = !fref->textmode;
    str->file = fl;
    str->filename = strdup(fref->filename);

    str->lastop = 0;

    return str;
}

strid_t glk_stream_open_file(fileref_t *fref, glui32 fmode,
                             glui32 rock)
{
    return gli_stream_open_file(fref, fmode, rock, FALSE);
}

#ifdef GLK_MODULE_UNICODE

strid_t glk_stream_open_memory_uni(glui32 *ubuf, glui32 buflen, glui32 fmode,
                                   glui32 rock)
{
    stream_t *str;

    if (fmode != filemode_Read
        && fmode != filemode_Write
        && fmode != filemode_ReadWrite) {
        gli_strict_warning("stream_open_memory_uni: illegal filemode");
        return NULL;
    }

    str = gli_new_stream(strtype_Memory,
                         (fmode != filemode_Write),
                         (fmode != filemode_Read),
                         rock);
    if (!str) {
        gli_strict_warning("stream_open_memory_uni: unable to create stream.");
        return NULL;
    }

    str->unicode = TRUE;

    if (ubuf && buflen) {
        str->ubuf = ubuf;
        str->ubufptr = ubuf;
        str->buflen = buflen;
        str->ubufend = str->ubuf + str->buflen;
        if (fmode == filemode_Write)
            str->ubufeof = ubuf;
        else
            str->ubufeof = str->ubufend;
        if (gli_register_arr) {
            str->arrayrock = (*gli_register_arr)(ubuf, buflen, "&+#!Iu");
        }
    }

    return str;
}

strid_t glk_stream_open_file_uni(fileref_t *fref, glui32 fmode,
                                 glui32 rock)
{
    return gli_stream_open_file(fref, fmode, rock, TRUE);
}

#endif /* GLK_MODULE_UNICODE */

#ifdef GLK_MODULE_RESOURCE_STREAM

/* A missing blorb file or resource is not an error: the spec has the
   call return NULL, and games probe for optional resources. */
static strid_t gli_stream_open_resource(glui32 filenum, glui32 rock, int unicode)
{
    strid_t str;
    int isbinary;
    giblorb_err_t err;
    giblorb_result_t res;
    giblorb_map_t *map = giblorb_get_resource_map();
    if (!map)
        return NULL; /* Not running from a blorb file */

    err = giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Data, filenum);
    if (err)
    {
        gli_debug_log("stream_open_resource %d: Err %d\n", filenum, err);
        return NULL; /* Not found, or some other error */
    }

    /* We'll use the in-memory copy of the chunk data as the basis for
     our new stream. It's important to not call chunk_unload() until
     the stream is closed (and we won't).

     This will be memory-hoggish for giant data chunks, but I don't
     expect giant data chunks at this point. A more efficient model
     would be to use the file on disk, but this requires some hacking
     into the file stream code (we'd need to open a new FILE*) and
     I don't feel like doing that.

     Note that binary chunks are normally type BINA, but FORM
     chunks also count as binary. (This allows us to embed AIFF
     files as readable resources, for example.) */

    if (res.chunktype == giblorb_ID_TEXT)
        isbinary = FALSE;
    else if (res.chunktype == giblorb_ID_BINA
             || res.chunktype == giblorb_make_id('F', 'O', 'R', 'M'))
        isbinary = TRUE;
    else
    {
        gli_debug_log("stream_open_resource %d: unknown chunk type\n", filenum);
        return NULL;
    }

    str = gli_new_stream(strtype_Resource,
                         TRUE, FALSE, rock);
    if (!str) {
        gli_strict_warning("stream_open_resource: unable to create stream.");
        return NULL;
    }

    str->unicode = unicode;
    str->isbinary = isbinary;
    str->resfilenum = filenum;

    /* For a unicode stream too, we have been handed an array of bytes.
     (They're big-endian four-byte chunks, or perhaps a UTF-8 byte
     sequence, rather than native-endian four-byte integers). So we drop
     it into buf, rather than ubuf -- we'll have to do the translation
     in the get() functions. */

    if (res.data.ptr && res.length) {
        str->buf = (unsigned char *)res.data.ptr;
        str->bufptr = (unsigned char *)res.data.ptr;
        str->buflen = res.length;
        str->bufend = str->buf + str->buflen;
        str->bufeof = str->bufend;
    }

    return str;
}

strid_t glk_stream_open_resource(glui32 filenum, glui32 rock)
{
    return gli_stream_open_resource(filenum, rock, FALSE);
}

strid_t glk_stream_open_resource_uni(glui32 filenum, glui32 rock)
{
    return gli_stream_open_resource(filenum, rock, TRUE);
}

#endif /* GLK_MODULE_RESOURCE_STREAM */

strid_t gli_stream_open_pathname(char *pathname, int writemode,
                                 int textmode, glui32 rock)
{
    char modestr[16];
    stream_t *str;
    FILE *fl;

    strcpy(modestr, writemode ? "w" : "r");
    if (!textmode)
        strcat(modestr, "b");

    fl = fopen(pathname, modestr);
    if (!fl) {
        return NULL;
    }

    str = gli_new_stream(strtype_File,
                         !writemode, writemode, rock);
    if (!str) {
        fclose(fl);
        return NULL;
    }

    str->isbinary = !textmode;
    str->file = fl;
    str->filename = strdup(pathname);

    str->lastop = 0;

    return str;
}

stream_t *gli_stream_open_window(window_t *win)
{
    stream_t *str;

    str = gli_new_stream(strtype_Window, FALSE, TRUE, 0);
    if (!str)
        return NULL;

    str->win = win;

    return str;
}

strid_t glk_stream_iterate(strid_t str, glui32 *rock)
{
    if (!str) {
        str = gli_streamlist;
    }
    else {
        str = str->next;
    }

    if (str) {
        if (rock)
            *rock = str->rock;
        return str;
    }

    if (rock)
        *rock = 0;
    return NULL;
}

glui32 glk_stream_get_rock(stream_t *str)
{
    if (!str) {
        gli_strict_warning("stream_get_rock: invalid ref.");
        return 0;
    }

    return str->rock;
}

void gli_stream_set_current(stream_t *str)
{
    gli_currentstr = str;
}

void glk_stream_set_current(stream_t *str)
{
    gli_stream_set_current(str);
}

strid_t glk_stream_get_current(void)
{
    if (gli_currentstr)
        return gli_currentstr;
    else
        return NULL;
}

void glk_stream_set_position(stream_t *str, glsi32 pos, glui32 seekmode)
{
    if (!str) {
        gli_strict_warning("stream_set_position: invalid ref");
        return;
    }

    switch (str->type) {
        case strtype_Memory:
        case strtype_Resource:
            if (!str->unicode || str->type == strtype_Resource) {
                if (seekmode == seekmode_Current) {
                    pos = (glsi32)(str->bufptr - str->buf) + pos;
                }
                else if (seekmode == seekmode_End) {
                    pos = (glsi32)(str->bufeof - str->buf) + pos;
                }
                else {
                    /* pos = pos */
                }
                if (pos < 0)
                    pos = 0;
                if (pos > (str->bufeof - str->buf))
                    pos = (glsi32)(str->bufeof - str->buf);
                str->bufptr = str->buf + pos;
            }
            else {
                if (seekmode == seekmode_Current) {
                    pos = (glsi32)(str->ubufptr - str->ubuf) + pos;
                }
                else if (seekmode == seekmode_End) {
                    pos = (glsi32)(str->ubufeof - str->ubuf) + pos;
                }
                else {
                    /* pos = pos */
                }
                if (pos < 0)
                    pos = 0;
                if (pos > (str->ubufeof - str->ubuf))
                    pos = (glsi32)(str->ubufeof - str->ubuf);
                str->ubufptr = str->ubuf + pos;
            }
            break;
        case strtype_Window:
            /* do nothing; don't pass to echo stream */
            break;
        case strtype_File:
            /* Either reading or writing is legal after an fseek. */
            str->lastop = 0;
            if (str->unicode && str->isbinary) {
                /* Use 4 here, rather than sizeof(glui32). In a Unicode
                   text (UTF-8) file the position is a byte offset, a
                   magic cookie as far as the game is concerned. */
                pos *= 4;
            }
            if (!str->file) {
                gli_strict_warning("stream_set_position: file stream has no file!");
                return;
            }
            fseek(str->file, pos,
                  ((seekmode == seekmode_Current) ? 1 :
                   ((seekmode == seekmode_End) ? 2 : 0)));
            break;
    }
}

glui32 glk_stream_get_position(stream_t *str)
{
    if (!str) {
        gli_strict_warning("stream_get_position: invalid ref");
        return 0;
    }

    switch (str->type) {
        case strtype_Memory:
        case strtype_Resource:
            if (!str->unicode || str->type == strtype_Resource) {
                return (glui32)(str->bufptr - str->buf);
            }
            else {
                return (glui32)(str->ubufptr - str->ubuf);
            }
        case strtype_File:
            if (!str->file) {
                gli_strict_warning("stream_get_position: file stream has no file!");
                return 0;
            }
            if (str->unicode && str->isbinary) {
                /* Use 4 here, rather than sizeof(glui32). */
                return (glui32)ftell(str->file) / 4;
            }
            return (glui32)ftell(str->file);
        case strtype_Window:
        default:
            return 0;
    }
}

static void gli_stream_ensure_op(stream_t *str, glui32 op)
{
    /* We have to do an fseek() between reading and writing. This will
     only come up for ReadWrite or WriteAppend files. */
    if (str->lastop != 0 && str->lastop != op) {
        long pos = ftell(str->file);
        fseek(str->file, pos, SEEK_SET);
    }
    str->lastop = op;
}

/* Store one character in a memory stream, as '?' if a narrow stream
   cannot hold it. Past the end of the buffer it is dropped. */
static void gli_memory_put_char(stream_t *str, glui32 ch)
{
    if (!str->unicode) {
        if (str->bufptr < str->bufend) {
            *(str->bufptr) = (ch >= 0x100) ? '?' : (unsigned char)ch;
            str->bufptr++;
            if (str->bufptr > str->bufeof)
                str->bufeof = str->bufptr;
        }
    }
    else if (str->ubufptr < str->ubufend) {
        *(str->ubufptr) = ch;
        str->ubufptr++;
        if (str->ubufptr > str->ubufeof)
            str->ubufeof = str->ubufptr;
    }
}

/* Prepare a file stream for writing; FALSE if it has no file */
static int gli_file_ready_to_write(stream_t *str, const char *caller)
{
    if (!str->file) {
        char msg[64];
        snprintf(msg, sizeof msg, "%s: file stream has no file!", caller);
        gli_strict_warning(msg);
        return FALSE;
    }
    gli_stream_ensure_op(str, filemode_Write);
    return TRUE;
}

/* Write one character to a file stream: a byte ('?' if it does not fit),
   UTF-8 (Unicode text) or a big-endian four-byte value (Unicode binary) */
static void gli_file_put_char(stream_t *str, glui32 ch)
{
    if (!str->unicode) {
        putc((ch >= 0x100) ? '?' : (int)ch, str->file);
    }
    else if (!str->isbinary) {
        /* cheap UTF-8 stream */
        gli_putchar_utf8(ch, str->file);
    }
    else {
        /* cheap big-endian stream */
        putc(((ch >> 24) & 0xFF), str->file);
        putc(((ch >> 16) & 0xFF), str->file);
        putc(((ch >>  8) & 0xFF), str->file);
        putc( (ch        & 0xFF), str->file);
    }
}

static void gli_put_char(stream_t *str, unsigned char ch)
{
    if (!str || !str->writable)
        return;

    str->writecount++;

    switch (str->type) {
        case strtype_Memory:
            gli_memory_put_char(str, ch);
            break;
        case strtype_Window:
            /* Unlike the other put functions, this one prints anyway:
               Adrian Mole and Dead Cities print single characters
               during line input (against the spec). */
            if (str->win->line_request)
                gli_strict_warning("put_char: window has pending line request");
            gli_window_put_char(str->win, ch);
            if (str->win->echostr)
                gli_put_char(str->win->echostr, ch);
            break;
        case strtype_File:
            if (gli_file_ready_to_write(str, "put_char"))
                gli_file_put_char(str, ch);
            break;
        case strtype_Resource:
            /* resource streams are never writable */
            break;
    }
}

#ifdef GLK_MODULE_UNICODE

static void gli_put_char_uni(stream_t *str, glui32 ch)
{
    if (!str || !str->writable)
        return;

    str->writecount++;

    switch (str->type) {
        case strtype_Memory:
            gli_memory_put_char(str, ch);
            break;
        case strtype_Window:
            if (str->win->line_request) {
                gli_strict_warning("put_char_uni: window has pending line request");
                break;
            }
            gli_window_put_char(str->win, ch);
            if (str->win->echostr)
                gli_put_char_uni(str->win->echostr, ch);
            break;
        case strtype_File:
            if (gli_file_ready_to_write(str, "put_char_uni"))
                gli_file_put_char(str, ch);
            break;
        case strtype_Resource:
            /* resource streams are never writable */
            break;
    }
}

#endif /* GLK_MODULE_UNICODE */

static void gli_put_buffer(stream_t *str, char *buf, glui32 len)
{
    glui32 lx;

    if (!str || !str->writable)
        return;

    str->writecount += len;

    switch (str->type) {
        case strtype_Memory:
            for (lx=0; lx<len; lx++)
                gli_memory_put_char(str, ((unsigned char *)buf)[lx]);
            break;
        case strtype_Window:
            if (str->win->line_request) {
                gli_strict_warning("put_buffer: window has pending line request");
                break;
            }
            /* Never write to stdout here: that is the pipe to the app. */
            for (lx=0; lx<len; lx++)
                gli_window_put_char(str->win, ((unsigned char *)buf)[lx]);
            if (str->win->echostr)
                gli_put_buffer(str->win->echostr, buf, len);
            break;
        case strtype_File:
            if (!gli_file_ready_to_write(str, "put_buffer"))
                break;
            if (!str->unicode) {
                fwrite(buf, 1, len, str->file);
            }
            else {
                for (lx=0; lx<len; lx++)
                    gli_file_put_char(str, ((unsigned char *)buf)[lx]);
            }
            break;
        case strtype_Resource:
            /* resource streams are never writable */
            break;
    }
}

static void gli_set_style(stream_t *str, glui32 val)
{
    if (!str || !str->writable)
        return;

    if (val >= style_NUMSTYLES)
        val = 0;

    switch (str->type)
    {
        case strtype_Window:
            str->win->style = val;
            if (str->win->echostr)
                gli_set_style(str->win->echostr, val);
            break;
    }
}

void gli_stream_echo_line(stream_t *str, char *buf, glui32 len)
{
    /* This is only used to echo line input to an echo stream. See
     glk_select(). */
    gli_put_buffer(str, buf, len);
    gli_put_char(str, '\n');
}

#ifdef GLK_MODULE_UNICODE

void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len)
{
    glui32 ix;
    /* This is only used to echo line input to an echo stream. See
     glk_select(). */
    for (ix=0; ix<len; ix++) {
        gli_put_char_uni(str, buf[ix]);
    }
    gli_put_char(str, '\n');
}

#else

void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len)
{
    gli_strict_warning("stream_echo_line_uni: called with no Unicode line request");
}

#endif /* GLK_MODULE_UNICODE */

/* File streams, and Unicode resource streams, are read a byte at a time
   (a narrow resource stream is read like a memory stream). */
static int gli_read_byte(stream_t *str)
{
    if (str->type == strtype_File)
        return getc(str->file);
    if (str->bufptr >= str->bufend)
        return -1;
    return *(str->bufptr++);
}

/* Read one character from a file or Unicode resource stream: a byte, a
   big-endian four-byte value (Unicode binary) or a UTF-8 sequence (Unicode
   text). Returns FALSE at the end of the stream, or on a truncated
   four-byte value or bad UTF-8. */
static int gli_read_char(stream_t *str, glui32 *chp)
{
    int res;

    if (!str->unicode) {
        res = gli_read_byte(str);
        if (res == -1)
            return FALSE;
        *chp = (glui32)res;
        return TRUE;
    }

    if (str->isbinary) {
        /* cheap big-endian stream */
        glui32 ch = 0;
        for (int ix = 0; ix < 4; ix++) {
            res = gli_read_byte(str);
            if (res == -1)
                return FALSE;
            ch = (ch << 8) | (res & 0xFF);
        }
        *chp = ch;
        return TRUE;
    }

    /* slightly less cheap UTF-8 stream */
    glui32 val0, val1, val2, val3;
    return UTF8_DECODE_INLINE(chp, (res = gli_read_byte(str), res == -1), (res & 0xFF), val0, val1, val2, val3);
}

/* Prepare a file stream for reading; FALSE if it has no file */
static int gli_file_ready_to_read(stream_t *str, const char *caller)
{
    if (!str->file) {
        char msg[64];
        snprintf(msg, sizeof msg, "%s: file stream has no file!", caller);
        gli_strict_warning(msg);
        return FALSE;
    }
    gli_stream_ensure_op(str, filemode_Read);
    return TRUE;
}

/* Store a character read for get_buffer or get_line, as '?' if a narrow
   buffer cannot hold it */
static void gli_store_char(char *cbuf, glui32 *ubuf, glui32 ix, glui32 ch)
{
    if (cbuf)
        cbuf[ix] = (ch >= 0x100) ? '?' : (char)ch;
    else
        ubuf[ix] = ch;
}

static glsi32 gli_get_char(stream_t *str, int want_unicode)
{
    glui32 ch;

    if (!str || !str->readable)
        return -1;

    switch (str->type) {
        case strtype_Resource:
            if (str->unicode)
                break;
            /* for text streams, fall through to memory case */
        case strtype_Memory:
            if (!str->unicode) {
                if (str->bufptr < str->bufend) {
                    unsigned char ch;
                    ch = *(str->bufptr);
                    str->bufptr++;
                    str->readcount++;
                    return ch;
                }
                else {
                    return -1;
                }
            }
            else {
                if (str->ubufptr < str->ubufend) {
                    glui32 ch;
                    ch = *(str->ubufptr);
                    str->ubufptr++;
                    str->readcount++;
                    if (!want_unicode && ch >= 0x100)
                        return '?';
                    return ch;
                }
                else {
                    return -1;
                }
            }
        case strtype_File:
            if (!gli_file_ready_to_read(str, "get_char"))
                return -1;
            break;
        case strtype_Window:
        default:
            return -1;
    }

    if (!gli_read_char(str, &ch))
        return -1;
    str->readcount++;
    if (!want_unicode && ch >= 0x100)
        return '?';
    return (glsi32)ch;
}

static glui32 gli_get_buffer(stream_t *str, char *cbuf, glui32 *ubuf,
                             glui32 len)
{
    if (!str || !str->readable)
        return 0;

    switch (str->type) {
        case strtype_Resource:
            if (str->unicode)
                break;
            /* for text streams, fall through to memory case */
        case strtype_Memory:
            if (!str->unicode) {
                if (str->bufptr >= str->bufend) {
                    len = 0;
                }
                else {
                    if (str->bufptr + len > str->bufend) {
                        glui32 lx;
                        lx = (glui32)((str->bufptr + len) - str->bufend);
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                if (len) {
                    if (cbuf) {
                        memcpy(cbuf, str->bufptr, len);
                    }
                    else {
                        glui32 lx;
                        for (lx=0; lx<len; lx++) {
                            ubuf[lx] = (unsigned char)str->bufptr[lx];
                        }
                    }
                    str->bufptr += len;
                    if (str->bufptr > str->bufeof)
                        str->bufeof = str->bufptr;
                }
            }
            else {
                if (str->ubufptr >= str->ubufend) {
                    len = 0;
                }
                else {
                    if (str->ubufptr + len > str->ubufend) {
                        glui32 lx;
                        lx = (glui32)((str->ubufptr + len) - str->ubufend);
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                if (len) {
                    glui32 lx, ch;
                    if (cbuf) {
                        for (lx=0; lx<len; lx++) {
                            ch = str->ubufptr[lx];
                            if (ch >= 0x100)
                                ch = '?';
                            cbuf[lx] = ch;
                        }
                    }
                    else {
                        for (lx=0; lx<len; lx++) {
                            ubuf[lx] = str->ubufptr[lx];
                        }
                    }
                    str->ubufptr += len;
                    if (str->ubufptr > str->ubufeof)
                        str->ubufeof = str->ubufptr;
                }
            }
            str->readcount += len;
            return len;
        case strtype_File:
            if (!gli_file_ready_to_read(str, "get_buffer"))
                return 0;
            if (!str->unicode && cbuf) {
                glui32 res;
                res = (glui32)fread(cbuf, 1, len, str->file);
                str->readcount += res;
                return res;
            }
            break;
        case strtype_Window:
        default:
            return 0;
    }

    glui32 lx, ch;
    for (lx=0; lx<len; lx++) {
        if (!gli_read_char(str, &ch))
            break;
        str->readcount++;
        gli_store_char(cbuf, ubuf, lx, ch);
    }
    return lx;
}

static glui32 gli_get_line(stream_t *str, char *cbuf, glui32 *ubuf,
                           glui32 len)
{
    glui32 lx;
    int gotnewline;

    if (!str || !str->readable)
        return 0;

    switch (str->type) {
        case strtype_Resource:
            if (str->unicode)
                break;
            /* for text streams, fall through to memory case */
        case strtype_Memory:
            if (len == 0)
                return 0;
            len -= 1; /* for the terminal null */
            if (!str->unicode) {
                if (str->bufptr >= str->bufend) {
                    len = 0;
                }
                else {
                    if (str->bufptr + len > str->bufend) {
                        lx = (glui32)((str->bufptr + len) - str->bufend);
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                if (cbuf) {
                    for (lx=0; lx<len && !gotnewline; lx++) {
                        cbuf[lx] = str->bufptr[lx];
                        gotnewline = (cbuf[lx] == '\n');
                    }
                    cbuf[lx] = '\0';
                }
                else {
                    for (lx=0; lx<len && !gotnewline; lx++) {
                        ubuf[lx] = (unsigned char)str->bufptr[lx];
                        gotnewline = (ubuf[lx] == '\n');
                    }
                    ubuf[lx] = '\0';
                }
                str->bufptr += lx;
            }
            else {
                if (str->ubufptr >= str->ubufend) {
                    len = 0;
                }
                else {
                    if (str->ubufptr + len > str->ubufend) {
                        lx = (glui32)((str->ubufptr + len) - str->ubufend);
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                if (cbuf) {
                    for (lx=0; lx<len && !gotnewline; lx++) {
                        glui32 ch;
                        ch = str->ubufptr[lx];
                        if (ch >= 0x100)
                            ch = '?';
                        cbuf[lx] = ch;
                        gotnewline = (ch == '\n');
                    }
                    cbuf[lx] = '\0';
                }
                else {
                    for (lx=0; lx<len && !gotnewline; lx++) {
                        glui32 ch;
                        ch = str->ubufptr[lx];
                        ubuf[lx] = ch;
                        gotnewline = (ch == '\n');
                    }
                    ubuf[lx] = '\0';
                }
                str->ubufptr += lx;
            }
            str->readcount += lx;
            return lx;
        case strtype_File:
            if (!gli_file_ready_to_read(str, "get_line"))
                return 0;
            if (!str->unicode && cbuf) {
                char *res;
                if (len == 0)
                    return 0;
                res = fgets(cbuf, (int)len, str->file);
                if (!res)
                    return 0;
                lx = (glui32)strlen(cbuf);
                str->readcount += lx;
                return lx;
            }
            break;
        case strtype_Window:
        default:
            return 0;
    }

    if (len == 0)
        return 0;
    len -= 1; /* for the terminal null */
    gotnewline = FALSE;
    for (lx=0; lx<len && !gotnewline; lx++) {
        glui32 ch;
        if (!gli_read_char(str, &ch))
            break;
        str->readcount++;
        gli_store_char(cbuf, ubuf, lx, ch);
        gotnewline = (ch == '\n');
    }
    gli_store_char(cbuf, ubuf, lx, '\0');
    return lx;
}

void glk_put_char(unsigned char ch)
{
    gli_put_char(gli_currentstr, ch);
}

void glk_put_char_stream(stream_t *str, unsigned char ch)
{
    if (!str) {
        gli_strict_warning("put_char_stream: invalid ref");
        return;
    }
    gli_put_char(str, ch);
}

void glk_put_string(char *s)
{
    gli_put_buffer(gli_currentstr, s, (glui32)strlen(s));
}

void glk_put_string_stream(stream_t *str, char *s)
{
    if (!str) {
        gli_strict_warning("put_string_stream: invalid ref");
        return;
    }
    gli_put_buffer(str, s, (glui32)strlen(s));
}

void glk_put_buffer(char *buf, glui32 len)
{
    gli_put_buffer(gli_currentstr, buf, len);
}

void glk_put_buffer_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str) {
        gli_strict_warning("put_buffer_stream: invalid ref");
        return;
    }
    gli_put_buffer(str, buf, len);
}

#ifdef GLK_MODULE_UNICODE

static glui32 strlen_uni(glui32 *s)
{
    glui32 length = 0;
    while (*s++) length++;
    return length;
}

void glk_put_char_uni(glui32 ch)
{
    gli_put_char_uni(gli_currentstr, ch);
}

void glk_put_char_stream_uni(stream_t *str, glui32 ch)
{
    if (!str) {
        gli_strict_warning("put_char_stream_uni: invalid ref");
        return;
    }
    gli_put_char_uni(str, ch);
}

void glk_put_string_uni(glui32 *us)
{
    int len = 0;
    glui32 val;

    while (1) {
        val = us[len];
        if (!val)
            break;
        gli_put_char_uni(gli_currentstr, val);
        len++;
    }
}

void glk_put_string_stream_uni(stream_t *str, glui32 *us)
{
    int len = 0;
    glui32 val;

    if (!str) {
        gli_strict_warning("put_string_stream_uni: invalid ref");
        return;
    }

    while (1) {
        val = us[len];
        if (!val)
            break;
        gli_put_char_uni(str, val);
        len++;
    }
}

void glk_put_buffer_uni(glui32 *buf, glui32 len)
{
    glui32 ix;
    for (ix=0; ix<len; ix++) {
        gli_put_char_uni(gli_currentstr, buf[ix]);
    }
}

void glk_put_buffer_stream_uni(stream_t *str, glui32 *buf, glui32 len)
{
    glui32 ix;
    if (!str) {
        gli_strict_warning("put_buffer_stream_uni: invalid ref");
        return;
    }
    for (ix=0; ix<len; ix++) {
        gli_put_char_uni(str, buf[ix]);
    }
}

glsi32 glk_get_char_stream_uni(strid_t str)
{
    if (!str) {
        gli_strict_warning("get_char_stream_uni: invalid ref");
        return -1;
    }
    return gli_get_char(str, 1);
}

glui32 glk_get_buffer_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
    if (!str) {
        gli_strict_warning("get_buffer_stream_uni: invalid ref");
        return 0;
    }
    return gli_get_buffer(str, NULL, buf, len);
}

glui32 glk_get_line_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
    if (!str) {
        gli_strict_warning("get_line_stream_uni: invalid ref");
        return 0;
    }
    return gli_get_line(str, NULL, buf, len);
}

#endif /* GLK_MODULE_UNICODE */

void glk_set_style(glui32 val)
{
    gli_set_style(gli_currentstr, val);
}

void glk_set_style_stream(stream_t *str, glui32 val)
{
    if (!str) {
        gli_strict_warning("set_style_stream: invalid ref");
        return;
    }
    gli_set_style(str, val);
}

glsi32 glk_get_char_stream(stream_t *str)
{
    if (!str) {
        gli_strict_warning("get_char_stream: invalid ref");
        return -1;
    }
    return gli_get_char(str, 0);
}

glui32 glk_get_line_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str) {
        gli_strict_warning("get_line_stream: invalid ref");
        return 0;
    }
    return gli_get_line(str, buf, NULL, len);
}

glui32 glk_get_buffer_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str) {
        gli_strict_warning("get_buffer_stream: invalid ref");
        return 0;
    }
    return gli_get_buffer(str, buf, NULL, len);
}

/* For autorestore */
stream_t *gli_stream_for_tag(int tag)
{

    stream_t *str = gli_streamlist;
    while (str)
    {

        if (str->tag == tag)
            return str;
        str = str->next;
    }
    return NULL;
}

void gli_replace_stream_list(stream_t *newlist) /* Only used by autorestore */
{
    stream_t *str;

    if (!newlist)
    {
        gli_strict_warning("gli_replace_stream_list: invalid ref");
        return;
    }

    /* At the time when this is called, the stream list should be empty */
    while (gli_streamlist)
    {
        str = gli_streamlist;
        gli_debug_log("gli_replace_stream_list: Closed a stream with tag:%d \n",str->tag);
        /* Not glk_stream_close, which refuses window streams */
        gli_delete_stream(str);
    }
    gli_streamlist = newlist;
}

/* A window stream that has no window, or whose window does not know it */
static int gli_window_stream_is_stray(stream_t *str)
{
    return str->type == strtype_Window
        && (!str->win
            || (str->win->str && str->win->str != str && str->win->echostr != str));
}

/* Reports inconsistencies after an autorestore, and deletes stray window
   streams. They are deleted after the walk, as deleting one unlinks it from
   the list. */
void gli_sanity_check_streams(void)
{
    stream_t *str, *next;

    if (gli_currentstr && !gli_stream_for_tag(gli_currentstr->tag))
        fprintf(stderr, "sanity_check: current stream not listed\n");

    for (str = glk_stream_iterate(NULL, NULL); str; str = glk_stream_iterate(str, NULL)) {
        if (!str->type)
            fprintf(stderr, "sanity_check: stream lacks type\n");
        if (!(str->readable || str->writable))
            fprintf(stderr, "sanity_check: stream should be readable or writable\n");

        switch (str->type) {
            case strtype_Window: {
                if (!str->win) {
                    fprintf(stderr, "sanity_check: window stream %d has no window\n", str->tag);
                } else if (!str->win->str) {
                    fprintf(stderr, "sanity_check: window of window stream has no stream\n");
                }
                else if (str->win->str != str && str->win->echostr != str)
                {
                    fprintf(stderr, "sanity_check: window stream does not match stream of window\n");
                    fprintf(stderr, "str(%d)->win(%d)->str(%d)->win(%d)\n",str->tag, str->win->tag, str->win->str->tag, str->win->str->win->tag);
                    if (str->win->echostr)
                        fprintf(stderr, "str(%d)->win(%d)->echostr: %d\n",str->tag, str->win->tag, str->win->echostr->tag);
                    fprintf(stderr, "Deleting.\n");
                }
            }
                break;

            case strtype_File: {
                if (!str->filename)
                    fprintf(stderr, "sanity_check: file stream lacks pathname\n");
                if (!str->file)
                    fprintf(stderr, "sanity_check: file stream lacks file handle\n");
            }
                break;

            case strtype_Memory: {
                // Remember, the buffer may be nil as long as the length limit is zero
                if (str->unicode && str->buf)
                    fprintf(stderr, "sanity_check: memory stream %d is unicode, but has buf\n", str->tag);
                if (!str->unicode && str->ubuf)
                    fprintf(stderr, "sanity_check: memory stream %d is not unicode, but has ubuf\n", str->tag);
                if (!str->buf && !str->ubuf && str->buflen)
                    fprintf(stderr, "sanity_check: memory stream has no buffer, but positive buflen\n");
            }
                break;

            default:
                break;
        }
    }

    for (str = gli_streamlist; str; str = next) {
        next = str->next;
        if (gli_window_stream_is_stray(str))
            gli_delete_stream(str);
    }
}

glui32 garglk_unput_string_count_uni(glui32 *s)
{
    /* Only a window stream has a `win` to retract from -- the current stream
     * may just as well be a transcript file or a memory stream, and win is
     * meaningless (uninitialised) for those. */
    if (gli_currentstr == NULL || gli_currentstr->type != strtype_Window ||
        gli_currentstr->win == NULL || s == NULL)
        return 0;
    int len = strlen_uni(s);
    glui32 result = win_unprint(gli_currentstr->win->peer, (glui32 *)s,  len);
    if (result)
    {
        gli_currentstr->win->str->writecount -= len;
        if (gli_currentstr->win->echostr)
            gli_currentstr->win->echostr->writecount -= len;
    }
    return result;
}

glui32 garglk_unput_string_count(char *s)
{
    /* Not implemented: there is no Latin-1 win_unprint */
    return 0;
}

void garglk_unput_string(char *s)
 {
     garglk_unput_string_count((char *)s);
     return;
 }

void garglk_unput_string_uni(glui32 *s)
{
    garglk_unput_string_count_uni((glui32 *)s);
    return;
}

static void gli_set_zcolors(stream_t *str, glui32 fg, glui32 bg)
{
    if (!str || !str->writable)
        return;

    if (str->type == strtype_Window)
    {
        win_setzcolor(str->win->peer, fg, bg);
    
        if (str->win->echostr)
            gli_set_zcolors(str->win->echostr, fg, bg);
    }   
}

void garglk_set_zcolors_stream(stream_t *str, glui32 fg, glui32 bg)
{
    if (!str)
    {
        gli_strict_warning("garglk_set_zcolors_stream: invalid ref");
        return;
    }
    gli_set_zcolors(str, fg, bg);
}

void garglk_set_zcolors(glui32 fg, glui32 bg)
{
    gli_set_zcolors(gli_currentstr, fg, bg);
}

static void gli_set_reversevideo(stream_t *str, glui32 reverse)
{
    if (!str || !str->writable)
        return;

    if (str->type == strtype_Window)
    {
        win_setreverse(str->win->peer, reverse);
        str->win->attr.reverse = (reverse != 0);
        if (str->win->echostr)
            gli_set_reversevideo(str->win->echostr, reverse);
    }
}

void garglk_set_reversevideo(glui32 reverse)
{
    gli_set_reversevideo(gli_currentstr, reverse);
}

void garglk_set_reversevideo_stream(stream_t *str, glui32 reverse)
{
    if (!str)
    {
        gli_strict_warning("garglk_set_reversevideo_stream: invalid ref");
        return;
    }
    gli_set_reversevideo(str, reverse);
}


#ifdef GLK_MODULE_HYPERLINKS

static void gli_set_hyperlink(stream_t *str, glui32 linkval)
{
    if (!str || !str->writable)
        return;

    switch (str->type)
    {
        case strtype_Window:
            str->win->attr.hyper = linkval;
			win_setlink(str->win->peer, linkval);
            break;
    }
}

void glk_set_hyperlink(glui32 linkval)
{
    gli_set_hyperlink(gli_currentstr, linkval);
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    if (!str)
    {
        gli_strict_warning("set_hyperlink_stream: invalid ref");
        return;
    }
    gli_set_hyperlink(str, linkval);
}

void glk_request_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("request_hyperlink_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
			win->hyper_request = TRUE;
            win_initlink(win->peer);
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("cancel_hyperlink_event: invalid ref");
        return;
    }
    
    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
			win->hyper_request = FALSE;
			win_cancellink(win->peer);
            break;
        default:
            /* do nothing */
            break;
    }
}

#endif /* GLK_MODULE_HYPERLINKS */
