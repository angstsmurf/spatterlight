//
//  apple2draw.c
//
//  Apple 2 bitmap graphics drawing for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Created by Petter Sjölund on 2022-08-18.
//

#include <string.h>

#include "glk.h"
#include "debugprint.h"
#include "read_le16.h"
#include "minmax.h"
#include "memory_allocation.h"
#include "writetiff.h"

#include "apple2draw.h"

extern winid_t Graphics;

extern int pixel_size;
extern int x_offset;
extern int y_offset;

uint8_t *descrambletable = NULL;
uint8_t *screenmem = NULL;

void ClearApple2ScreenMem(void)
{
    if (screenmem) {
        memset(screenmem, 0, SCREEN_MEM_SIZE);
    }
}

static int WriteByteAndAdvanceY(uint8_t x, uint8_t *y, uint8_t byte_to_write)
{
    uint16_t address = (((*y / 8) & 0x07) << 7) + (((*y / 8) & 0x18) * 5) + ((*y & 7) << 10) + x;
    if (address > MAX_SCREEN_ADDR) {
        debug_print("apple2draw WriteByteAt() ERROR: Attempt to write outside Apple 2 screen memory at offset 0x%04x. Max: 0x%04x\n", address, MAX_SCREEN_ADDR);
        return 0;
    }
    screenmem[address] = byte_to_write;
    (*y)++;
    return 1;
}

static uint16_t DescrambleScreenAddress(uint8_t ypos)
{
    if (0xc0 + ypos >= 0x182)
        return 0;
    uint16_t result = descrambletable[ypos] + descrambletable[0xc0 + ypos] * 0x100 - SCREEN_MEM_SIZE;
    if (result > MAX_SCREEN_ADDR)
        return 0;
    return result;
}

static size_t DrawScrambledApple2Image(uint8_t *ptr, size_t datasize)
{
    if (screenmem == NULL) {
        screenmem = MemCalloc(SCREEN_MEM_SIZE);
    }

    uint8_t *origptr = ptr;
    uint8_t *end_of_data = ptr + datasize;

    uint8_t x_origin = *ptr++;
    uint8_t y_origin = *ptr++;
    uint8_t width = *ptr++;
    uint8_t height = *ptr++;

    uint8_t xpos = x_origin;
    uint8_t ypos = y_origin;

    if (ypos > 0xc0) {
        return 0;
    }

    while (ptr < end_of_data - 1) {
        uint8_t repeats = 1;
        uint8_t work1 = *ptr++;
        if (work1 == 0) {
            if (ptr > end_of_data - 3) {
                return ptr - origptr;
            }
            repeats = *ptr++;
            work1 = *ptr++;
        }

        uint8_t work2 = *ptr++;

        if (repeats == 0) {
            repeats = 1;
        }

        while (repeats--) {
            screenmem[DescrambleScreenAddress(ypos) + xpos] = work1;
            ypos++;
            screenmem[DescrambleScreenAddress(ypos) + xpos] = work2;
            ypos++;
            if (height <= ypos) {
                ypos = y_origin;
                xpos++;
                if (width <= xpos) {
                    return ptr - origptr;
                }
            }
        }
    }
    return ptr - origptr;
}

int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize, int is_the_count, adjust_plus_fn adjust_plus)
{
    uint8_t pattern_top = 0, pattern_bottom = 0;
    uint8_t repetitions;
    uint8_t i;

    uint8_t xpos = 0;
    uint8_t ypos = 0;

    uint8_t left = 0, top, right, bottom;

    if (ptr == NULL)
        return 0;

    uint8_t *origptr = ptr;

    if (screenmem == NULL) {
        screenmem = MemCalloc(SCREEN_MEM_SIZE);
    }

    if (descrambletable) {
        size_t result = DrawScrambledApple2Image(ptr, datasize);
        debug_print("Stopped drawing at offset %zx\n", result);
        return (result > 0);
    }

    if (!is_the_count) {
        // Get the origin
        left = *ptr++;
        top = *ptr++;
        xpos = left;
        ypos = top;

        // Get the dimensions
        right = *ptr++;
        bottom = *ptr++;
    } else {
        top = 0;
        right = 0;
        bottom = 0;
        while (right == 0 && bottom == 0) {
            right = *ptr++;
            bottom = *ptr++;
        }
    }

    while (right == 0 && bottom == 0) {
        right = *ptr++;
        bottom = *ptr++;
    }

    debug_print("width: %d height: %d\n", right, bottom);

    // Graphics window adjustments if we are being called
    // from Plus. (In the ScottFree games using this image
    // format, the images are all the same size.)
    if (adjust_plus != NULL) {
        adjust_plus(right, bottom, top);
    }

    while (ptr - origptr < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;

        // If bit 7 is set or this is an early game
        // we write the next two bytes (repetitions + 1) times
        int repeat_next_two_bytes = 0;
        if (is_the_count || (repetitions & 0x80) != 0) {
            repeat_next_two_bytes = 1;
            if (is_the_count) {
                repetitions -= 1;
            } else {
                repetitions &= 0x7f;
            }
            pattern_top = *ptr++;
            pattern_bottom = *ptr++;
        }
        for (i = 0; i < repetitions + 1; i++) {
            // If we are not repeating bytes, we simply
            // draw the next (repetitions + 1) * 2 bytes,
            // i.e. we read two new bytes on every iteration
            if (!repeat_next_two_bytes) {
                if (ptr - origptr >= datasize - 1) {
                    debug_print("Reached end of image data at offset %lx\n", ptr - origptr);
                    return 1;
                }
                pattern_top = *ptr++;
                pattern_bottom = *ptr++;
            }

            if (!WriteByteAndAdvanceY(xpos, &ypos, pattern_top) ||
                !WriteByteAndAdvanceY(xpos, &ypos, pattern_bottom)) {
                return 1;
            }
            if (ypos - 2 >= bottom && xpos < right) {
                xpos++;
                ypos = top;
            }
            if (xpos > right || ypos > bottom) {
                debug_print("Reached end of image dimensions at offset %lx\n", ptr - origptr);
                return 1;
            }
        }
    }
    return 1;
}

