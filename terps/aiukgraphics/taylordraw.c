//
//  taylordraw.c
//  Spatterlight
//
//  Created by Administrator on 2025-12-10.
//

#include <stdio.h>
#include <string.h>

#include "taylor.h"
#include "taylordraw.h"

#include "irmak.h"

static void mirror_area(int x1, int y1, int width, int y2)
{
    for (int line = y1; line < y2; line++) {
        uint16_t source = line * IRMAK_IMGWIDTH + x1;
        uint16_t target = source + width - 1;
        for (int col = 0; col < width / 2; col++) {
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] = imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][pixrow] = imagebuffer[source][pixrow];
            Flip(imagebuffer[target]);
            source++;
            target--;
        }
    }
}

static void mirror_top_half(void)
{
    for (int line = 0; line < IRMAK_IMGHEIGHT / 2; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            uint16_t target = (11 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = line * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] =
            imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][7 - pixrow] =
                imagebuffer[source][pixrow];
        }
    }
}

static void flip_image_horizontally(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 32; col > 0; col--) {
            uint16_t target = line * IRMAK_IMGWIDTH + col - 1;
            uint16_t source = line * IRMAK_IMGWIDTH + (IRMAK_IMGWIDTH - col);
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[target][pixrow] = imagebuffer[source][pixrow];
            Flip(mirror[target]);
        }
    }

    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

static void flip_image_vertically(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            uint16_t target = (11 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = line * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[target][7 - pixrow] =
                imagebuffer[source][pixrow];
            mirror[target][8] =
            imagebuffer[source][8];
        }
    }
    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

static void flip_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    //    fprintf(stderr, "flip_area_vertically x1: %d: y1: %d width: %d y2 %d\n", x1, y1, width, y2);
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line <= y2 && line < IRMAK_IMGHEIGHT - y1; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t target = (y2 - line) * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE)
                return;
            uint16_t source = (y1 + line) * IRMAK_IMGWIDTH + col;
            if (source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[target][7 - pixrow] = imagebuffer[source][pixrow];
            mirror[target][8] = imagebuffer[source][8];
        }
    }
    for (int line = y1; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t index = line * IRMAK_IMGWIDTH + col;
            if (index >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[index][pixrow] =
                mirror[index][pixrow];
            imagebuffer[index][8] =
            mirror[index][8];
        }
    }
}

static void mirror_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    for (int line = 0; line <= y2 / 2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t target = (y2 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = (y1 + line) * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] =
            imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][7 - pixrow] =
                imagebuffer[source][pixrow];
        }
    }
}

static void flip_area_horizontally(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = y1; line < y2; line++) {
        for (int col = 0; col < width; col++) {
            uint16_t target = line * IRMAK_IMGWIDTH + x1 + col;
            uint16_t source = line * IRMAK_IMGWIDTH + x1 + width - col - 1;
            if (target > IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[target][pixrow] =
                imagebuffer[source][pixrow];
            Flip(mirror[target]);
        }
    }

    for (int line = y1; line < y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t index = line * IRMAK_IMGWIDTH + col;
            if (index > IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[index][pixrow] = mirror[index][pixrow];
            imagebuffer[index][8] = mirror[index][8];
        }
    }
}

static void draw_colour_old(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    uint16_t index = y * IRMAK_IMGWIDTH + x;
    if (index >= IRMAK_IMGSIZE)
        return;
    if (index + length >= IRMAK_IMGSIZE)
        length = IRMAK_IMGSIZE - index - 1;
    for (int i = 0; i < length; i++) {
        imagebuffer[index + i][8] = colour;
    }
}

static void draw_colour(uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            int index = (y + h) * IRMAK_IMGWIDTH + x + w;
            if (index >= IRMAK_IMGSIZE)
                return;
            imagebuffer[index][8] = colour;
        }
    }
}

static void make_light(void)
{
    for (int i = 0; i < IRMAK_IMGSIZE; i++) {
        imagebuffer[i][8] = imagebuffer[i][8] | BRIGHT_FLAG;
    }
}

static void replace_colour(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & INK_MASK;
    uint8_t afterink = after & INK_MASK;

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;

    for (int j = 0; j < IRMAK_IMGSIZE; j++) {
        if ((imagebuffer[j][8] & INK_MASK) == beforeink) {
            imagebuffer[j][8] = (imagebuffer[j][8] & ~INK_MASK) | afterink;
        }

        if ((imagebuffer[j][8] & PAPER_MASK) == beforepaper) {
            imagebuffer[j][8] = (imagebuffer[j][8] & ~PAPER_MASK) | afterpaper;
        }
    }
}

static uint8_t ink2paper(uint8_t ink)
{
    uint8_t paper = (ink & INK_MASK) << 3; // 0000 0111 mask ink
    paper = paper & PAPER_MASK; // 0011 1000 mask paper
    return (ink & BRIGHT_FLAG) | paper; // 0x40 = 0100 0000 preserve brightness bit from ink
}

