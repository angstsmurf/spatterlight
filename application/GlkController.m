#import "main.h"
#import "Compatibility.h"
#import "InfoController.h"
#import "NSString+Signature.h"

#include <sys/time.h>

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif

#define MINTIMER 5 /* Transparent wants this */

//static const char *msgnames[] =
//{
//    "NOREPLY",
//    "OKAY", "ERROR", "HELLO", "PROMPTOPEN", "PROMPTSAVE",
//    "NEWWIN", "DELWIN", "SIZWIN", "CLRWIN",
//    "MOVETO", "PRINT",
//    "MAKETRANSPARENT", "STYLEHINT", "CLEARHINT", "STYLEMEASURE", "SETBGND", "SETTITLE", "AUTOSAVE",
//    "TIMER", "INITCHAR", "CANCELCHAR", "INITLINE", "CANCELLINE", "SETECHO", "TERMINATORS", "INITMOUSE", "CANCELMOUSE",
//    "FILLRECT", "FINDIMAGE", "LOADIMAGE", "SIZEIMAGE",
//    "DRAWIMAGE", "FLOWBREAK", "NEWCHAN", "DELCHAN",
//    "FINDSOUND", "LOADSOUND", "SETVOLUME", "PLAYSOUND", "STOPSOUND",
//    "SETLINK", "INITLINK", "CANCELLINK", "EVTHYPER",
//    "NEXTEVENT", "EVTARRANGE", "EVTLINE", "EVTKEY",
//    "EVTMOUSE", "EVTTIMER", "EVTSOUND", "EVTVOLUME", "EVTPREFS"
//};

static const char *wintypenames[] =
{
    "wintype_AllTypes", "wintype_Pair", "wintype_Blank", "wintype_TextBuffer","wintype_TextGrid", "wintype_Graphics"
};

//static const char *stylenames[] =
//{
//    "style_Normal", "style_Emphasized", "style_Preformatted", "style_Header", "style_Subheader", "style_Alert", "style_Note", "style_BlockQuote", "style_Input", "style_User1", "style_User2", "style_NUMSTYLES"
//};
//
//static const char *stylehintnames[] =
//{
//    "stylehint_Indentation", "stylehint_ParaIndentation", "stylehint_Justification", "stylehint_Size", "stylehint_Weight","stylehint_Oblique", "stylehint_Proportional", "stylehint_TextColor", "stylehint_BackColor", "stylehint_ReverseColor", "stylehint_NUMHINTS"
//};

@implementation GlkHelperView

- (BOOL) isFlipped
{
    return YES;
}

- (BOOL) isOpaque
{
    return YES;
}

//- (void) drawRect: (NSRect)rect
//{
//    //    [[NSColor whiteColor] set];
//    //    NSRectFill(rect);
//}

- (void) setFrame: (NSRect)frame
{
//    NSLog (@"GlkHelperView (_contentView) setFrame: %@", NSStringFromRect(frame));
    super.frame = frame;
    if (!self.inLiveResize)
        [delegate contentDidResize: frame];
}

- (void) viewDidEndLiveResize
{
//    NSLog (@"GlkHelperView (_contentView) viewDidEndLiveResize self.frame: %@", NSStringFromRect(self.frame));
    // We use a custom fullscreen width, so don't resize to full screen width when viewDidEndLiveResize
    // is called because we just entered fullscreen
    if ((delegate.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask)
        [delegate contentDidResize: self.frame];
}

@end

@implementation GlkController

/*
 * This is the real initializer.
 */

#pragma mark Initialization

- (void) runTerp: (NSString*)terpname
    withGameFile: (NSString*)gamefile_
            IFID: (NSString*)gameifid_
            info: (NSDictionary*)gameinfo_
{
    NSLog(@"glkctl: runterp %@ %@", terpname, gamefile_);

    _supportsAutorestore = [self.window isRestorable];

    NSSize defsize = [Preferences defaultWindowSize];

    _hasAutorestored = NO;
    turns = 0;

    gamefile = gamefile_;
    gameifid = gameifid_;
    gameinfo = gameinfo_;

    [self appSupportDir];
    [self autosaveFile];

    /* Setup our own stuff */

    restoredController = nil;

    _queue = [[NSMutableArray alloc] init];
    _gwindows = [[NSMutableDictionary alloc] init];

    waitforevent = NO;
    waitforfilename = NO;
    dead = NO;
    crashed = NO;

    windowdirty = NO;

    lastimageresno = -1;
    lastsoundresno = -1;
    lastimage = nil;
    //lastsound = nil;


    /* Setup Cocoa stuff */

    self.window.representedFilename = gamefile;
    self.window.title = [gameinfo objectForKey:@"title"];

    [self.window setContentSize: defsize];

    NSInteger border = [Preferences border];
    _borderView.frame = NSMakeRect(0, 0, defsize.width, defsize.height);
    [self setBorderColor:[Preferences bufferBackground]];
    _contentView.frame = NSMakeRect(border, border, defsize.width - (border * 2), defsize.height - (border * 2));

    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [self.window setValue:@2 forKey:@"tabbingMode"];
    }

    [self restoreAutosave];

    // Clamp to max screen size
    //defsize.height = self.window.frame.size.height;
    //defsize.width = self.window.frame.size.width;
    //[self.window setContentSize: defsize];

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(notePreferencesChanged:)
     name: @"PreferencesChanged"
     object: nil];

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteDefaultSizeChanged:)
     name: @"DefaultSizeChanged"
     object: nil];

    /* Fork the interpreter process */

    NSString *terppath;
    NSPipe *readpipe;
    NSPipe *sendpipe;

    terppath = [[NSBundle mainBundle] pathForAuxiliaryExecutable: terpname];
    readpipe = [NSPipe pipe];
    sendpipe = [NSPipe pipe];
    readfh = readpipe.fileHandleForReading;
    sendfh = sendpipe.fileHandleForWriting;

    task = [[NSTask alloc] init];
    task.currentDirectoryPath = NSHomeDirectory();
    task.standardOutput = readpipe;
    //        [[task.standardOutput fileHandleForReading] setReadabilityHandler:^(NSFileHandle *file) {
    //            NSData *data = [file availableData]; // this will read to EOF, so call only once
    //            NSLog(@"Task output! %@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    //
    //            // if you're collecting the whole output of a task, you may store it on a property
    //            [self.taskOutput appendData:data];
    //        }];


    task.standardInput = sendpipe;

#ifdef TEE_TERP_OUTPUT
    [task setLaunchPath: @"/bin/bash"];

    NSString *cmdline = @" "; //@"\"";
    cmdline = [cmdline stringByAppendingString:terppath];
    cmdline = [cmdline stringByAppendingString:@" \""];
    cmdline = [cmdline stringByAppendingString:gamefile];

    cmdline = [cmdline stringByAppendingString:@"\" | tee -a ~/Desktop/Spatterlight\\ "];

    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd HH.mm"];
    NSString *stringFromDate = [formatter stringFromDate:[NSDate date]];

    stringFromDate = [stringFromDate stringByReplacingOccurrencesOfString:@" " withString:@"\\ "];
    cmdline = [cmdline stringByAppendingString:stringFromDate];
    cmdline = [cmdline stringByAppendingString:@".txt"];

    [task setArguments: @[ @"-c", cmdline ]];
#else

    task.launchPath = terppath;
    task.arguments = @[gamefile];

#endif //TEE_TERP_OUTPUT

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteTaskDidTerminate:)
     name: NSTaskDidTerminateNotification
     object: task];

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteDataAvailable:)
     name: NSFileHandleDataAvailableNotification
     object: readfh];

    if (_hasAutorestored)
    {
        [readfh waitForDataInBackgroundAndNotify];
    }

    [task launch];

    [readfh waitForDataInBackgroundAndNotify];

    /* Send a prefs and an arrange event first thing */
    GlkEvent *gevent;


    gevent = [[GlkEvent alloc] initPrefsEvent];
    [self queueEvent: gevent];

    if (_hasAutorestored) {
        gevent = [[GlkEvent alloc] initArrangeWidth: _contentView.frame.size.width height: _contentView.frame.size.height];
    } else gevent = [[GlkEvent alloc] initArrangeWidth: defsize.width - Preferences.border * 2 height: defsize.height  - Preferences.border * 2];
    [self queueEvent: gevent];

    //}
    soundNotificationsTimer = [NSTimer scheduledTimerWithTimeInterval: 2.0 target: self selector: @selector(keepAlive:) userInfo: nil repeats: YES];
}

