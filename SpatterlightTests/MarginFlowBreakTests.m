//
//  MarginFlowBreakTests.m
//  SpatterlightTests
//
//  Pins how MarginContainer lays text out around margin images and flow
//  breaks (glk_window_flow_break), and that the layout holds up when the
//  window is resized.
//
//  A flow break is an invisible mark in the text. If the line it is on would
//  sit beside a margin image, the line is moved down below every margin image
//  anchored before the break. Otherwise it does nothing, and it may switch
//  between the two as the text rewraps.
//
//  The tests build a bare text stack around a MarginContainer, the way
//  GlkTextBufferWindow does, and read back the line fragments. No window,
//  controller or interpreter is involved.
//

#import <XCTest/XCTest.h>

#import "MarginContainer.h"
#import "MarginImage.h"

#include "glk.h"

// A text stack with a MarginContainer, and the positions of its flow breaks,
// which the container keeps to itself.
@interface FlowBreakRig : NSObject

@property (nonatomic, strong) NSTextStorage *storage;
@property (nonatomic, strong) NSLayoutManager *layout;
@property (nonatomic, strong) MarginContainer *container;
@property (nonatomic, strong) NSTextView *view;
@property (nonatomic, strong) NSMutableArray<NSNumber *> *breaks;

@end

@implementation FlowBreakRig

- (instancetype)initWithWidth:(CGFloat)width {
    self = [super init];
    if (self) {
        _storage = [[NSTextStorage alloc] init];
        _layout = [[NSLayoutManager alloc] init];
        _layout.backgroundLayoutEnabled = NO;
        _layout.allowsNonContiguousLayout = NO;
        [_storage addLayoutManager:_layout];

        _container = [[MarginContainer alloc] initWithContainerSize:NSMakeSize(0, 10000000)];
        [_layout addTextContainer:_container];

        _view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                    textContainer:_container];
        _view.minSize = NSMakeSize(1, 10000000);
        _view.maxSize = NSMakeSize(10000000, 10000000);
        _container.widthTracksTextView = YES;
        _container.heightTracksTextView = NO;
        _view.textContainerInset = NSMakeSize(10, 10);
        [_view setFrameSize:NSMakeSize(width, 10000000)];

        _breaks = [NSMutableArray array];
    }
    return self;
}

- (void)print:(NSString *)string {
    NSDictionary *attributes = @{ NSFontAttributeName : [NSFont userFixedPitchFontOfSize:12] };
    [_storage appendAttributedString:[[NSAttributedString alloc] initWithString:string
                                                                     attributes:attributes]];
}

- (void)printMarker {
    unichar uc = NSAttachmentCharacter;
    [self print:[NSString stringWithCharacters:&uc length:1]];
}

// A margin image is anchored on an attachment character at the start of a
// line. Returns the image.
- (MarginImage *)marginImageOfSize:(NSSize)size alignment:(NSInteger)alignment {
    [_container addImage:[[NSImage alloc] initWithSize:size]
               alignment:alignment
                      at:_storage.length
                  linkid:0];
    [self printMarker];
    return _container.marginImages.lastObject;
}

// A flow break is anchored on an attachment character of its own, wherever
// the game asked for it.
- (void)flowBreak {
    [self printMarker];
    [_container flowBreakAt:_storage.length - 1];
    [_breaks addObject:@(_storage.length - 1)];
}

// What GlkTextBufferWindow does when its frame changes.
- (void)resizeToWidth:(CGFloat)width {
    [_view setFrameSize:NSMakeSize(width, 10000000)];
    [_container invalidateLayout:nil];
}

- (void)layOut {
    [_layout ensureLayoutForTextContainer:_container];
}

// The line fragment that holds the character at index.
- (NSRect)lineAt:(NSUInteger)index {
    [self layOut];
    return [_layout lineFragmentRectForGlyphAtIndex:[_layout glyphIndexForCharacterAtIndex:index]
                                     effectiveRange:NULL];
}

// Every line fragment and every margin image.
- (NSString *)signature {
    [self layOut];
    NSMutableString *result = [NSMutableString string];
    NSUInteger count = _layout.numberOfGlyphs, glyph = 0;
    while (glyph < count) {
        NSRange range;
        NSRect line = [_layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:&range];
        [result appendFormat:@"%lu %@\n", [_layout characterIndexForGlyphAtIndex:glyph],
         NSStringFromRect(line)];
        glyph = NSMaxRange(range);
    }
    for (MarginImage *image in _container.marginImages)
        [result appendFormat:@"image %lu %@\n", image.pos, NSStringFromRect(image.bounds)];
    return result;
}

