#import "GlkController_Private.h"
#import "GlkController+BorderColor.h"
#import "GlkController+FullScreen.h"
#import "GlkController+GlkRequests.h"
#import "GlkController+Speech.h"

#import "BufferTextView.h"
#import "GridTextView.h"
#import "CommandScriptHandler.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "Preferences.h"
#import "SoundHandler.h"
#import "ImageHandler.h"
#import "GlkEvent.h"
#import "FolderAccess.h"
#import "NotificationBezel.h"
#import "GlkStyle.h"
#import "CoverImageHandler.h"
#import "JourneyMenuHandler.h"
#import "TableViewController.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"

#import "NSString+Categories.h"
#import "NSColor+integer.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif

@implementation GlkController (Autorestore)

- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView
{
    // Search the subviews for a view of class GlkTextGridWindow
    GlkTextGridWindow __block __weak *(^weak_findGridWindow)(NSView *);

    GlkTextGridWindow * (^findGridWindow)(NSView *);

    weak_findGridWindow = findGridWindow = ^(NSView *view) {
        if ([view isKindOfClass:[GlkTextGridWindow class]])
            return (GlkTextGridWindow *)view;
        GlkTextGridWindow __block *foundView = nil;
        [view.subviews enumerateObjectsUsingBlock:^(NSView *subview, NSUInteger idx, BOOL *stop) {
            foundView = weak_findGridWindow(subview);
            if (foundView)
                *stop = YES;
        }];
        return foundView;
    };

    return findGridWindow(theView);
}

- (Theme *)findThemeByName:(NSString *)name {
    if (!name || name.length == 0)
        return nil;
    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    NSManagedObjectContext *context = libcontroller.managedObjectContext;
    Theme *foundTheme = nil;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    NSError *error = nil;
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects && fetchedObjects.count) {
        foundTheme = fetchedObjects[0];
    } else {
        if (error != nil)
            NSLog(@"GlkController findThemeByName: %@", error);
    }
    return foundTheme;
}

