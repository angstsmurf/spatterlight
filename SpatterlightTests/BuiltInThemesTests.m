//
//  BuiltInThemesTests.m
//  SpatterlightTests
//
//  BuiltInThemes against an in-memory store: every built-in theme is
//  created once, is not editable, and has a font for every style.
//  Rebuilding (with or without force) must neither duplicate themes nor
//  change their values.
//
//  Set BUILTIN_THEMES_DUMP=<path> to write a canonical dump of every
//  built-in theme, for before/after comparisons when refactoring
//  BuiltInThemes.m.
//

#import "Font3LayoutManager.h"
#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "NSFont+Categories.h"

#include "glk.h"

@interface BuiltInThemesTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;

@end

@implementation BuiltInThemesTests

- (void)setUp {
    [super setUp];

    // Always an in-memory store: forceRebuild would rewrite the real
    // library's built-in themes.
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    XCTAssertNotNil(self.context);
}

- (void)tearDown {
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

#pragma mark - Helpers

+ (NSArray<NSString *> *)builtInNames {
    // Not "Old settings": that is imported from the pre-themes defaults by
    // Preferences readDefaults, not by createBuiltInThemesInContext:
    return @[ @"Default", @"Spatterlight Classic", @"Gargoyle",
              @"Lectrote", @"Lectrote Dark", @"Zoom", @"MS-DOS", @"DOSBox",
              @"Atari ST", @"Montserrat", @"Automap" ];
}

- (NSArray<Theme *> *)allThemes {
    NSFetchRequest *request = [Theme fetchRequest];
    request.sortDescriptors = @[ [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES] ];
    NSError *error = nil;
    NSArray *themes = [self.context executeFetchRequest:request error:&error];
    XCTAssertNil(error);
    return themes;
}

// A stable text form of a value: colours and fonts by their components,
// dictionaries sorted by key. Relationships are skipped.
static NSString *describeValue(id value) {
    if (!value || value == [NSNull null])
        return @"nil";
    if ([value isKindOfClass:[NSColor class]]) {
        NSColor *color = [(NSColor *)value colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if (!color)
            return [value description];
        return [NSString stringWithFormat:@"rgba(%.4f %.4f %.4f %.4f)",
                color.redComponent, color.greenComponent, color.blueComponent, color.alphaComponent];
    }
    if ([value isKindOfClass:[NSFont class]]) {
        NSFont *font = value;
        return [NSString stringWithFormat:@"font(%@ %.2f)", font.fontName, font.pointSize];
    }
    if ([value isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = value;
        NSMutableArray *parts = [NSMutableArray new];
        for (id key in [dict.allKeys sortedArrayUsingSelector:@selector(compare:)])
            [parts addObject:[NSString stringWithFormat:@"%@=%@", key, describeValue(dict[key])]];
        return [NSString stringWithFormat:@"{%@}", [parts componentsJoinedByString:@", "]];
    }
    if ([value isKindOfClass:[NSParagraphStyle class]]) {
        NSParagraphStyle *para = value;
        return [NSString stringWithFormat:@"para(align %ld line %.2f mult %.2f before %.2f after %.2f head %.2f tail %.2f first %.2f min %.2f max %.2f hyph %.2f)",
                (long)para.alignment, para.lineSpacing, para.lineHeightMultiple,
                para.paragraphSpacingBefore, para.paragraphSpacing, para.headIndent,
                para.tailIndent, para.firstLineHeadIndent, para.minimumLineHeight,
                para.maximumLineHeight, para.hyphenationFactor];
    }
    return [value description];
}

static NSString *describeObject(NSManagedObject *object) {
    NSMutableArray *lines = [NSMutableArray new];
    NSDictionary *attributes = object.entity.attributesByName;
    for (NSString *key in [attributes.allKeys sortedArrayUsingSelector:@selector(compare:)])
        [lines addObject:[NSString stringWithFormat:@"  %@ = %@", key, describeValue([object valueForKey:key])]];
    return [lines componentsJoinedByString:@"\n"];
}

- (NSString *)dumpOfBuiltInThemes {
    NSMutableString *dump = [NSMutableString new];
    for (Theme *theme in [self allThemes]) {
        if (theme.editable)
            continue;
        [dump appendFormat:@"== %@\n%@\n", theme.name, describeObject(theme)];
        for (GlkStyle *style in theme.allStyles) {
            NSUInteger index = [theme.allStyles indexOfObject:style];
            [dump appendFormat:@" -- style %lu\n%@\n", (unsigned long)index, describeObject(style)];
        }
    }
    return dump;
}

#pragma mark - Tests

- (void)testBuiltInThemesAreCreatedOnceAndNotEditable {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];

    NSArray<Theme *> *themes = [self allThemes];
    NSArray<NSString *> *names = [themes valueForKey:@"name"];
    for (NSString *name in [BuiltInThemesTests builtInNames]) {
        XCTAssertEqual([names indexesOfObjectsPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
            return [obj isEqualToString:name];
        }].count, 1, @"%@", name);
    }

    XCTAssertLessThanOrEqual([names indexesOfObjectsPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
        return [obj isEqualToString:@"Old settings"];
    }].count, 1u);

    for (Theme *theme in themes) {
        // Old settings holds the user's pre-themes preferences, so it can be
        // edited and deleted like a user theme
        if ([theme.name isEqualToString:@"Old settings"])
            XCTAssertTrue(theme.editable);
        else
            XCTAssertFalse(theme.editable, @"%@", theme.name);
        XCTAssertEqual(theme.allStyles.count, (NSUInteger)(2 * style_NUMSTYLES), @"%@", theme.name);
        for (GlkStyle *style in theme.allStyles) {
            XCTAssertNotNil(style.font, @"%@ style %lu", theme.name, (unsigned long)[theme.allStyles indexOfObject:style]);
            XCTAssertNotNil(style.attributeDict, @"%@", theme.name);
        }
        XCTAssertGreaterThan(theme.cellWidth, 0, @"%@", theme.name);
        XCTAssertGreaterThan(theme.cellHeight, 0, @"%@", theme.name);
        XCTAssertGreaterThan(theme.bufferCellWidth, 0, @"%@", theme.name);
        XCTAssertGreaterThan(theme.bufferCellHeight, 0, @"%@", theme.name);
    }
}

