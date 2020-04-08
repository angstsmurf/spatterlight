/* TempLibrary.m: Temporary Glk library objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempLibrary.h"

@implementation TempLibrary

#define SERIAL_VERSION (1)

//static TempLibrary *singleton = nil;
static void (*extra_archive_hook)(TempLibrary *, NSCoder *) = nil;
static void (*extra_unarchive_hook)(TempLibrary *, NSCoder *) = nil;

static window_t *temp_windowlist = NULL; /* linked list of all windows */

static stream_t *temp_streamlist = NULL; /* linked list of all streams */

static fileref_t *temp_filereflist = NULL;  /* linked list of all filerefs */
static channel_t *temp_channellist = NULL;  /* linked list of all sound channels */

//+ (TempLibrary *) singleton {
//	return singleton;
//}

- (instancetype) init {

    window_t *win;
    stream_t *str;
    fileref_t *fref;
    channel_t *chan;

    self = [super init];

    if (self) {

        program_name = @(gli_program_name);
        program_info = @(gli_program_info);
        story_name = @(gli_story_name);
        story_title = @(gli_story_title);

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
            //NSLog(@"TempLibrary init: Error: no root window!");
            root = glk_window_iterate(NULL, NULL);
            _rootwintag = root->tag;
            //NSLog(@"TempLibrary init: Setting root to first window in list!");
        }

        strid_t current = glk_stream_get_current();
		_currentstrtag = current->tag;
	}

    [self sanityCheck];
	return self;
}

