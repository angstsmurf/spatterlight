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
#include <stdlib.h>
#include <string.h>

#include "graphics.h"
#include "image_probe.h"
#include "irmak.h"
#include "player.h"
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

/* TAYLOR_PROBE_STATE names a folder of room<N>.state files, each the flags
   and object locations of an original game that was put in room N and took a
   turn there (see test/images/mame_capture.py). The room is then drawn from
   that state, as Look() would: the turn may have brought something into the
   room, or taken the player out of it. Returns 0 if the interpreter would
   leave the screen as it was. The original keeps 67 flags, 69 in Questprobe
   3, and the object locations after them. */
#define ORIGINAL_FLAGS (Version == QUESTPROBE3_TYPE ? 69 : 67)
#define MAX_ORIGINAL_FLAGS 69
#define OBJECT_LOCATIONS 256

static int LoadRoomState(const char *folder, int room)
{
    char path[1024];
    uint8_t state[MAX_ORIGINAL_FLAGS + OBJECT_LOCATIONS];
    size_t size = ORIGINAL_FLAGS + OBJECT_LOCATIONS;
    snprintf(path, sizeof path, "%s/room%d.state", folder, room);
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    size_t got = fread(state, 1, size, f);
    fclose(f);
    if (got != size)
        return 0;
    memcpy(Flag, state, ORIGINAL_FLAGS);
    memcpy(ObjectLoc, state + ORIGINAL_FLAGS, OBJECT_LOCATIONS);
    return 1;
}

static void DumpRoomsFromState(const char *folder, int rooms)
{
    for (int room = 0; room < rooms; room++) {
        char name[16];
        snprintf(name, sizeof name, "room%d", room);
        if (!LoadRoomState(folder, room))
            continue;
        ImageProbeClear();
        ClearGraphMem();
        if (Version == QUESTPROBE3_TYPE) {
            /* The pictures are drawn by the game's own status table. */
            Look();
            ImageProbeReport(name, NULL);
            continue;
        }
        if (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91)) {
            ImageProbeNote("%-24s not drawn in room %d", name, MyLoc);
            continue;
        }
        /* A dark room is painted black. */
        if (!Flag[1] && !DrawTaylor(MyLoc, MyLoc)) {
            ImageProbeNote("%-24s no picture", name);
            continue;
        }
        DrawIrmakPictureFromBuffer();
        ImageProbeReport(name, NULL);
    }
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
       instructions: its room pictures are the pictures above, drawn by the
       game's actions, so its rooms are only drawn from a captured state. */
    const char *state = getenv("TAYLOR_PROBE_STATE");
    if (!pieces && state == NULL)
        return;
    int rooms = NumRooms();
    ImageProbeNote("rooms    %d", rooms);
    if (state != NULL) {
        DumpRoomsFromState(state, rooms);
        return;
    }
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
