//
//  Font3ThemeChangeTests.m
//  SpatterlightTests
//
//  Font 3 (Beyond Zork's map, Journey's borders) has to survive a change of
//  theme, both in what is already on screen and in what is printed after.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "Font3LayoutManager.h"
#import "Theme.h"
#import "Game.h"
#import "BuiltInThemes.h"

#include "glk.h"
#include "protocol.h"

@interface BuiltInThemes (Font3ThemeChangeTests)
+ (Theme *)createClassicSpatterlightThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createAtariSTThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createDOSBoxThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createZoomThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
@end

static const int kStory = 0;
static const int kGrid = 1;

@interface Font3ThemeChangeTests : GlkControllerTestCase
@end

@implementation Font3ThemeChangeTests

- (NSTextView *)textViewOf:(GlkWindow *)win {
    if ([win isKindOfClass:[GlkTextGridWindow class]])
        return ((GlkTextGridWindow *)win).textview;
    return ((GlkTextBufferWindow *)win).textview;
}

// Every character of the codes is shown as font 3 somewhere in the window.
- (void)assertFont3:(NSString *)codes in:(GlkWindow *)win label:(NSString *)label {
    NSTextStorage *ts = [self textViewOf:win].textStorage;
    NSString *string = ts.string;
    for (NSUInteger i = 0; i < codes.length; i++) {
        unichar code = [codes characterAtIndex:i];
        unichar unicode = [Font3LayoutManager unicodeForCode:code];
        NSRange r = [string rangeOfString:[NSString stringWithCharacters:&unicode length:1]];
        XCTAssertNotEqual(r.location, NSNotFound, @"%@: no stand-in for '%C' in \"%@\"", label, code, string);
        if (r.location != NSNotFound) {
            XCTAssertEqual([Font3LayoutManager codeAtIndex:r.location inText:ts], code, @"%@", label);
            // Laid out as a cell that Font3LayoutManager paints, not drawn
            // by the font.
            NSLayoutManager *lm = [self textViewOf:win].layoutManager;
            [lm ensureLayoutForCharacterRange:NSMakeRange(0, ts.length)];
            NSUInteger glyph = [lm glyphIndexForCharacterAtIndex:r.location];
            XCTAssertTrue([lm propertyForGlyphAtIndex:glyph] & NSGlyphPropertyControlCharacter,
                          @"%@: '%C' is drawn by the font", label, code);
        }
    }
    XCTAssertTrue([[self textViewOf:win].layoutManager isKindOfClass:[Font3LayoutManager class]], @"%@", label);
}

- (void)checkThemeChangeFrom:(Theme *)from to:(Theme *)to {
    NSString *label = [NSString stringWithFormat:@"%@ -> %@", from.name, to.name];
    self.game.theme = from;
    GlkController *ctl = [self makeControllerWithTheme:from];
    ctl.usesFont3 = YES;
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kGrid a3:0 text:nil], kGrid);
    ctl.gwindows[@(kGrid)].frame = NSMakeRect(0, 0, 600, 100);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 100, 600, 300);

    [self send:MOVETO to:ctl a1:kGrid a2:0 a3:0 text:nil];
    [self send:PRINT to:ctl a1:kGrid a2:style_BlockQuote a3:0 text:@"OO"];
    [self send:PRINT to:ctl a1:kStory a2:style_BlockQuote a3:0 text:@"OO"];
    [self nextEventIn:ctl];
    [self assertFont3:@"O" in:ctl.gwindows[@(kGrid)] label:[label stringByAppendingString:@" grid before"]];
    [self assertFont3:@"O" in:ctl.gwindows[@(kStory)] label:[label stringByAppendingString:@" buffer before"]];

    self.game.theme = to;
    [ctl notePreferencesChanged:[NSNotification notificationWithName:@"PreferencesChanged" object:to]];
    XCTAssertEqual(ctl.gwindows[@(kGrid)].theme, to);
    // The interpreter answers the arrange event by resizing its windows.
    ctl.gwindows[@(kGrid)].frame = NSMakeRect(0, 0, 640, 110);
    [self nextEventIn:ctl];
    [self assertFont3:@"O" in:ctl.gwindows[@(kGrid)] label:[label stringByAppendingString:@" grid after"]];
    [self assertFont3:@"O" in:ctl.gwindows[@(kStory)] label:[label stringByAppendingString:@" buffer after"]];

    [self send:MOVETO to:ctl a1:kGrid a2:0 a3:1 text:nil];
    [self send:PRINT to:ctl a1:kGrid a2:style_BlockQuote a3:0 text:@"a"];
    [self send:PRINT to:ctl a1:kStory a2:style_BlockQuote a3:0 text:@"a"];
    [self nextEventIn:ctl];
    [self assertFont3:@"a" in:ctl.gwindows[@(kGrid)] label:[label stringByAppendingString:@" grid printed after"]];
    [self assertFont3:@"a" in:ctl.gwindows[@(kStory)] label:[label stringByAppendingString:@" buffer printed after"]];
}

