@class GlkController, Theme, ZColor, InputTextField, InputHistory;

struct fillrect;

@interface GlkWindow : NSView <NSSecureCoding> {

    ZColor *currentZColor;

    // An array of attribute dictionaries, with
    // style hints applied if the "use hints"
    // option is on for this theme
    NSMutableArray *styles;

    BOOL char_request;
    BOOL dirty;

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

// A list of ranges of previous moves
@property NSMutableArray<NSValue *> *moveRanges;

- (instancetype)initWithGlkController:(GlkController *)glkctl
                                 name:(NSInteger)name;

- (BOOL)getStyleVal:(NSUInteger)style
               hint:(NSUInteger)hint
              value:(NSInteger *)value;

- (BOOL)wantsFocus;
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
- (NSString *)cancelLine;
- (void)initMouse;
- (void)cancelMouse;
- (void)initHyperlink;
- (void)cancelHyperlink;

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg;

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict background:(NSColor *)backCol;

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)n;
- (void)drawImage:(NSImage *)buf
             val1:(NSInteger)v1
             val2:(NSInteger)v2
            width:(NSInteger)w
           height:(NSInteger)h
            style:(NSUInteger)style;
- (void)flowBreak;
- (void)prefsDidChange;
- (void)terpDidStop;
- (NSArray *)deepCopyOfStyleHintsArray:(NSArray *)array;
- (void)postRestoreAdjustments:(GlkWindow *)win;

- (BOOL)hasCharRequest;

- (NSArray *)links;
- (NSArray *)images;

- (void)repeatLastMove:(id)sender;;
- (void)speakPrevious;
- (void)speakNext;
- (BOOL)setLastMove;
- (void)adjustBZTerminators:(NSMutableDictionary *)terminators;

@end