- (void)testRebuildDoesNotDuplicateOrChangeThemes {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    NSUInteger count = [self allThemes].count;
    NSString *first = [self dumpOfBuiltInThemes];

    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    XCTAssertEqual([self allThemes].count, count);
    XCTAssertEqualObjects([self dumpOfBuiltInThemes], first);

    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:YES];
    XCTAssertEqual([self allThemes].count, count);
    XCTAssertEqualObjects([self dumpOfBuiltInThemes], first);

    NSString *dumpPath = NSProcessInfo.processInfo.environment[@"BUILTIN_THEMES_DUMP"];
    if (dumpPath.length) {
        NSError *error = nil;
        [first writeToFile:dumpPath atomically:YES encoding:NSUTF8StringEncoding error:&error];
        XCTAssertNil(error);
    }
}

// The 0 literal in a dictionary subscript assignment removes the key;
// DOSBox (grid normal) and Montserrat (buffer header) must store an
// explicit zero like the other builders.
- (void)testBaselineOffsetZeroIsStored {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    for (Theme *theme in [self allThemes]) {
        if ([theme.name isEqualToString:@"DOSBox"])
            XCTAssertEqualObjects(theme.gridNormal.attributeDict[NSBaselineOffsetAttributeName], @(0));
        else if ([theme.name isEqualToString:@"Montserrat"])
            XCTAssertEqualObjects(theme.bufHead.attributeDict[NSBaselineOffsetAttributeName], @(0));
    }
}

