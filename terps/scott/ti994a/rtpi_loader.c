/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  rtpi_loader.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for "Return to Pirate's Isle" (RTPI), a Scott Adams adventure
 *  originally released for the TI-99/4A.
 *
 *  The game data is distributed as a zip archive containing ROM files
 *  ("c.bin" for CPU ROM, "g.bin" for GROM graphics data, or the five
 *  split MAME GROM files "phm3189g3.bin" through "phm3189g7.bin"), or as a
 *  disk image.
 *
 *  If the game data contains a standard Scott Adams header, I have not been
 *  able to find it. Instead, all offsets and counts are hardcoded in
 *  DetectRTPI().
 */

#include <string.h>
#include <stdlib.h>

#include "scott.h"
#include "detect_game.h"
#include "scott_database.h"
#include "uk_loader.h"
#include "us_loader.h"
#include "sagagraphics.h"
#include "unzip_in_mem.h"

#include "rtpi_graphics.h"
#include "rtpi_loader.h"
#include "ti99_disk.h"

#define NUM_GROM_BANKS        5
#define GROM_BANK_SIZE              0x2000

/* Total GROM data size (5 × 0x2000 byte banks). */
#define GROM_SIZE (NUM_GROM_BANKS * GROM_BANK_SIZE)

/* Size of the per-bank header that prefixes each GROM bank, and the
   usable data region that follows it within each bank. */
#define GROM_HEADER_SIZE 0x802
#define GROM_BANK_DATA_SIZE (GROM_BANK_SIZE - GROM_HEADER_SIZE)

/* Usable GROM data size with headers cut out (5 × 0x17fe = 0x77f6). */
#define GROM_FLAT_SIZE (NUM_GROM_BANKS * GROM_BANK_DATA_SIZE)

#define NUM_RTPI_ROOM_IMAGES (24 + 9)
#define NUM_RTPI_ITEM_IMAGES 7
#define RTPI_TITLE_TEXT_LENGTH 0x1e2

/* MAME zip layout splits the GROM across five 0x2000-byte bank files
 named phm3189g3.bin through phm3189g7.bin. */
#define FIRST_GROM_BANK_NUMBER      3
#define RTPI_GROM_FILENAME_BUF_SIZE 14

/* Copy image data from GROM into a newly allocated USImage, handling
   the GROM bank header discontinuity. Each 0x2000-byte GROM bank has
   an 0x802-byte header that must be skipped when an image spans a bank
   boundary. After copying, allocates and links a new (empty) USImage
   node and returns it for the next image to use. */
static USImage *finalize_img_make_new(USImage *image, uint16_t offset, uint16_t size, uint8_t *grom) {

    if (offset >= GROM_SIZE)
        Fatal("Bad image data\n");

    image->imagedata = MemAlloc(size);
    memcpy(image->imagedata, grom + offset, size);

    uint16_t maxrange = offset + size;

    /* If the image spans a GROM bank boundary, the middle portion
       contains a bank header (GROM_HEADER_SIZE bytes) that is not
       image data. Re-copy the tail from past the header to stitch
       the image back together. */
    if (maxrange > GROM_BANK_DATA_SIZE && (maxrange - GROM_BANK_DATA_SIZE) % GROM_BANK_SIZE < size) {
        uint16_t cutoff = size - ((maxrange - GROM_BANK_DATA_SIZE) % GROM_BANK_SIZE);
        if (offset + cutoff + GROM_HEADER_SIZE + size - cutoff >= GROM_SIZE)
            Fatal("Bad image data\n");
        memcpy(image->imagedata + cutoff, grom + offset + cutoff + GROM_HEADER_SIZE, size - cutoff);
    }

    image->datasize = size;

    return AppendImage(image);
}

#define LAST_IMAGE_SIZE 0x72

/* Build a linked list of USImage structs from the GROM data. Room
   images come first (24 rooms + 9 extra scenes), followed by 7 item
   overlay images. Image sizes are generally computed from the gap
   between consecutive offsets, with hardcoded overrides for images
   whose data is not contiguous (17, 22, 26). */
