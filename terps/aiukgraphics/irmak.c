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
//  The Irmak graphics system renders tile-based images used in
//  Adventure International UK / Adventure Soft text adventures.
//  Each image is stored as a stream of tile references with
//  transformation commands (rotation, flip, overlay compositing),
//  followed by colour attribute data (ink/paper pairs per tile cell).
//
//  Rendering proceeds in three stages:
//    1. PerformTileTransformations — decode the tile stream, apply
//       rotations/flips/overlays, and write the result into layout[][].
//    2. DecodeAttributes — decode the RLE-compressed colour stream
//       into ink/paper arrays (or directly into imagebuffer).
//    3. DrawDecodedImage — rasterise the layout+attributes either
//       to the screen directly, or into imagebuffer for later compositing.
//
//  Two rendering paths exist: "direct" mode renders each image
//  independently to the screen; "buffer" mode composites multiple
//  sub-images into the shared imagebuffer[][] before a final
//  DrawIrmakPictureFromBuffer() renders the whole frame.
//

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "debugprint.h"
#include "memory_allocation.h"
#include "palette.h"

#include "irmak.h"

/* Command byte layout (when COMMAND_BIT is set):
     Bit 7 (0x80): COMMAND_BIT — distinguishes commands from solo tile indices
     Bit 6 (0x40): FLIP_BIT — horizontal mirror
     Bits 5-4 (0x30): ROTATE_BITS — 0/90/180/270 degree rotation
     Bits 3-2 (0x0C): OVERLAY_BITS — compositing mode (OR/AND/XOR) and
                       signals that an overlay chain follows
     Bit 1 (0x02): REPEAT_BIT — next byte is a repeat count
     Bit 0 (0x01): ADD_128_BIT — add 128 to the tile index (for tiles 128–255)
   OVERLAY_MASK (0xF3) masks out the overlay bits, passing through
   rotation/flip for Transform(). */
#define COMMAND_BIT      0x80
#define FLIP_BIT         0x40
#define ROTATE_BITS      0x30
#define OVERLAY_BITS     0x0C
#define OVERLAY_MASK     0xF3
#define REPEAT_BIT       0x02
#define ADD_128_BIT      0x01

/* Overlay compositing modes (values of OVERLAY_BITS) */
#define MODE_XOR         0x0c
#define MODE_AND         0x08
#define MODE_OR          0x04

/* Rotation modes (values of ROTATE_BITS) */
#define MODE_ROT90       0x10
#define MODE_ROT180      0x20
#define MODE_ROT270      0x30

/* Colour attribute masks for version 0–2 format.
   Version 3+ masks (INK_MASK, PAPER_MASK, BRIGHT_FLAG) are in irmak.h. */
#define OLD_PAPER_MASK   0x07
#define OLD_INK_MASK     0x70
#define V2_BRIGHT_FLAG   0x08

Image *images = NULL;
int number_of_images;
int image_version;

/* Called from SagaSetup() after the game data has been parsed. */
void InitIrmak(int numimg, int imgver) {
    number_of_images = numimg;
    image_version = imgver;
}

/* The tile font: 256 tiles, each 8 bytes (one byte per row, MSB = left). */
uint8_t tiles[256][8];

/* Scratch space for decoded tile data. Each cell holds 8 bytes of
   1-bpp pixel data built up by Transform() during decoding. */
uint8_t layout[IRMAK_IMGSIZE][8];

/* Compositing buffer for multi-part images. Bytes 0–7 hold tile pixel
   data (same format as layout), byte 8 holds the colour attribute.
   Multiple sub-images are drawn into this buffer, then the whole
   frame is rendered at once by DrawIrmakPictureFromBuffer(). */
uint8_t imagebuffer[IRMAK_IMGSIZE][9];

/* Forward declarations of necessary external functions */
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height, int32_t color);
void PutPixel(glsi32 x, glsi32 y, int32_t color);

