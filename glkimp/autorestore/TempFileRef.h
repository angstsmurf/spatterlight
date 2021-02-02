/* TempFileRef.h: Temporary file-reference objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>
#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

@interface TempFileRef : NSObject <NSSecureCoding>

@property int tag;
@property glui32 rock;
@property int textmode;
@property int filetype;
@property glui32 prev;
@property glui32 next;

- (id) initWithCStruct:(fileref_t *)ref;
- (void) copyToCStruct:(fileref_t *)ref;
- (void) sanityCheck;

@end
