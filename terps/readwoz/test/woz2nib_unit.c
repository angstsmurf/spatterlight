/*
 * woz2nib_unit -- tests the parts of woz2nib.c one at a time, on bitstreams
 * and WOZ files made up here, so that it needs no disk images. Run it with
 * ./unit.sh. It includes woz2nib.c to get at its static functions.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../woz2nib.c"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL    %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                \
        }                                                              \
    } while (0)

/* A bitstream that is written a bit at a time, most significant bit first. */
typedef struct {
    uint8_t bytes[80000];
    uint32_t bits;
} Stream;

static void PutBits(Stream *s, uint32_t value, int count)
{
    while (count-- > 0) {
        if ((value >> count) & 1)
            s->bytes[s->bits >> 3] |= (uint8_t)(0x80 >> (s->bits & 7));
        s->bits++;
    }
}

static void PutSync(Stream *s, int count, int zeros)
{
    while (count-- > 0) {
        PutBits(s, 0xff, 8);
        PutBits(s, 0, zeros);
    }
}

/* The stream in a buffer of its own that ends where it does, for the address
 * sanitizer to see a read past it, with the bits left over in its last byte
 * set, for the decoder to trip on if it reads them. The caller frees it. */
static uint8_t *Exact(const Stream *s)
{
    size_t size = (s->bits + 7) / 8;
    uint8_t *copy = malloc(size);
    memcpy(copy, s->bytes, size);
    if (s->bits % 8)
        copy[size - 1] |= (uint8_t)(0xff >> (s->bits % 8));
    return copy;
}

static uint32_t seed = 1;

static uint32_t Random(void)
{
    seed = seed * 1664525 + 1013904223;
    return seed >> 8;
}

/* The decoder with nothing clever in it: one bit at a time into the latch. */
static size_t ReferenceDecode(const uint8_t *src, uint32_t bit_count, uint32_t p, uint8_t *out, size_t limit)
{
    size_t count = 0;
    int reg = 0;
    for (uint64_t guard = 0; count < limit && guard < (uint64_t)bit_count * WOZ_GUARD_REVOLUTIONS; guard++) {
        reg = ((reg << 1) | ((src[p >> 3] >> (7 - (p & 7))) & 1)) & 0xff;
        if (reg & 0x80) {
            out[count++] = (uint8_t)reg;
            reg = 0;
        }
        if (++p == bit_count)
            p = 0;
    }
    return count;
}

static void TestSmallThings(void)
{
    CHECK(woz_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926);
    CHECK(woz_crc32((const uint8_t *)"", 0) == 0);

    static const uint8_t high[4] = { 0x01, 0x02, 0x03, 0xff };
    CHECK(READ_LE_UINT32(high) == 0xff030201);

    CHECK(is_woz_image((const uint8_t *)"WOZ1", 4));
    CHECK(is_woz_image((const uint8_t *)"WOZ2", 4));
    CHECK(!is_woz_image((const uint8_t *)"WOZ2", 3));
    CHECK(!is_woz_image((const uint8_t *)"WOZ3", 4));
    CHECK(!is_woz_image((const uint8_t *)"woz1", 4));
    CHECK(!is_woz_image(NULL, 0));

    CHECK(SYNC16_BIT_LENGTH == 50);
    CHECK(SYNC13_BIT_LENGTH == 72);
}

/* A gap of sync bytes after some bytes that hold no sync, starting at each
 * bit of a byte: found, and the position is that of the bit after a pattern,
 * where a sync byte begins. */
