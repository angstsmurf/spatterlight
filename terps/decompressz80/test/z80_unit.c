/*
 * z80_unit -- unit tests of the decompressz80 library. They make their own
 * snapshots and tapes and need no game files. Run them with unit.sh.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"

#define PAGE ZX_PAGE_SIZE

static int checks, failures;

#define CHECK(cond) \
    do { \
        checks++; \
        if (!(cond)) { \
            failures++; \
            printf("FAIL    %s:%d: %s\n", __func__, __LINE__, #cond); \
        } \
    } while (0)

/* --- A buffer to build files in --- */

typedef struct {
    uint8_t *data;
    size_t length, room;
} Buf;

static void Put(Buf *b, const void *bytes, size_t n)
{
    if (b->length + n > b->room) {
        b->room = (b->length + n) * 2;
        b->data = realloc(b->data, b->room);
        if (b->data == NULL)
            abort();
    }
    if (n)
        memcpy(b->data + b->length, bytes, n);
    b->length += n;
}

static void PutByte(Buf *b, uint8_t value)
{
    Put(b, &value, 1);
}

static void PutLE(Buf *b, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
        PutByte(b, (uint8_t)(value >> (8 * i)));
}

static void PutFill(Buf *b, uint8_t value, size_t n)
{
    for (size_t i = 0; i < n; i++)
        PutByte(b, value);
}

/* An exact-size copy, so that the address sanitizer sees a read past the end */
static uint8_t *Exact(const Buf *b)
{
    uint8_t *copy = malloc(b->length ? b->length : 1);
    if (copy == NULL)
        abort();
    memcpy(copy, b->data, b->length);
    return copy;
}

/* --- .z80 snapshots --- */

/* Page n of test RAM: runs, single 0xED bytes and pairs of them, all of it
   different for every page */
static void FillPage(uint8_t *page, int n)
{
    for (int i = 0; i < PAGE; i++) {
        int phase = i % 97;
        if (phase < 40)
            page[i] = (uint8_t)(n * 16 + 1);
        else if (phase == 50 || phase == 60 || phase == 61 || phase == 96)
            page[i] = 0xed;
        else
            page[i] = (uint8_t)(i * 7 + n);
    }
    page[0] = (uint8_t)(0xa0 + n);
}

/* The compression of the format: ED ED nn xx for a run of five or more, and
   for any two or more of ED. A byte after a single ED is never part of a
   run. */
static void Pack(Buf *out, const uint8_t *src, size_t n)
{
    int after_ed = 0;
    size_t i = 0;
    while (i < n) {
        size_t run = 1;
        while (i + run < n && src[i + run] == src[i] && run < 255)
            run++;
        if (!after_ed && (run >= 5 || (src[i] == 0xed && run >= 2))) {
            PutByte(out, 0xed);
            PutByte(out, 0xed);
            PutByte(out, (uint8_t)run);
            PutByte(out, src[i]);
            i += run;
        } else {
            PutByte(out, src[i]);
            after_ed = (src[i] == 0xed);
            i++;
        }
    }
}

static void PutV1Header(Buf *b, int compressed)
{
    uint8_t header[30] = { 0 };
    header[6] = 0x34; /* PC, not 0 */
    header[7] = 0x12;
    header[12] = compressed ? 0x20 : 0;
    Put(b, header, sizeof header);
}

/* Header of version 2 (extra == 23) or 3 (54 or 55) */
static void PutV23Header(Buf *b, int extra, uint8_t machine, uint8_t port, uint8_t modifier)
{
    uint8_t header[32 + 55] = { 0 };
    header[30] = (uint8_t)extra;
    header[34] = machine;
    header[35] = port;
    header[37] = modifier;
    Put(b, header, 32 + (size_t)extra);
}

static void PutPage(Buf *b, int number, const uint8_t *page, int compressed)
{
    if (compressed) {
        Buf packed = { 0 };
        Pack(&packed, page, PAGE);
        PutLE(b, (uint32_t)packed.length, 2);
        PutByte(b, (uint8_t)number);
        Put(b, packed.data, packed.length);
        free(packed.data);
    } else {
        PutLE(b, 0xffff, 2);
        PutByte(b, (uint8_t)number);
        Put(b, page, PAGE);
    }
}

static uint8_t ram[16][PAGE]; /* test RAM pages by number */

static uint8_t *Flat(const Buf *b, size_t *length)
{
    uint8_t *file = Exact(b);
    *length = b->length;
    uint8_t *image = DecompressZ80(file, length);
    free(file);
    return image;
}

static int Rejected(const Buf *b)
{
    size_t length;
    uint8_t *image = Flat(b, &length);
    int rejected = (image == NULL && length == b->length);
    free(image);

    int is_128k = 77, banked = 77;
    uint8_t *file = Exact(b);
    uint8_t *pages = DecompressZ80Pages(file, b->length, &is_128k, &banked);
    free(file);
    rejected = rejected && pages == NULL;
    free(pages);
    return rejected;
}

/* The pages of the flat image are the test pages 'order', and after them
   there is nothing but zeroes */
static int FlatIs(const Buf *b, size_t want_length, const int *order, int count)
{
    size_t length;
    uint8_t *image = Flat(b, &length);
    if (image == NULL)
        return 0;
    int ok = (length == want_length);
    for (int i = 0; ok && i < count; i++)
        ok = memcmp(image + i * PAGE, ram[order[i]], PAGE) == 0;
    for (size_t i = (size_t)count * PAGE; ok && i < length; i++)
        ok = (image[i] == 0);
    free(image);
    return ok;
}

