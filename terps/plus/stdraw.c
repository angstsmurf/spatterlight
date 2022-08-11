/* Routine to draw the Atari RLE graphics
 
 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "glk.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

extern PALETTE pal;

extern winid_t Graphics;

void PutPixel(glsi32 x, glsi32 y, int32_t color);

void SetColour(int32_t index, const RGB *colour);

void PrintFirstTenBytes(uint8_t *ptr, size_t offset);

void PutPixel(glsi32 x, glsi32 y, int32_t color);

void SetSTBit(int bit, uint16_t *word) {
    *word |= 1 << bit;
}

void ResetSTBit(int bit, uint16_t *word) {
    *word &= ~(1 << bit);
}

int IsSTBitSet(int bit, uint16_t word) {
    return ((word & (1 << bit)) != 0);
}

extern uint8_t STGraphBytes[8];
extern int ByteIndex;


void DrawSTPixels(void)
{
    uint16_t col = 0;
    uint16_t word;
    for (int i = 15; i >= 0; i--) {
        for (int j = 0; j < 4; j++) {
            word = STGraphBytes[j * 2] << 8;
            word |= STGraphBytes[j * 2 + 1];
            if (IsSTBitSet(i, word))
                SetSTBit(j, &col);

        }
        if (col)
            PutPixel(x,y, col);
        x++;
        col = 0;
        if (x > xlen) {
            x = 0;
            y++;
        }
    }
}


void AddSTByte(uint8_t byte) {
    STGraphBytes[ByteIndex++] = byte;
    if (ByteIndex == 8) {
        DrawSTPixels();
        ByteIndex = 0;
    }

}

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

extern uint8_t stmem[];
extern int x_offset, right_margin;

void writeword(uint16_t word, int offset) {
    uint8_t low = word & 0xff;
    uint8_t high = word >> 8;
    stmem[offset] = stmem[offset] | high;
    stmem[offset + 1] = stmem[offset + 1] | low;
}


void PlotToMem(int x, int y, int colour) {
    fprintf(stderr, "PlotToMem x:%d y:%d\n",x,y);
    // we assume screen mem starts at 0x21628
    int base = 0x21628;
    // 160 bytes per row
    // every pixel uses 4 bits
    fprintf(stderr, "xpos: %d\n", 8 * (x / 16));
    int address = base + 160 * y + 8 * (x / 16);
    // bit position in word = (x % 16)
    int bit = 15 - (x % 16);
    uint16_t wordval = 1 << bit;
//    wordval = 0xffff;
    // we set all four: colour 15
    writeword(wordval, address);
    writeword(wordval, address + 2);
    writeword(wordval, address + 4);
    writeword(wordval, address + 6);
}

int DrawSTImageFromGraphicsMemory(uint8_t *ptr, size_t datasize)
{
    ByteIndex = 0;

//    size_t offset = ptr - stmem;
//    fprintf(stderr, "DrawSTImageFromGraphicsMemory: offset 0x%04lx datasize: %ld\n", offset, datasize);

    uint8_t *origptr = ptr;

//    PrintFirstTenBytes(ptr, 0);

    x = 0; y = 0;

//    xoff = 0; yoff = 0;
//    x_offset = 0;
//    right_margin = 740;

    xlen = 319;
    ylen = 200;
    
    while (ptr - origptr < datasize - 3)
    {
        AddSTByte(*ptr++);
    }
    return 1;
}

/*
void PUTBYTE(int param_1,uint8_t *param_2,short param_3,ushort param_4)

{
    uint8_t bVar1;
    uint8_t bVar2;
    uint8_t bVar3;
    short sVar4;

    bVar3 = (uint8_t)(param_4 >> 8);
    bVar2 = (uint8_t)param_4;
    sVar4 = param_3 << 2;
    bVar1 = *(uint8_t *)(param_1 + 3 + (int)sVar4);
    *param_2 = bVar3 & *param_2;
    *param_2 = *param_2 | bVar1 & bVar2;
    bVar1 = *(uint8_t *)(param_1 + 2 + (int)sVar4);
    param_2[2] = bVar3 & param_2[2];
    param_2[2] = param_2[2] | bVar1 & bVar2;
    bVar1 = *(uint8_t *)(param_1 + 1 + (int)sVar4);
    param_2[4] = bVar3 & param_2[4];
    param_2[4] = param_2[4] | bVar1 & bVar2;
    bVar1 = *(uint8_t *)(param_1 + sVar4);
    param_2[6] = bVar3 & param_2[6];
    param_2[6] = param_2[6] | bVar1 & bVar2;
    return;
}

void SetPalette(uint8_t *array,short numcolours)

{
    uint8_t bVar1;
    short sVar3;
    ushort uVar4;
    uint8_t *puVar6;
    uint8_t *puVar7;

    uint8_t DAT_0001bfe6, DAT_0001bfe8, DAT_0001bfea, DAT_0001bfec, DAT_0001bfee;

    puVar6 = &DAT_0001bfe6;
    // Set 182 zeros at address 0001bfe6
    for (int i = 0; i < 0xb6; i++) {
        *puVar6++ = 0;
    }
    DAT_0001bfe6 = 0xe;
    if (numcolours != 0) {
        for (int i = 0; i < numcolours; i++) {
            bVar1 = *array;
            sVar3 = (ushort)(bVar1 >> 4) * 0x1a;
            *(uint8_t *)((int)&DAT_0001bfe8 + sVar3 + 1) = *(uint8_t *)((int)array + 1);
            *(uint8_t *)((int)&DAT_0001bfea + (int)sVar3) = array[1];
            uVar4 = bVar1 & 0xf;
            *(char *)((int)&DAT_0001bfec + sVar3 + 1) = (char)uVar4;
            puVar6 = array + 2;
            puVar7 = (uint8_t *)(&DAT_0001bfee + sVar3);
            // Copy uVar4 (*param_1 & 0xf) - 1 bytes from param_1 + 2 to address 0001bfee + (*param_1  >> 4 * 26)
            for (int i = 0; i < uVar4; i++) {
                array = puVar6 + 1;
                *puVar7 = *puVar6;
                puVar6 = array;
                puVar7 = puVar7 + 1;
            }
        }
    }
    return;
}

void DrawPixels(int param_1,ushort work,uint8_t *pixptr1,uint8_t *pixptr2,ushort param_5)

{
    uint8_t pattern1;
    uint8_t pattern2;

    pattern1 = *(uint8_t *)(param_1 + (short)(work & 0xf));
    pattern2 = *(uint8_t *)(param_1 + (short)(work >> 4));
    *pixptr1 = pattern2 & 0xf0 | (uint8_t)(((param_5 & 0xff00 | (ushort)pattern1) & 0xf0) >> 4);
    *pixptr2 = (uint8_t)((pattern2 & 0xf) << 4) | pattern1 & 0xf;
    return;
}

void ASM5(uint8_t *pStartOfData, int param_1)

{
    uint8_t *pEndofData;
    short sVar5;
    ushort work;
    ushort uVar7;
    uint8_t c;
    uint8_t *puVar9;
    uint8_t uVar10;
    uint8_t *ptr;
    uint8_t *ptr2 = NULL;
    uint8_t *pPalette;
    uint8_t *pixptr1;
    uint8_t *pixptr2;

    uint8_t DAT_0001bf56, DAT_0001bf5a, DAT_0001bfc4, _DAT_0001bfe4, DAT_0001bfe6, DAT_0001c156, DAT_0001c158, DAT_0002295a, DAT_0001c15a, DAT_00030a5c, DAT_00030a5e, DAT_00030a60, DAT_00030a61;

    DAT_0001c156 = DAT_00030a5e;
    DAT_0001c158 = (ushort)DAT_00030a60;
    ptr = pStartOfData;
    pPalette = &DAT_0001bfc4;
    // Set palette: Copy 15 words(?) from ptr to address 0x0001bfc
    for (sVar5 = 0xf; sVar5 >= 0; sVar5--) {
        ptr2 = ptr + 1;
        *pPalette = *ptr;
        ptr = ptr2;
        pPalette++;
    }
    DAT_0001bfe6 = 0;
    _DAT_0001bfe4 = 0;
    SetPalette(ptr2,(ushort)DAT_00030a61);
    _DAT_0001bfe4 = 1;
    pEndofData = ptr2 + DAT_00030a5c;
    sVar5 = (DAT_0001c156 >> 2) * 8;
    if ((DAT_0001c156 & 2) != 0) {
        sVar5 = sVar5 + 1;
    }
    uVar7 = 0xf0;
    if ((DAT_0001c156 & 1) != 0) {
        uVar7 = 0xf;
    }
    puVar9 = &DAT_0001c15a;
    work = 0;
    if (DAT_0001bf5a == 0) {
        do {
            ptr = ptr2 + 1;
            c = *ptr2;
            if ((*ptr2 & 0x80) == 0) {
                // Don't count on the next c characters
                for (int i = 0; i < c + 1; i++) {
                    ptr2 = ptr + 1;
                    work = work & 0xff00 | (ushort)*ptr;
                    PUTBYTE(puVar9,(int)&DAT_0002295a + (int)sVar5,work,uVar7 | (uVar7 ^ 0xff) << 8);
                    ptr = ptr2;
                }
            }
            else {
                // c is a counter
                ptr2 = ptr2 + 1;
                work = work & 0xff00 | (ushort)*ptr;
                for (int i = 0; i < c + 1; i++) {
                    PUTBYTE(puVar9,(int)&DAT_0002295a + (int)sVar5,work,uVar7 | (uVar7 ^ 0xff) << 8);
                    work = work & 0xffff;
                }
            }
        } while (ptr2 < pEndofData);
    }
    else {
        pixptr2 = ((DAT_0001c156 / 0x50) * 0xa0 + DAT_0001c156 % 0x50);
        pixptr1 = (int)&DAT_0002295a + pixptr2;
        pixptr2 = pixptr2 + 0x229aa;
        work = 0;
        uVar10 = DAT_0001bf56;
        do {
            ptr = ptr2 + 1;
            c = *ptr2;
            if ((*ptr2 & 0x80) == 0) {
                // Don't count on the next c characters
                for (int i = 0; i < c + 1; i++) {
                    ptr2 = ptr + 1;
                    work = (work & 0xff00) | *ptr;
                    DrawPixels(uVar10,work,pixptr1,pixptr2,uVar7);
                    ptr = ptr2;
                }
            }
            else {
                // is a counter
                ptr2++;
                work = (work & 0xff00) | *ptr;
                c &= 0x7f;
                for (int i = 0; i < c + 1; i++) {
                    DrawPixels(uVar10,work,pixptr1,pixptr2,uVar7);
                }
            }
        } while (ptr2 < pEndofData);
    }
    if (*(short *)(param_1 + 0x59e) != 0) {
//        WUPDATE();
//        VBLSET();
    }
    return;
}

*/