static void TestSyncSearch(void)
{
    for (int zeros = 1; zeros <= 2; zeros++) {
        uint32_t pattern_bits = zeros == 2 ? SYNC16_BIT_LENGTH : SYNC13_BIT_LENGTH;
        for (int offset = 0; offset < 8; offset++) {
            static Stream s;
            memset(&s, 0, sizeof s);
            for (int i = 0; i < 40; i++)
                PutBits(&s, 0xaa, 8);
            PutBits(&s, 0, offset);
            uint32_t gap = s.bits;
            PutSync(&s, 12, zeros);
            for (int i = 0; i < 400; i++)
                PutBits(&s, 0x96, 8);
            /* Byte-aligned matches are looked for first, so the pattern found
             * need not begin with the first sync byte of the gap. */
            uint8_t *bytes = Exact(&s);
            uint32_t found = find_sync_end(bytes, s.bits);
            free(bytes);
            CHECK(found >= gap + pattern_bits && found <= gap + 12 * (8 + (uint32_t)zeros));
            CHECK((found - gap - pattern_bits) % (8 + (uint32_t)zeros) == 0);
            if (offset == 0)
                CHECK(found == gap + pattern_bits);
        }
    }

    /* no gap at all, and a track too short to search */
    static Stream s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 400; i++)
        PutBits(&s, 0x96, 8);
    CHECK(find_sync_end(s.bytes, s.bits) == 0);
    CHECK(find_sync_end(s.bytes, 72) == 0);

    /* all ones, which sends the search as far into the track as it goes */
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 400; i++)
        PutBits(&s, 0xff, 8);
    uint8_t *ones = Exact(&s);
    CHECK(find_sync_end(ones, s.bits) == 0);
    free(ones);

    /* The one gap of the track lies across its end: not found, which is how
     * it has always been. The decoder then starts at bit 0. */
    memset(&s, 0, sizeof s);
    PutBits(&s, 0, 2);
    PutSync(&s, 2, 2);
    for (int i = 0; i < 400; i++)
        PutBits(&s, 0x96, 8);
    PutSync(&s, 2, 2);
    PutBits(&s, 0xff, 8);
    CHECK(find_sync_end(s.bytes, s.bits) == 0);
}

/* Tracks of sectors-like data, of noise and of next to nothing, of lengths
 * that are not a whole number of bytes, read from anywhere: the decoder gives
 * what the reference gives. */
static void TestDecode(void)
{
    static Stream s;
    static uint8_t got[WOZ_NIB_TRACK_LEN], want[WOZ_NIB_TRACK_LEN + 1];

    for (int round = 0; round < 400; round++) {
        memset(&s, 0, sizeof s);
        int kind = round % 4;
        uint32_t target = kind == 3 ? 200 + Random() % 3000 : 30000 + Random() % 25000;
        while (s.bits < target) {
            if (kind == 0) {            /* gaps and runs of nibbles */
                PutSync(&s, 5 + (int)(Random() % 40), 2);
                for (int n = 20 + (int)(Random() % 400); n > 0; n--)
                    PutBits(&s, 0x80 | Random(), 8);
            } else if (kind == 1) {     /* noise */
                PutBits(&s, Random(), 8);
            } else {                    /* long stretches of nothing */
                PutBits(&s, 0, (int)(Random() % 30));
                for (int n = (int)(Random() % 12); n > 0; n--)
                    PutBits(&s, 0, 32);
                if (Random() % 4)
                    PutBits(&s, 0x80 | Random(), 8);
            }
        }
        s.bits -= Random() % 8;
        uint8_t *bytes = Exact(&s);
        uint32_t start = round % 8 == 0 ? find_sync_end(bytes, s.bits) : Random() % s.bits;

        memset(got, 0, sizeof got);
        size_t ngot = woz_decode_track(bytes, s.bits, start, got);
        size_t nwant = ReferenceDecode(bytes, s.bits, start, want, sizeof want);
        free(bytes);
        if (nwant >= WOZ_NIB_TRACK_LEN) {
            CHECK(ngot == WOZ_NIB_TRACK_LEN);
        } else {
            /* stopped by the guard, which the fast path may pass by the
             * bits of one step: one nibble more at the most */
            CHECK(ngot == nwant || ngot == nwant + 1);
            if (ngot == nwant + 1)
                ngot = nwant;
        }
        CHECK(memcmp(got, want, ngot) == 0);
    }

    /* nothing on the track at all: comes to an end, with nothing */
    memset(&s, 0, sizeof s);
    CHECK(woz_decode_track(s.bytes, 51200, 0, got) == 0);
    CHECK(woz_decode_track(s.bytes, 73, 5, got) == 0);
}

