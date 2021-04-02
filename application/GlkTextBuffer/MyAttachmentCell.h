//
//  MyAttachmentCell.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// I suppose this is necessary to get rid of that ugly Markup menu on attached
// images.

@interface MyAttachmentCell : NSTextAttachmentCell <NSSecureCoding>

@property (weak) NSAttributedString *attrstr;

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos;

@end

NS_ASSUME_NONNULL_END
