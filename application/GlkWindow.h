@class GlkController, GlkHyperlink, Theme, ZColor, ZReverseVideo;

@interface GlkWindow : NSView {

    NSMutableArray *hyperlinks;
    GlkHyperlink *currentHyperlink;

    ZColor *currentZColor;
    BOOL currentReverseVideo;

    NSMutableDictionary *currentTerminators;

    // An array of attribute dictionaries, with
    // style hints applied if the "use hints"
    // option is on for this theme
    NSMutableArray *styles;

    BOOL char_request;

    NSInteger bgnd;
}

@property(readonly) NSInteger name;
@property GlkController *glkctl;

@property NSArray *styleHints;
@property NSMutableDictionary *pendingTerminators;
@property BOOL terminatorsPending;
@property Theme *theme;

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
- (void)initLine:(NSString *)buf;
- (void)initChar;
- (void)cancelChar;
- (NSString *)cancelLine;
- (void)initMouse;
- (void)cancelMouse;
- (void)setHyperlink:(NSUInteger)linkid;
- (void)initHyperlink;
- (void)cancelHyperlink;

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg;
- (void)setReverseVideo:(BOOL)reverse;

- (NSMutableDictionary *)reversedAttributes:(NSMutableDictionary *)dict background:(NSColor *)backCol;

- (void)fillRects:(struct fillrect *)rects count:(NSInteger)n;
- (void)drawImage:(NSImage *)buf
             val1:(NSInteger)v1
             val2:(NSInteger)v2
            width:(NSInteger)w
           height:(NSInteger)h;
- (void)flowBreak;
- (void)prefsDidChange;
- (void)terpDidStop;
- (NSArray *)deepCopyOfStyleHintsArray:(NSArray *)array;

- (void)restoreSelection;

@end