- (void) restoreAutosave {
    
    // If there already is an interpreter-create autosave file, we autorestore the UI here.
    // Otherwise this might be the first turn, so we look for an UI autosave file later,
    // in handleRequest at the first NEXTEVENT.
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:[_appSupportDir stringByAppendingPathComponent:@"autosave.plist"]]) {
        restoredController = [NSKeyedUnarchiver unarchiveObjectWithFile:_autosaveFile];
        if (restoredController) {
            [self.window setFrame:restoredController.storedWindowFrame display:NO];
            _hasAutorestored = YES;
        }
    }
}

- (void) restoreUI: (GlkController *) controller {

    GlkWindow *win;
    if (!controller) {
        controller = [NSKeyedUnarchiver unarchiveObjectWithFile:_autosaveFile];
    }
    if (controller) {
        NSLog(@"Restoring UI");
        _hasAutorestored = YES;
        _firstResponderView = controller.firstResponderView;
        _timerInterval = controller.timerInterval;
        _timerLeft = controller.timerLeft;
        _queue = controller.queue;

        if (_timerLeft) {
            NSLog(@"timerLeft:%f timerInterval:%f", _timerLeft, _timerInterval);
            if (timer) {
                [timer invalidate];
                timer = nil;
            }
            timer = [NSTimer scheduledTimerWithTimeInterval: _timerLeft
                                                     target: self
                                                   selector: @selector(restartTimer:)
                                                   userInfo: 0
                                                    repeats: NO];
            NSLog(@"timerLeft was %f, so started a timer.", _timerInterval);

        } else if (_timerInterval) {
            [self handleSetTimer:_timerInterval * 1000];
            NSLog(@"_timerInterval was %f, so started a timer.", _timerLeft);
        }

        for (id key in controller.gwindows) {
            win = [_gwindows objectForKey:key];
            if (win)
                [win removeFromSuperview];
            win = [controller.gwindows objectForKey:key];
            [_gwindows setObject:win forKey:@(win.name)];
            [win removeFromSuperview];
            [_contentView addSubview:win];
            NSLog (@"Added GlkWindow %@, name %ld to contentView", win, win.name);
            NSLog (@"Frame: %@ Bounds: %@", NSStringFromRect(win.frame), NSStringFromRect(win.bounds));

            win.glkctl=self;
//            [win restoreSelection];
//            if (win.name == _firstResponderView)
//                [win grabFocus];
        }
    }



    //GlkEvent *gevent = [[GlkEvent alloc] initArrangeWidth: defsize.width - Preferences.border * 2 height: defsize.height  - Preferences.border * 2];
    //[self queueEvent: gevent];
    restoredController = nil;
}

