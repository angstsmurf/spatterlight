/* Sagadraw v2.5
 Originally by David Lodge

 With help from Paul David Doherty (including the colour code)
 */

#include "glk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "taylor.h"
#include "utility.h"

#include "sagadraw.h"

int draw_to_buffer = 1;

uint8_t sprite[256][8];
uint8_t screenchars[768][8];
uint8_t buffer[384][9];

Image *images = NULL;

glui32 pixel_size;
glui32 x_offset;

uint8_t *taylor_image_data;

/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

int white_colour = 15;
int blue_colour = 9;

int32_t errorcount = 0;

palette_type palchosen = NO_PALETTE;

#define STRIDE 765 /* 255 pixels * 3 (r, g, b) */

#define INVALIDCOLOR 16

const char *flipdescription[] = { "none", "90°", "180°", "270°", "ERROR" };

//#define DRAWDEBUG

void colrange(int32_t c)
{
    if ((c < 0) || (c > 15)) {
#ifdef DRAWDEBUG
        fprintf(stderr, "# col out of range: %d\n", c);
#endif
        errorcount++;
    }
}

void checkrange(int32_t x, int32_t y)
{
    if ((x < 0) || (x > 254)) {
#ifdef DRAWDEBUG
        fprintf(stderr, "# x out of range: %d\n", x);
#endif
        errorcount++;
    }
    if ((y < 96) || (y > 191)) {
#ifdef DRAWDEBUG
        fprintf(stderr, "# y out of range: %d\n", y);
#endif
        errorcount++;
    }
}

void do_palette(const char *palname)
{
    if (strcmp("zx", palname) == 0)
        palchosen = ZX;
    else if (strcmp("zxopt", palname) == 0)
        palchosen = ZXOPT;
    else if (strcmp("c64a", palname) == 0)
        palchosen = C64A;
    else if (strcmp("c64b", palname) == 0)
        palchosen = C64B;
    else if (strcmp("vga", palname) == 0)
        palchosen = VGA;
}

void set_color(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}

void DefinePalette(void)
{
    /* set up the palette */
    if (palchosen == VGA) {
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 255 };
        RGB red = { 255, 0, 0 };
        RGB magenta = { 255, 0, 255 };
        RGB green = { 0, 255, 0 };
        RGB cyan = { 0, 255, 255 };
        RGB yellow = { 255, 255, 0 };
        RGB white = { 255, 255, 255 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 255 };
        RGB brred = { 255, 0, 0 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);
    } else if (palchosen == ZX) {
        /* corrected Sinclair ZX palette (pretty dull though) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 154 };
        RGB red = { 154, 0, 0 };
        RGB magenta = { 154, 0, 154 };
        RGB green = { 0, 154, 0 };
        RGB cyan = { 0, 154, 154 };
        RGB yellow = { 154, 154, 0 };
        RGB white = { 154, 154, 154 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 170 };
        RGB brred = { 186, 0, 0 };
        RGB brmagenta = { 206, 0, 206 };
        RGB brgreen = { 0, 206, 0 };
        RGB brcyan = { 0, 223, 223 };
        RGB bryellow = { 239, 239, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;

    } else if (palchosen == ZXOPT) {
        /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 202 };
        RGB red = {
            202,
            0,
            0,
        };
        RGB magenta = { 202, 0, 202 };
        RGB green = { 0, 202, 0 };
        RGB cyan = { 0, 202, 202 };
        RGB yellow = { 202, 202, 0 };
        RGB white = { 202, 202, 202 };
        /*
         old David Lodge palette:

         RGB black = { 0, 0, 0 };
         RGB blue = { 0, 0, 214 };
         RGB red = { 214, 0, 0 };
         RGB magenta = { 214, 0, 214 };
         RGB green = { 0, 214, 0 };
         RGB cyan = { 0, 214, 214 };
         RGB yellow = { 214, 214, 0 };
         RGB white = { 214, 214, 214 };
         */
        RGB brblack = { 0, 0, 0 };
        RGB brblue = {
            0,
            0,
            255,
        };
        RGB brred = { 255, 0, 20 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;

    } else if ((palchosen == C64A) || (palchosen == C64B)) {
        /* and now: C64 palette (pepto/VICE) */
        RGB black = { 0, 0, 0 };
        RGB white = { 255, 255, 255 };
        RGB red = { 191, 97, 72 };
        RGB cyan = { 153, 230, 249 };
        RGB purple = { 177, 89, 185 };
        RGB green = { 121, 213, 112 };
        RGB blue = { 95, 72, 233 };
        RGB yellow = { 247, 255, 108 };
        RGB orange = { 186, 134, 32 };
        RGB brown = { 116, 105, 0 };
        RGB lred = { 180, 105, 164 };
        RGB dgrey = { 69, 69, 69 };
        RGB grey = { 167, 167, 167 };
        RGB lgreen = { 154, 210, 134 };
        RGB lblue = { 162, 143, 255 };
        RGB lgrey = { 150, 150, 150 };

        set_color(0, &black);
        set_color(1, &white);
        set_color(2, &red);
        set_color(3, &cyan);
        set_color(4, &purple);
        set_color(5, &green);
        set_color(6, &blue);
        set_color(7, &yellow);
        set_color(8, &orange);
        set_color(9, &brown);
        set_color(10, &lred);
        set_color(11, &dgrey);
        set_color(12, &grey);
        set_color(13, &lgreen);
        set_color(14, &lblue);
        set_color(15, &lgrey);

        white_colour = 1;
        blue_colour = 6;
    }
}

