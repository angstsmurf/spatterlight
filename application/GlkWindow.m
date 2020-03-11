#import "main.h"

@implementation GlkWindow

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name {
    self = [super initWithFrame:NSZeroRect];

    if (self) {
        _glkctl = glkctl_;
        _theme = glkctl_.theme;
        _name = name;

        _pendingTerminators = [[NSMutableDictionary alloc]
            initWithObjectsAndKeys:@(NO), @keycode_Func1, @(NO), @keycode_Func2,
                                   @(NO), @keycode_Func3, @(NO), @keycode_Func4,
                                   @(NO), @keycode_Func5, @(NO), @keycode_Func6,
                                   @(NO), @keycode_Func7, @(NO), @keycode_Func8,
                                   @(NO), @keycode_Func9, @(NO),
                                   @keycode_Func10, @(NO), @keycode_Func11,
                                   @(NO), @keycode_Func12, @(NO),
                                   @keycode_Escape, nil];
        currentTerminators = _pendingTerminators;
        _terminatorsPending = NO;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _name = [decoder decodeIntegerForKey:@"name"];
        hyperlinks = [decoder decodeObjectForKey:@"hyperlinks"];
        currentHyperlink = [decoder decodeObjectForKey:@"currentHyperlink"];
        currentTerminators = [decoder decodeObjectForKey:@"currentTerminators"];
        _pendingTerminators =
            [decoder decodeObjectForKey:@"pendingTerminators"];
        _terminatorsPending = [decoder decodeBoolForKey:@"terminatorsPending"];
        char_request = [decoder decodeBoolForKey:@"char_request"];
        _styleHints = [decoder decodeObjectForKey:@"styleHints"];
        styles = [decoder decodeObjectForKey:@"styles"];

//        NSLog(@"GlkWindow initWithCoder: restored the following style hints:");
//        [self printStyleHints];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:_name forKey:@"name"];
    [encoder encodeObject:hyperlinks forKey:@"hyperlinks"];
    [encoder encodeObject:currentHyperlink forKey:@"currentHyperlink"];
    [encoder encodeObject:currentTerminators forKey:@"currentTerminators"];
    [encoder encodeObject:_pendingTerminators forKey:@"pendingTerminators"];
    [encoder encodeBool:_terminatorsPending forKey:@"terminatorsPending"];
    [encoder encodeBool:char_request forKey:@"char_request"];
    [encoder encodeObject:_styleHints forKey:@"styleHints"];
    [encoder encodeObject:styles forKey:@"styles"];
}

- (void)printStyleHints {

     NSArray *styleHintNames = @[ @"stylehint_Indentation", @"stylehint_ParaIndentation",
        @"stylehint_Justification", @"stylehint_Size",
        @"stylehint_Weight",@"stylehi@nt_Oblique", @"stylehint_Proportional",
        @"stylehint_TextColor", @"stylehint_BackColor", @"stylehint_ReverseColor",
        @"stylehint_NUMHINTS"];

    for (NSArray *style in _styleHints) {
        NSString *styleName;
        NSLog(@"[_styleHints indexOfObject:style] == %ld", [_styleHints indexOfObject:style]);
        if ([self isKindOfClass:[GlkTextBufferWindow class]])
            styleName = gBufferStyleNames[[_styleHints indexOfObject:style]];
        else
            styleName = gGridStyleNames[[_styleHints indexOfObject:style]];
        
        NSLog (@"Stylehints for style %@:", styleName);
        for (NSUInteger i = 0; i < stylehint_NUMHINTS ; i ++) {
            NSLog(@"%@: %@", styleHintNames[i], style[i]);
        }
    }
}

- (NSString *)sayMask:(NSUInteger)mask {
    NSString *maskToSay = [NSString
        stringWithFormat:@" %@ | %@",
                         (mask & NSViewWidthSizable) ? @"NSViewWidthSizable"
                                                     : @"NSViewMaxXMargin",
                         (mask & NSViewHeightSizable) ? @"NSViewHeightSizable"
                                                      : @"NSViewMaxYMargin"];
    return maskToSay;
}

//- (void)setStyle:(NSInteger)style
//      windowType:(NSInteger)wintype
//          enable:(NSInteger *)enable
//           value:(NSInteger *)value {
//    [styles removeObjectAtIndex:style];
//    [styles insertObject:[[GlkStyle alloc] initWithStyle:style
//                                              windowType:wintype
//                                                  enable:enable
//                                                   value:value]
//                 atIndex:style];
//}

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
           height:(NSInteger)h {
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

//- (NSDictionary *)attributesFromStylevalue:(NSInteger)stylevalue {
//    NSInteger style = stylevalue & 0xff;
//    NSInteger fg = (stylevalue >> 8) & 0xff;
//    NSInteger bg = (stylevalue >> 16) & 0xff;
//
//    if (fg || bg) {
//        NSMutableDictionary *mutatt =
//            [[styles objectAtIndex:style] mutableCopy];
//        [mutatt setObject:@(stylevalue) forKey:@"GlkStyle"];
//        if ([Preferences stylesEnabled]) {
//            if (fg)
//                [mutatt setObject:[Preferences foregroundColor:(int)(fg - 1)]
//                           forKey:NSForegroundColorAttributeName];
//            if (bg)
//                [mutatt setObject:[Preferences backgroundColor:(int)(bg - 1)]
//                           forKey:NSBackgroundColorAttributeName];
//        }
//        return (NSDictionary *)mutatt;
//    } else {
//        return [styles objectAtIndex:style];
//    }
//}

- (void)moveToColumn:(NSUInteger)x row:(NSUInteger)y {
    NSLog(@"move cursor in %@ not implemented", [self class]);
}

- (void)initLine:(NSString *)buf {
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

- (void)setHyperlink:(NSUInteger)linkid;
{ NSLog(@"hyperlink input in %@ not implemented", [self class]); }

- (void)initHyperlink {
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (void)cancelHyperlink {
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (BOOL)hasLineRequest {
    return NO;
}

#pragma mark -
#pragma mark Windows restoration

+ (NSArray *)restorableStateKeyPaths {
    return @[ @"name" ];
}

#pragma mark Accessibility

- (BOOL)accessibilityIsIgnored {
    return NO;
}

- (void)restoreSelection {
}

@end