// MS-DOS gets scanlines, glow and a block cursor, DOSBox and Atari ST only
// the block cursor, and the other themes none of them. A forced rebuild
// restores them (DOSBox used to have scanlines and glow); an ordinary
// launch leaves them alone.
- (void)testRetroThemesHaveTheirLook {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    for (Theme *theme in [self allThemes]) {
        if ([theme.name isEqualToString:@"MS-DOS"]) {
            theme.crtScanlines = NO;
            theme.crtGlow = NO;
            theme.caretShape = kCaretLine;
        } else if ([theme.name isEqualToString:@"DOSBox"] || [theme.name isEqualToString:@"Atari ST"]) {
            theme.crtScanlines = YES;
            theme.crtGlow = YES;
            theme.caretShape = kCaretLine;
        }
    }
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    for (Theme *theme in [self allThemes])
        if ([theme.name isEqualToString:@"DOSBox"])
            XCTAssertTrue(theme.crtScanlines);
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:YES];

    NSUInteger found = 0;
    for (Theme *theme in [self allThemes]) {
        BOOL crt = [theme.name isEqualToString:@"MS-DOS"];
        BOOL block = crt || [theme.name isEqualToString:@"DOSBox"] ||
            [theme.name isEqualToString:@"Atari ST"];
        if (block)
            found++;
        XCTAssertEqual(theme.crtScanlines, crt, @"%@", theme.name);
        XCTAssertEqual(theme.crtGlow, crt, @"%@", theme.name);
        XCTAssertEqual(theme.caretShape, block ? kCaretBlock : kCaretLine, @"%@", theme.name);
    }
    XCTAssertEqual(found, 3u);
}

// Font 3 is stored as Unicode stand-ins for VoiceOver, each marked with its
// code, and laid out as empty cells of the text font's width without the
// fallback fonts of the stand-ins changing the line height.
- (void)testFont3CellsKeepTheTextFontsMetrics {
    NSFont *font = [NSFont fontWithName:@"Menlo" size:13];
    XCTAssertEqual([Font3LayoutManager unicodeForCode:'O'], 0x2550);
    XCTAssertEqual([Font3LayoutManager unicodeForCode:'a'], 0x16AA);
    XCTAssertEqual([Font3LayoutManager unicodeForCode:200], 0);

    NSAttributedString *codes = [Font3LayoutManager
                                 attributedStringWithCodes:@"OOa"
                                 attributes:@{NSFontAttributeName: font}];
    XCTAssertEqualObjects(codes.string, @"\u2550\u2550\u16AA");
    XCTAssertEqual([Font3LayoutManager codeAtIndex:2 inText:codes], 'a');

    NSTextStorage *storage = [[NSTextStorage alloc] initWithString:@"x\n"
                                                        attributes:@{NSFontAttributeName: font}];
    [storage appendAttributedString:codes];
    Font3LayoutManager *layout = [[Font3LayoutManager alloc] init];
    NSTextContainer *container = [[NSTextContainer alloc] initWithSize:NSMakeSize(1000, 1000)];
    [layout addTextContainer:container];
    [storage addLayoutManager:layout];
    id<NSLayoutManagerDelegate> delegate = (id)self;
    layout.delegate = delegate;
    storage.delegate = (id)self;
    [storage edited:NSTextStorageEditedAttributes range:NSMakeRange(0, storage.length) changeInLength:0];
    [layout ensureLayoutForTextContainer:container];

    NSRect plain = [layout lineFragmentRectForGlyphAtIndex:0 effectiveRange:NULL];
    NSRect font3 = [layout lineFragmentRectForGlyphAtIndex:3 effectiveRange:NULL];
    XCTAssertEqual(font3.size.height, plain.size.height);
    CGFloat x1 = [layout locationForGlyphAtIndex:3].x;
    CGFloat x2 = [layout locationForGlyphAtIndex:4].x;
    XCTAssertEqualWithAccuracy(x2 - x1, [Font3LayoutManager cellWidthForFont:font], 0.001);

    // A character typed over a font 3 one inherits its attributes, but is not font 3.
    [storage replaceCharactersInRange:NSMakeRange(2, 1) withString:@"O"];
    XCTAssertEqual([Font3LayoutManager codeAtIndex:2 inText:storage], 0);
}

- (void)textStorage:(NSTextStorage *)textStorage didProcessEditing:(NSTextStorageEditActions)editedMask range:(NSRange)editedRange changeInLength:(NSInteger)delta {
    [Font3LayoutManager restoreBaseFontsInTextStorage:textStorage range:editedRange];
}

- (NSUInteger)layoutManager:(NSLayoutManager *)layoutManager shouldGenerateGlyphs:(const CGGlyph *)glyphs properties:(const NSGlyphProperty *)props characterIndexes:(const NSUInteger *)charIndexes font:(NSFont *)aFont forGlyphRange:(NSRange)glyphRange {
    return [Font3LayoutManager layoutManager:layoutManager shouldGenerateGlyphs:glyphs properties:props characterIndexes:charIndexes font:aFont forGlyphRange:glyphRange];
}

