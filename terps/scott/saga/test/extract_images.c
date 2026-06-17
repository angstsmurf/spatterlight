// Headless image-blob dumper. Runs the real interpreter's game detection /
// loading pipeline (DetectGame -> the per-platform loaders, including the C64
// decrunch and Apple II / Atari / TI-99 extractors) and writes every resulting
// USImage->imagedata to <outdir>/<sys>_<usage>_<index>.dat. These are the exact
// bytes the renderers (DrawApple2ImageFromData, DrawC64A8ImageFromData, ...)
// consume, so the per-format byte-exact tests can use real game data without
// re-implementing the (heavily entangled) extraction pipeline.
//
//   usage: extract_images <game-file> <outdir>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "detect_game.h"
#include "sagagraphics.h"

extern const char *game_file;

static const char *sysname(MachineType s) {
    switch (s) {
        case SYS_MSDOS: return "dos";
        case SYS_C64: return "c64";
        case SYS_ATARI8: return "atari8";
        case SYS_APPLE2: return "apple2";
        case SYS_APPLE2_LINES: return "apple2lines";
        case SYS_ATARI8_LINES: return "atari8lines";
        case SYS_TI994A: return "ti994a";
        case SYS_C64_TINY: return "c64tiny";
        default: return "unknown";
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <game-file> <outdir>\n", argv[0]);
        return 2;
    }
    game_file = argv[1];
    const char *outdir = argv[2];

    GameIDType gt = DetectGame(game_file);
    fprintf(stderr, "DetectGame -> %d, CurrentSys=%d\n", (int)gt, CurrentSys);

#ifdef DUMP_GAME_CONTENT
    fprintf(stderr, "HDR rooms=%d items=%d actions=%d words=%d messages=%d\n",
            GameHeader.NumRooms, GameHeader.NumItems, GameHeader.NumActions,
            GameHeader.NumWords, GameHeader.NumMessages);
    if (Rooms) {
        for (int i = 0; i <= GameHeader.NumRooms && i < 5; i++)
            fprintf(stderr, "  ROOM[%d]=\"%s\"\n", i, Rooms[i].Text ? Rooms[i].Text : "(null)");
    }
    if (Messages) {
        for (int i = 0; i < 5 && i <= GameHeader.NumMessages; i++)
            fprintf(stderr, "  MSG[%d]=\"%s\"\n", i, Messages[i] ? Messages[i] : "(null)");
    }
#endif

    // The Apple II bitmap renderer (DrawApple2ImageFromData) needs the per-game
    // descramble table (the disk's $2000 row-address LUT) when present; dump it
    // alongside the blobs so the render test can reproduce the scrambled path.
    extern uint8_t *descrambletable;
    if (descrambletable) {
        char path[1100];
        snprintf(path, sizeof path, "%s/descrambletable.bin", outdir);
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(descrambletable, 1, 0x200, f); fclose(f);
                 printf("  %s  (descramble LUT)\n", path); }
    }

    int n = 0;
    for (USImage *img = USImages; img != NULL; img = img->next) {
        if (img->imagedata == NULL || img->datasize == 0)
            continue;
        char path[1100];
        snprintf(path, sizeof path, "%s/%s_u%d_i%03d.dat",
                 outdir, sysname(img->systype), img->usage, img->index);
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(img->imagedata, 1, img->datasize, f);
            fclose(f);
            printf("  %s  (%zu bytes)\n", path, img->datasize);
            n++;
        }
    }
    printf("dumped %d image blob(s)\n", n);
    return n ? 0 : 1;
}
