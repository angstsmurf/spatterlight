//
//  PreferencesThemeSelectionTests.m
//  SpatterlightTests
//
//  The themes table selection of a Preferences panel loaded from
//  PrefsWindow.xib over an in-memory store: restoring a selection before the
//  themes array controller has fetched, and editing a built-in theme making
//  exactly one clone however the edits and run loop passes interleave.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "ThemeArrayController.h"
#import "PreferencesPrivate.h"

@interface PreferencesThemeSelectionTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *builtInTheme;
@property (nonatomic, strong) Theme *savedTheme;
@property (nonatomic, strong) Preferences *prefs;
@property (nonatomic, strong) NSArray *topLevelObjects;

@end

@implementation PreferencesThemeSelectionTests

- (void)setUp {
    [super setUp];
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];

    NSFetchRequest *request = [Theme fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"name == %@", @"Default"];
    self.builtInTheme = [self.context executeFetchRequest:request error:nil].firstObject;
    XCTAssertNotNil(self.builtInTheme);
    XCTAssertFalse(self.builtInTheme.editable);

    self.savedTheme = Preferences.theme;
    Preferences.theme = self.builtInTheme;

    // The nib binds the themes array controller to the owner's context,
    // which is the app's store unless it is set first
    Preferences *prefs = [[Preferences alloc] init];
    [prefs setValue:self.context forKey:@"managedObjectContext"];
    NSNib *nib = [[NSNib alloc] initWithNibNamed:@"PrefsWindow"
                                          bundle:[NSBundle bundleForClass:[Preferences class]]];
    NSArray *topLevelObjects = nil;
    XCTAssertTrue([nib instantiateWithOwner:prefs topLevelObjects:&topLevelObjects]);
    self.topLevelObjects = topLevelObjects;
    self.prefs = prefs;
    XCTAssertEqual(prefs.arrayController.managedObjectContext, self.context);

    [prefs setUpSimpleSettings];
}

- (void)tearDown {
    [NSObject cancelPreviousPerformRequestsWithTarget:self.prefs];
    // Let any deferred selection finish before the panel goes away
    [self spinFor:0.2];
    Preferences.theme = self.savedTheme;
    self.prefs = nil;
    self.topLevelObjects = nil;
    self.builtInTheme = nil;
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

- (void)spinFor:(NSTimeInterval)seconds {
    NSDate *end = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while (end.timeIntervalSinceNow > 0)
        [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
}

- (BOOL)spinUntil:(BOOL (^)(void))condition {
    NSDate *end = [NSDate dateWithTimeIntervalSinceNow:3];
    while (!condition() && end.timeIntervalSinceNow > 0)
        [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    return condition();
}

- (NSArray<Theme *> *)clonesOf:(Theme *)theme {
    NSFetchRequest *request = [Theme fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"defaultParent == %@", theme];
    return [self.context executeFetchRequest:request error:nil];
}

- (void)toggle:(NSString *)outlet {
    NSButton *button = [self.prefs valueForKey:outlet];
    button.state = (button.state == NSControlStateValueOn) ? NSControlStateValueOff : NSControlStateValueOn;
    [self.prefs changeSimpleSetting:button];
}

- (void)selectBuiltInTheme {
    [self.prefs restoreThemeSelection:self.builtInTheme];
    XCTAssertTrue([self spinUntil:^BOOL{ return self.prefs.arrayController.selectedTheme == self.builtInTheme; }]);
    [self.prefs updatePrefsPanel];
}

- (void)assertOneCloneSelected {
    NSArray<Theme *> *clones = [self clonesOf:self.builtInTheme];
    XCTAssertEqual(clones.count, 1u, @"Clones: %@", [clones valueForKey:@"name"]);
    Theme *clone = clones.firstObject;
    XCTAssertTrue(clone.editable);
    XCTAssertEqualObjects(clone.name, @"Default (modified)");
    XCTAssertEqual(Preferences.theme, clone);
    XCTAssertTrue([self spinUntil:^BOOL{ return self.prefs.arrayController.selectedTheme == clone; }],
                  @"Selected: %@", self.prefs.arrayController.selectedTheme.name);
    XCTAssertEqual(Preferences.theme, clone);
}

// The array controller fetches in a later run loop pass than the nib load
- (void)testSelectionRestoredBeforeFetchIsAppliedAfterIt {
    [self.prefs restoreThemeSelection:self.builtInTheme];
    XCTAssertTrue([self spinUntil:^BOOL{ return self.prefs.arrayController.selectedTheme == self.builtInTheme; }],
                  @"Selected: %@", self.prefs.arrayController.selectedTheme.name);
    XCTAssertEqual(Preferences.theme, self.builtInTheme);
}

- (void)testEditsWithoutRunLoopPassesMakeOneClone {
    [self selectBuiltInTheme];
    [self toggle:@"btnSmartQuotes"];
    [self toggle:@"btnEnableSound"];
    [self toggle:@"btnDeterminism"];
    [self assertOneCloneSelected];
}

- (void)testEditsWithRunLoopPassesMakeOneClone {
    [self selectBuiltInTheme];
    for (NSString *outlet in @[ @"btnSmartQuotes", @"btnEnableSound", @"btnDeterminism", @"btnSmartQuotes" ]) {
        [self toggle:outlet];
        // Shorter and longer than the 0.1 second restore timer
        [self spinFor:0.05];
        [self toggle:outlet];
        [self spinFor:0.3];
    }
    [self assertOneCloneSelected];
    XCTAssertEqual([self clonesOf:[self clonesOf:self.builtInTheme].firstObject].count, 0u);
}

// An edit while the table is still waiting for its content: the clone,
// not the theme asked for before the fetch, must end up selected
- (void)testEditBeforeFetchMakesOneClone {
    // As windowDidLoad: the controls show the theme before the fetch, so
    // toggling one changes it
    [self.prefs updatePrefsPanel];
    [self.prefs restoreThemeSelection:self.builtInTheme];
    XCTAssertEqual(self.prefs.arrayController.selectedTheme, nil);
    [self toggle:@"btnSmartQuotes"];
    // Edit again as soon as the deferred restore has selected a row, before
    // the clone's own 0.1 second restore timer: if the original were selected
    // now, this edit would clone it a second time
    XCTAssertTrue([self spinUntil:^BOOL{ return self.prefs.arrayController.selectedTheme != nil; }]);
    [self toggle:@"btnDeterminism"];
    [self spinFor:0.3];
    [self assertOneCloneSelected];
    [self toggle:@"btnEnableSound"];
    [self spinFor:0.3];
    [self assertOneCloneSelected];
}

@end