- (void)restoreUI {
    // We try to restore the UI here, in order to catch things
    // like entered text and scrolling, that has changed the UI
    // but not sent any events to the interpreter process.
    // This method is called in handleRequest on NEXTEVENT.

    if (restoreUIOnly) {
        restoredController = restoredControllerLate;
        self.shouldShowAutorestoreAlert = NO;
    } else {
        self.windowsToBeAdded = [[NSMutableArray alloc] init];

        if (restoredController.commandScriptRunning) {
            CommandScriptHandler *handler = restoredControllerLate.commandScriptHandler;
            if (handler.commandIndex >= handler.commandArray.count - 1) {
                restoredController.commandScriptHandler = nil;
                restoredController.commandScriptRunning = NO;
            } else {
                skipNextScriptCommand = YES;
                restoredController = restoredControllerLate;
            }
        }
    }

    shouldRestoreUI = NO;
    self.shouldStoreScrollOffset = NO;

    self.soundHandler = restoredControllerLate.soundHandler;
    self.soundHandler.glkctl = self;
    [self.soundHandler restartAll];

    GlkWindow *win;

    // Copy values from autorestored GlkController object
    self.firstResponderView = restoredControllerLate.firstResponderView;
    self.windowPreFullscreenFrame = [self frameWithSanitycheckedSize:restoredControllerLate.windowPreFullscreenFrame];
    self.autosaveTag = restoredController.autosaveTag;

    self.bufferStyleHints = restoredController.bufferStyleHints;
    self.gridStyleHints = restoredController.gridStyleHints;

    // Restore frame size
    self.gameView.frame = restoredControllerLate.storedGameViewFrame;

    // Copy all views and GlkWindow objects from restored Controller
    for (id key in restoredController.gwindows.allKeys) {

        win = self.gwindows[key];

        if (!restoreUIOnly) {
            if (win) {
                [win removeFromSuperview];
            }
            win = (restoredController.gwindows)[key];

            self.gwindows[@(win.name)] = win;

            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                NSScrollView *scrollview = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;

                GlkTextGridWindow *aView = [self findGridWindowIn:scrollview];
                while (aView) {
                    [aView removeFromSuperview];
                    aView = [self findGridWindowIn:scrollview];
                }

                GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)win).quoteBox;
                if (quotebox && (restoredController.numberOfPrintsAndClears == quotebox.quoteboxAddedOnPAC) && (restoredController.printsAndClearsThisTurn == 0 || quotebox.quoteboxAddedOnPAC == 0)) {
                    if (self.quoteBoxes == nil)
                        self.quoteBoxes = [[NSMutableArray alloc] init];
                    [quotebox removeFromSuperview];
                    [self.quoteBoxes addObject:quotebox];
                    quotebox.glkctl = self;
                    quotebox.quoteboxParent = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;
                }
            }

            [win removeFromSuperview];
            [self.gameView addSubview:win];

            win.glkctl = self;
            win.theme = self.theme;

        // Ugly hack to make sure quote boxes shown at game start still appear when restoring UI only
        } else if (self.quoteBoxes.count == 0 && self.restorationHandler && [win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkWindow *restored = restoredController.gwindows[@(win.name)];
            GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)restored).quoteBox;
            if (quotebox) {
                self.quoteBoxes = [[NSMutableArray alloc] init];
                [quotebox removeFromSuperview];
                [self.quoteBoxes addObject:quotebox];
                quotebox.glkctl = self;
                quotebox.quoteboxAddedOnPAC = self.numberOfPrintsAndClears;
                quotebox.quoteboxParent = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;
                ((GlkTextBufferWindow *)win).quoteBox = quotebox;
            }
        }
        GlkWindow *laterWin = (restoredControllerLate.gwindows)[key];
        win.frame = laterWin.frame;
    }

    // This makes autorestoring in fullscreen a little less flickery
    [self adjustContentView];

    if (restoredControllerLate.bgcolor)
        [self setBorderColor:restoredControllerLate.bgcolor];

    GlkWindow *winToGrabFocus = nil;

    if (restoredController.commandScriptRunning) {
        [self.commandScriptHandler copyPropertiesFrom:restoredController.commandScriptHandler];
    }

    if (self.gameID == kGameIsJourney && restoredController.journeyMenuHandler) {
        self.journeyMenuHandler = restoredController.journeyMenuHandler;
        restoredController.journeyMenuHandler = nil;
        self.journeyMenuHandler.delegate = self;
        self.journeyMenuHandler.textGridWindow = (GlkTextGridWindow *)self.gwindows[@(self.journeyMenuHandler.gridTextWinName)];
        self.journeyMenuHandler.textBufferWindow = (GlkTextBufferWindow *)self.gwindows[@(self.journeyMenuHandler.bufferTextWinName)];

        [self.journeyMenuHandler showJourneyMenus];
    }

    // Restore scroll position etc
    for (win in self.gwindows.allValues) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]] && !self.windowsToRestore.count) {
            [win postRestoreAdjustments:(restoredControllerLate.gwindows)[@(win.name)]];
        }
        if (win.name == self.firstResponderView) {
            winToGrabFocus = win;
        }
        if (self.commandScriptRunning && win.name == self.commandScriptHandler.lastCommandWindow) {
            if (self.commandScriptHandler.lastCommandType == kCommandTypeLine) {
                [self.commandScriptHandler sendCommandLineToWindow:win];
            } else {
                [self.commandScriptHandler sendCommandKeyPressToWindow:win];
            }
        }
    }

    if (winToGrabFocus)
        [winToGrabFocus grabFocus];

    if (!restoreUIOnly)
        self.hasAutoSaved = YES;

    restoredController = nil;
    restoredControllerLate = nil;
    restoreUIOnly = NO;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that
    // affect window size since the autosave was created.

    [self performSelector:@selector(postRestoreArrange:) withObject:nil afterDelay:0.2];
}


