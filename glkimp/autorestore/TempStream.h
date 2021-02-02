/* TempStream.h: Temporary Glk stream object
 objc class for serializing, adapted from IosGlk,
 the iOS implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>
#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

@interface TempStream : NSObject <NSSecureCoding>

@property glui32 tag;
//@property gidispatch_rock_t disprock;
@property int type;
@property glui32 rock;
@property FILE *file;
@property glui32 prev;
@property glui32 next;

- (instancetype) initWithCStruct:(stream_t *)str;
- (void) copyToCStruct:(stream_t *)str;
- (void) updateRegisterArray;
- (void) updateResource;
- (BOOL) reopenInternal;
- (void) sanityCheck;

@end
