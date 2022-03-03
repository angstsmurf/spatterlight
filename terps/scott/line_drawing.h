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

void DrawLinePicture(int image);

extern struct line_image *LineImages;

#endif /* line_drawing_h */
