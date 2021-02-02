/* TempWindow.h: Temporary Glk window objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>
#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

@class TempLibrary;

@interface TempWindow : NSObject <NSSecureCoding>

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