static void LoadRTPIGraphics(uint16_t *room_image_offsets, uint16_t item_image_offsets[NUM_RTPI_ITEM_IMAGES][2], uint8_t *grom) {
    USImages = NewImage();
    USImage *image = USImages;

    for (int i = 0; i < NUM_RTPI_ROOM_IMAGES; i++) {

        if (room_image_offsets[i] == 0)
            continue;

        size_t offset = room_image_offsets[i];

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM;
        image->index = i;
        if (i == 25) {
            /* Image 25 is the Texas Instruments logo / title screen;
               map it to the standard title image index (99). */
            image->index = 99;
        } else if (i == 27) {
            /* Smuggler's hold from below — displayed as an object
               overlay for room 29 (inside of boat). */
            image->index = 29;
            image->usage = IMG_ROOM_OBJ;
        } else if (i == 28) {
            /* Deck at night — displayed as an object overlay for
               room 4 (isle off in distance). */
            image->index = 4;
            image->usage = IMG_ROOM_OBJ;
        }

        size_t size;

        /* Some images span GROM bank headers, so their sizes can't be computed
         from the offset table and must be hardcoded. */
        if (i == 17) {
            size = 0x183;
        } else if (i == 22) {
            size = 0xf7;
        } else if (i == 26) {
            size = 0xdd4;
        } else {
            /* Normal case: walk forward to the next entry whose offset
               is >= ours and use the gap as the size. If we run out of
               room entries, fall back to the first item image offset. */
            int j = i + 1;
            while (j < NUM_RTPI_ROOM_IMAGES && room_image_offsets[j] < offset)
                j++;
            size_t next = (j < NUM_RTPI_ROOM_IMAGES) ? room_image_offsets[j]
                                                           : item_image_offsets[0][1];
            size = next - offset;
        }

        image = finalize_img_make_new(image, offset, size, grom);
    }

    /* Item overlay images. Each entry in item_image_offsets[][] is a pair:
       [0] = index of room image the item overlays, [1] = GROM offset of image
           data.
       Size is computed from the gap to the next item image, except for the
       last one which has a hardcoded size (LAST_IMAGE_SIZE). */
    for (int i = 0; i < NUM_RTPI_ITEM_IMAGES; i++) {
        size_t offset = item_image_offsets[i][1];

        size_t size;
        if (i < NUM_RTPI_ITEM_IMAGES - 1) {
            size = item_image_offsets[i + 1][1] - offset;
        } else {
            size = LAST_IMAGE_SIZE;
        }

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM_OBJ;
        image->index = item_image_offsets[i][0];

        image = finalize_img_make_new(image, offset, size, grom);
    }

    if (image->imagedata == NULL)
        FreeTrailingEmptyImage(image);
}

typedef struct {
    const char *filename;
    USImageType type;
    int index;
    size_t size;
} rtpi_image_record;

