//
//  v6_image.h
//  Spatterlight
//
//  Created by Administrator on 2023-07-17.
//

#ifndef v6_image_h
#define v6_image_h

#include "types.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

enum GraphicsType {
    kGraphicsTypeAmiga,
    kGraphicsTypeMacBW,
    kGraphicsTypeApple2,
    kGraphicsTypeVGA,
    kGraphicsTypeEGA,
    kGraphicsTypeCGA,
    kGraphicsTypeBlorb,
    kGraphicsTypeNoGraphics,
};

struct ImageStruct {
    int index;
    int width;
    int height;
    bool transparency;
    int transparent_color;
    uint8_t *data;
    size_t datasize;
    GraphicsType type;
    uint8_t *palette;
    uint8_t *huffman_tree;
};

typedef struct ImageStruct ImageStruct;

extern ImageStruct *raw_images;
extern int image_count;
extern int palentries;
extern uint8_t global_palette[16 * 3];

extern float imagescalex;
extern float imagescaley;
extern float hw_screenwidth;
extern float pixelwidth;

extern ImageStruct *raw_images;
extern int image_count;
extern GraphicsType graphics_type;

#endif /* v6_image_h */