- (NSString *) appSupportDir {
    if (!_appSupportDir) {
        NSDictionary *gFolderMap = @{@"adrift": @"SCARE",
                                     @"advsys": @"AdvSys",
                                     @"agt": @"AGiliTy",
                                     @"glulx": @"Glulxe",
                                     @"hugo": @"Hugo",
                                     @"level9": @"Level 9",
                                     @"magscrolls": @"Magnetic",
                                     @"quill": @"UnQuill",
                                     @"tads2": @"TADS",
                                     @"tads3": @"TADS",
                                     //@"zcode": @"Frotz",
                                     @"zcode": @"Fizmo"};

        NSError *error;
        NSURL *appSupportURL = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@", error);

        NSString *terpFolder = [[gFolderMap objectForKey:[gameinfo objectForKey:@"format"]] stringByAppendingString:@" Files"];

        NSString *dirstr = [@"Spatterlight" stringByAppendingPathComponent:terpFolder];
        dirstr = [dirstr stringByAppendingPathComponent:@"Autosaves"];
        dirstr = [dirstr stringByAppendingPathComponent:[gamefile signatureFromFile]];
        dirstr = [dirstr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

        appSupportURL = [NSURL URLWithString: dirstr relativeToURL:appSupportURL];

        [[NSFileManager defaultManager] createDirectoryAtURL:appSupportURL withIntermediateDirectories:YES attributes:nil error:NULL];

        NSString *dummyfilename = [[gameinfo objectForKey:@"title"] stringByAppendingPathExtension:@"txt"];

        NSString *dummytext = [NSString stringWithFormat:@"This file, %@, was placed here in order to make it easier for humans to guess what game these autosave files belong to. Any files in this folder are for the game %@, or possibly a game with another name but identical contents.", dummyfilename, [gameinfo objectForKey:@"title"]];

        NSString *dummyfilepath = [appSupportURL.path stringByAppendingPathComponent:dummyfilename];

        BOOL succeed = [dummytext writeToURL: [NSURL fileURLWithPath:dummyfilepath] atomically:YES encoding:NSUTF8StringEncoding error:&error];
        if (!succeed){
            NSLog(@"Failed to write dummy file to autosave directory. Error:%@", error);
        }

        _appSupportDir = appSupportURL.path;
    }
    return _appSupportDir;
}

- (NSString *) autosaveFile {
    if (!_autosaveFile)
        _autosaveFile = [_appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"];
    return _autosaveFile;
}

- (instancetype) initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];
    if (self)
    {
        /* for talking to the interpreter */
//        task =;
//        readfh =;
//        sendfh =;
//        queue =;

        /* current state of the protocol */

        waitforevent = NO;
        waitforfilename = NO;
        turns = [decoder decodeIntegerForKey:@"turns"];

        /* the glk objects */

        _gwindows = [decoder decodeObjectForKey:@"gwindows"];
        NSLog(@"Decoded %ld gwindows",(unsigned long)_gwindows.count);

        contentFullScreenFrame = [decoder decodeRectForKey:@"contentFullScreenFrame"];
        windowPreFullscreenFrame = [decoder decodeRectForKey:@"windowPreFullscreenFrame"];
        fontSizePreFullscreen = [decoder decodeFloatForKey:@"fontSizePreFullscreen"];
        _storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        NSLog(@"decoded main window frame as %@", NSStringFromRect(_storedWindowFrame));
        _queue = [decoder decodeObjectForKey:@"queue"];
        _timerLeft = [decoder decodeDoubleForKey:@"timerLeft"];
        _timerInterval = [decoder decodeDoubleForKey:@"timerInterval"];
        _firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        //NSLog (@"Decoded window %ld as first responder.", _firstResponderView);
        restoredController = nil;
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder
{
    [super encodeWithCoder:encoder];

    [encoder encodeRect:self.window.frame forKey:@"windowFrame"];
    NSLog(@"encoded main window frame as %@", NSStringFromRect(self.window.frame));
    [encoder encodeObject:_gwindows forKey:@"gwindows"];
    NSLog(@"Encoded %ld gwindows",(unsigned long)_gwindows.count);
    [encoder encodeRect:contentFullScreenFrame forKey:@"contentFullScreenFrame"];
    [encoder encodeRect:windowPreFullscreenFrame forKey:@"windowPreFullscreenFrame"];
    [encoder encodeFloat:fontSizePreFullscreen forKey:@"fontSizePreFullscreen"];
    [encoder encodeObject:_queue forKey:@"queue"];
    _timerLeft = 0;
    _timerInterval = 0;
    if (timer && timer.isValid)
    {
        _timerLeft = [[timer fireDate] timeIntervalSinceDate: [[NSDate alloc] init]];
        _timerInterval = [timer timeInterval];
    }
    _firstResponderView = -1;
    if ([self.window.firstResponder isKindOfClass:[GlkWindow class]]) {
        _firstResponderView = ((GlkWindow *)self.window.firstResponder).name;
    } else if ([self.window.firstResponder isKindOfClass:[NSTextView class]] && [((NSTextView *)self.window.firstResponder).delegate isKindOfClass:[GlkWindow class]]) {
        _firstResponderView = ((GlkWindow *)((NSTextView *)self.window.firstResponder).delegate).name;
    }
    //NSLog (@"Encoded %ld as first responder.", _firstResponderView);
    [encoder encodeInteger:_firstResponderView forKey:@"firstResponder"];
    [encoder encodeDouble:_timerLeft forKey:@"timerLeft"];
    [encoder encodeDouble:_timerInterval forKey:@"timerInterval"];
    [encoder encodeInteger:turns forKey:@"turns"];
}

- (void) keepAlive: (NSTimer *)timer
{
    [readfh waitForDataInBackgroundAndNotify];
}

- (void) windowWillClose: (id)sender
{
    //NSLog(@"glkctl: windowWillClose");

    if (_supportsAutorestore && !crashed)
        [self autoSaveOnExit];

    [self.window setDelegate: nil];

    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (timer)
    {
        NSLog(@"glkctl: force stop the timer");
        [timer invalidate];
        timer = nil;
    }

    if (soundNotificationsTimer)
    {
        NSLog(@"glkctl: force stop the sound notifications timer");
        [soundNotificationsTimer invalidate];
        soundNotificationsTimer = nil;
    }

    if (task)
    {
        NSLog(@"glkctl: force stop the interpreter");
        [task terminate];
        task = nil;
    }

    [((AppDelegate*)[NSApplication sharedApplication].delegate).libctl.gameSessions removeObjectForKey:gameifid];
}

- (void) autoSaveOnExit {
    NSInteger res = [NSKeyedArchiver archiveRootObject:self toFile:_autosaveFile];
    if (!res) {
        NSLog(@"UI serialize on exit failed!");
        return;
    }

}

/*
 *
 */

#pragma mark Cocoa glue


- (IBAction) showGameInfo: (id)sender
{
    [((AppDelegate*)[NSApplication sharedApplication].delegate).libctl showInfo:gameinfo forFile: gamefile];
}

- (IBAction) revealGameInFinder: (id)sender
{
    [[NSWorkspace sharedWorkspace] selectFile: gamefile inFileViewerRootedAtPath: @""];
}

- (BOOL) isAlive
{
    return !dead;
}

- (NSRect) windowWillUseStandardFrame: (NSWindow*)window defaultFrame:(NSRect)defaultFrame
{
    //NSLog(@"glkctl: windowWillUseStandardFrame");

    NSRect screenframe = self.window.screen.frame;

    NSSize windowSize = [Preferences defaultWindowSize];
    NSRect frame = NSMakeRect((NSWidth(screenframe) - windowSize.width) / 2, 0, windowSize.width, NSHeight(screenframe));
    return frame;
}

- (void) contentDidResize: (NSRect)frame
{
    //NSLog(@"glkctl: contentDidResize: %@", NSStringFromRect(frame));

    NSInteger borders = Preferences.border * 2;
    if ((NSWidth(frame) != NSWidth(_borderView.frame) - borders) && ((self.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask))
        frame = NSMakeRect(Preferences.border, Preferences.border, NSWidth(_borderView.frame) - borders, NSHeight(_borderView.frame) - borders);
//    if (dead)
//        for (NSInteger i = 0; i < MAXWIN; i++)
//            if (gwindows[i] && [gwindows[i] isKindOfClass:[GlkTextBufferWindow class]])
//                [gwindows[i] setFrame:frame];

    GlkEvent *gevent;
    gevent = [[GlkEvent alloc] initArrangeWidth: frame.size.width height: frame.size.height];
    [self queueEvent: gevent];
}

- (void) closeAlertDidFinish: (id)alert rc: (int)rc ctx: (void*)ctx
{
    if (rc == NSAlertFirstButtonReturn)
    {
        [self close];
    }
}

- (BOOL) windowShouldClose: (id)sender
{
    NSLog(@"glkctl: windowShouldClose");

    NSAlert *alert;

    if (dead || _supportsAutorestore)
        return YES;

    alert = [[NSAlert alloc] init];
    alert.messageText = @"Do you want to abandon the game?";
    alert.informativeText = @"Any unsaved progress will be lost.";
    [alert addButtonWithTitle: @"Close"];
    [alert addButtonWithTitle: @"Cancel"];

    [alert beginSheetModalForWindow: self.window
                      modalDelegate: self
                     didEndSelector: @selector(closeAlertDidFinish:rc:ctx:)
                        contextInfo: NULL];

    return NO;
}

- (void) flushDisplay
{
    for (GlkWindow *win in [_gwindows allValues])
        [win flushDisplay];

    if (windowdirty)
    {
        [_contentView setNeedsDisplay: YES];
        windowdirty = NO;
    }
}

- (void) guessFocus
{
    id focuswin;

//    NSLog(@"glkctl guessFocus");

    focuswin = self.window.firstResponder;
    while (focuswin)
    {
        if ([focuswin isKindOfClass: [NSView class]])
        {
            if ([focuswin isKindOfClass: [GlkWindow class]])
                break;
            else
                focuswin = [focuswin superview];
        }
        else
            focuswin = nil;
    }

    //if (focuswin)
      //  NSLog(@"window %ld has focus", (long)[(GlkWindow*)focuswin name]);

    if (focuswin && [focuswin wantsFocus])
        return;

    //NSLog(@"glkctl guessing new window to focus on");

    for (GlkWindow *win in [_gwindows allValues])
    {
        if (win.wantsFocus)
        {
            [win grabFocus];
            return;
        }
    }
}

- (void) restoreFocus: (id)sender {
    for (GlkWindow *win in [_gwindows allValues]) {
        //nt[win restoreSelection];
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkTextBufferWindow *textbuf = (GlkTextBufferWindow *)win;
            [textbuf restoreScroll];
            //[textbuf restoreTextFinder];
        }
        if (win.name == _firstResponderView) {
            [win grabFocus];
        }
    }
}

- (void) markLastSeen
{
    for (GlkWindow *win in [_gwindows allValues])
        [win markLastSeen];
}

- (void) performScroll
{
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [win performScroll];
}


- (void) handleAutosave:(int)hash
{
    NSInteger res = [NSKeyedArchiver archiveRootObject:self toFile:_autosaveFile];

    if (!res) {
        NSLog(@"Window serialize failed!");
        return;
    }
    //NSLog(@"Autosave request: %d", hash);
}

/*
 *
 */

#pragma mark Preference and style hint glue

- (void) notePreferencesChanged: (id)sender
{
    //NSLog(@"glkctl: notePreferencesChanged");

    GlkEvent *gevent;

    NSRect frame = _contentView.frame;
    NSInteger border = Preferences.border;

    if ((self.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask)
    {
        frame.origin.x = frame.origin.y = border;

        frame.size.width = _borderView.frame.size.width - (border * 2);
        frame.size.height = _borderView.frame.size.height - (border * 2);
    }
    else // We are in fullscreen
    {
        frame.origin.y = border;
        frame.size.height = _borderView.frame.size.height - (border * 2);
    }

    if (!NSEqualRects(frame, _contentView.frame))
    {
        //NSLog(@"glkctl: notePreferencesChanged: _contentView frame changed from %@ to %@", NSStringFromRect(_contentView.frame), NSStringFromRect(frame));
        _contentView.frame = frame;
        [self contentDidResize:frame];
    }

    gevent = [[GlkEvent alloc] initArrangeWidth: frame.size.width height: frame.size.height];
    [self queueEvent: gevent];

    gevent = [[GlkEvent alloc] initPrefsEvent];
    [self queueEvent: gevent];

    for (GlkWindow *win in [_gwindows allValues])
        [win prefsDidChange];
}

- (void) noteDefaultSizeChanged: (id)sender
{
    NSSize defaultWindowSize = Preferences.defaultWindowSize;

    if ((self.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask)
    {
        NSRect screenframe = [NSScreen mainScreen].visibleFrame;

        NSRect contentRect = NSMakeRect(0, 0, defaultWindowSize.width, defaultWindowSize.height);

        NSRect winrect = [self.window frameRectForContentRect:contentRect];
        winrect.origin = self.window.frame.origin;

        //If the new size is too big to fit on screen, clip at screen size
        if (winrect.size.height > screenframe.size.height - 1)
            winrect.size.height = screenframe.size.height - 1;
        if (winrect.size.width > screenframe.size.width)
            winrect.size.width = screenframe.size.width;

        CGFloat offset = winrect.size.height - self.window.frame.size.height;

        winrect.origin.y -= offset;

        //If window is partly off the screen, move it (just) inside
        if (NSMaxX(winrect) > NSMaxX(screenframe))
            winrect.origin.x = NSMaxX(screenframe) - winrect.size.width;

        if (NSMinY(winrect) < 0)
            winrect.origin.y = NSMinY(screenframe);

        //[self.window setFrame:winrect display:YES animate:YES];
        [self.window setFrame:winrect display:NO animate:NO];
    }
    else
    {
        NSRect oldframe = _contentView.frame;
        NSInteger borders = Preferences.border * 2;
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y, defaultWindowSize.width - borders, defaultWindowSize.height - borders);

        if (newframe.size.width > _borderView.frame.size.width - borders)
            newframe.size.width = _borderView.frame.size.width - borders;
        if (newframe.size.height > _borderView.frame.size.height - borders)
            newframe.size.height = _borderView.frame.size.height - borders;

        newframe.origin.x += (oldframe.size.width - newframe.size.width) / 2;

        CGFloat offset = newframe.size.height - oldframe.size.height;
        newframe.origin.y -= offset;

        //[[_contentView animator] setFrame:newframe];
        _contentView.frame = newframe;
        [self contentDidResize:newframe];
    }
}

- (void) handleChangeTitle:(char*)buf length: (int)len
{
    buf[len]='\0';
    NSString *str = @(buf);
    //[@(buf) substringToIndex: len];
    //self.window.title = str;
    NSLog(@"Change title request: %@", str);
}


/*
 *
 */

#pragma mark Glk requests

- (void) handleOpenPrompt: (int)fileusage
{
    NSURL *directory = [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] objectForKey: @"SaveDirectory"] isDirectory:YES];

    NSInteger sendfd = sendfh.fileDescriptor;

    // Create and configure the panel.
    NSOpenPanel* panel = [NSOpenPanel openPanel];

    waitforfilename = YES; /* don't interrupt */

    if (fileusage == fileusage_SavedGame)
        panel.prompt = @"Restore";
    panel.directoryURL = directory;

    // Display the panel attached to the document's window.
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){

        const char *s;
        struct message reply;

        if (result == NSFileHandlingPanelOKButton)
        {
            NSURL*  theDoc = [panel.URLs objectAtIndex:0];

            [[NSUserDefaults standardUserDefaults] setObject: theDoc.path.stringByDeletingLastPathComponent forKey: @"SaveDirectory"];
            s = (theDoc.path).UTF8String;
        }
        else
            s = "";

        reply.cmd = OKAY;
        reply.len = (int)strlen(s);

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];

}

