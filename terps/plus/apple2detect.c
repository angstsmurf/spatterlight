//
//  apple2detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-03.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "companionfile.h"
#include "definitions.h"
#include "read_le16.h"
#include "graphics.h"

#include "woz2nib.h"
#include "ciderpress.h"

#include "apple2detect.h"

static const imglist a2listFantastic[] = {
    { "R001", 0x1e800 },
    { "R002", 0x1df00 },
    { "R004", 0x1db00 },
    { "R005", 0x1cb00 },
    { "R006", 0x1c200 },
    { "R007", 0x1bb00 },
    { "R008", 0x1b300 },
    { "R009", 0x12400 },
    { "R010", 0x1aa00 },
    { "R011", 0x1a100 },
    { "R012", 0x18e00 },
    { "R013", 0x18b00 },
    { "R014", 0x18100 },
    { "R015", 0x17800 },
    { "R016", 0x16d00 },
    { "R017", 0x16800 },
    { "R018", 0x15c00 },
    { "R019", 0x15500 },
    { "R020", 0x14d00 },
    { "R021", 0x13e00 },
    { "R022", 0x13300 },
    { "B020", 0xc10e },
    { "B002", 0x11500 },
    { "B003", 0x10f00 },
    { "B004", 0x10a00 },
    { "B005", 0x10000 },
    { "B006", 0xfb00 },
    { "B007", 0xf900 },
    { "B008", 0xf700 },
    { "B009", 0xf200 },
    { "B010", 0xe800 },
    { "B011", 0xe600 },
    { "B012", 0xe400 },
    { "B013", 0xe300 },
    { "B014", 0xc600 },
    { "B015", 0xde00 },
    { "B016", 0xda00 },
    { "B017", 0xd800 },
    { "B018", 0xd700 },
    { "B019", 0xd000 },
    { "B020", 0xcc00 },
    { "B021", 0xca00 },
    { "B022", 0xc900 },
    { "B023", 0xc800 },
    { "S000", 0x1000 },
    { "S001", 0x2000 },
    { "S002", 0x2400 },
    { "S003", 0x2600 },
    { "S004", 0x2800 },
    { "S005", 0x2a00 },
    { "S006", 0x2c00 },
    { "S007", 0x2e00 },
    { "S008", 0x3500 },
    { "S009", 0x3600 },
    { "S010", 0x3700 },
    { "S011", 0x3900 },
    { "S012", 0x3b00 },
    { "S013", 0x3c00 },
    { "S014", 0x3f00 },
    { "S015", 0x4100 },
    { "S016", 0x4200 },
    { "S017", 0x4300 },
    { "S018", 0x4400 },
    { "S019", 0x4500 },
    { "S020", 0x4600 },
    { NULL, 0, }
};

static const imglist a2listSpidey[] = {
    { "R001", 0x19f00 },
    { "R002", 0x19900 },
    { "R003", 0x14200 },
    { "R005", 0xdf00 },
    { "R006", 0x19300 },
    { "R007", 0x18500 },
    { "R008", 0x17800 },
    { "R013", 0x16f00 },
    { "R015", 0xd700 },
    { "R017", 0x16100 },
    { "R018", 0xce00 },
    { "R019", 0x13400 },
    { "R022", 0x12400 },
    { "R024", 0x10000 },
    { "R025", 0x15700 },
    { "R034", 0x14f00 },
    { "R037", 0x11500 },
    { "R040", 0xf100 },
    { "B015", 0x5d00 }, // Bio gem
    { "B016", 0xbd00 }, // Egg
    { "B021", 0xc100 }, // Stopped fan
    { "B023", 0xb900 }, // Shaft
    { "B025", 0xc600 }, // wall mirror
    { "B027", 0xb700 }, // open air duct
    { "B028", 0xb500 }, // strange cloud
    { "B031", 0xae00 }, // Octopus
    { "B033", 0xa900 }, // Electro
    { "B034", 0xa400 }, // Stunned Ock
    { "B035", 0x9f00 }, // Stunned Electro
    { "B038", 0x9c00 }, // Lizard
    { "B044", 0x7300 },
    { "B049", 0x7700 },
    { "B050", 0x6500 },
    { "B051", 0x9100 },
    { "B052", 0x8d00 },
    { "B053", 0x7000 },
    { "B055", 0x6800 },
    { "B056", 0x6b00 },
    { "B058", 0x8700 },
    { "B059", 0x8500 },
    { "B060", 0x8100 },
    { "B062", 0x7f00 }, // Ringmaster
    { "B071", 0x7800 }, // Clock
    { "S000", 0x5100 },
    { "S001", 0x6e00 },
    { "S002", 0x7100 },
    { "S003", 0x7400 },
    { "S004", 0x7700 },
    { "S005", 0x7a00 },
    { "S009", 0x4200 },
    { "S010", 0x3800 },
    { "S011", 0x1900 },
    { "S012", 0x2300 },
    { "S013", 0x2900 },
    { "S014", 0x6200 },
    { "S015", 0x6700 },
    { "S016", 0x6900 },
    { "S017", 0x6a00 },
    { "S018", 0x6c00 },
    { "S020", 0x7d00 },
    { "S021", 0x8300 },
    { "S022", 0x8900 },
    { "S023", 0x16179 },
    { "S024", 0x16682 },
    { "S026", 0x3100 },
    { NULL, 0 }
};

