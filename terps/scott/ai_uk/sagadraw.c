//
//  Based on Sagadraw v2.5 by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//
//
//  The 8 x 8 pixel graphic building blocks used for these images were
//  previously referred to in code as "characters", but that was a bit
//  confusing, as they don't have anything to do with the characters
//  used to print text. Also, this kind of computer graphics is commonly
//  called "tile based" when used in arcade games and similar.
//
//  All instances of "character" are now changed to "tile".
//  Hopefully this will make things less rather than more confusing.
//


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scottdefines.h"
#include "seasofblood.h"
#include "irmak.h"

#include "sagadraw.h"

static uint8_t *EndOfGraphicsData;

int32_t errorcount = 0;

/* real code starts here */

struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

static const struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    // Claymorgue C64: Many images are hopelessly broken, but some can be salvaged.
    // 12. "Dungeon" room with door
    { CLAYMORGUE_C64, 12, 378, 9, "\x20\xa4\x02\x80\x20\x82\x01\x20\x84" },
    // 28. Dragon
    { CLAYMORGUE_C64, 28, 584, 1, "\x90" },
    // 16. Empty cyan square, "Underwater"
    { CLAYMORGUE_C64, 16, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    // 16. Spectrum version. Empty cyan square, "Underwater"
    { CLAYMORGUE, 14, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    // 21. Gremlins C64. Snow plough
    { GREMLINS_C64, 21, 10, 5, "\x01\xa0\x03\x00\x01" },
    // 44. Behind bar
    { GREMLINS_C64, 44, 304, 1, "\xb1" },
    // 81. Empty pool bottom with drain
    { GREMLINS_C64, 81, 176, 1, "\xa0" },
    // 85. Sports department
    { GREMLINS_C64, 85, 413, 1, "\x94" },
    // 20. Garage
    { GREMLINS_GERMAN_C64, 20, 191, 9, "\x03\x00\x94\x0c\xb0\x02\x94\x0d\xb0" },
    // QP2 Spider-Man: 9. Madame Web. One tile with bad colour in T64 version
    { SPIDERMAN_C64, 9, 1241, 1, "\x81" },
    // 11. Secret Mission. White visitor's room with window. Broken in D64 version
    { SECRET_MISSION_C64, 11, 88, 2, "\x90\x18" },
    // 33. Key on window sill, fix middle segment rotation
    { SECRET_MISSION, 33, 2, 1, "\xa0" },
    // 33. Key on window sill, fix middle segment rotation
    { SECRET_MISSION_C64, 33, 2, 1, "\xa0" },
    // 38. Broken glass on window sill, fix bg colour
    { SECRET_MISSION, 38, 53, 1, "\x53" },
    // 38. Broken glass on window sill, fix bg colour
    { SECRET_MISSION_C64, 38, 53, 1, "\x53" },
    // 34. Left half of room with bricks and pillars, minor misplaced line
    { SEAS_OF_BLOOD_C64, 34, 471, 1, "\xa0" },
    { NUMGAMES, 0, 0, 0, "" },
};

static int FindImagePatch(GameIDType game, int image_number, int start)
{
    for (int i = start + 1; image_patches[i].id != NUMGAMES; i++) {
        if (image_patches[i].id == game && image_patches[i].picture_number == image_number) {
            return i;
        }
    }
    return 0;
}

static void Patch(uint8_t *offset, int patch_number)
{
    const struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        const char newval = patch->patch[i];
        offset[i + patch->offset] = (uint8_t)newval;
    }
}