- (void)postRestoreArrange:(id)sender {
    if (!self.startingInFullscreen) {
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
    }

    Theme *stashedTheme = self.stashedTheme;
    if (stashedTheme && stashedTheme != self.theme) {
        self.theme = stashedTheme;
        self.stashedTheme = nil;
    }
    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:self.theme];
    [self notePreferencesChanged:notification];
    self.shouldStoreScrollOffset = YES;

    // Now we can actually show the window
    if (![self runWindowsRestorationHandler]) {
        [self showWindow:nil];
        [self.window makeKeyAndOrderFront:nil];
        [self.window makeFirstResponder:nil];
    }
    if (self.startingInFullscreen) {
        [self performSelector:@selector(deferredEnterFullscreen:) withObject:nil afterDelay:0.1];
    } else {
        autorestoring = NO;
    }
}


- (NSString *)buildAppSupportDir {
    {
        NSDictionary *gFolderMap = @{@"adrift" : @"SCARE",
                                     @"advsys" : @"AdvSys",
                                     @"agt"    : @"AGiliTy",
                                     @"glulx"  : @"Glulxe",
                                     @"hugo"   : @"Hugo",
                                     @"level9" : @"Level 9",
                                     @"magscrolls" : @"Magnetic",
                                     @"quest4" : @"Quest",
                                     @"quill"  : @"UnQuill",
                                     @"tads2"  : @"TADS",
                                     @"tads3"  : @"TADS",
                                     @"zcode"  : @"Bocfel",
                                     @"sagaplus" : @"Plus",
                                     @"scott"  : @"ScottFree",
                                     @"jacl"   : @"JACL",
                                     @"taylor" : @"TaylorMade"
        };

        NSDictionary *gFolderMapExt = @{@"acd" : @"Alan 2",
                                        @"a3c" : @"Alan 3",
                                        @"d$$" : @"AGiliTy"};

        NSError *error;
        NSURL *appSupportURL = [[NSFileManager defaultManager]
                                URLForDirectory:NSApplicationSupportDirectory
                                inDomain:NSUserDomainMask
                                appropriateForURL:nil
                                create:NO
                                error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@",
                  error);

        if (appSupportURL == nil)
            return nil;

        Game *game = self.game;
        NSString *detectedFormat = game.detectedFormat;

        if (!detectedFormat) {
            NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", game.metadata.title);
            return nil;
        }

        NSString *signature = self.gamefile.signatureFromFile;

        if (signature.length == 0) {
            signature = game.hashTag;
            if (signature.length == 0) {
                NSLog(@"GlkController appSupportDir: Could not create signature from game file \"%@\"!", self.gamefile);
                return nil;
            }
        }

        NSString *terpFolder =
        [gFolderMap[detectedFormat]
         stringByAppendingString:@" Files"];

        if (!terpFolder) {
            terpFolder = [gFolderMapExt[self.gamefile.pathExtension.lowercaseString]
                          stringByAppendingString:@" Files"];
        }

        if (!terpFolder) {
            NSLog(@"GlkController appSupportDir: Could not map game format %@ to a folder name!", detectedFormat);
            return nil;
        }

        NSString *dirstr =
        [@"Spatterlight" stringByAppendingPathComponent:terpFolder];
        dirstr = [dirstr stringByAppendingPathComponent:@"Autosaves"];
        dirstr = [dirstr
                  stringByAppendingPathComponent:signature];
        dirstr = [dirstr stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];

        appSupportURL = [NSURL URLWithString:dirstr
                               relativeToURL:appSupportURL];

        if (self.supportsAutorestore && self.theme.autosave) {
            error = nil;
            BOOL succeed  = [[NSFileManager defaultManager] createDirectoryAtURL:appSupportURL
                                     withIntermediateDirectories:YES
                                                      attributes:nil
                                                           error:&error];

            if (!succeed || error != nil) {
                NSLog(@"Error when creating appSupportDir: %@", error);
            }

            NSString *dummyfilename = [game.metadata.title
                                       stringByAppendingPathExtension:@"txt"];

            NSString *dummytext = [NSString
                                   stringWithFormat:
                                       @"This file, %@, was placed here by Spatterlight in order to make "
                                   @"it easier for humans to guess what game these autosave files belong "
                                   @"to. Any files in this folder are for the game %@, or possibly "
                                   @"a game with a different name but identical contents.",
                                   dummyfilename, game.metadata.title];

            NSString *dummyfilepath =
            [appSupportURL.path stringByAppendingPathComponent:dummyfilename];

            error = nil;
            succeed =
            [dummytext writeToURL:[NSURL fileURLWithPath:dummyfilepath isDirectory:NO]
                       atomically:YES
                         encoding:NSUTF8StringEncoding
                            error:&error];
            if (!succeed || error) {
                NSLog(@"Failed to write dummy file to autosave directory. Error:%@",
                      error);
            }
        }
        return appSupportURL.path;
    }
    return nil;
}

