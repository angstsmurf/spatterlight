//
//  image_probe.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  The TaylorMade half of the picture regression tests. The window layer in
//  ../../common_imagetest runs the real glk_main() up to the first command
//  prompt and then calls ImageProbeDump(), which draws every picture of the
//  game and every room, through the same calls the interpreter uses.
//

#include <stdio.h>

#include "graphics.h"
#include "image_probe.h"
#include "irmak.h"
#include "taylor.h"
#include "taylordraw.h"

void ImageProbeGraphicsSize(int *width, int *height)
{
    *width = IRMAK_IMGWIDTH * 8;
    *height = IRMAK_IMGHEIGHT * 8;
}

/* The exit table has a block for every room, headed by the room number with
   the high bit set, and ends with 0xfe. The draw instruction streams have no
   count of their own, and those past the last room are not pictures. */
static int NumRooms(void)
{
    uint8_t *p = FileImage + Game->start_of_room_connections + FileBaselineOffset;
    int highest = 0;
    for (; p < EndOfData && *p != 0xfe; p++) {
        if (*p & 0x80) {
            if ((*p & 0x7f) > highest)
                highest = *p & 0x7f;
        } else {
            p++;
        }
    }
    return highest + 1;
}

void ImageProbeDump(void)
{
    ImageProbeNote("game     %s", Game->Title);
    ImageProbeNote("pictures %d", NoGraphics ? 0 : Game->number_of_pictures);
    if (NoGraphics || images == NULL)
        return;

    if (Graphics == NULL)
        OpenGraphicsWindow();

    /* The pictures, each alone on an empty screen. Questprobe 3 has whole
       pictures that know where they go. In the other games they are pieces
       that the Taylor draw instructions place, so those are drawn in the top
       left corner. */
    int pieces = Version != QUESTPROBE3_TYPE;
    for (int i = 0; i < Game->number_of_pictures; i++) {
        char name[16], detail[48];
        snprintf(name, sizeof name, "pic%d", i);
        if (images[i].imagedata == NULL) {
            ImageProbeNote("%-24s no data", name);
            continue;
        }
        snprintf(detail, sizeof detail, "%4zu %08x", images[i].datasize,
            ImageProbeCRC(images[i].imagedata, images[i].datasize));
        ImageProbeClear();
        ClearGraphMem();
        if (pieces)
            DrawPictureAtPos(i, 0, 0, 1);
        else
            DrawPictureNumber(i, 1);
        DrawIrmakPictureFromBuffer();
        ImageProbeReport(name, detail);
    }

    /* The rooms, composed from the pieces by the Taylor draw instructions,
       with the objects that start there. Questprobe 3 has no such
       instructions: its room pictures are the pictures above. */
    if (!pieces)
        return;
    int rooms = NumRooms();
    ImageProbeNote("rooms    %d", rooms);
    for (int room = 0; room < rooms; room++) {
        char name[16];
        snprintf(name, sizeof name, "room%d", room);
        ImageProbeClear();
        ClearGraphMem();
        if (!DrawTaylor(room, room)) {
            ImageProbeNote("%-24s no picture", name);
            continue;
        }
        DrawIrmakPictureFromBuffer();
        ImageProbeReport(name, NULL);
    }
}
