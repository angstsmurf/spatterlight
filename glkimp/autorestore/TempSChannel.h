/*  TempSChannel.h: Temporary sound channel objc class
for serializing, inspired by IosGlk, the iOS
implementation of the Glk API by Andrew Plotkin
*/

#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

#import <Foundation/Foundation.h>

@interface TempSChannel : NSObject <NSSecureCoding>

@property glui32 tag;
@property glui32 rock;
@property glui32 peer;
@property glui32 prev;
@property glui32 next;

- (id) initWithCStruct:(channel_t *)chan;
- (void) copyToCStruct:(channel_t *)chan;

@end
