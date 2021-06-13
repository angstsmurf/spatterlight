//
//  OSImageHashing.h
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

NS_ASSUME_NONNULL_BEGIN

@interface OSImageHashing : NSObject

+ (instancetype)sharedInstance;

- (BOOL)compareImageData:(NSData *)leftHandImageData
                      to:(NSData *)rightHandImageData;

@end

NS_ASSUME_NONNULL_END
