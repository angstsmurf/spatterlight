//
//  asm5.c
//  plus
//
//  Created by Administrator on 2022-08-08.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "common.h"


uint8_t stmem[0x1000000];

static uint32_t
D0 = 0, D1 = 0,
D2 = 0, D3 = 0,
D4 = 0, D5 = 0,
D6 = 0, D7 = 0,
A0 = 0, A1 = 0,
A2 = 0, A3 = 0,
A4 = 0, A5 = 0,
A6 = 0, A7 = 0;

static void printbytes(uint8_t *source, int num) {
    fprintf(stderr, "0x%04lx: ", source - stmem);
    for (int i = 0; i < num; i++) {
        fprintf(stderr, "%02x", source[i]);
    }
    fprintf(stderr, "\n");
}

static void movelong(uint32_t source, uint8_t *dest) {
//    fprintf(stderr, "source :%08x\n", source);
    dest[0] = source >> 24;
    dest[1] = (source >> 16) & 0xff;
    dest[2] = (source >> 8) & 0xff;
    dest[3] = source & 0xff;
}

static void moveword(uint16_t source, uint8_t *dest) {
//    fprintf(stderr, "source :%04x\n", source);
    dest[0] = (source >> 8) & 0xff;
    dest[1] = source & 0xff;
}

static uint8_t *SetPalette(uint8_t *ptr) {
    A2 = 0x1acb4;
    for (int i = 0; i <= 0xb6; i++) {
        stmem[A2++] = 0;
        stmem[A2++] = 0;
    }
    moveword(0x000e, &stmem[0x1acb4]);

    if ((D0 & 0xffff) != 0) {
        for (int i = 0; i < D0; i++) {
            D4 = *ptr++;
            D3 = (uint16_t)(D4 >> 4) * 0x1a;

            stmem[0x1acb6 + D3 + 1] = *ptr++;
            stmem[0x1acb6 + D3 + 2] = *ptr++;
            stmem[0x1acb6 + D3 + 3] = *ptr++;
            D4 = D4 & 0xf;
            stmem[0x1acb6 + D3 + 5] = D4;
            A3 = 0x1acb6 + D3 + 6;

            for (int j = D4; j > 0; j--) {
                stmem[A3++] = *ptr++;
                stmem[A3++] = *ptr++;
            };
        }
    }
    return ptr;
}

uint8_t STGraphBytes[8];
int ByteIndex = 0;


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c\n"
#define BYTE_TO_BINARY(uint8_t)  \
(uint8_t & 0x80 ? '1' : '0'), \
(uint8_t & 0x40 ? '1' : '0'), \
(uint8_t & 0x20 ? '1' : '0'), \
(uint8_t & 0x10 ? '1' : '0'), \
(uint8_t & 0x08 ? '1' : '0'), \
(uint8_t & 0x04 ? '1' : '0'), \
(uint8_t & 0x02 ? '1' : '0'), \
(uint8_t & 0x01 ? '1' : '0')


void SetST(int bit, uint8_t *byte) {
    *byte |= 1 << bit;
}

int IsSTSet(int bit, uint8_t byte) {
    return ((byte & (1 << bit)) != 0);
}

void PutPixel(glsi32 x, glsi32 y, int32_t color);

extern int x,y, xlen;


//void DrawSTByte(uint8_t *ptr)
//{
//    uint8_t col = 0;
//    uint8_t byte;
//    for (int i = 7; i >= 0; i--) {
//        for (int j = 0; j < 8; j += 2) {
//            byte = ptr[j];
//            if (IsSTSet(i, byte))
//                SetST(j / 2, &col);
//
//        }
//        if (col)
//            PutPixel(x,y, col);
//        x++;
//        col = 0;
//        if (x > xlen) {
//            x = 0;
//            y++;
//        }
//    }
//}


//if ((D6 & 0x80) == 0) {
//    for (int i = D6; i >= 0; i--) {
//
//
//        D1 = stmem[A1++];
//        //            A5 = D6;
//        //            PUTBYTE(); // 13b66
//
//        word2 = (word2 & 0xffffff00) | D1;
//
//
//        uint8_t *array = &stmem[A2];
//        uint8_t param_4 = D2 & 0xff;
//        uint8_t param_5 = D3 & 0xff;
//
//        PutByte(&stmem[0x21628 + sVar5],word2,bVar2 ^ 0xff,
//                bVar2);
//
//        //            D6 = A5;
//        //            D1 = A6;
//    }
//    D6 = 0;


