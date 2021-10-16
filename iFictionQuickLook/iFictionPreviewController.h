//
//  iFictionPreviewController.h
//  iFictionQuickLook
//
//  Created by Administrator on 2021-02-15.
//

#import <Cocoa/Cocoa.h>

@class UKSyntaxColor;

NS_ASSUME_NONNULL_BEGIN

@interface iFictionPreviewController : NSViewController

@property UKSyntaxColor * syntaxColorer;

@property NSTextView * textview;

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler;

@end

NS_ASSUME_NONNULL_END
