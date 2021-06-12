
/* neo.c
 *
 * Converts Atari ST Neochrome image files to the z_image intermediary
 * format.
 *
 * This file is based on code from NEOLoad by Jason "Joefish" Railton
 * (See https://www.atari-forum.com/viewtopic.php?t=29650)
 * and drilbo-mg1.h, part of Fizmo
 *
 * Copyright (c) 2021 Petter Sjölund.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 
 * This module is an adapted version of Mark Howell's pix2gif utility from
 * the ztools package version 7.3.1, available from:
 * http://www.ifarchive.org/if-archive/infocom/tools/ztools/ztools731.tar.gz
 * 
 */

#ifndef neo_c_INCLUDED
#define neo_c_INCLUDED

#include "neo.h"

#define IMAGE_TYPE_RGB 1
#define IMAGE_TYPE_GRAYSCALE 2

struct neo_color {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

static FILE *neo_file = NULL;
uint8_t * imgPic = NULL;
z_image *neo_zimage = NULL;


z_image * get_neo_picture(char *neo_filename)
{
    int               iLoadSize;
    unsigned char     bNeoPic[32768];
    struct neo_color  colNeoPalette[16];
    int               iN;
    int               iPtr, iInk;
    uint8_t           iR, iG, iB;
    int               iX, iY, iZ;
    unsigned int      uW0, uW1, uW2, uW3;
    unsigned int      uBit;

    if ((neo_file = fopen(neo_filename, "rb"))
        == NULL)
        return NULL;

    // Load 32128 bytes:
    iLoadSize = fread(bNeoPic, 1, 32128, neo_file);

    if (iLoadSize != 32128) {
        return NULL;
    }

    if (imgPic != NULL)
        free(imgPic);

    imgPic = malloc(3 * 320 * 200);
    if (!imgPic)
        return NULL;

    // Skip two words (Flag byte=0, assume resolution=0)
    // Extract the palette from the next 16 words, and
    // convert into a temporary array of TColor:
    for (iN=0; iN<16; ++iN)
    {
        iR = (uint8_t)((bNeoPic[iN*2 + 4] & 0x07) * 255.0 / 7.0);
        iG = (uint8_t)((bNeoPic[iN*2 + 5] & 0x70) * 255.0 / 7.0 / 16.0);
        iB = (uint8_t)((bNeoPic[iN*2 + 5] & 0x07) * 255.0 / 7.0);

        colNeoPalette[iN] = (struct neo_color){.red = iR, .green = iG, .blue = iB};
    }

    // Address pointer, past 128b header:
    iPtr = 128;

    size_t offset = 0;

    // 200 rows of image:
    for (iY=0; iY<200; ++iY)
    {
        // 20 column blocks:
        for (iX=0; iX<20; ++iX)
        {
            // Fetch the 4 words that make up the
            // next 16 pixels across 4 bitplanes:
            uW0 = bNeoPic[iPtr+0] * 256 + bNeoPic[iPtr+1];
            uW1 = bNeoPic[iPtr+2] * 256 + bNeoPic[iPtr+3];
            uW2 = bNeoPic[iPtr+4] * 256 + bNeoPic[iPtr+5];
            uW3 = bNeoPic[iPtr+6] * 256 + bNeoPic[iPtr+7];

            // The first pixel is found in the highest bit:
            uBit = 0x8000;

            // 16 pixels to process:
            for (iZ=0; iZ<16; ++iZ)
            {
                // Work out the colour index:
                iInk = 0;
                if (uW0 & uBit) iInk += 1;
                if (uW1 & uBit) iInk += 2;
                if (uW2 & uBit) iInk += 4;
                if (uW3 & uBit) iInk += 8;

                // Plot a pixel of that color:
                *(imgPic + offset++) = colNeoPalette[iInk].red;
                *(imgPic + offset++) = colNeoPalette[iInk].green;
                *(imgPic + offset++) = colNeoPalette[iInk].blue;

                uBit >>= 1;
            }
            iPtr += 8;
        }
    }

    if (neo_zimage != NULL)
        free(neo_zimage);

    if ((neo_zimage = (z_image*)malloc(sizeof(z_image))) == NULL)
    { free(imgPic); return NULL; }

    neo_zimage->bits_per_sample = 8;
    neo_zimage->width = 320;
    neo_zimage->height = 200;
    neo_zimage->image_type = IMAGE_TYPE_RGB;
    neo_zimage->data = imgPic;

    return neo_zimage;
}



int end_neo_graphics()
{
    if (neo_file != NULL)
        fclose(neo_file);

    if (imgPic != NULL) {
        free(imgPic);
        imgPic = NULL;
    }

    if (neo_zimage != NULL) {
        free(neo_zimage);
        neo_zimage = NULL;
    }

    return 0;
}

#endif /* neo_c_INCLUDED */

