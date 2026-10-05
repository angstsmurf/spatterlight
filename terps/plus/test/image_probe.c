/* Picture fingerprint for the image regression tests (test/images/README.md).

   Built into plus_image_probe together with the whole interpreter and the fake
   window layer in ../common_imagetest/image_glk.c. That layer runs the real
   glk_main() on a game file, reports the title picture when the game waits on
   it, and calls ImageProbeDump() below when the game first asks for a command.

   Two sets of lines come out:

     R012 ...   every picture the loader found, drawn by itself on an empty
                window: name, size and CRC32 of the packed data, then what it
                painted
     room 12    every room that has a picture, drawn by DrawCurrentRoom() as the
                game would on walking in at the start of the game: the room
                picture with the pictures of the objects that start there on
                top, upside down where the game says so

   The first set says which picture changed and whether it was the loader (data
   CRC) or the renderer (pixel CRC); the second covers the code that puts
   pictures together. */

#include <stdio.h>
#include <string.h>

#include "animations.h"
#include "apple2draw.h"
#include "common.h"
#include "graphics.h"
#include "image_probe.h"
#include "parseinput.h"

extern int ColorCyclingRunning;

void ImageProbeGraphicsSize(int *width, int *height)
{
    *width = ImageWidth;
    *height = ImageHeight;
}

static const char *SystemName(void)
{
    switch (CurrentSys) {
    case SYS_MSDOS:  return "MS-DOS";
    case SYS_C64:    return "C64";
    case SYS_ATARI8: return "Atari 8-bit";
    case SYS_APPLE2: return "Apple II";
    case SYS_ST:     return "Atari ST";
    default:         return "unknown";
    }
}

static const char *GameName(void)
{
    switch (CurrentGame) {
    case BANZAI:     return "Buckaroo Banzai";
    case CLAYMORGUE: return "Sorcerer of Claymorgue Castle";
    case SPIDERMAN:  return "Spider-Man";
    case FANTASTIC4: return "Fantastic Four";
    case XMEN:       return "X-Men";
    default:         return "unknown";
    }
}

void ImageProbeDump(void)
{
    /* The MS-DOS pictures are files next to the game, looked up by name when
       first drawn. Ask for every name there can be. */
    if (CurrentSys == SYS_MSDOS) {
        for (const char *type = "RBS"; *type; type++)
            for (int i = 0; i < 100; i++) {
                char name[5];
                snprintf(name, sizeof name, "%c0%02d", *type, i);
                DrawImageWithName(name);
            }
    }

    int count = 0;
    while (Images[count].Filename != NULL)
        count++;

    ImageProbeNote("game     %s", GameName());
    ImageProbeNote("system   %s", SystemName());
    ImageProbeNote("pictures %d", count);
    ImageProbeNote("rooms    %d", GameHeader.NumRooms + 1);

    SetBit(GRAPHICSBIT);
    OpenGraphicsWindow();
    int width = ImageWidth, height = ImageHeight;

    for (int i = 0; i < count; i++) {
        /* Some pictures resize the window (Atari 8-bit Spider-Man, Apple II):
           start each one from the game's own size. */
        ImageWidth = width;
        ImageHeight = height;
        upside_down = 0;
        AnimationRunning = 0;
        ColorCyclingRunning = 0;
        OpenGraphicsWindow();
        glk_window_clear(Graphics);
        if (CurrentSys == SYS_APPLE2)
            ClearApple2ScreenMem();

        DrawImageWithName(Images[i].Filename);
        if (CurrentSys == SYS_APPLE2)
            DrawApple2ImageFromVideoMem();

        char detail[32];
        snprintf(detail, sizeof detail, "%5zu %08x", Images[i].Size,
            ImageProbeCRC(Images[i].Data, Images[i].Size));
        ImageProbeReport(Images[i].Filename, detail);
    }

    ResetBit(DARKBIT);
    for (int room = 0; room <= GameHeader.NumRooms; room++) {
        if (Rooms[room].Image == 255)
            continue;
        ImageWidth = width;
        ImageHeight = height;
        AnimationRunning = 0;
        ColorCyclingRunning = 0;
        MyLoc = room;
        char name[16];
        snprintf(name, sizeof name, "room%d", room);
        DrawCurrentRoom();
        if (Graphics == NULL) {
            ImageProbeNote("%-24s no picture", name);
            continue;
        }
        ImageProbeReport(name, NULL);
        glk_window_clear(Graphics);
    }
}
