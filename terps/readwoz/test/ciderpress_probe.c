/*
 * ciderpress_probe -- prints what ciderpress.c extracts from an Apple II disk
 * image, one tab-separated line per result, so that a refactor of the reader
 * can be checked against a snapshot rather than against "the games still run".
 *
 *   ciderpress_probe <op> <image>
 *
 * ops: dos     GetAllApple2DOSFiles
 *      saga    ReadApple2DOSFile (database, inventory image, M2, M3)
 *      prodos  GetAllProDOSFiles (on the image as a DOS-order .dsk)
 *      full    ReadFullImageFromNib (nibble images only)
 *      range   ReadImageFromNib at a few offsets (nibble images only)
 *      v6      ReadInfocomV6File, called the way bocfel calls it
 *      twice   GetAllApple2DOSFiles twice with no FreeDiskImage in between
 *
 * A .woz is run through woz2nib first. Sizes other than a 35-track .nib or
 * .dsk are passed through as "sector" images, which is what the callers do.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciderpress.h"
#include "woz2nib.h"

#define NIB_SIZE 232960

static uint64_t Hash(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

static void Line(const char *what, const char *name, const uint8_t *p, size_t n)
{
    if (p == NULL)
        printf("%s\t%s\t-\t-\n", what, name);
    else
        printf("%s\t%s\t%zu\t%016llx\n", what, name, n, (unsigned long long)Hash(p, n));
}

static void Init(uint8_t *data, size_t size, int isnib)
{
    if (isnib)
        InitNibImage(data, size);
    else
        InitDskImage(data, size);
}

static void PrintRecs(const char *what, A2FileRec *recs, size_t n)
{
    printf("%s\tcount\t%zu\n", what, n);
    for (size_t i = 0; i < n; i++) {
        Line(what, recs[i].filename, recs[i].data, recs[i].datasize);
        free(recs[i].filename);
        free(recs[i].data);
    }
    free(recs);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s dos|saga|prodos|full|range|v6|twice <image>\n", argv[0]);
        return 2;
    }
    const char *op = argv[1];

    FILE *f = fopen(argv[2], "rb");
    if (f == NULL) {
        perror(argv[2]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    rewind(f);
    uint8_t *data = malloc(size ? size : 1);
    if (fread(data, 1, size, f) != size) {
        perror(argv[2]);
        return 2;
    }
    fclose(f);

    int waswoz = 0;
    if (size >= 3 && memcmp(data, "WOZ", 3) == 0) {
        uint8_t *nib = woz2nib(data, &size);
        if (nib == NULL) {
            printf("%s\twoz2nib\tfailed\n", op);
            return 0;
        }
        free(data);
        data = nib;
        waswoz = 1;
    }
    int isnib = (size == NIB_SIZE);

    if (strcmp(op, "dos") == 0) {
        size_t n = 0;
        Init(data, size, isnib);
        A2FileRec *recs = GetAllApple2DOSFiles(data, size, &n);
        PrintRecs(op, recs, n);
        FreeDiskImage();
    } else if (strcmp(op, "twice") == 0) {
        size_t n = 0, n2 = 0;
        Init(data, size, isnib);
        A2FileRec *recs = GetAllApple2DOSFiles(data, size, &n);
        A2FileRec *recs2 = GetAllApple2DOSFiles(data, size, &n2);
        printf("%s\tfirst\t%zu\tsecond\t%zu\n", op, n, n2);
        (void)recs;
        (void)recs2;
        FreeDiskImage();
    } else if (strcmp(op, "saga") == 0) {
        uint8_t *inv = NULL, *m2 = NULL, *m3 = NULL;
        size_t len = size, invlen = 0, m3len = 0;
        Init(data, size, isnib);
        uint8_t *db = ReadApple2DOSFile(data, &len, &inv, &invlen, &m2, &m3, &m3len);
        Line(op, "database", db, len);
        /* The last four bytes of the inventory image were never written. */
        printf("%s\tinvlen\t%zu\n", op, inv ? invlen : 0);
        Line(op, "inventory", inv, inv && invlen >= 4 ? invlen - 4 : 0);
        Line(op, "m2", m2, 0x182);
        Line(op, "m3", m3, m3len);
        FreeDiskImage();
    } else if (strcmp(op, "prodos") == 0) {
        uint8_t *dsk = data;
        size_t dsksize = size;
        if (isnib) {
            dsk = ReadFullImageFromNib(data, size);
            dsksize = 143360;
        }
        size_t n = 0;
        A2FileRec *recs = GetAllProDOSFiles(dsk, dsksize, &n);
        PrintRecs(op, recs, n);
        FreeDiskImage();
    } else if (strcmp(op, "full") == 0) {
        if (isnib)
            Line(op, "image", ReadFullImageFromNib(data, size), 143360);
    } else if (strcmp(op, "range") == 0) {
        if (isnib) {
            static const size_t at[][2] = {
                { 0x11000, 0x100 }, { 0x3000, 0x1234 }, { 0x1ff00, 0x804 }, { 0x22f00, 0x100 }
            };
            InitNibImage(data, size);
            for (size_t i = 0; i < sizeof at / sizeof at[0]; i++) {
                char name[32];
                snprintf(name, sizeof name, "%zx+%zx", at[i][0], at[i][1]);
                Line(op, name, ReadImageFromNib(at[i][0], at[i][1], data, size), at[i][1]);
            }
            FreeDiskImage();
        }
    } else if (strcmp(op, "v6") == 0) {
        /* bocfel only initialises for a WOZ */
        int game = -1, disk = -1;
        size_t len = size;
        if (waswoz)
            InitNibImage(data, size);
        uint8_t *buf = ReadInfocomV6File(data, &len, &game, &disk);
        if (buf == NULL) {
            printf("%s\tfile\t-\t-\n", op);
        } else {
            char name[32];
            snprintf(name, sizeof name, "game%d.disk%d", game, disk);
            Line(op, name, buf, len);
        }
    } else {
        fprintf(stderr, "unknown op %s\n", op);
        return 2;
    }
    return 0;
}