const char *colortext(int32_t col)
{
    const char *zxcolorname[] = {
        "black",
        "blue",
        "red",
        "magenta",
        "green",
        "cyan",
        "yellow",
        "white",
        "bright black",
        "bright blue",
        "bright red",
        "bright magenta",
        "bright green",
        "bright cyan",
        "bright yellow",
        "bright white",
        "INVALID",
    };

    const char *c64colorname[] = {
        "black",
        "white",
        "red",
        "cyan",
        "purple",
        "green",
        "blue",
        "yellow",
        "orange",
        "brown",
        "light red",
        "dark grey",
        "grey",
        "light green",
        "light blue",
        "light grey",
        "INVALID",
    };

    if ((palchosen == C64A) || (palchosen == C64B))
        return (c64colorname[col]);
    else
        return (zxcolorname[col]);
}

int32_t remap(int32_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* nothing to remap here; shows that the gfx were created on a ZX */
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);
    } else if (palchosen == C64A) {
        /* remap A determined from Golden Baton, applies to S1/S3/S13 too (8col) */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
            8,
            1,
            1,
            1,
            7,
            12,
            8,
            7,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64B) {
        /* remap B determined from Spiderman (16col) */
        int32_t c64remap[] = {
            0,
            6,
            9,
            4,
            5,
            14,
            8,
            12,
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);

    return (mapcol);
}

/* real code starts here */

void Flip(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[i] & (1 << j)) != 0)
                work2[i] += 1 << (7 - j);
    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

void rot90(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[j] & (1 << i)) != 0)
                work2[7 - i] += 1 << j;

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

void rot270(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[j] & (1 << i)) != 0)
                work2[i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

void rot180(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[i] & (1 << j)) != 0)
                work2[7 - i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

void transform(int32_t character, int32_t flip_mode, int32_t ptr)
{
    uint8_t work[8];
    int32_t i;

#ifdef DRAWDEBUG
    fprintf(stderr, "Plotting char: %d with flip: %02x (%s) at %d: %d,%d\n",
            character, flip_mode, flipdescription[(flip_mode & 48) >> 4], ptr,
            ptr % 0x20, ptr / 0x20);
#endif

    // first copy the character into work
    for (i = 0; i < 8; i++)
        work[i] = sprite[character][i];

    // Now flip it
    if ((flip_mode & 0x30) == 0x10) {
        rot90(work);
        //      fprintf(stderr, "rot 90 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x20) {
        rot180(work);
        //       fprintf(stderr, "rot 180 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x30) {
        rot270(work);
        //       fprintf(stderr, "rot 270 character %d\n",character);
    }
    if ((flip_mode & 0x40) != 0) {
        Flip(work);
        /* fprintf("flipping character %d\n",character); */
    }
    Flip(work);

    // Now mask it onto the previous character
    for (i = 0; i < 8; i++) {
        if ((flip_mode & 0x0c) == 12)
            screenchars[ptr][i] ^= work[i];
        else if ((flip_mode & 0x0c) == 8)
            screenchars[ptr][i] &= work[i];
        else if ((flip_mode & 0x0c) == 4)
            screenchars[ptr][i] |= work[i];
        else
            screenchars[ptr][i] = work[i];
    }
}

void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    int y_offset = 0;

    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
                         y * pixel_size + y_offset, pixel_size, pixel_size);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color)
{
    glui32 y_offset = 0;

    int bufferpos = (y / 8) * 32 + (x / 8);
    if (bufferpos >= 0xD80)
        return;
    buffer[bufferpos][8] = (uint8_t)(buffer[bufferpos][8] | (color << 3));

    glui32 glk_color = (glui32)(((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]));

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
                         y * pixel_size + y_offset, width * pixel_size,
                         height * pixel_size);
}