// Describes the first way the layout is wrong, or returns nil if it is fine:
// lines must follow each other downward, and the line of every flow break
// must start below the margin images anchored before the break.
- (nullable NSString *)firstFault {
    [self layOut];

    CGFloat bottom = 0;
    NSUInteger count = _layout.numberOfGlyphs, glyph = 0;
    while (glyph < count) {
        NSRange range;
        NSRect line = [_layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:&range];
        if (NSMinY(line) < bottom - 0.5)
            return [NSString stringWithFormat:@"the line at character %lu starts at y %.1f, above the end of the line before it at %.1f",
                    [_layout characterIndexForGlyphAtIndex:glyph], NSMinY(line), bottom];
        bottom = NSMaxY(line);
        glyph = NSMaxRange(range);
    }

    for (NSNumber *number in _breaks) {
        NSUInteger pos = number.unsignedIntegerValue;
        NSRect line = [self lineAt:pos];
        for (MarginImage *image in _container.marginImages) {
            // MarginContainer ignores a flow break more than 1000 characters
            // past an image.
            if (image.pos >= pos || pos - image.pos > 1000)
                continue;
            if (NSMaxY(image.bounds) > NSMinY(line) + 0.5)
                return [NSString stringWithFormat:@"the line of the flow break at character %lu starts at y %.1f, but the margin image at character %lu reaches down to %.1f",
                        pos, NSMinY(line), image.pos, NSMaxY(image.bounds)];
        }
    }
    return nil;
}

@end

#pragma mark -

@interface MarginFlowBreakTests : XCTestCase
@end

@implementation MarginFlowBreakTests {
    uint64_t _random;
}

- (uint32_t)random:(uint32_t)limit {
    _random = _random * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)((_random >> 33) % limit);
}

- (NSString *)words:(uint32_t)count {
    NSArray<NSString *> *words = @[@"the", @"lantern", @"flickers", @"over", @"a", @"mossy",
                                   @"stone", @"corridor", @"and", @"you", @"hear", @"distant",
                                   @"water", @"dripping", @"somewhere", @"beyond", @"in"];
    NSMutableArray<NSString *> *picked = [NSMutableArray arrayWithCapacity:count];
    for (uint32_t i = 0; i < count; i++)
        [picked addObject:words[[self random:(uint32_t)words.count]]];
    return [picked componentsJoinedByString:@" "];
}

- (NSInteger)randomMargin {
    return [self random:2] ? imagealign_MarginLeft : imagealign_MarginRight;
}

// A scrollback of margin images of all sizes, in both margins and sometimes
// stacked, with text between them and flow breaks in every position a game
// might put one. The same seed always gives the same buffer.
- (FlowBreakRig *)rigWithSeed:(uint64_t)seed width:(CGFloat)width sections:(int)sections {
    _random = seed;
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:width];

    [rig print:[[self words:40] stringByAppendingString:@"\n"]];
    for (int i = 0; i < sections; i++) {
        uint32_t kind = [self random:6];
        NSInteger margin = [self randomMargin];
        [rig marginImageOfSize:NSMakeSize(40 + [self random:120], 40 + [self random:160])
                     alignment:margin];
        if (kind == 1) {
            // A second image a line below the first.
            [rig print:[[self words:3 + [self random:10]] stringByAppendingString:@"\n"]];
            margin = [self randomMargin];
            [rig marginImageOfSize:NSMakeSize(40 + [self random:120], 40 + [self random:160])
                         alignment:margin];
        }
        [rig print:[self words:2 + [self random:25]]];
        switch (kind) {
            case 0:
            case 1: // The break ends the line.
                [rig flowBreak];
                [rig print:@"\n"];
                break;
            case 2: // The break starts the next line.
                [rig print:@"\n"];
                [rig flowBreak];
                break;
            case 3: // The break is alone on a line.
                [rig print:@"\n"];
                [rig flowBreak];
                [rig print:@"\n"];
                break;
            case 4: // The break is in the middle of a line.
                [rig flowBreak];
                [rig print:[[self words:5] stringByAppendingString:@"\n"]];
                break;
            default: // No break.
                [rig print:@"\n"];
                break;
        }
        [rig print:[[self words:20 + [self random:120]] stringByAppendingString:@"\n"]];
        if ([self random:3] == 0)
            [rig print:[[self words:60 + [self random:200]] stringByAppendingString:@"\n"]];
    }
    return rig;
}

