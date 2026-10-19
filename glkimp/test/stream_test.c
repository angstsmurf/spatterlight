/* stream_test.c: exercise glkimp's stream.c without the app.

   Reads and writes every stream backend (memory, file, resource; narrow,
   Unicode text = UTF-8, Unicode binary = big-endian 32-bit) through every
   get/put call, and prints what comes back. The output is compared with
   stream_test.expected; see run_stream_test.sh. The window side of glkimp
   is stubbed out, so window streams are not covered here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "glkimp.h"
#include "gi_blorb.h"

/* Stubs for what stream.c needs from the rest of glkimp */

int gli_error_handling = 0;
gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock) = NULL;

static glui32 tagcount = 1;
glui32 generate_tag(void) { return tagcount++; }
void win_showerror(const char *str) { printf("  [strict warning: %s]\n", str); }
void win_setlink(int name, int val) { }
void win_initlink(int name) { }
void win_cancellink(int name) { }
glui32 win_unprint(int name, glui32 *str, int len) { return 0; }
void win_setzcolor(int name, glui32 fg, glui32 bg) { }
void win_setreverse(int name, int reverse) { }
void gli_windows_unechostream(stream_t *str) { }
void gli_window_put_char(window_t *win, unsigned ch) { }

/* Resources: filenum indexes this table */

typedef struct {
    glui32 chunktype;
    const unsigned char *data;
    glui32 len;
} test_resource_t;

static test_resource_t resources[8];
static int resource_count = 0;
static giblorb_map_t *dummy_map = (giblorb_map_t *)&resources;

giblorb_map_t *giblorb_get_resource_map(void) { return dummy_map; }

giblorb_err_t giblorb_load_resource(giblorb_map_t *map, glui32 method,
                                    giblorb_result_t *res, glui32 usage, glui32 resnum)
{
    if ((int)resnum >= resource_count)
        return giblorb_err_NotFound;
    res->chunknum = resnum;
    res->data.ptr = (void *)resources[resnum].data;
    res->length = resources[resnum].len;
    res->chunktype = resources[resnum].chunktype;
    return giblorb_err_None;
}

static int add_resource(glui32 type, const void *data, glui32 len)
{
    resources[resource_count].chunktype = type;
    resources[resource_count].data = data;
    resources[resource_count].len = len;
    return resource_count++;
}

/* Test data */

/* Latin-1 for narrow streams, with a line longer than the small buffers
   and a last line without a newline */
static const char latin1_text[] = "Hello\nW\xe9rld, longer line\n\nlast";

/* UTF-8: e-acute, euro, an astral character, then a stray continuation
   byte and a truncated sequence at the very end */
static const char utf8_text[] = "Hi\nW\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80!\n\x80x\nend\xe2\x82";

/* Big-endian 32-bit: the same characters, and two stray bytes at the end */
static const unsigned char be32_data[] = {
    0, 0, 0, 'H', 0, 0, 0, '\n',
    0, 0, 0, 0xe9, 0, 0, 0x20, 0xac,
    0, 1, 0xf6, 0x00, 0, 0, 0, '\n',
    0, 0, 0, 'Z',
    0, 0
};

static glui32 uni_mem_data[] = { 'A', 0xe9, '\n', 0x20ac, 0x1f600, '\n', 'z' };

static char tmpdir[] = "/tmp/glkimp-stream-test-XXXXXX";
static int filecount = 0;

static char *write_temp_file(const void *data, size_t len)
{
    static char path[64][1024];
    char *p = path[filecount];
    snprintf(p, sizeof path[0], "%s/f%d", tmpdir, filecount);
    filecount++;
    FILE *fl = fopen(p, "wb");
    if (len)
        fwrite(data, 1, len, fl);
    fclose(fl);
    return p;
}

static fileref_t *make_fileref(char *path, int textmode)
{
    fileref_t *fref = calloc(1, sizeof(fileref_t));
    fref->filename = path;
    fref->textmode = textmode;
    return fref;
}

static void dump_file(const char *path)
{
    FILE *fl = fopen(path, "rb");
    int ch, n = 0;
    printf("  file:");
    while ((ch = getc(fl)) != EOF) {
        printf(" %02x", ch);
        n++;
    }
    printf(" (%d bytes)\n", n);
    fclose(fl);
}