static const rtpi_image_record rtpi_disk_image_table[] = {
    { "PICT//O", IMG_ROOM,     26, 0x167 },
    { "PICT1/O", IMG_ROOM,      1, 0x2d5 },
    { "PICT2/O", IMG_ROOM,      2, 0x21c },
    { "PICT3/O", IMG_ROOM,      3, 0x288 },
    { "PICT4/O", IMG_ROOM,      4, 0x1c0 },
    { "PICT5/O", IMG_ROOM,      5, 0x15c },
    { "PICT6/O", IMG_ROOM,      6, 0x25b },
    { "PICT7/O", IMG_ROOM,      7, 0x171 },
    { "PICT:/O", IMG_ROOM,     10, 0x148 },
    { "PICT;/O", IMG_ROOM,     11, 0x0f6 },
    { "PICT</O", IMG_ROOM,     12, 0x233 },
    { "PICT=/O", IMG_ROOM,     13, 0x125 },
    { "PICT?/O", IMG_ROOM,     15, 0x1f0 },
    { "PICT@/O", IMG_ROOM,     16, 0x157 },
    { "PICTA/O", IMG_ROOM,     17, 0x180 },
    { "PICTB/O", IMG_ROOM,     18, 0x27b },
    { "PICTC/O", IMG_ROOM,     19, 0x0ed },
    { "PICTD/O", IMG_ROOM,     20, 0x0e5 },
    { "PICTE/O", IMG_ROOM,     21, 0x079 },
    { "PICT;/O", IMG_ROOM,     22, 0x0f6 },
    { "PICTG/O", IMG_ROOM,     23, 0x29d },
    { "PICTH/O", IMG_ROOM,     24, 0x2a2 },
    { "PICB5/O", IMG_ROOM_OBJ,  4, 0x0d5 },
    { "PICTI/O", IMG_ROOM,     99, 0x2b0 },
    { "PICTK/O", IMG_ROOM_OBJ, 29, 0x0df },
    { "PICB9/O", IMG_ROOM,     30, 0x065 },
    { "PICBA/O", IMG_ROOM,     31, 0x065 },
    { "PICB1/O", IMG_ROOM_OBJ, 63, 0x181 },
    { "PICB2/O", IMG_ROOM_OBJ, 26, 0x0f9 },
    { "PICB3/O", IMG_ROOM_OBJ, 28, 0x0b0 },
    { "PICB4/O", IMG_ROOM_OBJ, 36, 0x061 },
    { "PICB6/O", IMG_ROOM_OBJ, 27, 0x198 },
    { "PICB7/O", IMG_ROOM_OBJ,  7, 0x061 },
    { "PICB8/O", IMG_ROOM_OBJ, 52, 0x072 },
    { "PICBB/O", IMG_ROOM_OBJ, 31, 0x036 },
    { "PICBE/O", IMG_ROOM,     33, 0x01e },
    { "PICBF/O", IMG_ROOM,     34, 0x024 },
    { "PICBG/O", IMG_ROOM,     35, 0x026 },
    { "PICBH/O", IMG_ROOM,     36, 0x01c },
    { "PICBI/O", IMG_ROOM,     37, 0x01c },
    {NULL, 0, 0, 0}
};

/* A picture file loads at an address of its own choosing (0x2700 on
   the known disks) and begins with a word giving the length of what
   follows. */
#define DSK_PICTURE_LEADER 2

/* The "ADV*RPI*CM" disk image has a damaged record in the middle of
   room 5's picture: a full record of 24 words, of which the first ten
   can be read. These are the other fourteen. */
#define ROOM_5_PATCH_OFFSET 0xfc
static const uint8_t rtpi_room_5_tile_patch[28] = {
    0xC4, 0x46, 0x4C, 0x00, 0xE0,
    0x05, 0x0F, 0x0C, 0xF5, 0x06, 0x5F, 0x03, 0x1F, 0x02, 0x2F, 0x04, 0x02,
    0x05, 0x0F, 0x06, 0xF5, 0x06, 0x15, 0x04, 0xF5, 0x05, 0x2F, 0x06
};

