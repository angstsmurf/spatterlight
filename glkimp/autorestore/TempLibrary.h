/* TempLibrary.h: Temporary Glk library objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>

#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

#import "TempWindow.h"
#import "TempStream.h"
#import "TempFileRef.h"
#import "TempSChannel.h"

@interface TempLibrary : NSObject <NSSecureCoding> 

/* Library top-level data

Current window size
All windows, streams, filerefs, and schannels
Identity of the root window
Identity of the current selected output stream */

//@property CGRect bounds;
@property NSMutableArray *windows;
@property NSMutableArray *streams;
@property NSMutableArray *filerefs;
@property NSMutableArray *schannels;

@property glui32 rootwintag;
@property glui32 currentstrtag;

@property glui32 timerInterval;
@property glui32 autosaveTag;

@property (nonatomic, retain) id extraData;

//@property id specialrequest;

+ (void) setExtraArchiveHook:(void (*)(TempLibrary *, NSCoder *))hook;
+ (void) setExtraUnarchiveHook:(void (*)(TempLibrary *, NSCoder *))hook;

- (void) strictWarning:(NSString *)msg;

- (TempWindow *) windowForTag:(glui32)tag;
- (TempStream *) streamForTag:(glui32)tag;
- (TempFileRef *) filerefForTag:(glui32)tag;

- (TempWindow *) getRootWindow;
- (TempStream *) getCurrentStream;

- (void) sanityCheck;
- (void) updateFromLibrary;
- (void) updateFromLibraryLate;

@end
