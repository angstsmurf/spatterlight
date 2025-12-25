//
//  irmak.c
//  Drawing code for Adventure International UK / Adventure Soft graphics
//  Used in ScottFree and TaylorMade
//
//  Created by Administrator on 2025-12-07.
//
//  Based on Sagadraw v2.5 by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//
//
//  The 8 x 8 pixel graphic building blocks used for these images were
//  previously referred to in code as "characters", but that was a bit
//  confusing, as they don't have anything to do with the characters
//  used to print text. Also, this kind of computer graphics is commonly
//  called "tile based" when used in arcade games and similar.
//
//  All instances of "character" are now changed to "tile".
//  Hopefully this will make things less rather than more confusing.
//

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "debugprint.h"
#include "memory_allocation.h"
#include "palette.h"

#include "irmak.h"

/* constants for bitmasks / flags used in encodings */
#define COMMAND_BIT      0x80
#define FLIP_BIT         0x40
#define ROTATE_BITS      0x30
#define OVERLAY_BITS     0x0C
#define OVERLAY_MASK     0xF3
#define REPEAT_BIT       0x02
#define ADD_128_BIT      0x01

#define OLD_PAPER_MASK   0x07
#define OLD_INK_MASK     0x70
#define V2_BRIGHT_FLAG   0x08

Image *images = NULL;
int number_of_images;
int image_version;

void InitIrmak(int numimg, int imgver) {
    number_of_images = numimg;
    image_version = imgver;
}

uint8_t tiles[256][8];
uint8_t layout[IRMAK_IMGSIZE][8];
uint8_t imagebuffer[IRMAK_IMGSIZE][9];

/* Forward declarations of necessary external functions */
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height, int32_t color);
void PutPixel(glsi32 x, glsi32 y, int32_t color);

int isNthBitSet(unsigned const char c, int n)
{
    static unsigned const char mask[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    return ((c & mask[n]) != 0);
}

/* Flip a tile horizontally, i.e. mirror it */
void Flip(uint8_t tile[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[i], j))
                work2[i] += 1 << (7 - j);
    for (i = 0; i < 8; i++)
        tile[i] = work2[i];
}

static void rot90(uint8_t tile[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[j], i))
                work2[7 - i] += 1 << j;

    for (i = 0; i < 8; i++)
        tile[i] = work2[i];
}

static void rot270(uint8_t tile[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[j], i))
                work2[i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        tile[i] = work2[i];
}

static void rot180(uint8_t tile[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[i], j))
                work2[7 - i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        tile[i] = work2[i];
}

/* Apply rotation, flip and overlay transformations to a tile and
 write the result into layout[offset] */
static void Transform(uint8_t tile, uint8_t mode, int offset)
{
    uint8_t work[8];
    int32_t i;

#ifdef DRAWDEBUG
    debug_print("Plotting char: %d with flip: %02x (%s) at %d: %d,%d\n",
                tile, flip_mode, flipdescription[(flip_mode & 48) >> 4], offset,
                offset % 0x20, offset / 0x20);
#endif
    /* first copy the tile into work */
    for (i = 0; i < 8; i++)
        work[i] = tiles[tile][i];

    uint8_t rotate_mode = mode & ROTATE_BITS;

    /* Now rotate it */
    if (rotate_mode == 0x10) {
        rot90(work);
    } else if (rotate_mode == 0x20) {
        rot180(work);
    } else if (rotate_mode == 0x30) {
        rot270(work);
    }

    /* We flip the tile horizontally
       if FLIP_BIT is set */
    if ((mode & FLIP_BIT) == FLIP_BIT) {
        Flip(work);
    }

    /* Now mask it onto the previous tile */
    mode &= OVERLAY_BITS;
    for (i = 0; i < 8; i++) {
        if (mode == 0x0c)
            layout[offset][i] ^= work[i];
        else if (mode == 0x08)
            layout[offset][i] &= work[i];
        else if (mode == 0x04)
            layout[offset][i] |= work[i];
        else
            layout[offset][i] = work[i];
    }
}

void FillBackground(int32_t x, int32_t y, int32_t color)
{
    /* Draw the background */
    RectFill(x * 8, y * 8, 8, 8, color);
}

void PlotTile(int32_t tile, int32_t x, int32_t y, int32_t fg,
              int32_t bg)
{
    if (fg > 15)
        fg = 0;
    if (bg > 15)
        bg = 0;
    int32_t i, j;
    FillBackground(x, y, bg);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            if (isNthBitSet(layout[tile][i], (7 - j)))
                PutPixel(x * 8 + j, y * 8 + i, fg);
    }
}

