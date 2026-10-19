//
//  SubImage.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-07.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// An image drawn into a graphics window, exposed to VoiceOver as a child of
// that window. frameRect is in the window's (bottom-left origin) coordinates.
@interface SubImage : NSAccessibilityElement

+ (instancetype)imageWithLabel:(nullable NSString *)label parent:(id)parent frame:(NSRect)frameRect;

@property NSRect frameRect;
@property NSString *uuid;

@end

NS_ASSUME_NONNULL_END
