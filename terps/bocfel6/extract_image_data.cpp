//
//  extract_image_data.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-17.
//
//  Parts are based on Mark Howell's pix2gif utility


#include <stdlib.h>
#include <string.h>

extern "C" {
#include "glkimp.h"
#include "gi_blorb.h"
}

#include "types.h"
#include "image.h"
#include "draw_png.h"

#include "extract_image_data.hpp"

#define    EXIT_FAILURE    1
#define    EXIT_SUCCESS    0

typedef struct header_s {
    unsigned char part;
    unsigned char flags;
    unsigned short unknown1;
    unsigned short images;
    unsigned short unknown2;
    unsigned char dir_size;
    unsigned char unknown3;
    unsigned short checksum;
    unsigned short unknown4;
    unsigned short version;
} header_t;

typedef struct pdirectory_s {
    uint16_t image_number;
    uint16_t image_width;
    uint16_t image_height;
    uint16_t image_flags;
    uint32_t image_data_addr;
    uint32_t next_data_addr;
    uint32_t image_cm_addr;
    uint32_t image_lookup_addr;
} pdirectory_t;


static unsigned char read_byte (uint8_t **ptr)
{
    int c;

    c = **ptr;
    *ptr += 1;

    return ((unsigned char) c);

}/* read_byte */

static unsigned short read_word (uint8_t **ptr)
{
    unsigned int w;

    w = (unsigned int) read_byte (ptr) << 8;
    w += (unsigned int) read_byte (ptr);

    return (w);
}/* read_word */

static uint16_t byte_swap(bool swap, uint16_t n)
{
    if (swap)
        return ((n>>8)|((n&255)<<8));
    return n;
}

static uint32_t read_24bit(uint8_t **in)
{
    int lower, middle, upper;

    if ((upper = read_byte(in)) == -1)
    { return -1; }

    if ((middle = read_byte(in)) == -1)
    { return -1; }

    if ((lower = read_byte(in)) == -1)
    { return -1; }

    return (uint32_t)((lower & 0xff) | ((middle & 0xff) << 8) | ((upper & 0xff) << 16));
}

