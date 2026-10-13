//
//  atari8detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "companionfile.h"
#include "definitions.h"
#include "gameinfo.h"
#include "graphics.h"
#include "loaddatabase.h"
#include "read_le16.h"

#include "atari8detect.h"

/* The pictures are found through a table on the boot disk, which gives
   two bytes for each picture: 100 room pictures (R), 100 object pictures
   (B) and 50 special pictures (S), in that order. The second byte and the
   two low bits of the first are the sector of the picture, counted from 1,
   and the five high bits of the first, times seven, its position there.
   A sector of 0 means that there is no such picture. What the table points
   at is the size word of the picture, two bytes into its data. */
enum {
    A8_HEADER_SIZE = 16,
    A8_SECTOR_SIZE = 128,
    A8_PICTURE_TABLE = 0x9593,
    A8_GROUP_SLOTS = 100,
    A8_SLOTS = 250,
    A8_NAME_SIZE = 5
};

/* The title picture of Spider-Man is on the boot disk and not in the
   table. */
#define A8_SPIDERMAN_TITLE 0x6d97

/* The names of the pictures in the table. They are never freed, as those of
   the other systems are string literals. */
static char a8names[A8_SLOTS][A8_NAME_SIZE];

/* The table is at the first sector position of the disk image and its first
   entry, a room 0, is empty. */
static const uint8_t *FindA8PictureTable(const uint8_t *disk, size_t size)
{
    if (disk == NULL || size < A8_PICTURE_TABLE + A8_SLOTS * 2)
        return NULL;
    const uint8_t *table = disk + A8_PICTURE_TABLE;
    if (table[0] != 0 || table[1] != 0)
        return NULL;
    return table;
}

/* A picture begins with its size and its left, top, right and bottom edges,
   the left and right ones in bytes. */
static int IsA8Picture(const uint8_t *data, size_t datasize, size_t offset)
{
    if (offset < 2 || offset + 6 > datasize)
        return 0;
    size_t size = READ_LE_UINT16(data + offset);
    const uint8_t *edges = data + offset + 2;
    return (size >= 4 && offset + 2 + size <= datasize && edges[0] <= edges[2] && edges[2] <= 40 && edges[1] <= edges[3] && edges[3] <= 192);
}

/* Fill list, which has room for A8_SLOTS + 1 entries, with
   the pictures of the table that are on the picture disk. Returns their
   number. */
static int ReadA8PictureTable(imglist *list, const uint8_t *table, const uint8_t *data, size_t datasize)
{
    int count = 0;

    for (int slot = 0; slot < A8_SLOTS; slot++) {
        uint8_t high = table[slot * 2];
        size_t sector = (high & 3) << 8 | table[slot * 2 + 1];
        if (sector == 0)
            continue;
        size_t offset = A8_HEADER_SIZE + (sector - 1) * A8_SECTOR_SIZE + (high >> 3) * 7;
        if (!IsA8Picture(data, datasize, offset)) {
            debug_print("No picture at %zx for slot %d\n", offset, slot);
            continue;
        }
        snprintf(a8names[slot], A8_NAME_SIZE, "%c%03d", "RBS"[slot / A8_GROUP_SLOTS], slot % A8_GROUP_SLOTS);
        list[count].filename = a8names[slot];
        list[count].offset = offset;
        list[count].on_boot_disk = 0;
        count++;
    }
    return count;
}

/* The picture data skips over the 128 bytes at this offset of the
   picture disk. A picture that begins before the hole continues after it. */
#define ATARI_HOLE_START 0xb390
#define ATARI_HOLE_END 0xb410

/* Copy the picture with its size word at offset on the picture disk to img.
   Returns 0 if it is not all there. */
