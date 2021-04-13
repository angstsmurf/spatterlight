#import "main.h"
#import "ZColor.h"
#import "InputHistory.h"
#import "Preferences.h"
#import "Theme.h"

#include "glkimp.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation GlkWindow

+ (BOOL) supportsSecureCoding {
     return YES;
 }

+ (BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl
                                 name:(NSInteger)name {
    self = [super initWithFrame:NSZeroRect];

    if (self) {
        _glkctl = glkctl;
        _theme = glkctl.theme;
        _name = name;
        bgnd = -1;

        _pendingTerminators = [[NSMutableDictionary alloc]
                               initWithObjectsAndKeys:
                               @(NO), @keycode_Func1,
                               @(NO), @keycode_Func2,
                               @(NO), @keycode_Func3,
                               @(NO), @keycode_Func4,
                               @(NO), @keycode_Func5,
                               @(NO), @keycode_Func6,
                               @(NO), @keycode_Func7,
                               @(NO), @keycode_Func8,
                               @(NO), @keycode_Func9,
                               @(NO), @keycode_Func10,
                               @(NO), @keycode_Func11,
                               @(NO), @keycode_Func12,
                               @(NO), @keycode_Left,
                               @(NO), @keycode_Right,
                               @(NO), @keycode_Up,
                               @(NO), @keycode_Down,
                               @(NO), @keycode_Escape,
                               @(NO), @keycode_Pad0,
                               @(NO), @keycode_Pad1,
                               @(NO), @keycode_Pad2,
                               @(NO), @keycode_Pad3,
                               @(NO), @keycode_Pad4,
                               @(NO), @keycode_Pad5,
                               @(NO), @keycode_Pad6,
                               @(NO), @keycode_Pad7,
                               @(NO), @keycode_Pad8,
                               @(NO), @keycode_Pad9,
                               nil];

        if (glkctl.beyondZork) {
            [self adjustBZTerminators:_pendingTerminators];
        }

        _currentTerminators = _pendingTerminators;
        _terminatorsPending = NO;
        
        self.canDrawConcurrently = YES;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _name = [decoder decodeIntegerForKey:@"name"];
        _currentHyperlink = [decoder decodeIntegerForKey:@"currentHyperlink"];
        _currentTerminators = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"currentTerminators"];
        _pendingTerminators = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"pendingTerminators"];
        _terminatorsPending = [decoder decodeBoolForKey:@"terminatorsPending"];
        char_request = [decoder decodeBoolForKey:@"char_request"];
        _styleHints = [decoder decodeObjectOfClass:[NSArray class] forKey:@"styleHints"];
        styles = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"styles"];
        currentZColor =  [decoder decodeObjectOfClass:[ZColor class] forKey:@"currentZColor"];
        _currentReverseVideo = [decoder decodeBoolForKey:@"currentReverseVideo"];
        bgnd = [decoder decodeIntegerForKey:@"bgnd"];
        _framePending = [decoder decodeBoolForKey:@"framePending"];
        _pendingFrame = ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"pendingFrame"]).rectValue;
        _moveRanges = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"moveRanges"];
        moveRangeIndex = (NSUInteger)[decoder decodeIntegerForKey:@"moveRangeIndex"];
        history = [decoder decodeObjectOfClass:[InputHistory class] forKey:@"history"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:_name forKey:@"name"];
    [encoder encodeInteger:_currentHyperlink forKey:@"currentHyperlink"];
    [encoder encodeObject:_currentTerminators forKey:@"currentTerminators"];
    [encoder encodeObject:_pendingTerminators forKey:@"pendingTerminators"];
    [encoder encodeBool:_terminatorsPending forKey:@"terminatorsPending"];
    [encoder encodeBool:char_request forKey:@"char_request"];
    [encoder encodeObject:_styleHints forKey:@"styleHints"];
    [encoder encodeObject:styles forKey:@"styles"];
    [encoder encodeObject:currentZColor forKey:@"currentZColor"];
    [encoder encodeBool:_currentReverseVideo forKey:@"currentReverseVideo"];
    [encoder encodeInteger:bgnd forKey:@"bgnd"];
    [encoder encodeBool:_framePending forKey:@"framePending"];
    NSValue *frameObj = [NSValue valueWithRect:_pendingFrame];
    [encoder encodeObject:frameObj forKey:@"pendingFrame"];
    [encoder encodeObject:_moveRanges forKey:@"moveRanges"];
    [encoder encodeInteger:(NSInteger)moveRangeIndex forKey:@"moveRangeIndex"];
    [encoder encodeObject:history forKey:@"history"];
}

- (NSArray *)deepCopyOfStyleHintsArray:(NSArray *)array {
    NSMutableArray *newArray = [[NSMutableArray alloc] initWithCapacity:style_NUMSTYLES];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        [newArray addObject:[[NSArray alloc] initWithArray:array[i] copyItems:YES]];
    }
    return newArray;
}

- (BOOL)getStyleVal:(NSUInteger)style
               hint:(NSUInteger)hint
              value:(NSInteger *)value {

    NSNumber *valObj = nil;
    
    if (style >= style_NUMSTYLES)
        return NO;

    if (hint >= stylehint_NUMHINTS)
        return NO;

    NSArray *hintsForStyle = _styleHints[style];

    valObj = hintsForStyle[hint];
    if ([valObj isNotEqualTo:[NSNull null]])
       *value = valObj.integerValue;

    return [valObj isNotEqualTo:[NSNull null]];
}

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict background:(NSColor *)backCol {
    NSColor *fg = dict[NSForegroundColorAttributeName];
    NSColor *bg = dict[NSBackgroundColorAttributeName];
    if (!bg)
        bg = backCol;
    if (bg)
        dict[NSForegroundColorAttributeName] = bg;
    if (fg)
        dict[NSBackgroundColorAttributeName] = fg;
    return dict;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)prefsDidChange {
}

