//
//  loading_screen.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Show a ZX Spectrum loading screen (SCREEN$) as a title image,
//  extracted from a .z80 snapshot or a .tap / .tzx tape image.
//

#ifndef loading_screen_h
#define loading_screen_h

#include <stdint.h>
#include <stddef.h>

#define ZX_SCREEN_SIZE 6912

/* Owns a malloc'd 6912-byte SCREEN$, or NULL. Captured by ProcessFile()
   during loading, consumed (and freed) by DrawZXTitleImage(). */
extern uint8_t *ZXLoadingScreen;

/* Locate a standard 6912-byte loading screen in a raw .tap (is_tzx == 0)
   or .tzx (is_tzx != 0) image. Returns a malloc'd 6912-byte buffer
   (caller frees) or NULL if none is found. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx);

/* True if a 6912-byte SCREEN$ is just black text on a white background
   (a snapshot taken at a BASIC/text prompt), i.e. not worth showing. */
int ZXScreenIsBlackOnWhite(const uint8_t *scr);

/* Show ZXLoadingScreen as a title image, wait for a keypress, then
   restore the normal game windows. Frees ZXLoadingScreen, sets it NULL. */
void DrawZXTitleImage(void);

#endif /* loading_screen_h */
