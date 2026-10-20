#import "GlkWindow.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkEvent.h"
#import "GlkTextGridWindow.h"

#import "Constants.h"
#import "GlkStyle.h"

#import "InputHistory.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "SPAccessibility.h"
#import "Theme.h"
#import "ZColor.h"

#include "glkimp.h"

#import "DebugLog.h"

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
        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];
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
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
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

+ (NSArray *)deepCopyOfStyleHintsArray:(NSArray *)array {
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

- (GlkStyle *)themeStyle:(NSUInteger)style {
    return nil;
}

- (NSColor *)themeBackground {
    return self.theme.bufferBackground;
}

- (NSDictionary *)themeAttributesForStyle:(NSUInteger)style {
    GlkStyle *glkStyle = [self themeStyle:style];
    if (self.theme.doStyles)
        return [glkStyle attributesWithHints:self.styleHints[style]];
    return glkStyle.attributeDict;
}

- (int32_t)themeLinkStyle {
    return NSUnderlineStyleNone;
}

- (BOOL)syncStyleFlagsWithTheme {
    BOOL different = NO;

    if (usingStyles != self.theme.doStyles) {
        different = YES;
        usingStyles = self.theme.doStyles;
    }

    BOOL newUnderlineLinks = (self.themeLinkStyle != NSUnderlineStyleNone);
    if (underlineLinks != newUnderlineLinks) {
        different = YES;
        underlineLinks = newUnderlineLinks;
    }

    return different;
}

- (void)updateLinkTextAttributesOf:(NSTextView *)textView {
    NSMutableDictionary *linkAttributes = [textView.linkTextAttributes mutableCopy];
    linkAttributes[NSUnderlineStyleAttributeName] = @(self.themeLinkStyle);
    linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
    textView.linkTextAttributes = linkAttributes;
}

- (NSDictionary *)baseAttributesForStyle:(NSUInteger)stylevalue {
    if (stylevalue >= style_NUMSTYLES)
        return nil;

    if (styles.count <= stylevalue || [styles[stylevalue] isEqual:[NSNull null]]) {
        return [self themeStyle:stylevalue].attributeDict;
    }

    return styles[stylevalue];
}

// A possible optimization would be to cache this
// instead of recreating it on every print operation.
- (NSMutableDictionary *)getCurrentAttributesForStyle:(NSUInteger)stylevalue {

    if (stylevalue >= styles.count)
        stylevalue = style_Normal;
    if (stylevalue >= styles.count)
        return nil;

    if ([styles[stylevalue] isEqual:[NSNull null]]) {
        GlkStyle *themeStyle = [self themeStyle:stylevalue];
        if (!themeStyle)
            return nil;
        styles[stylevalue] = themeStyle.attributeDict;
    }

    NSMutableDictionary *attributes = [styles[stylevalue] mutableCopy];

    NSArray *hintsForStyle = self.styleHints[stylevalue];
    if (hintsForStyle.count <= stylehint_ReverseColor)
        return attributes;

    if (currentZColor) {
        attributes[@"ZColor"] = currentZColor;
        if (self.theme.doStyles) {
            if ([hintsForStyle[stylehint_ReverseColor] isEqualTo:@(1)]) {
                // If the style has reverseColor hint set, we apply the zcolors in reverse
                attributes = [currentZColor reversedAttributes:attributes];
            } else {
                attributes = [currentZColor coloredAttributes:attributes];
            }
        }
    }

    if (self.currentReverseVideo) {
        attributes[@"ReverseVideo"] = @(YES);
        if (!self.theme.doStyles || [hintsForStyle[stylehint_ReverseColor] isNotEqualTo:@(1)]) {
            // Current style has stylehint_ReverseColor unset, so we reverse colors
            attributes = [self reversedAttributes:attributes background:self.themeBackground];
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
    if (self.window.firstResponder != self) {
        [self.window makeFirstResponder:self];
        NSAccessibilityPostNotification(
                                        self, NSAccessibilityFocusedUIElementChangedNotification);
    }
}

+ (unsigned)keycodeForEvent:(NSEvent *)evt mapNumericPad:(BOOL)mapNumericPad {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length - 1]);
    if (mapNumericPad && (evt.modifierFlags & NSEventModifierFlagNumericPad) && ch >= '0' && ch <= '9')
        ch = keycode_Pad0 - (ch - '0');
    return ch;
}

- (BOOL)forwardKeyDownToFocusedWindow:(NSEvent *)evt {
    if (self.wantsFocus)
        return NO;
    for (GlkWindow *win in self.glkctl.gwindows.allValues) {
        if (win != self && win.wantsFocus) {
            NSLog(@"%@ %ld: Passing on keypress to %@ %ld", self.class, self.name, win.class, win.name);
            [win grabFocus];
            [win keyDown:evt];
            return YES;
        }
    }
    return NO;
}

- (void)flushDisplay {
    if (dirty)
        self.needsDisplay = YES;
    dirty = NO;
}

- (BOOL)wantsDefaultClipping {
    return NO;
}

// The methods below that call this are overridden by the window types that
// support them. A game that asks the wrong kind of window for something only
// gets this log line.
- (void)subclassResponsibility:(SEL)selector {
    NSLog(@"%@ in %@ not implemented", NSStringFromSelector(selector), [self class]);
}

- (NSTextView *)textview {
    return nil;
}

- (void)setBgColor:(NSInteger)bc {
    [self subclassResponsibility:_cmd];
}

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)n {
    [self subclassResponsibility:_cmd];
}

