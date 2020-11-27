/* TempWindow.h: Temporary Glk window objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>
#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

@class TempLibrary;

@interface TempWindow : NSObject <NSSecureCoding> {

    TempLibrary *library;

    glui32 x0, y0, x1, y1;

    // For pair window struct:
    glui32 child1, child2;
    glui32 dir;			/* winmethod_Left, Right, Above, or Below */
    int vertical, backward;		/* flags */
    glui32 division;		/* winmethod_Fixed or winmethod_Proportional */
    glui32 key;			/* NULL or a leaf-descendant (not a Pair) */
    glui32 size;			/* size value */

    // For line input buffer struct:
    void *buf;
    int len;
    int cap;
    gidispatch_rock_t inarrayrock;

    glui32 background;	/* for graphics windows */

	int line_request;
    int line_request_uni;
    int char_request;
    int char_request_uni;
    int mouse_request;
    int hyper_request;

    int echo_line_input;

    glui32 style;

    int fgset;
    int bgset;
    int reverse;
    int attrstyle;
    int fgcolor;
    int bgcolor;
    int hyper;

    NSMutableArray *line_terminators;

    /* These values are only used in a temporary TempLibrary, while deserializing. */
	uint8_t *tempbufdata;
	NSUInteger tempbufdatalen;
	long tempbufkey;
}

@property glui32 peer;
@property glui32 tag;
//@property glui32 disprock;
@property glui32 type;
@property glui32 rock;
@property glui32 parent;

@property glui32 streamtag;
@property glui32 echostreamtag;

@property glui32 prev;
@property glui32 next;

- (instancetype) initWithCStruct:(window_t *)win;
- (void) updateRegisterArray;
- (void) copyToCStruct:(window_t *)win;
- (void) finalizeCStruct:(window_t *)win;
- (void) sanityCheckWithLibrary:(TempLibrary *)library;

@end