static int LoadRTPIGraphicsFromDSK(const uint8_t *dsk, size_t size, const ti99_disk_file *files) {
    USImages = NewImage();
    USImage *image = USImages;

    /* Walk the known image table and look up each entry on the disk */
    for (int i = 0; rtpi_disk_image_table[i].filename != NULL; i++) {
        const rtpi_image_record *rec = &rtpi_disk_image_table[i];

        ti99_object obj;
        if (!TI99LoadObject(dsk, size, files, rec->filename, &obj))
            continue;

        uint8_t *imagedata = TI99CopyObjectBytes(&obj, obj.low + DSK_PICTURE_LEADER, rec->size);
        int damaged = obj.damaged;
        TI99FreeObject(&obj);
        if (imagedata == NULL)
            continue;

        /* The data lost with the damaged record is a hole of zeros in
           the middle of the picture; the records after it say where
           they load, so the rest is in place. */
        if (damaged && rec->type == IMG_ROOM && rec->index == 5 &&
            rec->size >= ROOM_5_PATCH_OFFSET + sizeof(rtpi_room_5_tile_patch)) {
            memcpy(imagedata + ROOM_5_PATCH_OFFSET, rtpi_room_5_tile_patch, sizeof(rtpi_room_5_tile_patch));
        }

        image->systype = SYS_TI994A;
        image->usage = rec->type;
        image->index = rec->index;
        image->datasize = rec->size;
        image->imagedata = imagedata;

        /* Append a new node for the next image */
        image = AppendImage(image);
    }

    if (image->imagedata == NULL)
        FreeTrailingEmptyImage(image);

    return 1;
}

/* Sanity bound for word_length to prevent runaway parsing if the
   header value is bogus. */
#define RTPI_DICT_MAX_WORD_LENGTH 1000

/* Storage for one parsed dictionary word (one byte reserved for NUL). */
#define RTPI_DICT_WORD_BUF_SIZE 32

/* Trailing placeholder slots in Verbs[]/Nouns[] beyond number_of_words. */
#define RTPI_DICT_PADDING 2

/* End-of-dictionary marker: high bit set on the final byte. */
#define RTPI_DICT_END_THRESHOLD 0x7F

/* Parse the dictionary from the GROM data stream into the global
   Nouns[] and Verbs[] arrays.

   The dictionary is a flat byte stream of fixed-width words
   (info.word_length chars each). Nouns come first, then verbs. A '*'
   character marks a synonym: it restarts the current word entry so
   the synonym shares the same word number (the '*' prefix is preserved
   in the stored string, as the game engine uses it to identify synonyms).
   Trailing spaces within a word are collapsed. A byte with bit 7 set
   signals the end of the dictionary. */
static uint8_t *ReadRTPIDictionary(const GameInfo *info, uint8_t *ptr, uint8_t *endptr) {
    int wl = info->word_length;
    if (wl < 1 || wl > RTPI_DICT_MAX_WORD_LENGTH)
        Fatal("Bad word length");

    char dictword[RTPI_DICT_WORD_BUF_SIZE];

    int nw = info->number_of_words;
    int nv = info->number_of_verbs;
    int nn = info->number_of_nouns;

    /* Initialize all slots to "." (the Scott Adams placeholder for
       empty/unused dictionary entries). */
    for (int i = 0; i < nw + RTPI_DICT_PADDING; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    for (int wordnum = 0; wordnum <= nv + nn; wordnum++) {
        int charindex = 0;
        int restarts = 0;
        char c = 0;
        for (int i = 0; i < wl; i++) {
            if (ptr >= endptr)
                return ptr;
            c = *ptr++;

            /* Skip leading NUL bytes (padding between entries). */
            if (c == 0 && charindex == 0) {
                if (ptr >= endptr)
                    return ptr;
                c = *ptr++;
            }
            /* Collapse trailing spaces: if the previous char was a space
               and the current one isn't, back up to overwrite the space. */
            if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                i--;
                charindex--;
            }
            /* '*' marks a synonym — reset the word buffer and restart
               reading so this entry replaces the previous one at the
               same word number. The '*' prefix is preserved. */
            if (c == '*') {
                charindex = 0;
                i = -1;
                if (++restarts > wl)
                    break;
            }
            if (charindex < RTPI_DICT_WORD_BUF_SIZE - 1)
                dictword[charindex++] = c;
        }
        dictword[charindex] = 0;
        char *word = MemAlloc(charindex + 1);
        memcpy(word, dictword, charindex + 1);
        if (wordnum < nn) {
            Nouns[wordnum] = word;
            debug_print("Noun %d: \"%s\"\n", wordnum, word);
        } else if (wordnum - nn < nw + RTPI_DICT_PADDING) {
            Verbs[wordnum - nn] = word;
            debug_print("Verb %d: \"%s\"\n", wordnum - nn, word);
        }

        /* A byte with the high bit set terminates the dictionary. */
        if (c > RTPI_DICT_END_THRESHOLD)
            return ptr;
    }

    return ptr;
}