static void replace(uint8_t before, uint8_t after, uint8_t mask)
{
    for (int j = 0; j < IRMAK_IMGSIZE; j++) {
        uint8_t col = imagebuffer[j][8] & mask;
        if (col == before) {
            uint8_t newcol = imagebuffer[j][8] | mask;
            newcol = newcol ^ mask;
            imagebuffer[j][8] = newcol | after;
        }
    }
}

static void replace_paper_and_ink(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & (INK_MASK | BRIGHT_FLAG); // 0100 0111 ink + brightness
    replace(beforeink, after, INK_MASK | BRIGHT_FLAG);
    uint8_t beforepaper = ink2paper(before);
    uint8_t afterpaper = ink2paper(after);
    replace(beforepaper, afterpaper, PAPER_MASK | BRIGHT_FLAG); // 0111 1000 paper + brightness
}

uint8_t *TaylorImageData;
uint8_t *EndOfTaylorDrawData;
uint8_t *ObjectLocations;

int UseOlderVersion = 0;
int IsRebelplanet = 0;

draw_obj_fn draw_seas_obj = NULL;

void InitTaylor(uint8_t *data, uint8_t *end, uint8_t *objloc, int older, int rebel, draw_obj_fn obj_draw) {
    TaylorImageData = data;
    EndOfTaylorDrawData = end;
    ObjectLocations = objloc;
    UseOlderVersion = older;
    IsRebelplanet = rebel;
    draw_seas_obj = obj_draw;
}

/* 99 times out of a 100, loc is equal to current_location,
   but there are some edge cases we need to look out for when
   this function calls itself with a different loc parameter.
   See case 0xf9 below. */
