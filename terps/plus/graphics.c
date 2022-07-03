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

ImgType LastImgType;
int LastImgIndex;

static void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, y_offset, x_offset, 0, 280 * pixel_size,
                         158 * pixel_size);
}

int DrawImageFromFile(char *filename);

int DrawImageWithFilename(char *filename)
{
    if (!gli_enable_graphics)
        return 0;
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        fprintf(stderr, "DrawImage: Graphic window NULL?\n");
        return 0;
    }

    return DrawImageFromFile(filename);
}

void DrawItemImage(int item) {
    LastImgType = IMG_OBJECT;
    LastImgIndex = item;
    char buf[1024];
    int n = sprintf( buf, "%sB0%02d.PAK", dir_path, item);
    if (n < 0)
        return;
    debug_print("%s %d\n", buf, n);
    DrawImageWithFilename(buf);
}

int DrawCloseup(int img) {
    LastImgType = IMG_SPECIAL;
    LastImgIndex = img;
    char buf[1024];
    int n = sprintf(buf, "%sS0%02d.PAK", dir_path, img);
    if (n < 0)
        return 0;
    debug_print("%s %d\n", buf, n);
    return DrawImageWithFilename(buf);
}

void DrawRoomImage(int room) {
    LastImgType = IMG_ROOM;
    LastImgIndex = room;
    char buf[1024];
    sprintf(buf, "%sR0%02d.PAK", dir_path, Rooms[room].Image);
    glk_window_clear(Graphics);
    DrawImageWithFilename(buf);
}

void DrawCurrentRoom(void)
{
    int dark = IsSet(DARKBIT);
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Flag & 2) {
            if (Items[i].Location == MyLoc || Items[i].Location == CARRIED)
                dark = 0;
        }
    }

    if (dark && Graphics != NULL) {
        if(CurrentGame != CLAYMORGUE)
            DrawBlack();
        return;
    }

    if (Rooms[MyLoc].Image == 255) {
        CloseGraphicsWindow();
        return;
    }

    if (dark || Graphics == NULL)
        return;

    DrawRoomImage(MyLoc);

    if (CurrentGame == CLAYMORGUE && MyLoc == 33)
        showing_inventory = 1;
    else
        showing_inventory = 0;

    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].room == MyLoc && Items[ObjectImages[ct].object].Location == MyLoc ) {
            DrawItemImage(ObjectImages[ct].image);
        }
}
