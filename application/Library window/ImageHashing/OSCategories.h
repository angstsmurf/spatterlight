//
//  OSCategories.h
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

#import <Foundation/Foundation.h>

#pragma mark - NSData Category

NS_ASSUME_NONNULL_BEGIN

@interface NSData (CococaImageHashing)

- (nullable NSData *)RGBABitmapDataForResizedImageWithWidth:(NSUInteger)width
                                                  andHeight:(NSUInteger)height;

@end

NS_ASSUME_NONNULL_END

#pragma mark - NSBitmapImagerep Category


NS_ASSUME_NONNULL_BEGIN

@interface NSBitmapImageRep (CocoaImageHashing)

+ (NSBitmapImageRep *)imageRepFrom:(NSBitmapImageRep *)sourceImageRep
                     scaledToWidth:(NSUInteger)width
                    scaledToHeight:(NSUInteger)height
                usingInterpolation:(NSImageInterpolation)imageInterpolation;

@end

NS_ASSUME_NONNULL_END