- (void) handleSavePrompt: (int)fileusage
{
    NSURL *directory = [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] objectForKey: @"SaveDirectory"] isDirectory:YES];
    NSSavePanel *panel = [NSSavePanel savePanel];
    NSString *prompt;
    NSString *ext;
    NSString *filename;
    NSString *date;

    waitforfilename = YES; /* don't interrupt */

    switch (fileusage)
    {
        case fileusage_Data: prompt = @"Save data file: "; ext = @"glkdata"; filename = @"Data"; break;
        case fileusage_SavedGame: prompt = @"Save game: "; ext = @"glksave"; break;
        case fileusage_Transcript: prompt = @"Save transcript: "; ext = @"txt"; filename = @"Transcript of "; break;
        case fileusage_InputRecord: prompt = @"Save recording: "; ext = @"rec"; filename = @"Recordning of "; break;
        default: prompt = @"Save: "; ext = nil; break;
    }

    //[panel setNameFieldLabel: prompt];
    if (ext)
        panel.allowedFileTypes=@[ext];
    panel.directoryURL = directory;

    panel.extensionHidden=NO;
    [panel setCanCreateDirectories:YES];

    if (fileusage == fileusage_Transcript || fileusage == fileusage_InputRecord)
        filename = [filename stringByAppendingString:[gameinfo objectForKey:@"title"]];
    
    if (fileusage == fileusage_SavedGame)
    {
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        formatter.dateFormat = @" yyyy-MM-dd HH.mm";
        date = [formatter stringFromDate:[NSDate date]];


        filename = [[gameinfo objectForKey:@"title"] stringByAppendingString:date];
    }

    if (ext)
        filename = [filename stringByAppendingPathExtension: ext];

    if (filename)
        panel.nameFieldStringValue = filename;

	NSInteger sendfd = sendfh.fileDescriptor;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        struct message reply;
        const char *s;

        if (result == NSFileHandlingPanelOKButton)
        {
            NSURL*  theFile = panel.URL;
            [[NSUserDefaults standardUserDefaults] setObject: theFile.path.stringByDeletingLastPathComponent forKey: @"SaveDirectory"];
            s = (theFile.path).UTF8String;
        }
        else
            s = "";

        reply.cmd = OKAY;
        reply.len = (int)strlen(s);

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (NSInteger) handleNewWindowOfType: (NSInteger)wintype
{
    NSUInteger i, k;

    NSLog(@"handleNewWindowOfType: %s", wintypenames[wintype]);

    for (i = 0; i < MAXWIN; i++)
        if ([_gwindows objectForKey:@(i)]== nil)
            break;

    if (i == MAXWIN)
        return -1;

    NSLog(@"Adding new %s window with name: %ld", wintypenames[wintype], (long)i);

    switch (wintype)
    {
        case wintype_TextGrid:
            [_gwindows setObject:[[GlkTextGridWindow alloc] initWithGlkController: self name: i] forKey:@(i)];
            [_contentView addSubview:[_gwindows objectForKey:@(i)]];

            for (k = 0; k < style_NUMSTYLES; k++)
            {
                [[_gwindows objectForKey:@(i)] setStyle: k
                           windowType: wintype_TextGrid
                               enable: styleuse[0][k]
                                value: styleval[0][k]];

            }
            return i;

        case wintype_TextBuffer:
            [_gwindows setObject:[[GlkTextBufferWindow alloc] initWithGlkController: self name: i] forKey:@(i)];
            [_contentView addSubview:[_gwindows objectForKey:@(i)]];


            for (k = 0; k < style_NUMSTYLES; k++)
            {
                [[_gwindows objectForKey:@(i)] setStyle: k
                           windowType: wintype_TextBuffer
                               enable: styleuse[1][k]
                                value: styleval[1][k]];
            }
            return i;

        case wintype_Graphics:
            [_gwindows setObject:[[GlkGraphicsWindow alloc] initWithGlkController: self name: i] forKey:@(i)];
            [_contentView addSubview: [_gwindows objectForKey:@(i)]];
            return i;
    }

    return -1;
}

- (int) handleNewSoundChannel
{
//    int i;
//
//    for (i = 0; i < MAXSND; i++)
//        if (gchannels[i] == nil)
//            break;
//
//    if (i == MAXSND)
//        return -1;
//
//    gchannels[i] = [[GlkSoundChannel alloc] initWithGlkController: self name: i];
//
    return MAXSND;
}

- (void) handleSetTimer: (int)millisecs
{
    if (timer)
    {
        [timer invalidate];
        timer = nil;
    }

    if (millisecs > 0)
    {
        if (millisecs < MINTIMER)
        {
            NSLog(@"glkctl: too small timer interval (%d); increasing to %d", millisecs, MINTIMER);
            millisecs = MINTIMER;
        }

        timer = [NSTimer scheduledTimerWithTimeInterval: millisecs/1000.0
                                                 target: self
                                               selector: @selector(noteTimerTick:)
                                               userInfo: 0
                                                repeats: YES];
    }
}