- (id) initWithCoder:(NSCoder *)decoder {

	int version = [decoder decodeIntForKey:@"version"];
    if (version <= 0 || version > SERIAL_VERSION)
    {
        NSLog(@"Wrong serial version!");
        return nil;
    }

    //	/* If the vm has exited, we shouldn't have saved the state! */
    //	_vmexited = NO;
    //	self.specialrequest = nil;

	//_bounds = [decoder decodeRectForKey:@"bounds"];
    //	_geometrychanged = YES;
    //	_metricschanged = YES;
    //	_everythingchanged = YES;

    program_name = [decoder decodeObjectForKey:@"program_name"];
    program_info = [decoder decodeObjectForKey:@"program_info"];
    story_name = [decoder decodeObjectForKey:@"story_name"];
    story_title = [decoder decodeObjectForKey:@"story_title"];
    if (program_name != NULL)
        garglk_set_program_name([program_name UTF8String]);
    if (program_info != NULL)
        garglk_set_program_info([program_info UTF8String]);
    if (story_name != NULL)
        garglk_set_story_name([story_name UTF8String]);
    if (story_title != NULL)
        garglk_set_story_title([story_title UTF8String]);

	_windows = [decoder decodeObjectForKey:@"windows"];
    if (!_windows)
        NSLog(@"TempLibrary initWithCoder: No windows in archive file!");
	_streams = [decoder decodeObjectForKey:@"streams"];
    if (!_streams)
        NSLog(@"TempLibrary initWithCoder: No streams in archive file!");
	_filerefs = [decoder decodeObjectForKey:@"filerefs"];
    if (!_filerefs)
        NSLog(@"TempLibrary initWithCoder: No file references in archive file!");
    _schannels = [decoder decodeObjectForKey:@"schannels"];
    if (!_schannels)
        NSLog(@"TempLibrary initWithCoder: No sound channels in archive file!");

	// will be zero if no timerinterval was saved
	// timerinterval = [decoder decodeInt32ForKey:@"timerinterval"];

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

	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
//    NSLog(@"### TempLibrary: encoding with %ld windows, %ld streams, %ld filerefs, %ld sound channels", (unsigned long)_windows.count, (unsigned long)_streams.count, (unsigned long)_filerefs.count, (unsigned long)_schannels.count);
	[encoder encodeInt:SERIAL_VERSION forKey:@"version"];

    [encoder encodeObject:program_name forKey:@"program_name"];
    [encoder encodeObject:program_info forKey:@"program_info"];
    [encoder encodeObject:story_name forKey:@"story_name"];
    [encoder encodeObject:story_title forKey:@"story_title"];

	[encoder encodeObject:_windows forKey:@"windows"];
	[encoder encodeObject:_streams forKey:@"streams"];
	[encoder encodeObject:_filerefs forKey:@"filerefs"];
    [encoder encodeObject:_schannels forKey:@"schannels"];

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
            //NSLog(@"streamForTag:found stream object with a tag of %d (%@)",tag, str);
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

- (TempWindow *) getRootWindow {

	for (TempWindow *win in _windows) {
        if (win.tag == _rootwintag)
            return win;
    }

    return nil;
}

- (TempStream *) getCurrentStream {

    for (TempStream *str in _streams) {
		if (str.tag == _currentstrtag)
			return str;
	}

	return nil;
}

- (void) clearForRestart {

    window_t *rootwin = glk_window_get_root();

    if (rootwin) {
        // This takes care of all the windows
        glk_window_close(rootwin, NULL);
    }
    stream_t *str = glk_stream_iterate(NULL, NULL);

    while (str) {
        glk_stream_close(str, NULL);
        str = glk_stream_iterate(NULL, NULL);
    }

    fileref_t *fref = glk_fileref_iterate(NULL, NULL);

    while (fref) {
        glk_fileref_destroy(fref);
        fref = glk_fileref_iterate(NULL, NULL);
    }

    channel_t *chan = glk_schannel_iterate(NULL, NULL);

    while (chan) {
        glk_schannel_destroy(chan);
        chan = glk_schannel_iterate(NULL, NULL);
    }

    glk_request_timer_events(0);

    //    NSAssert(windows.count == 0 && streams.count == 0 && filerefs.count == 0, @"clearForRestart: unclosed objects remain!");
    //    NSAssert(currentstr == nil && rootwin == nil, @"clearForRestart: root references remain!");
}

- (void) updateFromLibrary {

    //	self.specialrequest = otherlib.specialrequest;

    TempWindow *tempwin = _windows.lastObject;
    window_t *tempCwin = NULL;

	if (tempwin) {
        tempCwin = (window_t *)malloc(sizeof(window_t));
        tempCwin->next = NULL;

        [tempwin copyToCStruct:tempCwin];

        tempwin = [self windowForTag:tempwin.prev];

        while (tempwin) {
            tempCwin->prev = (window_t *)malloc(sizeof(window_t));
            tempCwin->prev->next = tempCwin;
            tempCwin = tempCwin->prev;

            [tempwin copyToCStruct:tempCwin];
            if (tempwin.tag == _rootwintag)
                gli_window_set_root(tempCwin);

            tempwin = [self windowForTag:tempwin.prev];
        }

        tempCwin->prev = NULL;
        temp_windowlist = tempCwin;

        gli_replace_window_list(temp_windowlist);
    }

    TempStream *tempstream = _streams.lastObject;
    stream_t *tempCstream = NULL;

	if (tempstream) {
        tempCstream = (stream_t *)malloc(sizeof(stream_t));
        tempCstream->next = NULL;
        [tempstream copyToCStruct:tempCstream];

        tempstream = [self streamForTag:tempstream.prev];

        while (tempstream) {
            tempCstream->prev = (stream_t *)malloc(sizeof(stream_t));
            tempCstream->prev->next = tempCstream;

            tempCstream = tempCstream->prev;
            [tempstream copyToCStruct:tempCstream];

            /* Restore current output stream */
            if (tempstream.tag == _currentstrtag)
                gli_stream_set_current(tempCstream);

            tempstream = [self streamForTag:tempstream.prev];
        }

        tempCstream->prev = NULL;
        temp_streamlist = tempCstream;
        gli_replace_stream_list(temp_streamlist);
    }
    else NSLog(@"No streams in library! _streams.count = %lu", (unsigned long)_streams.count);

    if (!glk_stream_get_current())
        NSLog(@"ERROR: TempLibrary updateFromLibrary: could not update current stream!");

    TempFileRef *tempfileref = _filerefs.lastObject;
    fileref_t *tempCfileref = NULL;

	if (tempfileref) {
        tempCfileref = (fileref_t *)malloc(sizeof(fileref_t));
        tempCfileref->next = NULL;
        [tempfileref copyToCStruct:tempCfileref];

        tempfileref = [self filerefForTag:tempfileref.prev];

        while (tempfileref) {
            tempCfileref->prev = (fileref_t *)malloc(sizeof(fileref_t));
            tempCfileref->prev->next = tempCfileref;

            tempCfileref = tempCfileref->prev;
            [tempfileref copyToCStruct:tempCfileref];

            tempfileref = [self filerefForTag:tempfileref.prev];
        }

        tempCfileref->prev = NULL;
        temp_filereflist = tempCfileref;
        gli_replace_fileref_list(temp_filereflist);
    }

    TempSChannel *tempchannel = _schannels.lastObject;
    channel_t *tempCchannel = NULL;

	if (tempchannel) {
        tempCchannel = (channel_t *)malloc(sizeof(channel_t));
        tempCchannel->next = NULL;
        [tempchannel copyToCStruct:tempCchannel];

        tempchannel = [self schannelForTag:tempchannel.prev];

        while (tempchannel) {
            tempCchannel->prev = (channel_t *)malloc(sizeof(channel_t));
            tempCchannel->prev->next = tempCchannel;

            tempCchannel = tempCchannel->prev;
            [tempchannel copyToCStruct:tempCchannel];

            tempchannel = [self schannelForTag:tempchannel.prev];
        }

        tempCchannel->prev = NULL;
        temp_channellist = tempCchannel;

        gli_replace_schan_list(temp_channellist);
    }

    tempCwin = temp_windowlist;
    while (tempCwin) {
        tempwin = [self windowForTag:tempCwin->tag];
        [tempwin finalizeCStruct:tempCwin]; /* Now that we have created all objects, we can set pointers to them */
        tempCwin = tempCwin->next;
    }

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
        tempCstream = gli_stream_for_tag(str.tag);
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

    gli_repopulate_sound_channels_array();
    for (TempSChannel *chan in _schannels)
        [chan restartInternal];

    gli_sanity_check_windows();
    gli_sanity_check_streams();
    gli_sanity_check_filerefs();
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

/* Display a warning. Really this should be a fatal error. Eventually it will be visible on the screen somehow, but at the moment it's just a console log message.
 */
- (void) strictWarning:(NSString *)msg {
	NSLog(@"STRICT WARNING: %@", msg);
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