// If there exists an autosave dir using the old hashing method,
// copy any files from it to the new one, unless there are newer
// equivalents in the new-style autosave directory. Then delete the
// old files and the old directory.

-(void)dealWithOldFormatAutosaveDir {
    NSError *error = nil;

    NSURL *newDirURL = [NSURL fileURLWithPath:self.appSupportDir isDirectory:YES];

    // Create the URL an old-format autosave directory would have
    NSString *oldDirPath = [self.appSupportDir stringByDeletingLastPathComponent];
    oldDirPath = [oldDirPath stringByAppendingPathComponent:self.gamefile.oldSignatureFromFile];
    NSURL *oldDirURL = [NSURL fileURLWithPath:oldDirPath isDirectory:YES];

    // Get a list of files in the old directory and iterate through them
    NSArray<NSURL *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:oldDirURL includingPropertiesForKeys:nil options:0 error:&error];
    if (files.count) {
        for (NSURL *url in files) {
            error = nil;
            NSData *data = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedAlways error:&error];
            if (!data) {
                NSLog(@"GlkController: Could not read file in old autosave directory. Error: %@", error);
                continue;
            } else {
                NSString *filename = [url lastPathComponent];
                NSDate *dateInOld = nil, *dateInNew = nil;

                // Get the creation date of the file
                error = nil;
                NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:&error];
                if (attrs) {
                    dateInOld = (NSDate*)attrs[NSFileCreationDate];
                } else {
                    NSLog(@"GlkController: Failed to get creation date of file %@ in old directory. Error:%@", url.path, error);
                }
                error = nil;
                NSURL *newfileURL = [newDirURL URLByAppendingPathComponent:filename];
                BOOL overwrite = YES;

                // Get the creation date of the file with the same name in the new directory,
                // if there is one.
                error = nil;
                attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:newfileURL.path error:&error];
                if (attrs) {
                    dateInNew = (NSDate*)attrs[NSFileCreationDate];
                    if (dateInNew && dateInOld && [dateInNew compare:dateInOld] == NSOrderedDescending) {
                        NSLog(@"%@: Newer file exists in new directory. Don't overwrite! dateInNew:%@ dateInOld:%@", filename, dateInNew, dateInOld);
                        overwrite = NO;
                    }
                } else {
                    NSLog(@"GlkController: Failed to get creation date of file %@ in new directory. Error:%@", url.path, error);
                }

                // Copy the file from the old directory to the new, overwriting any older files there
                BOOL result = NO;
                if (overwrite) {
                    result = [data writeToURL:newfileURL options:NSDataWritingAtomic error:&error];
                    if (!result) {
                        NSLog(@"GlkController: Failed to copy file %@ to new autosave directory. Error:%@", filename, error);
                    }
                }

                // Delete the original file
                result = [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
                if (!result) {
                    NSLog(@"GlkController: Failed to delete file %@ in old autosave directory. Error:%@", filename, error);
                }
            }
        }
        // Delete the old directory
        BOOL result = [[NSFileManager defaultManager] removeItemAtURL:oldDirURL error:&error];
        if (!result) {
            NSLog(@"GlkController: Failed to delete old autosave directory at %@. Error:%@", oldDirURL.path, error);
        }
        // Re-write the late UI autosave, to give it a creation date later than autosave-GUI.plist
        // (Our autorestore code will compare the creation dates of autosave-GUI.plist and
        // autosave-GUI-late.plist.)
        NSURL *lateUIURL = [newDirURL URLByAppendingPathComponent:@"autosave-GUI-late.plist"];
        NSData *lateUISave = [NSData dataWithContentsOfURL:lateUIURL options:NSDataReadingMappedAlways error:&error];
        if (lateUISave) {
            [lateUISave writeToURL:lateUIURL options:NSDataWritingAtomic error:&error];
        }
    }
}