//do {
//    ptr2 = (byte *)((int)ptr2 + 1);
//    c = (ushort)*ptr2;
//    if ((*ptr2 & 0x80) == 0) {
//        do {
//            ptr2 = ptr2 + 1;
//            word2 = word2 & 0xffffff00 | (uint)*ptr2;
//            PUTBYTE((byte *)((int)&IMAGE_WRITTEN_HERE + (int)sVar5),(short)word2,puVar4,bVar2 ^ 0xff,
//                    bVar2);
//            c = c - 1;
//            ptr2 = ptr2;
//        } while (c != 0xffff);
//    }

//static void PutByte(uint8_t *array,short word2,uint8_t param_4,uint8_t param_5) {
//    uint8_t bVar1;
//    short sVar2;
//
//    sVar2 = word2 << 2;
//    bVar1 = stmem[0x1ae28 + 3 + sVar2];
//    *array = param_4 & *array;
//    *array = *array | (bVar1 & param_5);
//    bVar1 = stmem[0x1ae28 + 2 + sVar2];
//    array[2] = param_4 & array[2];
//    array[2] = array[2] | (bVar1 & param_5);
//    bVar1 = stmem[0x1ae28 + 1 + sVar2];
//    array[4] = param_4 & array[4];
//    array[4] = array[4] | (bVar1 & param_5);
//    bVar1 = stmem[0x1ae28 + sVar2];
//    array[6] = param_4 & array[6];
//    array[6] = array[6] | (bVar1 & param_5);
//    return;
//}

int DrawSTImageFromGraphicsMemory(uint8_t *ptr, size_t datasize);

#define WORD_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n"

#define WORD_TO_BINARY(uint16_t)  \
(uint16_t & 0x8000 ? '1' : '0'), \
(uint16_t & 0x4000 ? '1' : '0'), \
(uint16_t & 0x2000 ? '1' : '0'), \
(uint16_t & 0x1000 ? '1' : '0'), \
(uint16_t & 0x800 ? '1' : '0'), \
(uint16_t & 0x400 ? '1' : '0'), \
(uint16_t & 0x200 ? '1' : '0'), \
(uint16_t & 0x100 ? '1' : '0'), \
(uint16_t & 0x80 ? '1' : '0'), \
(uint16_t & 0x40 ? '1' : '0'), \
(uint16_t & 0x20 ? '1' : '0'), \
(uint16_t & 0x10 ? '1' : '0'), \
(uint16_t & 0x08 ? '1' : '0'), \
(uint16_t & 0x04 ? '1' : '0'), \
(uint16_t & 0x02 ? '1' : '0'), \
(uint16_t & 0x01 ? '1' : '0')

extern winid_t Graphics;
int IsSTBitSet(int bit, uint16_t word);

void DrawSTByte(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, int x, int y)
{
    for (int i = 7; i >= 0; i--) {
        uint8_t col = 0;
        if (IsSTBitSet(i, byte1))
            col |= 1;
        if (IsSTBitSet(i, byte2))
            col |= 2;
        if (IsSTBitSet(i, byte3))
            col |= 4;
        if (IsSTBitSet(i, byte4))
            col |= 8;

        if (col) {
            int x1 = x + 7 - i;
            int y1 = y;
            PutPixel(x1, y1, col);
            int address = 0x21628 + 160 * y1 + 8 * (x1 / 16);
        }


    }
}

