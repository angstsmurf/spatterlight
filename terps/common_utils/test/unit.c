/*
 *  unit.c
 *  Unit tests for terps/common_utils: see the Makefile. A small fake Glk stands
 *  in for the window calls of layouttext.c, and a fake reader for the file
 *  system in the companion file search.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "byteorder.h"
#include "common_file_utils.h"
#include "companion_search.h"
#include "fatal.h"
#include "glk_rocks.h"
#include "layouttext.h"
#include "memory_allocation.h"
#include "minmax.h"
#include "randomness.h"
#include "rgbsurface.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        checks++;                                                    \
        if (!(cond)) {                                               \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                              \
        }                                                            \
    } while (0)

static void check_str(const char *what, const char *got, const char *want, int line)
{
    checks++;
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "unit.c:%d: %s: got \"%s\", wanted \"%s\"\n", line, what, got, want);
        failures++;
    }
}

void Fatal(const char *x)
{
    fprintf(stderr, "Fatal: %s\n", x);
    exit(2);
}

/* ---- Fake Glk for ShowTextInTopWindow ---- */

static glui32 fake_top_w, fake_top_h, fake_bottom_h;
static int fake_arranged;
static glui32 fake_method, fake_size;
static char fake_printed[1024];
static winid_t fake_top = (winid_t)&fake_top_w;
static winid_t fake_bottom = (winid_t)&fake_bottom_h;
static winid_t fake_pair = (winid_t)&fake_arranged;

void glk_window_clear(winid_t win) { (void)win; }
void glk_window_move_cursor(winid_t win, glui32 x, glui32 y)
{
    (void)win;
    (void)x;
    (void)y;
}
winid_t glk_window_get_parent(winid_t win)
{
    return win == fake_top ? fake_pair : NULL;
}
void glk_window_get_size(winid_t win, glui32 *w, glui32 *h)
{
    if (w)
        *w = fake_top_w;
    if (h)
        *h = win == fake_top ? fake_top_h : fake_bottom_h;
}
void glk_window_set_arrangement(winid_t win, glui32 method, glui32 size, winid_t key)
{
    (void)key;
    CHECK(win == fake_pair);
    fake_arranged = 1;
    fake_method = method;
    fake_size = size;
}

static void fake_print(winid_t win, const char *text)
{
    CHECK(win == fake_top);
    strcat(fake_printed, "<");
    strcat(fake_printed, text);
    strcat(fake_printed, ">");
}

static int show(const char *text, glui32 width, glui32 height, glui32 bottom)
{
    fake_top_w = width;
    fake_top_h = height;
    fake_bottom_h = bottom;
    fake_arranged = 0;
    fake_size = 0;
    fake_printed[0] = '\0';
    return ShowTextInTopWindow(fake_top, fake_bottom, text, fake_print);
}

static void test_show_text(void)
{
    CHECK(show("one two three", 8, 1, 20) == 1);
    CHECK(fake_arranged && fake_size == 3);
    CHECK(fake_method == (winmethod_Above | winmethod_Fixed));
    check_str("two lines", fake_printed, "<one two\n><three>", __LINE__);

    /* Empty text leaves one row for the delimiter */
    CHECK(show("", 8, 4, 20) == 1);
    CHECK(fake_size == 1);
    check_str("empty", fake_printed, "", __LINE__);

    /* A short bottom window is not squeezed further... */
    CHECK(show("one two three", 8, 1, 2) == 0);
    CHECK(!fake_arranged);
    check_str("not resized", fake_printed, "<one two\n><three>", __LINE__);
    /* ...but top may still shrink, or stay as it is */
    CHECK(show("one", 8, 5, 2) == 1);
    CHECK(fake_size == 2);
}

/* ---- LineBreakText ---- */

static void wrap(const char *source, int columns, const char *want, int wantrows, int line)
{
    int rows = -1;
    char *got = LineBreakText(source, columns, &rows);
    check_str(source ? source : "(NULL)", got, want, line);
    checks++;
    if (rows != wantrows) {
        fprintf(stderr, "unit.c:%d: %d rows, wanted %d\n", line, rows, wantrows);
        failures++;
    }
    free(got);
}

