/* TempLibrary.m: Temporary Glk library objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempLibrary.h"

@implementation TempLibrary

static void (*extra_archive_hook)(TempLibrary *, NSCoder *) = nil;
static void (*extra_unarchive_hook)(TempLibrary *, NSCoder *) = nil;

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype) init {

    window_t *win;
    stream_t *str;
    fileref_t *fref;
    channel_t *chan;

    self = [super init];

    if (self) {
        _timerInterval = gtimerinterval;

        _windows = [NSMutableArray arrayWithCapacity:8];
        _streams = [NSMutableArray arrayWithCapacity:8];
        _filerefs = [NSMutableArray arrayWithCapacity:8];
        _schannels = [NSMutableArray arrayWithCapacity:8];

        // We now run through all existing Glk objects, and create serializable Objective C objects for them.
        for (win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL))
            [_windows addObject:[[TempWindow alloc] initWithCStruct:win]];

        for (str = glk_stream_iterate(NULL, NULL); str; str = glk_stream_iterate(str, NULL))
            [_streams addObject:[[TempStream alloc] initWithCStruct:str]];

        for (fref = glk_fileref_iterate(NULL, NULL); fref; fref = glk_fileref_iterate(fref, NULL))
            [_filerefs addObject:[[TempFileRef alloc] initWithCStruct:fref]];

        for (chan = glk_schannel_iterate(NULL, NULL); chan; chan = glk_schannel_iterate(chan, NULL))
            [_schannels addObject:[[TempSChannel alloc] initWithCStruct:chan]];

        winid_t root = glk_window_get_root();
        if (root)
            _rootwintag = root->tag;
        else {
            /* Fall back to the first window without a parent. There may be
               no windows at all, e.g. between glk_window_close and the next
               glk_window_open. */
            for (root = glk_window_iterate(NULL, NULL); root; root = glk_window_iterate(root, NULL)) {
                if (root->parent == NULL) {
                    _rootwintag = root->tag;
                    break;
                }
            }
            if (!root && _windows.count)
                NSLog(@"TempLibrary init: Error: no root window!");
        }

        strid_t current = glk_stream_get_current();
        if (current)
            _currentstrtag = current->tag;
        else
            _currentstrtag = 0;

        _autosaveTag = generate_tag();
	}

    [self sanityCheck];
	return self;
}

- (instancetype) initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
	int version = [decoder decodeIntForKey:@"version"];
    if (version <= 0 || version != AUTOSAVE_SERIAL_VERSION)
    {
        NSLog(@"Wrong serial version!");
        return nil;
    }

	_windows = [decoder decodeObjectOfClass:[NSMutableArray class]  forKey:@"windows"];
    if (!_windows)
        NSLog(@"TempLibrary initWithCoder: No windows in archive file!");
	_streams = [decoder decodeObjectOfClass:[NSMutableArray class]  forKey:@"streams"];
    if (!_streams)
        NSLog(@"TempLibrary initWithCoder: No streams in archive file!");
	_filerefs = [decoder decodeObjectOfClass:[NSMutableArray class]  forKey:@"filerefs"];
    if (!_filerefs)
        NSLog(@"TempLibrary initWithCoder: No file references in archive file!");
    _schannels = [decoder decodeObjectOfClass:[NSMutableArray class]  forKey:@"schannels"];
    if (!_schannels)
        NSLog(@"TempLibrary initWithCoder: No sound channels in archive file!");

	// skip the calendar and filemanager fields; they're not needed

    _rootwintag = [decoder decodeInt32ForKey:@"rootwintag"];
    _currentstrtag = [decoder decodeInt32ForKey:@"currentstrtag"];

	for (TempWindow *win in _windows) {
		glui32 tag = win.tag;
		if (tag > tagcounter)
			tagcounter = tag;
	}
	for (TempStream *str in _streams) {
		glui32 tag = str.tag;
		if (tag > tagcounter)
			tagcounter = tag;
	}
	for (TempFileRef *fref in _filerefs) {
		glui32 tag = fref.tag;
		if (tag > tagcounter)
			tagcounter = tag;
	}
    for (TempSChannel *chan in _schannels) {
        glui32 tag = chan.tag;
        if (tag > tagcounter)
            tagcounter = tag;
    }

	// We don't worry about glkdelegate or the dispatch hooks. (Because this will only be used through updateFromLibrary). Similarly, none of the windows need stylesets yet, and none of the file streams are really open.

	// Load any interpreter-specific data.
	if (extra_unarchive_hook)
		extra_unarchive_hook(self, decoder);

    _timerInterval = [decoder decodeInt32ForKey:@"timerInterval"];
    _autosaveTag = [decoder decodeInt32ForKey:@"autosaveTag"];
    }
	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    int serial_version = AUTOSAVE_SERIAL_VERSION;
	[encoder encodeInt:serial_version forKey:@"version"];

	[encoder encodeObject:_windows forKey:@"windows"];
	[encoder encodeObject:_streams forKey:@"streams"];
	[encoder encodeObject:_filerefs forKey:@"filerefs"];
    [encoder encodeObject:_schannels forKey:@"schannels"];

    [encoder encodeInt32:_timerInterval forKey:@"timerInterval"];
    [encoder encodeInt32:_autosaveTag forKey:@"autosaveTag"];

	if (_rootwintag)
		[encoder encodeInt32:_rootwintag forKey:@"rootwintag"];
	if (_currentstrtag)
		[encoder encodeInt32:_currentstrtag forKey:@"currentstrtag"];

	// Save any interpreter-specific data.
	if (extra_archive_hook)
		extra_archive_hook(self, encoder);
}

/* Locate the window matching a given tag. (Or nil, if no window matches or the tag is nil.) This isn't efficient, but it's not heavily used.
 */
