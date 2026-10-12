/*
 * c64diskimage_probe -- prints what c64diskimage.c extracts from a C64 disk
 * image, one tab-separated line per result, so that a change to the reader
 * can be checked against a snapshot rather than against "the games still run".
 *
 *   c64diskimage_probe <image>
 *
 * file     every name of di_get_all_file_names, read with di_read_file_named
 * largest  di_get_largest_file
 * named    di_get_file_named of the first name
 * ts       di_get_file_at_ts of every track and sector that gives a file,
 *          and of some that are on no disk
 *
 * Each line ends with the size of the file and a hash of it, or with two
 * dashes if there was no file. Bytes of a name that are not printable ASCII
 * are written as \xNN.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c64diskimage.h"

static uint64_t Hash(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

static void Line(const char *what, const char *name, uint8_t *p, size_t n)
{
    printf("%s\t", what);
    for (; *name; name++) {
        unsigned char c = (unsigned char)*name;
        if (c < 0x20 || c > 0x7e || c == '\\')
            printf("\\x%02x", c);
        else
            putchar(c);
    }
    if (p == NULL)
        printf("\t-\t-\n");
    else
        printf("\t%zu\t%016llx\n", n, (unsigned long long)Hash(p, n));
    free(p);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <image>\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    rewind(f);
    /* An exact-size buffer, so that the sanitizer sees a read past the image */
    uint8_t *data = malloc(size ? size : 1);
    if (data == NULL || fread(data, 1, size, f) != size) {
        fprintf(stderr, "%s: cannot read\n", argv[1]);
        return 2;
    }
    fclose(f);

    DiskImage *di = di_create_from_data(data, size);
    if (di == NULL) {
        size_t len = 1;
        /* The one-shot functions must refuse it too */
        if (di_get_file_named(data, size, &len, "*") != NULL || len != 0
            || di_get_largest_file(data, size, &len) != NULL || len != 0
            || di_get_file_at_ts(data, size, &len, 18, 1) != NULL || len != 0)
            return 3;
        printf("image\t\t-\t-\n");
        free(data);
        return 0;
    }

    size_t len;
    int count = -1;
    char **names = di_get_all_file_names(di, &count);
    if ((names == NULL) != (count == 0))
        return 3;
    for (int i = 0; i < count; i++) {
        uint8_t *file = di_read_file_named(di, names[i], &len);
        Line("file", names[i], file, len);
    }
    if (names != NULL && names[count] != NULL)
        return 3;

    Line("largest", "", di_get_largest_file(data, size, &len), len);
    if (count > 0)
        Line("named", names[0], di_get_file_named(data, size, &len, names[0]), len);

    /* A name of more than 16 characters is the name of no file */
    if (di_read_file_named(di, "12345678901234567", &len) != NULL)
        return 3;

    for (int track = -1; track <= 256; track++) {
        for (int sector = -1; sector <= 256; sector += (sector == 41 ? 214 : 1)) {
            uint8_t *file = di_get_file_at_ts(data, size, &len, track, sector);
            if (file != NULL) {
                char name[16];
                snprintf(name, sizeof name, "%d/%d", track, sector);
                Line("ts", name, file, len);
            }
        }
    }

    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
    di_free(di);
    free(data);
    return 0;
}
