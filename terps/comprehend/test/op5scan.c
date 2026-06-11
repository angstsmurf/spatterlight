/* op5scan.c -- extract every image file from an Apple II Comprehend woz/dsk
 * disk and report which images use opcode 5 (OPCODE_TEXT_OUTLINE).
 *
 * Usage: op5scan <legacy:0|1> <disk1> [disk2 ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <strings.h>

#include "../../readwoz/ciderpress.h"
#include "../../readwoz/woz2nib.h"

#define DSK_IMAGE_SIZE (35 * 16 * 256)
#define NIB_IMAGE_SIZE (35 * 6656)

static uint8_t *readFile(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return NULL; }
    fclose(f); *sz = n; return d;
}

static uint8_t *toDsk(uint8_t *data, size_t size, size_t *outSize) {
    if (size >= 4 && data[0]=='W' && data[1]=='O' && data[2]=='Z') {
        size_t nibSize = size;
        uint8_t *nib = woz2nib(data, &nibSize);
        if (!nib) return NULL;
        uint8_t *dsk = ReadFullImageFromNib(nib, nibSize);
        free(nib);
        *outSize = DSK_IMAGE_SIZE;
        return dsk;
    }
    if (size == NIB_IMAGE_SIZE) { *outSize = DSK_IMAGE_SIZE; return ReadFullImageFromNib(data, size); }
    if (size >= DSK_IMAGE_SIZE) { uint8_t *d=malloc(DSK_IMAGE_SIZE); memcpy(d,data,DSK_IMAGE_SIZE); *outSize=DSK_IMAGE_SIZE; return d; }
    return NULL;
}

/* Walk one image's opcode stream; return number of op5 (TEXT_OUTLINE) seen.
 * Also count op3 (TEXT_CHAR) for context. legacy: op7/op15 terminate. */
static int g_trace = 0;
static const char *g_opname[16] = {"END","TEXTPOS","PEN","CHAR","SHAPE","OUTLINE","FILLCOL","END2",
    "MOVE","BOX","LINE","CIRCLE","DRAWSHP","DELAY","PAINT","RESET"};

/* Dialects (how op7/op15 are parsed), matching the interpreters:
 *   MODE_GM   = graphics_magician non-legacy (Apple Talisman/OO-Topos):
 *               op7 = 2-operand no-op, op15 = RESET (param 2 => 4 operands)
 *   MODE_GM_LEGACY = graphics_magician legacy (Apple Transylvania/Crimson Crown):
 *               op7 and op15 both terminate the image
 *   MODE_DOS  = pics.cpp non-Talisman DOS games (Transylvania/Crimson Crown/
 *               OO-Topos): op7 terminates, op15 = RESET with NO operands and
 *               does not terminate.
 *   MODE_DOS_TALISMAN = pics.cpp Talisman DOS: op7 = 2-operand no-op (NOT end),
 *               op15 = RESET with operands only for sub-op 2 (4 bytes); none
 *               terminate. Talisman's vector data is byte-identical on Apple and
 *               DOS, so this matches MODE_GM exactly -- which is the point: the
 *               two walks must agree, since MODE_GM is the pixel-validated
 *               dialect (graphics_magician). (Before the pics.cpp fix, op15 read
 *               one operand unconditionally, desyncing the stream and fabricating
 *               phantom op5 hits; this mode now mirrors the corrected reader.)
 * All share identical operand sizes for every other opcode. */
enum { MODE_GM = 0, MODE_GM_LEGACY = 1, MODE_DOS = 2, MODE_DOS_TALISMAN = 3 };

/* Advance past one opcode's operands. Returns 1 if the opcode terminates the
 * image, else 0. op3/op5 tally TEXT_CHAR / TEXT_OUTLINE. */