- (NSControlCharacterAction)layoutManager:(NSLayoutManager *)layoutManager shouldUseAction:(NSControlCharacterAction)action forControlCharacterAtIndex:(NSUInteger)charIndex {
    return [Font3LayoutManager layoutManager:layoutManager shouldUseAction:action forControlCharacterAtIndex:charIndex];
}

- (NSRect)layoutManager:(NSLayoutManager *)layoutManager boundingBoxForControlGlyphAtIndex:(NSUInteger)glyphIndex forTextContainer:(NSTextContainer *)textContainer proposedLineFragment:(NSRect)proposedRect glyphPosition:(NSPoint)glyphPosition characterIndex:(NSUInteger)charIndex {
    return [Font3LayoutManager layoutManager:layoutManager boundingBoxForControlGlyphAtIndex:glyphIndex forTextContainer:textContainer proposedLineFragment:proposedRect glyphPosition:glyphPosition characterIndex:charIndex];
}

// Old settings belongs to the user: a forced rebuild (every new app build,
// or Rebuild Default Themes) re-imported it from the old defaults keys,
// overwriting edits, and recreated it after it had been deleted.
- (void)testForcedRebuildLeavesOldSettingsAlone {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];

    Theme *oldSettings = [NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    oldSettings.name = @"Old settings";
    oldSettings.editable = YES;
    oldSettings.defaultCols = 33;

    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:YES];
    XCTAssertFalse(oldSettings.isDeleted);
    XCTAssertEqual(oldSettings.defaultCols, 33);

    [self.context deleteObject:oldSettings];
    [self.context processPendingChanges];
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:YES];
    XCTAssertFalse([[[self allThemes] valueForKey:@"name"] containsObject:@"Old settings"]);
}

// On a store without built-in themes the defaults-plist import ran first
// and left a blank Spatterlight Classic that an unforced build kept.
- (void)testUnforcedBuildPopulatesClassic {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    for (Theme *theme in [self allThemes]) {
        if ([theme.name isEqualToString:@"Spatterlight Classic"]) {
            XCTAssertEqual(theme.defaultCols, 80);
            XCTAssertEqual(theme.defaultRows, 50);
            XCTAssertEqualObjects(theme.gridNormal.font.fontName, @"Monaco");
            return;
        }
    }
    XCTFail(@"No Spatterlight Classic theme");
}

#pragma mark - Theme helpers