static void test_line_break(void)
{
    wrap(NULL, 10, "", 0, __LINE__);
    wrap("", 10, "", 0, __LINE__);
    wrap("   ", 10, "", 0, __LINE__);
    wrap(" . .", 10, "", 0, __LINE__);
    wrap("  ..Hello", 10, "Hello", 1, __LINE__);
    wrap("abc   ", 10, "abc", 1, __LINE__);
    /* Exactly columns - 1 fits, columns does not */
    wrap("one two", 8, "one two", 1, __LINE__);
    wrap("one two", 7, "one\ntwo", 2, __LINE__);
    wrap("one two three", 8, "one two\nthree", 2, __LINE__);
    /* A word longer than the line is cut */
    wrap("abcdefghij", 5, "abcd\nefgh\nij", 3, __LINE__);
    wrap("ab abcdefghij", 5, "ab\nabcd\nefgh\nij", 4, __LINE__);
    /* Runs of spaces at a break vanish, elsewhere they stay */
    wrap("a  b", 3, "a\nb", 2, __LINE__);
    wrap("a  b", 10, "a  b", 1, __LINE__);
    /* All the line break styles */
    wrap("a\r\nb\rc\nd", 10, "a\nb\nc\nd", 4, __LINE__);
    wrap("a\n\nb", 10, "a\n\nb", 3, __LINE__);
    wrap("a\n\n\n", 10, "a", 1, __LINE__);
    /* Narrow windows: columns <= 2 means one character a line */
    wrap("ab", 2, "a\nb", 2, __LINE__);
    wrap("ab", 1, "a\nb", 2, __LINE__);
    wrap("ab", 0, "a\nb", 2, __LINE__);
    wrap("ab", -3, "a\nb", 2, __LINE__);

    /* Anything goes: no line over the width, right row count, and the
       sanitizer watches the size bound of the result. */
    static const char alphabet[] = "aaaab  ..\r\n";
    srand(1);
    for (int n = 0; n < 20000; n++) {
        char source[64];
        int len = rand() % 63;
        for (int i = 0; i < len; i++)
            source[i] = alphabet[rand() % (sizeof alphabet - 1)];
        source[len] = '\0';
        int columns = rand() % 12;
        size_t width = columns > 2 ? (size_t)columns - 1 : 1;
        int rows;
        char *got = LineBreakText(source, columns, &rows);
        int newlines = 0;
        size_t linelen = 0;
        int ok = strlen(got) <= 2 * (size_t)len;
        for (const char *p = got; *p; p++) {
            if (*p == '\n') {
                newlines++;
                linelen = 0;
            } else if (++linelen > width) {
                ok = 0;
            }
        }
        CHECK(ok);
        CHECK(rows == (*got ? newlines + 1 : 0));
        free(got);
    }
}

/* ---- Companion file search ---- */

static char requests[1024];
static const char *existing;

/* Records the name asked for, and "finds" only the file named existing */
static uint8_t *fake_reader(const char *filename, size_t *size, void *user)
{
    (void)user;
    if (requests[0])
        strcat(requests, "|");
    strcat(requests, filename);
    if (existing && strcmp(filename, existing) == 0) {
        *size = 1;
        return MemAlloc(1);
    }
    return NULL;
}

static void companion(const char *gamefile, int match_char, CompanionFallback fallback,
    const char *found, const char *want, int line)
{
    size_t size = 0;
    CompanionSearch search = { fake_reader, NULL, fallback, match_char };
    requests[0] = '\0';
    existing = found;
    uint8_t *result = FindCompanionFile(gamefile, &search, &size);
    check_str(gamefile, requests, want, line);
    checks++;
    if ((result != NULL) != (found != NULL)) {
        fprintf(stderr, "unit.c:%d: %s: wrong result\n", line, gamefile);
        failures++;
    }
    free(result);
}

