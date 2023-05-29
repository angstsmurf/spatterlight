//
//  writetiff.cpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//

#include "writetiff.h"

static void writehexstring(FILE *fptr, const char *s)
{
    unsigned int i,c;
    char hex[3];

    for (i = 0; i < strlen(s);i += 2) {
        hex[0] = s[i];
        hex[1] = s[i+1];
        hex[2] = '\0';
        sscanf(hex,"%X",&c);
        putc(c, fptr);
    }
}

static void write32BE(FILE *fptr, uint32_t offset) {
    putc(offset >> 24, fptr);
    putc((offset >> 16) & 0xff, fptr);
    putc((offset >> 8) & 0xff, fptr);
    putc(offset & 0xff, fptr);
}

void writetiff(FILE *fptr, uint8_t *pixarray, uint32_t pixarraysize, int width) {

    uint16_t height = pixarraysize / (width * 4);
    fprintf(stderr, "writetiff: width %d height %d pixarraysize: %d\n", width, height, pixarraysize);
    /* Write the header */
    writehexstring(fptr, "4d4d002a");    /* Big endian & TIFF identifier */
    uint32_t offset = pixarraysize + 8;
    write32BE(fptr, offset);

    /* Write the binary data */
    if (fwrite(pixarray, 1, pixarraysize, fptr) != pixarraysize)
        return;

    /* Write the footer */
    writehexstring(fptr, "000f");  /* The number of directory entries (15) */

    /* Width tag, short int */
    writehexstring(fptr, "0100000300000001");
    fputc((width >> 8) & 0xff, fptr);    /* Image width */
    fputc(width & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Height tag, short int */
    writehexstring(fptr, "0101000300000001");
    fputc((height >> 8) & 0xff, fptr);    /* Image height */
    fputc(height & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Bits per sample tag, short int */
    writehexstring(fptr, "0102000300000004");

    /* Offset of first byte after directory entries */
    offset = pixarraysize + 194;
    write32BE(fptr, offset);

    /* Compression flag, short int */
    writehexstring(fptr, "010300030000000100010000");

    /* Photometric interpolation tag, short int */
    writehexstring(fptr, "010600030000000100020000");

    /* Strip offset tag, long int */
    writehexstring(fptr, "011100040000000100000008");

    /* Orientation flag, short int */
    writehexstring(fptr, "011200030000000100010000");

    /* Sample per pixel tag, short int */
    writehexstring(fptr, "011500030000000100040000");

    /* Rows per strip tag, short int */
    writehexstring(fptr, "0116000300000001");
    fputc((height >> 8) & 0xff, fptr);    /* Image height */
    fputc(height & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Strip byte count flag, long int */
    writehexstring(fptr, "0117000400000001");
    write32BE(fptr, pixarraysize);

    /* Minimum sample value flag, short int */
    writehexstring(fptr, "0118000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* Maximum sample value tag, short int */
    writehexstring(fptr, "0119000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* Planar configuration tag, short int */
    writehexstring(fptr, "011c00030000000100010000");

    /* Extra samples tag, short int */
    writehexstring(fptr, "015200030000000100010000");

    /* Sample format tag, short int */
    writehexstring(fptr, "0153000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* End of the directory entry */
    writehexstring(fptr, "00000000");

    /* Bits for each colour channel */
    writehexstring(fptr, "0008000800080008");

    /* Minimum value for each component */
    writehexstring(fptr, "0000000000000000");

    /* Maximum value per channel */
    writehexstring(fptr, "00ff00ff00ff00ff");

    /* Samples per pixel for each channel */
    writehexstring(fptr, "0001000100010001");
}
