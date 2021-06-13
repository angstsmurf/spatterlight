//
//  OSFastGraphics.m
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

#import "OSFastGraphics.h"

#pragma mark - Common Macros

#define GREYSCALE(red, green, blue) ((red + green + blue) / (double)3.0)
#define SETBIT(i, k) (i |= ((SInt64)1 << (k)))

#pragma mark - Greyscale 32 x 32

#define INLINE_GREYSCALE(row, col)                  \
    red = *pixels;                                  \
    pixels++;                                       \
    green = *pixels;                                \
    pixels++;                                       \
    blue = *pixels;                                 \
    pixels++;                                       \
    result[row][col] = GREYSCALE(red, green, blue); \
    pixels++; /*alpha*/

#define UNROLL_GREYSCALE_Y(row) \
    INLINE_GREYSCALE(row, 0);   \
    INLINE_GREYSCALE(row, 1);   \
    INLINE_GREYSCALE(row, 2);   \
    INLINE_GREYSCALE(row, 3);   \
    INLINE_GREYSCALE(row, 4);   \
    INLINE_GREYSCALE(row, 5);   \
    INLINE_GREYSCALE(row, 6);   \
    INLINE_GREYSCALE(row, 7);   \
    INLINE_GREYSCALE(row, 8);   \
    INLINE_GREYSCALE(row, 9);   \
    INLINE_GREYSCALE(row, 10);  \
    INLINE_GREYSCALE(row, 11);  \
    INLINE_GREYSCALE(row, 12);  \
    INLINE_GREYSCALE(row, 13);  \
    INLINE_GREYSCALE(row, 14);  \
    INLINE_GREYSCALE(row, 15);  \
    INLINE_GREYSCALE(row, 16);  \
    INLINE_GREYSCALE(row, 17);  \
    INLINE_GREYSCALE(row, 18);  \
    INLINE_GREYSCALE(row, 19);  \
    INLINE_GREYSCALE(row, 20);  \
    INLINE_GREYSCALE(row, 21);  \
    INLINE_GREYSCALE(row, 22);  \
    INLINE_GREYSCALE(row, 23);  \
    INLINE_GREYSCALE(row, 24);  \
    INLINE_GREYSCALE(row, 25);  \
    INLINE_GREYSCALE(row, 26);  \
    INLINE_GREYSCALE(row, 27);  \
    INLINE_GREYSCALE(row, 28);  \
    INLINE_GREYSCALE(row, 29);  \
    INLINE_GREYSCALE(row, 30);  \
    INLINE_GREYSCALE(row, 31);

#define UNROLL_GREYSCALE_X() \
    UNROLL_GREYSCALE_Y(0);   \
    UNROLL_GREYSCALE_Y(1);   \
    UNROLL_GREYSCALE_Y(2);   \
    UNROLL_GREYSCALE_Y(3);   \
    UNROLL_GREYSCALE_Y(4);   \
    UNROLL_GREYSCALE_Y(5);   \
    UNROLL_GREYSCALE_Y(6);   \
    UNROLL_GREYSCALE_Y(7);   \
    UNROLL_GREYSCALE_Y(8);   \
    UNROLL_GREYSCALE_Y(9);   \
    UNROLL_GREYSCALE_Y(10);  \
    UNROLL_GREYSCALE_Y(11);  \
    UNROLL_GREYSCALE_Y(12);  \
    UNROLL_GREYSCALE_Y(13);  \
    UNROLL_GREYSCALE_Y(14);  \
    UNROLL_GREYSCALE_Y(15);  \
    UNROLL_GREYSCALE_Y(16);  \
    UNROLL_GREYSCALE_Y(17);  \
    UNROLL_GREYSCALE_Y(18);  \
    UNROLL_GREYSCALE_Y(19);  \
    UNROLL_GREYSCALE_Y(20);  \
    UNROLL_GREYSCALE_Y(21);  \
    UNROLL_GREYSCALE_Y(22);  \
    UNROLL_GREYSCALE_Y(23);  \
    UNROLL_GREYSCALE_Y(24);  \
    UNROLL_GREYSCALE_Y(25);  \
    UNROLL_GREYSCALE_Y(26);  \
    UNROLL_GREYSCALE_Y(27);  \
    UNROLL_GREYSCALE_Y(28);  \
    UNROLL_GREYSCALE_Y(29);  \
    UNROLL_GREYSCALE_Y(30);  \
    UNROLL_GREYSCALE_Y(31);