- (void) noteTimerTick: (id)sender
{
    if (waitforevent)
    {
        GlkEvent *gevent = [[GlkEvent alloc] initTimerEvent];
        [self queueEvent: gevent];
    }
}

- (void) restartTimer: (id)sender
{
    [self handleSetTimer:(int)(_timerInterval * 1000)];
}

- (void) handleLoadSoundNumber: (int)resno from: (char*)buffer length: (int)length
{
    lastsoundresno = -1;

    if (lastsound)
    {
        lastsound = nil;
    }

    lastsound = [[NSData alloc] initWithBytes: buffer length: length];
    if (lastsound)
        lastsoundresno = resno;
}

- (void) handleLoadImageNumber: (int)resno from: (char*)buffer length: (int)length
{
    lastimageresno = -1;

    if (lastimage)
    {
        lastimage = nil;
    }

    NSData *data = [[NSData alloc] initWithBytesNoCopy: buffer length: length freeWhenDone: NO];
    if (!data)
        return;

    NSArray * reps = [NSBitmapImageRep imageRepsWithData:data];

    NSSize size = NSZeroSize;

    for (NSImageRep * imageRep in reps) {
        if (imageRep.pixelsWide > size.width) size.width = imageRep.pixelsWide;
        if (imageRep.pixelsHigh > size.height) size.height = imageRep.pixelsHigh;
    }

    lastimage = [[NSImage alloc] initWithSize:size];

    if (!lastimage)
    {
        NSLog(@"glkctl: failed to decode image");
        return;
    }

    [lastimage addRepresentations:reps];

    NSData *tiffdata = lastimage.TIFFRepresentation;

    lastimage = [[NSImage alloc] initWithData:tiffdata];
    lastimage.size = size;

    lastimageresno = resno;
}


- (void) handleStyleHintOnWindowType: (int)wintype style: (int)style hint:(int)hint value:(int)value
{

    //NSLog(@"handleStyleHintOnWindowType: %s style: %s hint: %s value: %d", wintypenames[wintype], stylenames[style], stylehintnames[hint], value);

    if (style < 0 || style >= style_NUMSTYLES)
        return;

    if (wintype == wintype_AllTypes)
    {
        styleuse[0][style][hint] = YES;
        styleval[0][style][hint] = value;
        styleuse[1][style][hint] = YES;
        styleval[1][style][hint] = value;
    }
    else if (wintype == wintype_TextGrid)
    {
        styleuse[0][style][hint] = YES;
        styleval[0][style][hint] = value;
    }
    else if (wintype == wintype_TextBuffer)
    {
        styleuse[1][style][hint] = YES;
        styleval[1][style][hint] = value;
    }

}

NSInteger colorToInteger(NSColor *color)
{
    CGFloat r, g, b, a;
    uint32_t buf[3];
    NSInteger i;
    color = [color colorUsingColorSpaceName: NSCalibratedRGBColorSpace];

    [color getRed:&r green:&g blue:&b alpha:&a];

    buf[0] = (int)(r * 255);
    buf[1] = (int)(g * 255);
    buf[2] = (int)(b * 255);

    i = buf[2] + (buf[1] << 8) + (buf[0] << 16);
    return i;
}


- (BOOL) handleStyleMeasureOnWin: (GlkWindow*)gwindow style: (int)style hint:(int)hint result:(NSInteger *)result
{
    if (styleuse[1][style_Normal][stylehint_TextColor])
        NSLog(@"styleuse[1][style_Normal][stylehint_TextColor] is true. Value:%ld", (long)styleval[1][style_Normal][stylehint_TextColor]);

    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;
    else
    {
        if (hint == stylehint_TextColor)
        {
            if ([gwindow isKindOfClass: [GlkTextBufferWindow class]])
                *result = colorToInteger([Preferences bufferForeground]);
            else
                *result = colorToInteger([Preferences gridForeground]);

            return YES;
        }
        if (hint == stylehint_BackColor)
        {
            if ([gwindow isKindOfClass: [GlkTextBufferWindow class]])
                *result = colorToInteger([Preferences bufferBackground]);
            else
                *result = colorToInteger([Preferences gridBackground]);

            return YES;
        }

    }
    return NO;
}


- (void) handleClearHintOnWindowType: (int)wintype style: (int)style hint:(int)hint
{
    if (style < 0 || style >= style_NUMSTYLES)
        return;

    if (wintype == wintype_AllTypes)
    {
        styleuse[0][style][hint] = NO;
        styleuse[1][style][hint] = NO;
    }
    else if (wintype == wintype_TextGrid)
    {
        styleuse[0][style][hint] = NO;
    }
    else if (wintype == wintype_TextBuffer)
    {
        styleuse[1][style][hint] = NO;
    }
}

- (void) handlePrintOnWindow: (GlkWindow*)gwindow style: (int)style buffer: (unichar*)buf length: (int)len
{
    NSString *str;

    if ([gwindow isKindOfClass: [GlkTextBufferWindow class]] && (style & 0xff) != style_Preformatted)
    {
        GlkTextBufferWindow *textwin = (GlkTextBufferWindow*) gwindow;
        NSInteger smartquotes = [Preferences smartQuotes];
        NSInteger spaceformat = [Preferences spaceFormat];
        NSInteger lastchar = textwin.lastchar;
        NSInteger spaced = 0;
        NSInteger i;

        for (i = 0; i < len; i++)
        {
            /* turn (punct sp sp) into (punct sp) */
            if (spaceformat)
            {
                if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?')
                    spaced = 1;
                else if (buf[i] == ' ' && spaced == 1)
                    spaced = 2;
                else if (buf[i] == ' ' && spaced == 2)
                {
                    memmove(buf+i, buf+i+1, (len - (i + 1)) * sizeof(unichar));
                    len --;
                    i--;
                    spaced = 0;
                }
                else
                {
                    spaced = 0;
                }
            }

            if (smartquotes && buf[i] == '`')
                buf[i] = 0x2018;

            else if (smartquotes && buf[i] == '\'')
            {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x2018;
                else
                    buf[i] = 0x2019;
            }

            else if (smartquotes && buf[i] == '"')
            {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x201c;
                else
                    buf[i] = 0x201d;
            }

            else if (smartquotes && i > 1 && buf[i-1] == '-' && buf[i] == '-')
            {
                memmove(buf+i, buf+i+1, (len - (i + 1)) * sizeof(unichar));
                len --;
                i--;
                buf[i] = 0x2013;
            }

            else if (smartquotes && i > 1 && buf[i-1] == 0x2013 && buf[i] == '-')
            {
                memmove(buf+i, buf+i+1, (len - (i + 1)) * sizeof(unichar));
                len --;
                i--;
                buf[i] = 0x2014;
            }

            lastchar = buf[i];
        }

        len = (int)i;
    }

    str = [NSString stringWithCharacters: buf length: len];

    [gwindow putString: str style: style];
}

- (void) handleSoundNotification: (NSInteger)notify withSound:(NSInteger)sound
{
    GlkEvent *gev = [[GlkEvent alloc] initSoundNotify:notify withSound:sound];
    [self queueEvent:gev];
}

- (void) handleVolumeNotification: (NSInteger)notify
{
    GlkEvent *gev = [[GlkEvent alloc] initVolumeNotify:notify];
    [self queueEvent:gev];
}

- (void) handleSetTerminatorsOnWindow:(GlkWindow*)gwindow    buffer: (glui32 *)buf length: (glui32)len
{
    NSMutableDictionary *myDict = gwindow.pendingTerminators;
    NSNumber *key;
    NSArray *keys = myDict.allKeys;

    for (key in keys)    {
        [myDict setObject:@(NO) forKey:key];
    }

//    NSLog(@"handleSetTerminatorsOnWindow: %ld length: %u", (long)gwindow.name, len );

    for (NSInteger i = 0; i < len; i++)
    {
        key = @(buf[i]);
        id terminator_setting = [myDict objectForKey:key];
        if (terminator_setting)
        {
            [myDict setObject:@(YES) forKey:key];
        }
        else NSLog(@"Illegal line terminator request: %u", buf[i]);
    }
    gwindow.terminatorsPending = YES;
}

