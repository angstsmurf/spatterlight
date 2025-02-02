#import "GlkWindow.h"
#import "GlkController.h"
#import "GlkTextGridWindow.h"

#import "ZColor.h"
#import "InputHistory.h"
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

        if (_glkctl.gameID == kGameIsBeyondZork || [_glkctl zVersion6]) {
            [self adjustBZTerminators:_pendingTerminators];
        }

        _currentTerminators = _pendingTerminators;
        self.canDrawConcurrently = YES;
        usingStyles = self.theme.doStyles;
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
        usingStyles = [decoder decodeBoolForKey:@"usingStyles"];
        underlineLinks = [decoder decodeBoolForKey:@"underlineLinks"];
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
    [encoder encodeBool:usingStyles forKey:@"usingStyles"];
    [encoder encodeBool:underlineLinks forKey:@"underlineLinks"];
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
    if ([valObj isNotEqualTo:[NSNull null]]) {
        *value = valObj.integerValue;
        return YES;
    }

    return NO;
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

// A possible optimization would be to cache this
// instead of recreating it on every print operation.
- (NSMutableDictionary *)getCurrentAttributesForStyle:(NSUInteger)stylevalue {

    NSMutableDictionary *attributes = [styles[stylevalue] mutableCopy];

    if (currentZColor) {
        attributes[@"ZColor"] = currentZColor;
        if (self.theme.doStyles) {
            if ([self.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                // If the style has reverseColor hint set, we apply the zcolors in reverse
                attributes = [currentZColor reversedAttributes:attributes];
            } else {
                attributes = [currentZColor coloredAttributes:attributes];
            }
        }
    }

    if (self.currentReverseVideo) {
        attributes[@"ReverseVideo"] = @(YES);
        if (!self.theme.doStyles || [self.styleHints[stylevalue][stylehint_ReverseColor] isNotEqualTo:@(1)]) {
            // Current style has stylehint_ReverseColor unset, so we reverse colors
            attributes = [self reversedAttributes:attributes background:[self isKindOfClass:[GlkTextGridWindow class]] ? self.theme.gridBackground : self.theme.bufferBackground];
        }
    }

    if (self.currentHyperlink) {
        attributes[NSLinkAttributeName] = @(self.currentHyperlink);
    }

    return attributes;
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

    if (currentZColor && !(currentZColor.fg == fg && currentZColor.bg == bg)) {
        // If there already was a previous active zcolor, we
        // deactivate it here, unless it uses the same colors as the new one.
        currentZColor = nil;
    }

    if (!currentZColor && !(fg == zcolor_Default && bg == zcolor_Default)) {
        currentZColor =
        [[ZColor alloc] initWithText:fg background:bg];
    }
}

- (NSMutableAttributedString *)applyZColorsAndThenReverse:(NSMutableAttributedString *)attStr {
    NSUInteger textstoragelength = attStr.length;

    GlkWindow * __weak weakSelf = self;
    NSColor *blockBgCol;
    if ([self isKindOfClass:[GlkTextGridWindow class]])
        blockBgCol = self.theme.gridBackground;
    else
        blockBgCol = self.theme.bufferBackground;

    if (self.theme.doStyles) {
        [attStr
         enumerateAttribute:@"ZColor"
         inRange:NSMakeRange(0, textstoragelength)
         options:0
         usingBlock:^(id value, NSRange range, BOOL *stop) {
            if (!value) {
                return;
            }
            ZColor *z = value;
            [attStr
             enumerateAttributesInRange:range
             options:0
             usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
                NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
                NSMutableDictionary *mutDict = [dict mutableCopy];
                if ([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                    // Style has stylehint_ReverseColor set,
                    // So we apply Zcolor with reversed attributes
                    mutDict = [z reversedAttributes:mutDict];
                } else {
                    // Apply Zcolor normally
                    mutDict = [z coloredAttributes:mutDict];
                }
                [attStr addAttributes:mutDict range:range2];
            }];
        }];
    }

    [attStr
     enumerateAttribute:@"ReverseVideo"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (!value) {
            return;
        }
        [attStr
         enumerateAttributesInRange:range
         options:0
         usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
            NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
            BOOL zcolorValue = (dict[@"ZColor"] != nil);
            if (!([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)] && !zcolorValue)) {
                NSMutableDictionary *mutDict = [dict mutableCopy];
                mutDict = [weakSelf reversedAttributes:mutDict background:blockBgCol];
                [attStr addAttributes:mutDict range:range2];
            }
        }];
    }];

    return attStr;
}