static uint8_t *myalloc(size_t size)
{
    uint8_t *t = (uint8_t *)malloc(size);
    if (t == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return (t);
}

uint8_t lookup_table[256];

int extract_images(uint8_t *data, size_t datasize, int disk, off_t offset, ImageStruct **image_list, int *version, GraphicsType *type)
{
    int i;
    uint8_t *ptr = data;
    header_t imgheader;
    pdirectory_t *directory;

    ptr = data + offset;
    imgheader.part = read_byte(&ptr);
    *image_list = NULL;

    if (imgheader.part != disk) {
        // Wrong part
        return 0;
    }

    bool byteswap = (*type == kGraphicsTypeVGA || *type == kGraphicsTypeEGA || *type == kGraphicsTypeCGA);

    imgheader.flags = read_byte(&ptr);

    // A file named "pic.data" may contain colour Amiga graphics or monochrome Mac graphics,
    // but the flags always seem to equal 0xe (14) if the graphics are monochrome.
    // (Amiga graphics data and Mac colour graphics data are identical.)
    if (imgheader.flags == 0xe) {
        if (*type == kGraphicsTypeAmiga)
            *type = kGraphicsTypeMacBW;
    } else if (*type == kGraphicsTypeMacBW) {
        *type = kGraphicsTypeAmiga;
    }
    imgheader.unknown1 = byte_swap(byteswap, read_word(&ptr));
    imgheader.images = byte_swap(byteswap, read_word(&ptr));
    imgheader.unknown2 = byte_swap(byteswap, read_word (&ptr));
    imgheader.dir_size = read_byte (&ptr);
    imgheader.unknown3 = read_byte (&ptr);
    imgheader.checksum = byte_swap(byteswap, read_word(&ptr));
    imgheader.unknown4 = byte_swap(byteswap, read_word(&ptr));
    imgheader.version = byte_swap(byteswap, read_word(&ptr));

    if ((directory = (pdirectory_t *) calloc((size_t) imgheader.images, sizeof (pdirectory_t))) == NULL) {
        fprintf (stderr, "Insufficient memory\n");
        exit (EXIT_FAILURE);
    }

    for (i = 0; i < imgheader.images; i++) {
        directory[i].image_number = byte_swap(byteswap, read_word(&ptr));
        if (imgheader.dir_size == 8) {
            directory[i].image_width = read_byte(&ptr);
            directory[i].image_height = read_byte(&ptr);
            directory[i].image_flags = read_byte(&ptr);
        } else {
            directory[i].image_width = byte_swap(byteswap, read_word(&ptr));
            directory[i].image_height = byte_swap(byteswap, read_word(&ptr));
            directory[i].image_flags = byte_swap(byteswap, read_word(&ptr));
        }

        directory[i].image_data_addr = read_24bit(&ptr);

        if (directory[i].image_data_addr != 0) {
            directory[i].next_data_addr = (uint32_t)datasize;
        }
        if ((unsigned int) imgheader.dir_size == 14) {
            directory[i].image_cm_addr = read_24bit(&ptr);
        } else if (imgheader.dir_size == 16 ){
            directory[i].image_cm_addr = read_24bit(&ptr);
            // lookup table offset must be doubled
            directory[i].image_lookup_addr = read_word(&ptr) * 2;
        } else if (imgheader.dir_size != 8) {
            directory[i].image_cm_addr = 0;
            read_byte(&ptr);
        }
    }

    memcpy(lookup_table, ptr, 256);

    // Ugly calculation of data size by looking for following image offset
    for (i = 0; i < imgheader.images - 1; i++) {
        if (directory[i].image_data_addr != 0) {
            for (int j = i + 1; j < imgheader.images - 1; j++) {
                if (directory[j].image_data_addr > directory[i].image_data_addr) {
                    directory[i].next_data_addr = directory[j].image_data_addr;
                    break;
                }
            }
        }
    }

    *image_list = (ImageStruct *)calloc(imgheader.images, sizeof(ImageStruct));
    if (*image_list == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < imgheader.images; i++) {
        (*image_list)[i].index = directory[i].image_number;
        (*image_list)[i].width = directory[i].image_width;
        (*image_list)[i].height = directory[i].image_height;
        (*image_list)[i].type = *type;

        if (directory[i].image_data_addr == 0 || directory[i].image_width == 0 || directory[i].image_height == 0)
            continue;

        uint32_t length = directory[i].next_data_addr - directory[i].image_data_addr;

        (*image_list)[i].datasize = length;
        (*image_list)[i].data = myalloc(length);

        memcpy((*image_list)[i].data, data + directory[i].image_data_addr, length);
        if (directory[i].image_flags & 1) {
            (*image_list)[i].transparency = 1;
            if (imgheader.dir_size == 8) {
                (*image_list)[i].transparent_color = directory[i].image_flags >> 4;
            } else {
                (*image_list)[i].transparent_color = directory[i].image_flags >> 12;
            }
        }

        if (directory[i].image_lookup_addr != 0) {
            (*image_list)[i].huffman_tree = myalloc(256);
            memcpy((*image_list)[i].huffman_tree, data + directory[i].image_lookup_addr, 256);
        } else {
            (*image_list)[i].huffman_tree = lookup_table;
        }

        if (directory[i].image_cm_addr != 0) {
            (*image_list)[i].palette = myalloc(512);
            memcpy((*image_list)[i].palette, data + directory[i].image_cm_addr, 512);
        }
    }

    free(directory);
    *version = imgheader.version;

    return imgheader.images;
}

int extract_images_from_blorb(ImageStruct **image_list)
{

    giblorb_map_t *map = giblorb_get_resource_map();

    if (map == nullptr) {
        return 0;
    }

    glui32 number_of_images, first, last;
    giblorb_count_resources(map, giblorb_ID_Pict,
                            &number_of_images, &first, &last);
    if (number_of_images == 0)
        return 0;

    *image_list = (ImageStruct *)calloc(number_of_images, sizeof(ImageStruct));
    if (*image_list == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    giblorb_result_t res;
    int index = 0;

    for (int i = first; i <= last; i++) {
        if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, i) == giblorb_err_None) {
            glui32 width, height;
            win_purgeimage(i, 0, 0);
            glk_image_get_info(i, &width, &height);
            (*image_list)[index].index = i;
            (*image_list)[index].type = kGraphicsTypeBlorb;
            (*image_list)[index].width = width;
            (*image_list)[index].height = height;
            if (res.data.ptr != nullptr && res.length > 8) {
                (*image_list)[index].data = (uint8_t *)res.data.ptr;
                (*image_list)[index].datasize = res.length;
                (*image_list)[index].palette = extract_palette_from_png_data((*image_list)[index].data, (*image_list)[index].datasize);
            }
            index++;
        }
    }

    if (giblorb_load_chunk_by_type(map,
                                   giblorb_method_Memory, &res, giblorb_make_id('A', 'P', 'a', 'l'),
                                   0) == giblorb_err_None) {
        for (int i = 0; i < res.length; i += 4) {
            int adaptive_image = *((uint8_t *)res.data.ptr + i + 3) + *((uint8_t *)res.data.ptr + i + 2) * 0x100;
            for (int j = 0; j < number_of_images; j++) {
                if ((*image_list)[j].index == adaptive_image) {
                    if ((*image_list)[j].palette != nullptr) {
                        free((*image_list)[j].palette);
                        (*image_list)[j].palette = nullptr;
                    }
                    break;
                }
            }
        }
    }
    return number_of_images;
}