/* Apply the tile transformation data.
   The result is written into layout[][]
   by the Transform() function */
static void PerformTileTranformations(IrmakImgContext *ctx)
{
    uint8_t *dataptr = ctx->dataptr;
    uint8_t *origptr = ctx->origptr;
    int offset = 0;
    int imagesize = ctx->imagesize;

    uint8_t tile = 0;

    while (offset < imagesize) {
        if ((size_t)(dataptr - origptr) >= ctx->datasize) {
            fprintf(stderr, "PerformTileTranformations: tile data out of range\n");
            return;
        }

        uint8_t data = *dataptr++;
        int count = 1;

        /* Check if this is a "command" byte */
        if ((data & COMMAND_BIT) == 0) {
            /* Solo tile */
            if (tile > 127 && image_version > 2) {
                data += 128;
            }
            tile = data;
#ifdef DRAWDEBUG
            debug_print("******* SOLO TILE: %04x\n", tile);
#endif
            Transform(tile, 0, offset);
            offset++;
            if (offset > imagesize) break;
        } else {
            /* Possibly a repeated run with optional overlays */
            if ((data & REPEAT_BIT) == REPEAT_BIT) {
                if ((size_t)(dataptr - origptr) >= ctx->datasize) {
                    fprintf(stderr, "PerformTileTranformations: count byte out of range\n");
                    return;
                }
                count = (int)(*dataptr++) + 1;
            }

            if ((size_t)(dataptr - origptr) >= ctx->datasize) {
                fprintf(stderr, "PerformTileTranformations: tile byte out of range\n");
                return;
            }

            /* Get tile and plot it (count) times */
            tile = *dataptr++;

            if ((data & ADD_128_BIT) == ADD_128_BIT && tile < 128)
                tile += 128;

            for (int i = 0; i < count; i++) {
                if (offset + i >= imagesize) {
                    ctx->dataptr = dataptr;
                    return;
                }
                Transform(tile, data & OVERLAY_MASK, offset + i); /* Ignore overlay bits */
            }

            /* overlays handling */
            if ((data & OVERLAY_BITS) != 0) {
                uint8_t mask_mode = (data & OVERLAY_BITS);
                if ((size_t)(dataptr - origptr) >= ctx->datasize) {
                    fprintf(stderr, "PerformTileTranformations: overlay stream out of range\n");
                    return;
                }
                uint8_t data2 = *dataptr++;
                uint8_t previous = data;
                int keep_going;
                do {
                    keep_going = 0;
                    if (data2 < COMMAND_BIT) {
#ifdef DRAWDEBUG
                        debug_print("Plotting %d directly (overlay) at %d\n", data2,
                                    offset);
#endif
                        /* direct plotting overlay */
                        if (image_version == 4 && (previous & ADD_128_BIT) == ADD_128_BIT)
                            data2 += 128;
                        for (int i = 0; i < count; ++i)
                            Transform(data2, previous & OVERLAY_BITS, offset + i);
                    } else {
                        if ((size_t)(dataptr - origptr) >= ctx->datasize) {
                            fprintf(stderr, "PerformTileTranformations: overlay tile out of range\n");
                            return;
                        }
                        tile = *dataptr++;
                        if ((data2 & ADD_128_BIT) == ADD_128_BIT)
                            tile += 128;
#ifdef DRAWDEBUG
                        debug_print("Plotting %d with flip %02x (%s) at %d %d\n",
                                    tile, (data2 | mask_mode),
                                    flipdescription[((data2 | mask_mode) & 48) >> 4], offset,
                                    count);
#endif
                        for (int i = 0; i < count; i++)
                            /* Use mask mode of previous command byte */
                            Transform(tile, (data2 & OVERLAY_MASK) | mask_mode, offset + i);

                        if ((data2 & OVERLAY_BITS) != 0) {
                            mask_mode = data2 & OVERLAY_BITS;
                            previous = data2;
                            keep_going = 1;
                            if ((size_t)(dataptr - origptr) > ctx->datasize) {
                                fprintf(stderr, "PerformTileTranformations: overlay chain ends prematurely\n");
                                return;
                            }
                            data2 = *dataptr++;
                        }
                    }
                } while (keep_going);
            }
            offset += count;
        }
    }
    ctx->dataptr = dataptr;
}

/* Parse attribute (ink/paper) data after tiles have been transformed
 and placed. If not drawing to buffer, this allocates ink and paper
 arrays (*out_ink and *out_paper) which the caller must free.

 If we *are* drawing to buffer, the ink and paper arguments
 will be unused, and the attributes will be written to
 the ninth byte of the corresponding imagebuffer[][] cell instead. */
