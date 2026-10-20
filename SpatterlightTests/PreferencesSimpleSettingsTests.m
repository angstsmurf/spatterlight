//
//  PreferencesSimpleSettingsTests.m
//  SpatterlightTests
//
//  The table of Preferences controls that store one value under one key:
//  every entry is wired to changeSimpleSetting:, and every value it can take
//  survives the trip from the store to the control and back, against an
//  editable theme in an in-memory store.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "Theme.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "PreferencesPrivate.h"
#import "Constants.h"

@interface PreferencesSimpleSettingsTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *editableTheme;
@property (nonatomic, strong) Theme *savedTheme;
@property (nonatomic, strong) Preferences *prefs;
@property (nonatomic, strong) NSArray *topLevelObjects;
@property (nonatomic, strong) NSMutableDictionary *savedDefaults;

@end

@implementation PreferencesSimpleSettingsTests

- (void)setUp {
    [super setUp];
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];

    NSFetchRequest *request = [Theme fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"name == %@", @"Default"];
    self.editableTheme = [self.context executeFetchRequest:request error:nil].firstObject;
    XCTAssertNotNil(self.editableTheme);
    // An editable theme is changed in place, so changeSimpleSetting: never clones
    self.editableTheme.editable = YES;

    Preferences *prefs = [[Preferences alloc] init];
    NSNib *nib = [[NSNib alloc] initWithNibNamed:@"PrefsWindow"
                                          bundle:[NSBundle bundleForClass:[Preferences class]]];
    NSArray *topLevelObjects = nil;
    XCTAssertTrue([nib instantiateWithOwner:prefs topLevelObjects:&topLevelObjects]);
    self.topLevelObjects = topLevelObjects;
    self.prefs = prefs;

    self.savedTheme = Preferences.theme;
    Preferences.theme = self.editableTheme;

    [prefs setUpSimpleSettings];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    self.savedDefaults = [NSMutableDictionary new];
    for (NSString *key in Preferences.simpleSettingKeys.allValues)
        if (![self isThemeKey:key])
            self.savedDefaults[key] = [defaults objectForKey:key] ?: [NSNull null];
}

- (void)tearDown {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [self.savedDefaults enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
        if (value == [NSNull null])
            [defaults removeObjectForKey:key];
        else
            [defaults setObject:value forKey:key];
    }];
    Preferences.theme = self.savedTheme;
    self.prefs = nil;
    self.topLevelObjects = nil;
    self.editableTheme = nil;
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

- (BOOL)isThemeKey:(NSString *)key {
    return self.editableTheme.entity.attributesByName[key] != nil;
}

- (id)storedValueForKey:(NSString *)key {
    if ([self isThemeKey:key])
        return [self.editableTheme valueForKey:key];
    return [[NSUserDefaults standardUserDefaults] objectForKey:key];
}

- (void)storeValue:(NSInteger)value forKey:(NSString *)key {
    if ([self isThemeKey:key])
        [self.editableTheme setValue:@(value) forKey:key];
    else
        [[NSUserDefaults standardUserDefaults] setObject:@(value) forKey:key];
}

// The values the control can show: check box states, or the pop-up tags
- (NSArray<NSNumber *> *)valuesOfControl:(NSControl *)control {
    if (![control isKindOfClass:[NSPopUpButton class]])
        return @[ @NO, @YES ];
    NSMutableOrderedSet *tags = [NSMutableOrderedSet new];
    for (NSMenuItem *item in ((NSPopUpButton *)control).itemArray)
        if (!item.separatorItem)
            [tags addObject:@(item.tag)];
    return tags.array;
}

- (NSInteger)valueShownByControl:(NSControl *)control {
    if ([control isKindOfClass:[NSPopUpButton class]])
        return ((NSPopUpButton *)control).selectedTag;
    return ((NSButton *)control).state == NSControlStateValueOn;
}