/* Load all game data (dictionary, rooms, messages, items, actions,
   room connections) from the GROM data for Return to Pirate's Isle.

   The data is at absolute offsets within the GROM image (specified in
   the GameInfo struct). Each section is parsed sequentially, with
   bounds checks between sections. Returns the game ID on success, or
   UNKNOWN_GAME if any section extends past the data boundary. */
static GameIDType LoadRTPI(uint8_t *data, size_t length, GameInfo info)
{
    uint8_t *ptr;
    uint8_t *endptr = data + length;

/* MARK: dictionary */

    ptr = data + info.start_of_dictionary;
    if (ptr >= endptr)
        return FreeGameResources();

    ReadRTPIDictionary(&info, ptr, endptr);

/* MARK: rooms */

    ptr = data + info.start_of_room_descriptions;
    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseRooms(ptr, endptr, GameHeader.NumRooms);

/* MARK: messages */

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseMessages(ptr, endptr, GameHeader.NumMessages);

/* MARK: items */

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseItems(ptr, endptr, GameHeader.NumItems, 1);

/* MARK: actions */

    ptr += 0x90;

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseActions(ptr, data, length, GameHeader.NumActions, 1);
    if (ptr == NULL)
        return FreeGameResources();

    ptr += 0x32;

    if (ptr >= endptr)
        return FreeGameResources();

/* MARK: room connections */

    ParseRoomConnections(ptr, endptr, GameHeader.NumRooms, 1);

    return info.gameID;
}

/* Translate a TI-99/4A GROM address into a file offset.

   In the original hardware, GROM addresses start at 0x6000. The file
   image starts at offset 0, so we subtract 0x6000 from any address
   greater than or equal to 0x8000, which are in the range the game uses.
   Addresses below 0x8000 are left unchanged. */
static inline uint16_t adjust_grom_offset(uint16_t offset) {
    return (offset >= 0x8000) ? offset - 0x6000 : offset;
}

/* To make the game data contiguous, we cut out all the grom headers
   and arrange the banks in order 1, 2, 3, 4, 0. The output buffer
   is always GROM_FLAT_SIZE (0x77f6) bytes. */
static uint8_t *CutOutGromHeaders(uint8_t *grom) {
    uint8_t *result = MemAlloc(GROM_FLAT_SIZE);

    for (size_t i = 0; i < NUM_GROM_BANKS; i++) {
        /* Bank order 1, 2, 3, 4, 0. */
        size_t src_bank = (i + 1) % NUM_GROM_BANKS;
        memcpy(result + i * GROM_BANK_DATA_SIZE, grom + src_bank * GROM_BANK_SIZE, GROM_BANK_DATA_SIZE);
    }

    free(grom);
    return result;
}

/* Byte offsets within the c.bin (CPU ROM) file where the image offset
   tables begin. ROOM_IMAGES_OFFSET points to 33 big-endian 16-bit
   GROM addresses (one per room image). ITEM_IMAGES_OFFSET points to
   7 pairs of (room_index, GROM_address), each 4 bytes. */
#define ROOM_IMAGES_OFFSET 0x12
#define ITEM_IMAGES_OFFSET 0x54

/* Minimum c.bin size for all of the embedded tables, title text and
   tile font to fit. */
#define CPU_ROM_MIN_SIZE          0x1e94

/* Byte offsets within the c.bin (CPU ROM) of additional payloads. */
#define CPU_ROM_TITLE_TEXT_OFFSET 0x13db
#define CPU_ROM_TILE_FONT_OFFSET  0x1cf4