static void print_result(stream_t *str)
{
    stream_result_t res;
    glk_stream_close(str, &res);
    printf("  close: read %u written %u\n", res.readcount, res.writecount);
}

/* Readers */

typedef stream_t *(*opener_t)(void *ctx);

static void read_chars(const char *name, opener_t open, void *ctx)
{
    stream_t *str = open(ctx);
    glsi32 ch;
    printf("%s get_char:", name);
    while ((ch = glk_get_char_stream(str)) != -1)
        printf(" %x", ch);
    printf(" | pos %u\n", glk_stream_get_position(str));
    print_result(str);

    str = open(ctx);
    printf("%s get_char_uni:", name);
    while ((ch = glk_get_char_stream_uni(str)) != -1)
        printf(" %x", ch);
    printf(" | pos %u\n", glk_stream_get_position(str));
    print_result(str);
}

static void read_buffers(const char *name, opener_t open, void *ctx)
{
    static const glui32 sizes[] = { 1, 2, 3, 5, 64 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        glui32 n, len = sizes[i];
        char cbuf[64];
        glui32 ubuf[64];

        stream_t *str = open(ctx);
        printf("%s get_buffer(%u):", name, len);
        while ((n = glk_get_buffer_stream(str, cbuf, len)) > 0) {
            printf(" [");
            for (glui32 j = 0; j < n; j++)
                printf("%s%x", j ? " " : "", (unsigned char)cbuf[j]);
            printf("]");
        }
        printf("\n");
        print_result(str);

        str = open(ctx);
        printf("%s get_buffer_uni(%u):", name, len);
        while ((n = glk_get_buffer_stream_uni(str, ubuf, len)) > 0) {
            printf(" [");
            for (glui32 j = 0; j < n; j++)
                printf("%s%x", j ? " " : "", ubuf[j]);
            printf("]");
        }
        printf("\n");
        print_result(str);
    }
}

static void read_lines(const char *name, opener_t open, void *ctx)
{
    static const glui32 sizes[] = { 1, 2, 4, 64 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        glui32 n, len = sizes[i];
        char cbuf[64];
        glui32 ubuf[64];

        stream_t *str = open(ctx);
        printf("%s get_line(%u):", name, len);
        memset(cbuf, 0x55, sizeof cbuf);
        while ((n = glk_get_line_stream(str, cbuf, len)) > 0) {
            printf(" [");
            for (glui32 j = 0; j < n; j++)
                printf("%s%x", j ? " " : "", (unsigned char)cbuf[j]);
            printf("|%x]", (unsigned char)cbuf[n]);
            memset(cbuf, 0x55, sizeof cbuf);
        }
        printf("\n");
        print_result(str);

        str = open(ctx);
        printf("%s get_line_uni(%u):", name, len);
        while ((n = glk_get_line_stream_uni(str, ubuf, len)) > 0) {
            printf(" [");
            for (glui32 j = 0; j < n; j++)
                printf("%s%x", j ? " " : "", ubuf[j]);
            printf("|%x]", ubuf[n]);
            memset(ubuf, 0x55, sizeof ubuf);
        }
        printf("\n");
        print_result(str);
    }
}

static void read_all(const char *name, opener_t open, void *ctx)
{
    read_chars(name, open, ctx);
    read_buffers(name, open, ctx);
    read_lines(name, open, ctx);
}

typedef struct {
    char *path;
    int textmode;
    int unicode;
} file_ctx_t;

static stream_t *open_file(void *ctx)
{
    file_ctx_t *f = ctx;
    fileref_t *fref = make_fileref(f->path, f->textmode);
    stream_t *str = f->unicode ? glk_stream_open_file_uni(fref, filemode_Read, 0)
                               : glk_stream_open_file(fref, filemode_Read, 0);
    free(fref);
    return str;
}

typedef struct {
    int filenum;
    int unicode;
} res_ctx_t;

static stream_t *open_res(void *ctx)
{
    res_ctx_t *r = ctx;
    return r->unicode ? glk_stream_open_resource_uni(r->filenum, 0)
                      : glk_stream_open_resource(r->filenum, 0);
}

static stream_t *open_mem(void *ctx)
{
    return glk_stream_open_memory((char *)latin1_text, sizeof latin1_text - 1, filemode_Read, 0);
}