static void PUTBYTE(uint8_t byte) {
    // D2 and D3 alternates between 0f and f0
    uint8_t otherhalf = 0xf0;
    if (D2 == 0xf0)
        otherhalf = 0xf;
    uint16_t offs = byte << 2;
    uint16_t result = stmem[0x1ae28 + offs + 3] & otherhalf;

    int scroffset = A2 - 0x21628;
    int y1 = scroffset / 160;
    int x1 = 16 * (scroffset % 160) / 128;
    uint8_t byte1 = result;

    stmem[A2] = (stmem[A2] & D2) | result;
    result = stmem[0x1ae28 + offs + 2] & otherhalf;
    uint8_t byte2 = result;

    stmem[A2 + 2] = (stmem[A2 + 2] & D2) | result;
    result = stmem[0x1ae28 + offs + 1] & otherhalf;
    uint8_t byte3 = result;

    stmem[A2 + 4] = (stmem[A2 + 4] & D2) | result;
    result = stmem[0x1ae28 + offs] & otherhalf;
    uint8_t byte4 = result;

    x1 *= 16;
    if (A2 % 2) {
        x1 += 8;
    }
    DrawSTByte(byte1, byte2, byte3, byte4, x1, y1);

    stmem[A2 + 6] = (stmem[A2 + 6] & D2) | result;
    otherhalf = otherhalf ^ 0xff;
    D2 = D2 ^ 0xff;
    if (otherhalf != 0xf) {
        A2 = A2 + 1;
        result = A2 & 0xffff;
        if ((result & 1) == 0) {
            A2 = A2 + 6;
        }
    }

    A4 = A4 - 1;
    result = A4 & 0xffff;
    if (result != 0)
        return;
    D4 += 0xa0; //160 with in double pixels
    A2 = D4;
    otherhalf = D5 & 0xffff;
    D2 = otherhalf ^ 0xff;
    A4 = stmem[0x01ae26] * 256 + stmem[0x01ae27];
}

static void DrawPixels(void) {
    D2 = D1;
    D1 = D1 & D5;
    D2 = D2 << 4;
    D3 = stmem[A0 + D1];
    D1 = stmem[A0 + D2];
    D4 = D3;
    D2 = D1;
    D3 = D3 & D5;
    D1 = D1 & D3;
    D4 = D4 & D6;
    D2 = D2 & D6;
    D4 = D4 << 4;
    D1 = D1 << 4;
    D1 = D1 & D3;
    D2 = D2 & D4;
    stmem[A2++] = D2;
    stmem[A6++] = D1;
    D7--;
    if (D7 == 0) {
        A4 += 0xa0;
        A6 = A4 + 0x50;
        A2 = A4;
        D7 = stmem[0x1ae26] * 256 + stmem[0x1ae27];
    }


}

void SetRGB(int32_t index, int red, int green, int blue);

extern int xoff, yoff, ylen;