inline void greyscale_pixels_rgba_32_32(const unsigned char *pixels, double result[32][32])
{
    unsigned char red, green, blue;
    UNROLL_GREYSCALE_X()
}

#undef UNROLL_GREYSCALE_X
#undef UNROLL_GREYSCALE_Y
#undef INLINE_GREYSCALE

#pragma mark - Fast DCT

#define INLINE_DCT_SUM(I, U, J, V) (sum += CO1[I][U] * CO2[J][V] * pixels[I][J])

#define UNROLL_DCT_J(I, U, V)    \
    INLINE_DCT_SUM(I, U, 0, V);  \
    INLINE_DCT_SUM(I, U, 1, V);  \
    INLINE_DCT_SUM(I, U, 2, V);  \
    INLINE_DCT_SUM(I, U, 3, V);  \
    INLINE_DCT_SUM(I, U, 4, V);  \
    INLINE_DCT_SUM(I, U, 5, V);  \
    INLINE_DCT_SUM(I, U, 6, V);  \
    INLINE_DCT_SUM(I, U, 7, V);  \
    INLINE_DCT_SUM(I, U, 8, V);  \
    INLINE_DCT_SUM(I, U, 9, V);  \
    INLINE_DCT_SUM(I, U, 10, V); \
    INLINE_DCT_SUM(I, U, 11, V); \
    INLINE_DCT_SUM(I, U, 12, V); \
    INLINE_DCT_SUM(I, U, 13, V); \
    INLINE_DCT_SUM(I, U, 14, V); \
    INLINE_DCT_SUM(I, U, 15, V); \
    INLINE_DCT_SUM(I, U, 16, V); \
    INLINE_DCT_SUM(I, U, 17, V); \
    INLINE_DCT_SUM(I, U, 18, V); \
    INLINE_DCT_SUM(I, U, 19, V); \
    INLINE_DCT_SUM(I, U, 20, V); \
    INLINE_DCT_SUM(I, U, 21, V); \
    INLINE_DCT_SUM(I, U, 22, V); \
    INLINE_DCT_SUM(I, U, 23, V); \
    INLINE_DCT_SUM(I, U, 24, V); \
    INLINE_DCT_SUM(I, U, 25, V); \
    INLINE_DCT_SUM(I, U, 26, V); \
    INLINE_DCT_SUM(I, U, 27, V); \
    INLINE_DCT_SUM(I, U, 28, V); \
    INLINE_DCT_SUM(I, U, 29, V); \
    INLINE_DCT_SUM(I, U, 30, V); \
    INLINE_DCT_SUM(I, U, 31, V);

#define UNROLL_DCT_I(U, V)  \
    UNROLL_DCT_J(0, U, V);  \
    UNROLL_DCT_J(1, U, V);  \
    UNROLL_DCT_J(2, U, V);  \
    UNROLL_DCT_J(3, U, V);  \
    UNROLL_DCT_J(4, U, V);  \
    UNROLL_DCT_J(5, U, V);  \
    UNROLL_DCT_J(6, U, V);  \
    UNROLL_DCT_J(7, U, V);  \
    UNROLL_DCT_J(8, U, V);  \
    UNROLL_DCT_J(9, U, V);  \
    UNROLL_DCT_J(10, U, V); \
    UNROLL_DCT_J(11, U, V); \
    UNROLL_DCT_J(12, U, V); \
    UNROLL_DCT_J(13, U, V); \
    UNROLL_DCT_J(14, U, V); \
    UNROLL_DCT_J(15, U, V); \
    UNROLL_DCT_J(16, U, V); \
    UNROLL_DCT_J(17, U, V); \
    UNROLL_DCT_J(18, U, V); \
    UNROLL_DCT_J(19, U, V); \
    UNROLL_DCT_J(20, U, V); \
    UNROLL_DCT_J(21, U, V); \
    UNROLL_DCT_J(22, U, V); \
    UNROLL_DCT_J(23, U, V); \
    UNROLL_DCT_J(24, U, V); \
    UNROLL_DCT_J(25, U, V); \
    UNROLL_DCT_J(26, U, V); \
    UNROLL_DCT_J(27, U, V); \
    UNROLL_DCT_J(28, U, V); \
    UNROLL_DCT_J(29, U, V); \
    UNROLL_DCT_J(30, U, V); \
    UNROLL_DCT_J(31, U, V);

