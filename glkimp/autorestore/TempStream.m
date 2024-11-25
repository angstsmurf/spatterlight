/* TempStream.m: Temporary Glk stream object
 objc class for serializing, adapted from IosGlk,
 the iOS implementation of the Glk API by Andrew Plotkin
 */

#import "TempStream.h"

@interface TempStream () <NSSecureCoding> {

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
@end

@implementation TempStream

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype) initWithCStruct:(stream_t *)str {
    
    self = [super init];
    
    if (self) {
        
        _type = str->type;
        _rock = str->rock;
        _tag = str->tag;
        
        unicode = str->unicode;
        readable = str->readable;
        writable = str->writable;
        
        readcount = str->readcount;
        writecount = str->writecount;
        lastop = str->lastop;
        
        isbinary = str->isbinary;
        resfilenum = str->resfilenum;
        
        if (str->win && str->type == strtype_Window) {
            wintag = str->win->tag;
        } else wintag = 0;
        
        URL = nil;
        if (str->filename)
            URL = [NSURL fileURLWithPath:@(str->filename) isDirectory:NO];
        _file = str->file;
        
        /* for strtype_Memory and strtype_Resource. Separate pointers for
         one-byte and four-byte streams */
        buf = str->buf;
        bufptr = str->bufptr;
        bufend = str->bufend;
        bufeof = str->bufeof;
        ubuf = str->ubuf;
        ubufptr = str->ubufptr;
        ubufend = str->ubufend;
        ubufeof = str->ubufeof;
        
        buflen = str->buflen;
        
        if (_type == strtype_Memory && !unicode && ubuf)
            NSLog(@"TempStream initiWithCStruct: memory stream %d is not unicode, but has ubuf", str->tag);
        
        _next = _prev = 0;
        
        if (str->next)
            _next = (str->next)->tag;
        if (str->prev)
            _prev = (str->prev)->tag;
        
        arrayrock = str->arrayrock;
    }
    return self;
}