- (BOOL) handleRequest: (struct message *)req reply: (struct message *)ans buffer: (char *)buf
{
    //NSLog(@"glkctl: incoming request %s", msgnames[req->cmd]);

    NSInteger result;
    GlkWindow *reqWin = nil;

    if (req->a1 >= 0 && req->a1 < MAXWIN && [_gwindows objectForKey:@(req->a1)])
        reqWin = [_gwindows objectForKey:@(req->a1)];

    switch (req->cmd)
    {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = (int)[Preferences graphicsEnabled];
            ans->a2 = (int)[Preferences soundEnabled];
            break;

        case NEXTEVENT:
            [self flushDisplay];

            if (_queue.count)
            {
                GlkEvent *gevent;
                gevent = [_queue objectAtIndex:0];
                //NSLog(@"glkctl: writing queued event %s", msgnames[[gevent type]]);

                [gevent writeEvent: sendfh.fileDescriptor];
                [_queue removeObjectAtIndex: 0];
                return NO; /* keep reading ... we sent the reply */
            }
            else
            {
                //No queued events.

                if (!req->a1)
                {
                    //Argument 1 is FALSE. No waiting for more events. Send a dummy reply to hand over to the interpreter immediately.
                    ans->cmd = OKAY;
                    break;
                }
            }

            [self guessFocus];

            // If this is the first turn, we try to restore the UI from an autosave file,
            // in order to catch things like entered text and scrolling, that has changed the
            // UI but not sent any events to the interpreter.
            if (++turns == 1 && _supportsAutorestore)
            {
                [self restoreUI: restoredController];

                if (_hasAutorestored)
                {
                    [self.window makeKeyAndOrderFront:nil];
                    [self.window makeFirstResponder: self.window];
                    [self restoreFocus:nil];
                }
            }

            waitforevent = YES;
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTOPEN:
            [self handleOpenPrompt: req->a1];
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTSAVE:
            [self handleSavePrompt: req->a1];
            return YES; /* stop reading ... terp is waiting for reply */

        case STYLEHINT:
            [self handleStyleHintOnWindowType:req->a1 style:req->a2 hint:req->a3 value:req->a4];
            break;

        case STYLEMEASURE:
            result = 0;
            ans->cmd = OKAY;
            ans->a1 = [self handleStyleMeasureOnWin:reqWin style:req->a2 hint:req->a3 result:&result];
            ans->a2 = (int)result;
            break;

        case CLEARHINT:
            [self handleClearHintOnWindowType:req->a1 style:req->a2 hint:req->a3];
            break;

            /*
             * Create and destroy windows and channels
             */

#pragma mark Create and destroy windows and sound channels

        case NEWWIN:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleNewWindowOfType: req->a1];
//            NSLog(@"glkctl newwin %d (type %d)", ans->a1, req->a1);
            break;

        case NEWCHAN:
            ans->cmd = OKAY;
            ans->a1 = [self handleNewSoundChannel];
            break;

        case DELWIN:
            NSLog(@"glkctl delwin %d", req->a1);
            if (reqWin)
            {
                [reqWin removeFromSuperview];
                reqWin = nil;
                [_gwindows removeObjectForKey:@(req->a1)];
            }
            else
                NSLog(@"delwin: something went wrong.");

            break;

        case DELCHAN:
//            if (req->a1 >= 0 && req->a1 < MAXSND && gchannels[req->a1])
//            {
//                gchannels[req->a1] = nil;
//            }
            break;

            /*
             * Load images; load and play sounds
             */

#pragma mark Load images; load and play sounds

        case FINDIMAGE:
            ans->cmd = OKAY;
            ans->a1 = lastimageresno == req->a1;
            break;

        case FINDSOUND:
            ans->cmd = OKAY;
            ans->a1 = lastsoundresno == req->a1;
            break;

        case LOADIMAGE:
            buf[req->len] = 0;
            [self handleLoadImageNumber: req->a1 from: buf length: req->len];
            break;

        case SIZEIMAGE:
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            if (lastimage)
            {
                NSSize size;
                size = lastimage.size;
                ans->a1 = size.width;
                ans->a2 = size.height;
            }
            break;

        case LOADSOUND:
            buf[req->len] = 0;
            [self handleLoadSoundNumber: req->a1 from: buf length: req->len];
            break;

        case SETVOLUME:
//            if (req->a1 >= 0 && req->a1 < MAXSND && gchannels[req->a1])
//            {
//                [gchannels[req->a1] setVolume: req->a2];
//            }
            break;

        case PLAYSOUND:
//            if (req->a1 >= 0 && req->a1 < MAXSND && gchannels[req->a1])
//            {
//                if (lastsound)
//                    [gchannels[req->a1] play: lastsound repeats: req->a2 notify: req->a3];
//            }
            break;

        case STOPSOUND:
//            if (req->a1 >= 0 && req->a1 < MAXSND && gchannels[req->a1])
//            {
//                [gchannels[req->a1] stop];
//            }
            break;

            /*
             * Window sizing, printing, drawing, etc...
             */

#pragma mark Window sizing, printing, drawing …

        case SIZWIN:
            NSLog(@"glkctl sizwin %d: %d x %d", req->a1, req->a4-req->a2, req->a5-req->a3);
            if (reqWin)
            {
                int x0, y0, x1, y1;
                NSRect rect;
                x0 = req->a2;
                y0 = req->a3;
                x1 = req->a4;
                y1 = req->a5;
                rect = NSMakeRect(x0, y0, x1 - x0, y1 - y0);
                if (rect.size.width < 0)
                    rect.size.width = 0;
                if (rect.size.height < 0)
                    rect.size.height = 0;
                reqWin.frame = rect;
                windowdirty = YES;
            }
            else
                NSLog(@"sizwin: something went wrong.");

            break;

        case CLRWIN:
            if (reqWin)
            {
                //NSLog(@"glkctl: CLRWIN %d.", req->a1);
                [reqWin clear];
            }
            break;

        case SETBGND:
            if (reqWin)
            {
                if (![reqWin isKindOfClass:[GlkGraphicsWindow class]])
                {
                    NSLog(@"glkctl: SETBGND: ERROR win %d is not a graphics window.", req->a1);
                    break;
                }

                [reqWin setBgColor: req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin)
            {
                if (lastimage)
                {
                    [reqWin drawImage: lastimage
                                            val1: req->a2
                                            val2: req->a3
                                           width: req->a4
                                          height: req->a5];
                }
            }
            break;

        case FILLRECT:
            if (reqWin)
            {
                int realcount = req->len / sizeof (struct fillrect);
                if (realcount == req->a2)
                {
                    [reqWin fillRects: (struct fillrect *)buf count: req->a2];
                }
            }
            break;

        case PRINT:
            if (reqWin)
            {
                [self handlePrintOnWindow: reqWin
                                    style: req->a2
                                   buffer: (unichar*)buf
                                   length: req->len / sizeof(unichar)];
            }
            break;

        case MOVETO:
            if (reqWin)
            {
                int x = req->a2;
                int y = req->a3;
                if (x < 0) x = 10000;
                if (y < 0) y = 10000;
                [reqWin moveToColumn: x row: y];
            }
            break;

        case SETECHO:
            if (reqWin && [reqWin isKindOfClass: [GlkTextBufferWindow class]])
                [(GlkTextBufferWindow *)reqWin echo:(req->a2 != 0)];
            break;

            /*
             * Request and cancel events
             */

        case TERMINATORS:
            [self handleSetTerminatorsOnWindow: reqWin
                                        buffer: (glui32 *)buf
                                        length: req->a2];
            break;

        case FLOWBREAK:
            NSLog(@"glkctl: WEE! WE GOT A FLOWBREAK! ^^;");
            if (reqWin)
            {
                [reqWin flowBreak];
            }
            break;

#pragma mark Request and cancel events

        case INITLINE:
            //NSLog(@"glkctl INITLINE %d", req->a1);
            [self performScroll];
            if (reqWin)
            {
                [reqWin initLine: [[NSString alloc]
                                               initWithData: [NSData dataWithBytes: buf
                                                                            length: req->len]
                                               encoding: NSUTF8StringEncoding]];

            }
            break;

        case CANCELLINE:
            NSLog(@"glkctl CANCELLINE %d", req->a1);
            ans->cmd = OKAY;
            if (reqWin)
            {
                const char *str = [reqWin cancelLine].UTF8String;
                strlcpy(buf, str, GLKBUFSIZE);
                ans->len = (int)strlen(buf);
            }
            break;

        case INITCHAR:
            [self performScroll];
//            NSLog(@"glkctl initchar %d", req->a1);
            if (reqWin)
                [reqWin initChar];
            break;

        case CANCELCHAR:
//            NSLog(@"glkctl CANCELCHAR %d", req->a1);
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
//            NSLog(@"glkctl initmouse %d", req->a1);
            [self performScroll];
            if (reqWin)
                [reqWin initMouse];
            break;

        case CANCELMOUSE:
            if (reqWin)
                [reqWin cancelMouse];
            break;

        case SETLINK:
            //            NSLog(@"glkctl set hyperlink %d in window %d", req->a2, req->a1);
            if (reqWin)
            {
                [reqWin setHyperlink:req->a2];
            }
            break;

        case INITLINK:
            //            NSLog(@"glkctl request hyperlink event in window %d", req->a1);
            [self performScroll];
            if (reqWin)
            {
                [reqWin initHyperlink];
            }
            break;

        case CANCELLINK:
            //            NSLog(@"glkctl cancel hyperlink event in window %d", req->a1);
            if (reqWin)
            {
                [reqWin cancelHyperlink];
            }
            break;

        case TIMER:
            [self handleSetTimer: req->a1];
            break;

        case EVTSOUND:
            NSLog(@"glkctl EVTSOUND %d, %d. Send it back to where it came from.", req->a2, req->a3);
            [self handleSoundNotification: req->a3 withSound:req->a2];
            break;

        case EVTVOLUME:
            NSLog(@"glkctl EVTVOLUME %d. Send it back where it came.", req->a3);
            [self handleVolumeNotification: req->a3];
            break;


            /*
             * Hugo specifics (hugo doesn't use glk to arrange windows)
             */

#pragma mark Non-standard Glk extensions stuff

        case MAKETRANSPARENT:
            if (reqWin)
                [reqWin makeTransparent];
            break;

        case SETTITLE:
            [self handleChangeTitle: (char*)buf
                             length: req->len];
            break;


        case AUTOSAVE:
            [self handleAutosave: req->a2];
            break;
            /*
             * HTML-TADS specifics will go here.
             */

        default:
            NSLog(@"glkctl: unhandled request (%d)", req->cmd);
    }

    return NO; /* keep reading */
}