static int CopyAtariImage(imgrec *img, const imglist *entry, const uint8_t *data, size_t datasize)
{
    size_t offset = entry->offset;

    if (offset < 2 || offset + 2 > datasize) {
        debug_print("Image %s offset %zx out of bounds. Disk size: %zx\n", entry->filename, offset, datasize);
        return 0;
    }

    size_t size = READ_LE_UINT16(data + offset) + 4;
    if (offset + size >= datasize) {
        debug_print("Image %s offset %zx out of bounds. Image size: %zx, disk size: %zx\n", entry->filename, offset, size, datasize);
        size = datasize - offset + 2;
    }

    /* The names are string literals, never written to or freed */
    img->Filename = (char *)entry->filename;
    img->Data = MemAlloc(size);
    img->Size = size;
    memcpy(img->Data, data + offset - 2, size);

    if (offset < ATARI_HOLE_START && offset + size > ATARI_HOLE_START) {
        size_t before_hole = ATARI_HOLE_START - offset + 2;
        size_t after_hole = size - before_hole;
        if (ATARI_HOLE_END + after_hole <= datasize)
            memcpy(img->Data + before_hole, data + ATARI_HOLE_END, after_hole);
    }
    return 1;
}

static void ExtractImagesFromAtariCompanionFile(uint8_t *data, size_t datasize, uint8_t *bootdisk, size_t bootsize)
{
    imglist list[A8_SLOTS + 1];
    int count = 0;

    const uint8_t *table = FindA8PictureTable(bootdisk, bootsize);
    if (table != NULL)
        count = ReadA8PictureTable(list, table, data, datasize);

    if (CurrentGame == SPIDERMAN) {
        list[count].filename = "S000";
        list[count].offset = A8_SPIDERMAN_TITLE;
        list[count].on_boot_disk = 1;
        count++;
    }

    Images = NewImageList(count);

    /* A picture that is not all on the disk is left out, as one without
       data cannot be drawn */
    int loaded = 0;
    for (int i = 0; i < count; i++) {
        const uint8_t *disk = list[i].on_boot_disk ? bootdisk : data;
        size_t disksize = list[i].on_boot_disk ? bootsize : datasize;
        if (disk && CopyAtariImage(&Images[loaded], &list[i], disk, disksize))
            loaded++;
    }
    Images[loaded].Filename = NULL;
}

static const uint8_t atrheader[6] = { 0x96, 0x02, 0x80, 0x16, 0x80, 0x00 };

/* Try to load the game database at data_start in a disk image. On
   success, mem is left pointing into the disk image, which therefore
   must never be freed. */
static int TryDatabaseIn(uint8_t *file, size_t size, size_t data_start)
{
    if (size <= data_start)
        return 0;
    mem = file + data_start;
    memlen = size - data_start;
    return LoadDatabaseBinary();
}

int DetectAtari8(uint8_t **sf, size_t *extent)
{
    uint8_t *main_disk = *sf;
    size_t main_size = *extent;
    int result = 0;
    size_t data_start = 0xff8e;
    // Header actually starts at offset 0xffc0 (0xff8e + 0x32).
    // We add 50 bytes at the head to match the C64 files.

    if (main_size > MAX_LENGTH || main_size < data_start)
        return 0;

    for (int i = 0; i < 6; i++)
        if ((*sf)[i] != atrheader[i])
            return 0;

    size_t companionsize;
    uint8_t *companionfile = GetCompanionFile(game_file, &companionsize);

    result = TryDatabaseIn(main_disk, main_size, data_start);
    if (!result && companionfile != NULL) {
        debug_print("Could not find database in this file, trying the companion file\n");
        result = TryDatabaseIn(companionfile, companionsize, data_start);
        if (result) {
            debug_print("Found database in companion file. Switching files.\n");
            uint8_t *temp = companionfile;
            size_t tempsize = companionsize;
            companionfile = main_disk;
            companionsize = main_size;
            main_disk = temp;
            main_size = tempsize;
        }
    }

    if (result) {
        SetSystem(SYS_ATARI8);
        if (companionfile) {
            ExtractImagesFromAtariCompanionFile(companionfile, companionsize, main_disk, main_size);
            free(companionfile);
        } else {
            // No picture disk: play without pictures.
            Images = NewImageList(0);
        }
    } else {
        debug_print("Failed loading database\n");
        /* Give the other detectors the file as we found it */
        mem = main_disk;
        memlen = main_size;
        free(companionfile);
    }

    return result;
}
