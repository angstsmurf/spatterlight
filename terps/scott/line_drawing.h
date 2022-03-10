//
//  line_drawing.h
//  Spatterlight
//
//  Created by Administrator on 2022-03-03.
//

#ifndef line_drawing_h
#define line_drawing_h

struct line_image {
    uint8_t *data;
    int bgcolour;
    size_t size;
};

void DrawVectorPicture(int image);
void DrawSomeVectorPixels(int from_start);
int DrawingVector(void);

extern struct line_image *LineImages;

typedef enum {
    NO_VECTOR_IMAGE,
    DRAWING_VECTOR_IMAGE,
    SHOWING_VECTOR_IMAGE
} VectorStateType;

extern VectorStateType VectorState;
extern int vector_image_shown;

#endif /* line_drawing_h */