// Extract the SAGA bitmap image blobs from a Scott Adams C64 .d64 disk image,
// exactly as the interpreter's LoadC64USImages() does (ai_uk/c64decrunch.c):
// every directory entry whose name is a SAGA image (R/B/S + three digits) is
// read straight off the disk and written out verbatim as <name>.dat. These are
// the byte-for-byte inputs the C renderer (DrawC64A8ImageFromData) consumes, so
// the bitmap test can feed the real game data without linking the whole loader.
//
//   usage: c64extract <game.d64> <outdir>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "c64diskimage.h"

// Mirrors sagagraphics.c:IsSagaImage — R/B/S followed by three digits.
static int is_saga_image(const char *name) {
    if (name == NULL || strlen(name) < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for (int i = 1; i < 4; i++)
            if (!isdigit((unsigned char)name[i]))
                return 0;
        return 1;
    }
    return 0;
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf && size) *size = (size_t)n;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <game.d64> <outdir>\n", argv[0]);
        return 2;
    }
    const char *d64path = argv[1];
    const char *outdir = argv[2];

    size_t disksize = 0;
    uint8_t *disk = read_file(d64path, &disksize);
    if (!disk) { fprintf(stderr, "cannot read %s\n", d64path); return 1; }

    DiskImage *d64 = di_create_from_data(disk, (int)disksize);
    if (!d64) { fprintf(stderr, "not a valid d64: %s\n", d64path); return 1; }

    int numfiles = 0;
    char **names = di_get_all_file_names(d64, &numfiles);
    if (!names) { fprintf(stderr, "no directory entries\n"); return 1; }

    int extracted = 0;
    for (int i = 0; i < numfiles; i++) {
        if (names[i] && is_saga_image(names[i])) {
            unsigned char rawname[1024];
            di_rawname_from_name(rawname, names[i]);
            ImageFile *cf = di_open(d64, rawname, 0xc2, "rb");
            if (cf) {
                unsigned char buf[0xffff];
                int n = di_read(cf, buf, 0xffff);
                free(cf);
                if (n > 0) {
                    char outpath[1100];
                    snprintf(outpath, sizeof outpath, "%s/%s.dat", outdir, names[i]);
                    FILE *o = fopen(outpath, "wb");
                    if (o) {
                        fwrite(buf, 1, (size_t)n, o);
                        fclose(o);
                        printf("  %-8s %5d bytes\n", names[i], n);
                        extracted++;
                    }
                }
            }
        }
        free(names[i]);
    }
    free(names);
    printf("extracted %d image blob(s) to %s\n", extracted, outdir);
    return extracted ? 0 : 1;
}
