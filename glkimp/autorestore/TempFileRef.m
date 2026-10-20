/* TempFileRef.m: Temporary file-reference objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempFileRef.h"
#include "fileref.h"

@interface TempFileRef () <NSSecureCoding> {
    NSURL *URL;
}
@end

@implementation TempFileRef

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype) initWithCStruct:(fileref_t *)ref {

    self = [super init];

	if (self) {
        _tag = ref->tag;
        _rock = ref->rock;
        if (ref->filename)
            URL = [NSURL fileURLWithPath:@(ref->filename) isDirectory:NO];
        else
            URL = nil;
        _filetype = ref->filetype;
        _textmode = ref->textmode;

        _prev = 0;

        if (ref->prev)
            _prev = ref->prev->tag;
    }
    return self;
}

- (instancetype) initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
	_tag = [decoder decodeInt32ForKey:@"tag"];
	_rock = [decoder decodeInt32ForKey:@"rock"];

    _prev = [decoder decodeInt32ForKey:@"prev"];

    URL = gli_decode_file_url(decoder, NO);
    _filetype = [decoder decodeInt32ForKey:@"filetype"];
    _textmode = [decoder decodeInt32ForKey:@"textmode"];
    }
	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
	[encoder encodeInt32:_tag forKey:@"tag"];
	[encoder encodeInt32:_rock forKey:@"rock"];

    [encoder encodeInt32:_prev forKey:@"prev"];

    gli_encode_file_url(encoder, URL);
	[encoder encodeInt32:_filetype forKey:@"filetype"];
	[encoder encodeInt32:_textmode forKey:@"textmode"];
}

- (void) copyToCStruct:(fileref_t *)ref{

    ref->tag = _tag;
    ref->rock = _rock;
    ref->filetype = _filetype;
    ref->textmode = _textmode;
    const char *path = URL.fileSystemRepresentation;
    ref->filename = path ? strdup(path) : NULL;
    if (!path)
        NSLog(@"TempFileRef copyToCStruct: no URL for fileref %d, will have no filename",_tag);
}

- (void) sanityCheck
{
    if (!URL)
        NSLog(@"SANITY: fileref has no pathname");
}

@end