static const imglist a2listBanzai[] = {
    { "R001", 0x1dc00 },
    { "R002", 0x1d600 },
    { "R003", 0x1cd00 },
    { "R004", 0x1c000 },
    { "R005", 0x1b400 },
    { "R006", 0x1a900 },
    { "R008", 0x1a000 },
    { "R009", 0x19500 },
    { "R010", 0x16f00 },
    { "R011", 0x18c00 },
    { "R012", 0x18700 },
    { "R013", 0x18000 },
    { "R014", 0x17600 },
    { "R015", 0x16f00 },
    { "R016", 0xf300 },
    { "R017", 0x16c00 },
    { "R018", 0x15f00 },
    { "R019", 0x15400 },
    { "R020", 0x14d00 },
    { "R021", 0x14100 },
    { "R022", 0x13900 },
    { "R023", 0x13100 },
    { "R024", 0x12a00 },
    { "R025", 0x11f00 },
    { "R026", 0x11500 },
    { "R028", 0x10d00 },
    { "R029", 0x10600 },
    { "R035", 0xfd00 },
    { "S000", 0x4700 },
    { "B027", 0xf000 },
    { "B004", 0xe600 },
    { "B005", 0xe500 },
    { "B006", 0xdf00 },
    { "B019", 0xda00 },
    { "B007", 0xd700 },
    { "B030", 0xd600 },
    { "B025", 0xd100 },
    { "B026", 0xd000 },
    { "B051", 0xcf00 },
    { "B023", 0xcd00 },
    { "B055", 0xcc00 },
    { "B056", 0xc300 },
    { "B070", 0xb900 },
    { "B071", 0xb400 },
    { "B073", 0xb200 },
    { "B074", 0xb000 },
    { "B037", 0xb100 },
    { "B034", 0xaf00 },
    { "B035", 0xa700 },
    { "S001", 0x1000 },
    { "S002", 0x1900 },
    { "S003", 0x1a00 },
    { "S004", 0x1d00 },
    { "S005", 0x2000 },
    { "S006", 0x2300 },
    { "S007", 0x2600 },
    { "S008", 0x2900 },
    { "S009", 0x2c00 },
    { "S010", 0x2f00 },
    { "S011", 0x3700 },
    { "S012", 0x3a00 },
    { "S013", 0x3e00 },
    { "S014", 0x4100 },
    { "S015", 0x4300 },
    { "S016", 0x4500 },
    { "S017", 0x4600 },
    { NULL, 0, }
};

/* Size of a 35 track, 16 sector .dsk image */
#define A2_DISK_SIZE (35 * 16 * 256)
#define A2_SECTOR_SIZE 256

/* The game database begins at this offset of the sector-reversed disk
   image, and the size of its actions is found at A2_ACTION_SIZE_OFFSET. */
#define A2_DATABASE_OFFSET 125438
#define A2_ACTION_SIZE_OFFSET 0x1EA32