static void test_companion_search(void)
{
    companion("/x/Game Side 1.dsk", 0, NULL, NULL, "/x/Game Side 2.dsk", __LINE__);
    companion("/x/Game Side 2.dsk", 0, NULL, NULL, "/x/Game Side 1.dsk", __LINE__);
    companion("/x/game_disk_1.dsk", 0, NULL, NULL, "/x/game_disk_2.dsk", __LINE__);
    /* Letters and words follow the case of the original */
    companion("/x/Game side a.dsk", 0, NULL, NULL, "/x/Game side b.dsk", __LINE__);
    companion("/x/Game SIDE B.dsk", 0, NULL, NULL, "/x/Game SIDE A.dsk", __LINE__);
    companion("/x/Game Disk One.dsk", 0, NULL, NULL, "/x/Game Disk Two.dsk", __LINE__);
    companion("/x/Game disk two.dsk", 0, NULL, NULL, "/x/Game disk one.dsk", __LINE__);
    /* No match */
    companion("/x/Game Side 3.dsk", 0, NULL, NULL, "", __LINE__);
    companion("/x/Game Side x.dsk", 0, NULL, NULL, "", __LINE__);
    companion("/x/Side.dsk", 0, NULL, NULL, "", __LINE__);
    companion("/x/inside1.dsk", 0, NULL, NULL, "", __LINE__);
    /* Only the file name counts, not the folder */
    companion("/x/Side 1/game.dsk", 0, NULL, NULL, "", __LINE__);
    /* The keyword may start the name */
    companion("Side 1.dsk", 0, NULL, NULL, "Side 2.dsk", __LINE__);
    companion("disk a", 0, NULL, NULL, "disk b", __LINE__);
    /* No extension, and bytes above 127 in the name */
    companion("/x/Game Side 1", 0, NULL, NULL, "/x/Game Side 2", __LINE__);
    companion("/x/Sp\xc3\xabl Side 1.dsk", 0, NULL, NULL, "/x/Sp\xc3\xabl Side 2.dsk", __LINE__);
    /* Several patterns are tried from the end until one exists */
    companion("/x/disk 1 side 2.dsk", 0, NULL, NULL, "/x/disk 1 side 1.dsk|/x/disk 2 side 2.dsk", __LINE__);
    companion("/x/disk 1 side 2.dsk", 0, NULL, "/x/disk 1 side 1.dsk", "/x/disk 1 side 1.dsk", __LINE__);

    /* The optional single character before the extension */
    companion("/x/game1.dsk", 0, NULL, NULL, "", __LINE__);
    companion("/x/game1.dsk", 1, NULL, NULL, "/x/game2.dsk", __LINE__);
    companion("/x/game2.dsk", 1, NULL, NULL, "/x/game1.dsk", __LINE__);
    companion("/x/gamea.dsk", 1, NULL, NULL, "/x/gameb.dsk", __LINE__);
    companion("/x/gameB.dsk", 1, NULL, NULL, "/x/gameA.dsk", __LINE__);

    /* Bracket fallback */
    companion("/x/Game Side A[cr CSS].dsk", 0, CompanionBracketFallback, NULL,
        "/x/Game Side B[cr CSS].dsk|/x/Game Side B.dsk", __LINE__);
    companion("/x/Game Side B.dsk", 0, CompanionBracketFallback, "/x/Game Side A[cr CSS].dsk",
        "/x/Game Side A.dsk|/x/Game Side A[cr CSS].dsk", __LINE__);
    companion("/x/Game Side A.dsk", 0, CompanionBracketFallback, NULL, "/x/Game Side B.dsk", __LINE__);
    companion("/x/Game Side 1.dsk", 0, CompanionBracketFallback, NULL, "/x/Game Side 2.dsk", __LINE__);

    /* A name far longer than any stack frame should hold */
    size_t longlen = 1 << 20;
    char *longname = MemAlloc(longlen + 16);
    memset(longname, 'x', longlen);
    strcpy(longname + longlen, " Side 1.dsk");
    existing = NULL;
    requests[0] = '\0';
    size_t size;
    CompanionSearch search = { NULL, NULL, NULL, 0 };
    CHECK(FindCompanionFile(longname, &search, &size) == NULL);
    free(longname);

    /* Bad arguments */
    CHECK(FindCompanionFile(NULL, &search, &size) == NULL);
    CHECK(FindCompanionFile("x", NULL, &size) == NULL);
    CHECK(FindCompanionFile("x", &search, NULL) == NULL);
    CHECK(FindCompanionFile("", &search, &size) == NULL);
}

static void strip(const char *name, char open, char close, int space, int wantret, const char *want, int line)
{
    char buffer[64];
    strcpy(buffer, name);
    int ret = CompanionStripEnclosed(buffer, strlen(buffer), open, close, space);
    checks++;
    if (ret != wantret) {
        fprintf(stderr, "unit.c:%d: %s: returned %d\n", line, name, ret);
        failures++;
    }
    check_str(name, buffer, want, line);
}

