//
//  decompress_vga.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-03.
//
//  Based on drilbo-mg1.h from Fizmo by Christoph Ender,
//  which in turn is based on Mark Howell's pix2gif utility

#include <stdlib.h>
#include <string.h>

#include "decompress_vga.hpp"

#define CODE_SIZE 8
#define CODE_TABLE_SIZE 4096
#define PREFIX 0
#define PIXEL 1

static const short mask[16] = {
    0x0000, 0x0001, 0x0003, 0x0007,
    0x000f, 0x001f, 0x003f, 0x007f,
    0x00ff, 0x01ff, 0x03ff, 0x07ff,
    0x0fff, 0x1fff, 0x3fff, 0x7fff
};

typedef struct compress_s {
    short next_code;
    short slen;
    short sptr;
    short tlen;
    short tptr;
} compress_t;


static int16_t read_code(uint8_t **ptr, size_t bytes_remaining, compress_t *comp, unsigned char *code_buffer)
{
    short code, bsize, tlen, tptr;

    code = 0;
    tlen = comp->tlen;
    tptr = 0;

    while (tlen)
    {
        if (comp->slen == 0)
        {
            size_t toread = CODE_TABLE_SIZE;
            if (bytes_remaining < toread) {
                toread = bytes_remaining;
            }
            memcpy(code_buffer, *ptr, toread);
            (*ptr) += toread;
            bytes_remaining -= toread;
            comp->slen = toread * 8;
            comp->sptr = 0;
        }

        bsize = ((comp->sptr + 8) & ~7) - comp->sptr;
        bsize = (tlen > bsize) ? bsize : tlen;
        code |= (((unsigned int) code_buffer[comp->sptr >> 3] >> (comp->sptr & 7)) & (unsigned int)mask[bsize]) << tptr;

        tlen -= bsize;
        tptr += bsize;
        comp->slen -= bsize;
        comp->sptr += bsize;
    }

    if ((comp->next_code == mask[comp->tlen]) && (comp->tlen < 12))
        comp->tlen++;

    return code;
}


uint8_t *decompress_vga(ImageStruct *image) {
    int i;
    uint8_t *ptr;
    short code, old = 0, first, clear_code;
    compress_t comp;
    unsigned char code_buffer[CODE_TABLE_SIZE];

    size_t final_size = image->width * image->height;
    uint8_t *result = (uint8_t *)malloc(final_size);
    if (result == nullptr)
        exit(1);

    clear_code = 1 << CODE_SIZE;
    comp.next_code = clear_code + 2;
    comp.slen = 0;
    comp.sptr = 0;
    comp.tlen = CODE_SIZE + 1;
    comp.tptr = 0;

    uint8_t buffer[CODE_TABLE_SIZE];
    uint16_t code_table[CODE_TABLE_SIZE][2];

    for (i = 0; i < CODE_TABLE_SIZE; i++) {
        code_table[i][PREFIX] = CODE_TABLE_SIZE;
        code_table[i][PIXEL] = (uint16_t)i;
    }

    int j = 0;
    ptr = image->data;
    for (;;) {
        if ((code = read_code(&ptr, image->datasize - (ptr - image->data), &comp, code_buffer)) == (clear_code + 1))
            break;

        if (code == clear_code) {
            comp.tlen = CODE_SIZE + 1;
            comp.next_code = clear_code + 2;
            code = read_code(&ptr, image->datasize - (ptr - image->data), &comp, code_buffer);
        } else {
            first = (code == comp.next_code) ? old : code;

            while (code_table[first][PREFIX] != CODE_TABLE_SIZE)
                first = code_table[first][PREFIX];

            code_table[comp.next_code][PREFIX] = old;
            code_table[comp.next_code++][PIXEL] = code_table[first][PIXEL];
        }
        old = code;

        i = 0;
        do
            buffer[i++] = (unsigned char) code_table[code][PIXEL];
        while ((code = code_table[code][PREFIX]) != CODE_TABLE_SIZE);

        do {
            result[j++] = buffer[--i];
        } while (i > 0);
    }
    return result;
}
