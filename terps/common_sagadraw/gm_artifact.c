//
//  gm_artifact.c
//
//  Shared Apple II hi-res -> colour kernel. See gm_artifact.h.
//
//  Verbatim extraction of the artifact-colour code that was duplicated between
//  common_sagadraw/apple2draw.c and comprehend/graphics_magician.cpp. The two
//  copies were byte-for-byte identical; this is a pure move, not a rewrite.
//  (Borrowed originally from the MAME Apple II driver.)
//

#include "gm_artifact.h"

// 4-bit left rotate. Bits 4-6 of n must be a copy of bits 0-2.
static unsigned rotl4b(unsigned n, unsigned count) { return (n >> (-count & 3)) & 0x0f; }

static const uint8_t artifact_color_lut[128] =
{
    0x00,0x00,0x00,0x00,0x88,0x00,0xcc,0x00,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0xff,
    0x22,0x22,0x66,0x66,0xaa,0xaa,0xee,0xee,0x33,0x33,0x33,0x33,0xbb,0xbb,0xff,0xff,
    0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x55,0x55,0x55,0x55,0x99,0x99,0xdd,0xff,
    0x66,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0x77,0x77,0x77,0x77,0xff,0xff,0xff,0xff,
    0x00,0x00,0x00,0x00,0x88,0x88,0x88,0x88,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0x99,
    0x00,0x22,0x66,0x66,0xaa,0xaa,0xaa,0xaa,0x33,0x33,0x33,0x33,0xbb,0xbb,0xff,0xff,
    0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x11,0x11,0x55,0x55,0x99,0x99,0xdd,0xdd,
    0x00,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0xff,0x33,0xff,0x77,0xff,0xff,0xff,0xff,
};

const uint32_t gm_apple2_palette[16] =
{
    0x000000, /* Black */
    0xa70b40, /* Dark Red */
    0x401cf7, /* Dark Blue */
    0xe628ff, /* Purple */
    0x007440, /* Dark Green */
    0x808080, /* Dark Gray */
    0x1990ff, /* Medium Blue */
    0xbf9cff, /* Light Blue */
    0x406300, /* Brown */
    0xe66f00, /* Orange */
    0x808080, /* Light Grey */
    0xff8bbf, /* Pink */
    0x19d700, /* Light Green */
    0xbfe308, /* Yellow */
    0x58f4bf, /* Aquamarine */
    0xffffff  /* White */
};

#define CONTEXTBITS 3

uint16_t gm_double7bits(uint8_t value) {
    static uint16_t table[128];
    static int init = 0;
    if (!init) {
        for (unsigned k = 1; k < 128; k++)
            table[k] = table[k >> 1] * 4 + (k & 1) * 3;
        init = 1;
    }
    return (value > 127) ? 0 : table[value];
}

void gm_compute_row_words(const uint8_t *vram_row, uint16_t *words) {
    unsigned last_output_bit = 0;
    for (int col = 0; col < 40; col++) {
        unsigned word = gm_double7bits(vram_row[col] & 0x7f);
        if (vram_row[col] & 0x80)
            word = (word * 2 + last_output_bit) & 0x3fff;
        words[col] = (uint16_t)word;
        last_output_bit = word >> 13;
    }
}

// The pixel-run colour kernel. `phase` is MAME's `is_80_column` rotation offset:
// 0 for standard hi-res (40-column chroma phase), 1 for DHGR (80-column phase).
static void render_line_colors_phase(const uint16_t *in, uint8_t *colors560, int phase) {
    // w holds 3 bits of the previous 14-pixel group and the current and next groups.
    uint32_t w = 0;
    w += in[0] << CONTEXTBITS;
    for (int col = 0; col < 40; col++) {
        if (col < 39)
            w += in[col + 1] << (14 + CONTEXTBITS);
        for (int b = 0; b < 14; b++) {
            // colour is an index 0 (black)..15 (white) into gm_apple2_palette.
            colors560[col * 14 + b] = (uint8_t)rotl4b(artifact_color_lut[w & 0x7f], col * 14 + b + phase);
            w >>= 1;
        }
    }
}

void gm_render_line_colors(const uint16_t *in, uint8_t *colors560) {
    render_line_colors_phase(in, colors560, 0);
}

void gm_compute_dhgr_row_words(const uint8_t *aux_row, const uint8_t *main_row,
                               uint16_t *words) {
    for (int col = 0; col < 40; col++)
        words[col] = (uint16_t)((aux_row[col] & 0x7f) | ((main_row[col] & 0x7f) << 7));
}

void gm_render_dhgr_line_colors(const uint16_t *in, uint8_t *colors560) {
    render_line_colors_phase(in, colors560, 1);
}