static int DecodeAttributes(IrmakImgContext *ctx, uint8_t *ink, uint8_t *paper)
{
    uint8_t *dataptr = ctx->dataptr;
    uint8_t *origptr = ctx->origptr;
    size_t datasize = ctx->datasize;
    int xsize = ctx->xsize;
    int ysize = ctx->ysize;
    int xoff = ctx->xoff;
    int yoff = ctx->yoff;

    int y = 0;
    int x = 0;
    uint8_t colour = 0;

    while (y < ysize) {
        if ((size_t)(dataptr - origptr) > datasize) {
            fprintf(stderr, "DecodeAttributes: data offset %zu out of range! Image size %zu. Bailing!\n",
                    (size_t)(dataptr - origptr), datasize);
            /* free on error */
            return 0;
        }
        uint8_t data = *dataptr++;
        int count;
        if (data & COMMAND_BIT) {
            count = data - COMMAND_BIT + 1;
            if (image_version >= 3) {
                /* in version 3 and above, repeat the *previous* colour byte */
                count--;
            } else {
                /* in version 2 and below, repeat the *following* colour byte */
                if ((size_t)(dataptr - origptr) > datasize) {
                    fprintf(stderr, "DecodeAttributes: missing colour byte\n");
                    return 0;
                }
                colour = *dataptr++;
            }
        } else {
            count = 1;
            colour = data;
        }

        for (int i = 0; i < count; count--) {
            if (ctx->draw_to_buffer) {
                /* write colours into imagebuffer */
                unsigned bufpos = (yoff + y) * IRMAK_IMGWIDTH + xoff + x;
                if (bufpos < IRMAK_IMGSIZE) {
                    imagebuffer[bufpos][8] = colour;
                } else {
                    /* out of buffer: ignore but warn once */
                    static int warned = 0;
                    if (!warned) {
                        fprintf(stderr, "DecodeAttributes: attribute write out of range! bufpos:%d\n", bufpos);
                        warned = 1;
                    }
                }
            } else { /* Not drawing to buffer */
                if (x >= xsize) {
                    fprintf(stderr, "parse_attributes: x position out of range\n");
                    return 0;
                }
                /* write colours into ink/paper arrays */
                int idx = y * xsize + x;
                if (idx >= ctx->imagesize)
                    break;
                if (image_version > 2) {
                    ink[idx] = colour & INK_MASK;
                    paper[idx] = (colour & PAPER_MASK) >> 3;
                    if ((colour & BRIGHT_FLAG) == BRIGHT_FLAG) {
                        paper[idx] += 8;
                        ink[idx] += 8;
                    }
                } else {
                    paper[idx] = colour & OLD_PAPER_MASK;
                    ink[idx] = ((colour & OLD_INK_MASK) >> 4);
                    /* Version 0 and 1 always use bright colours */
                    if ((colour & V2_BRIGHT_FLAG) == V2_BRIGHT_FLAG || image_version < 2) {
                        paper[idx] += 8;
                        ink[idx] += 8;
                    }
                }
            }

            x++;
            if (x == xsize) {
                x = 0;
                y++;
                if (y > ysize) break;
            }
        }
    }

    ctx->dataptr = dataptr;
    return 1;
}

/* compose the final image: copy layout into imagebuffer
   (if ctx->draw_to_buffer is set) or render directly with PlotTile()
   using ink and paper arrays. The ink and paper arguments are not
   used if we are drawing to the buffer. */
static void DrawDecodedImage(IrmakImgContext *ctx, uint8_t *ink, uint8_t *paper)
{
    int xsize = ctx->xsize;
    int ysize = ctx->ysize;
    int yoff = ctx->yoff;
    int xoff = ctx->xoff;
    if (image_version > 0 && image_version < 3)
        xoff -= 4;

    int offset = 0;

    for (int y = 0; y < ysize; y++) {
        for (int x = 0; x < xsize; x++) {
            if (ctx->draw_to_buffer) {
                /* We only replace the data in imagebuffer
                   which the image covers. Any other data written
                   by previous draw operations is left intact. */
                unsigned bufpos = (y + yoff) * IRMAK_IMGWIDTH + x + xoff;
                if (bufpos < IRMAK_IMGSIZE) {
                    for (int i = 0; i < 8; ++i)
                        imagebuffer[bufpos][i] = layout[offset][i];
                }
            } else {
                int idx = y * xsize + x;
                int fg = Remap(ink[idx]);
                int bg = Remap(paper[idx]);
                PlotTile(offset, x + xoff, y + yoff, fg, bg);
            }

            offset++;
            if (offset > ctx->imagesize)
                return;
        }
    }
}