- (TempWindow *) windowForTag:(glui32)tag {
	if (!tag)
		return nil;

	for (TempWindow *win in _windows) {
		if (win.tag == tag)
			return win;
	}

	return nil;
}

- (TempStream *) streamForTag:(glui32)tag {

    if (!tag)
		return nil;

	for (TempStream *str in _streams) {
		if (str.tag == tag)
        {
			return str;
        }
	}
	return nil;
}

- (TempFileRef *) filerefForTag:(glui32)tag {
	if (!tag)
		return nil;

	for (TempFileRef *fref in _filerefs) {
		if (fref.tag == tag)
			return fref;
	}

	return nil;
}

- (TempSChannel *) schannelForTag:(glui32)tag {
	if (!tag)
		return nil;

	for (TempSChannel *chan in _schannels) {
		if (chan.tag == tag)
			return chan;
	}

	return nil;
}

- (TempWindow *) rootWindow {

	for (TempWindow *win in _windows) {
        if (win.tag == _rootwintag)
            return win;
    }

    return nil;
}

- (TempStream *) currentStream {

    for (TempStream *str in _streams) {
		if (str.tag == _currentstrtag)
			return str;
	}

	return nil;
}

/* Rebuild a C object list from the archived objects, which are stored in
   list order: start from the tail (the last one) and follow the prev tags
   back to the head. The structs are zeroed, as copyToCStruct does not set
   every field. */
#define REBUILD_C_LIST(objctype, ctype, array, lookup, head) do { \
    objctype *tempobj = (array).lastObject;                       \
    ctype *cobj = NULL;                                           \
    while (tempobj) {                                             \
        ctype *newobj = calloc(1, sizeof(ctype));                 \
        newobj->next = cobj;                                      \
        if (cobj)                                                 \
            cobj->prev = newobj;                                  \
        cobj = newobj;                                            \
        [tempobj copyToCStruct:cobj];                             \
        tempobj = [self lookup:tempobj.prev];                     \
    }                                                             \
    (head) = cobj;                                                \
} while (0)

- (void) updateFromLibrary {
    window_t *windowlist;
    stream_t *streamlist;
    fileref_t *filereflist;
    channel_t *channellist;

    REBUILD_C_LIST(TempWindow, window_t, _windows, windowForTag, windowlist);
    if (windowlist) {
        gli_replace_window_list(windowlist);
        if (_rootwintag)
            gli_window_set_root(gli_window_for_tag(_rootwintag));
    }

    REBUILD_C_LIST(TempStream, stream_t, _streams, streamForTag, streamlist);
    if (streamlist) {
        gli_replace_stream_list(streamlist);
        /* Restore current output stream */
        if (_currentstrtag)
            gli_stream_set_current(gli_stream_for_tag(_currentstrtag));
    }
    else NSLog(@"No streams in library! _streams.count = %lu", (unsigned long)_streams.count);

    if (!glk_stream_get_current())
        NSLog(@"ERROR: TempLibrary updateFromLibrary: could not update current stream!");

    REBUILD_C_LIST(TempFileRef, fileref_t, _filerefs, filerefForTag, filereflist);
    if (filereflist)
        gli_replace_fileref_list(filereflist);

    REBUILD_C_LIST(TempSChannel, channel_t, _schannels, schannelForTag, channellist);
    if (channellist)
        gli_replace_schan_list(channellist);

    /* Now that we have created all objects, we can set pointers to them */
    for (window_t *tempCwin = windowlist; tempCwin; tempCwin = tempCwin->next)
        [[self windowForTag:tempCwin->tag] finalizeCStruct:tempCwin];

    NSMutableArray *failedstreams = [NSMutableArray arrayWithCapacity:4];
    for (TempStream *str in _streams) {
        if (str.type == strtype_File) {
            BOOL res = [str reopenInternal];
            if (!res)
                [failedstreams addObject:str];
        }
    }

    for (TempStream *str in failedstreams) {
        NSLog(@"### stream %d failed to reopen; closing it", str.tag);
        stream_t *tempCstream = gli_stream_for_tag(str.tag);
        if (tempCstream)
        {
            tempCstream->file = NULL;
            glk_stream_close(tempCstream, NULL);
        }
    }
}

- (void)updateFromLibraryLate {
    for (TempStream *str in _streams)
        if (str.type == strtype_Resource)
            [str updateResource];

    gli_sanity_check_windows();
    gli_sanity_check_streams();
    gli_sanity_check_filerefs();

    glk_request_timer_events(_timerInterval);
}

- (void) sanityCheck {
#ifdef DEBUG

    if (_rootwintag && !_windows.count)
        NSLog(@"SANITY: root window but no listed windows");
    if (!_rootwintag && _windows.count)
        NSLog(@"SANITY: no root window but listed windows");
    if (_rootwintag && ![self windowForTag:_rootwintag])
        NSLog(@"SANITY: root window not listed");

    if (_currentstrtag && ![self streamForTag:_currentstrtag])
        NSLog(@"SANITY: current stream not listed");

    for (TempWindow *win in _windows) {
        [win sanityCheckWithLibrary:self];
    }
    
    for (TempStream *str in _streams) {
        [str sanityCheck];
    }
    
    for (TempFileRef *fref in _filerefs) {
        [fref sanityCheck];
    }
    
#endif // DEBUG
}

/* Set the global hook which is called whenever TempLibrary is serialized.
 */
+ (void) setExtraArchiveHook:(void (*)(TempLibrary *, NSCoder *))hook {
	extra_archive_hook = hook;
} 

/* Set the global hook which is called whenever TempLibrary is deserialized.
 */
+ (void) setExtraUnarchiveHook:(void (*)(TempLibrary *, NSCoder *))hook {
	extra_unarchive_hook = hook;
}

@end