void background(int32_t x, int32_t y, int32_t color)
{
    /* Draw the background */
    RectFill(x * 8, y * 8, 8, 8, color);
}

void plotsprite(int32_t character, int32_t x, int32_t y, int32_t fg,
                int32_t bg)
{
    int32_t i, j;
    background(x, y, bg);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            if ((screenchars[character][i] & (1 << j)) != 0)
                PutPixel(x * 8 + j, y * 8 + i, fg);
    }
}

int isNthBitSet(unsigned const char c, int n)
{
    static unsigned const char mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };
    return ((c & mask[n]) != 0);
}
uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                 int xoff, int yoff);

struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { NUMGAMES, 0, 0, 0, "" },
};

int FindImagePatch(GameIDType game, int image_number, int start)
{
    for (int i = start + 1; image_patches[i].id != NUMGAMES; i++) {
        if (image_patches[i].id == game && image_patches[i].picture_number == image_number) {
            return i;
        }
    }
    return 0;
}

void Patch(uint8_t *data, uint8_t *offset, int patch_number)
{
    struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        uint8_t newval = patch->patch[i];
        //    fprintf(stderr, "Patch: changing offset %d in image %d from %x to
        //    %x.\n",
        //            i + patch->offset, patch->picture_number, offset[i +
        //            patch->offset], newval);
        offset[i + patch->offset] = newval;
    }
}

static size_t FindCharacterStart(void)
{
    /* Look for the character data */
    size_t pos = FindCode("\x00\x00\x00\x00\x00\x00\x00\x00\x80\x80\x80\x80\x80\x80\x80\x80\x40\x40\x40\x40\x40\x40\x40\x40", 0, 24);
    if(pos == -1) {
        fprintf(stderr, "Cannot find character data.\n");
        exit(1);
    }
    fprintf(stderr, "Found characters at pos %zx\n", pos);
    return pos;
}

void OpenGraphicsWindow(void);
void draw_taylor(int loc);
void HitEnter(void);

void SagaSetup(size_t imgoffset)
{
    int32_t i, y;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        fprintf(stderr, "unknown palette\n");
        exit(EXIT_FAILURE);
    }

    DefinePalette();

//    int32_t CHAR_START = Game->start_of_characters + FileBaselineOffset;
    size_t CHAR_START = FindCharacterStart();
    fprintf(stderr, "CHAR_START: %zx (%zu)\n", CHAR_START, CHAR_START);
    size_t image_blocks_start_address = Game->start_of_image_blocks + FileBaselineOffset;

    if (Game->start_of_image_blocks == FOLLOWS) {
        image_blocks_start_address = CHAR_START + 0x800;
    }

    int32_t DATA_OFFSET = 0; // = Game->image_address_offset + FileBaselineOffset;
    if (imgoffset)
        DATA_OFFSET = (int32_t)imgoffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
jumpChar:
    pos = SeekToPos(FileImage, CHAR_START);

#ifdef DRAWDEBUG
    fprintf(stderr, "Grabbing Character details\n");
    fprintf(stderr, "Character Offset: %04lx\n",
            CHAR_START - FileBaselineOffset);
#endif
    for (i = 0; i < 256; i++) {
//        fprintf (stderr, "\nCharacter %d:\n", i);
        for (y = 0; y < 8; y++) {
            sprite[i][y] = *(pos++);
//            if ((i == 0 && sprite[i][y] != 0) || (i == 1 &&
//                                                  sprite[i][y] != 255)) {
//                CHAR_START--;
//                goto jumpChar;
//            }
//
//            fprintf (stderr, "\n%s$%04lX DEFS ", (y == 0) ? "s" : " ", CHAR_START + i * 8 + y);
//            for (int n = 0; n < 8; n++) {
//                if (isNthBitSet(sprite[i][y], n))
//                    fprintf (stderr, "■");
//                else
//                    fprintf (stderr, "0");
//            }
        }
    }
//    fprintf(stderr, "Final CHAR_START: %04lx\n",
//              CHAR_START - FileBaselineOffset);