static void PutApplePixel(glsi32 xpos, glsi32 ypos, glui32 color, int width)
{
    xpos = xpos * pixel_size + x_offset;
    ypos = ypos * pixel_size + y_offset;
    glk_window_fill_rect(Graphics, color, xpos, ypos, pixel_size * width, pixel_size * 2);
}

static void PutApplePixelFlippable(glsi32 xpos, glsi32 ypos, glui32 color, int width, int upside_down)
{
    if (upside_down) {
        xpos = (560 - xpos - width) * pixel_size + x_offset;
        ypos = (319 - ypos) * pixel_size + y_offset - 1;
        glk_window_fill_rect(Graphics, color, xpos, ypos, pixel_size * width, pixel_size * 2);
    } else {
        PutApplePixel(xpos, ypos, color, width);
    }
}

#define STRIDE 560 * 4
#define BITMAPHEIGHT 640

static void WriteApplePixelToPixmap(glsi32 xpos, glsi32 ypos, glui32 color, uint8_t *bitmap)
{
    if (bitmap == NULL) {
        bitmap = MemCalloc(STRIDE * BITMAPHEIGHT);
    }
    size_t offset = ypos * STRIDE + xpos * 4;
    uint8_t red = (color >> 16) & 0xff;
    uint8_t green = (color >> 8) & 0xff;
    uint8_t blue = color & 0xff;
    bitmap[offset] = red;
    bitmap[offset + 1] = green;
    bitmap[offset + 2] = blue;
    bitmap[offset + 3] = 0xff;
}

/* The code below is borrowed from the MAME Apple 2 driver. */

extern int ImageHeight;

// 4-bit left rotate. Bits 4-6 of n must be a copy of bits 0-2.
static unsigned rotl4b(unsigned n, unsigned count) { return (n >> (-count & 3)) & 0x0f; }

static uint8_t const artifact_color_lut[128] =
{
    0x00,0x00,0x00,0x00,0x88,0x00,0xcc,0x00,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0xff,
    0x22,0x22,0x66,0x66,0xaa,0xaa,0xee,0xee,0x33,0x33,0x33,0x33,0xbb,0xbB,0xff,0xff,
    0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x55,0x55,0x55,0x55,0x99,0x99,0xdd,0xff,
    0x66,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0x77,0x77,0x77,0x77,0xff,0xfF,0xff,0xff,
    0x00,0x00,0x00,0x00,0x88,0x88,0x88,0x88,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0x99,
    0x00,0x22,0x66,0x66,0xaa,0xaa,0xaa,0xaa,0x33,0x33,0x33,0x33,0xbb,0xbB,0xff,0xff,
    0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x11,0x11,0x55,0x55,0x99,0x99,0xdd,0xdd,
    0x00,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0xff,0x33,0xff,0x77,0xff,0xfF,0xff,0xff,
};

static const glui32 apple2_palette[16] =
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

static void  RenderLineWithA2ArtifactColors(uint16_t const *in, int startcol, int stopcol, int row, int upside_down)
{
    if (startcol >= stopcol)
        return;  // to avoid OOB read

    // w holds 3 bits of the previous 14-pixel group and the current and next groups.
    uint32_t w = (CONTEXTBITS && startcol > 0) ? (in[startcol - 1] >> (14 - CONTEXTBITS)) : 0;
    w += in[startcol] << CONTEXTBITS;

    uint8_t lastcolor = 0xff;
    int runlength = 0;

    for (int col = startcol; col < stopcol; col++)
    {
        if (col < 39)
            w += in[col + 1] << (14 + CONTEXTBITS);

        for (int b = 0; b < 14; b++)
        {
            // color is an index value between 0 (black) and 15 (white).
            // See apple2_palette[16] above.
            uint8_t color = (uint8_t)rotl4b(artifact_color_lut[w & 0x7f], col * 14 + b);
            // We optimize runs of the same color by only drawing
            // when the color changes or we are at the last pixel of the line.
            int at_last_pixel = (b == 13 && col == stopcol - 1);
            if (at_last_pixel || (color != lastcolor && runlength > 0)) {
                glui32 glkcolor = apple2_palette[lastcolor];
                PutApplePixelFlippable(col * 14 + b - runlength, row * 2, glkcolor, runlength + 1, upside_down);
                // The code above only draws the *previous* pixel(s),
                // (except we always have to add 1 pixel of overlap to
                // runlength to avoid gaps, not sure why) so if the last pixel
                // of a line has a different color to the one on its
                // we have to handle it separately.
                if (at_last_pixel && color != lastcolor) {
                    glui32 glkcolor = apple2_palette[color];
                    PutApplePixelFlippable(col * 14 + b, row * 2, glkcolor, 1, upside_down);
                }
                runlength = 0;
            } else {
                runlength++;
            }
            lastcolor = color;
            w >>= 1;
        }
    }
}

