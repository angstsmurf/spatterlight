//
//  InfoView.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-09.
//

#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>

@class Metadata, Image, NonInterpolatedImage;

@interface InfoView : NSView

@property (readonly) NonInterpolatedImage *imageView;

@property (strong) NSData *imageData;

@property NSAttributedString *starString;


- (void)updateWithMetadata:(Metadata *)somedata;
- (void)updateWithImage:(Image *)image;

@end