//      fprintf(stderr, "File offset after reading char data: %ld (%lx)\n",
//              pos - FileImage - FileBaselineOffset,
//              pos - FileImage - FileBaselineOffset);

    /* Now we have hopefully read the character data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;

//    fprintf(stderr, "Final OFFSET_TABLE_START:%lx\n", image_blocks_start_address -
//       FileBaselineOffset);
//    fprintf(stderr, "File offset after reading image offset data: %ld (%lx)\n", pos - FileImage - FileBaselineOffset, pos
//       - FileImage - FileBaselineOffset);

    pos = SeekToPos(FileImage, image_blocks_start_address);

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {

        uint8_t widthheight = *pos++;
        img->width = ((widthheight & 0xf0) >> 4) + 1;
        fprintf (stderr, "width of image %d: %d\n", picture_number, img->width);

        img->height = (widthheight & 0x0f) + 1;
fprintf (stderr, "height of image %d: %d\n", picture_number, img->height);

        uint8_t instructions[2048];
        int number = 0;
        do {
            instructions[number++] = *pos;
//            fprintf(stderr, "Instruction %d is 0x%02x (0x%04lx)\n", number - 1, *pos, pos - FileImage + 0x4000);
            for (int i = 0; i < 0x12; i++) {
                if (*pos == FileImage[0x7837 - 0x4000 + i]) {
//                    fprintf(stderr, "Found 0x%02x at address 0x%04x (%d), so ", *pos, 0x7837 + i, i);
                    number--;

                    uint16_t base = 0x7849 + i * 2 - 0x4000;
                    int newoffset = FileImage[base] + FileImage[base + 1] * 256 - 0x4000;
//                    fprintf(stderr, "start reading at 0x%04x\n", newoffset + 4000);
                    while (FileImage[newoffset] != 0xaa) {
                        instructions[number++] = FileImage[newoffset++];
//                        fprintf(stderr, "Instruction %d (at 0x%04x) is 0x%02x\n", number - 1, newoffset + 0x3fff, instructions[number - 1]);
                    }
                    break;
                }
            }
            pos++;
        } while (*pos != 0xfe);

        instructions[number++] = 0xfe;

        img->imagedata = MemAlloc(number);
        memcpy(img->imagedata, instructions, number);

        int patch = FindImagePatch(CurrentGame, picture_number, 0);
        while (patch) {
            Patch(FileImage, pos, patch);
            patch = FindImagePatch(CurrentGame, picture_number, patch);
        }
        pos++;
        img++;
    }

    taylor_image_data = &FileImage[Game->start_of_image_instructions];

    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    OpenGraphicsWindow();
    glk_window_clear(Graphics);
    for (int i = 3; i < 100; i++) {
        bzero(buffer, 384 * 9);
        fprintf(stderr, "Room image %d\n", i);
        MyLoc = i;
        draw_taylor(i);
        DrawSagaPictureFromBuffer();
        HitEnter();
    }
}

void PrintImageContents(int index, uint8_t *data, size_t size)
{
    fprintf(stderr, "/* image %d ", index);
    fprintf(stderr,
            "width: %d height: %d xoff: %d yoff: %d size: %zu bytes*/\n{ ",
            images[index].width, images[index].height, images[index].xoff,
            images[index].yoff, size);
    for (int i = 0; i < size; i++) {
        fprintf(stderr, "0x%02x, ", data[i]);
        if (i % 8 == 7)
            fprintf(stderr, "\n  ");
    }

    fprintf(stderr, " },\n");
    return;
}

void debugdrawcharacter(int character)
{
    fprintf(stderr, "Contents of character %d of 256:\n", character);
    for (int row = 0; row < 8; row++) {
        for (int n = 0; n < 8; n++) {
            if (isNthBitSet(sprite[character][row], n))
                fprintf(stderr, "■");
            else
                fprintf(stderr, "0");
        }
        fprintf(stderr, "\n");
    }
    if (character != 255)
        debugdrawcharacter(255);
}

void debugdraw(int on, int character, int xoff, int yoff, int width)
{
    if (on) {
        int x = character % width;
        int y = character / width;
        plotsprite(character, x + xoff, y + yoff, 0, 15);
        fprintf(stderr, "Contents of character position %d:\n", character);
        for (int row = 0; row < 8; row++) {
            for (int n = 0; n < 8; n++) {
                if (isNthBitSet(screenchars[character][row], n))
                    fprintf(stderr, "■");
                else
                    fprintf(stderr, "0");
            }
            fprintf(stderr, "\n");
        }
    }
}