uint8_t *GetOffsetInPixmap(int x, int y, uint8_t *pixmap, size_t stride)
{
    return pixmap + x * 4 + y * stride;
}

enum {
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    ALPHA = 3,
};

typedef uint8_t RGB[3];
typedef RGB PALETTE[16];

extern PALETTE pal;

void ClearGraphMem(void)
{
    memset(imagebuffer, 0, IRMAK_IMGSIZE * 9);
}

void PutPixelInPixmap(int x, int y, uint8_t *pixmap, size_t stride, uint8_t color)
{
    uint8_t *offset = GetOffsetInPixmap(x, y, pixmap, stride);
    offset[RED]   = pal[color][RED];
    offset[GREEN] = pal[color][GREEN];
    offset[BLUE]  = pal[color][BLUE];
    offset[ALPHA] = 0xff;
}

void FillPixmapBackground(int x, int y, uint8_t *pixmap, size_t stride, uint8_t color) {
    for (int line = 0; line < 8; line++) {
        for (int col = 0; col < 8; col++) {
            PutPixelInPixmap(x * 8 + col, y * 8 + line, pixmap, stride, color);
        }
    }
}

void DrawIrmakPictureFromContext(IrmakImgContext ctx)
{
    if (!ctx.dataptr) return;

    ctx.imagesize = ctx.xsize * ctx.ysize;
    ctx.origptr = ctx.dataptr;

    /* Step 1: Transform and draw tiles into layout[][] */
    PerformTileTranformations(&ctx);

    /* Step 2: Write attribute bytes */
    uint8_t ink[IRMAK_IMGSIZE];
    uint8_t paper[IRMAK_IMGSIZE];
    /* The ink and paper arguments will only be used
       if we are not drawing to buffer */
    if (DecodeAttributes(&ctx, ink, paper)) {
        /* Step 3: compose image to buffer or direct render */
        /* The ink and paper arguments will still not be used
           if we are drawing to buffer. */
        DrawDecodedImage(&ctx, ink, paper);
    }
}

void DrawIrmakPictureFromBuffer(void)
{
    for (int line = 0; line < 12; line++) {
        for (int col = 0; col < 32; col++) {
            int index = col + line * IRMAK_IMGWIDTH;

            uint8_t colour = imagebuffer[index][8];

            int paper = (colour & PAPER_MASK) >> 3;
            if ((colour & BRIGHT_FLAG) == BRIGHT_FLAG)
                paper += 8;
            paper = Remap(paper);
            int ink = colour & INK_MASK;
            if ((colour & BRIGHT_FLAG) == BRIGHT_FLAG)
                ink += 8;
            ink = Remap(ink);

            FillBackground(col, line, paper);

            for (int i = 0; i < 8; i++) {
                /* Don't draw anything if current byte is zero */
                if (imagebuffer[index][i] == 0)
                    continue;
                /* Draw a single box if current byte is 255 */
                if (imagebuffer[index][i] == 255) {
                    RectFill(col * 8, line * 8 + i, 8, 1, ink);
                    continue;
                }
                /* Else check every bit and draw a pixel if set */
                for (int j = 0; j < 8; j++) {
                    if (isNthBitSet(imagebuffer[index][i], (7 - j))) {
                        int ypos = line * 8 + i;
                        PutPixel(col * 8 + j, ypos, ink);
                    }
                }
            }
        }
    }
}

int DrawPictureAtPos(int picture_number, uint8_t x, uint8_t y, int draw_to_buffer)
{

    if (number_of_images == 0)
        return 0;
    if (picture_number >= number_of_images) {
        debug_print("Invalid image number %d! Last image:%d\n", picture_number,
                    number_of_images - 1);
        return 0;
    }

    Image img = images[picture_number];
    if (img.imagedata == NULL)
        return 0;

    if (x >= IRMAK_IMGWIDTH) {
        x = img.xoff;
    }
    if (y >= IRMAK_IMGHEIGHT) {
        y = img.yoff;
    }

    IrmakImgContext ctx;
    ctx.dataptr = img.imagedata;
    ctx.xsize = img.width;
    ctx.ysize = img.height;
    ctx.xoff = x;
    ctx.yoff = y;
    ctx.datasize = img.datasize;
    ctx.draw_to_buffer = draw_to_buffer;

    DrawIrmakPictureFromContext(ctx);
    return 1;
}

extern int last_image_index;

void DrawPictureNumber(int picture_number, int draw_to_buffer)
{
    DrawPictureAtPos(picture_number, -1, -1, draw_to_buffer);
    last_image_index = picture_number;
}
