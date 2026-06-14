// Headless ZX Spectrum graphics-state dumper. Runs the real interpreter's
// game-detection / loading pipeline (DetectGame -> SagaGraphicsSetup) on a ZX
// Spectrum Scott Adams game (a .z80 snapshot or .tzx tape) and dumps the state
// the Irmak tile renderer (aiukgraphics/irmak.c:DrawPictureNumber) consumes:
//
//   tiles.bin            the 256-entry tile font (256 * 8 bytes)
//   meta.txt             game id, picture_format_version, palette, and a per-
//                        picture table (width,height,xoff,yoff in tiles)
//   pic<NNN>.dat         one picture's raw tile/attribute data stream
//                        (img.imagedata .. end-of-graphics; the decoder stops
//                        itself, exactly as in-game)
//
// Unlike the C64/Atari/Apple titles the ZX graphics never populate USImages —
// they live in the separate aiukgraphics/ai_uk subsystem (irmak globals
// `images[]` / `tiles[]`), so the USImage-based extract_images yields nothing.
//
//   usage: zxextract <game-file> <outdir> [picnum]   (picnum default 1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "detect_game.h"
#include "irmak.h"
#include "palette.h"
#include "line_drawing.h"

extern const char *game_file;
extern Image *images;
extern int number_of_images;
extern int image_version;
extern uint8_t tiles[256][8];
extern palette_type palchosen;
extern line_image *LineImages;

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <game-file> <outdir> [picnum]\n", argv[0]);
        return 2;
    }
    game_file = argv[1];
    const char *outdir = argv[2];
    int picnum = (argc > 3) ? atoi(argv[3]) : 1;

    GameIDType gt = DetectGame(game_file);
    int pfv = Game ? Game->picture_format_version : -1;
    fprintf(stderr, "DetectGame -> %d, CurrentSys=%d, number_of_images=%d, version=%d, palette=%d, pfv=%d\n",
            (int)gt, CurrentSys, number_of_images, image_version, (int)palchosen, pfv);

    char path[1200];

    // ---- Howarth / Mysterious Adventures vector format (picture_format_version 99) ----
    if (pfv == 99 && LineImages != NULL) {
        int nrooms = GameHeader.NumRooms + 1;
        snprintf(path, sizeof path, "%s/meta.txt", outdir);
        FILE *mf = fopen(path, "w");
        if (mf) {
            fprintf(mf, "game %d\nformat howarth\npalette %d\nnumimages %d\n",
                    (int)gt, (int)Game->palette, nrooms);
            for (int i = 0; i < nrooms; i++)
                fprintf(mf, "pic %d bg %d size %zu\n", i, LineImages[i].bgcolour, LineImages[i].size);
            fclose(mf);
        }
        if (picnum < 0 || picnum >= nrooms) {
            fprintf(stderr, "picnum %d out of range (0..%d)\n", picnum, nrooms - 1);
            return 1;
        }
        snprintf(path, sizeof path, "%s/pic%03d.dat", outdir, picnum);
        FILE *pf = fopen(path, "wb");
        if (pf) {
            fwrite(LineImages[picnum].data, 1, LineImages[picnum].size, pf);
            fclose(pf);
            printf("pic%03d (howarth): bg=%d, %zu bytes, palette=%d -> %s\n",
                   picnum, LineImages[picnum].bgcolour, LineImages[picnum].size,
                   (int)Game->palette, path);
        }
        return 0;
    }

    if (images == NULL || number_of_images == 0) {
        fprintf(stderr, "No Irmak images (not a ZX tile-graphics game?)\n");
        return 1;
    }

    // tile font
    snprintf(path, sizeof path, "%s/tiles.bin", outdir);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(tiles, 1, 256 * 8, f); fclose(f); }

    // meta + per-picture geometry table
    snprintf(path, sizeof path, "%s/meta.txt", outdir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "game %d\nversion %d\npalette %d\nnumimages %d\n",
                (int)gt, image_version, (int)palchosen, number_of_images);
        for (int i = 0; i < number_of_images; i++)
            fprintf(f, "pic %d w %d h %d xoff %d yoff %d size %zu\n",
                    i, images[i].width, images[i].height,
                    images[i].xoff, images[i].yoff, images[i].datasize);
        fclose(f);
    }

    if (picnum < 0 || picnum >= number_of_images) {
        fprintf(stderr, "picnum %d out of range (0..%d)\n", picnum, number_of_images - 1);
        return 1;
    }
    Image *img = &images[picnum];
    snprintf(path, sizeof path, "%s/pic%03d.dat", outdir, picnum);
    f = fopen(path, "wb");
    if (f) {
        fwrite(img->imagedata, 1, img->datasize, f);
        fclose(f);
        printf("pic%03d: %dx%d tiles, xoff=%d yoff=%d, %zu data bytes -> %s\n",
               picnum, img->width, img->height, img->xoff, img->yoff,
               img->datasize, path);
    }
    return 0;
}