static void PatchOutBrokenClaymorgueImagesC64(void)
{
    Output("[This copy of The Sorcerer of Claymorgue Castle has 16 broken or "
           "missing pictures. These have been patched out.]\n\n");
    for (int i = 12; i < 28; i++) {
        if (i != 16)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

static void PatchOutBrokenClaymorgueImagesZX(void)
{
    Output("[This copy of The Sorcerer of Claymorgue Castle has 26 broken or "
           "missing pictures. These have been patched out.]\n\n");
    for (int i = 9; i < 36; i++) {
        if (i != 14)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

void SagaSetup(size_t imgoffset)
{
	if (images != NULL)
        return;

    EndOfGraphicsData = entire_file + file_length;

    int32_t i, y;

    uint16_t image_offsets[Game->number_of_pictures];

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        fprintf(stderr, "SagaSetup: invalid palette. Entering text-only mode.\n");
        Game->number_of_pictures = 0;
        return;
    }

    DefinePalette();

    int version = Game->picture_format_version;

    int32_t tiles_start = Game->start_of_tiles + file_baseline_offset;
    int32_t offset_table_start = Game->start_of_image_data + file_baseline_offset;

    if (Game->start_of_image_data == FOLLOWS) {
        offset_table_start = tiles_start + 0x800;
    }

    int32_t data_offset = Game->image_address_offset + file_baseline_offset;
    if (imgoffset)
        data_offset = imgoffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(entire_file, tiles_start);

#ifdef DRAWDEBUG
    debug_print("Grabbing tile details\n");
    debug_print("Tile Offset: %04x\n", tiles_start - file_baseline_offset);
#endif
    for (i = 0; i < 256; i++) {
        for (y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *pos++;
        }
    }

    /* Now we have hopefully read the tile data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;

    pos = SeekToPos(entire_file, offset_table_start);

    int broken_claymorgue_pictures_c64 = 0;
    int broken_claymorgue_pictures_zx = 0;

    size_t hulk_coordinates = 0;
    size_t hulk_item_image_offsets = 0;
    size_t hulk_closeup_image_offsets = 0;
    size_t hulk_special_image_offsets = 0;
    size_t hulk_image_offset = 0;

    if (Game->picture_format_version == 0) {
        if (CurrentGame == HULK_C64) {
            hulk_coordinates = 0x22cd;
            hulk_item_image_offsets = 0x2731;
            hulk_closeup_image_offsets = 0x2761;
            hulk_special_image_offsets = 0x2781;
            hulk_image_offset = -0x7ff;
        } else if (CurrentGame == HULK) {
            hulk_coordinates = 0x26db;
            hulk_item_image_offsets = 0x2798;
            hulk_closeup_image_offsets = 0x27bc;
            hulk_special_image_offsets = 0x276e;
            hulk_image_offset = 0x441b;
        } else {
            fprintf(stderr, "SagaSetup: Unknown game with version 0 graphics. Entering text-only mode.\n");
            Game->number_of_pictures = 0;
            return;
        }
    }

    for (i = 0; i < numgraphics; i++) {
        if (Game->picture_format_version == 0) {
            uint16_t address;

            if (i < 11) {
                address = Game->start_of_image_data + (i * 2);
            } else if (i < 28) {
                address = hulk_item_image_offsets + (i - 10) * 2;
            } else if (i < 34) {
                address = hulk_closeup_image_offsets + (i - 28) * 2;
            } else {
                address = hulk_special_image_offsets + (i - 34) * 2;
            }

            address += file_baseline_offset;
            address = entire_file[address] + entire_file[address + 1] * 0x100;

            image_offsets[i] = address + hulk_image_offset;
        } else {
            image_offsets[i] = *(pos++);
            image_offsets[i] += *(pos++) * 0x100;
        }
    }

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
        pos = SeekToPos(entire_file, image_offsets[picture_number] + data_offset);
        if (pos == 0)
            return;

        img->width = *(pos++);
        if (img->width > IRMAK_IMGWIDTH)
            img->width = IRMAK_IMGWIDTH;

        img->height = *(pos++);
        if (img->height > IRMAK_IMGHEIGHT)
            img->height = IRMAK_IMGHEIGHT;

        if (version > 0) {
            img->xoff = *(pos++);
            if (img->xoff > IRMAK_IMGWIDTH)
                img->xoff = 4;
            img->yoff = *(pos++);
            if (img->yoff > IRMAK_IMGHEIGHT)
                img->yoff = 0;
        } else {
            if (picture_number > 9 && picture_number < 28) {
                img->xoff = entire_file[hulk_coordinates + picture_number - 10 + file_baseline_offset];
                img->yoff = entire_file[hulk_coordinates + 18 + picture_number - 10 + file_baseline_offset];
            } else {
                img->xoff = img->yoff = 0;
            }
        }

        if (CurrentGame == CLAYMORGUE_C64 && img->height == 0 && img->width == 0 && picture_number == 13) {
            PatchOutBrokenClaymorgueImagesC64();
            broken_claymorgue_pictures_c64 = 1;
        }

        if (broken_claymorgue_pictures_c64 && (picture_number == 16 || picture_number == 28)) {
            img->height = IRMAK_IMGHEIGHT;
            img->width = IRMAK_IMGWIDTH;
            img->xoff = 4;
            img->yoff = 0;
            if (picture_number == 28)
                pos = entire_file + 0x2005;
        }

        if (CurrentGame == CLAYMORGUE && img->height == 0 && img->width == 0 && picture_number == 9) {
            PatchOutBrokenClaymorgueImagesZX();
            broken_claymorgue_pictures_zx = 1;
        }

        if (broken_claymorgue_pictures_zx && (picture_number == 14)) {
            img->height = IRMAK_IMGHEIGHT;
            img->width = IRMAK_IMGWIDTH;
            img->xoff = 4;
            img->yoff = 0;
        }

        img->imagedata = pos;
        img->datasize = EndOfGraphicsData - pos;

        int patch = FindImagePatch(CurrentGame, picture_number, 0);
        while (patch) {
            Patch(img->imagedata, patch);
            patch = FindImagePatch(CurrentGame, picture_number, patch);
        }

        img++;
    }

    InitIrmak(Game->number_of_pictures, Game->picture_format_version);
}