/* Each ITEM_IMAGES_OFFSET entry is 4 bytes: a 2-byte room_index
   followed by a 2-byte GROM address. */
#define ITEM_IMAGE_ENTRY_SIZE 4

/* Number of int fields in the synthetic Scott Adams header. */
#define RTPI_RAW_HEADER_SIZE 25

/* Disk-image (.dsk) distribution layout: game data is stored as
   DSK_RECORD_SIZE-byte records on DSK_SECTOR_SIZE-byte sectors,
   starting at DSK_GROM_SRC_START in the disk image and placed
   into the rebuilt GROM buffer starting at DSK_GROM_DST_START.
   The title screen and tile font are data in the object files of two
   of the program's modules, at these addresses within them. */
#define DSK_GROM_DST_START    0x3d79
#define DSK_GROM_DST_END      0x76f6
#define DSK_GROM_SRC_START    0x2200
#define DSK_RECORD_SIZE       0xfe
#define DSK_SECTOR_SIZE       0x100
#define DSK_TITLE_TEXT_FILE    "ADV5/O"
#define DSK_TITLE_TEXT_ADDRESS 0x01e4
#define DSK_TILE_FONT_FILE     "ADV6/O"
#define DSK_TILE_FONT_ADDRESS  0x0370

/* GameInfo describing the GROM layout. The offsets are raw byte
   positions within the GROM file. Graphics data is handled
   separately via LoadRTPIGraphics, so the image-related fields
   are all zero. */
static const GameInfo rtpi_info = {
    .Title = "Return to Pirate's Isle",
    .gameID = RETURN_TO_PIRATES_ISLE,
    .type = US_VARIANT,
    .subtype = ENGLISH,
    .dictionary = FOUR_LETTER_UNCOMPRESSED,

    .number_of_items = 71,
    .number_of_actions = 277,
    .number_of_words = 104,
    .number_of_rooms = 26,
    .max_carried = 10,
    .word_length = 4,
    .number_of_messages = 89,

    .number_of_verbs = 103,
    .number_of_nouns = 105,

    .start_of_actions = 0x4F6C,
    .actions_style = UNCOMPRESSED,
    .start_of_dictionary = 0x3d98,
    .start_of_room_descriptions = 0x41B1,
    .start_of_room_connections = 0x60fE,
    .start_of_messages = 0x438D,
    .start_of_item_descriptions = FOLLOWS,
    .start_of_item_locations = FOLLOWS,
};

/* Replace embedded NULs with newlines so a multi-line blob can be
   displayed as a single string, then NUL-terminate at the end. */
static void replace_nuls_with_newlines(char *str, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (str[i] == 0)
            str[i] = '\n';
    }
    str[length - 1] = '\0';
}

/* Load the zip-archive distribution: extract c.bin and g.bin (or
   the split MAME phm3189*.bin files), parse the image offset
   tables, extract title text and tile font, load image data, and
   produce the contiguous GROM buffer. Returns NULL on failure. */
