// DOS (IBM PCjr / Tandy 16-colour) Graphics Magician picture renderer, the counterpart of
// graphics_magician_cga.cpp for the same vector stream. Used by the Transylvania (v1) and Crimson
// Crown releases when run on a PCjr/Tandy: NOVEL.EXE checks the BIOS model byte (F000:FFFE == 0xFD)
// and on a PCjr loads JR_GRAPH.OVR instead of PC_GRAPH.OVR, using BIOS video mode 9.
//
// Pixel model, reverse-engineered from JR_GRAPH.OVR and NOVEL.EXE:
//   - 320x200, 16 colours, 2 pixels/byte; the leftmost pixel of a byte is its high nibble.
//   - 4-bank interleave: VRAM offset = (y & 3) * 0x2000 + 160 * (y >> 2) + ((x + 20) >> 1). The
//     picture window is offset +20 px, so the logical picture is 280x160 as in CGA/Apple.
//   - Pen plot: pen index (& 7) selects a colour from colLow[]/colHigh[], OR'd into the target
//     nibble of the existing byte.
//   - Fills: 30 entries x 4 bytes of 16-colour dither, selected per op6 byte through a 108-entry
//     subindex (one pattern index per row parity).
//   - Brush bitmaps and the font are mode-independent masks shared with the CGA renderer.

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

// Slow ("animated") draw, one-for-one with the gm*/gmcga* APIs: when enabled, gmpcjrDrawImage
// records the order its bytes are plotted, and the host drives a progressive reveal on a timer.
void gmpcjrSetSlowDraw(bool on);
bool gmpcjrSlowDrawActive();
bool gmpcjrAdvanceSlowDraw(int budget);
void gmpcjrFinishSlowDraw();
bool gmpcjrSlowConsumeDirty(int *y0, int *y1);
void gmpcjrBlitSlowToSurface(uint32_t *out, int w, int h);

} // namespace Comprehend
} // namespace Glk

#endif