#pragma mark Taylorimage

void mirror_left_half(void)
{
    for (int line = 0; line < 12; line++) {
        for (int col = 32; col > 16; col--) {
            buffer[line * 32 + col - 1][8] = buffer[line * 32 + (32 - col)][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + col - 1][pixrow] = buffer[line * 32 + (32 - col)][pixrow];
            Flip(buffer[line * 32 + col - 1]);
        }
    }
}

void mirror_area(int x1, int y1,int x2, int y2)
{
    fprintf(stderr, "mirror_area x1:%d y1:%d x2:%d y2:%d\n", x1, y1, x2, y2);
    int width = x2 - x1 - 5;
    for (int line = y1; line < y2; line++) {
        for (int col = width; col >= -2 ; col--) {
            buffer[line * 32 + x2 + width - col][8] = buffer[line * 32 + x1 + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + x2 + width - col][pixrow] = buffer[line * 32 + x1 + col][pixrow];
            Flip(buffer[line * 32 + x2 + width - col]);
        }
    }
}

void mirror_top_half(void)
{
    for (int line = 0; line < 6; line++) {
        for (int col = 0; col < 32; col++) {
            buffer[line * 32 + col][8] = buffer[(11 - line) * 32 + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + col][pixrow] = buffer[(11 - line) * 32 + col][7 - pixrow];
        }
    }
}

void replace_colour(uint8_t before, uint8_t after)
{

    // I don't think any of the data has bit 7 set,
    // so masking it is probably unnecessary, but this is what
    // the original code does.
    uint8_t beforeink = before & 7;
    uint8_t afterink = after & 7;
    uint8_t inkmask = 0x07;

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;
    uint8_t papermask = 0x38;

    for (int j = 0; j < 384; j++) {
        if ((buffer[j][8] & inkmask) == beforeink) {
            buffer[j][8] = (buffer[j][8] & ~inkmask) | afterink;
        }

        if ((buffer[j][8] & papermask) == beforepaper) {
            buffer[j][8] = (buffer[j][8] & ~papermask) | afterpaper;
        }
    }
}

void draw_colour( uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            buffer[(y + h) * 32 + x + w][8] = colour;
        }
    }
}

void make_light(void)
{
    for (int i = 0; i < 384; i++) {
        buffer[i][8] = buffer[i][8] | 0x40;
    }
}


//IX++;
//A = mem[IX];
//A = A & 0x47;
//IX++;
//DE = 0x47 + mem[IX] * 256;
//call77e4();
//BC = (BC & 0xff00) + mem[IX - 1];
//call77d6();
//uint8_t myPushedA = A;
//BC = (BC & 0xff00) + mem[IX];
//call77d6();
//DE = (DE & 0xff) + A * 256;
//A = myPushedA;
//DE = (DE & 0xff00) + 0x78;
//call77e4();
//IX++;

void call77e4(void) {
    BC = 0x180;
    pushedIX = IX;
    IX = 0x6e50; // Attributes buffer
    HL = (HL & 0xff00) + A;
jump77ee:
    A = mem[IX];
    A = A & (DE & 0xff);
    if (A != (HL & 0xff))
        goto jump77fe;
    A = mem[IX];
    A = A | (DE & 0xff);
    A = A ^ (DE & 0xff);
    A = A | (DE >> 8);
    mem[IX] = A;
jump77fe:
    IX++;
    BC--;
    A = BC >> 8;
    A = A | (BC & 0xff);
    if (A != 0)
        goto jump77ee;
    IX = pushedIX;
}

void call77d6(void) {
    A = (BC & 0xff) & 0x07;
    carry = rotate_left_with_carry(&A, 0);
    carry = rotate_left_with_carry(&A, carry);
    carry = rotate_left_with_carry(&A, carry);
    A = A & 0x78;
    BC = (BC & 0xff) + A * 256;
    A = ((BC & 0xff) & 0x40) | (BC >> 8);
}

void replace_attribute(uint8_t before, uint8_t after) {
    uint8_t A = before & 0x47;
    uint16_t DE = 0x47 + before * 256;
    for (int i = 0; i < 384; i++) {
        if (buffer[i][8] == before)
            buffer[i][8] = after;
    }
}


void flip_image_horizontally(void)
{
    uint8_t mirror[384][9];

    for (int line = 0; line < 12; line++) {
        for (int col = 32; col > 0; col--) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * 32 + col - 1][pixrow] = buffer[line * 32 + (32 - col)][pixrow];
            Flip(mirror[line * 32 + col - 1]);
        }
    }

    memcpy(buffer, mirror, 384 * 9);
}