static void insert(const char *name, size_t bufsize, int wantret, const char *want, int line)
{
    char buffer[64];
    strcpy(buffer, name);
    int ret = CompanionInsertBeforeExtension(buffer, bufsize, strlen(buffer), "[m]");
    checks++;
    if (ret != wantret) {
        fprintf(stderr, "unit.c:%d: %s: returned %d\n", line, name, ret);
        failures++;
    }
    check_str(name, buffer, want, line);
}

static void test_companion_helpers(void)
{
    strip("file[tag].dsk", '[', ']', 0, 1, "file.dsk", __LINE__);
    strip("file [tag].dsk", '[', ']', 0, 1, "file .dsk", __LINE__);
    strip("file [tag].dsk", '[', ']', 1, 1, "file.dsk", __LINE__);
    strip("file(tag).dsk", '(', ')', 0, 1, "file.dsk", __LINE__);
    strip("a[1]b[2].dsk", '[', ']', 0, 1, "a[1]b.dsk", __LINE__);
    strip("file.dsk", '[', ']', 0, 0, "file.dsk", __LINE__);
    strip("file]x[.dsk", '[', ']', 0, 0, "file]x[.dsk", __LINE__);
    strip("file.d[s]k", '[', ']', 0, 0, "file.d[s]k", __LINE__);
    strip("noextension[tag]", '[', ']', 0, 0, "noextension[tag]", __LINE__);
    /* Brackets and full stops in a folder name are no part of the file name */
    strip("/a[b]/file.dsk", '[', ']', 0, 0, "/a[b]/file.dsk", __LINE__);
    strip("/a[b/file].dsk", '[', ']', 0, 0, "/a[b/file].dsk", __LINE__);
    strip("/a.b[c]/file", '[', ']', 0, 0, "/a.b[c]/file", __LINE__);

    insert("file.dsk", 64, 1, "file[m].dsk", __LINE__);
    insert("a.b.dsk", 64, 1, "a.b[m].dsk", __LINE__);
    insert("noextension", 64, 0, "noextension", __LINE__);
    insert(".hidden", 64, 0, ".hidden", __LINE__);
    insert("", 64, 0, "", __LINE__);
    insert("/dir.v2/file", 64, 0, "/dir.v2/file", __LINE__);
    insert("/dir.v2/file.dsk", 64, 1, "/dir.v2/file[m].dsk", __LINE__);
    /* 8 characters + 3 of marker + the terminating zero */
    insert("file.dsk", 12, 1, "file[m].dsk", __LINE__);
    insert("file.dsk", 11, 0, "file.dsk", __LINE__);
}

/* ---- Files and memory ---- */

static void test_files(void)
{
    char path[] = "/tmp/common_utils_unit_XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd != -1);
    close(fd);

    static const uint8_t bytes[] = { 'a', '\r', '\n', 0x1a, 0, 0xff, 'z' };
    FILE *f = fopen(path, "wb");
    fwrite(bytes, 1, sizeof bytes, f);
    fclose(f);

    size_t size = 12345;
    uint8_t *data = ReadFileIfExists(path, &size);
    CHECK(data != NULL);
    CHECK(size == sizeof bytes);
    CHECK(data && memcmp(data, bytes, sizeof bytes) == 0);
    free(data);

    f = fopen(path, "rb");
    CHECK(GetFileLength(f) == sizeof bytes);
    CHECK(ftell(f) == 0);
    fclose(f);

    /* Empty, missing and unreadable files all leave a size of 0 */
    f = fopen(path, "wb");
    fclose(f);
    size = 12345;
    CHECK(ReadFileIfExists(path, &size) == NULL);
    CHECK(size == 0);
    remove(path);
    size = 12345;
    CHECK(ReadFileIfExists(path, &size) == NULL);
    CHECK(size == 0);
    size = 12345;
    CHECK(ReadFileIfExists("/tmp", &size) == NULL);
    CHECK(size == 0);
}

static void test_memory(void)
{
    void *p = MemAlloc(0);
    CHECK(p != NULL);
    free(p);
    p = MemCalloc(0);
    CHECK(p != NULL);
    free(p);
    p = MemRealloc(NULL, 0);
    CHECK(p != NULL);
    p = MemRealloc(p, 8);
    p = MemRealloc(p, 0);
    CHECK(p != NULL);
    free(p);

    char *copy = MemStrndup("abcdef", 3);
    check_str("MemStrndup", copy, "abc", __LINE__);
    free(copy);

    /* Bytes past a zero are copied too: MemStrndup is no strndup */
    copy = MemStrndup("a\0b", 3);
    CHECK(copy[2] == 'b' && copy[3] == '\0');
    free(copy);
}

