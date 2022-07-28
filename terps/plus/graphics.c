//
//  graphics.c
//  Plus
//
//  Created by Administrator on 2022-06-04.
//

#include "glk.h"
#include "glkimp.h"

#include "common.h"
#include "graphics.h"
#include "loaddatabase.h"

ImgType LastImgType;
int LastImgIndex;

struct imgrec *Images;

static void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, y_offset, x_offset, 0, 280 * pixel_size,
                         158 * pixel_size);
    LastImgType = NO_IMG;
}

char *ShortNameFromType(char type, int index) {
    char buf[1024];
    int n = sprintf(buf, "%c0%02d", type, index);
    if (n < 0)
        return NULL;
    size_t len = strlen(buf) + 1;
    char *result = MemAlloc(len);
    memcpy(result, buf, len);
    return result;
}

int DrawC64ImageFromData(uint8_t *ptr, size_t datasize);
int DrawDOSImageFromData(uint8_t *ptr, size_t datasize);

int DrawImageWithName(char *filename)
{
    debug_print("DrawImageWithName %s\n", filename);

    int i;
    for (i = 0; Images[i].filename != NULL; i++ )
        if (strcmp(filename, Images[i].filename) == 0)
            break;

    if (Images[i].filename == NULL) {
        if (CurrentSys == SYS_MSDOS) {
            if (FindAndAddImageFile(filename, &Images[i])) {
                Images[i + 1].filename = NULL;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }

    if (!gli_enable_graphics)
        return 0;

    if (CurrentSys == SYS_C64)
        return DrawC64ImageFromData(Images[i].data, Images[i].size);
    else
        return DrawDOSImageFromData(Images[i].data, Images[i].size);
}

void DrawItemImage(int item) {
    LastImgType = IMG_OBJECT;
    LastImgIndex = item;
    char buf[1024];

        int n = sprintf( buf, "B0%02d", item);
        if (n < 0)
            return;

    DrawImageWithName(buf);
}

int DrawCloseup(int img) {
    LastImgType = IMG_SPECIAL;
    LastImgIndex = img;
    char buf[1024];

    int n = sprintf( buf, "S0%02d", img);
    if (n < 0)
        return 0;

    return DrawImageWithName(buf);
}

int DrawRoomImage(int roomimg) {
    LastImgType = IMG_ROOM;
    LastImgIndex = roomimg;

    char buf[5];
    int n = sprintf( buf, "R0%02d", roomimg);
    if (n < 0)
        return 0;

    if (Graphics)
        glk_window_clear(Graphics);

    return DrawImageWithName(buf);
}

void DrawCurrentRoom(void)
{
    if (CurrentGame == CLAYMORGUE && MyLoc == 33)
        showing_inventory = 1;
    else
        showing_inventory = 0;

    if (!gli_enable_graphics) {
        CloseGraphicsWindow();
        return;
    }


    int dark = IsSet(DARKBIT);
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Flag & 2) {
            if (Items[i].Location == MyLoc || Items[i].Location == CARRIED)
                dark = 0;
        }
    }


    if (Rooms[MyLoc].Image == 255) {
        CloseGraphicsWindow();
        return;
    }

    if (dark && Graphics != NULL) {
        if(CurrentGame != CLAYMORGUE) {
            DrawBlack();
        }
        return;
    }

    if (Graphics == NULL)
        return;

    if (!DrawRoomImage(Rooms[MyLoc].Image)) {
        CloseGraphicsWindow();
        return;
    }

    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].room == MyLoc && Items[ObjectImages[ct].object].Location == MyLoc ) {
            DrawItemImage(ObjectImages[ct].image);
        }
}