- (void)terpDidStop {
}

- (BOOL)wantsFocus {
    return NO;
}

- (void)grabFocus {
    // NSLog(@"grab focus in window %ld", self.name);
    [self.window makeFirstResponder:self];
    NSAccessibilityPostNotification(
        self, NSAccessibilityFocusedUIElementChangedNotification);
}

- (void)flushDisplay {
    if (dirty)
        self.needsDisplay = YES;
    dirty = NO;
}

- (BOOL)wantsDefaultClipping {
    return NO;
}

- (void)setBgColor:(NSInteger)bc {
    NSLog(@"set background color in %@ not allowed", [self class]);
}

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)n {
    NSLog(@"fillrect in %@ not implemented", [self class]);
}

- (void)drawImage:(NSImage *)buf
             val1:(NSInteger)v1
             val2:(NSInteger)v2
            width:(NSInteger)w
           height:(NSInteger)h
            style:(NSUInteger)style {
    NSLog(@"drawimage in %@ not implemented", [self class]);
}

- (void)flowBreak {
    NSLog(@"flowbreak in %@ not implemented", [self class]);
}

- (void)makeTransparent {
    NSLog(@"makeTransparent in %@ not implemented", [self class]);
}

- (void)markLastSeen {
}
- (void)performScroll {
}

- (void)clear {
    NSLog(@"clear in %@ not implemented", [self class]);
}

- (void)putString:(NSString *)buf style:(NSUInteger)style {
    NSLog(@"print in %@ not implemented", [self class]);
}

- (NSUInteger)unputString:(NSString *)buf {
    NSLog(@"unprint in %@ not implemented", [self class]);
    return 0;
}

- (void)moveToColumn:(NSUInteger)x row:(NSUInteger)y {
    NSLog(@"move cursor in %@ not implemented", [self class]);
}

- (void)initLine:(NSString *)buf maxLength:(NSUInteger)maxLength {
    NSLog(@"line input in %@ not implemented", [self class]);
}

- (NSString *)cancelLine {
    return @"";
}

- (void)initChar {
    NSLog(@"char input in %@ not implemented", [self class]);
}

- (void)cancelChar {
}

- (void)initMouse {
    NSLog(@"mouse input in %@ not implemented", [self class]);
}

- (void)cancelMouse {
}

- (void)initHyperlink {
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (void)cancelHyperlink {
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg {
    NSLog(@"ZColors in %@ not implemented", [self class]);
}

- (void)postRestoreAdjustments:(GlkWindow *)win {
    NSLog(@"postRestoreAdjustments in %@ not implemented", [self class]);
}

- (BOOL)hasCharRequest {
    return char_request;
}

- (NSArray *)links {
    NSLog(@"links in %@ not implemented", [self class]);
    return @[];
}

// Convert key terminators for Beyond Zork arrow keys hack/setting
- (void)adjustBZTerminators:(NSMutableDictionary *)terminators {
    if (_theme.bZTerminator == kBZArrowsOriginal) {
        if (terminators[@(keycode_Left)] == nil) {
            terminators[@(keycode_Left)] = terminators[@"storedLeft"];
            terminators[@"storedLeft"] = nil;
            
            terminators[@(keycode_Right)] = terminators[@"storedRight"];
            terminators[@"storedRight"] = nil;
        }
    } else {
        if (terminators[@(keycode_Left)] != nil) {
            terminators[@"storedLeft"] = terminators[@(keycode_Left)];
            terminators[@(keycode_Left)] = nil;

            terminators[@"storedRight"] = terminators[@(keycode_Right)];
            terminators[@(keycode_Right)] = nil;
        }
    }

    // We don't hack the up and down keys for grid windows
    if ([self isKindOfClass:[GlkTextGridWindow class]])
        return;

    if (_theme.bZTerminator != kBZArrowsSwapped) {
        if (terminators[@(keycode_Up)] == nil) {
            terminators[@(keycode_Up)] = terminators[@(keycode_Home)];
            terminators[@(keycode_Home)] = nil;

            terminators[@(keycode_Down)] = terminators[@(keycode_End)];
            terminators[@(keycode_End)] = nil;
        }
    } else {
        if (terminators[@(keycode_Home)] == nil) {
            terminators[@(keycode_Home)] = terminators[@(keycode_Up)];
            terminators[@(keycode_Up)] = nil;

            terminators[@(keycode_End)] = terminators[@(keycode_Down)];
            terminators[@(keycode_Down)] = nil;
        }
    }
}

#pragma mark -
#pragma mark Windows restoration

+ (NSArray *)restorableStateKeyPaths {
    return @[ @"name" ];
}

#pragma mark Accessibility

- (void)repeatLastMove:(id)sender {
    NSLog(@"repeatLastMove in %@ not implemented", [self class]);
}

- (void)speakPrevious {
    NSLog(@"speakPrevious in %@ not implemented", [self class]);
}
- (void)speakNext {
    NSLog(@"speakNext in %@ not implemented", [self class]);
}

- (BOOL)setLastMove {
    NSLog(@"setLastMove in %@ not implemented", [self class]);
    return NO;
}

- (NSArray *)accessibilityCustomRotors  {
   return [self.glkctl createCustomRotors];
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    NSArray *actions = [self.glkctl accessibilityCustomActions];
    return actions;
}

- (BOOL)isAccessibilityElement {
    return NO;
}

@end
