/*
 * z80_probe -- prints what the decompressz80 library makes of a ZX Spectrum
 * snapshot or tape image, one tab-separated line per result, so that a change
 * to the library can be checked against a snapshot of its output rather than
 * against "the games still run".
 *
 *   z80_probe <op> <file>
 *
 * ops: z80       DecompressZ80: length and hash of the flat image
 *      pages     DecompressZ80Pages: is_128k, banked page, hash of each page
 *      tzx       GetTZXBlock for every data block
 *      tap       GetTAPBlock for every block
 *      payloads  ExtractTapePayloads as a .tzx or .tap (by signature), with
 *                the block table
 *
 * Every op takes any file: the interpreters hand tapes to DecompressZ80 and
 * snapshots to the tape readers when they are working out what a file is.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"

#define PAGE 0x4000
#define MAX_INDEX 300
#define MAX_BLOCKS 256

static uint64_t Hash(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

static void OpZ80(uint8_t *data, size_t size)
{
    size_t len = size;
    uint8_t *image = DecompressZ80(data, &len);
    if (image == NULL) {
        printf("z80\t-\n");
        return;
    }
    printf("z80\t%zu\t%016llx\n", len, (unsigned long long)Hash(image, len));
    free(image);
}

static void OpPages(uint8_t *data, size_t size)
{
    int is_128k = -1, banked = -1;
    uint8_t *pages = DecompressZ80Pages(data, size, &is_128k, &banked);
    if (pages == NULL) {
        printf("pages\t-\n");
        return;
    }
    printf("pages\tis_128k\t%d\n", is_128k);
    /* The banked page of a 48K snapshot means nothing */
    if (is_128k)
        printf("pages\tbanked\t%d\n", banked);
    for (int i = 0; i < 8; i++)
        printf("pages\t%d\t%016llx\n", i, (unsigned long long)Hash(pages + i * PAGE, PAGE));
    free(pages);
}

static void OpTZX(uint8_t *data, size_t size)
{
    for (int i = 0; i < MAX_INDEX; i++) {
        size_t len = 0;
        uint8_t *block = GetTZXBlock(i, data, size, &len);
        if (block == NULL)
            break;
        printf("tzx\t%d\t%zu\t%016llx\n", i, len, (unsigned long long)Hash(block, len));
        free(block);
    }
}

static void OpTAP(uint8_t *data, size_t size)
{
    for (int i = 0; i < MAX_INDEX; i++) {
        size_t len = 0;
        uint8_t *block = GetTAPBlock(i, data, size, &len);
        if (block == NULL)
            break;
        printf("tap\t%d\t%zu\t%016llx\n", i, len, (unsigned long long)Hash(block, len));
        free(block);
    }
}

static void OpPayloads(uint8_t *data, size_t size)
{
    static size_t offsets[MAX_BLOCKS], lengths[MAX_BLOCKS];
    size_t len = 0;
    int count = 0;
    int is_tzx = size > 8 && memcmp(data, "ZXTape!\x1a", 8) == 0;
    uint8_t *payloads = ExtractTapePayloads(data, size, is_tzx, &len, offsets, lengths,
        &count, MAX_BLOCKS);
    if (payloads == NULL) {
        printf("payloads\t-\n");
        return;
    }
    printf("payloads\tall\t%zu\t%016llx\n", len, (unsigned long long)Hash(payloads, len));
    for (int i = 0; i < count; i++)
        printf("payloads\t%d\t%zu\t%zu\n", i, offsets[i], lengths[i]);
    free(payloads);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: z80_probe z80|pages|tzx|tap|payloads <file>\n");
        return 2;
    }

    FILE *f = fopen(argv[2], "rb");
    if (f == NULL) {
        perror(argv[2]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    /* An exact-size buffer, so that the address sanitizer sees any read past
       the end of the file */
    uint8_t *data = malloc(size > 0 ? size : 1);
    if (data == NULL || (size > 0 && fread(data, size, 1, f) != 1)) {
        fprintf(stderr, "%s: cannot read\n", argv[2]);
        return 2;
    }
    fclose(f);

    const char *op = argv[1];
    if (strcmp(op, "z80") == 0)
        OpZ80(data, size);
    else if (strcmp(op, "pages") == 0)
        OpPages(data, size);
    else if (strcmp(op, "tzx") == 0)
        OpTZX(data, size);
    else if (strcmp(op, "tap") == 0)
        OpTAP(data, size);
    else if (strcmp(op, "payloads") == 0)
        OpPayloads(data, size);
    else {
        fprintf(stderr, "z80_probe: unknown op %s\n", op);
        return 2;
    }
    free(data);
    return 0;
}
