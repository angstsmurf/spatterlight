//
//  apple2draw.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-08-18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sagagraphics.h"
#include "scott.h"

#include "apple2draw.h"

#define SCREEN_MEM_SIZE 0x2000
#define MAX_SCREEN_ADDR 0x1fff

extern int x, y, xlen, ylen, xoff, yoff, size;

uint8_t *descrambletable = NULL;

static uint8_t *screenmem = NULL;
static uint8_t lobyte = 0, hibyte = 0;

void ClearApple2ScreenMem(void)
{
    if (screenmem) {
        memset(screenmem, 0, SCREEN_MEM_SIZE);
    }
}

static void AdvanceScreenByte(void)
{
    lobyte = ((y & 0xc0) >> 2 | (y & 0xc0)) >> 1 | (y & 8) << 4;
    hibyte = ((y & 7) << 1 | (uint8_t)(y << 2) >> 7) << 1 | (uint8_t)(y << 3) >> 7;
}

static int PutByte(uint8_t work, uint8_t work2)
{
    AdvanceScreenByte();
    if (hibyte * 0x100 + lobyte + x > MAX_SCREEN_ADDR)
        return 0;
    screenmem[hibyte * 0x100 + lobyte + x] = work;
    y++;
    AdvanceScreenByte();
    if (hibyte * 0x100 + lobyte + x > MAX_SCREEN_ADDR)
        return 0;
    screenmem[hibyte * 0x100 + lobyte + x] = work2;
    y++;
    if (y - 2 >= ylen) {
        if (x == xlen) {
            return 1;
        }
        x++;
        y = yoff;
    }
    return 1;
}

static uint16_t CalcScreenAddress(uint8_t ypos)
{
    if (0xc0 + ypos >= 0x182)
        return 0;
    uint16_t result = descrambletable[ypos] + descrambletable[0xc0 + ypos] * 0x100 - SCREEN_MEM_SIZE;
    if (result > MAX_SCREEN_ADDR)
        return 0;
    return result;
}

int DrawScrambledApple2Image(uint8_t *ptr, size_t datasize)
{
    if (screenmem == NULL) {
        screenmem = MyCalloc(SCREEN_MEM_SIZE);
    }

    uint8_t *origptr = ptr;
    uint8_t *end_of_data = ptr + datasize;

    uint8_t xoff = *ptr++;
    uint8_t yoff = *ptr++;
    uint8_t width = *ptr++;
    uint8_t height = *ptr++;

    uint8_t xpos = xoff;
    uint8_t ypos = yoff;

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
            screenmem[CalcScreenAddress(ypos) + xpos] = work1;
            ypos++;
            screenmem[CalcScreenAddress(ypos) + xpos] = work2;
            ypos++;
            if (height <= ypos) {
                ypos = yoff;
                xpos++;
                if (width <= xpos) {
                    return ptr - origptr;
                }
            }
        }
    }
    return ptr - origptr;
}

int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize)
{
    if (ptr == NULL)
        return 0;

    if (screenmem == NULL) {
        screenmem = MyCalloc(0x2000);
    }

    if (descrambletable) {
        int result = DrawScrambledApple2Image(ptr, datasize);
        debug_print("Stopped drawing at offset %x\n", result);
        return (result > 0);
    }

    int work, work2;
    int c;
    int i;

    uint8_t *origptr = ptr;

    int countflag = (CurrentGame == COUNT_US);

    x = 0;
    y = 0;

    if (!countflag) {
        // Get the offsets
        xoff = *ptr++;
        yoff = *ptr++;
        x = xoff;
        y = yoff;

        // Get the x length
        xlen = *ptr++;
        ylen = *ptr++;
    } else {
        xoff = 0;
        yoff = 0;
        xlen = 0;
        ylen = 0;
        while (xlen == 0 && ylen == 0) {
            xlen = *ptr++;
            ylen = *ptr++;
        }
    }

    while (xlen == 0 && ylen == 0) {
        xlen = *ptr++;
        ylen = *ptr++;
    }

    debug_print("xlen: %d ylen: %d\n", xlen, ylen);

    while (ptr - origptr < datasize - 2) {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80 || countflag) { // is a counter
            if (countflag) {
                c -= 1;
            } else {
                c &= 0x7f;
            }
            work = *ptr++;
            work2 = *ptr++;
            for (i = 0; i < c + 1 && ptr - origptr < datasize; i++) {
                if (!PutByte(work, work2)) {
                    debug_print("Reached end of screen memory at offset %lx\n", ptr - origptr);
                    return 1;
                }
                if (x > xlen || y > ylen) {
                    debug_print("Reached end of image dimensions at offset %lx\n", ptr - origptr);
                    return 1;
                }
            }
        } else {
            // Don't count on the next j characters

            for (i = 0; i < c + 1 && ptr - origptr < datasize - 1; i++) {
                work = *ptr++;
                work2 = *ptr++;
                if (!PutByte(work, work2)) {
                    debug_print("Reached end of screen memory at offset %lx\n", ptr - origptr);
                    return 1;
                }
                if (x > xlen || y > ylen) {
                    debug_print("Reached end of image dimensions at offset %lx\n", ptr - origptr);
                    return 1;
                }
            }
        }
    }
    return 1;
}

int DrawApple2Image(USImage *image)
{
    if (image->usage == IMG_ROOM)
        ClearApple2ScreenMem();
    DrawApple2ImageFromData(image->imagedata, image->datasize);
    debug_print("Drawing image with index %d, usage %d\n", image->index, image->usage);
    return 1;
}

static void PutApplePixel(glsi32 xpos, glsi32 ypos, glui32 color)
{
    xpos = xpos * pixel_size + x_offset;
    ypos = ypos * pixel_size + y_offset;
    glk_window_fill_rect(Graphics, color, xpos, ypos, pixel_size, pixel_size);
}

/* The code below is borrowed from the MAME Apple 2 driver */

#define BLACK 0
#define PURPLE 0xD53EF9
#define BLUE 0x458ff7
#define ORANGE 0xd7762c
#define GREEN 0x64d440
#define WHITE 0xffffff

static const int32_t hires_artifact_color_table[] = {
    BLACK, PURPLE, GREEN, WHITE,
    BLACK, BLUE, ORANGE, WHITE
};

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

void DrawApple2ImageFromVideoMem(void)
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
                PutApplePixel(pixpos++, row, v);
            }
        }
    }
}
