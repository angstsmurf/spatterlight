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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "animations.h"
#include "apple2draw.h"
#include "common.h"
#include "graphics.h"
#include "image_probe.h"
#include "loaddatabase.h"
#include "parseinput.h"
#include "stdraw.h"


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

/* The truncated-file test (make trunccheck). With PLUS_PROBE_TRUNCATE set, the
   probe does not draw anything. It loads the binary database of the game again
   and again, each time from a copy that is cut off somewhere and ends right
   before a page that may not be read, so that a loader reading one byte past
   the end of its data is killed for it. Every load runs in a process of its
   own, since a loader that sees the end coming gives up with Fatal().

   The first step finds the length of the database, the shortest copy that
   still loads: the picture data behind it is not the loader's business. */

enum { CUT_LOADED, CUT_REFUSED, CUT_CRASHED, CUT_HUNG, CUT_OUTCOMES };

static int LoadCutCopy(uint8_t *guard, const uint8_t *database, size_t cut)
{
    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        exit(1);
    }
    if (child == 0) {
        alarm(5);
        if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr))
            _exit(3);
        mem = guard - cut;
        memlen = cut;
        memcpy(mem, database, cut);
        _exit(LoadDatabaseBinary() ? 0 : 2);
    }
    int status;
    if (waitpid(child, &status, 0) != child) {
        perror("waitpid");
        exit(1);
    }
    if (WIFSIGNALED(status))
        return WTERMSIG(status) == SIGALRM ? CUT_HUNG : CUT_CRASHED;
    return WEXITSTATUS(status) == 0 ? CUT_LOADED : CUT_REFUSED;
}

static void TruncationTest(void)
{
    if (CurrentSys == SYS_MSDOS) {
        printf("truncate: not a binary database\n");
        return;
    }
    fflush(stdout);

    const uint8_t *database = mem;
    size_t full = memlen;
    size_t page = (size_t)getpagesize();
    size_t room = (full + page - 1) / page * page;
    uint8_t *buffer = mmap(NULL, room + page, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (buffer == MAP_FAILED || mprotect(buffer + room, page, PROT_NONE) != 0) {
        perror("truncation test");
        exit(1);
    }
    uint8_t *guard = buffer + room;

    if (LoadCutCopy(guard, database, full) != CUT_LOADED) {
        printf("truncate: the whole database does not load a second time\n");
        return;
    }
    /* A longer copy of a database that loads will load as well */
    size_t low = 0, high = full;
    while (low + 1 < high) {
        size_t middle = low + (high - low) / 2;
        if (LoadCutCopy(guard, database, middle) == CUT_LOADED)
            high = middle;
        else
            low = middle;
    }
    size_t length = high;

    /* Every length near the end, where a reader is most likely to be one
       byte off, and a thousand or so of the others */
    int outcomes[CUT_OUTCOMES] = { 0 };
    size_t first_bad = 0;
    size_t step = length / 1000 | 1;
    size_t dense = length > 300 ? length - 300 : 0;
    for (size_t cut = 0; cut < length; cut += cut < dense ? step : 1) {
        int outcome = LoadCutCopy(guard, database, cut);
        outcomes[outcome]++;
        if (outcome != CUT_REFUSED && first_bad == 0)
            first_bad = cut + 1;
    }
    printf("truncate: database %zu bytes, %d cuts refused, %d loaded, %d crashed, %d hung",
        length, outcomes[CUT_REFUSED], outcomes[CUT_LOADED], outcomes[CUT_CRASHED],
        outcomes[CUT_HUNG]);
    if (first_bad)
        printf(", first at %zu bytes", first_bad - 1);
    printf("\n");
}

void ImageProbeDump(void)
{
    if (getenv("PLUS_PROBE_TRUNCATE")) {
        TruncationTest();
        return;
    }

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
        if (CurrentSys == SYS_APPLE2) {
            GraphicsWindowChanged();
            DrawApple2ImageFromVideoMem();
        }

        char detail[32];
        snprintf(detail, sizeof detail, "%5zu %08x", Images[i].Size,
            ImageProbeCRC(Images[i].Data, Images[i].Size));
        ImageProbeReport(Images[i].Filename, detail);
    }

    ResetBit(DARKBIT);
    for (int room = 0; room <= GameHeader.NumRooms; room++) {
        if (Rooms[room].Exits[EXIT_IMAGE] == NO_ROOM_IMAGE)
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
        ClearGraphicsWindow();
    }
}