/*
 *
 */

#pragma mark Interpreter glue

static NSString *signalToName(NSTask *task)
{
    switch (task.terminationStatus)
    {
        case 1: return @"sighup";
        case 2: return @"sigint";
        case 3: return @"sigquit";
        case 4: return @"sigill";
        case 6: return @"sigabrt";
        case 8: return @"sigfpe";
        case 9: return @"sigkill";
        case 10: return @"sigbus";
        case 11: return @"sigsegv";
        case 13: return @"sigpipe";
        case 15: return @"sigterm";
        default:
            return [NSString stringWithFormat: @"%d", task.terminationStatus];
    }
}

static BOOL pollMoreData(int fd)
{
    struct timeval timeout;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(fd + 1, &set, NULL, NULL, &timeout) == 1;
}

- (void) noteTaskDidTerminate: (id)sender
{
    NSLog(@"glkctl: noteTaskDidTerminate");

    dead = YES;

    if (timer)
    {
        [timer invalidate];
        timer = nil;
    }

    if (task && task.terminationStatus != 0)
    {
        NSAlert *alert;

        alert = [NSAlert alertWithMessageText: @"The game has unexpectedly terminated."
                                defaultButton: @"Oops"
                              alternateButton: nil
                                  otherButton: nil
                    informativeTextWithFormat: @"Error code: %@.", signalToName(task)];

        [alert beginSheetModalForWindow: self.window
                          modalDelegate: nil
                         didEndSelector: nil
                            contextInfo: nil];
        crashed = YES;
    }

    [self performScroll];

    for (GlkWindow *win in [_gwindows allValues])
        [win terpDidStop];

//    for (i = 0; i < MAXSND; i++)
//        if (gchannels[i])
//            [gchannels[i] stop];

    self.window.title = [self.window.title stringByAppendingString: @" (finished)"];

    task = nil;

    // [self setDocumentEdited: NO];
}

- (void) queueEvent: (GlkEvent*)gevent
{
    if (waitforfilename)
    {
        [_queue addObject: gevent];
    }
    else if (waitforevent)
    {
        [gevent writeEvent: sendfh.fileDescriptor];
        waitforevent = NO;
        [readfh waitForDataInBackgroundAndNotify];
    }
    else
    {
        [_queue addObject: gevent];
    }
}