static uint8_t *load_rtpi_zip_payload(uint8_t *data, size_t datalength) {
    size_t rom_length;

    /* Try to extract the CPU ROM, accepting either filename variant. */
    uint8_t *cpu_rom = extract_file_from_zip_data(data, datalength, "c.bin", &rom_length);
    if (!cpu_rom) {
        cpu_rom = extract_file_from_zip_data(data, datalength, "phm3189c.bin", &rom_length);
        if (!cpu_rom)
            return NULL;
    }

    if (rom_length < CPU_ROM_MIN_SIZE) {
        free(cpu_rom);
        return NULL;
    }

    /* Extract room and item image offset tables from the CPU ROM.
       Room offsets are 33 big-endian 16-bit GROM addresses at 0x12.
       Item offsets are 7 (room_index, GROM_address) pairs at 0x54. */
    uint16_t room_image_offsets[NUM_RTPI_ROOM_IMAGES];
    uint16_t item_image_offsets[NUM_RTPI_ITEM_IMAGES][2];

    for (int i = 0; i < NUM_RTPI_ROOM_IMAGES; i++) {
        room_image_offsets[i] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ROOM_IMAGES_OFFSET + i * 2));
    }
    for (int i = 0; i < NUM_RTPI_ITEM_IMAGES; i++) {
        item_image_offsets[i][0] = READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * ITEM_IMAGE_ENTRY_SIZE);
        item_image_offsets[i][1] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * ITEM_IMAGE_ENTRY_SIZE + 2));
    }

    /* Extract the title screen text. */
    title_screen = MemAlloc(RTPI_TITLE_TEXT_LENGTH);
    memcpy((char *)title_screen, cpu_rom + CPU_ROM_TITLE_TEXT_OFFSET, RTPI_TITLE_TEXT_LENGTH);
    replace_nuls_with_newlines((char *)title_screen, RTPI_TITLE_TEXT_LENGTH);

    /* Extract the 52-tile font (8 bytes per tile). */
    memcpy(rtpi_tile_font, cpu_rom + CPU_ROM_TILE_FONT_OFFSET, NUM_RTPI_TILES * 8);

    free(cpu_rom);

    /* Load the GROM data. Try the single g.bin file first. */
    size_t grom_length;
    uint8_t *grom_data = extract_file_from_zip_data(data, datalength, "g.bin", &grom_length);

    /* If g.bin is not present, this may be the MAME zip format which
       splits the GROM across NUM_GROM_BANKS bank files (g3–g7). */
    if (!grom_data) {
        grom_data = MemCalloc(GROM_SIZE);
        char filename[RTPI_GROM_FILENAME_BUF_SIZE];
        size_t bank_length;
        for (int i = 0; i < NUM_GROM_BANKS; i++) {
            snprintf(filename, RTPI_GROM_FILENAME_BUF_SIZE, "phm3189g%d.bin", FIRST_GROM_BANK_NUMBER + i);
            uint8_t *bank = extract_file_from_zip_data(data, datalength, filename, &bank_length);
            if (!bank) {
                free(grom_data);
                return NULL;
            }
            memcpy(grom_data + GROM_BANK_SIZE * i, bank, bank_length);
            free(bank);
        }
    }

    /* Image positions are relative to the raw banks, so copy them
       out before CutOutGromHeaders re-arranges the buffer. */
    LoadRTPIGraphics(room_image_offsets, item_image_offsets, grom_data);
    return CutOutGromHeaders(grom_data);
}

/* Load the disk-image distribution: rebuild the GROM buffer from
   on-disk records and extract title text and tile font. */
static uint8_t *load_rtpi_disk_payload(uint8_t *data, size_t datalength) {
    uint8_t *grom_data = MemAlloc(GROM_FLAT_SIZE);
    size_t dst_off = DSK_GROM_DST_START;
    size_t src_off = DSK_GROM_SRC_START;
    while (dst_off < DSK_GROM_DST_END) {
        memcpy(grom_data + dst_off, data + src_off, DSK_RECORD_SIZE);
        src_off += DSK_SECTOR_SIZE;
        dst_off += DSK_RECORD_SIZE;
    }

    /* Without a directory there is still the game, with no pictures. */
    ti99_disk_file *files = TI99ReadDirectory(data, datalength);
    if (files == NULL)
        return grom_data;

    /* The title text and the tile font are data in two of the
       program's modules. */
    ti99_object obj;
    if (TI99LoadObject(data, datalength, files, DSK_TITLE_TEXT_FILE, &obj)) {
        title_screen = (char *)TI99CopyObjectBytes(&obj, DSK_TITLE_TEXT_ADDRESS, RTPI_TITLE_TEXT_LENGTH);
        TI99FreeObject(&obj);
        if (title_screen != NULL)
            replace_nuls_with_newlines(title_screen, RTPI_TITLE_TEXT_LENGTH);
    }

    if (TI99LoadObject(data, datalength, files, DSK_TILE_FONT_FILE, &obj)) {
        uint8_t *flat_tiles = TI99CopyObjectBytes(&obj, DSK_TILE_FONT_ADDRESS, NUM_RTPI_TILES * 8);
        TI99FreeObject(&obj);
        if (flat_tiles != NULL) {
            memcpy(rtpi_tile_font, flat_tiles, NUM_RTPI_TILES * 8);
            free(flat_tiles);
        }
    }

    LoadRTPIGraphicsFromDSK(data, datalength, files);
    TI99FreeDirectory(files);
    return grom_data;
}