/* ---- Small headers ---- */

static void test_byteorder(void)
{
    uint8_t bytes[] = { 0x12, 0x34, 0x56, 0xf8 };
    CHECK(READ_LE_UINT16(bytes) == 0x3412);
    CHECK(READ_BE_UINT16(bytes) == 0x1234);
    CHECK(READ_LE_UINT32(bytes) == 0xf8563412u);
    uint8_t *p = bytes;
    CHECK(READ_LE_UINT16_AND_ADVANCE(&p) == 0x3412);
    CHECK(p == bytes + 2);
    CHECK(READ_BE_UINT16_AND_ADVANCE(&p) == 0x56f8);
    CHECK(p == bytes + 4);
}

static void test_minmax(void)
{
    /* Each of these comes out wrong without the parentheses */
    CHECK(MAX(4 & 6, 1) == 4);
    CHECK(MIN(4 | 1, 3) == 3);
    CHECK(10 * MIN(2, 3) == 20);
    CHECK(MAX(1 ? 2 : 9, 3) == 3);
    CHECK(GLK_BUFFER_ROCK == 1 && GLK_STATUS_ROCK == 1010 && GLK_GRAPHICS_ROCK == 1020);
}

/* ---- Random numbers ---- */

static void test_random(void)
{
    /* Worked out separately from the published xoshiro128** and SplitMix32.
       Game transcripts and autosaves depend on this exact sequence. */
    static const uint32_t seed1[] = { 0x443636e7, 0x4ce753de, 0xd9a8ad4a, 0x2550aade, 0xd5910424 };
    static const uint32_t seed12345[] = { 0x7ae3a926, 0x9b13cc03, 0x24852372, 0xf3170055, 0xf1d3131f };

    set_erkyrath_random(1);
    for (int i = 0; i < 5; i++)
        CHECK(erkyrath_random() == seed1[i]);
    set_erkyrath_random(12345);
    for (int i = 0; i < 5; i++)
        CHECK(erkyrath_random() == seed12345[i]);

    /* A saved state continues the same sequence */
    set_erkyrath_random(1);
    erkyrath_random();
    int usenative;
    uint32_t *arr;
    int count;
    erkyrath_random_get_detstate(&usenative, &arr, &count);
    CHECK(usenative == 0);
    CHECK(count == 4);
    uint32_t saved[4];
    memcpy(saved, arr, sizeof saved);
    set_erkyrath_random(99);
    erkyrath_random();
    erkyrath_random_set_detstate(0, saved, 4);
    for (int i = 1; i < 5; i++)
        CHECK(erkyrath_random() == seed1[i]);

    /* A bad state changes nothing */
    set_erkyrath_random(1);
    erkyrath_random_set_detstate(0, NULL, 4);
    erkyrath_random_set_detstate(0, saved, 3);
    CHECK(erkyrath_random() == seed1[0]);

    /* Seed 0 selects the native generator */
    set_erkyrath_random(0);
    erkyrath_random();
    erkyrath_random_get_detstate(&usenative, &arr, &count);
    CHECK(usenative == 1);
}

/* ---- RGB surface ---- */

enum { SURF_W = 24, SURF_H = 16 };

static void span_paint(void *ctx, unsigned int rgb, int x, int y, int len)
{
    unsigned int *dest = ctx;
    for (int i = 0; i < len; i++) {
        CHECK(dest[y * SURF_W + x + i] == 0xffffffffu);
        dest[y * SURF_W + x + i] = rgb;
    }
}

static void rect_paint(void *ctx, unsigned int rgb, int x, int y, int w, int h)
{
    for (int i = 0; i < h; i++)
        span_paint(ctx, rgb, x, y + i, w);
}

