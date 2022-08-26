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

int upside_down = 0;

struct imgrec *Images;

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, y_offset, x_offset, 0, ImageWidth * pixel_size,
                         ImageHeight * pixel_size);
    LastImgType = NO_IMG;
}

char *ShortNameFromType(char type, int index) {
    char buf[5];
    int n = sprintf(buf, "%c0%02d", type, index);
    if (n < 0)
        return NULL;
    size_t len = strlen(buf) + 1;
    char *result = MemAlloc(len);
    memcpy(result, buf, len);
    return result;
}

int DrawDOSImageFromData(uint8_t *ptr, size_t datasize);
int DrawAtariC64ImageFromData(uint8_t *ptr, size_t datasize);
int DrawSTImageFromData(uint8_t *ptr, size_t datasize);
int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize);
void DrawApple2ImageFromVideoMem(void);
void ClearApple2ScreenMem(void);

size_t writeToFile(const char *name, uint8_t *data, size_t size);

int DrawImageWithName(char *filename)
{
    debug_print("DrawImageWithName %s\n", filename);

    int i;
    for (i = 0; Images[i].Filename != NULL; i++ )
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

    if (CurrentSys == SYS_C64 || CurrentSys == SYS_ATARI8) {
        return DrawAtariC64ImageFromData(Images[i].Data, Images[i].Size);
    } else if (CurrentSys == SYS_ST) {
        return DrawSTImageFromData(Images[i].Data, Images[i].Size);
    } else if (CurrentSys == SYS_APPLE2) {
        return DrawApple2ImageFromData(Images[i].Data, Images[i].Size);
    } else
        return DrawDOSImageFromData(Images[i].Data, Images[i].Size);
}

void DrawItemImage(int item) {
    LastImgType = IMG_OBJECT;
    LastImgIndex = item;
    char buf[1024];

        int n = sprintf( buf, "B0%02d", item);
        if (n < 0)
            return;

    upside_down = (CurrentGame == SPIDERMAN && Items[0].Location == MyLoc);

    DrawImageWithName(buf);
}

int DrawCloseup(int img) {
    LastImgType = IMG_SPECIAL;
    LastImgIndex = img;
    char buf[1024];

    int n = sprintf( buf, "S0%02d", img);
    if (n < 0)
        return 0;

    upside_down = 0;

    int result = DrawImageWithName(buf);

    if (result && CurrentSys == SYS_APPLE2) {
        ClearApple2ScreenMem();
        glk_window_clear(Graphics);
        DrawApple2ImageFromVideoMem();
    }

    return result;
}

void ClearApple2ScreenMem(void);

int DrawRoomImage(int roomimg) {
    LastImgType = IMG_ROOM;
    LastImgIndex = roomimg;

    char buf[5];
    int n = sprintf( buf, "R0%02d", roomimg);
    if (n < 0)
        return 0;
    buf[4] = 0;

    if (Graphics)
        glk_window_clear(Graphics);

    if (CurrentSys == SYS_ST)
        DrawImageWithName("S999");
    else if (CurrentSys == SYS_APPLE2)
        ClearApple2ScreenMem();

    upside_down = (CurrentGame == SPIDERMAN && Items[0].Location == MyLoc);

    return DrawImageWithName(buf);
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

    if (Graphics == NULL || AnimationRoom == MyLoc)
        return;

    if (!DrawRoomImage(Rooms[MyLoc].Image)) {
        CloseGraphicsWindow();
        return;
    }

    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Room == MyLoc && Items[ObjectImages[ct].Object].Location == MyLoc ) {
            DrawItemImage(ObjectImages[ct].Image);
        }

    if (CurrentSys == SYS_APPLE2) {
        DrawApple2ImageFromVideoMem();
    }
}