static int stepOp(const uint8_t **pp, int mode, int *op3, int *op5) {
    uint8_t opcode = *(*pp)++;
    uint8_t param = opcode & 0xf;
    opcode >>= 4;
    switch (opcode) {
    case 0: return 1;                                  /* END */
    case 1: *pp += 2; break;                           /* SET_TEXT_POS */
    case 2: break;                                     /* SET_PEN_COLOR */
    case 3: *pp += 1; if (op3) (*op3)++; break;        /* TEXT_CHAR */
    case 4: break;                                     /* SET_SHAPE */
    case 5: *pp += 1; if (op5) (*op5)++; break;        /* TEXT_OUTLINE */
    case 6: *pp += 1; break;                           /* SET_FILL_COLOR */
    case 7:                                            /* END2 */
        if (mode == MODE_GM_LEGACY || mode == MODE_DOS) return 1;
        *pp += 2; break;                               /* MODE_GM / MODE_DOS_TALISMAN: 2-operand no-op */
    case 8: *pp += 2; break;                           /* MOVE_TO */
    case 9: *pp += 2; break;                           /* DRAW_BOX */
    case 10: *pp += 2; break;                          /* DRAW_LINE */
    case 11: *pp += 1; break;                          /* DRAW_CIRCLE */
    case 12: *pp += 2; break;                          /* DRAW_SHAPE */
    case 13: *pp += 1; break;                          /* DELAY */
    case 14: *pp += 2; break;                          /* PAINT */
    case 15:                                           /* RESET */
        if (mode == MODE_GM_LEGACY) return 1;
        /* MODE_GM and MODE_DOS_TALISMAN: operands only for sub-op 2 (4 bytes of
         * fill bounds); MODE_DOS: zero operands. None terminate. */
        if ((mode == MODE_GM || mode == MODE_DOS_TALISMAN) && param == 2) *pp += 4;
        break;
    }
    return 0;
}

static int scanImage(const uint8_t *p, const uint8_t *end, int mode, int *op3out) {
    int op5 = 0, op3 = 0;
    const uint8_t *base = p;
    while (p < end) {
        const uint8_t *opstart = p;
        if (g_trace)
            fprintf(stderr, "  +%-4ld op=%02x %-8s param=%d\n",
                    (long)(opstart - base), opstart[0], g_opname[(opstart[0]>>4)&0xf], opstart[0]&0xf);
        if (stepOp(&p, mode, &op3, &op5))
            break;
    }
    *op3out = op3;
    return op5;
}

/* Scan a file that is a SINGLE Graphics Magician picture (no offset table):
 * decode from byte 0 to END and report op5 plus how cleanly it terminated. */
static void scanSingleFile(const char *fname, const uint8_t *fb0, size_t flen0, int mode) {
    /* DOS 3.3 BIN files carry a 4-byte load-addr/length header; the Graphics
     * Magician opcode stream begins after it. */
    int skip = getenv("OP5_SKIP") ? atoi(getenv("OP5_SKIP")) : 0;
    const uint8_t *fb = fb0 + skip;
    size_t flen = flen0 - skip;
    /* find where decode stops (END or run-off) to validate the dialect/format */
    const uint8_t *p = fb, *end = fb + flen;
    int op3 = 0;
    const char *tf = getenv("OP5_TRACE_FILE");
    g_trace = (tf && strcasecmp(tf, fname) == 0);
    if (g_trace) fprintf(stderr, "--- trace %s (mode %d) ---\n", fname, mode);
    int op5 = scanImage(fb, end, mode, &op3);
    g_trace = 0;
    /* re-walk to find terminal offset */
    long termAt = -1;
    p = fb;
    while (p < end) {
        const uint8_t *s = p;
        if (stepOp(&p, mode, NULL, NULL)) { termAt = s - fb; break; }
    }
    long trailing = (termAt < 0) ? -1 : (long)flen - (termAt + 1);
    printf("  %-12s size=%5zu  OP5=%d  op3=%d  END@%ld trailing=%ld%s\n",
           fname, flen, op5, op3, termAt, trailing,
           op5 > 0 ? "   <== uses OPCODE 5" : "");
    if (getenv("OP5_HEX")) {
        printf("      first16:");
        for (int k = 0; k < 16 && k < (int)flen; k++) printf(" %02x", fb[k]);
        printf("\n");
    }
}

static void scanFile(const char *fname, const uint8_t *fb, size_t flen, int mode) {
    if (flen < 36) return;
    uint16_t version = fb[0] | (fb[1] << 8);
    size_t base = (version == 0x1000) ? 4 : 0;
    uint16_t off[16];
    for (int i = 0; i < 16; i++) {
        size_t pp = base + i * 2;
        off[i] = fb[pp] | (fb[pp+1] << 8);
        if (version == 0x1000) off[i] += 4;
    }
    const char *tf = getenv("OP5_TRACE_FILE");
    int ti = getenv("OP5_TRACE_IDX") ? atoi(getenv("OP5_TRACE_IDX")) : -1;
    for (int i = 0; i < 16; i++) {
        uint16_t start = off[i];
        if (start == 0 || start >= flen) continue;
        int op3 = 0;
        g_trace = (tf && strcasecmp(tf, fname) == 0 && i == ti);
        if (g_trace) fprintf(stderr, "--- trace %s img %d start=%u ---\n", fname, i, start);
        int op5 = scanImage(fb + start, fb + flen, mode, &op3);
        g_trace = 0;
        printf("  %-6s img %2d  start=%5u  OP5=%d  op3(char)=%d%s\n",
               fname, i, start, op5, op3, op5 > 0 ? "   <== uses OPCODE 5" : "");
    }
}