static void TestV1(void)
{
    static const int order[] = { 5, 2, 0 };
    Buf b = { 0 };

    PutV1Header(&b, 0);
    for (int i = 0; i < 3; i++)
        Put(&b, ram[order[i]], PAGE);
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order, 3));
    PutByte(&b, 0);
    CHECK(Rejected(&b));
    b.length -= 2;
    CHECK(Rejected(&b));

    uint8_t *all = malloc(3 * PAGE);
    for (int i = 0; i < 3; i++)
        memcpy(all + i * PAGE, ram[order[i]], PAGE);

    b.length = 0;
    PutV1Header(&b, 1);
    Pack(&b, all, 3 * PAGE);
    size_t packed_end = b.length;
    CHECK(packed_end < 30 + 3 * PAGE);
    CHECK(Rejected(&b)); /* no end marker */
    PutLE(&b, 0x00eded00, 4);
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order, 3));

    int is_128k = -1, banked = -1;
    uint8_t *pages = DecompressZ80Pages(b.data, b.length, &is_128k, &banked);
    CHECK(pages != NULL && is_128k == 0 && banked == 0);
    if (pages) {
        CHECK(memcmp(pages + 5 * PAGE, ram[5], PAGE) == 0);
        CHECK(memcmp(pages + 2 * PAGE, ram[2], PAGE) == 0);
        CHECK(memcmp(pages, ram[0], PAGE) == 0);
        for (int i = 0; i < 8; i++)
            if (i != 0 && i != 2 && i != 5)
                CHECK(pages[i * PAGE] == 0 && pages[i * PAGE + PAGE - 1] == 0);
    }
    free(pages);

    PutByte(&b, 0); /* something after the marker */
    CHECK(Rejected(&b));

    /* a byte too few and a byte too many in the RAM */
    b.length = 0;
    PutV1Header(&b, 1);
    Pack(&b, all, 3 * PAGE - 1);
    PutLE(&b, 0x00eded00, 4);
    CHECK(Rejected(&b));
    b.length = 0;
    PutV1Header(&b, 1);
    Pack(&b, all, 3 * PAGE);
    PutByte(&b, 0x55);
    PutLE(&b, 0x00eded00, 4);
    CHECK(Rejected(&b));

    /* a run that goes past the end of the RAM */
    b.length = 0;
    PutV1Header(&b, 1);
    Pack(&b, all, 3 * PAGE - 3);
    PutLE(&b, 0x0109eded, 4);
    PutLE(&b, 0x00eded00, 4);
    CHECK(Rejected(&b));

    /* the marker and nothing else, and less than a header */
    b.length = 0;
    PutV1Header(&b, 1);
    PutLE(&b, 0x00eded00, 4);
    CHECK(Rejected(&b));
    for (size_t n = 0; n <= 30; n++) {
        b.length = n;
        CHECK(Rejected(&b));
    }
    size_t length = 100;
    CHECK(DecompressZ80(NULL, &length) == NULL && length == 100);
    CHECK(DecompressZ80(b.data, NULL) == NULL);
    CHECK(DecompressZ80Pages(NULL, 100, NULL, NULL) == NULL);

    free(all);
    free(b.data);
}

static void Put48KPages(Buf *b, int compressed)
{
    PutPage(b, 8, ram[5], compressed);
    PutPage(b, 4, ram[2], compressed);
    PutPage(b, 5, ram[0], compressed);
}

static void TestMachines(void)
{
    /* What each hardware type is in a version 2 and in a version 3 file:
       'k' for 48K, 'K' for 128K, '-' for one that is turned away */
    static const struct {
        int type;
        char v2, v3;
    } machines[] = {
        { 0, 'k', 'k' }, { 1, 'k', 'k' }, { 2, 'k', 'k' }, { 3, 'K', 'k' },
        { 4, 'K', 'K' }, { 5, '-', 'K' }, { 6, '-', 'K' }, { 7, 'K', 'K' },
        { 8, 'K', 'K' }, { 9, 'K', 'K' }, { 10, 'K', 'K' }, { 11, '-', '-' },
        { 12, 'K', 'K' }, { 13, 'K', 'K' }, { 14, 'k', 'k' }, { 15, 'k', 'k' },
        { 16, '-', '-' }, { 127, '-', '-' }, { 128, 'k', 'k' }, { 129, '-', '-' },
        { 255, '-', '-' },
    };
    static const int extras[] = { 23, 54, 55 };
    static const int order48[] = { 5, 2, 0 };
    /* Port value 0x13: ROM 1 and page 3 */
    static const int order128[] = { 5, 2, 3, 0, 1, 4, 6, 7 };

    for (size_t m = 0; m < sizeof machines / sizeof machines[0]; m++) {
        for (int e = 0; e < 3; e++) {
            char kind = e == 0 ? machines[m].v2 : machines[m].v3;
            Buf b = { 0 };
            PutV23Header(&b, extras[e], (uint8_t)machines[m].type, 0x13, 0);
            if (kind == 'K') {
                /* in no order in particular, and compressed or not */
                static const int file_order[] = { 9, 3, 8, 10, 4, 6, 5, 7 };
                for (int i = 0; i < 8; i++)
                    PutPage(&b, file_order[i], ram[file_order[i] - 3], i & 1);
                CHECK(FlatIs(&b, Z80_IMAGE_SIZE_128K, order128, 8));
                int is_128k = -1, banked = -1;
                uint8_t *pages = DecompressZ80Pages(b.data, b.length, &is_128k, &banked);
                CHECK(pages != NULL && is_128k == 1 && banked == 3);
                for (int i = 0; pages && i < 8; i++)
                    CHECK(memcmp(pages + i * PAGE, ram[i], PAGE) == 0);
                free(pages);
            } else {
                Put48KPages(&b, e & 1);
                if (kind == 'k') {
                    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order48, 3));
                    int is_128k = -1, banked = -1;
                    uint8_t *pages = DecompressZ80Pages(b.data, b.length, &is_128k, &banked);
                    /* the port byte of a 48K file is not a paging port */
                    CHECK(pages != NULL && is_128k == 0 && banked == 0);
                    free(pages);
                } else {
                    CHECK(Rejected(&b));
                }
            }
            free(b.data);
        }
    }
}

