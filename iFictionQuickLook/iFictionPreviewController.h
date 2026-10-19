//
//  iFictionPreviewController.h
//  iFictionQuickLook
//
//  Created by Administrator on 2021-02-15.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Fills a text view with a pretty-printed, syntax-coloured copy of an
// .iFiction file. Used by PreviewViewController, which owns the view.
@interface iFictionPreviewController : NSObject

@property NSTextView *textview;

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler;

// Applies the colouring rules to an already pretty-printed XML string.
+ (NSAttributedString *)coloredXMLString:(NSString *)xml attributes:(NSDictionary<NSAttributedStringKey, id> *)attributes;

@end

NS_ASSUME_NONNULL_END
