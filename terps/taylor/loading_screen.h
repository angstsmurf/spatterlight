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

#include "decompressz80.h"  /* ZX_SCREEN_SIZE and the SCREEN$ decode helpers */

/* Owns a malloc'd 6912-byte SCREEN$, or NULL. Captured by ProcessFile()
   during loading, consumed (and freed) by DrawZXTitleImage(). */
extern uint8_t *ZXLoadingScreen;

/* Locate a standard 6912-byte loading screen in a raw .tap (is_tzx == 0)
   or .tzx (is_tzx != 0) image. Returns a malloc'd 6912-byte buffer
   (caller frees) or NULL if none is found. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx);

/* Reconstruct an Alkatraz-drawn loading screen (e.g. Kayleth, Terraquake)
   from the raw TZX image, using the game's window descriptors and stream
   parameters. Returns a malloc'd 6912-byte SCREEN$ (caller frees) or NULL. */
uint8_t *DecodeAlkatrazLoadingScreen(uint8_t *image, size_t length,
    const uint8_t *descriptors, int num_windows, int block_index, int offset,
    uint8_t key_seed, uint8_t key_step, uint8_t attr_fill);

/* Record the order the loader drew the screen in, so the title can be
   revealed slowly in that same sequence (see DrawZXTitleImage). Callers that
   decode the screen themselves (e.g. Temple of Terror) use this to pass the
   order along. */
void ZXSetDrawOrder(const uint16_t *order, int count);

/* Show ZXLoadingScreen as a title image, wait for a keypress, then
   restore the normal game windows. Frees ZXLoadingScreen, sets it NULL. */
void DrawZXTitleImage(void);

#endif /* loading_screen_h */