static void Test128K(void)
{
    Buf b = { 0 };

    /* The page banked in is put third, whatever the other bits of the port
       are, on a +2 and a +3 as on a 128K. Pages 5 and 2 are not put in twice. */
    static const uint8_t types[] = { 4, 12, 7, 13, 9 };
    static const uint8_t high_bits[] = { 0x00, 0x10, 0x08, 0x38, 0xf8 };
    CHECK(Z80_IMAGE_SIZE_128K == 8 * PAGE);
    for (size_t t = 0; t < sizeof types; t++) {
        for (int port = 0; port < 8; port++) {
            for (size_t h = 0; h < sizeof high_bits; h++) {
                b.length = 0;
                PutV23Header(&b, 54, types[t], (uint8_t)(port | high_bits[h]), 0);
                for (int i = 0; i < 8; i++)
                    PutPage(&b, i + 3, ram[i], 1);
                int order[8] = { 5, 2 }, count = 2;
                if (port != 5 && port != 2)
                    order[count++] = port;
                for (int i = 0; i < 8; i++)
                    if (i != 5 && i != 2 && i != port)
                        order[count++] = i;
                CHECK(count == 8);
                CHECK(FlatIs(&b, Z80_IMAGE_SIZE_128K, order, 8));
                int banked = -1;
                uint8_t *pages = DecompressZ80Pages(b.data, b.length, NULL, &banked);
                CHECK(pages != NULL && banked == port);
                free(pages);
            }
        }
    }

    /* The modifier bit makes a +2 of the 128K and a 16K of the 48K */
    b.length = 0;
    PutV23Header(&b, 54, 4, 0x10, 0x80);
    for (int i = 0; i < 8; i++)
        PutPage(&b, i + 3, ram[i], 1);
    static const int order128[] = { 5, 2, 0, 1, 3, 4, 6, 7 };
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_128K, order128, 8));
    b.length = 0;
    PutV23Header(&b, 54, 0, 0, 0x80);
    PutPage(&b, 8, ram[5], 1);
    static const int order16[] = { 5 };
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order16, 1));

    /* Pages that the file does not have are zeroes */
    b.length = 0;
    PutV23Header(&b, 23, 3, 0x10, 0);
    PutPage(&b, 8, ram[5], 1);
    PutPage(&b, 10, ram[7], 0);
    {
        size_t length;
        uint8_t *image = Flat(&b, &length);
        CHECK(image != NULL && length == Z80_IMAGE_SIZE_128K);
        if (image) {
            CHECK(memcmp(image, ram[5], PAGE) == 0);
            CHECK(memcmp(image + 7 * PAGE, ram[7], PAGE) == 0);
            int zero = 1;
            for (size_t i = PAGE; i < 7 * PAGE; i++)
                zero = zero && image[i] == 0;
            CHECK(zero);
        }
        free(image);
    }

    /* ROM pages, the Multiface ROM and the pages past the eighth are read
       past, on a Scorpion too */
    for (int scorpion = 0; scorpion < 2; scorpion++) {
        b.length = 0;
        PutV23Header(&b, 55, scorpion ? 10 : 9, 0x10, 0);
        PutPage(&b, 1, ram[9], 1);
        PutPage(&b, 2, ram[10], 0);
        for (int i = 0; i < 8; i++)
            PutPage(&b, i + 3, ram[i], 1);
        for (int page = 11; page <= 18; page++)
            PutPage(&b, page, ram[page - 3], page & 1);
        CHECK(FlatIs(&b, Z80_IMAGE_SIZE_128K, order128, 8));
    }

    /* A 48K file may have its ROM in it */
    b.length = 0;
    PutV23Header(&b, 23, 0, 0, 0);
    PutPage(&b, 3, ram[9], 1);
    Put48KPages(&b, 1);
    static const int order48[] = { 5, 2, 0 };
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order48, 3));

    free(b.data);
}

static void TestBadPages(void)
{
    static const int order48[] = { 5, 2, 0 };
    Buf b = { 0 };

    PutV23Header(&b, 54, 0, 0, 0);
    size_t header_end = b.length;
    CHECK(Rejected(&b)); /* a header and no RAM */
    for (size_t n = 30; n < header_end; n++) {
        b.length = n;
        CHECK(Rejected(&b));
    }
    b.length = header_end;
    PutPage(&b, 1, ram[9], 1);
    CHECK(Rejected(&b)); /* a ROM and no RAM */

    b.length = header_end;
    Put48KPages(&b, 1);
    size_t good = b.length;
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order48, 3));

    /* the level data of a .slt file after the pages */
    PutFill(&b, 0, 3);
    Put(&b, "SLT", 3);
    PutFill(&b, 0xff, 300);
    CHECK(FlatIs(&b, Z80_IMAGE_SIZE_48K, order48, 3));

    /* cut anywhere in the last page, or with a few bytes after it */
    for (size_t cut = 1; cut < 700; cut += 7) {
        b.length = good - cut;
        CHECK(Rejected(&b));
    }
    for (int junk = 1; junk <= 2; junk++) {
        b.length = good;
        PutFill(&b, 0, (size_t)junk);
        CHECK(Rejected(&b));
    }

    static const int bad_numbers[] = { 0, 19, 20, 255 };
    for (int i = 0; i < 4; i++) {
        b.length = good;
        PutPage(&b, bad_numbers[i], ram[9], 1);
        CHECK(Rejected(&b));
    }
    b.length = good;
    PutPage(&b, 4, ram[9], 0); /* a page for the second time */
    CHECK(Rejected(&b));

    /* a page that makes fewer or more than 16K */
    for (int more = 0; more < 2; more++) {
        Buf packed = { 0 };
        Pack(&packed, ram[5], PAGE - 1);
        if (more)
            PutFill(&packed, 0x11, 2);
        b.length = header_end;
        PutLE(&b, (uint32_t)packed.length, 2);
        PutByte(&b, 8);
        Put(&b, packed.data, packed.length);
        CHECK(Rejected(&b));
        free(packed.data);
    }

    /* ED ED with no room for the two bytes that make it a run is two bytes */
    static uint8_t tail[PAGE];
    memset(tail, 0x42, PAGE);
    tail[PAGE - 2] = tail[PAGE - 1] = 0xed;
    b.length = header_end;
    PutLE(&b, 4 * (PAGE - 2) / 254 + 4 + 2, 2);
    PutByte(&b, 8);
    size_t left = PAGE - 2, at = b.length;
    while (left) {
        size_t run = left > 254 ? 254 : left;
        PutLE(&b, 0xeded | (uint32_t)run << 16 | 0x42000000, 4);
        left -= run;
    }
    PutLE(&b, 0xeded, 2);
    b.data[at - 3] = (uint8_t)(b.length - at);
    b.data[at - 2] = (uint8_t)((b.length - at) >> 8);
    {
        size_t length;
        uint8_t *image = Flat(&b, &length);
        CHECK(image != NULL && memcmp(image, tail, PAGE) == 0);
        free(image);
    }

    /* header lengths that no version has */
    static const int bad_extras[] = { 0, 22, 24, 53, 56, 0xff };
    for (int i = 0; i < 6; i++) {
        b.length = header_end;
        Put48KPages(&b, 1);
        b.data[30] = (uint8_t)bad_extras[i];
        CHECK(Rejected(&b));
    }

    free(b.data);
}

