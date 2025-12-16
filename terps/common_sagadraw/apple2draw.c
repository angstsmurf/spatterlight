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
#include "memory_allocation.h"

#include "apple2draw.h"

#define SCREEN_MEM_SIZE 0x2000
#define MAX_SCREEN_ADDR 0x1fff

extern winid_t Graphics;

extern int pixel_size;
extern int x_offset;
extern int y_offset;

uint8_t *descrambletable = NULL;

static uint8_t *screenmem = NULL;

void ClearApple2ScreenMem(void)
{
    if (screenmem) {
        memset(screenmem, 0, SCREEN_MEM_SIZE);
    }
}

static int WriteByteAndAdvanceY(uint8_t x, uint8_t *y, uint8_t byte_to_write)
{
    uint8_t lobyte = ((*y & 0xc0) >> 2 | (*y & 0xc0)) >> 1 | (*y & 8) << 4;
    uint8_t hibyte = ((*y & 7) << 1 | (uint8_t)(*y << 2) >> 7) << 1 | (uint8_t)(*y << 3) >> 7;

    uint16_t address = hibyte << 8 | lobyte + x;
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

size_t DrawScrambledApple2Image(uint8_t *ptr, size_t datasize)
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
    uint8_t work = 0, work2 = 0;
    uint8_t repetitions;
    uint8_t i;

    uint8_t xpos = 0;
    uint8_t ypos = 0;

    uint8_t x_origin = 0, y_origin, width, height;

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
        x_origin = *ptr++;
        y_origin = *ptr++;
        xpos = x_origin;
        ypos = y_origin;

        // Get the dimensions
        width = *ptr++;
        height = *ptr++;
    } else {
        y_origin = 0;
        width = 0;
        height = 0;
        while (width == 0 && height == 0) {
            width = *ptr++;
            height = *ptr++;
        }
    }

    while (width == 0 && height == 0) {
        width = *ptr++;
        height = *ptr++;
    }

    debug_print("width: %d height: %d\n", width, height);

    // Graphics window adjustments if we are being called
    // from Plus. (In the ScottFree games using this image
    // format, the images are all the same size.)
    if (adjust_plus != NULL) {
        adjust_plus(width, height, y_origin);
    }

    while (ptr - origptr < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;

        // If bit 7 is set or this is an early game
        // we write the next two bytes (repetitions + 1) times
        int repeat_next_two_bytes = 0;
        if ((repetitions & 0x80) == 0x80 || is_the_count) {
            repeat_next_two_bytes = 1;
            if (is_the_count) {
                repetitions -= 1;
            } else {
                repetitions &= 0x7f;
            }
            work = *ptr++;
            work2 = *ptr++;
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
                work = *ptr++;
                work2 = *ptr++;
            }

            if (!WriteByteAndAdvanceY(xpos, &ypos, work) ||
                !WriteByteAndAdvanceY(xpos, &ypos, work2)) {
                return 1;
            }
            if (ypos - 2 >= height && xpos < width) {
                xpos++;
                ypos = y_origin;
            }
            if (xpos > width || ypos > height) {
                debug_print("Reached end of image dimensions at offset %lx\n", ptr - origptr);
                return 1;
            }
        }
    }
    return 1;
}

static void PutApplePixel(glsi32 xpos, glsi32 ypos, glui32 color)
{
    xpos = xpos * pixel_size + x_offset;
    ypos = ypos * pixel_size + y_offset;
    glk_window_fill_rect(Graphics, color, xpos, ypos, pixel_size, pixel_size);
}

static void PutApplePixelUpsideDown(glsi32 xpos, glsi32 ypos, glui32 color)
{
    xpos = (280 - xpos) * pixel_size + x_offset;
    ypos = (157 - ypos) * pixel_size + y_offset;
    glk_window_fill_rect(Graphics, color, xpos, ypos, pixel_size, pixel_size);
}

/* The code below is borrowed from the MAME Apple 2 driver. */

/* Since I wrote this, MAME has upgraded to a more accurate
   way of rendering Apple 2 graphics, but it would require
   substantial changes to use that instead. Perhaps one day. */

// clang-format off
#define BLACK   0
#define PURPLE  0xd53ef9
#define BLUE    0x458ff7
#define ORANGE  0xd7762c
#define GREEN   0x64d440
#define WHITE   0xffffff

static const int32_t hires_artifact_color_table[] =
{
    BLACK,  PURPLE, GREEN,  WHITE,
    BLACK,  BLUE,   ORANGE, WHITE
};
// clang-format on

static int32_t *m_hires_artifact_map = NULL;

static void generate_artifact_map(void)
{
    /* generate hi-res artifact data */
    int i, j;
    uint16_t c;

    /* 2^3 dependent pixels * 2 color sets * 2 offsets */
    m_hires_artifact_map = MemAlloc(sizeof(int32_t) * 8 * 2 * 2);

    /* build hires artifact map */
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 2; j++) {
            if (i & 0x02) {
                if ((i & 0x05) != 0)
                    c = 3;
                else
                    c = j ? 2 : 1;
            } else {
                if ((i & 0x05) == 0x05)
                    c = j ? 1 : 2;
                else
                    c = 0;
            }
            m_hires_artifact_map[0 + j * 8 + i] = hires_artifact_color_table[(c + 0) % 8];
            m_hires_artifact_map[16 + j * 8 + i] = hires_artifact_color_table[(c + 4) % 8];
        }
    }
}

extern int ImageHeight;

void DrawApple2ImageFromVideoMemWithFlip(int upside_down)
{
    if (m_hires_artifact_map == NULL)
        generate_artifact_map();

    int32_t *artifact_map_ptr;

    uint8_t const *const vram = screenmem;
    uint8_t vram_row[42];
    vram_row[0] = 0;
    vram_row[41] = 0;

    for (int row = 0; row < ImageHeight; row++) {
        for (int col = 0; col < 40; col++) {
            int const offset = ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5 + col)) | ((row & 7) << 10);
            vram_row[1 + col] = vram[offset];
        }

        int pixpos = 0;

        for (int col = 0; col < 40; col++) {
            uint32_t w = (((uint32_t)vram_row[col + 0] & 0x7f) << 0)
                | (((uint32_t)vram_row[col + 1] & 0x7f) << 7)
                | (((uint32_t)vram_row[col + 2] & 0x7f) << 14);

            artifact_map_ptr = &m_hires_artifact_map[((vram_row[col + 1] & 0x80) >> 7) * 16];

            for (int b = 0; b < 7; b++) {
                int32_t const v = artifact_map_ptr[((w >> (b + 7 - 1)) & 0x07) | (((b ^ col) & 0x01) << 3)];
                if (upside_down)
                    PutApplePixelUpsideDown(pixpos++, row, v);
                else
                    PutApplePixel(pixpos++, row, v);
            }
        }
    }
}

void DrawApple2ImageFromVideoMem(void)
{
    DrawApple2ImageFromVideoMemWithFlip(0);
}