- (void)setControl:(NSControl *)control toValue:(NSInteger)value {
    if ([control isKindOfClass:[NSPopUpButton class]])
        [(NSPopUpButton *)control selectItemWithTag:value];
    else
        ((NSButton *)control).state = value ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)testEveryEntryIsWiredToTheGenericAction {
    NSDictionary<NSString *, NSString *> *keys = Preferences.simpleSettingKeys;
    XCTAssertGreaterThan(keys.count, 0u);
    // A copy-pasted row would store two controls under one key
    XCTAssertEqual([NSSet setWithArray:keys.allValues].count, keys.count, @"Two controls store the same key");
    [keys enumerateKeysAndObjectsUsingBlock:^(NSString *outlet, NSString *key, BOOL *stop) {
        NSControl *control = [self.prefs valueForKey:outlet];
        XCTAssertNotNil(control, @"%@ is not connected", outlet);
        XCTAssertTrue([control isKindOfClass:[NSButton class]], @"%@ is a %@", outlet, control.class);
        XCTAssertEqual(control.target, self.prefs, @"%@", outlet);
        XCTAssertEqual(control.action, @selector(changeSimpleSetting:), @"%@", outlet);
        if (![control isKindOfClass:[NSPopUpButton class]])
            XCTAssertFalse(((NSButton *)control).allowsMixedState, @"%@", outlet);
        // A key that is not a theme attribute is a defaults key, which by
        // convention starts with an upper case letter
        if (![self isThemeKey:key])
            XCTAssertTrue([[NSCharacterSet uppercaseLetterCharacterSet] characterIsMember:[key characterAtIndex:0]],
                          @"%@: %@ is neither a theme attribute nor a defaults key", outlet, key);
    }];
}

- (void)testEveryValueRoundTrips {
    [Preferences.simpleSettingKeys enumerateKeysAndObjectsUsingBlock:^(NSString *outlet, NSString *key, BOOL *stop) {
        NSControl *control = [self.prefs valueForKey:outlet];
        NSArray<NSNumber *> *values = [self valuesOfControl:control];
        XCTAssertGreaterThan(values.count, 1u, @"%@ has only one value", outlet);

        for (NSUInteger i = 0; i < values.count; i++) {
            NSInteger value = values[i].integerValue;
            NSInteger other = values[(i + 1) % values.count].integerValue;

            // Store to control
            [self storeValue:value forKey:key];
            [self.prefs updateSimpleSettings];
            XCTAssertEqual([self valueShownByControl:control], value, @"%@ does not show %@ = %ld", outlet, key, value);

            // Control to store
            [self setControl:control toValue:other];
            [self.prefs changeSimpleSetting:control];
            XCTAssertEqual([[self storedValueForKey:key] integerValue], other, @"%@ did not store %@ = %ld", outlet, key, other);
            XCTAssertEqual(Preferences.theme, self.editableTheme, @"%@ replaced the theme", outlet);
        }
    }];
}

// The table needs compile-time constants, so it spells out the keys that the
// rest of the app reads through the Constants.h names
- (void)testSharedDefaultsKeysMatchTheConstants {
    NSDictionary<NSString *, NSString *> *keys = Preferences.simpleSettingKeys;
    XCTAssertEqualObjects(keys[@"imageReplacePopup"], kImageReplacementKey);
    XCTAssertEqualObjects(keys[@"saveInGameDirCheckbox"], kSaveInGameDirectoryKey);
    XCTAssertEqualObjects(keys[@"btnShowBezels"], kShowBezelsKey);
    XCTAssertEqualObjects(keys[@"libraryAtStartCheckbox"], kShowLibraryKey);
    XCTAssertEqualObjects(keys[@"addToLibraryCheckbox"], kAddToLibraryKey);
}

// A sender that is not in the table changes nothing
- (void)testUnknownSenderIsIgnored {
    NSArray *attributes = self.editableTheme.entity.attributesByName.allKeys;
    NSDictionary *before = [self.editableTheme dictionaryWithValuesForKeys:attributes];
    for (NSInteger state = 0; state < 2; state++) {
        NSButton *stranger = [NSButton checkboxWithTitle:@"stranger" target:nil action:nil];
        stranger.state = state ? NSControlStateValueOn : NSControlStateValueOff;
        [self.prefs changeSimpleSetting:stranger];
    }
    XCTAssertEqualObjects([self.editableTheme dictionaryWithValuesForKeys:attributes], before);
    XCTAssertEqual(Preferences.theme, self.editableTheme);
}

@end