/* --- Tapes --- */

static void PutTZXHeader(Buf *b)
{
    Put(b, "ZXTape!\x1a\x01\x14", 10);
}

/* A standard speed data block of n bytes that begin with 'flag' */
static void PutTZXData(Buf *b, uint8_t flag, size_t n)
{
    PutByte(b, 0x10);
    PutLE(b, 1000, 2);
    PutLE(b, (uint32_t)n, 2);
    if (n)
        PutByte(b, flag);
    for (size_t i = 1; i < n; i++)
        PutByte(b, (uint8_t)(flag + i * 3));
}

static void PutTZXTurbo(Buf *b, uint8_t flag, size_t n)
{
    PutByte(b, 0x11);
    PutFill(b, 0x77, 15);
    PutLE(b, (uint32_t)n, 3);
    if (n)
        PutByte(b, flag);
    for (size_t i = 1; i < n; i++)
        PutByte(b, (uint8_t)(flag + i * 3));
}

/* One block of every other kind that TZX 1.20 has, each made the way the
   specification lays it out and filled with 0xff, which would be read as an
   enormous length if a block were stepped over wrongly. 'kind' counts from
   0; returns 0 when there are no more. */
static int PutTZXOther(Buf *b, int kind)
{
    switch (kind) {
    case 0: /* pure tone */
        PutByte(b, 0x12);
        PutFill(b, 0xff, 4);
        return 1;
    case 1: /* pulse sequence: a count and that many words */
        PutByte(b, 0x13);
        PutByte(b, 5);
        PutFill(b, 0xff, 10);
        return 1;
    case 2: /* pure data */
        PutByte(b, 0x14);
        PutFill(b, 0xff, 7);
        PutLE(b, 300, 3);
        PutFill(b, 0xff, 300);
        return 1;
    case 3: /* direct recording */
        PutByte(b, 0x15);
        PutFill(b, 0xff, 5);
        PutLE(b, 70, 3);
        PutFill(b, 0xff, 70);
        return 1;
    case 4: /* the four that begin with the length of the rest of the block */
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: {
        static const uint8_t ids[] = { 0x16, 0x17, 0x18, 0x19, 0x2a, 0x2b };
        PutByte(b, ids[kind - 4]);
        PutLE(b, 40, 4);
        PutFill(b, 0xff, 40);
        return 1;
    }
    case 10: /* pause */
        PutByte(b, 0x20);
        PutFill(b, 0xff, 2);
        return 1;
    case 11: /* group start: a length and a name */
        PutByte(b, 0x21);
        PutByte(b, 9);
        PutFill(b, 0xff, 9);
        return 1;
    case 12: /* group end */
        PutByte(b, 0x22);
        return 1;
    case 13: /* jump */
        PutByte(b, 0x23);
        PutFill(b, 0xff, 2);
        return 1;
    case 14: /* loop start */
        PutByte(b, 0x24);
        PutFill(b, 0xff, 2);
        return 1;
    case 15: /* loop end */
        PutByte(b, 0x25);
        return 1;
    case 16: /* call sequence: a count and that many words */
        PutByte(b, 0x26);
        PutLE(b, 3, 2);
        PutFill(b, 0xff, 6);
        return 1;
    case 17: /* return from sequence */
        PutByte(b, 0x27);
        return 1;
    case 18: /* select */
        PutByte(b, 0x28);
        PutLE(b, 12, 2);
        PutFill(b, 0xff, 12);
        return 1;
    case 19: /* text description */
        PutByte(b, 0x30);
        PutByte(b, 20);
        PutFill(b, 0xff, 20);
        return 1;
    case 20: /* message: how long to show it, a length and the text */
        PutByte(b, 0x31);
        PutByte(b, 0xff);
        PutByte(b, 20);
        PutFill(b, 0xff, 20);
        return 1;
    case 21: /* archive info */
        PutByte(b, 0x32);
        PutLE(b, 33, 2);
        PutFill(b, 0xff, 33);
        return 1;
    case 22: /* hardware type: a count and that many entries of three bytes */
        PutByte(b, 0x33);
        PutByte(b, 4);
        PutFill(b, 0xff, 12);
        return 1;
    case 23: /* emulation info */
        PutByte(b, 0x34);
        PutFill(b, 0xff, 8);
        return 1;
    case 24: /* custom info: a name of sixteen bytes, a length and the data */
        PutByte(b, 0x35);
        PutFill(b, 0xff, 16);
        PutLE(b, 50, 4);
        PutFill(b, 0xff, 50);
        return 1;
    case 25: /* snapshot: its type, a length and the data */
        PutByte(b, 0x40);
        PutByte(b, 0xff);
        PutLE(b, 60, 3);
        PutFill(b, 0xff, 60);
        return 1;
    case 26: /* glue: the header of the file that was joined on */
        PutByte(b, 0x5a);
        PutFill(b, 0xff, 9);
        return 1;
    default:
        return 0;
    }
}

static int IsBlock(uint8_t *block, size_t length, uint8_t flag, size_t n)
{
    if (block == NULL || length != n || block[0] != flag)
        return 0;
    for (size_t i = 1; i < n; i++)
        if (block[i] != (uint8_t)(flag + i * 3))
            return 0;
    return 1;
}

