//
//  MapWindowController.h
//  Spatterlight
//
//  Separate OS window for the Glk mapping extension.
//

#import <Cocoa/Cocoa.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface MapFocusRect : NSObject
@property NSInteger left;
@property NSInteger top;
@property NSUInteger width;
@property NSUInteger height;
@end

@interface MapHyperlink : NSObject
@property NSUInteger linkId;
@property (copy, nullable) NSString *label;
@property (strong) NSArray<NSValue *> *points; /* NSPoint in document space, >= 3 */
@end

@interface MapOverlay : NSObject
@property NSUInteger overlayId;
@property (strong, nullable) NSImage *image;
@property (strong, nullable) NSColor *fillColor; /* solid fill when set (image may be nil) */
@property NSInteger left;
@property NSInteger top;
@property NSUInteger width;  /* 0 = natural (image only) */
@property NSUInteger height; /* 0 = natural (image only) */
@property NSUInteger zindex;
@property NSUInteger linkId; /* 0 = not a hotspot */
@property (copy, nullable) NSString *label;
@end

@interface MapWindowController : NSWindowController <NSWindowDelegate>

- (instancetype)initWithGlkController:(GlkController *)glkctl;

@property (weak, nullable) GlkController *glkctl;
@property (readonly) BOOL mapVisible;
@property (readonly) BOOL hasDocument;

- (void)presentSVG:(NSString *)svg
             flags:(NSUInteger)flags
          bgcolor:(NSUInteger)bgcolor
             focus:(nullable MapFocusRect *)focus
       hyperlinks:(NSArray<MapHyperlink *> *)hyperlinks;
- (void)presentImage:(NSImage *)image
               flags:(NSUInteger)flags
            bgcolor:(NSUInteger)bgcolor
               focus:(nullable MapFocusRect *)focus
          hyperlinks:(NSArray<MapHyperlink *> *)hyperlinks;
- (void)setOverlay:(MapOverlay *)overlay;
- (void)moveOverlay:(NSUInteger)overlayId
               left:(NSInteger)left
                top:(NSInteger)top
              width:(NSUInteger)width
             height:(NSUInteger)height
             zindex:(NSUInteger)zindex;
- (void)clearOverlay:(NSUInteger)overlayId;
- (void)clearAllOverlays;
- (void)setHyperlinks:(NSArray<MapHyperlink *> *)hyperlinks;
- (void)closeMap;
- (void)setFocus:(nullable MapFocusRect *)focus;
- (void)showMap;
- (void)hideMap;
- (void)noteHyperlinkActivated:(NSUInteger)linkId;

@end

NS_ASSUME_NONNULL_END