// UNPACK SAGAP16
static void ASM5(void) {

    x = 0; y = 0;

    xoff = 0; yoff = 0;

    xlen = 319;
    ylen = 200;

    //2f72a = IMGDATA_SIZE
    fprintf(stderr, "IMGDATA_SIZE: %d\n", stmem[0x2f72a] * 256 + stmem[0x2f72b]);

    movelong(A6, &stmem[A7]);

    //2f72c = XYOFFSET 2f72c = x offset, 2f72d = y offset
    fprintf(stderr, "x offset: %d\n", stmem[0x2f72c]);
    fprintf(stderr, "y offset: %d\n", stmem[0x2f72d]);

    uint8_t *ptr = &stmem[0x2f72c];

    stmem[0x1ae24] = *ptr++;
    stmem[0x1ae25] = *ptr++;
    //2f72e = IMGWIDTH
    //2f72f = IMGHEIGHT
    fprintf(stderr, "image width: %d\n", stmem[0x2f72e]);
    fprintf(stderr, "image height: %d\n", stmem[0x2f72f]);


    stmem[0x1ae27] = *ptr++;
    D0 = *ptr++;
    fprintf(stderr, "Animation colours: %d\n", D0);
    A2 = 0x1ac92;
    // 0x2f730 = Palette start
//    ptr = &stmem[0x2f730];
//    PALOUT
    for (int i = 0; i <= 0xf; i++) {
        int blue = *(ptr + 1) & 0xf;
        int green = (*(ptr + 1) >> 4) & 0xf;
        int red = *ptr & 0xf;

        SetRGB(i, red, green, blue);

        stmem[A2++] = *ptr++;
        stmem[A2++] = *ptr++;
    }

    stmem[0x1acb2] = 0;
    stmem[0x1acb3] = 0;
    stmem[0x1acb4] = 0;
    stmem[0x1acb5] = 0;
//    printbytes(&mem[0x1ac92], 34);
//    printbytes(&mem[0x1acaf], 8);

//    fprintf(stderr, "D0 = 0x%08x\n", D0);

    ptr = SetPalette(ptr);

    //13aba
    stmem[0x1acb2] = 0;
    stmem[0x1acb3] = 1;

    A2 = 0x21628;
    uint16_t dataSize = stmem[0x2f72a] * 256 + stmem[0x2f72b];
//    fprintf(stderr, "A1=0x%08x D0=0x%08x\n", A1, D0);
    uint8_t *endOfData = ptr + dataSize;
    D2 = stmem[0x1ae24] * 256 + stmem[0x1ae25]; // D2 = xpos?
    D2 = (D2 >> 2) * 8;
//    printbytes(&mem[0x1ae25], 4);
    if ((stmem[0x1ae28] & 2) != 0) {
        D2++;
    }

    fprintf(stderr, "XPOS = %x\n", D2);

    A2 = A2 + D2;

    D0 = 0xff;
    D3 = 0xf0;
    uint bVar2 = 0xf0;

    uint8_t XPOS = stmem[0x1ae28];
    if ((XPOS & 1) != 0) {
        D3 = D3 ^ 0xff;
        bVar2 = 0xf;
    }

    uint16_t sVar5 = (XPOS >> 2) * 8;
    if ((XPOS & 2) != 0) {
        sVar5 = sVar5 + 1;
    }

    uint8_t c;

    D5 = D3;
    D2 = D3;
    D2 = D2 ^ 0xff;
    D4 = A2;
    A4 = stmem[0x1ae26] * 256 + stmem[0x1ae27];
    A0 = 0x1ae28;
    D1 = 0;
    if (stmem[0x1ac28] + stmem[0x1ac29] + stmem[0x1ac2a] + stmem[0x1ac2b] != 0) {
        goto MUNPACK;
    }
//UNPLP1:
    do {
        c = *ptr++;
        if ((c & 0x80) == 0) {
            for (int i = c; i >= 0; i--) {
                c = *ptr++;
                PUTBYTE(c); // 13b66
            }
        } else {
            // Repeat next byte (c & 0x7f) times
            c = c & 0x7f;
            uint8_t val = *ptr++;
            for (int i = c; i >= 0; i--) {
                PUTBYTE(val); // 13b66
            }
        }
        //    NXTBYT1
    } while (ptr < endOfData);

jump13b52:
    A6 = (stmem[A7] << 24) | (stmem[A7 + 1] << 16) | (stmem[A7 + 2] << 8) | (stmem[A7 + 3]);
    A7 = A7 + 4;
    if (stmem[A6 + 0x59e] + stmem[A6 + 0x59e + 1]  != 0) {
        fprintf(stderr, "WUPDATE()\n");
        fprintf(stderr, "VBLSET()\n");
        //        WUPDATE();
        //        VBLSET();
    }
//    printbytes(&stmem[0x21625], 0x400);
    return;


    //    sVar5 = (XPOS >> 2) * 8;
    //    if ((XPOS & 2) != 0) {
    //        sVar5 = sVar5 + 1;
    //    }
    //    bVar2 = 0xf0;
    //    if ((XPOS & 1) != 0) {
    //        bVar2 = 0xf;
    //    }
    //    puVar3 = &DAT_0001ae28;
    //    word2 = 0;

    //    do {
    //        ptr2 = (byte *)((int)ptr2 + 1);
    //        c = (ushort)*ptr2;
    //        if ((*ptr2 & 0x80) == 0) {
    //            do {
    //                ptr2 = ptr2 + 1;
    //                word2 = word2 & 0xffffff00 | (uint)*ptr2;
    //                PUTBYTE((byte *)((int)&IMAGE_WRITTEN_HERE + (int)sVar5),(short)word2,puVar3,bVar2 ^ 0xff,
    //                        bVar2);
    //                c = c - 1;
    //                ptr2 = ptr2;
    //            } while (c != 0xffff);
    //        }
    //        else {
    //            c = c & 0x7f;
    //            ptr2 = (byte *)((int)ptr2 + 2);
    //            word2 = word2 & 0xffffff00 | (uint)*ptr2;
    //            do {
    //                PUTBYTE((byte *)((int)&IMAGE_WRITTEN_HERE + (int)sVar5),(short)word2,puVar3,bVar2 ^ 0xff,
    //                        bVar2);
    //                word2 = word2 & 0xffff;
    //                c = c - 1;
    //            } while (c != 0xffff);
    //        }
    //    } while ((int)ptr2 < iVar1);

MUNPACK:
    fprintf(stderr, "MUNPACK\n");
    A2 = 0x21628;
    D0 = stmem[0x1ae24] / 50;
    D1 = D0;
    uint32_t highword = D0 >> 16;
    uint32_t loword = D0 & 0xffff;
    D0 = (loword << 16) | highword;
    D1 = D1 * 0xa0;
    D1 = D1 + D0;
    A2 = A2 + D1;
    A0 = stmem[0x1ac24];
    D7 = stmem[0x1ac26];
    A4 = A2;
    D6 = 0xf0;
    D5 = 0xf;
    A6 = A2 + 0x50;
    D1 = 0;
jump13c08:
    D0 = *(ptr + 0x2f732);
    if (D0 & 0x80)
        goto jump13c24;
    D0 = D0 & 0x7f;
    D1 = *(ptr + 0x2f733);
    A5 = D1;
    do {
        D1 = A5;
        DrawPixels();
        D0--;
    } while ((int)D0 != -1);
    goto jump13c2c;
jump13c24:
    do {
        D1 = stmem[A1 + 0x2f733];
        DrawPixels();
        D0--;
    } while ((int)D0 != -1);

jump13c2c:
    if (endOfData < ptr)
        goto jump13c08;
    goto jump13b52;
}