#pragma mark - A single image

- (void)testTextFlowsBesideAMarginImage {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    [rig print:@"Intro.\n"];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 100) alignment:imagealign_MarginLeft];
    NSUInteger caption = rig.storage.length;
    [rig print:@"A caption.\n"];
    NSUInteger after = rig.storage.length;
    [rig print:@"More text.\n"];

    XCTAssertEqualWithAccuracy(NSMinX([rig lineAt:caption]), NSMaxX(image.bounds), 0.5);
    XCTAssertEqualWithAccuracy(NSMinX([rig lineAt:after]), NSMaxX(image.bounds), 0.5);
    XCTAssertLessThan(NSMinY([rig lineAt:after]), NSMaxY(image.bounds));
}

- (void)testFlowBreakAtStartOfLineMovesItBelowTheImage {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    [rig print:@"Intro.\n"];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 100) alignment:imagealign_MarginLeft];
    NSUInteger caption = rig.storage.length;
    [rig print:@"A caption.\n"];
    [rig flowBreak];
    NSUInteger after = rig.storage.length;
    [rig print:@"After the break.\n"];

    // The caption stays beside the image.
    XCTAssertEqualWithAccuracy(NSMinX([rig lineAt:caption]), NSMaxX(image.bounds), 0.5);
    XCTAssertEqualWithAccuracy(NSMinY([rig lineAt:caption]), NSMinY(image.bounds), 0.5);

    NSRect line = [rig lineAt:after];
    XCTAssertGreaterThanOrEqual(NSMinY(line), NSMaxY(image.bounds));
    XCTAssertEqualWithAccuracy(NSMinX(line), 0, 0.5, @"The line gets the full width back");
}

// A flow break does not have to be at the start of a line. The speed-up that
// made MarginContainer skip flow breaks anchored past the start of the line
// being laid out once made it ignore these altogether.
- (void)testFlowBreakAfterTextMovesItsLineBelowTheImage {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    [rig print:@"Intro.\n"];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 100) alignment:imagealign_MarginLeft];
    [rig print:@"A caption."];
    [rig flowBreak];
    [rig print:@"\n"];
    NSUInteger after = rig.storage.length;
    [rig print:@"After the break.\n"];

    XCTAssertNil([rig firstFault]);
    NSRect line = [rig lineAt:after];
    XCTAssertGreaterThanOrEqual(NSMinY(line), NSMaxY(image.bounds));
    XCTAssertEqualWithAccuracy(NSMinX(line), 0, 0.5);
}

- (void)testFlowBreakInTheMiddleOfALine {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    [rig print:@"Intro.\n"];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 100) alignment:imagealign_MarginRight];
    [rig print:@"Before."];
    [rig flowBreak];
    NSUInteger after = rig.storage.length;
    [rig print:@" After the break.\n"];

    XCTAssertNil([rig firstFault]);
    XCTAssertGreaterThanOrEqual(NSMinY([rig lineAt:after]), NSMaxY(image.bounds));
}

- (void)testFlowBreakBelowTheImageDoesNothing {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 20) alignment:imagealign_MarginLeft];
    [rig print:@"One.\nTwo.\nThree.\nFour.\n"];
    NSUInteger before = rig.storage.length - 1;
    [rig flowBreak];
    NSUInteger after = rig.storage.length;
    [rig print:@"After the break.\n"];

    XCTAssertGreaterThan(NSMinY([rig lineAt:before]), NSMaxY(image.bounds));
    XCTAssertEqualWithAccuracy(NSMinY([rig lineAt:after]), NSMaxY([rig lineAt:before]), 0.5,
                               @"No gap opens up at a flow break that is clear of the image");
}

- (void)testFlowBreakGoesBelowImagesInBothMargins {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:500];
    MarginImage *left = [rig marginImageOfSize:NSMakeSize(80, 60) alignment:imagealign_MarginLeft];
    [rig print:@"Left.\n"];
    MarginImage *right = [rig marginImageOfSize:NSMakeSize(80, 150) alignment:imagealign_MarginRight];
    [rig print:@"Right.\n"];
    [rig flowBreak];
    NSUInteger after = rig.storage.length;
    [rig print:@"After the break.\n"];

    NSRect line = [rig lineAt:after];
    XCTAssertGreaterThanOrEqual(NSMinY(line), NSMaxY(left.bounds));
    XCTAssertGreaterThanOrEqual(NSMinY(line), NSMaxY(right.bounds));
}