- (NSString *)autosaveFileGUI {
    if (!_autosaveFileGUI)
        _autosaveFileGUI = [self.appSupportDir
                            stringByAppendingPathComponent:@"autosave-GUI.plist"];
    return _autosaveFileGUI;
}

- (NSString *)autosaveFileTerp {
    if (!_autosaveFileTerp)
        _autosaveFileTerp =
        [self.appSupportDir stringByAppendingPathComponent:@"autosave.plist"];
    return _autosaveFileTerp;
}

// TableViewController calls this to reset non-running games
- (void)deleteAutosaveFilesForGame:(Game *)aGame {
    self.gamefile = aGame.urlForBookmark.path;
    aGame.autosaved = NO;
    if (!self.gamefile)
        return;
    self.game = aGame;
    [self deleteAutosaveFiles];
}

- (void)deleteAutosaveFiles {
    if (self.autosaveFileGUI == nil || self.autosaveFileTerp == nil)
        return;
    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI isDirectory:NO],
                         [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.glksave"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI-late.plist"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"]  isDirectory:NO]]];
}

- (void)deleteFiles:(NSArray<NSURL *> *)urls {
    NSError *error;
    for (NSURL *url in urls) {
        error = nil;
        [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
        //        if (error)
        //            NSLog(@"Error: %@", error);
    }
}

- (void)autoSaveOnExit {
    if (self.supportsAutorestore && self.theme.autosave && self.appSupportDir.length) {
        NSString *autosaveLate = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

        NSError *error = nil;

        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
        [data writeToFile:autosaveLate options:NSDataWritingAtomic error:&error];

        if (error) {
            NSLog(@"autoSaveOnExit: Write returned error: %@", [error localizedDescription]);
            return;
        }

        self.game.autosaved = !dead;
    } else {
        [self deleteAutosaveFiles];
        self.game.autosaved = NO;
    }
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    [NSKeyedUnarchiver setClass:[BufferTextView class] forClassName:@"MyTextView"];
    [NSKeyedUnarchiver setClass:[GridTextView class] forClassName:@"MyGridTextView"];

    self = [super initWithCoder:decoder];
    if (self) {
        dead = [decoder decodeBoolForKey:@"dead"];
        waitforevent = NO;
        waitforfilename = NO;

        /* the glk objects */

        self.autosaveVersion = [decoder decodeIntegerForKey:@"version"];
        self.hasAutoSaved = [decoder decodeBoolForKey:@"hasAutoSaved"];
        self.autosaveTag =  [decoder decodeIntegerForKey:@"autosaveTag"];

        self.gwindows = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"gwindows"];

        self.soundHandler = [decoder decodeObjectOfClass:[SoundHandler class] forKey:@"soundHandler"];
        self.imageHandler = [decoder decodeObjectOfClass:[ImageHandler class] forKey:@"imageHandler"];

        self.storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        self.windowPreFullscreenFrame =
        [decoder decodeRectForKey:@"windowPreFullscreenFrame"];

        self.storedGameViewFrame = [decoder decodeRectForKey:@"contentFrame"];

        self.bgcolor = [decoder decodeObjectOfClass:[NSColor class] forKey:@"backgroundColor"];

        self.bufferStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"bufferStyleHints"];
        self.gridStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"gridStyleHints"];

        self.queue = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"queue"];

        self.firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        self.inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        self.turns = [decoder decodeIntegerForKey:@"turns"];
        self.numberOfPrintsAndClears = [decoder decodeIntegerForKey:@"printsAndClears"];
        self.printsAndClearsThisTurn = [decoder decodeIntegerForKey:@"printsAndClearsThisTurn"];

        self.oldThemeName = [decoder decodeObjectOfClass:[NSString class] forKey:@"oldThemeName"];

        self.showingCoverImage = [decoder decodeBoolForKey:@"showingCoverImage"];

        self.commandScriptRunning = [decoder decodeBoolForKey:@"commandScriptRunning"];
        if (self.commandScriptRunning)
            self.commandScriptHandler = [decoder decodeObjectOfClass:[CommandScriptHandler class] forKey:@"commandScriptHandler"];

        self.journeyMenuHandler = [decoder decodeObjectOfClass:[JourneyMenuHandler class] forKey:@"journeyMenuHandler"];
        if (self.journeyMenuHandler)
            self.journeyMenuHandler.delegate = self;
        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:AUTOSAVE_SERIAL_VERSION forKey:@"version"];
    [encoder encodeBool:self.hasAutoSaved forKey:@"hasAutoSaved"];
    [encoder encodeInteger:self.autosaveTag forKey:@"autosaveTag"];

    [encoder encodeBool:dead forKey:@"dead"];
    [encoder encodeRect:self.window.frame forKey:@"windowFrame"];
    [encoder encodeRect:self.gameView.frame forKey:@"contentFrame"];

    [encoder encodeObject:self.bgcolor forKey:@"backgroundColor"];

    [encoder encodeObject:self.bufferStyleHints forKey:@"bufferStyleHints"];
    [encoder encodeObject:self.gridStyleHints forKey:@"gridStyleHints"];

    [encoder encodeObject:self.gwindows forKey:@"gwindows"];
    [encoder encodeObject:self.soundHandler forKey:@"soundHandler"];
    [encoder encodeObject:self.imageHandler forKey:@"imageHandler"];
    [encoder encodeObject:self.journeyMenuHandler forKey:@"journeyMenuHandler"];

    [encoder encodeRect:self.windowPreFullscreenFrame
                 forKey:@"windowPreFullscreenFrame"];

    [encoder encodeObject:self.queue forKey:@"queue"];
    self.firstResponderView = -1;

    NSResponder *firstResponder = self.window.firstResponder;

    if ([firstResponder isKindOfClass:[GlkWindow class]]) {
        self.firstResponderView = ((GlkWindow *)firstResponder).name;
    } else {
        id delegate = nil;
        if ([firstResponder isKindOfClass:[NSTextView class]]) {
            delegate = ((NSTextView *)firstResponder).delegate;
            if (![delegate isKindOfClass:[GlkWindow class]]) {
                delegate = nil;
            }
        }
        if (delegate) {
            self.firstResponderView = ((GlkWindow *)delegate).name;
        }
    }
    [encoder encodeInteger:self.firstResponderView forKey:@"firstResponder"];
    [encoder encodeBool:((self.window.styleMask & NSWindowStyleMaskFullScreen) ==
                         NSWindowStyleMaskFullScreen)
                 forKey:@"fullscreen"];

    [encoder encodeInteger:self.turns forKey:@"turns"];
    [encoder encodeInteger:self.numberOfPrintsAndClears forKey:@"printsAndClears"];
    [encoder encodeInteger:self.printsAndClearsThisTurn forKey:@"printsAndClearsThisTurn"];

    [encoder encodeObject:self.theme.name forKey:@"oldThemeName"];

    [encoder encodeBool:self.showingCoverImage forKey:@"showingCoverImage"];

    [encoder encodeBool:self.commandScriptRunning forKey:@"commandScriptRunning"];
    if (self.commandScriptRunning)
        [encoder encodeObject:self.commandScriptHandler forKey:@"commandScriptHandler"];
}

