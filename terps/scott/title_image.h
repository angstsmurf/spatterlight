//
//  title_image.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#ifndef title_image_h
#define title_image_h

#include <stdint.h>
#include <stddef.h>

#define ZX_SCREEN_SIZE 6912

void DrawTitleImageScott(void);
void PrintTitleScreenBuffer(void);
void PrintTitleScreenGrid(void);

/* Locate a standard 6912-byte ZX Spectrum loading screen (SCREEN$) in a
   raw .tap (is_tzx == 0) or .tzx (is_tzx != 0) image. Returns a malloc'd
   6912-byte buffer (caller frees) or NULL if none is found. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx);

/* True if a 6912-byte SCREEN$ is just black text on a white background
   (a snapshot taken at a BASIC/text prompt), i.e. not worth showing. */
int ZXScreenIsBlackOnWhite(const uint8_t *scr);

/* Show the captured ZXLoadingScreen as a title image, then restore the
   normal game windows. Frees ZXLoadingScreen and sets it to NULL. */
void DrawZXTitleImage(void);

#endif /* title_image_h */
