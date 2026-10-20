//
//  PreviewController.h
//  Spatterlight
//
//  Created by Administrator on 2023-02-25.
//

#import <Cocoa/Cocoa.h>

@class Theme;

NS_ASSUME_NONNULL_BEGIN

@interface PreviewController : NSViewController

@property (weak) Theme *theme;
@property (weak) IBOutlet NSLayoutConstraint *textHeight;

/// The sample text the preview shows for a theme. Needs no view.
+ (NSAttributedString *)attributedPreviewForTheme:(Theme *)theme;

- (CGFloat)calculateHeight;
- (void)updatePreviewText;
- (void)fixScrollBar;
- (void)scrollToTop;

@end

NS_ASSUME_NONNULL_END