static stream_t *open_mem_uni(void *ctx)
{
    return glk_stream_open_memory_uni(uni_mem_data, sizeof uni_mem_data / sizeof uni_mem_data[0], filemode_Read, 0);
}

/* Writers */

static void write_everything(stream_t *str)
{
    static glui32 ustr[] = { 'u', 0xe9, 0x20ac, 0x1f600, 0 };
    glk_put_char_stream(str, 'A');
    glk_put_char_stream(str, 0xe9);
    glk_put_char_stream_uni(str, 0x20ac);
    glk_put_char_stream_uni(str, 0x1f600);
    glk_put_buffer_stream(str, "xy\n", 3);
    glk_put_buffer_stream_uni(str, ustr, 4);
    glk_put_string_stream(str, "s\xe9");
    glk_put_string_stream_uni(str, ustr);
}

static void write_files(void)
{
    static const struct { const char *name; int textmode, unicode; } kinds[] = {
        { "file-narrow-binary", 0, 0 },
        { "file-narrow-text", 1, 0 },
        { "file-uni-binary", 0, 1 },
        { "file-uni-text", 1, 1 },
    };
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        char *path = write_temp_file(NULL, 0);
        fileref_t *fref = make_fileref(path, kinds[i].textmode);
        stream_t *str = kinds[i].unicode ? glk_stream_open_file_uni(fref, filemode_Write, 0)
                                         : glk_stream_open_file(fref, filemode_Write, 0);
        printf("%s write:\n", kinds[i].name);
        write_everything(str);
        printf("  pos %u\n", glk_stream_get_position(str));
        print_result(str);
        dump_file(path);

        /* Append, then read it back through a ReadWrite stream with seeks */
        str = kinds[i].unicode ? glk_stream_open_file_uni(fref, filemode_WriteAppend, 0)
                               : glk_stream_open_file(fref, filemode_WriteAppend, 0);
        glk_put_char_stream(str, 'Q');
        print_result(str);
        dump_file(path);

        str = kinds[i].unicode ? glk_stream_open_file_uni(fref, filemode_ReadWrite, 0)
                               : glk_stream_open_file(fref, filemode_ReadWrite, 0);
        glsi32 a = glk_get_char_stream_uni(str);
        glk_put_char_stream(str, 'W');
        glsi32 b = glk_get_char_stream_uni(str);
        glk_stream_set_position(str, 0, seekmode_End);
        glui32 end = glk_stream_get_position(str);
        glk_stream_set_position(str, kinds[i].unicode && !kinds[i].textmode ? 4 : 1, seekmode_Start);
        glsi32 c = glk_get_char_stream_uni(str);
        printf("  readwrite: %x %x end %u then %x\n", a, b, end, c);
        print_result(str);
        dump_file(path);
        free(fref);
    }
}

static void write_memory(void)
{
    char cbuf[8];
    glui32 ubuf[8];
    stream_t *str;

    for (glui32 len = 4; len <= 8; len += 4) {
        memset(cbuf, '.', sizeof cbuf);
        str = glk_stream_open_memory(cbuf, len, filemode_Write, 0);
        printf("mem-narrow(%u) write:\n", len);
        write_everything(str);
        printf("  pos %u\n", glk_stream_get_position(str));
        print_result(str);
        printf("  buf:");
        for (int i = 0; i < 8; i++)
            printf(" %x", (unsigned char)cbuf[i]);
        printf("\n");

        memset(ubuf, 0, sizeof ubuf);
        str = glk_stream_open_memory_uni(ubuf, len, filemode_Write, 0);
        printf("mem-uni(%u) write:\n", len);
        write_everything(str);
        printf("  pos %u\n", glk_stream_get_position(str));
        print_result(str);
        printf("  buf:");
        for (int i = 0; i < 8; i++)
            printf(" %x", ubuf[i]);
        printf("\n");
    }

    /* A NULL buffer of length 0 counts writes but stores nothing */
    str = glk_stream_open_memory(NULL, 0, filemode_Write, 0);
    printf("mem-null write:\n");
    write_everything(str);
    print_result(str);
}

