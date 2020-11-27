/* TempStream.h: Temporary Glk stream object
 objc class for serializing, adapted from IosGlk,
 the iOS implementation of the Glk API by Andrew Plotkin
 */

#import <Foundation/Foundation.h>
#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

@interface TempStream : NSObject <NSSecureCoding> {

    glui32 readcount, writecount;
    glui32 lastop;

	glui32 wintag;

    int unicode;
    int readable;
    int writable;
    int append;

    /* The pointers needed for stream operation. We keep separate sets for the one-byte and four-byte cases. */
    unsigned char *buf;
	unsigned char *bufptr;
	unsigned char *bufend;
	unsigned char *bufeof;
    glui32 *ubuf;
	glui32 *ubufptr;
	glui32 *ubufend;
	glui32 *ubufeof;
	glui32 buflen;
	gidispatch_rock_t arrayrock;

    NSURL *URL;

    int isbinary;
    glui32 resfilenum;

	uint8_t *tempbufdata;
	NSUInteger tempbufdatalen;
	long tempbufkey;
	glui32 tempbufptr, tempbufend, tempbufeof;

    unsigned long long offsetinfile; // (in bytes) only used during deserialization; zero normally
}

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
