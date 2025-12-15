//
//  irmak.h
//  scott
//
//  Created by Administrator on 2025-12-07.
//

#ifndef irmak_h
#define irmak_h

#include <stdlib.h>

typedef struct {
    uint8_t *imagedata;
    size_t datasize;
    uint8_t xoff;
    uint8_t yoff;
    uint8_t width;
    uint8_t height;
} Image;

typedef struct {
    uint8_t *dataptr;
    uint8_t *origptr;
    size_t datasize;
    int xsize;
    int ysize;
    int xoff;
    int yoff;
    int imagesize;
    int draw_to_buffer;
} IrmakImgContext;

void DrawIrmakPictureFromBuffer(void);
void Flip(uint8_t character[]);
int isNthBitSet(unsigned const char c, int n);
void ClearGraphMem(void);
void DrawPictureNumber(int picture_number, int draw_to_buffer);
int DrawPictureAtPos(int picture_number, uint8_t x, uint8_t y, int draw_to_buffer);

void InitIrmak(int numimg, int imgver);

#define INK_MASK         0x07
#define PAPER_MASK       0x38
#define BRIGHT_FLAG      0x40

// The image area consists of 12 rows
// of 32 tile cells each
// (for a total of 384 tiles)
#define IRMAK_IMGWIDTH 32
#define IRMAK_IMGHEIGHT 12
#define IRMAK_IMGSIZE IRMAK_IMGWIDTH * IRMAK_IMGHEIGHT

// Used by SagaSetup() and TaylorSetup()
extern uint8_t tiles[256][8];
// Used by animate_waterfall() in Robin of Sherwood
extern uint8_t layout[IRMAK_IMGSIZE][8];
// Used by Seas of Blood and Taylormade draw routines
extern uint8_t imagebuffer[IRMAK_IMGSIZE][9];

extern Image *images;

#endif /* irmak_h */