/* Test whether bit n (0 = LSB, 7 = MSB) of c is set. */
int isNthBitSet(unsigned const char c, int n)
{
    static unsigned const char mask[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    return ((c & mask[n]) != 0);
}

/* Flip a tile horizontally, i.e. mirror it */
void Flip(uint8_t *tile)
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[i], j))
                work2[i] += 1 << (7 - j);
    memcpy(tile, work2, 8);
}

/* Rotate a tile 90 degrees clockwise. */
void Rot90(uint8_t *tile)
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[j], i))
                work2[7 - i] += 1 << j;

    memcpy(tile, work2, 8);
}

/* Rotate a tile 180 degrees. */
void Rot180(uint8_t *tile)
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[i], j))
                work2[7 - i] += 1 << (7 - j);

    memcpy(tile, work2, 8);
}

/* Rotate a tile 270 degrees clockwise (= 90 degrees counter-clockwise). */
void Rot270(uint8_t *tile)
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (isNthBitSet(tile[j], i))
                work2[i] += 1 << (7 - j);

    memcpy(tile, work2, 8);
}

/* Apply rotation, flip, and overlay transformations to a tile and
   write the result into layout[offset].

   The 'mode' byte encodes the full transformation:
     - ROTATE_BITS select 0/90/180/270 rotation
     - FLIP_BIT mirrors horizontally (after rotation)
     - OVERLAY_BITS select the compositing mode: direct copy (0),
       OR, AND, or XOR with the existing data in layout[offset].
       This allows multiple tiles to be layered on the same cell. */
static void Transform(uint8_t tile, uint8_t mode, int offset)
{
    uint8_t work[8];
    int32_t i;

    memcpy(work, tiles[tile], 8);

    uint8_t rotate_mode = mode & ROTATE_BITS;

    if (rotate_mode == MODE_ROT90) {
        Rot90(work);
    } else if (rotate_mode == MODE_ROT180) {
        Rot180(work);
    } else if (rotate_mode == MODE_ROT270) {
        Rot270(work);
    }

    if ((mode & FLIP_BIT) == FLIP_BIT) {
        Flip(work);
    }

    /* Composite onto layout: direct copy when no overlay mode is set,
       otherwise OR/AND/XOR with existing content. */
    mode &= OVERLAY_BITS;
    for (i = 0; i < 8; i++) {
        if (mode == MODE_XOR)
            layout[offset][i] ^= work[i];
        else if (mode == MODE_AND)
            layout[offset][i] &= work[i];
        else if (mode == MODE_OR)
            layout[offset][i] |= work[i];
        else
            layout[offset][i] = work[i];
    }
}

/* Fill a tile-sized (8x8) rectangle with a background colour.
   x and y are in tile coordinates. */
void FillBackground(int32_t x, int32_t y, int32_t color)
{
    RectFill(x * 8, y * 8, 8, 8, color);
}

/* Render a single tile cell from layout[] to the screen.
   Fills the cell with the background colour, then draws foreground
   pixels for each set bit. x/y are tile coordinates. */
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

/* Bounds check: returns true if the data pointer has reached or
   passed the end of the available data. */
static inline int DataExhausted(const uint8_t *dataptr, const uint8_t *origptr, size_t datasize)
{
    return (size_t)(dataptr - origptr) >= datasize;
}

/* Stage 1: Decode the tile data stream and populate layout[][].
   Each byte in the stream is either a solo tile index (COMMAND_BIT
   clear) or a command byte (COMMAND_BIT set) that specifies
   rotation, flip, overlay mode, an optional repeat count, and an
   optional chain of overlay tiles composited onto the same cells.

   Solo tiles:  [tile_index]
   Commands:    [command_byte] [count?] [tile] [overlay_chain...]

   The result is a fully decoded layout[][] ready for colouring
   in Stage 2 (DecodeAttributes). */
