/* Routines to draw the Atari ST RLE graphics
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animations.h"
#include "common.h"
#include "definitions.h"
#include "glk.h"
#include "graphics.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

extern PALETTE pal;

extern winid_t Graphics;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
} Pixel;

typedef struct {
    uint8_t ColIdx;
    uint8_t CurCol;
    uint8_t NumCol;
    glui32 *Colours;
    int Rate;
    int NumPix;
    Pixel *Pixels;
} AnimationColor;

int IsSTBitSet(int bit, uint8_t byte) {
    return ((byte & (1 << bit)) != 0);
}

uint8_t stmem[0x1000000];

static uint32_t NumAnimCols, NumOldAnimCols = 0, ImgAddrOffs, ImgWidth, NxtLine, InitialMask, LastMask, CurAddr, LineNibblesLeft;

static void moveword(uint16_t source, uint8_t *dest) {
    dest[0] = (source >> 8) & 0xff;
    dest[1] = source & 0xff;
}

static glui32 StColToGlk(uint8_t *ptr) {
    int blue = *(ptr + 1) & 0xf;
    int green = (*(ptr + 1) >> 4) & 0xf;
    int red = *ptr & 0xf;

    red = red * 35.7;
    green = green * 35.7;
    blue = blue * 35.7;

    return (red << 16) | (green << 8) | blue;
}

AnimationColor *AnimColors = NULL;

static void FreeAnimCols(void) {
    if (AnimColors == NULL || NumOldAnimCols == 0)
        return;
    for (int i = 0; i < NumOldAnimCols; i++) {
        AnimationColor col = AnimColors[i];
        if (col.Colours != NULL)
            free(col.Colours);
        col.Colours = NULL;
        if (col.Pixels != NULL)
            free(col.Pixels);
        col.Pixels = NULL;
    }
    if (AnimColors != NULL)
        free(AnimColors);
    AnimColors = NULL;
    NumAnimCols = 0;
}

static uint8_t *SetPaletteAnimation(uint8_t *ptr) {
    uint32_t addr = 0x1acb4;
    for (int i = 0; i <= 182; i++) {
        stmem[addr++] = 0;
        stmem[addr++] = 0;
    }
    moveword(0x000e, &stmem[0x1acb4]);

    if (NumAnimCols != 0) {
//        if (AnimColors != NULL) {
//            FreeAnimCols();
//        }
        AnimColors = MemAlloc(sizeof(AnimationColor) * (NumAnimCols + NumOldAnimCols));
        AnimationColor *col = AnimColors;
        for (int i = 0; i < NumAnimCols; i++, col++) {
            uint8_t val = *ptr++;
            col->ColIdx = 1 + (val >> 4);
            col->CurCol = 0;
            col->NumPix = 0;
            col->Pixels = NULL;
//            fprintf(stderr, "\ninitial val %d: %d\n", i, col->ColIdx);
            uint16_t offset = (uint16_t)(val >> 4) * 0x1a;
//            fprintf(stderr, "offset %d: %d\n", i, offset);
            stmem[0x1acb6 + offset + 1] = *ptr++;
//            fprintf(stderr, "offset + 1 %d\n", stmem[0x1acb6 + offset + 1]);
            stmem[0x1acb6 + offset + 2] = *ptr++;
//            fprintf(stderr, "offset + 2 %d\n", stmem[0x1acb6 + offset + 2]);
            stmem[0x1acb6 + offset + 3] = *ptr++;
//            fprintf(stderr, "offset + 3 %d\n", stmem[0x1acb6 + offset + 3]);
            col->Rate = 1 + stmem[0x1acb6 + offset + 3] / 3;
            val = val & 0xf;
            col->NumCol = val;
            col->Colours = MemAlloc(col->NumCol * sizeof(glui32));
//            fprintf(stderr, "repeats %d: %d\n", i, val);
            stmem[0x1acb6 + offset + 5] = val;
//            fprintf(stderr, "offset + 5 %d\n", stmem[0x1acb6 + offset + 5]);
            uint32_t index = 0x1acb6 + offset + 6;
//            fprintf(stderr, "index %d: %x\n", i, index);
            for (int j = 0; j < val; j++) {
                stmem[index++] = *ptr++;
                stmem[index++] = *ptr++;
//                fprintf(stderr, "offset %x: %d ", index, stmem[index - 2] * 256 + stmem[index - 1]);
                col->Colours[j] = StColToGlk(&stmem[index - 2]);
//                PrintRGB(&stmem[index - 2]);
            };
        }
    }
    return ptr;
}

void PutDoublePixel(glsi32 x, glsi32 y, int32_t color);

uint8_t *PatternLookup = NULL;

static void GeneratePatternLookup(void)
{
    uint pattern;
    ushort previous;

    pattern = 0xffff0000;
    PatternLookup = MemAlloc(1024);
    uint8_t *ptr = PatternLookup;
    do {
        for (int i = 0; i < 4; i++) {
            ptr[i] = 0;
            if ((pattern & 1 << i) != 0) {
                ptr[i] = ptr[i] | 0x33;
            }
            if ((pattern & 1 << (i + 4)) != 0) {
                ptr[i] = ptr[i] | 0xcc;
            }
        }
        ptr = ptr + 4;
        previous = pattern + 1;
        pattern = (pattern & 0xffff0000) | previous;
    } while (previous != 256);
}


static void DrawSTNibble(uint8_t byte, uint8_t mask, int x, int y, Pixel **pixels) {
    uint16_t offs = byte << 2;
    int startbit = 7;
    int endbit = 4;
    if (mask == 0xf) {
        startbit = 3;
        endbit = 0;
    }

    for (int i = startbit; i >= endbit; i -= 2) {
        uint8_t col = 0;
        for (int j = 0; j < 4; j++) {
            if (IsSTBitSet(i, PatternLookup[offs + j]))
                col |= 1 << (3 - j);
        }

        if (col) {
            for (int c = 0; c < NumAnimCols; c++) {
                if (col == AnimColors[c].ColIdx && AnimColors[c].NumPix < 3000) {
                    if (AnimColors[c].NumPix && pixels[c][AnimColors[c].NumPix - 1].y == y && pixels[c][AnimColors[c].NumPix - 1].x == x + 5 - i) {
                        pixels[c][AnimColors[c].NumPix - 1].width += 2;
                        break;
                    }

                    pixels[c][AnimColors[c].NumPix].x = x + 7 - i;
                    pixels[c][AnimColors[c].NumPix].y = y;
                    pixels[c][AnimColors[c].NumPix].width = 2;

                    AnimColors[c].NumPix++;

                    break;
                }
            }
            PutDoublePixel(x + 7 - i, y, col);
        }
    }
}


static void DrawPattern(uint8_t pattern, Pixel **pixels) {
    // mask and LastMask alternate between 0x0f and 0xf0
    uint8_t mask = LastMask ^ 0xff;

    // CurAddr: screen memory where we're currently writing
    int y1 = CurAddr / 160;
    int x1 = 2 * (CurAddr % 160) + 6 * (CurAddr % 2);

    DrawSTNibble(pattern, mask, x1, y1, pixels);

    mask = mask ^ 0xff;
    LastMask = LastMask ^ 0xff;
    if (mask != 0xf) {
        CurAddr += 1 + 6 * (CurAddr % 2);
    }

    // LineBytesLeft = bytes remaining to draw this line

    LineNibblesLeft--;
    if (LineNibblesLeft != 0)
        return;

    // Reached end of line
    // NxtLine: start address of next line
    NxtLine += 160; // move down one line
    CurAddr = NxtLine;
    // InitialMask: mask at start of line
    mask = InitialMask;
    LastMask = mask ^ 0xff;
    LineNibblesLeft = ImgWidth;
}

void SetRGB(int32_t index, int red, int green, int blue);

int ColorCyclingRunning = 0;

int Intersects(Pixel pix, int x, int y, int width, int height) {
    return MAX(pix.x, x) <= MIN(pix.x + pix.width, x + width)
    && MAX(pix.y, y) <= MIN(pix.y, y + height);
}

void CopyPixel(Pixel *a, Pixel *b) {
    a->width = b->width;
    a->x = b->x;
    a->y = b->y;
}

void CopyAnimCol(AnimationColor *a, AnimationColor *b) {
    a->ColIdx = b->ColIdx;
    a->CurCol = b->CurCol;
    a->NumCol = b->NumCol;
    int size = b->NumCol * sizeof(glui32);
    a->Colours = MemAlloc(size);
    memcpy(a->Colours, b->Colours, size);
    a->Rate = b->Rate;
    a->NumPix = b->NumPix;
    size = b->NumPix * sizeof(Pixel);
    a->Pixels = MemAlloc(size);
    memcpy(a->Pixels, b->Pixels, size);
}

void AddPixels(AnimationColor *animcol, Pixel *pixels, int numpixels) {
    int pixidx = 0;
    Pixel newpix[3000];
    for (int i = 0; i < animcol->NumPix; i++) {
        CopyPixel(&newpix[pixidx++], &(animcol->Pixels[i]));
    }

    for (int i = 0; i < numpixels; i++) {
        CopyPixel(&newpix[pixidx++], &pixels[i]);
    }
    int size = sizeof(Pixel) * pixidx;
    free(animcol->Pixels);
    animcol->Pixels = MemAlloc(size);
    memcpy(animcol->Pixels, newpix, size);
    animcol->NumPix = pixidx;
}


int AddNonHiddenColAnim(AnimationColor *old, AnimationColor *new, int numnew, int x, int y, int width, int height) {
    int goodpixels = 0;
    Pixel pixels[3000];

    for (int i = 0; i < old->NumPix; i++) {
        if (!Intersects(old->Pixels[i], x, y, width, height)) {
            CopyPixel(&pixels[goodpixels++], &(old->Pixels[i]));
        }
    }

    if (goodpixels == 0) {
        return 0;
    }

    for (int i = 0; i < numnew; i++) {
        if (new[i].ColIdx == old->ColIdx && new[i].NumPix) {
            AddPixels(&new[i], pixels, goodpixels);
            return 0;
        }
    }

    CopyAnimCol(&new[numnew], old);
    return 1;
}

int DrawSTImageFromData(uint8_t *imgdata, size_t datasize) {

    uint8_t *ptr = &imgdata[4];

    ImgAddrOffs = *ptr++ * 256;
    ImgAddrOffs += *ptr++;
    ImgAddrOffs *= 2;

    yoff = ImgAddrOffs / 160;
    xoff = 2 * (ImgAddrOffs % 160) + 6 * (ImgAddrOffs % 2);

    ImgWidth = *ptr++;

    if (ImgWidth > 70) {
        glk_window_clear(Graphics);
        FreeAnimCols();
    }

    NumOldAnimCols = NumAnimCols;
    NumAnimCols = *ptr++;

    for (int i = 0; i <= 15; i++) {
        int blue = *(ptr + 1) & 0xf;
        int green = (*(ptr + 1) >> 4) & 0xf;
        int red = *ptr & 0xf;

        SetRGB(i, red, green, blue);

        ptr += 2;
    }

    AnimationColor *oldColAnim = NULL;

    if (AnimColors != NULL) {
        oldColAnim = AnimColors;
        AnimColors = NULL;
    }

    ptr = SetPaletteAnimation(ptr);

    Pixel **Pixels = MemAlloc(sizeof(Pixel*) * NumAnimCols);
    for (int i = 0; i < NumAnimCols; i++)
        Pixels[i] = MemAlloc(sizeof(Pixel) * 3000);

    uint16_t dataSize = imgdata[2] * 256 + imgdata[3];

    uint8_t *endOfData = ptr + dataSize;


    if (PatternLookup == NULL)
        GeneratePatternLookup();

    CurAddr = ImgAddrOffs;

    // CurAddr == screen memory where we're currently writing

    InitialMask = 0xf0;
    LastMask = 0x0f;
    NxtLine = CurAddr;
    LineNibblesLeft = ImgWidth;

    uint8_t c;

    do {
        c = *ptr++;
        if ((c & 0x80) == 0) {
            for (int i = c; i >= 0; i--) {
                c = *ptr++;
                DrawPattern(c, Pixels);
            }
        } else {
            // Repeat next byte (c & 0x7f) times
            c = c & 0x7f;
            uint8_t val = *ptr++;
            for (int i = c; i >= 0; i--) {
                DrawPattern(val, Pixels);
            }
        }
    } while (ptr <= endOfData);

    int ImgHeight = NxtLine / 160 - yoff;

    for (int i = 0; i < NumAnimCols; i++) {
        if (AnimColors[i].NumPix) {
            AnimColors[i].Pixels = MemAlloc(AnimColors[i].NumPix * sizeof(Pixel));
            memcpy(AnimColors[i].Pixels, Pixels[i], AnimColors[i].NumPix * sizeof(Pixel));
            free(Pixels[i]);
        }
    }
    free(Pixels);

    for (int i = 0; i < NumOldAnimCols; i++) {
        NumAnimCols += AddNonHiddenColAnim(&oldColAnim[i], AnimColors, NumAnimCols, xoff, yoff, ImgWidth * 4, ImgHeight);
    }

    if (NumAnimCols) {
        ColorCyclingRunning = 1;
        glk_request_timer_events(50);
    }

    return 1;
}

void ColorCyclingPixel(Pixel p, glui32 glk_color)
{
    glsi32 xpos = p.x * pixel_size;

    if (upside_down)
        xpos = ImageWidth * pixel_size - xpos - (pixel_size * (p.width - 2));
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    int ypos = p.y * pixel_size;
    if (upside_down)
        ypos = ImageHeight * pixel_size - ypos - 3 * pixel_size;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         ypos, pixel_size * p.width, pixel_size);
}

uint ColorCycle = 0;

void UpdateColorCycling(void) {
    for (int i = 0; i < NumAnimCols; i++) {
        if (ColorCycle % AnimColors[i].Rate == 0) {
            glui32 color = AnimColors[i].Colours[AnimColors[i].CurCol];
            AnimColors[i].CurCol++;
            if (AnimColors[i].CurCol >= AnimColors[i].NumCol)
                AnimColors[i].CurCol = 0;
            for (int j = 0; j < AnimColors[i].NumPix; j++) {
                ColorCyclingPixel(AnimColors[i].Pixels[j], color);
            }
        }
    }
    ColorCycle++;
}