static void seek_reads(void)
{
    static const struct { const char *name; int textmode, unicode; const void *data; size_t len; } kinds[] = {
        { "file-narrow-binary", 0, 0, latin1_text, sizeof latin1_text - 1 },
        { "file-uni-binary", 0, 1, be32_data, sizeof be32_data },
        { "file-uni-text", 1, 1, utf8_text, sizeof utf8_text - 1 },
    };
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        file_ctx_t f = { write_temp_file(kinds[i].data, kinds[i].len), kinds[i].textmode, kinds[i].unicode };
        stream_t *str = open_file(&f);
        glk_get_char_stream_uni(str);
        glk_get_char_stream_uni(str);
        glui32 p1 = glk_stream_get_position(str);
        glk_stream_set_position(str, -1, seekmode_Current);
        glsi32 a = glk_get_char_stream_uni(str);
        glk_stream_set_position(str, -2, seekmode_End);
        glsi32 b = glk_get_char_stream_uni(str);
        printf("%s seek: pos %u, %x, %x\n", kinds[i].name, p1, a, b);
        print_result(str);
    }

    stream_t *str = open_mem(NULL);
    glk_stream_set_position(str, 3, seekmode_Start);
    glsi32 a = glk_get_char_stream(str);
    glk_stream_set_position(str, -1, seekmode_End);
    glsi32 b = glk_get_char_stream(str);
    glk_stream_set_position(str, 100, seekmode_Start);
    glui32 p = glk_stream_get_position(str);
    printf("mem seek: %x %x clamp %u\n", a, b, p);
    print_result(str);

    res_ctx_t r = { 1, 1 };
    str = open_res(&r);
    glk_stream_set_position(str, 8, seekmode_Start);
    a = glk_get_char_stream_uni(str);
    printf("res-uni-binary seek: %x pos %u\n", a, glk_stream_get_position(str));
    print_result(str);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 1;
    }

    int res_text = add_resource(giblorb_ID_TEXT, utf8_text, sizeof utf8_text - 1);
    int res_bina = add_resource(giblorb_ID_BINA, be32_data, sizeof be32_data);
    int res_latin = add_resource(giblorb_ID_TEXT, latin1_text, sizeof latin1_text - 1);
    add_resource(giblorb_make_id('P', 'N', 'G', ' '), be32_data, 4);
    int res_empty = add_resource(giblorb_ID_BINA, NULL, 0);

    read_all("mem-narrow", open_mem, NULL);
    read_all("mem-uni", open_mem_uni, NULL);

    file_ctx_t f;
    f = (file_ctx_t){ write_temp_file(latin1_text, sizeof latin1_text - 1), 1, 0 };
    read_all("file-narrow-text", open_file, &f);
    f = (file_ctx_t){ write_temp_file(be32_data, sizeof be32_data), 0, 0 };
    read_all("file-narrow-binary", open_file, &f);
    f = (file_ctx_t){ write_temp_file(utf8_text, sizeof utf8_text - 1), 1, 1 };
    read_all("file-uni-text", open_file, &f);
    f = (file_ctx_t){ write_temp_file(be32_data, sizeof be32_data), 0, 1 };
    read_all("file-uni-binary", open_file, &f);
    f = (file_ctx_t){ write_temp_file(NULL, 0), 0, 1 };
    read_all("file-uni-empty", open_file, &f);

    res_ctx_t r;
    r = (res_ctx_t){ res_latin, 0 };
    read_all("res-narrow-text", open_res, &r);
    r = (res_ctx_t){ res_bina, 0 };
    read_all("res-narrow-binary", open_res, &r);
    r = (res_ctx_t){ res_text, 1 };
    read_all("res-uni-text", open_res, &r);
    r = (res_ctx_t){ res_bina, 1 };
    read_all("res-uni-binary", open_res, &r);
    r = (res_ctx_t){ res_empty, 1 };
    read_all("res-uni-empty", open_res, &r);

    printf("res unknown type: %p\n", (void *)glk_stream_open_resource(3, 0));
    printf("res missing: %p\n", (void *)glk_stream_open_resource_uni(99, 0));
    dummy_map = NULL;
    printf("res no map: %p\n", (void *)glk_stream_open_resource(0, 0));
    dummy_map = (giblorb_map_t *)&resources;

    write_files();
    write_memory();
    seek_reads();

    printf("null stream: %d %u %u\n", glk_get_char_stream(NULL),
           glk_get_buffer_stream(NULL, NULL, 0), glk_get_line_stream_uni(NULL, NULL, 0));

    char cmd[1100];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmpdir);
    system(cmd);
    return 0;
}