void flip_image_vertically(void)
{
    uint8_t mirror[384][9];

    for (int line = 0; line < 12; line++) {
        for (int col = 0; col < 32; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[(11 - line) * 32 + col][7 - pixrow] = buffer[line * 32 + col][pixrow];
            mirror[(11 - line) * 32 + col][8] = buffer[line * 32 + col][8];
        }
    }
    memcpy(buffer, mirror, 384 * 9);
}

int should_draw_object_images;

void draw_object_image(uint8_t x, uint8_t y)
{
    for (int i = 0; i < NumLowObjects; i++) {
        if (Items[i].Flag != MyLoc)
            continue;
        if (ObjectLoc[i] != MyLoc)
            continue;
        DrawSagaPictureAtPos(Items[i].Image, x, y);
        should_draw_object_images = 0;
    }
}

void draw_taylor(int loc)
{
    uint8_t *ptr = taylor_image_data;
    for (int i = 0; i < loc; i++) {
        while (*(ptr) != 0xff)
            ptr++;
        ptr++;
    }
    int instruction = 1;
    while (ptr - FileImage < FileImageLen) {
        fprintf(stderr, "DrawTaylorRoomImage: Instruction %d: 0x%02x\n", instruction++, *ptr);
        switch (*ptr) {
            case 0xff:
                fprintf(stderr, "End of picture\n");
                return;
            case 0xfe: // 7470
                fprintf(stderr, "0xfe mirror_left_half\n");
                mirror_left_half();
                break;
            case 0xfd: // 7126
                fprintf(stderr, "0xfd Replace colour %x with %x\n", *(ptr + 1), *(ptr + 2));
                replace_colour(*(ptr + 1), *(ptr + 2));
                ptr += 2;
                break;
            case 0xfc: // Draw colour: x, y, attribute, length 7808
                fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d height %d width %d\n", *(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 3), *(ptr + 5));
                draw_colour(*(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 5), *(ptr + 3));
                ptr = ptr + 5;
                break;
            case 0xfb: // Make all screen colours bright 713e
                fprintf(stderr, "Make colours in picture area bright\n");
                make_light();
                break;
            case 0xfa: // Flip entire image horizontally 7646
                fprintf(stderr, "0xfa Flip entire image horizontally\n");
                flip_image_horizontally();
                break;
            case 0xf9: //0xf9 Draw picture n recursively;
                fprintf(stderr, "Draw Room Image %d recursively\n", *(ptr + 1));
                draw_taylor(*(ptr + 1));
                ptr++;
                break;
            case 0xf8: //73d1
                fprintf(stderr, "0xf8: Skip rest of picture if object %d is not present\n", *(ptr + 1));
                ptr++;
                fprintf(stderr, "Location of object %d: %d. MyLoc: %d\n", *ptr, ObjectLoc[*ptr], MyLoc);
                if (ObjectLoc[*ptr] != MyLoc) {
                    return;
//                    while (*ptr != 0xff && ptr - FileImage < FileImageLen)
//                        ptr++;
                }
                break;
            case 0xf7: //756e } set A to 0c and call 70b7
                break;
            case 0xf6: //7582 } set A to 04 and call 70b7
                break;
            case 0xf5: //7578 } set A to 08 and call 70b7
                break;
            case 0xf4: //758c End if object arg1 is present
                if (ObjectLoc[*(ptr + 1)] == MyLoc)
                    return;
                ptr++;
                break;
            case 0xf3: //753d
                fprintf(stderr, "0xf3: goto 753d Mirror top half vertically\n");
                mirror_top_half();
                break;
            case 0xf2: //7465 arg1 arg2 arg3 arg4
                fprintf(stderr, "0xf2: Mirror area x: %d y: %d x2:%d y2:%d horizontally?\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                mirror_area(*(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                ptr = ptr + 4;
                break;
            case 0xf1: //7532 arg1 arg2 arg3 arg4 Some kind of mirroring
                ptr = ptr + 4;
                break;
            case 0xee: //763b arg1 arg2 arg3 arg4  Some kind of mirroring
                ptr = ptr + 4;
                break;
            case 0xed: //7788
                fprintf(stderr, "0xed: Flip entire image vertically\n");
                flip_image_vertically();
                break;
            case 0xec: //777d Some kind of mirroring
                ptr = ptr + 4;
                break;
            case 0xe9: // 77ac arg1 arg2 ..
                fprintf(stderr, "0xe9: (77ac) swap attribute %d for colour %d?\n",  *(ptr + 1), *(ptr + 2));
                replace_attribute(*(ptr + 1), *(ptr + 2));
                ptr = ptr + 2;
                break;
            case 0xe8:
                fprintf(stderr, "Clear graphics memory\n");
                bzero(buffer, 384 * 9);
                break;
            default: // else draw image *ptr at x, y
                fprintf(stderr, "Default: Draw picture %d at %d,%d\n", *ptr, *(ptr + 1), *(ptr + 2));
                DrawSagaPictureAtPos(*ptr, *(ptr + 1), *(ptr + 2));
                ptr = ptr + 2;
                break;
        }
        DrawSagaPictureFromBuffer();
        HitEnter();
        ptr++;
    }
}

//void TaylorRoomImage(void)
//{
//    should_draw_object_images = 1;
//    draw_taylor(MyLoc);
//    for (int ct = 0; ct <= NumLowObjects; ct++)
//        if (Items[ct].Image && should_draw_object_images) {
//            if ((Items[ct].Flag & 127) == MyLoc &&
//                ObjectLoc[ct] == MyLoc) {
//                DrawSagaPictureNumber(Items[ct].Image);
//            }
//        }
//    DrawSagaPictureFromBuffer();
//}


uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                 int xoff, int yoff)
{
    int32_t offset = 0, cont = 0;
    int32_t i, x, y, mask_mode;
    uint8_t data, data2, old = 0;
    int32_t ink[0x22][14], paper[0x22][14];

    uint8_t *origptr = dataptr;
    int version = Game->picture_format_version;

    offset = 0;
    int32_t character = 0;
    int32_t count;
    do {
        count = 1;

        /* first handle mode */
        uint8_t data = *dataptr++;
        if (data < 0x80) {
            if (character > 127 && version > 2) {
                data += 128;
            }
            character = data;
#ifdef DRAWDEBUG
            fprintf(stderr, "******* SOLO CHARACTER: %04x\n", character);
#endif
            transform(character, 0, offset);
            offset++;
            if (offset > 767)
                break;
        } else {
            // first check for a count
            if ((data & 2) == 2) {
                count = (*dataptr++) + 1;
            }

            // Next get character and plot (count) times
            character = *dataptr++;

            // Plot the initial character
            if ((data & 1) == 1 && character < 128)
                character += 128;

            for (i = 0; i < count; i++)
                transform(character, (data & 0x0c) ? (data & 0xf3) : data, offset + i);

            // Now check for overlays
            if ((data & 0xc) != 0) {
                // We have overlays so grab each member of the stream and work out what
                // to do with it

                mask_mode = (data & 0xc);
                data2 = *dataptr++;
                old = data;
                do {
                    cont = 0;
                    if (data2 < 0x80) {
                        if (version == 4 && (old & 1) == 1)
                            data2 += 128;
#ifdef DRAWDEBUG
                        fprintf(stderr, "Plotting %d directly (overlay) at %d\n", data2,
                                offset);
#endif
                        for (i = 0; i < count; i++)
                            transform(data2, old & 0x0c, offset + i);
                    } else {
                        character = *dataptr++;
                        if ((data2 & 1) == 1)
                            character += 128;
#ifdef DRAWDEBUG
                        fprintf(stderr, "Plotting %d with flip %02x (%s) at %d %d\n",
                                character, (data2 | mask_mode),
                                flipdescription[((data2 | mask_mode) & 48) >> 4], offset,
                                count);
#endif
                        for (i = 0; i < count; i++)
                            transform(character, (data2 & 0xf3) | mask_mode, offset + i);
                        if ((data2 & 0x0c) != 0) {
                            mask_mode = (data2 & 0x0c);
                            old = data2;
                            cont = 1;
                            data2 = *dataptr++;
                        }
                    }
                } while (cont != 0);
            }
            offset += count;
        }
    } while (offset < xsize * ysize);

    y = 0;
    x = 0;

//       fprintf(stderr, "Attribute data begins at offset %ld\n", dataptr -
//       origptr);

    uint8_t colour = 0;
    // Note version 3 count is inverse it is repeat previous colour
    // Whilst version0-2 count is repeat next character
    while (y < ysize) {
        data = *dataptr++;
//        fprintf(stderr, "read attribute data byte %02x\n", data);
        if ((data & 0x80)) {
            count = (data & 0x7f) + 1;
            if (version >= 3) {
                count--;
            } else {
                colour = *dataptr++;
            }
        } else {
            count = 1;
            colour = data;
        }
        while (count > 0) {
            // Now split up depending on which version we're using

            // For version 3+

            if (draw_to_buffer)
                buffer[(yoff + y) * 32 + (xoff + x)][8] = colour;
            else {
                if (version > 2) {
                    if (x > 33)
                        return NULL;
                    // ink is 0-2, screen is 3-5, 6 in bright flag
                    ink[x][y] = colour & 0x07;
                    paper[x][y] = (colour & 0x38) >> 3;

                    if ((colour & 0x40) == 0x40) {
                        paper[x][y] += 8;
                        ink[x][y] += 8;
                    }
                } else {
                    if (x > 33)
                        return NULL;
                    paper[x][y] = colour & 0x07;
                    ink[x][y] = ((colour & 0x70) >> 4);

                    if ((colour & 0x08) == 0x08 || version < 2) {
                        paper[x][y] += 8;
                        ink[x][y] += 8;
                    }
                }
            }

            x++;
            if (x == xsize) {
                x = 0;
                y++;
            }
            count--;
        }
    }
    offset = 0;
    int32_t xoff2;
    for (y = 0; y < ysize; y++)
        for (x = 0; x < xsize; x++) {
            xoff2 = xoff;
            if (version > 0 && version < 3)
                xoff2 = xoff - 4;

            if (draw_to_buffer) {
                for (int i = 0; i < 8; i++)
                    buffer[(y + yoff) * 32 + x + xoff2][i] = screenchars[offset][i];
            } else {
                plotsprite(offset, x + xoff2, y + yoff, remap(ink[x][y]),
                           remap(paper[x][y]));
            }

#ifdef DRAWDEBUG
            fprintf(stderr, "(gfx#:plotting %d,%d:paper=%s,ink=%s)\n", x + xoff2,
                    y + yoff, colortext(remap(paper[x][y])),
                    colortext(remap(ink[x][y])));
#endif
            offset++;
            if (offset > 766)
                break;
        }
    return dataptr;
}