static void  RenderLineWithA2ArtifactColorsToPixmap(uint16_t const *in, int row, uint8_t *bitmap)
{
    int startcol = 0;
    int stopcol = 39;

    // w holds 3 bits of the previous 14-pixel group and the current and next groups.
    uint32_t w = (CONTEXTBITS && startcol > 0) ? (in[startcol - 1] >> (14 - CONTEXTBITS)) : 0;
    w += in[startcol] << CONTEXTBITS;

    for (int col = startcol; col < stopcol; col++)
    {
        if (col < 39)
            w += in[col + 1] << (14 + CONTEXTBITS);

        for (int b = 0; b < 14; b++)
        {
            // color is an index value between 0 (black) and 15 (white).
            // See apple2_palette[16] above.
            uint8_t color = (uint8_t)rotl4b(artifact_color_lut[w & 0x7f], col * 14 + b);
            glui32 glkcolor = apple2_palette[color];
            WriteApplePixelToPixmap(col * 14 + b, row * 2, glkcolor, bitmap);
            WriteApplePixelToPixmap(col * 14 + b, row * 2 + 1, glkcolor, bitmap);
            w >>= 1;

        }
    }
}

static uint16_t *d7b_lookup_table = NULL;

static uint16_t Double7Bits(int i) {
    if (d7b_lookup_table == NULL) {
        d7b_lookup_table = MemCalloc(128 * 2);
        for (unsigned i = 1; i < 128; i++)
        {
            d7b_lookup_table[i] = d7b_lookup_table[i >> 1] * 4 + (i & 1) * 3;
        }
    }
    if (i > 127)
        return 0;
    return d7b_lookup_table[i];
}

void DrawApple2ImageFromVideoMemWithFlip(int upside_down)
{
    if (screenmem == NULL)
        return;

    int endrow = MIN(ImageHeight / 2, 160);

    for (int row = 0; row <= endrow; row++) {
        /* calculate address */
        unsigned const address = (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
        uint8_t const *const vram_row = screenmem + address;
        uint16_t words[40];

        unsigned last_output_bit = 0;

        for (int col = 0; col < 40; col++) {
            unsigned word = Double7Bits(vram_row[col] & 0x7f);
            if (vram_row[col] & 0x80)
                word = (word * 2 + last_output_bit) & 0x3fff;
            words[col] = word;
            last_output_bit = word >> 13;
        }
        RenderLineWithA2ArtifactColors(words, 0, 40, row, upside_down);
    }
}

void DrawApple2ImageFromVideoMemToPixmap(uint8_t *bitmap)
{
    if (screenmem == NULL)
        return;
    if (bitmap == NULL)
        bitmap = MemCalloc(STRIDE * BITMAPHEIGHT);

    int endrow = MIN(ImageHeight / 2, 160);

    for (int row = 0; row <= endrow; row++) {
        /* calculate address */
        unsigned const address = (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
        uint8_t const *const vram_row = screenmem + address;
        uint16_t words[40];

        unsigned last_output_bit = 0;

        for (int col = 0; col < 40; col++) {
            unsigned word = Double7Bits(vram_row[col] & 0x7f);
            if (vram_row[col] & 0x80)
                word = (word * 2 + last_output_bit) & 0x3fff;
            words[col] = word;
            last_output_bit = word >> 13;
        }
        RenderLineWithA2ArtifactColorsToPixmap(words, row, bitmap);
    }
}

void DrawApple2ImageToTIFF(const char *name) {
    uint8_t *bitmap = MemCalloc(STRIDE * BITMAPHEIGHT);
    DrawApple2ImageFromVideoMemToPixmap(bitmap);

    const char *path = "/Users/administrator/Desktop/SAGA_vector/";
    size_t namelength = strlen(name) + strlen(path) + 6;
    char *filename = MemAlloc(namelength);
    snprintf(filename, namelength, "%s%s.tiff", path, name);

    FILE *fptr = fopen(filename, "w");

    if (fptr == NULL) {
        fprintf(stderr, "File open error!\n");
        return;
    }

    //    fprintf(stderr, "writeToTIFF: \"%s\"\n", name);

    writetiff(fptr, bitmap, STRIDE * BITMAPHEIGHT, 560);
    free(bitmap);

    fclose(fptr);

}


void DrawApple2ImageFromVideoMem(void)
{
    DrawApple2ImageFromVideoMemWithFlip(0);
}