static void TestTZX(void)
{
    Buf b = { 0 };
    PutTZXHeader(&b);

    /* A data block between every two blocks of the other kinds */
    int kinds = 0;
    PutTZXData(&b, 0x00, 19);
    while (PutTZXOther(&b, kinds)) {
        if (kinds & 1)
            PutTZXTurbo(&b, 0xff, 100 + (size_t)kinds);
        else
            PutTZXData(&b, 0xff, 100 + (size_t)kinds);
        kinds++;
    }
    CHECK(kinds == 27);

    uint8_t *tape = Exact(&b);
    /* The data blocks are numbered as if the others were not there, and
       come whole: flag, data and checksum */
    size_t length = 0;
    uint8_t *block = GetTZXBlock(0, tape, b.length, &length);
    CHECK(IsBlock(block, length, 0x00, 19));
    free(block);
    size_t payload_at = 0;
    static size_t offsets[64], lengths[64];
    for (int k = 0; k < kinds; k++) {
        block = GetTZXBlock(1 + k, tape, b.length, &length);
        CHECK(IsBlock(block, length, 0xff, 100 + (size_t)k));
        free(block);
    }
    length = 12345;
    CHECK(GetTZXBlock(1 + kinds, tape, b.length, &length) == NULL && length == 12345);
    CHECK(GetTZXBlock(-1, tape, b.length, &length) == NULL && length == 12345);
    CHECK(GetTZXBlock(0, tape, b.length, NULL) == NULL);

    /* The payloads: the header block is left out, the flag and the checksum
       are taken off the others */
    size_t out_len = 0;
    int count = 0;
    uint8_t *payloads = ExtractTapePayloads(tape, b.length, 1, &out_len, offsets, lengths, &count, 64);
    CHECK(payloads != NULL && count == kinds);
    for (int k = 0; payloads && k < count; k++) {
        size_t n = 100 + (size_t)k;
        CHECK(offsets[k] == payload_at && lengths[k] == n - 2);
        int same = 1;
        for (size_t i = 1; i < n - 1; i++)
            same = same && payloads[payload_at + i - 1] == (uint8_t)(0xff + i * 3);
        CHECK(same);
        payload_at += n - 2;
    }
    CHECK(out_len == payload_at);
    free(payloads);

    /* No more of the table than there is room for, and none at all */
    count = 0;
    offsets[5] = lengths[5] = 12345;
    payloads = ExtractTapePayloads(tape, b.length, 1, &out_len, offsets, lengths, &count, 5);
    CHECK(payloads != NULL && count == 5 && out_len == payload_at);
    CHECK(offsets[5] == 12345 && lengths[5] == 12345);
    free(payloads);
    payloads = ExtractTapePayloads(tape, b.length, 1, NULL, NULL, NULL, NULL, 0);
    CHECK(payloads != NULL);
    free(payloads);

    /* Cut anywhere, the tape gives what was whole before the cut and never
       more than the whole tape does */
    for (size_t cut = 0; cut < b.length; cut += 3) {
        uint8_t *part = malloc(cut ? cut : 1);
        memcpy(part, tape, cut);
        size_t part_len = 0;
        payloads = ExtractTapePayloads(part, cut, 1, &part_len, NULL, NULL, NULL, 0);
        CHECK(part_len <= payload_at && (payloads != NULL) == (part_len != 0));
        free(payloads);
        for (int i = 0; i < 30; i += 3)
            free(GetTZXBlock(i, part, cut, &length));
        free(part);
    }
    free(tape);

    /* A block of a kind there is not ends the tape */
    b.length = 0;
    PutTZXHeader(&b);
    PutTZXData(&b, 0xff, 10);
    PutByte(&b, 0x29);
    PutTZXData(&b, 0xff, 10);
    block = GetTZXBlock(0, b.data, b.length, &length);
    CHECK(IsBlock(block, length, 0xff, 10));
    free(block);
    CHECK(GetTZXBlock(1, b.data, b.length, &length) == NULL);
    payloads = ExtractTapePayloads(b.data, b.length, 1, &out_len, NULL, NULL, &count, 64);
    CHECK(payloads != NULL && out_len == 8 && count == 1);
    free(payloads);

    /* Blocks too short to have data, and a tape with nothing but a header */
    b.length = 0;
    PutTZXHeader(&b);
    PutTZXData(&b, 0xff, 0);
    PutTZXData(&b, 0xff, 1);
    PutTZXData(&b, 0xff, 2);
    PutTZXData(&b, 0x00, 19);
    out_len = 99;
    count = 99;
    CHECK(ExtractTapePayloads(b.data, b.length, 1, &out_len, NULL, NULL, &count, 64) == NULL);
    CHECK(out_len == 0 && count == 0);
    length = 12345;
    block = GetTZXBlock(0, b.data, b.length, &length);
    CHECK(block != NULL && length == 0);
    free(block);
    block = GetTZXBlock(1, b.data, b.length, &length);
    CHECK(IsBlock(block, length, 0xff, 1));
    free(block);
    block = GetTZXBlock(2, b.data, b.length, &length);
    CHECK(IsBlock(block, length, 0xff, 2));
    free(block);

    /* Not a .tzx: no signature, and a .tap */
    b.data[6] = '?';
    length = 12345;
    CHECK(GetTZXBlock(0, b.data, b.length, &length) == NULL && length == 12345);
    CHECK(ExtractTapePayloads(b.data, b.length, 1, &out_len, NULL, NULL, NULL, 0) == NULL);
    CHECK(ExtractTapePayloads(NULL, 100, 1, &out_len, NULL, NULL, NULL, 0) == NULL);
    CHECK(ExtractTapePayloads(b.data, 0, 0, &out_len, NULL, NULL, NULL, 0) == NULL);
    for (size_t n = 0; n <= 10; n++)
        CHECK(GetTZXBlock(0, (const uint8_t *)"ZXTape!\x1a\x01\x14", n, &length) == NULL);
    CHECK(GetTZXBlock(0, NULL, 100, &length) == NULL);

    free(b.data);
}

static void PutTAPBlock(Buf *b, uint8_t flag, size_t n)
{
    PutLE(b, (uint32_t)n, 2);
    if (n)
        PutByte(b, flag);
    for (size_t i = 1; i < n; i++)
        PutByte(b, (uint8_t)(flag + i * 3));
}