- (void) noteDataAvailable: (id)sender
{
    // NSLog(@"glkctl: noteDataAvailable");

    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf;
    char *buf;
    NSInteger n, t;
    BOOL stop;

    int readfd = readfh.fileDescriptor;
    int sendfd = sendfh.fileDescriptor;

again:

    buf = minibuf;
    maxibuf = NULL;

    n = read(readfd, &request, sizeof(struct message));
    if (n < (NSInteger)sizeof(struct message))
    {
        if (n < 0)
            NSLog(@"glkctl: could not read message header");
        else
            NSLog(@"glkctl: connection closed");
        return;
    }

    /* this should only happen when sending resources */
    if (request.len > GLKBUFSIZE)
    {
        maxibuf = malloc(request.len);
        if (!maxibuf)
        {
            NSLog(@"glkctl: out of memory for message (%d bytes)", request.len);
            return;
        }
        buf = maxibuf;
    }

    if (request.len)
    {
        n = 0;
        while (n < request.len)
        {
            t = read(readfd, buf + n, request.len - n);
            if (t <= 0)
            {
                NSLog(@"glkctl: could not read message body");
                if (maxibuf)
                    free(maxibuf);
                return;
            }
            n += t;
        }
    }

    memset(&reply, 0, sizeof reply);

    stop = [self handleRequest: &request reply: &reply buffer: buf];

    if (reply.cmd > NOREPLY)
    {
        write(sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write(sendfd, buf, reply.len);
    }

    if (maxibuf)
        free(maxibuf);

    /* if stop, don't read or wait for more data */
    if (stop)
        return;

    if (pollMoreData(readfd))
        goto again;
    else
        [readfh waitForDataInBackgroundAndNotify];
}

- (void) setBorderColor: (NSColor *)color;
{
    [_borderView setWantsLayer:YES];

    CGFloat components[[color numberOfComponents]];
    CGColorSpaceRef colorSpace = [[color colorSpace] CGColorSpace];
    [color getComponents:(CGFloat *)&components];
    CGColorRef cgcol = CGColorCreate(colorSpace, components);

    _borderView.layer.backgroundColor = cgcol;
    self.window.backgroundColor = color;
    CFRelease(cgcol);
}

#pragma mark Restore window




#pragma mark Zoom

- (IBAction) zoomIn:(id)sender
{
    [Preferences zoomIn];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

- (IBAction) zoomOut:(id)sender
{
    [Preferences zoomOut];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

- (IBAction) zoomToActualSize:(id)sender
{
    [Preferences zoomToActualSize];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

#pragma mark Full screen

- (NSSize)window:(NSWindow *)window
willUseFullScreenContentSize:(NSSize)proposedSize
{
    return proposedSize;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    return @[window];
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window
{
    return @[window];
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    // Save the window frame so that it can be restored later
    windowPreFullscreenFrame = self.window.frame;
    NSDictionary *dict = [Preferences attributesForGridStyle: style_Normal];
    NSFont *font = [dict objectForKey:NSFontAttributeName];
    fontSizePreFullscreen = font.pointSize;
}

- (void)window:(NSWindow *)window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration
{
    //NSLog(@"frame pre-fullscreen animation: %@", NSStringFromRect(self.window.frame));

    // Make sure the window style mask includes the
    // full screen bit
    window.styleMask = (window.styleMask | NSFullScreenWindowMask);
    NSScreen *screen = window.screen;

    // The final, full screen frame
    NSRect border_finalFrame = NSZeroRect;
    border_finalFrame.size = [self window:window
             willUseFullScreenContentSize:screen.frame.size];

    //NSLog(@"border_finalFrame: %@", NSStringFromRect(border_finalFrame));

    contentFullScreenFrame = _contentView.frame;

    // Calculate the origin as half the difference between
    // the window frame and the screen frame
    contentFullScreenFrame.origin.x +=
    ceil((NSWidth(screen.frame) -
           NSWidth(_contentView.frame)) / 2 - [Preferences border]);
    contentFullScreenFrame.origin.y = [Preferences border];
    contentFullScreenFrame.size.height = NSHeight(screen.frame) - [Preferences border] * 2;

    //NSLog(@"contentFullScreenFrame: %@", NSStringFromRect(contentFullScreenFrame));

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.
    NSRect centerWindowFrame = window.frame;
    centerWindowFrame.origin.x =
    (border_finalFrame.size.width - centerWindowFrame.size.width ) / 2 - [Preferences border];
    centerWindowFrame.origin.y =
    NSHeight(screen.frame) - NSHeight(centerWindowFrame);

    //NSLog(@"centerWindowFrame: %@", NSStringFromRect(centerWindowFrame));


    // Our animation must be slightly shorter than
    // the system animation to avoid a black flash
    // at the end of the animation -- seems like a bug
    //duration += DURATION_ADJUSTMENT;
    //duration += DURATION_ADJUSTMENT;

	_contentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;

    // Our animation will be broken into two steps.
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context)
     {
         // First, we move the window to the center
         // of the screen
         context.duration = duration / ANIMATION_STEPS;
         [[window animator] setFrame:centerWindowFrame display:YES];
     }
                        completionHandler:^
     {
         [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context)
          {
              // and then we enlarge it its full size.
              context.duration = duration/ ANIMATION_STEPS;
              [[window animator] setFrame:border_finalFrame display:YES];
              //[[_contentView animator] setFrame:contentFullScreenFrame];
          }
                             completionHandler:^
          {
          }
          ];
     }
     ];
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration
{
    // Make sure the window style mask does not
    // include full screen bit
    [window setStyleMask:([window styleMask] &
                          ~NSFullScreenWindowMask)];

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.
    NSRect centerWindowFrame = _contentView.frame;
    centerWindowFrame.origin.x -= [Preferences border];
    centerWindowFrame.origin.y -= [Preferences border];
    centerWindowFrame.size.width += [Preferences border] * 2;
    centerWindowFrame.size.height += [Preferences border] * 2;

	NSRect oldFrame = windowPreFullscreenFrame;

    // Our animation will be broken into two stages.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context)
     {

         // First, we'll restore the window to its original size
         // while centering it
         context.duration = duration / ANIMATION_STEPS;
         [[window animator]
          setFrame:centerWindowFrame display:YES];
     }
     completionHandler:^
     {
         [NSAnimationContext
          runAnimationGroup:^(NSAnimationContext *context)
          {

              // And then we'll move it back to its initial
              // position.
              context.duration = duration / ANIMATION_STEPS;
              [[window animator]
               setFrame:oldFrame display:YES];
          }
          completionHandler:^
          {
          }
          ];
     } ];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    //NSRect fullscreenContent = NSMakeRect((contentFullScreenFrame.size.width - preFullscreenFrame.size.width) / 2, [Preferences border], preFullscreenFrame.size.width, contentFullScreenFrame.size.height - [Preferences border] * 2);

    //NSLog(@"windowDidEnterFullScreen: contentview set to: %@", NSStringFromRect(contentFullScreenFrame));

    [[_contentView animator] setFrame:contentFullScreenFrame];
    [self contentDidResize: contentFullScreenFrame];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    //NSLog(@"windowDidExitFullScreen: contentview was: %@", NSStringFromRect(_contentView.frame));
    //NSLog(@"windowDidExitFullScreen: borderview is: %@", NSStringFromRect(_borderView.frame));

    NSRect frame;

    NSDictionary *dict = [Preferences attributesForGridStyle: style_Normal];
    NSFont *font = [dict objectForKey:NSFontAttributeName];
    [Preferences scale:fontSizePreFullscreen / font.pointSize];

    NSInteger border = Preferences.border;

    frame.origin.x = frame.origin.y = border;

    frame.size.width = _borderView.frame.size.width - (border * 2);
    frame.size.height = _borderView.frame.size.height - (border * 2);

    //NSLog(@"windowDidExitFullScreen: contentview is set to: %@", NSStringFromRect(frame));

    _contentView.frame = frame;
    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self contentDidResize: frame];
}

#pragma mark Accessibility

- (NSString *)accessibilityActionDescription: (NSString*) action {
    return [self.window accessibilityActionDescription:  action];
}

- (NSArray *)accessibilityActionNames {
    return [self.window  accessibilityActionNames];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute {
    return [self.window  accessibilityIsAttributeSettable: attribute];;
}

- (void)accessibilityPerformAction:(NSString *)action {
    [self.window  accessibilityPerformAction: action];
}

- (void)accessibilitySetValue: (id)value
                 forAttribute: (NSString*) attribute {
    [self.window  accessibilitySetValue: value
                           forAttribute: attribute];
}

- (NSArray*) accessibilityAttributeNames {
    NSMutableArray* result = [[self.window  accessibilityAttributeNames] mutableCopy];
    if (!result) result = [[NSMutableArray alloc] init];
    
    [result addObjectsFromArray:@[NSAccessibilityContentsAttribute,
     NSAccessibilityChildrenAttribute,
     NSAccessibilityHelpAttribute,
     NSAccessibilityDescriptionAttribute,
     NSAccessibilityTitleAttribute,
     NSAccessibilityFocusedUIElementAttribute]];
    
    return result;
}

- (id) accessibilityFocusedUIElement {
    NSResponder* firstResponder = self.window.firstResponder;
    
    if (firstResponder == nil) return self;
    
    if ([firstResponder isKindOfClass: [NSView class]]) {
        NSView* windowView = (NSView*) firstResponder;
        
        while (windowView != nil) {
            if ([windowView isKindOfClass: [GlkWindow class]]) {
                return windowView;
            }
            
            windowView = windowView.superview;
        }
    }
    
    return super.accessibilityFocusedUIElement;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
    if ([attribute isEqualToString: NSAccessibilityChildrenAttribute]
        || [attribute isEqualToString: NSAccessibilityContentsAttribute]) {
        //return [NSArray arrayWithObjects:gwindows count:MAXWIN];
           // return [NSArray arrayWithObject: rootWindow];
    } else if ([attribute isEqualToString: NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString: NSAccessibilityHelpAttribute]
               || [attribute isEqualToString: NSAccessibilityDescriptionAttribute]) {
        NSString* description = @"an interactive fiction game";
        return [NSString stringWithFormat: @"%@ %@",(!dead)?@"Running":@"Finished", description];
    } else if ([attribute isEqualToString: NSAccessibilityRoleDescriptionAttribute]) {
        return @"GLK view";
    } else if ([attribute isEqualToString: NSAccessibilityRoleAttribute]) {
        return NSAccessibilityGroupRole;
    } else if ([attribute isEqualToString: NSAccessibilityParentAttribute]) {
        return self.window;
    }

    NSLog(@"%@", attribute);
    
    return [super accessibilityAttributeValue: attribute];
}

- (BOOL)accessibilityIsIgnored {
    return NO;
}

@end