- (void)testFont3SurvivesThemeChange {
    [self makeGameWithIfid:@"ZCODE-57-871221" theme:self.theme];
    NSManagedObjectContext *c = self.context;
    NSArray<Theme *> *themes = @[self.theme,
                                 [BuiltInThemes createClassicSpatterlightThemeInContext:c forceRebuild:YES],
                                 [BuiltInThemes createAtariSTThemeInContext:c forceRebuild:YES],
                                 [BuiltInThemes createDOSBoxThemeInContext:c forceRebuild:YES],
                                 [BuiltInThemes createZoomThemeInContext:c forceRebuild:YES]];
    for (Theme *from in themes)
        for (Theme *to in themes)
            if (from != to)
                [self checkThemeChangeFrom:from to:to];
}

// DIAGNOSTIC: replays a v6trace of Beyond Zork.
// Turns the font 3 text of a window back into what an autosave from before
// font 3 was painted holds: the raw codes, with no code attribute.
- (void)makeArchivedFont3In:(NSMutableAttributedString *)text {
    NSRange all = NSMakeRange(0, text.length);
    [text enumerateAttribute:Font3CodeAttributeName inRange:all options:NSAttributedStringEnumerationReverse
                  usingBlock:^(NSNumber *code, NSRange range, BOOL *stop) {
        if (!code)
            return;
        unichar c = code.unsignedShortValue;
        NSString *codes = [@"" stringByPaddingToLength:range.length
                                            withString:[NSString stringWithCharacters:&c length:1]
                                       startingAtIndex:0];
        [text replaceCharactersInRange:range withString:codes];
    }];
    [text removeAttribute:Font3CodeAttributeName range:all];
    [text removeAttribute:Font3BaseFontAttributeName range:all];
}

// The Beyond Zork map of an autorestored game from an earlier version came
// back as letters when the theme changed.
- (void)testArchivedFont3SurvivesThemeChange {
    [self makeGameWithIfid:@"ZCODE-57-871221" theme:self.theme];
    Theme *to = [BuiltInThemes createAtariSTThemeInContext:self.context forceRebuild:YES];
    GlkController *ctl = [self makeControllerWithTheme:self.theme];
    ctl.usesFont3 = YES;
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kGrid a3:0 text:nil], kGrid);
    ctl.gwindows[@(kGrid)].frame = NSMakeRect(0, 0, 600, 100);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 100, 600, 300);
    [self send:MOVETO to:ctl a1:kGrid a2:0 a3:0 text:nil];
    [self send:PRINT to:ctl a1:kGrid a2:style_BlockQuote a3:0 text:@"D8A"];
    [self send:PRINT to:ctl a1:kStory a2:style_BlockQuote a3:0 text:@"D8A"];
    [self nextEventIn:ctl];

    GlkTextGridWindow *grid = (GlkTextGridWindow *)ctl.gwindows[@(kGrid)];
    GlkTextBufferWindow *buffer = (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
    [self makeArchivedFont3In:[self textViewOf:grid].textStorage];
    [self makeArchivedFont3In:grid.bufferTextStorage];
    [self makeArchivedFont3In:[self textViewOf:buffer].textStorage];
    XCTAssertTrue([[self textViewOf:grid].string containsString:@"D8A"]);

    self.game.theme = to;
    [ctl notePreferencesChanged:[NSNotification notificationWithName:@"PreferencesChanged" object:to]];
    [self nextEventIn:ctl];
    for (GlkWindow *win in @[grid, buffer]) {
        NSString *label = NSStringFromClass([win class]);
        XCTAssertFalse([[self textViewOf:win].string containsString:@"D8A"], @"%@: \"%@\"", label, [self textViewOf:win].string);
        [self assertFont3:@"D8A" in:win label:label];
    }
}

@end
