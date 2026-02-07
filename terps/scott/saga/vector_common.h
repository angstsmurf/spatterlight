//
//  vector_common.h
//  scott
//
//  Created by Administrator on 2026-02-01.
//

#ifndef vector_common_h
#define vector_common_h

void DrawSomeVectorPixels(int from_start);
int DrawingVector(void);
<<<<<<< HEAD
int TimerDelay(void);
=======
>>>>>>> 4ed3c32f (ciderpress: Don't bail on bad sector index)

typedef enum {
    NO_VECTOR_IMAGE,
    DRAWING_VECTOR_IMAGE,
    SHOWING_VECTOR_IMAGE
} VectorStateType;

extern VectorStateType VectorState;
extern int vector_image_shown;

#endif /* vector_common_h */