void DrawSagaPictureNumber(int picture_number)
{
    int numgraphics = Game->number_of_pictures;
    if (picture_number >= numgraphics) {
        fprintf(stderr, "Invalid image number %d! Last image:%d\n", picture_number,
                numgraphics - 1);
        return;
    }

    Image img = images[picture_number];

    if (img.imagedata == NULL)
        return;

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, img.xoff,
                            img.yoff);
}

void DrawSagaPictureAtPos(int picture_number, int x, int y)
{
    Image img = images[picture_number];

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y);
}

void SwitchPalettes(int pal1, int pal2)
{
    uint8_t temp[3];

    temp[0] = pal[pal1][0];
    temp[1] = pal[pal1][1];
    temp[2] = pal[pal1][2];

    pal[pal1][0] = pal[pal2][0];
    pal[pal1][1] = pal[pal2][1];
    pal[pal1][2] = pal[pal2][2];

    pal[pal2][0] = temp[0];
    pal[pal2][1] = temp[1];
    pal[pal2][2] = temp[2];
}

void DrawSagaPictureFromBuffer(void)
{
    for (int line = 0; line < 12; line++) {
        for (int col = 0; col < 32; col++) {

            uint8_t colour = buffer[col + line * 32][8];

            int paper = (colour >> 3) & 0x7;
            paper += 8 * ((colour & 0x40) == 0x40);
            paper = remap(paper);
            int ink = (colour & 0x7);
            ink += 8 * ((colour & 0x40) == 0x40);
            ink = remap(ink);

            background(col, line, paper);

            for (int i = 0; i < 8; i++) {
                if (buffer[col + line * 32][i] == 0)
                    continue;
                if (buffer[col + line * 32][i] == 255) {

                    glui32 glk_color = (glui32)((pal[ink][0] << 16) | (pal[ink][1] << 8) | pal[ink][2]);

                    glk_window_fill_rect(
                                         Graphics, glk_color, (glsi32)(col * 8 * pixel_size + x_offset),
                                         (glsi32)(line * 8 + i) * pixel_size, 8 * pixel_size, pixel_size);
                    continue;
                }
                for (int j = 0; j < 8; j++)
                    if ((buffer[col + line * 32][i] & (1 << j)) != 0) {
                        int ypos = line * 8 + i;
                        PutPixel(col * 8 + j, ypos, ink);
                    }
            }
        }
    }
}