static void TestTAP(void)
{
    Buf b = { 0 };
    PutTAPBlock(&b, 0x00, 19);
    PutTAPBlock(&b, 0xff, 500);
    PutTAPBlock(&b, 0x55, 2);
    PutTAPBlock(&b, 0xff, 1);
    PutTAPBlock(&b, 0xff, 40);
    size_t whole = b.length;

    uint8_t *tape = Exact(&b);
    /* Every block counts and comes whole, as from a .tzx */
    static const size_t sizes[] = { 19, 500, 2, 1, 40 };
    static const uint8_t flags[] = { 0x00, 0xff, 0x55, 0xff, 0xff };
    size_t length = 0;
    uint8_t *block;
    for (int i = 0; i < 5; i++) {
        block = GetTAPBlock(i, tape, whole, &length);
        CHECK(IsBlock(block, length, flags[i], sizes[i]));
        free(block);
    }
    length = 12345;
    CHECK(GetTAPBlock(5, tape, whole, &length) == NULL && length == 12345);
    CHECK(GetTAPBlock(-1, tape, whole, &length) == NULL && length == 12345);
    CHECK(GetTAPBlock(0, tape, whole, NULL) == NULL);
    CHECK(GetTAPBlock(0, NULL, 100, &length) == NULL);

    size_t out_len = 0, offsets[8], lengths[8];
    int count = 0;
    uint8_t *payloads = ExtractTapePayloads(tape, whole, 0, &out_len, offsets, lengths, &count, 8);
    CHECK(payloads != NULL && count == 2 && out_len == 498 + 38);
    CHECK(offsets[0] == 0 && lengths[0] == 498 && offsets[1] == 498 && lengths[1] == 38);
    CHECK(payloads && payloads[0] == (uint8_t)(0xff + 3) && payloads[498] == (uint8_t)(0xff + 3));
    free(payloads);

    /* Cut anywhere */
    for (size_t cut = 0; cut < whole; cut++) {
        uint8_t *part = malloc(cut ? cut : 1);
        memcpy(part, tape, cut);
        size_t part_len = 0;
        payloads = ExtractTapePayloads(part, cut, 0, &part_len, NULL, NULL, NULL, 0);
        CHECK(part_len <= 498 + 38);
        free(payloads);
        for (int i = 0; i < 6; i++)
            free(GetTAPBlock(i, part, cut, &length));
        free(part);
    }
    free(tape);

    /* A block of no length is the end of the tape, whatever follows it */
    b.length = 0;
    PutTAPBlock(&b, 0xff, 30);
    PutTAPBlock(&b, 0xff, 0);
    PutTAPBlock(&b, 0xff, 30);
    block = GetTAPBlock(0, b.data, b.length, &length);
    CHECK(IsBlock(block, length, 0xff, 30));
    free(block);
    CHECK(GetTAPBlock(1, b.data, b.length, &length) == NULL);
    CHECK(GetTAPBlock(2, b.data, b.length, &length) == NULL);
    payloads = ExtractTapePayloads(b.data, b.length, 0, &out_len, NULL, NULL, &count, 8);
    CHECK(payloads != NULL && out_len == 28 && count == 1);
    free(payloads);

    /* A file of zeroes, and one whose first length is past its end */
    b.length = 0;
    PutFill(&b, 0, 4000);
    CHECK(GetTAPBlock(0, b.data, b.length, &length) == NULL);
    CHECK(ExtractTapePayloads(b.data, b.length, 0, &out_len, NULL, NULL, NULL, 0) == NULL);
    memset(b.data, 0xff, b.length);
    CHECK(GetTAPBlock(0, b.data, b.length, &length) == NULL);
    CHECK(ExtractTapePayloads(b.data, b.length, 0, &out_len, NULL, NULL, NULL, 0) == NULL);

    free(b.data);
}

/* --- Alkatraz --- */

/* What the loader's decryption undoes, written the other way around: the
   same walk of the key, with the plain byte going in */
static void Encrypt(uint8_t *out, const uint8_t *plain, uint16_t target, uint16_t count,
    uint8_t *key, AlkatrazKey k)
{
    while (count) {
        uint8_t hi = count >> 8, lo = count & 0xff;
        *out++ = *plain ^ *key ^ hi ^ lo ^ (target >> 8) ^ (target & 0xff);
        if (k.adjust_address && target == k.adjust_address)
            k.adjust = *plain;
        if (k.step_address && target == k.step_address)
            k.step = *plain;
        if (lo & 0x10)
            *key = (uint8_t)(*key + k.adjust - hi + lo);
        *key = (uint8_t)(*key + k.step);
        plain++;
        target++;
        count--;
    }
}

