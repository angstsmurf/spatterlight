//
//  PreferencesThemeSelectionTests.m
//  SpatterlightTests
//
//  The themes table selection of a Preferences panel loaded from
//  PrefsWindow.xib over an in-memory store: restoring a selection before the
//  themes array controller has fetched, and editing a built-in theme making
//  exactly one clone however the edits and run loop passes interleave.
//  Also the edits that clone: changeBooleanAttribute:, +scale: and the
//  paragraph popover.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "ThemeArrayController.h"
#import "ParagraphPopOver.h"
#import "PreferencesPrivate.h"

@interface PreferencesThemeSelectionTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *builtInTheme;
@property (nonatomic, strong) Theme *savedTheme;
@property (nonatomic, strong) Preferences *savedInstance;
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
    self.savedInstance = Preferences.instance;

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
    Preferences.instance = self.savedInstance;
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

#pragma mark - Edits that clone

- (void)testBooleanAttributeClonesOnlyOnChange {
    [self selectBuiltInTheme];
    BOOL doStyles = self.builtInTheme.doStyles;
    NSButton *button = [NSButton checkboxWithTitle:@"doStyles" target:nil action:nil];

    // The value the theme already has: no clone
    button.state = doStyles ? NSControlStateValueOn : NSControlStateValueOff;
    [self.prefs changeBooleanAttribute:@"doStyles" fromButton:button];
    XCTAssertEqual([self clonesOf:self.builtInTheme].count, 0u);
    XCTAssertEqual(Preferences.theme, self.builtInTheme);

    button.state = doStyles ? NSControlStateValueOff : NSControlStateValueOn;
    [self.prefs changeBooleanAttribute:@"doStyles" fromButton:button];
    [self assertOneCloneSelected];
    XCTAssertEqual(Preferences.theme.doStyles, !doStyles);
    XCTAssertEqual(self.builtInTheme.doStyles, doStyles);

    // The clone is editable, so changed in place
    button.state = doStyles ? NSControlStateValueOn : NSControlStateValueOff;
    [self.prefs changeBooleanAttribute:@"doStyles" fromButton:button];
    [self assertOneCloneSelected];
    XCTAssertEqual(Preferences.theme.doStyles, doStyles);
}

- (NSParagraphStyle *)paragraphStyleOf:(GlkStyle *)style {
    return style.attributeDict[NSParagraphStyleAttributeName];
}

- (void)assertParagraphStyle:(NSParagraphStyle *)scaled is:(CGFloat)factor times:(NSParagraphStyle *)original {
    XCTAssertNotNil(scaled);
    XCTAssertEqualWithAccuracy(scaled.lineSpacing, original.lineSpacing * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.paragraphSpacing, original.paragraphSpacing * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.paragraphSpacingBefore, original.paragraphSpacingBefore * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.headIndent, original.headIndent * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.tailIndent, original.tailIndent * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.firstLineHeadIndent, original.firstLineHeadIndent * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.maximumLineHeight, original.maximumLineHeight * factor, 0.001);
    XCTAssertEqualWithAccuracy(scaled.minimumLineHeight, original.minimumLineHeight * factor, 0.001);
}