- (instancetype) initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    buf = 0;
    bufptr = 0;
    bufeof = 0;
    ubuf = 0;
    ubufptr = 0;
    ubufeof = 0;
    _file = nil;

    _tag = [decoder decodeInt32ForKey:@"tag"];

    _type = [decoder decodeInt32ForKey:@"type"];
    _rock = [decoder decodeInt32ForKey:@"rock"];

    _prev = [decoder decodeInt32ForKey:@"prev"];
    _next = [decoder decodeInt32ForKey:@"next"];

    wintag = [decoder decodeInt32ForKey:@"wintag"];

    unicode = [decoder decodeInt32ForKey:@"unicode"];

    readcount = [decoder decodeInt32ForKey:@"readcount"];
    writecount = [decoder decodeInt32ForKey:@"writecount"];
    readable = [decoder decodeInt32ForKey:@"readable"];
    writable = [decoder decodeInt32ForKey:@"writable"];
    append = [decoder decodeIntForKey:@"append"];

    isbinary = [decoder decodeInt32ForKey:@"isbinary"];
    resfilenum = [decoder decodeInt32ForKey:@"resfilenum"];

    buflen = [decoder decodeInt32ForKey:@"buflen"];

    if (_type == strtype_Memory) {
        // the decoded "buf" values are originally Glulx addresses (glui32), so stuffing them into a long is safe.
        if (!unicode) {
            tempbufkey = (long)[decoder decodeInt64ForKey:@"buf"];
            tempbufptr = [decoder decodeInt32ForKey:@"bufptr"];
            tempbufeof = [decoder decodeInt32ForKey:@"bufeof"];
            tempbufend = [decoder decodeInt32ForKey:@"bufend"];
            uint8_t *rawdata;
            NSUInteger rawdatalen;
            rawdata = (uint8_t *)[decoder decodeBytesForKey:@"bufdata" returnedLength:&rawdatalen];
            if (rawdata && rawdatalen) {
                tempbufdatalen = rawdatalen;
                tempbufdata = malloc(rawdatalen);
                memcpy(tempbufdata, rawdata, rawdatalen);
            }
        }
        else {
            tempbufkey = (long)[decoder decodeInt64ForKey:@"ubuf"];
            tempbufptr = [decoder decodeInt32ForKey:@"ubufptr"];
            tempbufeof = [decoder decodeInt32ForKey:@"ubufeof"];
            tempbufend = [decoder decodeInt32ForKey:@"ubufend"];
            uint8_t *rawdata;
            NSUInteger rawdatalen;
            rawdata = (uint8_t *)[decoder decodeBytesForKey:@"ubufdata" returnedLength:&rawdatalen];
            if (rawdata && rawdatalen) {
                tempbufdatalen = rawdatalen;
                tempbufdata = malloc(rawdatalen);
                memcpy(tempbufdata, rawdata, rawdatalen);
            }
        }
    }

    if (_type == strtype_Resource) {
        tempbufptr = [decoder decodeInt32ForKey:@"bufptr"];
        tempbufend = [decoder decodeInt32ForKey:@"bufend"];
        tempbufeof = [decoder decodeInt32ForKey:@"bufeof"];
    }

    NSData *bookmark =  [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];

    URL = [NSURL URLByResolvingBookmarkData:bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:nil error:nil];
    if (!URL)
        URL = [decoder decodeObjectOfClass:[NSURL class] forKey:@"URL"];
    lastop = [decoder decodeInt32ForKey:@"lastop"];

    offsetinfile = [decoder decodeInt64ForKey:@"offsetinfile"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {

	[encoder encodeInt32:_tag forKey:@"tag"];
	[encoder encodeInt32:_type forKey:@"type"];
	[encoder encodeInt32:_rock forKey:@"rock"];

    [encoder encodeInt32:_prev forKey:@"prev"];
    [encoder encodeInt32:_next forKey:@"next"];

    if (_type == strtype_Window)
        [encoder encodeInt32:wintag forKey:@"wintag"];
    [encoder encodeInt32:unicode forKey:@"unicode"];

	[encoder encodeInt32:readcount forKey:@"readcount"];
	[encoder encodeInt32:writecount forKey:@"writecount"];
    [encoder encodeInt32:readable forKey:@"readable"];
    [encoder encodeInt32:writable forKey:@"writable"];
    [encoder encodeInt32:append forKey:@"append"];

    [encoder encodeInt32:lastop forKey:@"lastop"];
    [encoder encodeInt32:isbinary forKey:@"isbinary"];
    [encoder encodeInt32:resfilenum forKey:@"resfilenum"];

	[encoder encodeInt32:buflen forKey:@"buflen"];

    if (_type == strtype_Memory) {

        if (!gli_locate_arr) {
            [NSException raise:@"GlkException" format:@"TempStream cannot be encoded without app support"];
        }

        long bufaddr = 0;
        int elemsize;
        if (!unicode) {
            if (buf && buflen) {
                bufaddr = (*gli_locate_arr)(buf, buflen, "&+#!Cn", arrayrock, &elemsize);
                [encoder encodeInt64:bufaddr forKey:@"buf"];
                [encoder encodeInt32:(int32_t)(bufptr - buf) forKey:@"bufptr"];
                [encoder encodeInt32:(int32_t)(bufeof - buf) forKey:@"bufeof"];
                [encoder encodeInt32:(int32_t)(bufend - buf) forKey:@"bufend"];
                if (elemsize) {
                    NSAssert(elemsize == 1, @"GlkStreamMemory encoding char array: wrong elemsize");
                    // could trim trailing zeroes here
                    [encoder encodeBytes:(uint8_t *)buf length:buflen forKey:@"bufdata"];
                }
            }
            else if (buflen)
                NSLog(@"encodeWithCoder: Encoding stream %d with buflen %lu but no buf!", _tag, (unsigned long)buflen);
        }
        else {
            if (ubuf && buflen) {
                bufaddr = (*gli_locate_arr)(ubuf, buflen, "&+#!Iu", arrayrock, &elemsize);
                [encoder encodeInt64:bufaddr forKey:@"ubuf"];
                [encoder encodeInt32:(int32_t)(ubufptr - ubuf) forKey:@"ubufptr"];
                [encoder encodeInt32:(int32_t)(ubufeof - ubuf) forKey:@"ubufeof"];
                [encoder encodeInt32:(int32_t)(ubufend - ubuf) forKey:@"ubufend"];
                if (elemsize) {
                    NSAssert(elemsize == 4, @"GlkStreamMemory encoding uni array: wrong elemsize");
                    // could trim trailing zeroes here
                    [encoder encodeBytes:(uint8_t *)ubuf length:sizeof(glui32)*buflen forKey:@"ubufdata"];
                }
            }
            else {
                [encoder encodeInt64:0 forKey:@"ubuf"];
                [encoder encodeInt32:0 forKey:@"ubufptr"];
                [encoder encodeInt32:0 forKey:@"ubufeof"];
                [encoder encodeInt32:0 forKey:@"ubufend"];
                [encoder encodeBytes:0 length:0 forKey:@"ubufdata"];
                if (buflen)
                    NSLog(@"Encoding unicode stream %d with buflen %lu but no ubuf!", _tag, (unsigned long)buflen);
            }
        }
    }
    else if (_type == strtype_Resource) {
        if (buf && buflen) {
            [encoder encodeInt32:(int32_t)(bufptr - buf) forKey:@"bufptr"];
            [encoder encodeInt32:(int32_t)(bufeof - buf) forKey:@"bufeof"];
            [encoder encodeInt32:(int32_t)(bufend - buf) forKey:@"bufend"];
        }
    }
    else {
        [encoder encodeInt64:0 forKey:@"buf"];
        [encoder encodeInt32:0 forKey:@"bufptr"];
        [encoder encodeInt32:0 forKey:@"bufeof"];
        [encoder encodeInt32:0 forKey:@"bufend"];

        [encoder encodeInt64:0 forKey:@"ubuf"];
        [encoder encodeInt32:0 forKey:@"ubufptr"];
        [encoder encodeInt32:0 forKey:@"ubufeof"];
        [encoder encodeInt32:0 forKey:@"ubufend"];
    }

    // [self flush];

    NSData *bookmark = nil;
    if (URL)
    {
        NSError* theError = nil;
        bookmark = [URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
      includingResourceValuesForKeys:nil
                       relativeToURL:nil
                               error:&theError];
        if (theError || !bookmark)
            NSLog(@"Error when encoding bookmark %@: %@", URL.path, theError);
    }

    if (bookmark)
        [encoder encodeObject:bookmark forKey:@"bookmark"];
    if (URL)
        [encoder encodeObject:URL forKey:@"URL"];

    offsetinfile = 0;
    if (_file)
    {
        offsetinfile = ftell(_file);
        fflush(_file);
    }
    [encoder encodeInt64:offsetinfile forKey:@"offsetinfile"];
}

- (void) updateRegisterArray {
	if (!gli_restore_arr) {
		[NSException raise:@"GlkException" format:@"GlkStreamMemory cannot be updated-from without app support"];
	}

    if (!unicode) {
		void *voidbuf = nil;
		arrayrock = (*gli_restore_arr)(tempbufkey, buflen, "&+#!Cn", &voidbuf);
		if (voidbuf) {
			buf = voidbuf;
			bufptr = buf + tempbufptr;
			bufeof = buf + tempbufeof;
			bufend = buf + tempbufend;
			if (tempbufdata) {
				if (tempbufdatalen > buflen)
					tempbufdatalen = buflen;
				memcpy(buf, tempbufdata, tempbufdatalen);
                NSLog(@"Copied data for stream %d with length %lu", _tag, (unsigned long)buflen);
				free(tempbufdata);
				tempbufdata = nil;
			}
		}
	}
	else {
		void *voidbuf = nil;
		arrayrock = (*gli_restore_arr)(tempbufkey, buflen, "&+#!Iu", &voidbuf);
		if (voidbuf) {
			ubuf = voidbuf;
			ubufptr = ubuf + tempbufptr;
			ubufeof = ubuf + tempbufeof;
			ubufend = ubuf + tempbufend;
			if (tempbufdata) {
				if (tempbufdatalen > sizeof(glui32)*buflen)
					tempbufdatalen = sizeof(glui32)*buflen;
				memcpy(ubuf, tempbufdata, tempbufdatalen);
                NSLog(@"Copied unicode data for stream %d with length %lu", _tag, (unsigned long)buflen);
				free(tempbufdata);
				tempbufdata = nil;
			}
		}
	}
}

- (void) copyToCStruct:(stream_t *)str {
    if (_type == strtype_Memory)
        [self updateRegisterArray];

    str->magicnum = MAGIC_STREAM_NUM;

    str->tag = _tag;
    str->rock = _rock;
    str->type = _type;

    str->win = gli_window_for_tag(wintag);
    if (wintag && !str->win)
        NSLog(@"TempStream copyToCStruct: could not find window struct for window %d", wintag);
    str->unicode = unicode;
    str->readcount = readcount;
    str->writecount = writecount;
    str->readable = readable;
    str->writable = writable;
    str->lastop = lastop;
    str->isbinary = isbinary;
    str->resfilenum = resfilenum;

    //    /* for strtype_Memory and strtype_Resource. Separate pointers for
    //     one-byte and four-byte streams */

    str->buf = buf;
    str->bufptr = bufptr;
    str->bufend = bufend;
    str->bufeof = bufeof;
    str->ubuf = ubuf;
    str->ubufptr = ubufptr;
    str->ubufend = ubufend;
    str->ubufeof = ubufeof;
    str->arrayrock.ptr = arrayrock.ptr;
    str->arrayrock.num = arrayrock.num;
    str->file = NULL;
    if (URL)
    {
        const char *path = URL.fileSystemRepresentation;
        size_t len = strlen(path);
        char *urlpath = malloc(1 + len);
        strncpy(urlpath, path, len);
        urlpath[len] = '\0';
        str->filename = urlpath;
    }
    else
    {
        str->filename = NULL;
    }
    str->buflen = buflen;
}

/* Open the file handle after a deserialize, and seek to the appropriate point. Called from TempLibrary.updateFromLibrary.
 */
- (BOOL) reopenInternal {

    stream_t *str = gli_stream_for_tag(_tag);
    const char *path = URL.fileSystemRepresentation;

    if (![[NSFileManager defaultManager] fileExistsAtPath:URL.path]) {

        NSString *parent = @(gli_parentdir);
        NSString *filename = URL.lastPathComponent;
        NSString *newPath = [parent stringByAppendingPathComponent:filename];
        if ([[NSFileManager defaultManager] fileExistsAtPath:newPath]) {
            NSLog(@"TempStream reopenInternal: Changed path from %@ to %@ for stream %d.", URL.path, newPath, _tag);
            path = newPath.fileSystemRepresentation;
            if (str->filename != NULL)
                free(str->filename);
            size_t len = strlen(path);
            char *urlpath = malloc(1 + len);
            strncpy(urlpath, path, len);
            urlpath[len] = '\0';
            str->filename = urlpath;
        } else if (str->writable) {
            // If a file is writable (and not, say, a blorb file) we create a new one as a last resort
            str->file = fopen(path,"w");
            if (!str->file)
            {
                NSLog(@"TempStream reopenInternal: Could not create file at %@ for stream %d.", URL.path, _tag);
                return NO;
            }
            NSLog(@"TempStream reopenInternal: Could not find file at %@ for stream %d, so created a new one.", URL.path, _tag);
            fclose(str->file);
        } else {
            NSLog(@"TempStream reopenInternal: Could not reopen file at %@ for stream %d.", URL.path, _tag);
            return NO;
        }
    }

    str->file = NULL;
    if (writable) {
    str->file = fopen(path, "r+");
    }
    else {
        str->file = fopen(path, "r");
    }

    if (!str->file) {
        return NO;
    }

    fseek(str->file, offsetinfile, SEEK_SET);
    return YES;
}

- (void) updateResource {

    stream_t *str = gli_stream_for_tag(_tag);

    giblorb_err_t err;
    giblorb_result_t res;
    giblorb_map_t *map = giblorb_get_resource_map();
    if (!map)
        [NSException raise:@"GlkException" format:@"Error when loading resource map!"];

    err = giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Data, resfilenum);
   if (err)
//        [NSException raise:@"GlkException" format:@"Error when loading blorb resource!"];
        NSLog(@"Error %d when loading blorb resource!", err);


    if (res.data.ptr && res.length) {
        buf = (unsigned char *)res.data.ptr;
        if (buflen != res.length)
            NSLog(@"Resource stream %d has wrong buflen (%lu), should be %u!", _tag, (unsigned long)buflen, res.length);

        bufptr = buf + tempbufptr;
        bufend = buf + tempbufend;
        bufeof = buf + tempbufeof;

        str->buf = buf;
        str->bufptr = bufptr;
        str->bufend = bufend;
        str->bufeof = bufeof;
    }
    else [NSException raise:@"GlkException" format:@"Error when loading blorb resource!"];
}

- (void) sanityCheck
{
    if (!_type)
        NSLog(@"SANITY: stream lacks type");
    if (!(readable || writable))
        NSLog(@"SANITY: stream should be readable or writable");

    switch (_type)
    {
        case strtype_Window: {
            if (!gli_window_for_tag(wintag))
                NSLog(@"SANITY: window stream has no window");
            break;

        case strtype_File: {
            if (!URL)
                NSLog(@"SANITY: file stream lacks pathname");
            if (!_file)
                NSLog(@"SANITY: file stream lacks file handle");
        }
            break;

        case strtype_Memory: {
            // Remember, the buffer may be nil as long as the length limit is zero
            if (unicode && buf)
                NSLog(@"SANITY: memory stream %d is unicode, but has buf", _tag);
            if (!unicode && ubuf)
                NSLog(@"SANITY: memory stream %d is not unicode, but has ubuf", _tag);
            if (!buf && !ubuf && buflen)
                NSLog(@"SANITY: memory stream has no buffer, but positive buflen");
        }
            break;

        default:
            break;

        }
    }
}

@end