inline void fast_dct_rgba_32_32(const double pixels[32][32], double result[32][32])
{
    static const int N = 32;
    static double c[32] = {0};
    static double CO1[32][32] = {{0}};
    static double CO2[32][32] = {{0}};
    static double CV[32][32] = {{0}};
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      c[0] = 1.0 / sqrt(2.0);
      for (int i = 1; i < N; i++) {
          c[i] = 1.0;
      }
      for (int i = 0; i < N; i++) {
          for (int j = 0; j < N; j++) {
              CO1[i][j] = cos(((2 * i + 1.0) / (2.0 * (double)N)) * j * M_PI);
              CO2[i][j] = cos(((2 * i + 1.0) / (2.0 * (double)N)) * j * M_PI);
              CV[i][j] = (c[i] * c[j]) / sqrt(2.0 * N);
          }
      }
    });
    for (int u = 0; u < N; u++) {
        for (int v = 0; v < N; v++) {
            double sum = 0.0;
            UNROLL_DCT_I(u, v)
            sum *= CV[u][v];
            result[u][v] = sum;
        }
    }
}

#undef UNROLL_DCT_J
#undef UNROLL_DCT_I
#undef INLINE_DCT_SUM

#pragma mark - Average Matrix Value

#define INLINE_AVERAGE(row, col) \
    dctSum += pixels[row][col];

#define UNROLL_AVERAGE_Y(row) \
    INLINE_AVERAGE(row, 0);   \
    INLINE_AVERAGE(row, 1);   \
    INLINE_AVERAGE(row, 2);   \
    INLINE_AVERAGE(row, 3);   \
    INLINE_AVERAGE(row, 4);   \
    INLINE_AVERAGE(row, 5);   \
    INLINE_AVERAGE(row, 6);   \
    INLINE_AVERAGE(row, 7);

#define UNROLL_AVERAGE_X()  \
    UNROLL_AVERAGE_Y(0);    \
    UNROLL_AVERAGE_Y(1);    \
    UNROLL_AVERAGE_Y(2);    \
    UNROLL_AVERAGE_Y(3);    \
    UNROLL_AVERAGE_Y(4);    \
    UNROLL_AVERAGE_Y(5);    \
    UNROLL_AVERAGE_Y(6);    \
    UNROLL_AVERAGE_Y(7);    \
    dctSum -= pixels[0][0]; \
    dctAverage = dctSum / (double)63.0;

inline double fast_avg_no_first_el_rgba_8_8(const double pixels[32][32])
{
    double dctSum = 0.0, dctAverage = 0.0;
    UNROLL_AVERAGE_X()
    return dctAverage;
}

#undef UNROLL_AVERAGE_X
#undef UNROLL_AVERAGE_Y
#undef INLINE_AVERAGE

#pragma mark - pHash

#define INLINE_PHASH(row, col)                                   \
    if (row != 0 && col != 0 && pixels[row][col] > dctAverage) { \
        SETBIT(result, cnt);                                     \
    }                                                            \
    cnt++;

#define UNROLL_PHASH_Y(row) \
    INLINE_PHASH(row, 0);   \
    INLINE_PHASH(row, 1);   \
    INLINE_PHASH(row, 2);   \
    INLINE_PHASH(row, 3);   \
    INLINE_PHASH(row, 4);   \
    INLINE_PHASH(row, 5);   \
    INLINE_PHASH(row, 6);   \
    INLINE_PHASH(row, 7);

#define UNROLL_PHASH_X() \
    UNROLL_PHASH_Y(0);   \
    UNROLL_PHASH_Y(1);   \
    UNROLL_PHASH_Y(2);   \
    UNROLL_PHASH_Y(3);   \
    UNROLL_PHASH_Y(4);   \
    UNROLL_PHASH_Y(5);   \
    UNROLL_PHASH_Y(6);   \
    UNROLL_PHASH_Y(7);

inline SInt64 phash_rgba_8_8(const double pixels[32][32], double dctAverage)
{
    unsigned char cnt = 0;
    SInt64 result = 0;
    UNROLL_PHASH_X()
    return result;
}

#undef UNROLL_PHASH_X
#undef UNROLL_PHASH_Y
#undef INLINE_PHASH

#undef GREYSCALE
#undef SETBIT