static void TestAlkatraz(void)
{
    uint8_t *mem = ZXNewMemory();
    CHECK(mem != NULL);
    if (mem == NULL)
        return;
    int zero = 1;
    for (int i = 0; i < ZX_MEMORY_SIZE; i++)
        zero = zero && mem[i] == 0;
    CHECK(zero);

    /* Worked out by hand: two bytes with the count at 0x10 and then 0x0f */
    const AlkatrazKey plain_key = { 7, 3, 0, 0 };
    uint8_t zeroes[2] = { 0, 0 }, key = 0x55;
    CHECK(DeAlkatraz(zeroes, 2, 0, mem, 0x8000, 0x10 - 0x0e, &key, &plain_key) == 1);
    /* count 2: 0x55 ^ 0x02 ^ 0x80 = 0xd7, key 0x58; count 1: 0x58 ^ 1 ^ 0x80 ^ 1 */
    CHECK(mem[0x8000] == 0xd7 && mem[0x8001] == 0xd8 && key == 0x5b);

    static uint8_t one[0x10];
    key = 0x55;
    CHECK(DeAlkatraz(one, sizeof one, 0, mem, 0x8000, 0x10, &key, &plain_key) == 1);
    /* count 0x10: 0x55 ^ 0x10 ^ 0x80 = 0xc5, and the key becomes
       0x55 + 7 - 0 + 0x10 + 3 = 0x6f; count 0x0f: 0x6f ^ 0x0f ^ 0x80 ^ 0x01 */
    CHECK(mem[0x8000] == 0xc5 && mem[0x8001] == 0xe1);

    /* There and back, with a loader that changes its own numbers as well */
    static uint8_t plain[0x2000], crypt[0x2000 + 5];
    for (size_t i = 0; i < sizeof plain; i++)
        plain[i] = (uint8_t)(i * 31 + (i >> 7));
    const AlkatrazKey schedules[] = {
        { 0xc1, 0xcb, 0, 0 },
        { 0xde, 0xe8, 0xe022, 0xe02f },
        { 0xde, 0xe8, 0xe02f, 0xe022 },
    };
    for (int s = 0; s < 3; s++) {
        uint8_t enc_key = 0xa7, dec_key = 0xa7;
        Encrypt(crypt + 5, plain, 0xd000, sizeof plain, &enc_key, schedules[s]);
        memset(mem, 0, ZX_MEMORY_SIZE);
        CHECK(DeAlkatraz(crypt, sizeof crypt, 5, mem, 0xd000, sizeof plain, &dec_key, &schedules[s]) == 1);
        CHECK(memcmp(mem + 0xd000, plain, sizeof plain) == 0);
        CHECK(dec_key == enc_key);
        CHECK(mem[0xcfff] == 0 && mem[0xd000 + sizeof plain] == 0);
        /* the numbers of the caller are as they were */
        CHECK(schedules[1].adjust == 0xde && schedules[1].step == 0xe8);

        /* The next block goes on with the key of the one before */
        Encrypt(crypt, plain, 0x6000, 0x200, &enc_key, schedules[s]);
        CHECK(DeAlkatraz(crypt, 0x200, 0, mem, 0x6000, 0x200, &dec_key, &schedules[s]) == 1);
        CHECK(memcmp(mem + 0x6000, plain, 0x200) == 0 && dec_key == enc_key);
    }
    /* The self-modifying loader does not decrypt as the plain one does */
    {
        uint8_t k1 = 1, k2 = 1;
        static uint8_t a[0x2000], b[0x2000];
        Encrypt(a, plain, 0xd000, sizeof plain, &k1, schedules[0]);
        const AlkatrazKey same_but = { 0xc1, 0xcb, 0xe022, 0xe02f };
        Encrypt(b, plain, 0xd000, sizeof plain, &k2, same_but);
        CHECK(memcmp(a, b, sizeof a) != 0);
    }

    /* Too little to decrypt: nothing is done */
    memset(mem, 0, ZX_MEMORY_SIZE);
    key = 0x33;
    uint8_t *small = malloc(100);
    memset(small, 0xaa, 100);
    CHECK(DeAlkatraz(small, 100, 0, mem, 0x8000, 101, &key, &plain_key) == 0);
    CHECK(DeAlkatraz(small, 100, 1, mem, 0x8000, 100, &key, &plain_key) == 0);
    CHECK(DeAlkatraz(small, 100, 101, mem, 0x8000, 0, &key, &plain_key) == 0);
    CHECK(DeAlkatraz(small, 100, (size_t)-1, mem, 0x8000, 2, &key, &plain_key) == 0);
    CHECK(DeAlkatraz(NULL, 100, 0, mem, 0x8000, 1, &key, &plain_key) == 0);
    CHECK(DeAlkatraz(small, 100, 0, NULL, 0x8000, 1, &key, &plain_key) == 0);
    CHECK(key == 0x33 && mem[0x8000] == 0);
    CHECK(DeAlkatraz(small, 100, 100, mem, 0x8000, 0, &key, &plain_key) == 1);
    /* The address wraps around as HL does */
    CHECK(DeAlkatraz(small, 100, 0, mem, 0xffff, 100, &key, &plain_key) == 1);
    CHECK(mem[0x8000] == 0);
    free(small);

    /* LDIR one byte along is a fill; LDDR moves a block up over itself */
    memset(mem, 0, ZX_MEMORY_SIZE);
    mem[0x9000] = 0x5a;
    ZXLdir(mem, 0x9001, 0x9000, 99);
    CHECK(mem[0x9063] == 0x5a && mem[0x9064] == 0);
    memcpy(mem + 0xa000, "ABCDEF", 6);
    ZXLddr(mem, 0xa007, 0xa005, 6);
    CHECK(memcmp(mem + 0xa000, "ABABCDEF", 8) == 0);
    ZXLdir(mem, 0xa000, 0xa002, 6);
    CHECK(memcmp(mem + 0xa000, "ABCDEFEF", 8) == 0);
    ZXLdir(mem, 0x0000, 0xffff, 0);
    mem[0xffff] = 0x11;
    mem[0x0000] = 0x22;
    ZXLdir(mem, 0x4000, 0xffff, 2); /* the addresses wrap around */
    CHECK(mem[0x4000] == 0x11 && mem[0x4001] == 0x22);

    /* Two gaps put back into "ABCDEF": the table is gone through from its
       last entry, so the gap higher up comes last in it... */
    memset(mem, 0, ZX_MEMORY_SIZE);
    memcpy(mem + 0x8000, "ABCDEF", 6);
    static const uint8_t table[] = {
        0x02, 0x80, 0x02, 0x00, '*', /* three of '*' at 0x8002 */
        0x08, 0x80, 0x00, 0x00, '+', /* then one '+' at 0x8008 */
    };
    memcpy(mem + 0x9000, table + 5, 5);
    memcpy(mem + 0x9005, table, 5);
    DeshuffleAlkatraz(mem, 2, 0x9009, 0x8005);
    CHECK(memcmp(mem + 0x8000, "AB***CDE+F", 10) == 0);
    DeshuffleAlkatraz(mem, 0, 0x9009, 0x8005);
    CHECK(memcmp(mem + 0x8000, "AB***CDE+F", 10) == 0);
    /* A table at the very top of memory is read without going past it */
    memset(mem, 0xff, ZX_MEMORY_SIZE);
    DeshuffleAlkatraz(mem, 255, 0xffff, 0xffff);
    DeshuffleAlkatraz(mem, 255, 0x0002, 0x0000);

    free(mem);
}

/* --- The screen --- */

