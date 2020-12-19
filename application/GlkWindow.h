@class GlkController, Theme, ZColor, InputTextField;

struct fillrect;

@interface GlkWindow : NSView <NSSecureCoding> {

    ZColor *currentZColor;

    // An array of attribute dictionaries, with
    // style hints applied if the "use hints"
    // option is on for this theme
    NSMutableArray *styles;

    BOOL char_request;
    BOOL dirty;

    NSInteger bgnd;
}

@property(readonly) NSInteger name;
@property GlkController *glkctl;

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
- (void)unputString:(NSString *)buf;
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

@end