- (void)showAutorestoreAlert:(id)userInfo {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *alertSuppressionKey = @"AutorestoreAlertSuppression";
    NSString *alwaysAutorestoreKey = @"AlwaysAutorestore";

    if ([defaults boolForKey:alertSuppressionKey] == YES)
        self.shouldShowAutorestoreAlert = NO;

    if (!self.shouldShowAutorestoreAlert) {
        self.mustBeQuiet = NO;
        [self.journeyMenuHandler recreateDialog];
        return;
    }

    if (dead)
        return;

    self.mustBeQuiet = YES;

    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    NSLocalizedString(@"This game was automatically restored from a previous session.", nil);
    anAlert.informativeText = NSLocalizedString(@"Would you like to start over instead?", nil);
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Remember this choice.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Continue", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Restart", nil)];

    GlkController * __weak weakSelf = self;

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

        weakSelf.mustBeQuiet = NO;

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

        weakSelf.shouldShowAutorestoreAlert = NO;

        if (result == NSAlertSecondButtonReturn) {
            [self reset:nil];
            if (anAlert.suppressionButton.state == NSOnState) {
                [defaults setBool:NO forKey:alwaysAutorestoreKey];
            }
            return;
        } else {
            if (anAlert.suppressionButton.state == NSOnState) {
                [defaults setBool:YES forKey:alwaysAutorestoreKey];
            }
        }

        if (weakSelf.gameID == kGameIsJourney)
            [weakSelf.journeyMenuHandler recreateDialog];

        [self forceSpeech];
    }];
}