/* The range of plausible action sizes: outside it, this is not the disk
   side with the database on it. */
#define A2_MIN_ACTION_SIZE 4000
#define A2_MAX_ACTION_SIZE 7000

/* The image offsets in the database count backwards from here */
#define A2_IMAGE_BASE (0x23000 - 0x100)

/* The game disk with its sectors in reverse order, kept from detection
   until LookForApple2Images() has taken the pictures from it. */
static uint8_t *reversed_disk = NULL;

// If the buffer is an Apple II WOZ disk image, convert it into a standard
// 143360-byte DOS 3.3 ordered .dsk image (via the shared readwoz library) so
// that the rest of this file can treat it exactly like a .dsk file.
// Returns a newly allocated buffer (caller frees) and sets *outsize, or NULL
// if the data is not WOZ or conversion fails.
static uint8_t *ConvertWozToDsk(uint8_t *data, size_t datasize, size_t *outsize)
{
    if (data == NULL || !is_woz_image(data, datasize))
        return NULL;

    size_t nibsize = datasize;
    uint8_t *nib = woz2nib(data, &nibsize);
    if (nib == NULL)
        return NULL;

    uint8_t *dsk = ReadFullImageFromNib(nib, nibsize);
    free(nib);
    if (dsk == NULL)
        return NULL;

    *outsize = A2_DISK_SIZE;
    return dsk;
}

// If *data is a WOZ image, replace it with its conversion to a .dsk image.
static void LoadAsDsk(uint8_t **data, size_t *size)
{
    size_t dsksize;
    uint8_t *dsk = ConvertWozToDsk(*data, *size, &dsksize);
    if (dsk != NULL) {
        free(*data);
        *data = dsk;
        *size = dsksize;
    }
}

// Copy the 256-byte sectors of src into a new buffer of imagesize bytes
// in reverse order, so that the last sector of src comes first.
static uint8_t *ReverseSectors(const uint8_t *src, size_t srcsize, size_t imagesize)
{
    uint8_t *reversed = MemAlloc(imagesize);
    size_t offset = imagesize - A2_SECTOR_SIZE;
    for (size_t i = 0; i < srcsize && i < imagesize; i += A2_SECTOR_SIZE) {
        memcpy(reversed + offset, src + i, A2_SECTOR_SIZE);
        offset -= A2_SECTOR_SIZE;
    }
    return reversed;
}

static int IsPlausibleActionSize(int actionsize)
{
    return (actionsize >= A2_MIN_ACTION_SIZE && actionsize <= A2_MAX_ACTION_SIZE);
}

int DetectApple2(uint8_t **sf, size_t *extent)
{
    // Accept WOZ images by converting them to a .dsk image up front.
    LoadAsDsk(sf, extent);

    if (*sf == NULL || *extent > MAX_LENGTH || *extent < A2_DISK_SIZE)
        return 0;

    reversed_disk = ReverseSectors(*sf, *extent, A2_DISK_SIZE);

    int actionsize = READ_LE_UINT16(reversed_disk + A2_ACTION_SIZE_OFFSET);
    debug_print("Actionsize: %d\n", actionsize);
    if (!IsPlausibleActionSize(actionsize)) {
        size_t companionsize;
        uint8_t *companionfile = GetCompanionFile(game_file, &companionsize);
        // The companion side may itself be a WOZ image; convert it too.
        if (companionfile != NULL)
            LoadAsDsk(&companionfile, &companionsize);
        if (companionfile && companionsize >= A2_DISK_SIZE) {
            free(reversed_disk);
            reversed_disk = ReverseSectors(companionfile, companionsize, companionsize);
            actionsize = READ_LE_UINT16(reversed_disk + A2_ACTION_SIZE_OFFSET);
        }
        free(companionfile);
        if (!IsPlausibleActionSize(actionsize)) {
            free(reversed_disk);
            reversed_disk = NULL;
            return 0;
        }
    }

    size_t datasize = A2_DISK_SIZE - A2_DATABASE_OFFSET;
    uint8_t *gamedata = MemAlloc(datasize);
    memcpy(gamedata, reversed_disk + A2_DATABASE_OFFSET, datasize);

    AdoptDataFile(sf, extent, gamedata, datasize, SYS_APPLE2);
    return 1;
}