int DrawTaylor(int loc, int current_location)
{
    uint8_t *ptr = TaylorImageData;
    for (int i = 0; i < loc; i++) {
        while (ptr < EndOfTaylorDrawData && *ptr != 0xff)
            ptr++;
        ptr++;
    }

    if (ptr >= EndOfTaylorDrawData)
        return 0;

    while (ptr < EndOfTaylorDrawData) {
        // fprintf(stderr, "DrawTaylorRoomImage: Instruction %d: 0x%02x\n", instruction++, *ptr);
        switch (*ptr) {
        case 0xff:
            // fprintf(stderr, "End of picture\n");
            return 1;
        case 0xfe:
            // fprintf(stderr, "0xfe mirror_left_half\n");
            mirror_area(0, 0, IRMAK_IMGWIDTH, IRMAK_IMGHEIGHT);
            break;
        case 0xfd:
            // fprintf(stderr, "0xfd Replace colour %x with %x\n", ptr[1], ptr[2]);
            replace_colour(ptr[1], ptr[2]);
            ptr += 2;
            break;
        case 0xfc: // Draw colour: x, y, attribute, length 7808
            if (UseOlderVersion) {
                // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d length %d\n", ptr[3], ptr[1], ptr[2], ptr[4]);
                draw_colour_old(ptr[1], ptr[2], ptr[3], ptr[4]);
                ptr = ptr + 4;
            } else {
                // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d height %d width %d\n", ptr[4], ptr[2], ptr[1], ptr[3], ptr[5]);
                draw_colour(ptr[4], ptr[2], ptr[1], ptr[5], ptr[3]);
                ptr = ptr + 5;
            }
            break;
        case 0xfb: // Make all screen colours bright 713e
            // fprintf(stderr, "Make colours in picture area bright\n");
            make_light();
            break;
        case 0xfa: // Flip entire image horizontally 7646
            // fprintf(stderr, "0xfa Flip entire image horizontally\n");
            flip_image_horizontally();
            break;
        case 0xf9: //0xf9 Draw picture n recursively;
                // For Seas of Blood, we just call the provided
                // draw_seas_obj function pointer (draw_seas_obj(x,y))
                if (draw_seas_obj != NULL) {
                    draw_seas_obj(ptr[1],ptr[2]);
                    ptr += 2;
                } else {
//                    fprintf(stderr, "DrawTaylor room %d: Draw Room Image %d recursively\n", loc, ptr[1]);
                    DrawTaylor(ptr[1], current_location);
                    ptr++;
                }
            break;
        case 0xf8:
//            fprintf(stderr, "DrawTaylor room %d: 0xf8: Skip rest of picture if object %d is not present\n", loc, ptr[1]);
            if (UseOlderVersion) {
                // Blizzard Pass and Rebel Planet: If object arg1 is present, draw image arg2 at arg3, arg4
                if (ObjectLocations[ptr[1]] == current_location) {
                    DrawPictureAtPos(ptr[2], ptr[3], ptr[4], 1);
                }
                ptr += 4;
            } else {
                // He-Man and later: Stop drawing if object arg1 is not present
                if (ObjectLocations[ptr[1]] != current_location) {
                    return 1;
                }
                ptr++;
            }
            break;
        case 0xf4: // End if object arg1 is present
//            fprintf(stderr, "DrawTaylor room %d: 0xf4: Skip rest of picture if object %d IS present\n", loc, ptr[1]);
            if (ObjectLocations[ptr[1]] == current_location)
                return 1;
            ptr++;
            break;
        case 0xf3:
            // fprintf(stderr, "0xf3: goto 753d Mirror top half vertically\n");
            mirror_top_half();
            break;
        case 0xf2: // arg1 arg2 arg3 arg4 Mirror horizontally
            // fprintf(stderr, "0xf2: Mirror area x: %d y: %d width:%d y2:%d horizontally\n", ptr[2], ptr[1], ptr[4],  ptr[3]);
            mirror_area(ptr[2], ptr[1], ptr[4], ptr[3]);
            ptr = ptr + 4;
            break;
        case 0xf1: // arg1 arg2 arg3 arg4 Mirror vertically
            // fprintf(stderr, "0xf1: Mirror area x: %d y: %d width:%d y2:%d vertically\n", ptr[2], ptr[1], ptr[4],  ptr[3]);
            mirror_area_vertically(ptr[1], ptr[2], ptr[4], ptr[3]);
            ptr = ptr + 4;
            break;
        case 0xee: // arg1 arg2 arg3 arg4  Flip area horizontally
            // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d horizontally\n", ptr[2], ptr[1], ptr[4],  ptr[3]);
            flip_area_horizontally(ptr[2], ptr[1], ptr[4], ptr[3]);
            ptr = ptr + 4;
            break;
        case 0xed:
            // fprintf(stderr, "0xed: Flip entire image vertically\n");
            flip_image_vertically();
            break;
        case 0xec: // Flip area vertically
            // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d vertically\n", ptr[2], ptr[1], ptr[4],  ptr[3]);
            flip_area_vertically(ptr[1], ptr[2], ptr[4], ptr[3]);
            ptr = ptr + 4;
            break;
        case 0xeb:
        case 0xea:
                fprintf(stderr, "Unimplemented draw instruction 0x%02x!\n", *ptr);
                return 0;
        case 0xe9:
            // fprintf(stderr, "0xe9: (77ac) replace paper and ink %d for colour %d\n",  ptr[1], ptr[2]);
            replace_paper_and_ink(ptr[1], ptr[2]);
            ptr = ptr + 2;
            break;
        case 0xe8:
            // fprintf(stderr, "Clear graphics memory\n");
            ClearGraphMem();
            break;
        case 0xf7: // set A to 0c and call draw image, but A seems to not be used. Vestigial code?

//                fprintf(stderr, "Draw instruction 0xf7: arg1:%d arg2:%d arg3:%d arg4:%d arg5:%d arg6:%d arg7:%d arg8:%d\n", ptr[1], ptr[2], ptr[3], ptr[4],  ptr[5],  ptr[6], ptr[7], ptr[8]);

          /* In the basement of the Zodiac Hotel in Rebel Planet is a locked alcove.
             There is an image of the bottles inside, but in the original interpreter
             it is never drawn.

             The image draw instructions for room 43 (the basement) call opcode 0xf7 with the
             bottles image index (85) as first argument (and then x and y coordinates, and then
             0xf7 and 85 again, drawing two copies of the image next to each other), but the
             original code returns without drawing anything.

             Below is a hack which checks if the phonic fork (object 131) is out of play
             (i.e. in dummy room 252), which only seems to be the case if the alcove is locked.
             If not, it falls through to the DrawSagaPictureAtPos() call, which will draw the
             bottles.

             This could potentially cause problems, if not for the fact that opcode 0xf7 is
             not used by any other image or any other TaylorMade game.
             (No game seems to use opcodes 0xf6 or 0xf5 either, for that matter.) */
            if (IsRebelplanet && current_location == 43 && ObjectLocations[131] == 252)
                return 1;
        case 0xf6: // set A to 04 and call draw image. See 0xf7 above.
        case 0xf5: // set A to 08 and call draw image. See 0xf7 above.
            // fprintf(stderr, "0x%02x: set A to unused value and draw image block %d at %d, %d\n",  *ptr, ptr[1], ptr[2], ptr[3]);
            ptr++; // Deliberate fallthrough
        default: // else draw image *ptr at x, y
            // fprintf(stderr, "Default: Draw image block %d at %d,%d\n", *ptr, ptr[1], ptr[2]);
            DrawPictureAtPos(ptr[0], ptr[1], ptr[2], 1);
            ptr = ptr + 2;
            break;
        }
        ptr++;
    }
    return 0;
}
