#import <AppKit/AppKit.h>

@class GlkController, Theme, ZColor, InputTextField, InputHistory;

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
/** CSS Basic span hints copied from the controller when the window opens. */
@property NSArray<NSDictionary *> *cssSpanHints;
@property NSArray<NSDictionary *> *cssParaHints;
@property NSArray<NSDictionary *> *cssHyperlinkHints;
/** CSS_Window hints copied at window open. */
@property NSMutableDictionary<NSString *, NSString *> *cssWindowHints;
/** CSS_Input hints copied at window open. */
@property NSMutableDictionary<NSString *, NSString *> *cssInputHints;
/** CSS_Image hints copied at window open. */
@property NSMutableDictionary<NSString *, NSString *> *cssImageHints;
/** YES when cssInputHints request border-style:solid and doStyles is on. */
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL cssInputWantsSolidBorder;
/** Effective CSS_Image border-style for the next drawn image (inline overrides hint). */
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL cssImageWantsSolidBorderForNextDraw;
@property NSMutableDictionary<NSString *, NSString *> *currentInlineCSS;
@property NSMutableDictionary<NSString *, NSString *> *currentInlineParaCSS;
@property NSMutableDictionary<NSString *, NSString *> *currentInlineHyperlinkCSS;
/** Inline CSS_Input props (override snapshotted cssInputHints while set). */
@property NSMutableDictionary<NSString *, NSString *> *currentInlineInputCSS;
/** Inline CSS_Image props (apply to the next drawn image). */
@property NSMutableDictionary<NSString *, NSString *> *currentInlineImageCSS;
@property Theme *theme;

@property NSMutableDictionary *pendingTerminators;
@property NSMutableDictionary *currentTerminators;
@property BOOL terminatorsPending;
@property BOOL framePending;
@property NSRect pendingFrame;

@property InputTextField *input;

// A list of ranges of previous moves
@property NSMutableArray<NSValue *> *moveRanges;

- (instancetype)initWithGlkController:(GlkController *)glkctl
                                 name:(NSInteger)name;

- (BOOL)getStyleVal:(NSUInteger)style
               hint:(NSUInteger)hint
              value:(NSInteger *)value;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL wantsFocus;
- (void)grabFocus;
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
- (NSMutableDictionary *)getCurrentAttributesForStyle:(NSUInteger)stylevalue;
/// Remove glyph-run NSBackgroundColor from newline characters. AppKit extends
/// that attribute to the end of the line fragment for '\\n', which makes span
/// reverse/background look like a content-box fill. Paragraph GlkParaBackground
/// is left alone (block fills are intentional).
- (void)stripSpanBackgroundFromNewlines:(NSMutableAttributedString *)attStr;
/// Style-table attributes only (theme +/- stylehints). Does not fold zcolor or reverse video.
- (NSDictionary *)baseAttributesForStyle:(NSUInteger)stylevalue;
/// Theme +/- stylehints, plus CSS_Window inheritance and per-style CSS span/para hints
/// when doStyles is on. Used by style_measure for computed colours.
- (NSMutableDictionary *)computedStyleAttributesForStyle:(NSUInteger)stylevalue;
/// Effective CSS_Window background-color (resolves -iftf-reverse-video). Nil if unset.
- (nullable NSColor *)effectiveCssWindowBackgroundColor;
/// Apply CSS_Window chrome (border-style) to this view when doStyles is on.
- (void)applyCSSWindowChrome;
/// Apply window CSS span/para hints onto a mutable attributes dictionary when doStyles is on.
- (void)applyCSSHintsToAttributes:(NSMutableDictionary *)attributes
                         forStyle:(NSUInteger)stylevalue
                       reverseOut:(nullable BOOL *)reverseOut;
/// Re-apply a preserved @"GlkCSS" property map (and optional reverse) onto attributes when doStyles is on.
- (void)applyPreservedInlineCSS:(NSDictionary *)css
                   toAttributes:(NSMutableDictionary *)attributes;
- (void)applyPreservedInlineCSS:(NSDictionary *)css
                   toAttributes:(NSMutableDictionary *)attributes
                 allowParagraph:(BOOL)allowParagraph;

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
/** Re-draw live CSS_Input chrome (e.g. border) after inline Input CSS changes. */
- (void)refreshCSSInputChrome;

- (void)sendCommandLine:(NSString *)command;
- (void)sendKeypress:(unsigned)ch;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *links;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *images;

- (void)repeatLastMove:(id)sender;
- (void)speakPrevious;
- (void)speakNext;
- (void)speakStatus;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL setLastMove;
- (void)adjustBZTerminators:(NSMutableDictionary *)terminators;

- (NSView *)saveScrollbackAccessoryViewHasImages:(BOOL)hasImages;
- (void)selectFormat:(id)sender;

- (IBAction)saveAsRTF:(id)sender;

@property NSSavePanel *savePanel;
@property NSPopUpButton *accessoryPopUp;

@end