/* Decide whether a loose file is a Comprehend graphics file and scan it.
 * basename is used for the RA../OA../R1.. naming heuristics. */
static void scanLooseFile(const char *path, int mode) {
    const char *fn = strrchr(path, '/');
    fn = fn ? fn + 1 : path;
    size_t flen = 0;
    uint8_t *fb = readFile(path, &flen);
    if (!fb) { fprintf(stderr, "cannot read %s\n", path); return; }
    int single = getenv("OP5_SINGLE") != NULL;
    if (single)
        scanSingleFile(fn, fb, flen, mode);
    else
        scanFile(fn, fb, flen, mode);   /* 16-image collection (e.g. RA.MS1) */
    free(fb);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "usage: %s <mode:0=gm 1=gm-legacy 2=dos 3=dos-talisman> <disk.woz|.dsk ...>\n"
            "       %s --files <mode> <image-file ...>   (already-extracted files)\n"
            "  env: OP5_SINGLE=1 single-picture-per-file format; OP5_SKIP=N header skip;\n"
            "       OP5_LISTFILES=1, OP5_HEX=1, OP5_TRACE_FILE=NAME OP5_TRACE_IDX=N\n",
            argv[0], argv[0]);
        return 2;
    }

    /* --files mode: each remaining arg is an already-extracted image file. */
    if (strcmp(argv[1], "--files") == 0) {
        if (argc < 4) { fprintf(stderr, "usage: %s --files <mode> <image-file ...>\n", argv[0]); return 2; }
        int mode = atoi(argv[2]);
        for (int a = 3; a < argc; a++)
            scanLooseFile(argv[a], mode);
        return 0;
    }

    int legacy = atoi(argv[1]);
    int totalImagesWithText = 0;
    for (int a = 2; a < argc; a++) {
        size_t rawSize = 0;
        uint8_t *raw = readFile(argv[a], &rawSize);
        if (!raw) { fprintf(stderr, "cannot read %s\n", argv[a]); continue; }
        size_t dskSize = 0;
        uint8_t *dsk = toDsk(raw, rawSize, &dskSize);
        free(raw);
        if (!dsk) { fprintf(stderr, "cannot decode %s\n", argv[a]); continue; }
        size_t numFiles = 0;
        A2FileRec *files = GetAllProDOSFiles(dsk, dskSize, &numFiles);
        if (numFiles == 0) {
            free(files);
            files = GetAllApple2DOSFiles(dsk, dskSize, &numFiles);
            printf("== %s  (%zu files, DOS 3.3) ==\n", argv[a], numFiles);
        } else {
            printf("== %s  (%zu files, ProDOS) ==\n", argv[a], numFiles);
        }
        if (getenv("OP5_LISTFILES"))
            for (size_t i = 0; i < numFiles; i++)
                printf("   file: '%s' (%zu bytes)\n", files[i].filename, files[i].datasize);
        int single = getenv("OP5_SINGLE") != NULL;
        for (size_t i = 0; i < numFiles; i++) {
            const char *fn = files[i].filename;
            if (single) {
                /* Penguin single-picture-per-file releases: R1, O1, R1.DPC ... */
                if ((fn[0]=='R' || fn[0]=='O') && (fn[1]>='0' && fn[1]<='9'))
                    scanSingleFile(fn, files[i].data, files[i].datasize, legacy);
            } else {
                /* 1985 collection format: RA..RG, OA..OF (16-image offset table) */
                int isImg = strlen(fn) == 2 &&
                            (fn[0]=='R' || fn[0]=='O') &&
                            (fn[1]>='A' && fn[1]<='G');
                if (isImg)
                    scanFile(fn, files[i].data, files[i].datasize, legacy);
            }
        }
        for (size_t i = 0; i < numFiles; i++) { free(files[i].filename); free(files[i].data); }
        free(files);
        free(dsk);
    }
    (void)totalImagesWithText;
    return 0;
}
