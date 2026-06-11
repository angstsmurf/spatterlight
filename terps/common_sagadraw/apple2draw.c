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

#include "apple2draw.h"
#include "gm_artifact.h"

extern winid_t Graphics;

extern int pixel_size;
extern int x_offset;
extern int y_offset;

uint8_t *descrambletable = NULL;
uint8_t *screenmem = NULL;

#define APPLE_SCREEN_WIDTH 160

void ClearApple2ScreenMem(void)
{
    if (screenmem) {
        memset(screenmem, 0, A2_SCREEN_MEM_SIZE);
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
    // The descrambletable values are copied from the disk image, so we must subtract
    // the offset of the Apple 2 screen memory (0x2000), in order to convert it to an index
    // for our screenmem array (starting at 0 rather than 0x2000).
    uint16_t result = ((descrambletable[0xc0 + ypos] << 8) | descrambletable[ypos]) - 0x2000;
    if (result > MAX_SCREEN_ADDR)
        return 0;
    return result;
}

static size_t DrawScrambledApple2Image(uint8_t *ptr, size_t datasize)
{
    if (screenmem == NULL) {
        screenmem = MemCalloc(A2_SCREEN_MEM_SIZE);
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
        screenmem = MemCalloc(A2_SCREEN_MEM_SIZE);
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

/* The NTSC artifact-colour kernel (artifact LUT, palette, Double7Bits, per-row
   word + colour expansion) lives in common_sagadraw/gm_artifact.c, shared with
   the Comprehend renderer. This file keeps only the Glk plotting on top of it. */

extern int ImageHeight;

static void  RenderLineWithA2ArtifactColors(uint16_t const *in, int row, int upside_down)
{
    uint8_t colors560[560];
    gm_render_line_colors(in, colors560);

    uint8_t lastcolor = 0xff;
    int runlength = 0;

    for (int col = 0; col < 40; col++)
    {
        for (int b = 0; b < 14; b++)
        {
            // color is an index value between 0 (black) and 15 (white).
            uint8_t color = colors560[col * 14 + b];
            // We optimize runs of the same color by only drawing
            // when the color changes or we are at the last pixel of the line.
            int at_last_pixel = (b == 13 && col == 39);
            if (at_last_pixel || (color != lastcolor && runlength > 0)) {
                glui32 glkcolor = gm_apple2_palette[lastcolor];
                PutApplePixelFlippable(col * 14 + b - runlength, row * 2, glkcolor, runlength + 1, upside_down);
                // The code above only draws the *previous* pixel(s),
                // (except we always have to add 1 pixel of overlap to
                // runlength to avoid gaps, not sure why) so if the last pixel
                // of a line has a different color to the one on its
                // we have to handle it separately.
                if (at_last_pixel && color != lastcolor) {
                    glui32 glkcolor = gm_apple2_palette[color];
                    PutApplePixelFlippable(col * 14 + b, row * 2, glkcolor, 1, upside_down);
                }
                runlength = 0;
            } else {
                runlength++;
            }
            lastcolor = color;
        }
    }
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
        gm_compute_row_words(vram_row, words);
        RenderLineWithA2ArtifactColors(words, row, upside_down);
    }
}

void DrawSingleApple2ImageByte(uint8_t *mem, size_t offset)
{
    if (mem == NULL)
        return;

    int row = ((offset & 0x7f) / 40) * 64 + ((offset >> 7) & 7) * 8 + ((offset >> 10) & 7);
    int col = (offset & 0x7f) % 40;

    unsigned const row_offset = (unsigned)offset - col;
    uint8_t const *const vram_row = mem + row_offset;

    uint16_t words[40];
    gm_compute_row_words(vram_row, words);
    RenderLineWithA2ArtifactColors(words, row, 0);
}

void DrawApple2ImageFromVideoMem(void)
{
    DrawApple2ImageFromVideoMemWithFlip(0);
}
