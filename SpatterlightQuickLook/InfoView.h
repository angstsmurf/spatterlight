//
//  InfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Metadata, Image, NonInterpolatedImage;

NS_ASSUME_NONNULL_BEGIN

@interface InfoView : NSView

@property (readonly) NonInterpolatedImage *imageView;

@property (strong) NSData *imageData;

@property (nullable) NSTextField *leftDateView;
@property (nullable) NSTextField *rightDateView;

- (void)updateWithMetadata:(Metadata *)somedata;
- (void)updateWithDictionary:(NSDictionary *)metadict;

- (void)updateWithImage:(Image *)image;
+ (nullable NSAttributedString *)starString:(NSInteger)rating alignment:(NSTextAlignment)alignment;
+ (NSString *)formattedStringFromDate:(NSDate *)date;
+ (NSView *)addTopConstraintsToView:(NSView *)view;

@end

NS_ASSUME_NONNULL_END