/* Detect and load the TI-99/4A "Return to Pirate's Isle" game.
   Supports the zip-archive distribution (c.bin + g.bin or the
   MAME split phm3189*.bin layout) and the .dsk disk image.

   On success, all game data structures are populated and the
   function returns RETURN_TO_PIRATES_ISLE. On failure (missing
   files, bad data) it returns UNKNOWN_GAME. */
GameIDType DetectRTPI(uint8_t *data, size_t datalength) {
    int is_disk = !memcmp(entire_file, "PIRATE", 6) || !memcmp(entire_file, "ADV*RPI*CM", 10) || !memcmp(entire_file, "RTNPIRAT", 8);

    uint8_t *grom_data = is_disk ? load_rtpi_disk_payload(data, datalength)
                                 : load_rtpi_zip_payload(data, datalength);
    if (!grom_data)
        return UNKNOWN_GAME;

    /* I have been unable to find a standard Scott Adams header in
       the data, so all game parameters are hardcoded here to match
       those in the adv14a.dat file created by Paul David Doherty
       (likely created by analyzing the same game data we are
       dealing with here.)
       See https://unbox.ifarchive.org/?url=/if-archive/scott-adams/games/scottfree/AdamsGames.zip
     */
    int raw_header[RTPI_RAW_HEADER_SIZE] = {
        4,       /* Word length */
        104,     /* Number of words */
        277,     /* Number of actions */
        71,      /* Number of items */
        89,      /* Number of messages */
        24,      /* Number of rooms */
        10,      /* Max carried */
        1,       /* Starting location */
        13,      /* Number of treasures */
        1,       /* Light time */
        13 << 8, /* Treasure room */
    };

    Header hdr;
    ParseHeader(raw_header, US_HEADER, &hdr);
    SetGameHeader(&hdr);
    /* Zero-filled, and never short of Items[LIGHT_SOURCE]: see
       AllocateGameData(). The dictionary padding matches RTPI_DICT_PADDING. */
    AllocateGameData();
    PrintHeaderInfo(raw_header, &hdr);

    /* Parse all game data from the GROM, then set up the TI-99/4A
       colour table. The original zip data is freed here since
       it's no longer needed once the GROM has been loaded. */
    if (LoadRTPI(grom_data, GROM_FLAT_SIZE, rtpi_info) == RETURN_TO_PIRATES_ISLE) {
        free(data);
        free(grom_data);
        SetupRTPIColors();
        return RETURN_TO_PIRATES_ISLE;
    }

    free(grom_data);
    return UNKNOWN_GAME;
}

/* Override the default Scott Adams system messages with the text used
   by the TI-99/4A version of Return to Pirate's Isle. These differ
   from the standard messages in wording and punctuation. */
void UpdateRTPISystemMessages(void) {
    sys[TOO_DARK_TO_SEE] = "I can't SEE!\n";
    sys[YOU_SEE] = ". Some visible items: ";
    sys[EXITS] = "Obvious exits: ";
    sys[YOU_FELL_AND_BROKE_YOUR_NECK] = "Fell & Thanks a lot; I'm Dead! ";
    sys[DANGEROUS_TO_MOVE_IN_DARK] = "Dangerous move! ";
    sys[WHAT_NOW] = "-> Command me: ";
    sys[YOUVE_SOLVED_IT] = "Congrats! You did it!\n";
}
