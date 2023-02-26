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
@property (strong) IBOutlet NSTextView *sampleTextView;

- (CGFloat)calculateHeight;

@end

NS_ASSUME_NONNULL_END
