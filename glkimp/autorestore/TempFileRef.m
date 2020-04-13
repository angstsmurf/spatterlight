/* TempFileRef.m: Temporary file-reference objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempFileRef.h"

@implementation TempFileRef

/* Work out the directory for a given type of file, based on the base directory, the usage, and the game identity.
 See the comments on GlkFileRefLayer.m for an explanation.
 */
//+ (NSString *) subDirOfBase:(NSString *)basedir forUsage:(glui32)usage gameid:(NSString *)gameid {
//	NSString *subdir;
//
//	switch (usage & fileusage_TypeMask) {
//		case fileusage_SavedGame:
//			subdir = [NSString stringWithFormat:@"GlkSavedGame_%@", gameid];
//			break;
//		case fileusage_InputRecord:
//			subdir = @"GlkInputRecord";
//			break;
//		case fileusage_Transcript:
//			subdir = @"GlkTranscript";
//			break;
//		case fileusage_Data:
//		default:
//			subdir = @"GlkData";
//			break;
//	}
//
//	NSString *dirname = [basedir stringByAppendingPathComponent:subdir];
//	return dirname;
//}

- (id) initWithCStruct:(fileref_t *)ref {

    self = [super init];

	if (self) {
        _tag = ref->tag;
        _rock = ref->rock;
        if (ref->filename)
            URL = [NSURL fileURLWithPath:@(ref->filename)];
        else
            URL = nil;
        _filetype = ref->filetype;
        _textmode = ref->textmode;

        _next = _prev = 0;

        if (ref->next)
            _next = ref->next->tag;
        if (ref->prev)
            _prev = ref->prev->tag;
    }
    return self;
}

- (id) initWithCoder:(NSCoder *)decoder {
	_tag = [decoder decodeInt32ForKey:@"tag"];
	_rock = [decoder decodeInt32ForKey:@"rock"];

    _prev = [decoder decodeInt32ForKey:@"prev"];
    _next = [decoder decodeInt32ForKey:@"next"];

    NSData *bookmark = [decoder decodeObjectForKey:@"bookmark"];
    URL = [NSURL URLByResolvingBookmarkData:bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:nil error:nil];
    if (!URL)
        URL = [decoder decodeObjectForKey:@"URL"];
    _filetype = [decoder decodeInt32ForKey:@"filetype"];
    _textmode = [decoder decodeInt32ForKey:@"textmode"];

	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
	[encoder encodeInt32:_tag forKey:@"tag"];
	[encoder encodeInt32:_rock forKey:@"rock"];

    [encoder encodeInt32:_prev forKey:@"prev"];
    [encoder encodeInt32:_next forKey:@"next"];

    NSData *bookmark = nil;
    if (URL)
    {
        NSError* theError = nil;
        bookmark = [URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                 includingResourceValuesForKeys:nil
                                  relativeToURL:nil
                                          error:&theError];
        if (theError || !bookmark)
            NSLog(@"Error when encoding bookmark: %@", theError);
    }

    [encoder encodeObject:bookmark forKey:@"bookmark"];
    [encoder encodeObject:URL forKey:@"URL"];
	[encoder encodeInt32:_filetype forKey:@"filetype"];
	[encoder encodeInt32:_textmode forKey:@"textmode"];
}

- (void) copyToCStruct:(fileref_t *)ref{

    ref->magicnum = MAGIC_FILEREF_NUM;

    ref->tag = _tag;
    ref->rock = _rock;
    ref->filetype = _filetype;
    ref->textmode = _textmode;
    if (URL)
    {
        const char *path = [URL.path UTF8String];
        char *urlpath = malloc(1 + strlen(path));

        strncpy(urlpath, path, strlen(path));
        urlpath[strlen(path)] = '\0';
        ref->filename = urlpath;
//        NSLog(@"TempFileRef copyToCStruct: set filename of fileref %d to %s",_tag, ref->filename);
    }
    else
    {
        NSLog(@"TempFileRef copyToCStruct: no URL for fileref %d, will have no filename",_tag);
        ref->filename = NULL;
    }
}

- (void) sanityCheck
{
    if (!URL)
        NSLog(@"SANITY: fileref has no pathname");
}

@end