static void PerformTileTransformations(IrmakImgContext *ctx)
{
    uint8_t *dataptr = ctx->dataptr;
    uint8_t *origptr = ctx->origptr;
    int offset = 0;
    int imagesize = ctx->imagesize;

    /* Carries across iterations; the previous tile value
       determines ADD_128 behavior for solo tiles */
    uint8_t tile = 0;

    while (offset < imagesize) {
        if (DataExhausted(dataptr, origptr, ctx->datasize)) {
            fprintf(stderr, "PerformTileTransformations: tile data out of range\n");
            return;
        }

        uint8_t data = *dataptr++;
        int count = 1;

        if ((data & COMMAND_BIT) == 0) {
            /* Solo tile: the byte is the tile index directly.
               In version 3+, if the previous tile was in the upper
               half (128–255), solo indices are offset by 128 to
               stay in the same bank. */
            if (tile > 127 && image_version > 2) {
                data += 128;
            }
            tile = data;
            Transform(tile, 0, offset);
            offset++;
            if (offset > imagesize) break;
        } else {
            /* Command byte: may specify a repeated run and/or
               overlay tiles composited on top. */
            if ((data & REPEAT_BIT) == REPEAT_BIT) {
                if (DataExhausted(dataptr, origptr, ctx->datasize)) {
                    fprintf(stderr, "PerformTileTransformations: count byte out of range\n");
                    return;
                }
                count = (int)(*dataptr++) + 1;
            }

            if (DataExhausted(dataptr, origptr, ctx->datasize)) {
                fprintf(stderr, "PerformTileTransformations: tile byte out of range\n");
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

            /* Overlay chain: when OVERLAY_BITS are set, one or more
               additional tiles follow and are composited (OR/AND/XOR)
               onto the same cell(s). The chain continues as long as
               each successive command byte also has OVERLAY_BITS set.
               A tile index below COMMAND_BIT ends the chain as a
               direct (unmodified) overlay. */
            if ((data & OVERLAY_BITS) != 0) {
                uint8_t mask_mode = (data & OVERLAY_BITS);
                if (DataExhausted(dataptr, origptr, ctx->datasize)) {
                    fprintf(stderr, "PerformTileTransformations: overlay stream out of range\n");
                    return;
                }
                uint8_t data2 = *dataptr++;
                uint8_t previous = data;
                while (1) {
                    if (data2 < COMMAND_BIT) {
                        /* Direct overlay: tile index without transformation */
                        if (image_version == 4 && (previous & ADD_128_BIT) == ADD_128_BIT)
                            data2 += 128;
                        for (int i = 0; i < count; ++i)
                            Transform(data2, previous & OVERLAY_BITS, offset + i);
                        break;
                    }

                    if (DataExhausted(dataptr, origptr, ctx->datasize)) {
                        fprintf(stderr, "PerformTileTransformations: overlay tile out of range\n");
                        return;
                    }
                    tile = *dataptr++;
                    if ((data2 & ADD_128_BIT) == ADD_128_BIT)
                        tile += 128;

                    for (int i = 0; i < count; i++)
                        /* Use mask mode of previous command byte */
                        Transform(tile, (data2 & OVERLAY_MASK) | mask_mode, offset + i);

                    if ((data2 & OVERLAY_BITS) == 0)
                        break;

                    mask_mode = data2 & OVERLAY_BITS;
                    previous = data2;
                    if (DataExhausted(dataptr, origptr, ctx->datasize)) {
                        fprintf(stderr, "PerformTileTransformations: overlay chain ends prematurely\n");
                        return;
                    }
                    data2 = *dataptr++;
                }
            }
            offset += count;
        }
    }
    ctx->dataptr = dataptr;
}

/* Stage 2: Decode the RLE-compressed colour attribute stream.
   Each tile cell gets an ink (foreground) and paper (background) colour.

   The encoding is version-dependent:
     - Version 0–2: a non-command byte is a literal colour; a command
       byte specifies a repeat count and the colour byte follows.
     - Version 3+: a command byte repeats the *previous* colour (no
       explicit colour byte follows), saving one byte per run.

   In buffer mode, attributes are written to imagebuffer[][8].
   In direct mode, they are unpacked into the ink[] and paper[] arrays.
   Returns 1 on success, 0 on data exhaustion. */
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
    int warned = 0;

    while (y < ysize) {
        if (DataExhausted(dataptr, origptr, datasize)) {
            fprintf(stderr, "DecodeAttributes: data offset %zu out of range! Image size %zu. Bailing!\n",
                    (size_t)(dataptr - origptr), datasize);
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
                if (DataExhausted(dataptr, origptr, datasize)) {
                    fprintf(stderr, "DecodeAttributes: missing colour byte\n");
                    return 0;
                }
                colour = *dataptr++;
            }
        } else {
            count = 1;
            colour = data;
        }

        while (count > 0) {
            count--;
            if (ctx->draw_to_buffer) {
                /* write colours into imagebuffer */
                unsigned bufpos = (yoff + y) * IRMAK_IMGWIDTH + xoff + x;
                if (bufpos < IRMAK_IMGSIZE) {
                    imagebuffer[bufpos][8] = colour;
                } else {
                    /* out of buffer: ignore but warn once */
                    if (!warned) {
                        fprintf(stderr, "DecodeAttributes: attribute write out of range! bufpos:%d\n", bufpos);
                        warned = 1;
                    }
                }
            } else { /* Not drawing to buffer */
                if (x >= xsize) {
                    fprintf(stderr, "DecodeAttributes: x position out of range\n");
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
                if (y >= ysize) break;
            }
        }
    }

    ctx->dataptr = dataptr;
    return 1;
}