- (NSMutableAttributedString *)applyReverseOnly:(NSMutableAttributedString *)attStr {
    NSUInteger textstoragelength = attStr.length;

    GlkWindow * __weak weakSelf = self;
    NSColor *blockBgCol;
    if ([self isKindOfClass:[GlkTextGridWindow class]])
        blockBgCol = self.theme.gridBackground;
    else
        blockBgCol = self.theme.bufferBackground;

    [attStr
     enumerateAttribute:@"ReverseVideo"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (!value) {
            return;
        }
        [attStr
         enumerateAttributesInRange:range
         options:0
         usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
            NSMutableDictionary *mutDict = [dict mutableCopy];
            mutDict = [weakSelf reversedAttributes:mutDict background:blockBgCol];
            [attStr addAttributes:mutDict range:range2];
        }];
    }];

    return attStr;
}

- (void)postRestoreAdjustments:(GlkWindow *)win {
    NSLog(@"postRestoreAdjustments in %@ not implemented", [self class]);
}

- (BOOL)hasCharRequest {
    return char_request;
}

- (BOOL)hasLineRequest {
    NSLog(@"hasLineRequest in %@ not implemented", [self class]);
    return NO;
}

- (void)sendCommandLine:(NSString *)command {
    NSLog(@"sendCommandLine in %@ not implemented", [self class]);
}

- (void)sendKeypress:(unsigned)ch {
    NSLog(@"sendKeypress in %@ not implemented", [self class]);
}


- (NSArray *)links {
    NSLog(@"links in %@ not implemented", [self class]);
    return @[];
}

- (NSArray *)images {
    NSLog(@"images in %@ not implemented", [self class]);
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

- (IBAction)saveAsRTF:(id)sender {
    [self flushDisplay];
    NSWindow *window = self.glkctl.window;

    NSString *newName = window.title.stringByDeletingPathExtension;

    // Set the default name for the file and show the panel.

    self.savePanel = [NSSavePanel savePanel];
    NSSavePanel *panel = self.savePanel;
    panel.nameFieldLabel = NSLocalizedString(@"Save text: ", nil);
    panel.extensionHidden = NO;
    panel.canCreateDirectories = YES;

    panel.nameFieldStringValue = newName;

    NSTextView *localTextView = (NSTextView *)((GlkTextGridWindow *)self).textview;
    NSAttributedString *localTextStorage = localTextView.textStorage;

    panel.accessoryView = [self saveScrollbackAccessoryViewHasImages:localTextStorage.containsAttachments];
    NSPopUpButton *popUp = self.accessoryPopUp;

    [self selectFormat:popUp];

    if (!localTextView || !localTextStorage)
        return;

    [panel
     beginSheetModalForWindow:window
     completionHandler:^(NSInteger result) {
        if (result == NSModalResponseOK) {
            NSURL *theFile = panel.URL;

            kSaveTextFormatType fileFormat = (kSaveTextFormatType)popUp.selectedItem.tag;
            [[NSUserDefaults standardUserDefaults] setInteger:fileFormat forKey:@"ScrollbackSaveFormat"];

            NSError *error = nil;
            BOOL writeResult = NO;
            if (fileFormat == kPlainText) {
                unichar nc = '\0';
                NSString *nullChar = [NSString stringWithCharacters:&nc length:1];
                NSString *string = [localTextStorage.string stringByReplacingOccurrencesOfString:nullChar withString:@""];
                if (![string hasSuffix:@"\n"])
                    string = [string stringByAppendingString:@"\n"];
                writeResult = [string writeToURL:theFile atomically:NO encoding:NSUTF8StringEncoding error:&error];
            } else {

                NSMutableAttributedString *mutattstr =
                [localTextStorage mutableCopy];

                if (localTextView.backgroundColor)
                    [mutattstr
                     enumerateAttribute:NSBackgroundColorAttributeName
                     inRange:NSMakeRange(0, mutattstr.length)
                     options:0
                     usingBlock:^(id value, NSRange range, BOOL *stop) {
                        if (!value || [value isEqual:[NSColor textBackgroundColor]]) {
                            [mutattstr
                             addAttribute:NSBackgroundColorAttributeName
                             value:localTextView.backgroundColor
                             range:range];
                        }
                    }];


                if (fileFormat == kRTFD) {
                    NSFileWrapper *wrapper;
                    wrapper = [mutattstr
                               RTFDFileWrapperFromRange:NSMakeRange(0, mutattstr.length)
                               documentAttributes:@{
                        NSDocumentTypeDocumentAttribute:NSRTFDTextDocumentType
                    }];

                    writeResult = [wrapper writeToURL:theFile
                                              options:
                                   NSFileWrapperWritingAtomic |
                                   NSFileWrapperWritingWithNameUpdating
                                  originalContentsURL:nil
                                                error:&error];

                } else {
                    NSData *data = [mutattstr
                                    RTFFromRange:NSMakeRange(0,
                                                             mutattstr.length)
                                    documentAttributes:@{
                        NSDocumentTypeDocumentAttribute :
                            NSRTFTextDocumentType
                    }];
                    writeResult = [data writeToURL:theFile options:0 error:&error];
                }
            }
            if (!writeResult || error)
                NSLog(@"Error writing file: %@", error);
        }
    }];
}

- (NSView *)saveScrollbackAccessoryViewHasImages:(BOOL)hasImages {

    NSArray *buttonItems;
    if (hasImages)
        buttonItems = @[@"Rich Text Format with images", @"Rich Text Format without images", @"Plain Text"];
    else
        buttonItems = @[@"Rich Text Format", @"Plain Text"];

    NSView  *accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 300, 32.0)];

    NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 60, 22)];
    label.editable = NO;
    label.stringValue = NSLocalizedString(@"Format:", nil);
    label.bordered = NO;
    label.bezeled = NO;
    label.drawsBackground = NO;

    _accessoryPopUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(50.0, 2, 240, 22.0) pullsDown:NO];
    [_accessoryPopUp addItemsWithTitles:buttonItems];

    if (hasImages) {
        NSMenuItem *rtfd = [_accessoryPopUp itemAtIndex:0];
        rtfd.tag = kRTFD;
        NSMenuItem *rtf = [_accessoryPopUp itemAtIndex:1];
        rtf.tag = kRTF;
        NSMenuItem *plainText = [_accessoryPopUp itemAtIndex:2];
        plainText.tag = kPlainText;
    } else {
        NSMenuItem *rtf = [_accessoryPopUp itemAtIndex:0];
        rtf.tag = kRTF;
        NSMenuItem *plainText = [_accessoryPopUp itemAtIndex:1];
        plainText.tag = kPlainText;
    }

    kSaveTextFormatType defaultType = (kSaveTextFormatType)[[NSUserDefaults standardUserDefaults] integerForKey:@"ScrollbackSaveFormat"];
    if (defaultType == kRTFD && !hasImages)
        defaultType = kRTF;

    _accessoryPopUp.target = self;
    _accessoryPopUp.action = @selector(selectFormat:);

    [accessoryView addSubview:label];
    [accessoryView addSubview:_accessoryPopUp];
    [_accessoryPopUp selectItemWithTag:defaultType];

    return accessoryView;
}

