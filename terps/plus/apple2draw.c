//
//  apple2draw.c
//  plus
//
//  Created by Administrator on 2022-08-18.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "glk.h"
#include "graphics.h"
#include "common.h"

extern int x, y, xlen, ylen, xoff, yoff, size;

void PrintFirstTenBytes(uint8_t *ptr, size_t offset);

static uint8_t screenmem[0x8000];

static uint8_t lobyte = 00, hibyte = 0x20;

void ClearApple2ScreenMem(void) {
    memset(&screenmem[0x2000], 0, 0x1fff);
}

void AdvanceScreenByte(void)
{
    lobyte = ((y & 0xc0) >> 2 | (y & 0xc0)) >> 1 | (y & 8) << 4;
    hibyte = ((y & 7) << 1 | (uint8_t)(y << 2) >> 7) << 1 | (uint8_t)(y << 3) >> 7 | 0x20;
}

void PutByte(uint8_t work, uint8_t work2)

{
    AdvanceScreenByte();
    screenmem[hibyte * 0x100 + lobyte + x] = work;
    y++;
    AdvanceScreenByte();
    screenmem[hibyte * 0x100 + lobyte + x] = work2;
    y++;
    if (y - 2 >= ylen) {
        if (x == xlen) {
            return;
        }
        x++;
        y = yoff;
    }
    return;
}

void DrawApple2ImageFromVideoMem(void);

int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize)
{
    int work,work2;
    int c;
    int i;

    uint8_t *origptr = ptr;

    x = 0; y = 0;

    work = *ptr++;
    size = work + (*ptr++ * 256);
    fprintf(stderr, "size: %d (%x)\n",size,size);

    // Get the offsets
    xoff = *ptr++; //- 3;
    if (xoff < 0) {
        xoff = 0;
    }
    yoff = *ptr++;
    x = xoff;
    y = yoff;
    fprintf(stderr, "xoff: %d yoff: %d\n",xoff,yoff);

    // Get the x length
    xlen = *ptr++;
    ylen = *ptr++;
    fprintf(stderr, "xlen: %d ylen: %d\n",xlen,ylen);

    while (ptr - origptr < size)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            c &= 0x7f;
            work=*ptr++;
            work2=*ptr++;
            for (i = 0; i < c + 1 && ptr - origptr < size; i++)
            {
                PutByte(work, work2);
                if (x > xlen || y > ylen) {
//                    DrawApple2ImageFromVideoMem();
                    return 1;
                }
            }
        }
        else
        {
            // Don't count on the next j characters

            for (i = 0; i < c + 1 && ptr - origptr < size; i++)
            {
                work = *ptr++;
                work2=*ptr++;
                PutByte(work, work2);
                if (x > xlen || y > ylen) {
                    return 1;
                }
            }
        }
    }
    return 1;
}


int IsBitSet(int bit, uint8_t byte) {
    return ((byte & (1 << bit)) != 0);
}

void PutApplePixel(glsi32 x, glsi32 y, glui32 color)
{
    glsi32 xpos = x * pixel_size;

    if (upside_down)
        xpos = 280 * pixel_size - xpos;
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    int ypos = y * pixel_size;
    if (upside_down)
        ypos = 157 * pixel_size - ypos;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, color, xpos,
                         ypos, pixel_size, pixel_size);
}

#define BLACK   0
#define PURPLE  0xD53EF9
#define BLUE    0x458ff7
#define ORANGE  0xd7762c
#define GREEN   0x64d440
#define WHITE   0xffffff

static const int32_t hires_artifact_color_table[] =
{
    BLACK,  PURPLE, GREEN,  WHITE,
    BLACK,  BLUE,   ORANGE, WHITE
};

int32_t *m_hires_artifact_map = NULL;

void generate_artifact_map(void) {
// generate hi-res artifact data
    int i, j;
    uint16_t c;

    /* 2^3 dependent pixels * 2 color sets * 2 offsets */
    m_hires_artifact_map = MemAlloc(sizeof(int32_t) * 8 * 2 * 2);

    /* build hires artifact map */
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 2; j++)
        {
            if (i & 0x02)
            {
                if ((i & 0x05) != 0)
                    c = 3;
                else
                    c = j ? 2 : 1;
            }
            else
            {
                if ((i & 0x05) == 0x05)
                    c = j ? 1 : 2;
                else
                    c = 0;
            }
            m_hires_artifact_map[ 0 + j*8 + i] = hires_artifact_color_table[(c + 0) % 8];
            m_hires_artifact_map[16 + j*8 + i] = hires_artifact_color_table[(c + 4) % 8];
        }
    }
}

void DrawApple2ImageFromVideoMem(void)
{

    if (m_hires_artifact_map == NULL)
        generate_artifact_map();

    int32_t *artifact_map_ptr;

    uint8_t const *const vram = &screenmem[0x2000];
    uint8_t vram_row[42];
    vram_row[0] = 0;
    vram_row[41] = 0;

    for (int row = 0; row < 192; row++)
    {
        for (int col = 0; col < 40; col++)
        {
            int const offset = ((((row/8) & 0x07) << 7) | (((row/8) & 0x18) * 5 + col)) | ((row & 7) << 10);
            vram_row[1+col] = vram[offset];
        }

        int pixpos = 0;

        for (int col = 0; col < 40; col++)
        {
            uint32_t w =    (((uint32_t) vram_row[col+0] & 0x7f) <<  0)
            |   (((uint32_t) vram_row[col+1] & 0x7f) <<  7)
            |   (((uint32_t) vram_row[col+2] & 0x7f) << 14);

            artifact_map_ptr = &m_hires_artifact_map[((vram_row[col + 1] & 0x80) >> 7) * 16];

            for (int b = 0; b < 7; b++)
            {

                int32_t const v = artifact_map_ptr[((w >> (b + 7-1)) & 0x07) | (((b ^ col) & 0x01) << 3)];
                PutApplePixel(pixpos++, row, v);
            }
        }
    }
}