- (void)testScaleClonesAndScalesEveryLength {
    // +scale: clones through the shared instance
    Preferences.instance = self.prefs;
    [self selectBuiltInTheme];
    CGFloat gridSize = self.builtInTheme.gridNormal.font.pointSize;
    CGFloat bufferSize = self.builtInTheme.bufferNormal.font.pointSize;
    NSParagraphStyle *bufferPara = [self paragraphStyleOf:self.builtInTheme.bufferNormal];
    XCTAssertNotNil(bufferPara);

    [Preferences scale:2];
    [self assertOneCloneSelected];
    Theme *clone = Preferences.theme;
    XCTAssertEqual(clone.gridNormal.font.pointSize, gridSize * 2);
    XCTAssertEqual(clone.bufferNormal.font.pointSize, bufferSize * 2);
    [self assertParagraphStyle:[self paragraphStyleOf:clone.bufferNormal] is:2 times:bufferPara];
    // The cell size follows the font
    XCTAssertEqual(clone.cellHeight, clone.gridNormal.cellSize.height);
    XCTAssertEqual(self.builtInTheme.gridNormal.font.pointSize, gridSize);

    // User styles (populateStyles regenerates the others): one with kerning,
    // a baseline offset and a paragraph style, one with none of them
    NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];
    para.lineSpacing = 1;
    para.paragraphSpacing = 2;
    para.paragraphSpacingBefore = 3;
    para.headIndent = 4;
    para.tailIndent = -5;
    para.firstLineHeadIndent = 6;
    para.minimumLineHeight = 7;
    para.maximumLineHeight = 8;
    GlkStyle *emph = clone.bufEmph;
    emph.autogenerated = NO;
    emph.attributeDict = @{ NSFontAttributeName: [NSFont fontWithName:@"Helvetica" size:20],
                            NSKernAttributeName: @(1.5),
                            NSBaselineOffsetAttributeName: @(-3),
                            NSParagraphStyleAttributeName: para };
    GlkStyle *alert = clone.bufAlert;
    alert.autogenerated = NO;
    alert.attributeDict = @{ NSFontAttributeName: [NSFont fontWithName:@"Helvetica" size:10] };

    // A negative factor counts as positive; the editable clone is scaled in place
    [Preferences scale:-0.5];
    [self assertOneCloneSelected];
    XCTAssertEqual(Preferences.theme, clone);
    XCTAssertEqual(clone.gridNormal.font.pointSize, gridSize);
    XCTAssertEqual(clone.bufEmph, emph);
    XCTAssertEqual(emph.font.pointSize, 10);
    XCTAssertEqualWithAccuracy([emph.attributeDict[NSKernAttributeName] doubleValue], 0.75, 0.001);
    XCTAssertEqualWithAccuracy([emph.attributeDict[NSBaselineOffsetAttributeName] doubleValue], -1.5, 0.001);
    [self assertParagraphStyle:[self paragraphStyleOf:emph] is:0.5 times:para];
    XCTAssertEqual(clone.bufAlert, alert);
    XCTAssertEqual(alert.font.pointSize, 5);
    XCTAssertNil([self paragraphStyleOf:alert]);
    XCTAssertNil(alert.attributeDict[NSKernAttributeName]);
}

// Actual size is the grid font size of the root of the clone's parent chain
- (void)testZoomToActualSizeReturnsToTheRootSize {
    Preferences.instance = self.prefs;
    [self selectBuiltInTheme];
    CGFloat gridSize = self.builtInTheme.gridNormal.font.pointSize;
    [Preferences zoomIn];
    [Preferences zoomIn];
    [self assertOneCloneSelected];
    XCTAssertEqualWithAccuracy(Preferences.theme.gridNormal.font.pointSize, gridSize + 2, 0.001);
    [Preferences zoomToActualSize];
    XCTAssertEqualWithAccuracy(Preferences.theme.gridNormal.font.pointSize, gridSize, 0.001);
    [Preferences zoomOut];
    XCTAssertEqualWithAccuracy(Preferences.theme.gridNormal.font.pointSize, gridSize - 1, 0.001);
    [self assertOneCloneSelected];
}

