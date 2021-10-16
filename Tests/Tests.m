//
//  Tests.m
//  Tests
//
//  Created by Administrator on 2021-07-20.
//

#import <XCTest/XCTest.h>
#import "CoreDataManager.h"

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkGraphicsWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"

#import "CommandScriptHandler.h"

#import "Game.h"
#import "Metadata.h"
#import "Constants.h"


@interface Tests : XCTestCase

@property (readonly) CoreDataManager *coreDataManager;
@property (strong) NSManagedObjectContext *managedObjectContext;

@end

@implementation Tests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    _managedObjectContext = self.coreDataManager.mainManagedObjectContext;

    gGameFileTypes = @[
        @"d$$", @"dat", @"sna", @"advsys", @"quill", @"l9",     @"mag",
        @"a3c", @"acd", @"agx", @"gam",    @"t3",    @"hex",    @"taf",
        @"z1",  @"z2",  @"z3",  @"z4",     @"z5",    @"z6",     @"z7",
        @"z8",  @"ulx", @"blb", @"blorb",  @"glb",   @"gblorb", @"zlb",
        @"zblorb"
    ];

    gDocFileTypes = @[@"rtf", @"rtfd", @"html", @"doc", @"docx", @"odt", @"xml", @"webarchive", @"txt"];

    gSaveFileTypes = @[@"glksave", @"sav", @"qut", @"qzl"];

    // To map the Glk style indices onto our Core Data relation names
    gBufferStyleNames = @[
        @"bufferNormal",
        @"bufEmph",
        @"bufPre",
        @"bufHead",
        @"bufSubH",
        @"bufAlert",
        @"bufNote",
        @"bufBlock",
        @"bufInput",
        @"bufUsr1",
        @"bufUsr2",
    ];

    gGridStyleNames = @[
        @"gridNormal",
        @"gridEmph",
        @"gridPre",
        @"gridHead",
        @"gridSubH",
        @"gridAlert",
        @"gridNote",
        @"gridBlock",
        @"gridInput",
        @"gridUsr1",
        @"gridUsr2",
    ];

    gExtMap = @{@"acd" : @"alan2", @"a3c" : @"alan3", @"d$$" : @"agility"};

    gFormatMap = @{
        @"adrift" : @"scare",
        @"advsys" : @"advsys",
        @"agt" : @"agility",
        @"glulx" : @"glulxe",
        @"hugo" : @"hugo",
        @"level9" : @"level9",
        @"magscrolls" : @"magnetic",
        @"quill" : @"unquill",
        @"tads2" : @"tadsr",
        @"tads3" : @"tadsr",
        @"zcode" : @"bocfel"
    };

    PasteboardFileURLPromise = (NSPasteboardType)kPasteboardTypeFileURLPromise;
    PasteboardFilePromiseContent = (NSPasteboardType)kPasteboardTypeFilePromiseContent;
    PasteboardFilePasteLocation = (NSPasteboardType)@"com.apple.pastelocation";
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testAutorestore {
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.

    NSFetchRequest *fetchRequest = [NSFetchRequest new];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.entity = entity;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"detectedFormat like[c] %@ OR detectedFormat like[c] %@ ", @"glulx", @"zcode"];

    NSError *error = nil;
    NSArray *fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    XCTAssertNotNil(fetchedObjects);

    if (fetchedObjects.count == 0)
        NSLog(@"No autosaveable games in library!");

    for (Game *game in fetchedObjects) {
        XCTestExpectation *exp = [[XCTestExpectation alloc] initWithDescription:@"Test passed"];
        __block GlkController *glkctl = [self startGame:game];
        if (glkctl) {
            double delayInSeconds = 2;
            dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
            dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
                CommandScriptHandler *csh = glkctl.commandScriptHandler;
                GlkWindow *inputView = [csh findGlkWindowWithInput];
//                if (inputView) {
//                    [csh startCommandScript:@"No\nNo\n" inWindow:inputView];
//                } else NSLog(@"No view with input found!");

                NSMutableDictionary *winDic = [NSMutableDictionary new];
                for (NSNumber *key in glkctl.gwindows.allKeys) {
                    NSLog(@"Comparing Glk window %@", key);
                    GlkWindow *win = glkctl.gwindows[key];
                    NSTextView *textView = [[NSTextView alloc] initWithFrame:win.frame];
                    if (![win isKindOfClass:[GlkGraphicsWindow class]])
                        textView.string = ((GlkTextBufferWindow *)win).textview.string;
                    winDic[key] = textView;
                }
                [glkctl.window performClose:nil];
                glkctl = [self startGame:game];
                double delayInSeconds = 2;
                dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
                dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
                    for (NSNumber *key in glkctl.gwindows.allKeys) {
                        NSLog(@"Comparing Glk window %@", key);
                        NSTextView *textView = winDic[key];
                        GlkWindow *win = glkctl.gwindows[key];
                        if (!NSEqualRects(textView.frame, win.frame)) {
                            NSLog(@"Error! Rects do not match: textView.frame: %@ win.frame: %@", NSStringFromRect(textView.frame), NSStringFromRect(win.frame));
                        }
//                        XCTAssert(NSEqualRects(textView.frame, win.frame));
                        if (textView.string.length) {
                            NSString *result = ((GlkTextBufferWindow *)win).textview.string;
                            [self saveStrings:result and:textView.string];
                            XCTAssert([result isEqualToString:textView.string]);
                        }
                    }
                    [exp fulfill];
                });
            });
            [self waitForExpectations:@[exp] timeout:6];
        }
    }
}