static int FindImageWithOffset(const imglist *list, size_t offset)
{
    for (int ct = 0; list[ct].filename != NULL; ct++) {
        if (list[ct].offset == offset)
            return ct;
    }
    return -1;
}

// Has an image with this disk offset been added to the first count Images?
static int IsImageAdded(int count, size_t offset)
{
    for (int j = 0; j < count; j++) {
        if (Images[j].DiskOffset == offset)
            return 1;
    }
    return 0;
}

static void CopyImageData(imgrec *rec, size_t offset, size_t size)
{
    rec->Data = MemAlloc(size);
    memcpy(rec->Data, reversed_disk + offset, size);
    rec->Size = size;
}

/* image is the disk offset of the picture of a room or an object, as
   found in the database. Unless that picture was added already, copy it
   from the disk to Images[*created]. Returns the number of the picture,
   or image unchanged if it was not added. The size word of an object
   picture does not count itself: size_extra is 2 for those. */
static uint32_t AddImageWithOffset(const imglist *list, uint32_t image, int *created, int capacity, size_t size_extra)
{
    if (image == 0 || IsImageAdded(*created, image) || *created >= capacity)
        return image;

    size_t offset = A2_IMAGE_BASE - image;
    if (offset + 2 > A2_DISK_SIZE)
        return image;
    size_t size = READ_LE_UINT16(reversed_disk + offset) + size_extra;
    if (offset + size > A2_DISK_SIZE)
        size = A2_DISK_SIZE - offset;

    int ct = FindImageWithOffset(list, image);
    if (ct < 0) {
        debug_print("Error, image with offset %x not found!\n", image);
        return image;
    }
    /* The names are string literals, never written to or freed */
    Images[*created].Filename = (char *)list[ct].filename;
    Images[*created].DiskOffset = list[ct].offset;

    CopyImageData(&Images[*created], offset, size);
    (*created)++;
    return ImageNumberFromName(list[ct].filename);
}

/* Change the disk offset of a picture that was added to its number */
static uint32_t ConvertOffsetToNumber(const imglist *list, uint32_t image)
{
    int ct = FindImageWithOffset(list, image);
    return (ct >= 0) ? ImageNumberFromName(list[ct].filename) : image;
}

void LookForApple2Images(void)
{
    if (reversed_disk == NULL)
        return;

    const imglist *list = NULL;

    switch (CurrentGame) {
    case SPIDERMAN:
        list = a2listSpidey;
        break;
    case BANZAI:
        list = a2listBanzai;
        break;
    case FANTASTIC4:
        list = a2listFantastic;
        break;
    default:
        Fatal("Unknown game!");
        break;
    }

    int count = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;

    Images = NewImageList(count);

    int created = 0;

    /* The first pass adds each picture once; the second is for the rooms
       and objects that share a picture with an earlier one. */
    for (int i = 0; i <= GameHeader.NumRooms; i++)
        Rooms[i].Image = AddImageWithOffset(list, Rooms[i].Image, &created, count, 0);
    for (int i = 0; i <= GameHeader.NumRooms; i++)
        Rooms[i].Image = ConvertOffsetToNumber(list, Rooms[i].Image);

    for (int i = 0; i <= GameHeader.NumObjImg; i++)
        ObjectImages[i].Image = AddImageWithOffset(list, ObjectImages[i].Image, &created, count, 2);
    for (int i = 0; i <= GameHeader.NumObjImg; i++)
        ObjectImages[i].Image = ConvertOffsetToNumber(list, ObjectImages[i].Image);

    /* The special pictures are not in the database */
    for (int ct = 0; list[ct].filename != NULL && created < count; ct++) {
        if (list[ct].filename[0] == 'S') {
            Images[created].Filename = (char *)list[ct].filename;
            CopyImageData(&Images[created], list[ct].offset, READ_LE_UINT16(reversed_disk + list[ct].offset));
            created++;
        }
    }

    Images[created].Filename = NULL;

    free(reversed_disk);
    reversed_disk = NULL;
}