- (void)drawImage:(NSImage *)buf
             val1:(NSInteger)v1
             val2:(NSInteger)v2
            width:(NSInteger)w
           height:(NSInteger)h
        imagerule:(NSUInteger)imagerule
         maxwidth:(NSUInteger)maxwidth
            style:(NSUInteger)style {
    [self subclassResponsibility:_cmd];
}

- (void)flowBreak {
    [self subclassResponsibility:_cmd];
}

- (void)makeTransparent {
    [self subclassResponsibility:_cmd];
}

- (void)markLastSeen {
}
- (void)performScroll {
}

- (void)clear {
    [self subclassResponsibility:_cmd];
}

- (void)putString:(NSString *)buf style:(NSUInteger)style {
    [self subclassResponsibility:_cmd];
}

- (NSUInteger)unputString:(NSString *)buf {
    [self subclassResponsibility:_cmd];
    return 0;
}

- (void)moveToColumn:(NSUInteger)x row:(NSUInteger)y {
    [self subclassResponsibility:_cmd];
}

- (void)initLine:(NSString *)buf maxLength:(NSUInteger)maxLength {
    [self subclassResponsibility:_cmd];
}

- (NSString *)cancelLine {
    return @"";
}

- (void)initChar {
    [self subclassResponsibility:_cmd];
}

- (void)cancelChar {
}

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
}

- (void)initHyperlink {
    hyper_request = YES;
}