// The break is clear of the image while the caption fits on one line, and
// has to start working when a narrower window wraps the caption further down
// beside the image.
- (void)testFlowBreakBecomesActiveAndInactiveAsTheTextRewraps {
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:900];
    MarginImage *image = [rig marginImageOfSize:NSMakeSize(100, 40) alignment:imagealign_MarginLeft];
    [rig print:@"One.\nTwo.\nThree.\n"];
    [rig print:[[self words:30] stringByAppendingString:@"\n"]];
    NSUInteger before = rig.storage.length - 1;
    [rig flowBreak];
    NSUInteger after = rig.storage.length;
    [rig print:@"After the break.\n"];

    XCTAssertEqualWithAccuracy(NSMinY([rig lineAt:after]), NSMaxY([rig lineAt:before]), 0.5);

    [rig resizeToWidth:160];
    XCTAssertNil([rig firstFault]);
    XCTAssertGreaterThanOrEqual(NSMinY([rig lineAt:after]), NSMaxY(image.bounds));

    [rig resizeToWidth:900];
    XCTAssertNil([rig firstFault]);
    XCTAssertEqualWithAccuracy(NSMinY([rig lineAt:after]), NSMaxY([rig lineAt:before]), 0.5);
}

#pragma mark - Resizing

// Drags a window full of margin images narrower and then wider again. After
// every step each flow break has to hold, and the layout has to be the one a
// window opened at that width would get.
//
// This is the test that fails if MarginContainer stops measuring the flow
// breaks before it places a line: lines then land beside images they should
// be below, or on top of the lines before them.
- (void)testFlowBreaksHoldWhileResizing {
    for (uint64_t seed = 1; seed <= 8; seed++) {
        int sections = 4 + (int)(seed % 12);
        FlowBreakRig *rig = [self rigWithSeed:seed width:700 sections:sections];
        XCTAssertNil([rig firstFault], @"seed %llu at the opening width", seed);

        NSMutableArray<NSNumber *> *widths = [NSMutableArray array];
        for (CGFloat width = 700; width >= 200; width -= 19 + (CGFloat)(seed % 5))
            [widths addObject:@(width)];
        for (CGFloat width = 200; width <= 900; width += 31 + (CGFloat)(seed % 7))
            [widths addObject:@(width)];

        for (NSNumber *number in widths) {
            CGFloat width = number.doubleValue;
            [rig resizeToWidth:width];

            NSString *fault = [rig firstFault];
            XCTAssertNil(fault, @"seed %llu resized to width %.0f: %@", seed, width, fault);

            FlowBreakRig *fresh = [self rigWithSeed:seed width:width sections:sections];
            XCTAssertEqualObjects([rig signature], [fresh signature],
                                  @"seed %llu resized to width %.0f differs from a fresh layout", seed, width);
            if (fault)
                break;
        }
    }
}

// A long scrollback laid out in one go, as when a game is restored.
- (void)testFlowBreaksHoldInAFreshLayout {
    for (uint64_t seed = 20; seed < 30; seed++) {
        FlowBreakRig *rig = [self rigWithSeed:seed width:240 + 40 * (CGFloat)(seed % 10) sections:12];
        NSString *fault = [rig firstFault];
        XCTAssertNil(fault, @"seed %llu: %@", seed, fault);
    }
}

// Text arrives a piece at a time while the game runs, with layout in between.
- (void)testFlowBreaksHoldAsTextArrives {
    _random = 99;
    FlowBreakRig *rig = [[FlowBreakRig alloc] initWithWidth:420];
    for (int i = 0; i < 12; i++) {
        [rig marginImageOfSize:NSMakeSize(60 + [self random:80], 50 + [self random:120])
                     alignment:[self randomMargin]];
        [rig print:[self words:4 + [self random:8]]];
        [rig layOut];
        if (i % 3 != 2)
            [rig print:@"\n"];
        [rig flowBreak];
        [rig layOut];
        [rig print:[[self words:10 + [self random:60]] stringByAppendingString:@"\n"]];
        NSString *fault = [rig firstFault];
        XCTAssertNil(fault, @"after section %d: %@", i, fault);
    }
}

@end
