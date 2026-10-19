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

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"

#include "glk.h"

@interface BuiltInThemesTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;

@end

@implementation BuiltInThemesTests

- (void)setUp {
    [super setUp];

    // Always an in-memory store: forceRebuild would rewrite the real
    // library's built-in themes.
    NSURL *modelURL = [[NSBundle bundleForClass:[CoreDataManager class]] URLForResource:@"Spatterlight" withExtension:@"momd"];
    NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    NSError *error = nil;
    [coordinator addPersistentStoreWithType:NSInMemoryStoreType
                              configuration:nil
                                        URL:nil
                                    options:nil
                                      error:&error];
    XCTAssertNil(error);
    self.context = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    self.context.persistentStoreCoordinator = coordinator;
}

- (void)tearDown {
    self.context = nil;
    [super tearDown];
}

#pragma mark - Helpers

+ (NSArray<NSString *> *)builtInNames {
    // Not "Old settings": that is imported from the pre-themes defaults by
    // Preferences readDefaults, not by createBuiltInThemesInContext:
    return @[ @"Default", @"Spatterlight Classic", @"Gargoyle",
              @"Lectrote", @"Lectrote Dark", @"Zoom", @"MS-DOS", @"DOSBox",
              @"Montserrat", @"Automap" ];
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

@end