/*
 * A WOZ with one track on it, at whole track 0, as version 1 or 2.
 * Returns the size of the file. The patches are made before the checksum.
 */
enum {
    kPlain,
    kNoChecksum,
    kWrongChecksum,
    kTrailingJunk,
    kBadMap,
    kOtherTrackBadMap,
    kTrackPastEnd,
    kSmallTrks,
    kHugeBitCount,
    kHugeChunk,
    kThreeAndAHalfInch,
    kTwoSided,
    kNoTmap,
    kEmptyChunk,
};

static uint8_t file[20000];
static Stream track;

static void PutChunk(size_t *pos, const char *id, uint32_t size)
{
    memcpy(file + *pos, id, 4);
    for (int i = 0; i < 4; i++)
        file[*pos + 4 + (size_t)i] = (uint8_t)(size >> (8 * i));
    *pos += 8;
}

static size_t MakeWoz(int version, int how)
{
    size_t pos = 12;
    uint32_t stream_bytes = (track.bits + 7) / 8;

    memset(file, 0, sizeof file);
    memcpy(file, version == 2 ? "WOZ2\xff\n\r\n" : "WOZ1\xff\n\r\n", 8);

    PutChunk(&pos, "INFO", 60);
    file[pos] = (uint8_t)version;
    file[pos + 1] = how == kThreeAndAHalfInch ? 2 : 1;
    memset(file + pos + 5, ' ', 32);
    file[pos + 37] = how == kTwoSided ? 2 : 1;
    pos += 60;

    if (how == kEmptyChunk)
        PutChunk(&pos, "WRIT", 0);

    if (how != kNoTmap) {
        PutChunk(&pos, "TMAP", 160);
        memset(file + pos, 0xff, 160);
        file[pos] = how == kBadMap ? 0xa0 : 0;
        if (how == kOtherTrackBadMap)
            file[pos + 4] = 0xfe;
        pos += 160;
    }

    uint32_t bit_count = how == kHugeBitCount ? 0xffffffff : track.bits;
    if (version == 2) {
        uint32_t trks_size = how == kSmallTrks ? 8 : 1280 + 512 * 14;
        PutChunk(&pos, "TRKS", how == kHugeChunk ? 0xfffffff0 : trks_size);
        if (how != kSmallTrks) {
            size_t block = (pos + 1280 + 511) / 512;
            size_t at = how == kTrackPastEnd ? 0xffff : block;
            file[pos] = (uint8_t)at;
            file[pos + 1] = (uint8_t)(at >> 8);
            file[pos + 2] = 13;
            for (int i = 0; i < 4; i++)
                file[pos + 4 + (size_t)i] = (uint8_t)(bit_count >> (8 * i));
            memcpy(file + block * 512, track.bytes, stream_bytes);
        }
        pos += trks_size;
    } else {
        PutChunk(&pos, "TRKS", how == kHugeChunk ? 0xfffffff0 : 2 * 6656);
        memcpy(file + pos, track.bytes, stream_bytes);
        file[pos + 6646] = (uint8_t)stream_bytes;
        file[pos + 6647] = (uint8_t)(stream_bytes >> 8);
        file[pos + 6648] = (uint8_t)bit_count;
        file[pos + 6649] = (uint8_t)(bit_count >> 8);
        pos += 2 * 6656;
    }

    if (how == kTrailingJunk) {
        memcpy(file + pos, "junk", 4);
        pos += 4;
    }

    uint32_t crc = how == kNoChecksum ? 0 : woz_crc32(file + 12, pos - 12);
    if (how == kWrongChecksum)
        crc ^= 0x80000000;
    for (int i = 0; i < 4; i++)
        file[8 + i] = (uint8_t)(crc >> (8 * i));
    return pos;
}

