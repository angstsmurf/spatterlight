//
//  OSFastGraphics.h
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark - Matrix Greyscale Mapping

void greyscale_pixels_rgba_32_32(const unsigned char *pixels, double result[_Nonnull 32][32]);

#pragma mark - DCT

void fast_dct_rgba_32_32(const double pixels[_Nonnull 32][32], double result[_Nonnull 32][32]);

double avg_greyscale_value_rgba_8_8(const unsigned char *pixels);
double fast_avg_no_first_el_rgba_8_8(const double pixels[_Nonnull 32][32]);

#pragma mark - Perceptual Hashes

SInt64 phash_rgba_8_8(const double pixels[_Nonnull 32][32], double dctAverage);

NS_ASSUME_NONNULL_END