- (void)saveStrings:(NSString *)stringA and:(NSString *)stringB {
    NSError *error = nil;

    NSURL *desktopURL = [NSURL fileURLWithPath:@"/Users"
                                   isDirectory:YES];

    NSURL *tempFolderURL = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                                                        inDomain:NSUserDomainMask
                                               appropriateForURL:desktopURL
                                                          create:YES
                                                           error:&error];
    if (error)
        NSLog(@"Error: %@", error);
    error = nil;

    NSURL *urlA = [tempFolderURL URLByAppendingPathComponent:@"StringA.txt"];
    [stringA writeToURL:urlA atomically:NO encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);
    else
        NSLog(@"Wrote file \"StringA.txt\" to %@", urlA.path);
    error = nil;

    NSURL *urlB = [tempFolderURL URLByAppendingPathComponent:@"StringB.txt"];
    [stringB writeToURL:urlB atomically:NO encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);
    else
        NSLog(@"Wrote file \"StringB.txt\" to %@", urlB.path);
    error = nil;
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

- (nullable GlkController *)startGame:(Game *)game {
    NSURL *url = [game urlForBookmark];
    NSString *path = url.path;

    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (![fileManager isReadableFileAtPath:path]) {
        NSLog(@"File not found at %@", path);
        return nil;
    }

    if (!game.detectedFormat) {
        game.detectedFormat = game.metadata.format;
    }

    NSString *terp = gExtMap[path.pathExtension.lowercaseString];
    if (!terp)
        terp = gFormatMap[game.detectedFormat];


    if (!terp) {
        NSLog(@"Unsupported format");
        return nil;
    }

    GlkController *gctl = [[GlkController alloc] initWithWindowNibName:@"GameWindow"];
    if ([terp isEqualToString:@"glulxe"] || [terp isEqualToString:@"bocfel"]) {
        gctl.window.identifier = [NSString stringWithFormat:@"gameWin%@", game.ifid];
    } else
        gctl.window.restorable = NO;

    [gctl runTerp:terp withGame:game reset:NO winRestore:NO];
    NSLog(@"Started game %@", game.metadata.title);
    return gctl;
}

#pragma mark - Core Data stack

@synthesize coreDataManager = _coreDataManager;

- (CoreDataManager *)coreDataManager {
    // The persistent container for the application. This implementation creates and returns a container, having loaded the store for the application to it.
    @synchronized (self) {
        if (_coreDataManager == nil) {
            _coreDataManager = [[CoreDataManager alloc] initWithModelName:@"Spatterlight"];
        }
    }

    return _coreDataManager;
}

@end