static void TestContainer(void)
{
    static uint8_t want[WOZ_NIB_TRACK_LEN];
    static const uint8_t zeros[WOZ_NIB_TRACK_LEN];

    memset(&track, 0, sizeof track);
    for (int sector = 0; sector < 16; sector++) {
        PutSync(&track, 14, 2);
        for (int n = 0; n < 380; n++)
            PutBits(&track, 0x80 | Random(), 8);
    }
    track.bits -= 3;
    woz_decode_track(track.bytes, track.bits, find_sync_end(track.bytes, track.bits), want);

    for (int version = 1; version <= 2; version++) {
        /* files that convert, with the one track or without it */
        static const struct { int how, track0; } good[] = {
            { kPlain, 1 }, { kNoChecksum, 1 }, { kTrailingJunk, 1 }, { kEmptyChunk, 1 },
            { kOtherTrackBadMap, 1 }, { kBadMap, 0 }, { kTrackPastEnd, 0 }, { kHugeBitCount, 0 },
        };
        for (size_t i = 0; i < sizeof good / sizeof good[0]; i++) {
            int how = good[i].how, track0 = good[i].track0;
            /* version 1 has no starting block to be wrong, and its bit count
             * is of 16 bits and held to the size of the record */
            if (version == 1 && (how == kTrackPastEnd || how == kHugeBitCount))
                continue;
            size_t size = MakeWoz(version, how);
            uint8_t *nib = woz2nib(file, &size);
            CHECK(nib != NULL);
            if (nib == NULL) {
                printf("        (version %d, case %d)\n", version, how);
                continue;
            }
            CHECK(size == WOZ_NIB_IMAGE_SIZE);
            CHECK(memcmp(nib, track0 ? want : zeros, WOZ_NIB_TRACK_LEN) == 0);
            for (int t = 1; t < WOZ_NIB_TRACKS; t++)
                CHECK(memcmp(nib + t * WOZ_NIB_TRACK_LEN, zeros, WOZ_NIB_TRACK_LEN) == 0);
            free(nib);
        }

        /* files that are refused, which leaves the size alone */
        static const int bad[] = { kWrongChecksum, kSmallTrks, kHugeChunk, kThreeAndAHalfInch, kTwoSided, kNoTmap };
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            if (version == 1 && (bad[i] == kSmallTrks || bad[i] == kTwoSided))
                continue;
            size_t made = MakeWoz(version, bad[i]), size = made;
            uint8_t *nib = woz2nib(file, &size);
            CHECK(nib == NULL);
            CHECK(size == made);
            free(nib);
        }

        /* cut short anywhere in the header and the chunks, without a checksum
         * to catch it: an exact copy, so that reading past the end shows */
        size_t whole = MakeWoz(version, kNoChecksum);
        for (size_t cut = 0; cut < 400 && cut < whole; cut++) {
            uint8_t *copy = malloc(cut ? cut : 1);
            memcpy(copy, file, cut);
            size_t size = cut;
            uint8_t *nib = woz2nib(copy, &size);
            CHECK(nib == NULL || size == WOZ_NIB_IMAGE_SIZE);
            CHECK(nib != NULL || size == cut);
            free(nib);
            free(copy);
        }
    }

    /* a version 1 record whose bit count runs into the next record is held
     * to its own 6646 bytes */
    size_t size = MakeWoz(1, kPlain);
    WozImage image;
    file[12 + 8 + 60 + 8 + 160 + 8 + 6648] = 0xff;
    file[12 + 8 + 60 + 8 + 160 + 8 + 6649] = 0xff;
    CHECK(woz_parse_chunks(file, size, &image));
    CHECK(image.trks[0].bit_count == 6646 * 8);
}

int main(void)
{
    TestSmallThings();
    TestSyncSearch();
    TestDecode();
    TestContainer();
    printf("woz2nib unit tests: %s\n", failures ? "FAILED" : "passed");
    return failures != 0;
}
