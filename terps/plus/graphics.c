//
//  graphics.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "animations.h"
#include "apple2draw.h"
#include "common.h"
#include "graphics.h"
#include "loaddatabase.h"
#include "read_le16.h"
#include "c64a8draw.h"
#include "pcdraw.h"
#include "sagadraw_glue.h"

ImgType LastImgType;
int LastImgIndex;

int upside_down = 0;

int pixel_size;
int x_offset, y_offset, right_margin;

glui32 pal[16];

imgrec *Images;

int DrawSTImageFromData(uint8_t *ptr, size_t datasize);

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, y_offset, (glui32)(ImageWidth * pixel_size),
        (glui32)(ImageHeight * pixel_size));
    LastImgType = NO_IMG;
}

/* Write the short image name for an image type ('R' = room,
   'B' = item/object, 'S' = special) and number, e.g. "R003", to buf,
   which must hold IMAGE_NAME_SIZE bytes. Returns 0 on failure. */
static int ImageNameFromType(char *buf, char type, int index)
{
    return snprintf(buf, IMAGE_NAME_SIZE, "%c0%02d", type, index) >= 0;
}

char *ShortNameFromType(char type, int index)
{
    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, type, index))
        return NULL;
    size_t len = strlen(buf) + 1;
    char *result = MemAlloc(len);
    memcpy(result, buf, len);
    return result;
}

/* The image number in a short image name, e.g. 3 for "R003". */
int ImageNumberFromName(const char *name)
{
    return (name[2] - '0') * 10 + name[3] - '0';
}

/* Used by IBM PC graphics in "striped" mode where every other pixel is skipped
   to make halftones */
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    PutPixelWithWidth(xpos, ypos, color, 1);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color) {
    PutPixelWithWidth(xpos, ypos, color, 2);
}


void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixelwidth)
{
    xpos = xpos * pixel_size;

    if (upside_down) {
        xpos = ImageWidth * pixel_size - xpos - 1;
    }
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    ypos = ypos * pixel_size;
    if (upside_down) {
        ypos = (ImageHeight - 1) * pixel_size - ypos;
        if (CurrentSys == SYS_ST)
            ypos -= 2 * pixel_size;
        else if (CurrentSys == SYS_ATARI8)
            ypos -= ImageHeight & 1;
    }
    ypos += y_offset;

    glk_window_fill_rect(Graphics, pal[color], xpos,
        ypos, pixel_size * pixelwidth, pixel_size);
}

void SetColor(int32_t index, glui32 color)
{
    pal[index] = color;
}

int DrawImageWithName(char *filename)
{
    debug_print("DrawImageWithName %s\n", filename);

    int i;
    for (i = 0; Images[i].Filename != NULL; i++)
        if (strcmp(filename, Images[i].Filename) == 0)
            break;

    if (Images[i].Filename == NULL) {
        if (CurrentSys == SYS_MSDOS) {
            if (FindAndAddImageFile(filename, &Images[i])) {
                Images[i + 1].Filename = NULL;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }

    if (!IsSet(GRAPHICSBIT))
        return 0;

    switch (Images[i].Filename[0]) {
    case 'B':
        LastImgType = IMG_OBJECT;
        break;
    case 'R':
        LastImgType = IMG_ROOM;
        break;
    case 'S':
        LastImgType = IMG_SPECIAL;
        break;
    default:
        debug_print("DrawImageWithName: Unknown image type!\n");
    }

    LastImgIndex = ImageNumberFromName(Images[i].Filename);

    if (CurrentSys == SYS_C64 || CurrentSys == SYS_ATARI8) {
        return DrawC64A8ImageFromData(Images[i].Data, Images[i].Size, 0, C64A8AdjustPlus, CurrentSys == SYS_C64);
    } else if (CurrentSys == SYS_ST) {
        return DrawSTImageFromData(Images[i].Data, Images[i].Size);
    } else if (CurrentSys == SYS_APPLE2) {
        return DrawApple2ImageFromData(Images[i].Data + 2, READ_LE_UINT16(Images[i].Data), 0, Apple2AdjustPlus);
    } else
        return DrawDOSImageFromData(Images[i].Data);
}

int DrawImageWithTypeAndNumber(char type, int index)
{
    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, type, index))
        return 0;
    return DrawImageWithName(buf);
}

void DrawItemImage(int item)
{
    LastImgType = IMG_OBJECT;
    LastImgIndex = item;
    char buf[IMAGE_NAME_SIZE];

    if (!ImageNameFromType(buf, 'B', item))
        return;

    upside_down = (CurrentGame == SPIDERMAN && Items[0].Location == MyLoc);

    DrawImageWithName(buf);
}

int DrawCloseup(int img)
{
    LastImgType = IMG_SPECIAL;
    LastImgIndex = img;
    char buf[IMAGE_NAME_SIZE];

    if (!ImageNameFromType(buf, 'S', img))
        return 0;

    upside_down = 0;

    if (CurrentSys == SYS_APPLE2)
        ClearApple2ScreenMem();

    int result = DrawImageWithName(buf);

    if (result && CurrentSys == SYS_APPLE2) {
        glk_window_clear(Graphics);
        DrawApple2ImageFromVideoMem();
    }

    return result;
}

int DrawRoomImage(int roomimg)
{
    LastImgType = IMG_ROOM;
    LastImgIndex = roomimg;

    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, 'R', roomimg))
        return 0;

    if (Graphics)
        glk_window_clear(Graphics);

    if (CurrentSys == SYS_ST && CurrentGame != CLAYMORGUE)
        DrawImageWithName("S999");
    else if (CurrentSys == SYS_APPLE2)
        ClearApple2ScreenMem();

    upside_down = (CurrentGame == SPIDERMAN && Items[0].Location == MyLoc);

    return DrawImageWithName(buf);
}

/* Redraw an item or closeup image that was showing on top of the room
   picture, given the type and index it was drawn with. Room images are
   left alone. */
void DrawOverlayImage(ImgType type, int index)
{
    if (type == IMG_SPECIAL)
        DrawCloseup(index);
    else if (type == IMG_OBJECT)
        DrawItemImage(index);
}

/* Draw the item images that ObjectImages assigns to room, for every
   such item at location. */
void DrawObjectImages(int room, int location)
{
    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Room == room && Items[ObjectImages[ct].Object].Location == location) {
            DrawItemImage(ObjectImages[ct].Image);
        }
}

extern int AnimationRoom;

void DrawCurrentRoom(void)
{
    OpenGraphicsWindow();

    showing_inventory = (CurrentGame == CLAYMORGUE && MyLoc == 33);

    if (!IsSet(GRAPHICSBIT)) {
        CloseGraphicsWindow();
        return;
    }

    int dark = IsDark();

    if (Rooms[MyLoc].Image == 255 || Images[0].Filename == NULL) {
        CloseGraphicsWindow();
        return;
    }

    if (dark && Graphics != NULL) {
        if (CurrentGame != CLAYMORGUE) {
            if (CurrentSys == SYS_ST && !AnimationRunning)
                DrawImageWithName("S999");
            else
                DrawBlack();
        } else {
            DrawImageWithName("R000");
        }
        return;
    }

    if (Graphics == NULL || AnimationRoom == MyLoc)
        return;

    if (!DrawRoomImage(Rooms[MyLoc].Image)) {
        CloseGraphicsWindow();
        return;
    }

    DrawObjectImages(MyLoc, MyLoc);

    if (CurrentSys == SYS_APPLE2) {
        DrawApple2ImageFromVideoMemWithFlip(upside_down);
    }
}
