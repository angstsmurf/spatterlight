//
//  ParagraphPopOver.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-02.
//

#import <Cocoa/Cocoa.h>

@class GlkStyle;

NS_ASSUME_NONNULL_BEGIN

@interface ParagraphPopOver : NSViewController

- (void)refreshForStyle:(GlkStyle *)style;

@end

NS_ASSUME_NONNULL_END
