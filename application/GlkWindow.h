#import <AppKit/AppKit.h>

@class GlkController, Theme, GlkStyle, ZColor, InputTextField, InputHistory;

struct fillrect;

typedef NS_ENUM(int32_t, kSaveTextFormatType) {
    kRTF,
    kRTFD,
    kPlainText,
};

@interface GlkWindow : NSView <NSSecureCoding> {

    ZColor *currentZColor;

    // An array of attribute dictionaries, with
    // style hints applied if the "use hints"
    // option is on for this theme
    NSMutableArray *styles;

    BOOL char_request;
    BOOL line_request;   // Not every window type supports every request,
    BOOL hyper_request;  // but they all archive all four.
    BOOL mouse_request;
    BOOL dirty;

    BOOL usingStyles;
    BOOL underlineLinks;

    /* For command history */
    InputHistory *history;

    /* Keeps track of which previous move to speak */
    NSUInteger moveRangeIndex;

    NSInteger bgnd;
}

@property(readonly) NSInteger name;
@property(weak) GlkController *glkctl;

@property BOOL currentReverseVideo;
@property NSInteger currentHyperlink;

@property NSArray *styleHints;
@property Theme *theme;

@property NSMutableDictionary *pendingTerminators;
@property NSMutableDictionary *currentTerminators;
@property BOOL terminatorsPending;
@property BOOL framePending;
@property NSRect pendingFrame;

@property InputTextField *input;

/// The text view of a grid or buffer window, nil for a graphics window.
@property (readonly) NSTextView *textview;

// A list of ranges of previous moves
@property NSMutableArray<NSValue *> *moveRanges;

- (instancetype)initWithGlkController:(GlkController *)glkctl
                                 name:(NSInteger)name;

- (BOOL)getStyleVal:(NSUInteger)style
               hint:(NSUInteger)hint
              value:(NSInteger *)value;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL wantsFocus;
- (void)grabFocus;
// The Glk keycode of the last character of evt (keycode_Unknown if none).
// With mapNumericPad, digits typed on the numeric keypad become keycode_Pad0-9.
+ (unsigned)keycodeForEvent:(NSEvent *)evt mapNumericPad:(BOOL)mapNumericPad;
// Unless this window wants focus, hand evt to the first other window that
// does, giving it focus. Returns YES if the key press was passed on.
- (BOOL)forwardKeyDownToFocusedWindow:(NSEvent *)evt;
- (void)flushDisplay;
- (void)markLastSeen;
- (void)performScroll;
- (void)makeTransparent;
- (void)setBgColor:(NSInteger)bc;
- (void)clear;
- (void)putString:(NSString *)buf style:(NSUInteger)style;
- (NSUInteger)unputString:(NSString *)buf;
- (void)moveToColumn:(NSUInteger)x row:(NSUInteger)y;
- (void)initLine:(NSString *)buf maxLength:(NSUInteger)maxLength;
- (void)initChar;
- (void)cancelChar;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *cancelLine;
- (void)initMouse;
- (void)cancelMouse;
- (void)initHyperlink;
- (void)cancelHyperlink;
- (void)recalcBackground;

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg;
- (NSMutableAttributedString *)applyZColorsAndThenReverse:(NSMutableAttributedString *)attStr;
- (NSMutableAttributedString *)applyReverseOnly:(NSMutableAttributedString *)attStr;

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict background:(NSColor *)backCol;
/// The theme's style for this kind of window: nil here, overridden by the
/// text grid and buffer windows.
- (GlkStyle *)themeStyle:(NSUInteger)style;
/// The theme's background for this kind of window, the colour reverse video
/// swaps in: the buffer background here, overridden by the text grid window.
@property (readonly) NSColor *themeBackground;
/// The attributes of a style in the current theme: with the game's style
/// hints applied if the theme does styles, and as the theme has them if not.
- (NSDictionary *)themeAttributesForStyle:(NSUInteger)style;
/// The theme's link underline style for this kind of window: none here,
/// overridden by the text grid and buffer windows.
@property (readonly) int32_t themeLinkStyle;
/// Brings usingStyles and underlineLinks up to date with the theme, and
/// tells whether either changed: all the text must be restyled then.
- (BOOL)syncStyleFlagsWithTheme;
/// Links are drawn in the Normal style's colour, underlined as the theme says.
- (void)updateLinkTextAttributesOf:(NSTextView *)textView;
- (NSMutableDictionary *)getCurrentAttributesForStyle:(NSUInteger)stylevalue;
/// Style-table attributes only (theme +/- stylehints). Does not fold zcolor or reverse video.
- (NSDictionary *)baseAttributesForStyle:(NSUInteger)stylevalue;

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)n;
- (void)drawImage:(NSImage *)buf
             val1:(NSInteger)v1
             val2:(NSInteger)v2
            width:(NSInteger)w
           height:(NSInteger)h
        imagerule:(NSUInteger)imagerule
         maxwidth:(NSUInteger)maxwidth
            style:(NSUInteger)style;
- (void)flowBreak;
- (void)prefsDidChange;
- (void)terpDidStop;
+ (NSArray *)deepCopyOfStyleHintsArray:(NSArray *)array;
- (void)postRestoreAdjustments:(GlkWindow *)win;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasCharRequest;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasLineRequest;

- (void)sendCommandLine:(NSString *)command;
- (void)sendKeypress:(unsigned)ch;
- (void)sendMouseEventAtPoint:(NSPoint)point;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *links;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *images;

- (void)repeatLastMove:(id)sender;
// Step back or forward through moveRanges and speak that move, saying so
// when there is none further.
- (void)speakPrevious;
- (void)speakNext;
// The text of a move, as speakPrevious and speakNext speak it.
- (NSString *)speakableStringForMoveRange:(NSValue *)range;
- (void)speakStatus;
// Put the previous (or next) history line into the input and speak it, or
// speak "No commands entered", or that history travel hit its start
// (backward) or end (forward).
- (void)travelInHistoryForward:(BOOL)forward;
// Overridden by windows that take line input: the text being edited, or nil
// when there is no input to travel in.
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *historyInputText;
- (void)replaceInputWithHistoryEntry:(NSString *)entry;
- (void)announceHistoryEdgeForward:(BOOL)forward;
// Cancel pending Z-machine menu or form announcements before speaking.
- (void)cancelPendingMenuAnnouncements;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL setLastMove;
- (void)adjustBZTerminators:(NSMutableDictionary *)terminators;

- (NSView *)saveScrollbackAccessoryViewHasImages:(BOOL)hasImages;
- (void)selectFormat:(id)sender;

- (IBAction)saveAsRTF:(id)sender;

@property NSSavePanel *savePanel;
@property NSPopUpButton *accessoryPopUp;

@end