void InitStMem(void) {
    FILE *f = fopen("/Users/administrator/mame/q3.bin", "r");
    if (f == NULL) {
        fprintf(stderr, "Cannot open file");
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size_t length = ftell(f);
    if (length == -1) {
        fclose(f);
        fprintf(stderr, "Cannot read file or file empty");
        exit(1);
    }
    fprintf(stderr, "Size of file: %ld\n", length);

    fseek(f, 0, SEEK_SET);
    fread(stmem, length, 1, f);
    fclose(f);
}

size_t writeToFile(const char *name, uint8_t *data, size_t size);

void SetupLookup(void)

{
    uint uVar1;
    ushort uVar2;
    uint uVar3;
    short offset;
    uint8_t *puVar5;

    uVar1 = 0xffff0000;
    puVar5 = &stmem[0x1ae28];
    do {
        uVar3 = 0;
        do {
            offset = (short)uVar3;
            puVar5[offset] = 0;
            if ((uVar1 & 1 << (uVar3 & 0x1f)) != 0) {
                puVar5[offset] = puVar5[offset] | 0x33;
            }
            if ((uVar1 & 1 << ((ushort)(offset + 4) & 0x1f)) != 0) {
                puVar5[offset] = puVar5[offset] | 0xcc;
            }
            uVar3 = (uint)(ushort)(offset + 1U);
        } while ((ushort)(offset + 1U) != 4);
        puVar5 = puVar5 + 4;
        uVar2 = (short)uVar1 + 1;
        uVar1 = (uVar1 & 0xffff0000) | (uint)uVar2;
    } while (uVar2 != 0x100);
}


void DrawSTImageFromData(uint8_t *ptr, size_t datasize) {
    bzero(stmem, 0x100000);
//    memset(stmem + 0x1ae28 + 0x100, 0, 0x100);

//    memset(stmem + 0x1ae28, 0, 0x400);
    SetupLookup();
    memset(stmem + 0x21628, 0, 0x7999);

    memcpy(stmem + 0x2f728, ptr, datasize);


    A0 = 0;
    A1 = 0;
    A2 = 0;
    A3 = 0;
    A4 = 0;
    A5 = 0;
    A6 = 0x141ae;
    A7 = 0x3a2fe;
    D0 = 0;
    D1 = 0;
    D2 = 0;
    D3 = 0;
    D4 = 0;
    D5 = 0;
    D6 = 0;
    D7 = 0;

    glk_request_timer_events(0);

    ASM5();

//    writeToFile("/Users/administrator/Desktop/0x21628.bin", &stmem[0x21628], 0x7D00);

//    DrawSTImageFromGraphicsMemory(&stmem[0x21628], 0x7999);

//    memset(stmem + 0x2f728, 0, datasize);

}