static void TestScreen(void)
{
    /* The bitmap is three bands of eight character rows. In a band the eight
       pixel rows of all character rows come first, then the second of all. */
    CHECK(ZXBitmapRow(0) == 0 && ZXBitmapRow(31) == 0);
    CHECK(ZXBitmapRow(32) == 8);
    CHECK(ZXBitmapRow(256) == 1);
    CHECK(ZXBitmapRow(2047) == 63);
    CHECK(ZXBitmapRow(2048) == 64);
    CHECK(ZXBitmapRow(6143) == 191);
    CHECK(ZXBitmapOffset(0, 0) == 0 && ZXBitmapOffset(8, 3) == 35);
    CHECK(ZXBitmapOffset(1, 0) == 256 && ZXBitmapOffset(191, 31) == 6143);

    static uint8_t seen[ZX_BITMAP_SIZE];
    int ok = 1;
    for (int y = 0; y < ZX_SCREEN_HEIGHT; y++)
        for (int col = 0; col < ZX_SCREEN_COLS; col++) {
            int offset = ZXBitmapOffset(y, col);
            ok = ok && offset >= 0 && offset < ZX_BITMAP_SIZE && !seen[offset]
                && ZXBitmapRow(offset) == y && offset % ZX_SCREEN_COLS == col;
            if (offset >= 0 && offset < ZX_BITMAP_SIZE)
                seen[offset] = 1;
        }
    CHECK(ok);

    int ink = -1, paper = -1;
    ZXDecodeAttr(0x38, &ink, &paper);
    CHECK(ink == 0 && paper == 7);
    ZXDecodeAttr(0x47, &ink, &paper);
    CHECK(ink == 15 && paper == 8);
    ZXDecodeAttr(0xd1, &ink, &paper); /* flashing bright blue on red */
    CHECK(ink == 9 && paper == 10);
    ZXDecodeAttr(0x38, NULL, NULL);
    ZXDecodeAttr(0x0a, &ink, NULL);
    CHECK(ink == 2);

    CHECK(ZXPalette[0] == 0x000000 && ZXPalette[8] == 0x000000);
    CHECK(ZXPalette[15] == 0xffffff);
    for (int i = 1; i < 8; i++) {
        /* blue, red and green are bits 0, 1 and 2, and bright is brighter */
        CHECK(((ZXPalette[i] & 0x0000ff) != 0) == ((i & 1) != 0));
        CHECK(((ZXPalette[i] & 0xff0000) != 0) == ((i & 2) != 0));
        CHECK(((ZXPalette[i] & 0x00ff00) != 0) == ((i & 4) != 0));
        CHECK(ZXPalette[i + 8] > ZXPalette[i]);
    }

    uint8_t *screen = malloc(ZX_SCREEN_SIZE);
    memset(screen, 0xff, ZX_BITMAP_SIZE);
    memset(screen + ZX_BITMAP_SIZE, 0x38, ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);
    CHECK(ZXScreenIsBlackOnWhite(screen) == 1);
    screen[ZX_SCREEN_SIZE - 1] = 0x78 | 0x80; /* bright and flashing */
    CHECK(ZXScreenIsBlackOnWhite(screen) == 1);
    screen[ZX_SCREEN_SIZE - 1] = 0x39;
    CHECK(ZXScreenIsBlackOnWhite(screen) == 0);
    screen[ZX_SCREEN_SIZE - 1] = 0x38;
    screen[ZX_BITMAP_SIZE] = 0x07;
    CHECK(ZXScreenIsBlackOnWhite(screen) == 0);
    CHECK(ZXScreenIsBlackOnWhite(NULL) == 1);
    free(screen);
}

/* A SCREEN$ as PutTAPBlock and PutTZXData make it: payload byte i is
   0xff + (i + 1) * 3 */
static int IsScreenPayload(const uint8_t *screen)
{
    if (screen == NULL)
        return 0;
    for (size_t i = 0; i < ZX_SCREEN_SIZE; i++)
        if (screen[i] != (uint8_t)(0xff + (i + 1) * 3))
            return 0;
    return 1;
}

static void TestLoadingScreen(void)
{
    const size_t screen_block = ZX_SCREEN_SIZE + ZX_TAPE_FRAME_BYTES;

    /* Passed over: a header, data one byte too long and one too short, and a
       block of the right length that is not data. The first screen is found,
       not the second. */
    Buf b = { 0 };
    PutTAPBlock(&b, 0x00, 19);
    PutTAPBlock(&b, 0xff, screen_block + 1);
    PutTAPBlock(&b, 0xff, screen_block - 1);
    PutTAPBlock(&b, 0x55, screen_block);
    PutTAPBlock(&b, 0xff, screen_block);
    size_t second = b.length;
    PutTAPBlock(&b, 0xff, screen_block);
    b.data[second + 3] ^= 0xaa;
    uint8_t *screen = FindTapeLoadingScreen(b.data, b.length, 0);
    CHECK(IsScreenPayload(screen));
    free(screen);
    /* Asked for as a .tzx, a .tap is not one */
    CHECK(FindTapeLoadingScreen(b.data, b.length, 1) == NULL);

    /* No screen-sized data block */
    b.length = 0;
    PutTAPBlock(&b, 0x00, 19);
    PutTAPBlock(&b, 0xff, 500);
    CHECK(FindTapeLoadingScreen(b.data, b.length, 0) == NULL);

    /* Only the first 64 data blocks are searched */
    for (int stop = 63; stop <= 64; stop++) {
        b.length = 0;
        for (int i = 0; i < stop; i++)
            PutTAPBlock(&b, 0xff, 10);
        PutTAPBlock(&b, 0xff, screen_block);
        screen = FindTapeLoadingScreen(b.data, b.length, 0);
        CHECK((screen != NULL) == (stop == 63));
        CHECK(screen == NULL || IsScreenPayload(screen));
        free(screen);
    }

    /* The same in a .tzx */
    b.length = 0;
    PutTZXHeader(&b);
    PutTZXData(&b, 0x00, 19);
    PutTZXData(&b, 0xff, screen_block);
    screen = FindTapeLoadingScreen(b.data, b.length, 1);
    CHECK(IsScreenPayload(screen));
    free(screen);

    CHECK(FindTapeLoadingScreen(NULL, 100, 0) == NULL);
    CHECK(FindTapeLoadingScreen(b.data, 0, 1) == NULL);

    free(b.data);
}

int main(void)
{
    for (int i = 0; i < 16; i++)
        FillPage(ram[i], i);

    TestV1();
    TestMachines();
    Test128K();
    TestBadPages();
    TestTZX();
    TestTAP();
    TestLoadingScreen();
    TestAlkatraz();
    TestScreen();

    printf("unit tests: %d passed, %d failed\n", checks - failures, failures);
    return failures ? 1 : 0;
}
