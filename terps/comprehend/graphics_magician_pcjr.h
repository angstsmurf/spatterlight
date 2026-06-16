/* graphics_magician_pcjr.h -- DOS (IBM PCjr / Tandy 16-colour) Graphics Magician
 * picture renderer.
 *
 * The PCjr counterpart of graphics_magician_cga.cpp.  Both decode the same
 * Penguin Graphics Magician vector stream (identical to the Apple II and CGA
 * renderers); only the pixel model and drawing tables differ.  Used by the
 * IBM PC/PCjr releases of Transylvania (v1) and The Crimson Crown when run on
 * a PCjr/Tandy: the original NOVEL.EXE checks the BIOS model byte
 * (F000:FFFE == 0xFD) and, on a PCjr, loads JR_GRAPH.OVR instead of
 * PC_GRAPH.OVR and drives BIOS video mode 9 (320x200, 16 colours).
 *
 * Pixel model (reverse-engineered from JR_GRAPH.OVR + NOVEL.EXE in Ghidra):
 *   - BIOS mode 9, 320x200, 16 colours, 4 bits/pixel, 2 pixels/byte
 *     (the leftmost pixel of a byte is its HIGH nibble).
 *   - 4-bank interleave: VRAM offset = (y & 3) * 0x2000 + 160 * (y >> 2) +
 *     ((x + 20) >> 1); the picture window is offset +20 px, so the logical
 *     picture is the same 280x160 as the CGA/Apple renderers.
 *   - Pen plot (jr_plot_pixel @ OVR 0x0e): pen index p (0-7 after & 7) selects
 *     a colour nibble from colLow[]/colHigh[] and is OR'd into the target
 *     nibble of the existing byte (read-modify-write).
 *   - Fills: 30 entries x 4 bytes of 16-colour dither, selected per op6 byte
 *     through a 108-entry subindex (one pattern index per row parity), exactly
 *     like the v1 CGA path but with 4bpp tiles.
 *   - Brush bitmaps and the in-picture font are mode-independent masks, shared
 *     byte-for-byte with the CGA renderer (NOVEL.EXE brushes, CHARSET.GDA font).
 *   - 16-colour output uses the standard PCjr/CGA RGBI palette (the game
 *     programs no custom palette).
 */

#ifndef GLK_COMPREHEND_GRAPHICS_MAGICIAN_PCJR_H
#define GLK_COMPREHEND_GRAPHICS_MAGICIAN_PCJR_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Install the PCjr drawing tables.  `ovr` is the JR_GRAPH.OVR image and `exe`
// the NOVEL.EXE / NOVEL1.EXE image, both already read into memory.  The pen
// colour tables, nibble masks, 4bpp fill-pattern table and subindex are located
// in the overlay by signature; the brush bitmaps come from the EXE (same
// brush-5 signature the CGA loader uses).  Returns true on success and sets
// gmpcjrHaveDrawingTables().
bool gmpcjrInstallDrawingTables(const uint8_t *ovr, size_t ovrSize,
                                const uint8_t *exe, size_t exeSize);

// Load the in-picture op3/op5 font from a CHARSET.GDA image (version word
// 0x1100, then 96 glyphs of 8 bytes each at offset 4, stored LSB-first).  Same
// file and format as the v1 CGA renderer.  Call after
// gmpcjrInstallDrawingTables.  Returns true on success.
bool gmpcjrSetFont(const uint8_t *charsetGda, size_t size);

// True after a successful gmpcjrInstallDrawingTables() call.
bool gmpcjrHaveDrawingTables();

// Clear the 320x200 (16-colour) framebuffer.  Pass white=true for a room reset
// (white background), false for a dark/transparent base.
void gmpcjrResetScreen(bool white);

// Run one image's vector-opcode stream onto the framebuffer.
void gmpcjrDrawImage(const uint8_t *data, size_t size);

// Convert the framebuffer's 280x160 picture window to RGBA (0xRRGGBBAA) into
// out[w*h], scaling the logical page to the requested dimensions.
void gmpcjrBlitToSurface(uint32_t *out, int w, int h);

// ---- Slow ("animated") draw -------------------------------------------------
// One-for-one with the gm*/gmcga* slow-draw APIs: when enabled, gmpcjrDrawImage
// records the order its bytes are plotted so the picture can be revealed
// progressively, and the host drives the reveal on a timer.
void gmpcjrSetSlowDraw(bool on);
bool gmpcjrSlowDrawActive();
bool gmpcjrAdvanceSlowDraw(int budget);
void gmpcjrFinishSlowDraw();
bool gmpcjrSlowConsumeDirty(int *y0, int *y1);
void gmpcjrBlitSlowToSurface(uint32_t *out, int w, int h);

} // namespace Comprehend
} // namespace Glk

#endif