static void test_surface_send(void)
{
    unsigned int nowpx[SURF_W * SURF_H], shownpx[SURF_W * SURF_H];
    unsigned int spans[SURF_W * SURF_H], rects[SURF_W * SURF_H];
    rgbsurf_t now = { SURF_W, SURF_H, nowpx };
    rgbsurf_t shown = { SURF_W, SURF_H, shownpx };

    srand(2);
    for (int n = 0; n < 2000; n++) {
        /* Few colours and some repeated rows, so that runs stack up */
        for (int y = 0; y < SURF_H; y++) {
            if (y > 0 && rand() % 2) {
                memcpy(&nowpx[y * SURF_W], &nowpx[(y - 1) * SURF_W], SURF_W * sizeof nowpx[0]);
            } else {
                int x = 0;
                while (x < SURF_W) {
                    unsigned int c = rand() % 3;
                    int len = 1 + rand() % 8;
                    while (len-- > 0 && x < SURF_W)
                        nowpx[y * SURF_W + x++] = c;
                }
            }
            if (rand() % 2)
                memcpy(&shownpx[y * SURF_W], &nowpx[y * SURF_W], SURF_W * sizeof nowpx[0]);
            else
                memset(&shownpx[y * SURF_W], 0x55, SURF_W * sizeof nowpx[0]);
        }
        const rgbsurf_t *prior = n % 2 ? &shown : NULL;
        memset(spans, 0xff, sizeof spans);
        memset(rects, 0xff, sizeof rects);
        rgbsurf_send(&now, prior, span_paint, spans);
        rgbsurf_send_rects(&now, prior, rect_paint, rects);
        /* The same pixels, none of them twice (the paint functions check) */
        CHECK(memcmp(spans, rects, sizeof spans) == 0);
    }
}

static void test_surface_clipping(void)
{
    enum { W = 8, H = 8, GUARD = 4 };
    unsigned int px[(H + 2 * GUARD) * W];
    rgbsurf_t s = { W, H, px + GUARD * W };
    static const int far[] = { -2000000000, -100000, -1, 0, 7, 8, 100000, 2000000000 };
    int count = sizeof far / sizeof far[0];

    memset(px, 0, sizeof px);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count; j++) {
            rgbsurf_line(&s, far[i], far[j], far[j], far[i], 1, RGBSURF_NIB_SQUARE, 0xffffff, 255, 0);
            rgbsurf_line(&s, far[i], 3, far[j], 3, 3, RGBSURF_NIB_ROUND, 0xffffff, 255, 1);
            rgbsurf_fill_rect(&s, far[i], far[j], far[j], far[i], 0xffffff, 255);
        }
        rgbsurf_fill_circle(&s, far[i], 3, 5, 0xffffff, 255);
        rgbsurf_nib(&s, far[i], 3, 5, RGBSURF_NIB_ROUND, 0xffffff, 255);
    }
    /* Nothing lands in the rows on either side of the surface */
    for (int i = 0; i < GUARD * W; i++) {
        CHECK(px[i] == 0);
        CHECK(px[(GUARD + H) * W + i] == 0);
    }

    /* Across the middle: drawn, and clipped at the far end */
    memset(px, 0, sizeof px);
    rgbsurf_line(&s, -1000, 3, 100000, 3, 1, RGBSURF_NIB_SQUARE, 0xffffff, 255, 0);
    for (int x = 0; x < W; x++)
        CHECK(s.px[3 * W + x] == 0xffffff);
}

static void test_surface_fonts(void)
{
    /* Every glyph row fits the cell, for the tables nobody can audit by eye */
    for (int c = 0; c < 95; c++) {
        for (int row = 0; row < rgbsurf_font5x7.h; row++)
            CHECK((rgbsurf_font5x7.rows[c * rgbsurf_font5x7.h + row] >> rgbsurf_font5x7.w) == 0);
    }
    CHECK(rgbsurf_glyph_cols(&rgbsurf_font5x7, ' ') == 0);
    CHECK(rgbsurf_glyph_cols(&rgbsurf_font5x7, 200) == 0);
    CHECK(rgbsurf_glyph_cols(&rgbsurf_font8x8, 'M') > 0);
}

int main(void)
{
    test_show_text();
    test_line_break();
    test_companion_search();
    test_companion_helpers();
    test_files();
    test_memory();
    test_byteorder();
    test_minmax();
    test_random();
    test_surface_send();
    test_surface_clipping();
    test_surface_fonts();

    if (failures) {
        fprintf(stderr, "common_utils unit tests: %d of %d checks FAILED\n", failures, checks);
        return 1;
    }
    printf("common_utils unit tests: PASS (%d checks)\n", checks);
    return 0;
}