/* Stage 3: Compose the final image from layout[][] and colour data.
   In buffer mode, copies layout rows into imagebuffer (attributes
   were already written in Stage 2). In direct mode, renders each
   tile to the screen immediately using ink/paper colours. */
static void DrawDecodedImage(IrmakImgContext *ctx, uint8_t *ink, uint8_t *paper)
{
    int xsize = ctx->xsize;
    int ysize = ctx->ysize;
    int yoff = ctx->yoff;
    int xoff = ctx->xoff;
    /* Version 1–2 images are offset 4 tiles to the right in the data
       but should be drawn starting from column 0. */
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

/* Reset the compositing buffer to blank (all tiles cleared,
   all attributes zeroed). Called before drawing a new frame. */
void ClearGraphMem(void)
{
    memset(imagebuffer, 0, IRMAK_IMGSIZE * 9);
}

/* Decode and render a single image from its data stream. Runs all
   three stages in sequence: tile transforms → attribute decode →
   final compositing/rendering. In direct mode, ink[] and paper[]
   carry colours between stages 2 and 3; in buffer mode they are
   unused (attributes go straight into imagebuffer[][8]). */
void DrawIrmakPictureFromContext(IrmakImgContext ctx)
{
    if (!ctx.dataptr) return;

    ctx.imagesize = ctx.xsize * ctx.ysize;
    ctx.origptr = ctx.dataptr;

    PerformTileTransformations(&ctx);

    uint8_t ink[IRMAK_IMGSIZE];
    uint8_t paper[IRMAK_IMGSIZE];
    if (DecodeAttributes(&ctx, ink, paper)) {
        DrawDecodedImage(&ctx, ink, paper);
    }
}

/* Render the entire compositing buffer to the screen. Called after
   all sub-images have been drawn into imagebuffer[][] via buffer mode.
   Extracts ink/paper colours from the attribute byte (index [8]) and
   renders each cell's 1-bpp pixel data (indices [0]–[7]).

   Uses fast paths for fully blank (0x00) and fully set (0xFF) rows
   to avoid per-pixel overhead. */
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
                if (imagebuffer[index][i] == 0)
                    continue;
                if (imagebuffer[index][i] == 255) {
                    RectFill(col * 8, line * 8 + i, 8, 1, ink);
                    continue;
                }
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

/* Draw a picture at an explicit tile position (x, y). If x or y
   are out of range (>= the image dimensions), the image's own
   stored offset is used as a fallback — this lets callers pass
   (uint8_t)-1 to mean "use the default position".
   Returns 1 on success, 0 if the image is invalid or missing. */
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

/* Convenience wrapper: draw a picture at its default position. */
void DrawPictureNumber(int picture_number, int draw_to_buffer)
{
    DrawPictureAtPos(picture_number, -1, -1, draw_to_buffer);
    last_image_index = picture_number;
}