// Every field of the paragraph popover, edited on a built-in theme: the
// first edit clones it, every edit lands in the clone's copy of the style,
// and a refresh shows what was stored
- (void)testParagraphPopOverRoundTrip {
    Preferences.instance = self.prefs;
    [self selectBuiltInTheme];
    GlkStyle *original = self.builtInTheme.bufEmph;
    NSDictionary *originalAttributes = original.attributeDict;

    ParagraphPopOver *popover = [[ParagraphPopOver alloc] initWithNibName:@"ParagraphPopOver"
                                                                   bundle:[NSBundle bundleForClass:[Preferences class]]];
    XCTAssertNotNil(popover.view);
    [popover refreshForStyle:original];

    // Text field outlet, action, value, and how the style stores it
    NSArray *fields = @[
        @[ @"lineSpacingTextField", @"changeLineSpacing:", @3, @"lineSpacing" ],
        @[ @"characterSpacingTextField", @"changeCharacterSpacing:", @1.5, NSKernAttributeName ],
        @[ @"spacingBeforeTextField", @"changeParagraphSpacingBefore:", @4, @"paragraphSpacingBefore" ],
        @[ @"spacingAfterTextField", @"changeParagraphSpacingAfter:", @5, @"paragraphSpacing" ],
        @[ @"indentLeadingTextField", @"changeLeadingIndent:", @6, @"headIndent" ],
        @[ @"indentTrailingTextField", @"changeTrailingIndent:", @7, @"tailIndent" ],
        @[ @"indentFirstLineTextField", @"changeFirstLineIndent:", @8, @"firstLineHeadIndent" ],
        @[ @"lineMultipleTextField", @"changeLineMultiple:", @1.25, @"lineHeightMultiple" ],
        @[ @"baseLineOffsetTextField", @"changeBaselineOffset:", @-2, NSBaselineOffsetAttributeName ],
        @[ @"maxLineHeightTextField", @"changeMaxLineHeight:", @30, @"maximumLineHeight" ],
        @[ @"minLineHeightTextField", @"changeMinLineHeight:", @10, @"minimumLineHeight" ],
        @[ @"hyphenationTextField", @"changeHyphenation:", @0.5, @"hyphenationFactor" ],
    ];

    double (^stored)(NSString *) = ^double(NSString *key) {
        NSDictionary *attributes = Preferences.theme.bufEmph.attributeDict;
        if ([key isEqualToString:NSKernAttributeName] || [key isEqualToString:NSBaselineOffsetAttributeName])
            return [attributes[key] doubleValue];
        double value = [[attributes[NSParagraphStyleAttributeName] valueForKey:key] doubleValue];
        // Shown positive, stored negative
        return [key isEqualToString:@"tailIndent"] ? -value : value;
    };

    for (NSArray *field in fields) {
        NSTextField *textField = [popover valueForKey:field[0]];
        XCTAssertNotNil(textField, @"%@ is not connected", field[0]);
        double value = [field[2] doubleValue];
        textField.doubleValue = value;
        [NSApp sendAction:NSSelectorFromString(field[1]) to:popover from:textField];
        XCTAssertEqualWithAccuracy(stored(field[3]), value, 0.0001, @"%@", field[1]);
        XCTAssertEqualWithAccuracy(textField.doubleValue, value, 0.0001, @"%@", field[0]);
    }

    for (NSString *button in @[ @"btnAlignmentRight", @"btnAlignmentCenter", @"btnAlignmentJustified", @"btnAlignmentLeft" ]) {
        NSButton *sender = [popover valueForKey:button];
        [NSApp sendAction:@selector(changeAlignment:) to:popover from:sender];
        NSTextAlignment alignment = [Preferences.theme.bufEmph.attributeDict[NSParagraphStyleAttributeName] alignment];
        NSTextAlignment expected = [button isEqualToString:@"btnAlignmentRight"] ? NSTextAlignmentRight :
            [button isEqualToString:@"btnAlignmentCenter"] ? NSTextAlignmentCenter :
            [button isEqualToString:@"btnAlignmentJustified"] ? NSTextAlignmentJustified : NSTextAlignmentLeft;
        XCTAssertEqual(alignment, expected, @"%@", button);
        XCTAssertEqual(sender.state, NSControlStateValueOn, @"%@", button);
    }

    [self assertOneCloneSelected];
    GlkStyle *edited = Preferences.theme.bufEmph;
    XCTAssertNotEqual(edited, original);
    XCTAssertFalse(edited.autogenerated);
    XCTAssertEqualObjects(original.attributeDict, originalAttributes, @"The built-in style changed");
    // The other style of the same index is left alone
    XCTAssertEqualObjects(Preferences.theme.gridEmph.attributeDict[NSKernAttributeName],
                          self.builtInTheme.gridEmph.attributeDict[NSKernAttributeName]);

    // A fresh popover shows what was stored
    ParagraphPopOver *fresh = [[ParagraphPopOver alloc] initWithNibName:@"ParagraphPopOver"
                                                                 bundle:[NSBundle bundleForClass:[Preferences class]]];
    XCTAssertNotNil(fresh.view);
    [fresh refreshForStyle:edited];
    for (NSArray *field in fields)
        XCTAssertEqualWithAccuracy([[fresh valueForKey:field[0]] doubleValue], [field[2] doubleValue], 0.0001,
                                   @"%@", field[0]);
    XCTAssertEqual(((NSButton *)[fresh valueForKey:@"btnAlignmentLeft"]).state, NSControlStateValueOn);
    XCTAssertEqual(((NSButton *)[fresh valueForKey:@"btnAlignmentRight"]).state, NSControlStateValueOff);
}

@end