- (void)testUnusedNameCountsUpCaseInsensitively {
    XCTAssertEqualObjects([Theme unusedNameBasedOn:@"New theme" inContext:self.context], @"New theme");

    Theme *first = [NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    first.name = @"new THEME";
    XCTAssertEqualObjects([Theme unusedNameBasedOn:@"New theme" inContext:self.context], @"New theme 2");

    Theme *second = [NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    second.name = @"New theme 2";
    XCTAssertEqualObjects([Theme unusedNameBasedOn:@"New theme" inContext:self.context], @"New theme 3");

    // A name with format and wildcard characters is matched literally.
    XCTAssertEqualObjects([Theme unusedNameBasedOn:@"100% *" inContext:self.context], @"100% *");
}

- (void)testCloneIsEditableChildWithOwnStyles {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    Theme *zoom = nil;
    for (Theme *theme in [self allThemes])
        if ([theme.name isEqualToString:@"Zoom"])
            zoom = theme;
    XCTAssertNotNil(zoom);

    Theme *clone = zoom.clone;
    XCTAssertEqualObjects(clone.name, zoom.name);
    XCTAssertEqual(clone.defaultParent, zoom);
    XCTAssertTrue(clone.editable);
    XCTAssertEqual(clone.allStyles.count, zoom.allStyles.count);
    XCTAssertNotEqual(clone.bufEmph, zoom.bufEmph);
    XCTAssertEqualObjects(clone.bufEmph.font, zoom.bufEmph.font);
}

- (Theme *)insertThemeNamed:(NSString *)name parent:(Theme *)parent {
    Theme *theme = [NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    theme.name = name;
    theme.defaultParent = parent;
    return theme;
}

// The ancestor is what removing or deleting a theme falls back to
- (void)testAncestorPrefersTheNameBeforeModified {
    Theme *root = [self insertThemeNamed:@"Root" parent:nil];
    Theme *base = [self insertThemeNamed:@"Base" parent:root];
    Theme *other = [self insertThemeNamed:@"Other" parent:nil];

    // A clone of a clone: the name wins over the root of the parent chain
    XCTAssertEqual([self insertThemeNamed:@"Base (modified)" parent:base].ancestor, base);
    XCTAssertEqual([self insertThemeNamed:@"Base (modified) 2" parent:base].ancestor, base);
    // Even when the parent chain leads elsewhere
    XCTAssertEqual([self insertThemeNamed:@"Base (modified) 3" parent:other].ancestor, base);
    // Matched case-insensitively, as unusedNameBasedOn: counts names
    XCTAssertEqual([self insertThemeNamed:@"BASE (modified)" parent:nil].ancestor, base);
}

- (void)testAncestorFallsBackToTheRootOfTheParentChain {
    Theme *root = [self insertThemeNamed:@"Root" parent:nil];
    Theme *middle = [self insertThemeNamed:@"Middle" parent:root];
    Theme *renamed = [self insertThemeNamed:@"Renamed clone" parent:middle];
    XCTAssertEqual(renamed.ancestor, root);
    XCTAssertEqual(middle.ancestor, root);
    // No theme by the name before " (modified)"
    XCTAssertEqual([self insertThemeNamed:@"Gone (modified)" parent:middle].ancestor, root);
    // Too short a name before " (modified)" to count
    XCTAssertEqual([self insertThemeNamed:@"R (modified)" parent:middle].ancestor, root);
    XCTAssertNil(root.ancestor);
    XCTAssertNil([self insertThemeNamed:@"Gone (modified)" parent:nil].ancestor);
}

// What cloneThemeIfNotEditable names a clone of a built-in theme
- (void)testCloneOfBuiltInThemeNamesCountUp {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    Theme *builtIn = nil;
    for (Theme *theme in [self allThemes])
        if ([theme.name isEqualToString:@"Default"])
            builtIn = theme;
    XCTAssertNotNil(builtIn);

    NSString *modified = [builtIn.name stringByAppendingString:@" (modified)"];
    for (NSString *expected in @[ @"Default (modified)", @"Default (modified) 2", @"Default (modified) 3" ]) {
        Theme *clone = builtIn.clone;
        clone.name = [Theme unusedNameBasedOn:modified inContext:self.context];
        XCTAssertEqualObjects(clone.name, expected);
        XCTAssertEqual(clone.ancestor, builtIn);
    }
}

// populateStyles regenerates the autogenerated styles from the Normal ones
// and leaves styles the user has set alone.
- (void)testPopulateStylesKeepsUserStyles {
    Theme *theme = [NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    theme.name = @"Populate";
    [theme populateStyles];
    XCTAssertEqual(theme.allStyles.count, (NSUInteger)style_NUMSTYLES * 2);

    NSFontManager *mgr = [NSFontManager sharedFontManager];
    XCTAssertTrue([mgr traitsOfFont:theme.bufEmph.font] & NSItalicFontMask);
    XCTAssertEqualObjects(theme.gridInput.font, [mgr convertWeight:YES ofFont:theme.gridNormal.font]);
    XCTAssertTrue([mgr traitsOfFont:theme.bufPre.font] & NSFixedPitchFontMask);
    XCTAssertEqualObjects(theme.bufNote.font, theme.bufferNormal.font);
    XCTAssertEqual(theme.bufHead.index, (NSInteger)style_Header);

    GlkStyle *userEmph = theme.bufEmph;
    userEmph.autogenerated = NO;
    NSFont *userFont = [NSFont fontWithName:@"Courier" size:20];
    userEmph.font = userFont;
    GlkStyle *oldAlert = theme.bufAlert;

    theme.bufferNormal.font = [NSFont fontWithName:@"Helvetica" size:20];
    [theme populateStyles];

    XCTAssertEqual(theme.bufEmph, userEmph);
    XCTAssertEqualObjects(theme.bufEmph.font, userFont);
    XCTAssertNotEqual(theme.bufAlert, oldAlert);
    XCTAssertTrue(oldAlert.isDeleted);
    XCTAssertEqual(theme.bufAlert.font.pointSize, 20);
}

@end