- (IBAction)reset:(id)sender {
    if (self.lastResetTimestamp && self.lastResetTimestamp.timeIntervalSinceNow < -1) {
        restartingAlready = NO;
    }

    if (restartingAlready || self.showingCoverImage)
        return;

    restartingAlready = YES;
    self.lastResetTimestamp = [NSDate date];
    self.mustBeQuiet = YES;

    [[NSNotificationCenter defaultCenter]
     removeObserver:self name: NSFileHandleDataAvailableNotification
     object: readfh];

    self.commandScriptHandler = nil;
    _journeyMenuHandler = nil;

    if (task) {
        // Stop the interpreter
        task.terminationHandler = nil;
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
        task = nil;
    }

    [self performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.3];
}

- (void)deferredRestart:(id)sender {
    [self cleanup];
    [self runTerp:(NSString *)self.terpname
         withGame:(Game *)self.game
            reset:YES
       restorationHandler:nil];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
    NSAccessibilityPostNotification(self.window.firstResponder, NSAccessibilityFocusedUIElementChangedNotification);
}

-(void)cleanup {
    [self.soundHandler stopAllAndCleanUp];
    [self deleteAutosaveFiles];
    [self handleSetTimer:0];

    for (GlkWindow *win in self.gwindows.allValues) {
        win.glkctl = nil;
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            ((GlkTextBufferWindow *)win).quoteBox = nil;
        [win removeFromSuperview];
    }

    for (GlkTextGridWindow *win in self.quoteBoxes) {
        win.glkctl = nil;
        win.quoteboxParent = nil;
        [win removeFromSuperview];
    }

    if (self.form) {
        self.form.glkctl = nil;
        self.form = nil;
    }

    if (self.zmenu) {
        self.zmenu.glkctl = nil;
        self.zmenu = nil;
    }

    readfh = nil;
    sendfh = nil;
    task = nil;
}

- (void)handleAutosave:(NSInteger)hash {
    self.autosaveTag = hash;

    @autoreleasepool {
        NSError *error = nil;
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
        [data writeToFile:self.autosaveFileGUI options:NSDataWritingAtomic error:&error];
        if (error) {
            NSLog(@"handleAutosave: Write returned error: %@", [error localizedDescription]);
            return;
        }
    }

    self.hasAutoSaved = YES;
}

/*
 *
 */

@end