- (void)cancelHyperlink {
    hyper_request = NO;
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
    
    NSArray *styleHints = self.styleHints;

    if (textstoragelength == 0 || styleHints.count == 0)
        return attStr;

    GlkWindow * __weak weakSelf = self;
    NSColor *blockBgCol = self.themeBackground;

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
                if (stylevalue >= styleHints.count)
                    return;
                NSArray *hintsForStyle = styleHints[stylevalue];
                if (hintsForStyle.count <= stylehint_ReverseColor)
                    return;
                NSMutableDictionary *mutDict = [dict mutableCopy];
                if ([hintsForStyle[stylehint_ReverseColor] isEqualTo:@(1)]) {
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
            if (stylevalue >= styleHints.count)
                return;
            NSArray *hintsForStyle = styleHints[stylevalue];
            if (hintsForStyle.count <= stylehint_ReverseColor)
                return;
            BOOL zcolorValue = (dict[@"ZColor"] != nil);
            if (!([hintsForStyle[stylehint_ReverseColor] isEqualTo:@(1)] && !zcolorValue)) {
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
    NSColor *blockBgCol = self.themeBackground;

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
    [self subclassResponsibility:_cmd];
}

- (BOOL)hasCharRequest {
    return char_request;
}

- (BOOL)hasLineRequest {
    [self subclassResponsibility:_cmd];
    return NO;
}

- (void)sendCommandLine:(NSString *)command {
    [self subclassResponsibility:_cmd];
}

// Queue a character event and end the char request.
- (void)sendKeypress:(unsigned)ch {
    GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
    [self.glkctl queueEvent:gev];
    char_request = NO;
}

// Queue a mouse event at `point`, in the window's Glk coordinates (pixels
// for graphics windows, cells for grids), and end the mouse request.
- (void)sendMouseEventAtPoint:(NSPoint)point {
    [self.glkctl markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:point forWindow:self.name];
    [self.glkctl queueEvent:gev];
    mouse_request = NO;
}


- (NSArray *)links {
    [self subclassResponsibility:_cmd];
    return @[];
}

- (NSArray *)images {
    [self subclassResponsibility:_cmd];
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

    NSTextView *localTextView = self.textview;
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
                // If a text buffer window starts with a margin image before any text, we insert a soft hyphen (codepoint U+00AD) before the attachment character to avoid layout glitches. We strip that character from the plain text here.
                // We also strip the unicode replacement character U+FFFC which both attachments and style changes seem to leave behind, and null characters, which might be hiding anywhere in an NSString.
                NSCharacterSet *unwanted = [NSCharacterSet characterSetWithCharactersInString:@"\u00AD\0\uFFFC"];
                NSString *string = [localTextStorage.string stringByTrimmingCharactersInSet:unwanted];
                string = [string stringByReplacingOccurrencesOfString:@"\uFFFC" withString:@""];
                unichar nc = '\0';
                NSString *nullChar = [NSString stringWithCharacters:&nc length:1];
                string = [string stringByReplacingOccurrencesOfString:nullChar withString:@""];
                if (![string hasSuffix:@"\n"])
                    string = [string stringByAppendingString:@"\n"];
                writeResult = [string writeToURL:theFile atomically:NO encoding:NSUTF8StringEncoding error:&error];
            } else {
                NSMutableAttributedString *mutattstr =
                [localTextStorage mutableCopy];

                NSUInteger __block index = 1;

                if (fileFormat == kRTFD) {
                    // We replace all image attachments with file wrappers.
                    // This will of course remove any image alignment settings. There is no support for that in RTFD (Apple's custom rich text format with attachments).
                    [localTextStorage
                     enumerateAttribute:NSAttachmentAttributeName
                     inRange:NSMakeRange(0, localTextStorage.length)
                     options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                     usingBlock:^(NSTextAttachment *attachment, NSRange range, BOOL *stop) {
                        MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
                        if (!cell)
                            return;
                        NSImage *image = cell.image;
                        if (!image && cell.marginImage)
                            image = cell.marginImage.image;
                        if (image) {
                            NSData *tiffdata = image.TIFFRepresentation;

                            NSFileWrapper *wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:tiffdata];
                            wrapper.preferredFilename = [@"image " stringByAppendingFormat:@"%ld.tiff", index++];
                            NSTextAttachment *newAttachment = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
                            [mutattstr
                             addAttribute:NSAttachmentAttributeName value:newAttachment range:range];
                        }
                    }];

                    NSFileWrapper *wrapper = [mutattstr
                               RTFDFileWrapperFromRange:NSMakeRange(0, mutattstr.length)
                               documentAttributes:@{
                        NSDocumentTypeDocumentAttribute:NSRTFDTextDocumentType,
                        NSBackgroundColorDocumentAttribute:localTextView.backgroundColor
                    }];

                    writeResult = [wrapper writeToURL:theFile
                                              options:
                                   NSFileWrapperWritingAtomic                                  originalContentsURL:nil
                                                error:&error];
                    if (writeResult == NO) NSLog(@"Error: %@", error);

                } else {
                    NSData *data = [mutattstr
                                    RTFFromRange:NSMakeRange(0, mutattstr.length)
                                    documentAttributes:@{
                           NSDocumentTypeDocumentAttribute: NSRTFTextDocumentType,
                        NSBackgroundColorDocumentAttribute:localTextView.backgroundColor
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

    _accessoryPopUp.accessibilityIdentifier = @"saveFormatPopUp";

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

+ (NSArray<Class> *) allowedClassesForRestorableStateKeyPath:(NSString *) keyPath {
    return @[[NSString class]];
}

#pragma mark Accessibility

- (void)repeatLastMove:(id)sender {
    [self subclassResponsibility:_cmd];
}

- (void)speakPrevious {
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex > 0) {
        moveRangeIndex--;
    } else {
        prefix = @"At first move.\n";
        moveRangeIndex = 0;
    }
    NSString *str = [prefix stringByAppendingString:[self speakableStringForMoveRange:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (void)speakNext {
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
    }
    NSString *str = [prefix stringByAppendingString:[self speakableStringForMoveRange:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (NSString *)speakableStringForMoveRange:(NSValue *)range {
    [self subclassResponsibility:_cmd];
    return @"";
}

#pragma mark Command history

- (void)travelInHistoryForward:(BOOL)forward {
    NSString *input = self.historyInputText;
    if (!input)
        return;
    NSString *entry = forward ? [history travelForward] : [history travelBackward:input];
    if (entry)
        [self replaceInputWithHistoryEntry:entry];

    if (entry.length) {
        // Post on the next run loop pass, after AppKit has reported the
        // input text change, so the announcement is the last thing said.
        dispatch_async(dispatch_get_main_queue(), ^{
            SPAnnounce(entry, NSAccessibilityPriorityHigh, nil);
        });
    } else {
        [self announceHistoryEdgeForward:forward];
    }
}

- (NSString *)historyInputText {
    return nil;
}

- (void)replaceInputWithHistoryEntry:(NSString *)entry {
    [self subclassResponsibility:_cmd];
}

- (void)announceHistoryEdgeForward:(BOOL)forward {
    NSString *message;
    if (history.isEmpty)
        message = NSLocalizedString(@"No commands entered", nil);
    else if (forward)
        message = NSLocalizedString(@"End of command history", nil);
    else
        message = NSLocalizedString(@"Start of command history", nil);
    [self.glkctl speakStringNow:message];
}

// Cancel any pending Z-machine menu or form announcement timers, so they
// don't talk over what we are about to speak.
- (void)cancelPendingMenuAnnouncements {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];
}

- (void)speakStatus {
    [self subclassResponsibility:_cmd];
}


- (BOOL)setLastMove {
    [self subclassResponsibility:_cmd];
    return NO;
}

- (void)recalcBackground {
    [self subclassResponsibility:_cmd];
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
