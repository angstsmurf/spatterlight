//
//  vector_oplist.h
//  scott
//
//  The growable list of draw operations recorded while decoding a vector
//  image, which the slow-draw timer then plays back a chunk at a time.
//  Shared by the Apple II, Atari 8-bit and Howarth vector renderers.
//
//  Header-only so that test harnesses which unity-include a single renderer
//  need not link vector_common.c.
//

#ifndef vector_oplist_h
#define vector_oplist_h

#include <stdlib.h>

#include "glk.h"
#include "memory_allocation.h"
#include "minmax.h"
#include "vector_common.h"

#define VECTOR_OPLIST_INITIAL_CAPACITY 100

typedef struct {
    void *ops;
    size_t elem_size;
    size_t capacity;
    size_t total; // Number of recorded ops
    size_t current; // Index of the next op to play back
} VectorOpList;

#define VECTOR_OPLIST(type) { NULL, sizeof(type), 0, 0, 0 }

static inline void *VectorOpAt(const VectorOpList *list, size_t index)
{
    return (char *)list->ops + index * list->elem_size;
}

static inline void VectorOpListFree(VectorOpList *list)
{
    free(list->ops);
    list->ops = NULL;
    list->capacity = 0;
}

// Returns a pointer to a new op at the end of the list, growing it as needed.
// This might be more ops than there are bytes or pixels on screen,
// as many ops may write to the same place.
static inline void *VectorOpListPush(VectorOpList *list)
{
    if (list->total >= list->capacity) {
        size_t capacity = MAX(list->capacity * 2, list->total + 1);
        list->capacity = MAX(capacity, VECTOR_OPLIST_INITIAL_CAPACITY);
        // Our wrapper of realloc() will exit() on failure,
        // so no need to check the result here.
        list->ops = MemRealloc(list->ops, list->capacity * list->elem_size);
    }
    return VectorOpAt(list, list->total++);
}

// When the image has finished recording all its ops,
// we might have allocated a lot more memory than we need,
// so we free any excess space.
static inline void VectorOpListShrink(VectorOpList *list)
{
    if (list->ops != NULL && list->capacity > list->total) {
        if (list->total == 0) {
            VectorOpListFree(list);
            return;
        }
        list->capacity = list->total;
        list->ops = MemRealloc(list->ops, list->capacity * list->elem_size);
    }
}

// Init a new image session. Cancel any in-progress drawing and free any ops.
static inline void VectorOpListStartSession(VectorOpList *list)
{
    VectorOpListFree(list);
    // Start with a small allocation. VectorOpListPush() will grow this as needed.
    list->capacity = VECTOR_OPLIST_INITIAL_CAPACITY;
    list->ops = MemAlloc(list->capacity * list->elem_size);
    list->total = 0;
    list->current = 0;
}

static inline int VectorOpListDrawing(const VectorOpList *list)
{
    return list->total > list->current;
}

// If all ops have been played back: stop the timer, mark the image as
// shown, release the ops and return 1.
static inline int VectorOpListFinishIfDone(VectorOpList *list)
{
    if (list->current < list->total)
        return 0;
    glk_request_timer_events(0);
    VectorState = SHOWING_VECTOR_IMAGE;
    VectorOpListFree(list);
    return 1;
}

struct USImage;

// Prepares a vector op list (Apple II or Atari 8-bit) for drawing img.
// A room image starts a new session. Any other image is drawn on top of the
// current room image: if that has already been fully drawn, redraw_shown_image
// paints it from screen memory and a new session is started for the overlay.
// Returns 0 if no room image is being drawn or shown (it is dark, or graphics
// are off), so there is no session to add the image to.
int VectorBeginImage(VectorOpList *list, const struct USImage *img, void (*redraw_shown_image)(void));

#endif /* vector_oplist_h */