- (void)selectFormat:(id)sender
{
    NSPopUpButton       *button =                 (NSPopUpButton *)sender;
    kSaveTextFormatType selectedItemTag = (kSaveTextFormatType)button.selectedItem.tag;
    NSString            *nameFieldString = self.savePanel.nameFieldStringValue;
    NSString            *trimmedNameFieldString = nameFieldString.stringByDeletingPathExtension;
    NSString            *extension;

    NSDictionary *tagToString = @{@(kRTFD) : @"rtfd",
                                  @(kRTF) : @"rtf",
                                  @(kPlainText) : @"txt"};
    extension = tagToString[@(selectedItemTag)];

    NSString *nameFieldStringWithExt = [NSString stringWithFormat:@"%@.%@", trimmedNameFieldString, extension];
    (self.savePanel).nameFieldStringValue = nameFieldStringWithExt;

    // If the Finder Preference "Show all filename extensions" is false or the
    // panel's extensionHidden property is YES (the default), then the
    // nameFieldStringValue will not include the extension we just changed/added.
    // So, in order to ensure that the panel's URL has the extension we've just
    // specified, the workaround is to restrict the allowed file types to only
    // the specified one.
    self.savePanel.allowedFileTypes = @[extension];
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

- (void)speakStatus {
    NSLog(@"speakStatus in %@ not implemented", [self class]);
}


- (BOOL)setLastMove {
    NSLog(@"setLastMove in %@ not implemented", [self class]);
    return NO;
}

- (void)recalcBackground {
    NSLog(@"recalcBackground in %@ not implemented", [self class]);
}

- (NSArray *)accessibilityCustomRotors  {
    return [self.glkctl createCustomRotors];
}

- (NSArray *)accessibilityCustomActions {
    NSArray *actions = self.glkctl.accessibilityCustomActions;
    return actions;
}

- (BOOL)isAccessibilityElement {
    return NO;
}

@end
