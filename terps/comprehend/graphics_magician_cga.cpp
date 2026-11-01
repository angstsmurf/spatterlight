// DOS (CGA) Graphics Magician picture renderer, for the same vector stream as the Apple II
// renderer in graphics_magician.cpp. It renders into a 280x160, 2-bpp logical raster matching
// the picture interpreter in NOVEL.EXE, which runs CGA 320x200 palette 1 at low intensity.
// The model was derived from disassembly of the DOS interpreters.
//
// Pixels: 4 per byte, LSB-first (pixel p in bits (2p+1):(2p)), with values 0=black, 1=cyan,
// 2=magenta, 3=grey. Drawing tables in the v2 interpreters (byte-identical in all of them):
//   - Fill: 76 entries x 2 bytes (even/odd column, always equal), each a 4-pixel 2-bpp tile
//     chosen through the SET_FILL_COLOR (op6) subindex table.
//   - Brushes: 8 x 32 bytes; quadrants Q0..Q3 = upper-left, lower-left, upper-right,
//     lower-right; bit 7 = leftmost pixel.
//   - Font: 96 chars x 8 bytes, same bit order as the brushes.

#include "graphics_magician_cga.h"
#include "graphics_magician.h"         // Opcode enum
#include "graphics_magician_vector.h"  // shared line/circle rasterizers
#include "gm_stream.h"                 // shared opcode dispatcher
#include "gm_span_fill.h"              // shared op14 flood fill
#include "slow_draw_page.h"            // shared progressive-reveal helper
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Glk {
namespace Comprehend {

// Drawing tables

static uint8_t s_fillTable[76];       // pattern[idx] = 2-bpp byte (4-pixel tile), LSB-first
static uint8_t s_fillTableRaw[76];    // same, in the native MSB-first bit order
static uint8_t s_fillSubindex[512];   // SET_FILL_COLOR byte -> (even,odd) pattern idx
static uint8_t s_brushBitmaps[256];   // 8 brushes × 32 bytes
static uint8_t s_fontGlyphs[96 * 8];  // chars 32-127, 8 bytes each

// v1 (Crimson Crown / Transylvania v1) fill patterns: 30 entries of four bytes, which the
// native word painter reads as pattern[idx * 4 + (byte column & 3)]. The fill only paints words
// at even byte offsets, so phases 1 and 3 are never reached, and the dithers ship with
// phase 0 == phase 2 (idx13 "40 80 40 80"). Dithers needing a half-byte shift are separate
// entries (12/13, 16/17) chosen by the subindex, so reading entry[0] matches the native fill.
// s_v1PatRaw keeps the native MSB-first byte for the word-based flood fill; s_v1PatLsb is the
// mirrored copy for the LSB-first framebuffer. The subindex format is the same as v2.
static uint8_t s_v1PatRaw[30][4];
static uint8_t s_v1PatLsb[30][4];
static bool    s_v1 = false;

static inline uint8_t mirror_2bpp_byte(uint8_t b);  // defined below

static bool s_haveDrawingTables = false;

bool gmcgaHaveDrawingTables() { return s_haveDrawingTables; }

// Built-in copies of the tables normally read from NOVEL.EXE / PC_GRAPH.OVR, for a game copy
// that has lost those files. They are the same in every known release: one set for the v2
// interpreters, one for the two v1 overlays, and one brush table for all. A game's own files
// take precedence when present.

// v2 fill table: 76 entries x (even column, odd column) bytes.
static const uint8_t kBuiltinFillV2[152] = {
	0x00,0x00,0x55,0x55,0xaa,0xaa,0xff,0xff,0x11,0x11,0x44,0x44,0x22,0x22,0x88,0x88,
	0x33,0x33,0xcc,0xcc,0x66,0x66,0x99,0x99,0x77,0x77,0xdd,0xdd,0xbb,0xbb,0xee,0xee,
	0x05,0x05,0x50,0x50,0x0a,0x0a,0xa0,0xa0,0x0f,0x0f,0xf0,0xf0,0x5a,0x5a,0xa5,0xa5,
	0x5f,0x5f,0xf5,0xf5,0xaf,0xaf,0xfa,0xfa,0x01,0x01,0x10,0x10,0x02,0x02,0x20,0x20,
	0x03,0x03,0x30,0x30,0x54,0x54,0x45,0x45,0x56,0x56,0x65,0x65,0x57,0x57,0x75,0x75,
	0xa8,0xa8,0x8a,0x8a,0x89,0x89,0x98,0x98,0xab,0xab,0xba,0xba,0xfc,0xfc,0xcf,0xcf,
	0xed,0xed,0xde,0xde,0xfe,0xfe,0xef,0xef,0x06,0x06,0x60,0x60,0x07,0x07,0x70,0x70,
	0x0b,0x0b,0xb0,0xb0,0x52,0x52,0x25,0x25,0x53,0x53,0x35,0x35,0x5b,0x5b,0xb5,0xb5,
	0xa1,0xa1,0x1a,0x1a,0xa3,0xa3,0x3a,0x3a,0xa7,0xa7,0x7a,0x7a,0xf1,0xf1,0x1f,0x1f,
	0xf2,0xf2,0x2f,0x2f,0xf6,0xf6,0x6f,0x6f,
};

// v2 fill-colour subindex: SET_FILL_COLOR byte -> (even row, odd row) entry.
static const uint8_t kBuiltinSubindexV2[512] = {
	0x03,0x03,0x2c,0x2d,0x20,0x21,0x30,0x31,0x08,0x09,0x14,0x00,0x00,0x00,0x14,0x15,
	0x23,0x03,0x48,0x49,0x02,0x00,0x12,0x13,0x02,0x33,0x0c,0x0d,0x04,0x07,0x2e,0x2f,
	0x04,0x09,0x10,0x11,0x23,0x2e,0x16,0x17,0x04,0x05,0x04,0x0c,0x1c,0x1d,0x1e,0x1f,
	0x46,0x47,0x22,0x23,0x24,0x25,0x36,0x37,0x2e,0x2f,0x35,0x36,0x2c,0x2d,0x28,0x29,
	0x2b,0x00,0x36,0x3a,0x34,0x35,0x2a,0x1d,0x0e,0x0a,0x3a,0x3b,0x02,0x01,0x3e,0x3f,
	0x40,0x41,0x42,0x43,0x44,0x45,0x16,0x17,0x48,0x49,0x4a,0x4b,0x18,0x19,0x2a,0x1f,
	0x16,0x1a,0x02,0x3f,0x0e,0x0f,0x12,0x13,0x03,0x03,0x00,0x00,0x48,0x49,0x32,0x33,
	0x03,0x16,0x03,0x1a,0x02,0x1a,0x1a,0x1b,0x02,0x02,0x02,0x00,0x1e,0x1f,0x12,0x13,
	0x36,0x37,0x04,0x0a,0x26,0x27,0x22,0x23,0x10,0x11,0x00,0x30,0x01,0x01,0x0a,0x0b,
	0x34,0x35,0x2a,0x2b,0x2a,0x22,0x36,0x35,0x0d,0x02,0x03,0x03,0x14,0x15,0x08,0x00,
	0x00,0x00,0x46,0x47,0x04,0x19,0x0c,0x0d,0x36,0x31,0x01,0x03,0x46,0x47,0x04,0x05,
	0x10,0x00,0x10,0x37,0x18,0x01,0x36,0x37,0x04,0x00,0x04,0x37,0x3c,0x3d,0x38,0x39,
	0x01,0x02,0x32,0x33,0x03,0x02,0x32,0x33,0x28,0x00,0x02,0x02,0x02,0x00,0x38,0x00,
	0x1e,0x00,0x12,0x00,0x06,0x07,0x06,0x00,0x00,0x00,0x11,0x11,0x22,0x22,0x33,0x33,
	0x44,0x44,0x55,0x55,0x66,0x66,0x77,0x77,0x88,0x88,0x99,0x99,0xaa,0xaa,0xbb,0xbb,
	0xcc,0xcc,0xdd,0xdd,0xee,0xee,0xff,0xff,0x01,0x01,0x10,0x10,0x02,0x02,0x20,0x20,
	0x03,0x03,0x30,0x30,0x04,0x04,0x40,0x40,0x05,0x05,0x50,0x50,0x06,0x06,0x60,0x60,
	0x07,0x07,0x70,0x70,0x08,0x08,0x80,0x80,0x09,0x09,0x90,0x90,0x0a,0x0a,0xa0,0xa0,
	0x0b,0x0b,0xb0,0xb0,0x0c,0x0c,0xc0,0xc0,0x0d,0x0d,0xd0,0xd0,0x0e,0x0e,0xe0,0xe0,
	0x0f,0x0f,0xf0,0xf0,0x12,0x12,0x21,0x21,0x13,0x13,0x31,0x31,0x14,0x14,0x41,0x41,
	0x15,0x15,0x51,0x51,0x16,0x16,0x61,0x61,0x17,0x17,0x71,0x71,0x18,0x18,0x81,0x81,
	0x19,0x19,0x91,0x91,0x1a,0x1a,0xa1,0xa1,0x1b,0x1b,0xb1,0xb1,0x1c,0x1c,0xc1,0xc1,
	0x1d,0x1d,0xd1,0xd1,0x1e,0x1e,0xe1,0xe1,0x1f,0x1f,0xf1,0xf1,0x23,0x23,0x32,0x32,
	0x24,0x24,0x42,0x42,0x25,0x25,0x52,0x52,0x26,0x26,0x62,0x62,0x27,0x27,0x72,0x72,
	0x28,0x28,0x82,0x82,0x29,0x29,0x92,0x92,0x2a,0x2a,0xa2,0xa2,0x2b,0x2b,0xb2,0xb2,
	0x2c,0x2c,0xc2,0xc2,0x2d,0x2d,0xd2,0xd2,0x2e,0x2e,0xe2,0xe2,0x2f,0x2f,0xf2,0xf2,
	0x34,0x34,0x43,0x43,0x35,0x35,0x53,0x53,0x36,0x36,0x63,0x63,0x37,0x37,0x73,0x73,
	0x38,0x38,0x83,0x83,0x39,0x39,0x93,0x93,0x3a,0x3a,0xa3,0xa3,0x3b,0x3b,0xb3,0xb3,
	0x3c,0x3c,0xc3,0xc3,0x3d,0x3d,0xd3,0xd3,0x3e,0x3e,0xe3,0xe3,0x3f,0x3f,0xf3,0xf3,
	0x45,0x45,0x54,0x54,0x46,0x46,0x64,0x64,0x47,0x47,0x74,0x74,0x48,0x48,0x84,0x84,
	0x49,0x49,0x94,0x94,0x4a,0x4a,0xa4,0xa4,0x4b,0x4b,0xb4,0xb4,0x4c,0x4c,0xc4,0xc4,
	0x4d,0x4d,0xd4,0xd4,0x4e,0x4e,0xe4,0xe4,0x56,0x56,0x65,0x65,0x57,0x57,0x75,0x75,
};

// v2 in-picture font, characters 32-127, 8 rows each, MSB = leftmost pixel.
// (v1 games keep theirs in CHARSET.GDA, a data file.)
static const uint8_t kBuiltinFontV2[768] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x30,0x78,0x78,0x30,0x30,0x00,0x30,0x00,
	0x6c,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00,
	0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00,
	0x30,0x7c,0xc0,0x78,0x0c,0xf8,0x30,0x00,
	0x00,0xc6,0xcc,0x18,0x30,0x66,0xc6,0x00,
	0x38,0x6c,0x38,0x76,0xdc,0xcc,0x76,0x00,
	0x60,0x60,0xc0,0x00,0x00,0x00,0x00,0x00,
	0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00,
	0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00,
	0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00,
	0x00,0x30,0x30,0xfc,0x30,0x30,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x60,
	0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,
	0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00,
	0x7c,0xc6,0xce,0xde,0xf6,0xe6,0x7c,0x00,
	0x30,0x70,0x30,0x30,0x30,0x30,0xfc,0x00,
	0x78,0xcc,0x0c,0x38,0x60,0xcc,0xfc,0x00,
	0x78,0xcc,0x0c,0x38,0x0c,0xcc,0x78,0x00,
	0x1c,0x3c,0x6c,0xcc,0xfe,0x0c,0x1e,0x00,
	0xfc,0xc0,0xf8,0x0c,0x0c,0xcc,0x78,0x00,
	0x38,0x60,0xc0,0xf8,0xcc,0xcc,0x78,0x00,
	0xfc,0xcc,0x0c,0x18,0x30,0x30,0x30,0x00,
	0x78,0xcc,0xcc,0x78,0xcc,0xcc,0x78,0x00,
	0x78,0xcc,0xcc,0x7c,0x0c,0x18,0x70,0x00,
	0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x00,
	0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x60,
	0x18,0x30,0x60,0xc0,0x60,0x30,0x18,0x00,
	0x00,0x00,0xfc,0x00,0x00,0xfc,0x00,0x00,
	0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00,
	0x78,0xcc,0x0c,0x18,0x30,0x00,0x30,0x00,
	0x7c,0xc6,0xde,0xde,0xde,0xc0,0x78,0x00,
	0x30,0x78,0xcc,0xcc,0xfc,0xcc,0xcc,0x00,
	0xfc,0x66,0x66,0x7c,0x66,0x66,0xfc,0x00,
	0x3c,0x66,0xc0,0xc0,0xc0,0x66,0x3c,0x00,
	0xf8,0x6c,0x66,0x66,0x66,0x6c,0xf8,0x00,
	0xfe,0x62,0x68,0x78,0x68,0x62,0xfe,0x00,
	0xfe,0x62,0x68,0x78,0x68,0x60,0xf0,0x00,
	0x3c,0x66,0xc0,0xc0,0xce,0x66,0x3e,0x00,
	0xcc,0xcc,0xcc,0xfc,0xcc,0xcc,0xcc,0x00,
	0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00,
	0x1e,0x0c,0x0c,0x0c,0xcc,0xcc,0x78,0x00,
	0xe6,0x66,0x6c,0x78,0x6c,0x66,0xe6,0x00,
	0xf0,0x60,0x60,0x60,0x62,0x66,0xfe,0x00,
	0xc6,0xee,0xfe,0xfe,0xd6,0xc6,0xc6,0x00,
	0xc6,0xe6,0xf6,0xde,0xce,0xc6,0xc6,0x00,
	0x38,0x6c,0xc6,0xc6,0xc6,0x6c,0x38,0x00,
	0xfc,0x66,0x66,0x7c,0x60,0x60,0xf0,0x00,
	0x78,0xcc,0xcc,0xcc,0xdc,0x78,0x1c,0x00,
	0xfc,0x66,0x66,0x7c,0x6c,0x66,0xe6,0x00,
	0x78,0xcc,0xe0,0x70,0x1c,0xcc,0x78,0x00,
	0xfc,0xb4,0x30,0x30,0x30,0x30,0x78,0x00,
	0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xfc,0x00,
	0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x30,0x00,
	0xc6,0xc6,0xc6,0xd6,0xfe,0xee,0xc6,0x00,
	0xc6,0xc6,0x6c,0x38,0x38,0x6c,0xc6,0x00,
	0xcc,0xcc,0xcc,0x78,0x30,0x30,0x78,0x00,
	0xfe,0xc6,0x8c,0x18,0x32,0x66,0xfe,0x00,
	0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00,
	0xc0,0x60,0x30,0x18,0x0c,0x06,0x02,0x00,
	0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00,
	0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x78,0x0c,0x7c,0xcc,0x76,0x00,
	0xe0,0x60,0x60,0x7c,0x66,0x66,0xdc,0x00,
	0x00,0x00,0x78,0xcc,0xc0,0xcc,0x78,0x00,
	0x1c,0x0c,0x0c,0x7c,0xcc,0xcc,0x76,0x00,
	0x00,0x00,0x78,0xcc,0xfc,0xc0,0x78,0x00,
	0x38,0x6c,0x60,0xf0,0x60,0x60,0xf0,0x00,
	0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0xf8,
	0xe0,0x60,0x6c,0x76,0x66,0x66,0xe6,0x00,
	0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00,
	0x0c,0x00,0x0c,0x0c,0x0c,0xcc,0xcc,0x78,
	0xe0,0x60,0x66,0x6c,0x78,0x6c,0xe6,0x00,
	0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00,
	0x00,0x00,0xcc,0xfe,0xfe,0xd6,0xc6,0x00,
	0x00,0x00,0xf8,0xcc,0xcc,0xcc,0xcc,0x00,
	0x00,0x00,0x78,0xcc,0xcc,0xcc,0x78,0x00,
	0x00,0x00,0xdc,0x66,0x66,0x7c,0x60,0xf0,
	0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0x1e,
	0x00,0x00,0xdc,0x76,0x66,0x60,0xf0,0x00,
	0x00,0x00,0x7c,0xc0,0x78,0x0c,0xf8,0x00,
	0x10,0x30,0x7c,0x30,0x30,0x34,0x18,0x00,
	0x00,0x00,0xcc,0xcc,0xcc,0xcc,0x76,0x00,
	0x00,0x00,0xcc,0xcc,0xcc,0x78,0x30,0x00,
	0x00,0x00,0xc6,0xd6,0xfe,0xfe,0x6c,0x00,
	0x00,0x00,0xc6,0x6c,0x38,0x6c,0xc6,0x00,
	0x00,0x00,0xcc,0xcc,0xcc,0x7c,0x0c,0xf8,
	0x00,0x00,0xfc,0x98,0x30,0x64,0xfc,0x00,
	0x1c,0x30,0x30,0xe0,0x30,0x30,0x1c,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,
	0xe0,0x30,0x30,0x1c,0x30,0x30,0xe0,0x00,
	0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x10,0x38,0x6c,0xc6,0xc6,0xfe,0x00,
};

// v1 fill patterns: 30 entries x 4 column-phase bytes.
static const uint8_t kBuiltinPatternsV1[120] = {
	0x00,0x00,0x00,0x00,
	0xaa,0xaa,0xaa,0xaa,
	0x55,0x55,0x55,0x55,
	0xff,0xff,0xff,0xff,
	0x00,0x00,0x00,0x00,
	0xaa,0xaa,0xaa,0xaa,
	0x55,0x55,0x55,0x55,
	0xff,0xff,0xff,0xff,
	0xc0,0xc0,0xc0,0xc0,
	0xc0,0xc0,0xc0,0xc0,
	0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0x30,
	0x80,0x40,0x80,0x40,
	0x40,0x80,0x40,0x80,
	0x08,0x04,0x08,0x04,
	0x04,0x08,0x04,0x08,
	0x20,0x10,0x20,0x10,
	0x10,0x20,0x10,0x20,
	0x02,0x01,0x02,0x01,
	0x01,0x02,0x01,0x02,
	0x80,0x08,0x10,0x80,
	0x08,0x10,0x80,0x08,
	0xee,0xee,0xee,0xee,
	0xee,0xee,0xee,0xee,
	0xbb,0xbb,0xbb,0xbb,
	0xbb,0xbb,0xbb,0xbb,
	0x77,0x77,0x77,0x77,
	0x77,0x77,0x77,0x77,
	0xdd,0xdd,0xdd,0xdd,
	0xdd,0xdd,0xdd,0xdd,
};

// v1 fill-colour subindex: 109 selectors x (even row, odd row) entry.
static const uint8_t kBuiltinSubindexV1[218] = {
	0x03,0x07,0x16,0x07,0x1a,0x1d,0x1c,0x17,0x08,0x0b,0x00,0x1b,0x00,0x04,0x03,0x1b,
	0x03,0x06,0x1a,0x06,0x00,0x06,0x00,0x11,0x02,0x06,0x1c,0x13,0x10,0x13,0x10,0x07,
	0x02,0x1b,0x02,0x07,0x02,0x17,0x02,0x09,0x1a,0x04,0x10,0x04,0x02,0x05,0x12,0x17,
	0x1a,0x07,0x03,0x17,0x16,0x19,0x03,0x05,0x03,0x0d,0x1a,0x0d,0x1a,0x05,0x10,0x05,
	0x00,0x0d,0x00,0x17,0x08,0x05,0x16,0x05,0x01,0x05,0x16,0x0b,0x01,0x07,0x01,0x17,
	0x01,0x09,0x01,0x04,0x16,0x04,0x0c,0x0f,0x01,0x1b,0x01,0x11,0x0c,0x17,0x0c,0x04,
	0x16,0x13,0x01,0x06,0x16,0x06,0x0c,0x11,0x07,0x07,0x04,0x04,0x07,0x1b,0x1b,0x1d,
	0x07,0x11,0x06,0x07,0x17,0x06,0x06,0x1b,0x06,0x06,0x04,0x06,0x11,0x13,0x04,0x11,
	0x17,0x07,0x17,0x0b,0x17,0x19,0x05,0x07,0x17,0x05,0x07,0x0d,0x05,0x05,0x05,0x0d,
	0x0d,0x0f,0x04,0x0d,0x17,0x04,0x05,0x1b,0x05,0x06,0x03,0x03,0x16,0x03,0x03,0x0c,
	0x00,0x00,0x08,0x1a,0x02,0x16,0x1a,0x1c,0x03,0x10,0x02,0x03,0x02,0x1a,0x02,0x02,
	0x12,0x1c,0x00,0x1a,0x12,0x1a,0x10,0x12,0x00,0x10,0x03,0x1a,0x16,0x1a,0x16,0x12,
	0x01,0x02,0x16,0x18,0x01,0x03,0x01,0x1a,0x01,0x16,0x01,0x01,0x01,0x00,0x16,0x00,
	0x16,0x0c,0x16,0x0e,0x0c,0x0e,0x00,0x0c,0x00,0x00,
};

// Brushes: 8 x 32 bytes, four 8-row columns each.
static const uint8_t kBuiltinBrushes[256] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
	0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x03,
	0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xc0,
	0xc0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x03,0x07,0x07,
	0x07,0x07,0x03,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xc0,0xe0,0xe0,
	0xe0,0xe0,0xc0,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x03,0x0f,0x0f,0x1f,0x1f,
	0x1f,0x1f,0x0f,0x0f,0x03,0x00,0x00,0x00,
	0x00,0x00,0x00,0xc0,0xf0,0xf0,0xf8,0xf8,
	0xf8,0xf8,0xf0,0xf0,0xc0,0x00,0x00,0x00,
	0x00,0x03,0x1f,0x3f,0x3f,0x3f,0x7f,0x7f,
	0x7f,0x7f,0x3f,0x3f,0x3f,0x1f,0x03,0x00,
	0x00,0xc0,0xf8,0xfc,0xfc,0xfc,0xfe,0xfe,
	0xfe,0xfe,0xfc,0xfc,0xfc,0xf8,0xc0,0x00,
	0x00,0x00,0x00,0x00,0x01,0x08,0x02,0x00,
	0x0a,0x00,0x04,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x90,
	0x00,0xa0,0x00,0x80,0x00,0x00,0x00,0x00,
	0x00,0x02,0x08,0x12,0x01,0x24,0x0b,0x03,
	0x23,0x09,0x22,0x0a,0x04,0x01,0x00,0x00,
	0x00,0x20,0x80,0x28,0x00,0xd4,0xc0,0xe4,
	0xe8,0x90,0x44,0xa8,0x00,0x50,0x00,0x00,
};

static void installV2Tables(const uint8_t *fill, const uint8_t *subindex,
							const uint8_t *brushes, const uint8_t *font);
static void installV1Tables(const uint8_t *patterns, const uint8_t *subindex,
							const uint8_t *brushes);

bool gmcgaInstallDrawingTables(const uint8_t *exe, size_t size) {
	if (!exe)
		return false;

	// Locate the tables inside the interpreter's data segment by the fill table's signature
	// (its first eight patterns), since a whole-binary hash would break on every re-release.
	// The brush and font tables are at fixed distances from it. The numbers below are file
	// offsets in Talisman's NOVEL.EXE / NOVEL1.EXE.
	static const uint8_t kFillSig[] = {
		0x00,0x00, 0x55,0x55, 0xaa,0xaa, 0xff,0xff,
		0x11,0x11, 0x44,0x44, 0x22,0x22, 0x88,0x88,
	};
	const size_t kFillToSub   = 0xca5c - 0xc8d0; // 0x18c
	const size_t kFillToBrush = 0xcdf1 - 0xc8d0; // 0x521
	const size_t kFillToFont  = 0xcef1 - 0xc8d0; // 0x621

	size_t fillBase = SIZE_MAX;
	if (size >= 0xc8d0 + sizeof(kFillSig) &&
		std::memcmp(exe + 0xc8d0, kFillSig, sizeof(kFillSig)) == 0) {
		fillBase = 0xc8d0;  // fast path: the known Talisman offset
	} else {
		for (size_t i = 0; i + sizeof(kFillSig) <= size; i++) {
			if (std::memcmp(exe + i, kFillSig, sizeof(kFillSig)) == 0) {
				fillBase = i;
				break;
			}
		}
	}
	if (fillBase == SIZE_MAX)
		return false;
	if (fillBase + kFillToFont + sizeof(s_fontGlyphs) > size)
		return false;

	installV2Tables(exe + fillBase, exe + fillBase + kFillToSub,
					exe + fillBase + kFillToBrush, exe + fillBase + kFillToFont);
	return true;
}

static void installV2Tables(const uint8_t *fill, const uint8_t *subindex,
							const uint8_t *brushes, const uint8_t *font) {
	// Fill table: even == odd for every entry, so keep one byte per entry. The interpreter
	// writes pixels MSB-first within a CGA byte (pixel 0 in bits 7:6) and this framebuffer is
	// LSB-first, so mirror each entry's 2-bit groups once here. Every LSB-first sample,
	// including fill_hline's whole-byte store, then lands the pattern at the correct column.
	for (int i = 0; i < 76; i++) {
		uint8_t b = fill[i * 2];
		s_fillTableRaw[i] = b;
		s_fillTable[i] = (uint8_t)(((b & 0x03) << 6) | ((b & 0x0c) << 2) |
								   ((b & 0x30) >> 2) | ((b & 0xc0) >> 6));
	}

	// Fill-colour subindex: the op6 byte indexes this as a little-endian word whose two bytes
	// are pattern-table indices for even and odd rows.
	std::memcpy(s_fillSubindex, subindex, sizeof(s_fillSubindex));

	std::memcpy(s_brushBitmaps, brushes, sizeof(s_brushBitmaps));
	std::memcpy(s_fontGlyphs,   font,    sizeof(s_fontGlyphs));

	s_v1 = false;
	s_haveDrawingTables = true;
}

// Install the drawing tables for the v1 DOS releases (Crimson Crown, Transylvania v1). The fill
// pattern table is BSS in NOVEL.EXE, copied from PC_GRAPH.OVR at overlay load, so it can only
// be read from the OVR file; the brushes are static data in the EXE. Layouts:
//   OVR fill patterns: 30 entries x 4 column-phase bytes, found by their first four entries.
//   OVR subindex:      immediately follows, 109 words (selector -> index pair).
//   EXE brushes:       8 x 32 bytes, located through brush 5 (the filled-circle default).
bool gmcgaInstallV1DrawingTables(const uint8_t *ovr, size_t ovrSize,
								 const uint8_t *exe, size_t exeSize) {
	if (!ovr || !exe)
		return false;

	// Fill pattern table signature: the first four patterns are solid black /
	// magenta-ish (aa) / cyan-ish (55) / white (ff), each replicated across all
	// four phase bytes.
	static const uint8_t kPatSig[] = {
		0x00,0x00,0x00,0x00, 0xaa,0xaa,0xaa,0xaa,
		0x55,0x55,0x55,0x55, 0xff,0xff,0xff,0xff,
	};
	size_t patBase = SIZE_MAX;
	for (size_t i = 0; i + sizeof(kPatSig) <= ovrSize; i++) {
		if (std::memcmp(ovr + i, kPatSig, sizeof(kPatSig)) == 0) {
			patBase = i;
			break;
		}
	}
	if (patBase == SIZE_MAX)
		return false;
	const size_t kPatBytes = 30 * 4;             // 120
	const size_t kSubBytes = 109 * 2;            // 218 (selectors 0..108)
	if (patBase + kPatBytes + kSubBytes > ovrSize)
		return false;

	// Brush table in NOVEL.EXE: locate brush 5 (filled circle), whose left
	// column (bytes 0-7) is a distinctive run, then step back to brush 0.
	size_t brushBase;
	if (!gmvFindBrushTable(exe, exeSize, &brushBase))
		return false;
	installV1Tables(ovr + patBase, ovr + patBase + kPatBytes, exe + brushBase);
	return true;
}

// `patterns` is 30 x 4 bytes, `subindex` 109 x 2, `brushes` 8 x 32.
static void installV1Tables(const uint8_t *patterns, const uint8_t *subindex,
							const uint8_t *brushes) {
	for (int idx = 0; idx < 30; idx++) {
		for (int ph = 0; ph < 4; ph++) {
			const uint8_t b = patterns[idx * 4 + ph];
			s_v1PatRaw[idx][ph] = b;
			s_v1PatLsb[idx][ph] = mirror_2bpp_byte(b);
		}
	}
	std::memset(s_fillSubindex, 0, sizeof(s_fillSubindex));
	std::memcpy(s_fillSubindex, subindex, 109 * 2);
	std::memcpy(s_brushBitmaps, brushes, sizeof(s_brushBitmaps));

	// v1 NOVEL.EXE has no embedded picture font; its op3/op5 glyphs come from
	// CHARSET.GDA, loaded separately via gmcgaSetV1Font().  Leave zeroed until
	// then (in-picture text stays blank, as it would without the file).
	std::memset(s_fontGlyphs, 0, sizeof(s_fontGlyphs));

	s_v1 = true;
	s_haveDrawingTables = true;
}

void gmcgaInstallBuiltinDrawingTables(bool v1) {
	if (v1)
		installV1Tables(kBuiltinPatternsV1, kBuiltinSubindexV1, kBuiltinBrushes);
	else
		installV2Tables(kBuiltinFillV2, kBuiltinSubindexV2, kBuiltinBrushes, kBuiltinFontV2);
}

bool gmcgaSetV1Font(const uint8_t *charsetGda, size_t size) {
	// charset.gda is LSB-first (bit 0 = leftmost); draw_glyph reads MSB-first, so
	// the loader reverses each glyph byte as it copies it in.
	return gmvLoadCharsetGda(s_fontGlyphs, charsetGda, size);
}

// 2-bpp framebuffer: 280 x 160, pixel p of a byte in bits (2p+1):(2p), as in the file comment.

#define GMCGA_WIDTH   280
#define GMCGA_HEIGHT  160
#define GMCGA_STRIDE  70   // 280 × 2 bpp / 8 = 70 bytes/row

static uint8_t s_screenmem[GMCGA_HEIGHT * GMCGA_STRIDE];

// Slow ("animated") draw, with the same contract as graphics_magician.cpp. While recording,
// every framebuffer byte write is also logged; the host replays the log onto s_slowScreen a
// chunk at a time, so the picture is revealed in paint order. op13 adds a pause. s_screenmem
// is the fully drawn page. The state lives in SlowDrawPage (slow_draw_page.h).
static uint8_t s_slowScreen[GMCGA_HEIGHT * GMCGA_STRIDE];
static SlowDrawPage s_slow(s_screenmem, s_slowScreen, sizeof(s_screenmem),
						   GMCGA_STRIDE);

// Apply one byte to the final page and, while recording, log it for the reveal.
static inline void store_byte(int off, uint8_t value) {
	s_screenmem[off] = value;
	s_slow.record(off, value);
}

static inline void write_2bpp(int x, int y, uint8_t val) {
	if ((unsigned)x >= GMCGA_WIDTH || (unsigned)y >= GMCGA_HEIGHT) return;
	int off = y * GMCGA_STRIDE + (x >> 2);
	int shift = (x & 3) << 1;
	store_byte(off, (uint8_t)((s_screenmem[off] & ~(0x3u << shift)) | ((val & 0x3u) << shift)));
}

static inline uint8_t read_2bpp(int x, int y) {
	if ((unsigned)x >= GMCGA_WIDTH || (unsigned)y >= GMCGA_HEIGHT) return 0;
	return (s_screenmem[y * GMCGA_STRIDE + (x >> 2)] >> ((x & 3) << 1)) & 0x3;
}

void gmcgaResetScreen(bool white) {
	memset(s_screenmem, white ? 0xff : 0x00, sizeof(s_screenmem));
	// A reset starts a fresh page: it appears instantly on the visible page too,
	// and any pending reveal is dropped.
	memset(s_slowScreen, white ? 0xff : 0x00, sizeof(s_slowScreen));
	s_slow.clear();
}

// Fill helpers

// Resolve an op6 selector byte to the 4-pixel pattern tile for row `y` at logical byte column
// `logByte`. The selector gives a pair of pattern indices, one per row parity, which makes the
// two-row dithers (e.g. the cyan/black checkerboard background).
//
// This is the path for the v1 brush and op5 text blitters, which stamp at arbitrary byte
// columns, so the phase matters: it is the global CGA screen byte & 3. The picture window is
// offset +20 px = +5 bytes, hence (logByte + 1) & 3. The flood fill uses pf_pattern_word.
static inline uint8_t fill_pattern(uint8_t sel, int y, int logByte) {
	uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
	if (s_v1)
		return (sub < 30) ? s_v1PatLsb[sub][(logByte + 1) & 3] : 0x00;
	return (sub < 76) ? s_fillTable[sub] : 0x00;
}

// Fill pixels [l..r] on row y with the current fill pattern (selector `sel`).
// The pattern tile covers 4 pixels and is aligned to the byte grid by absolute
// x phase (x & 3), so adjacent fills tile seamlessly.
static void fill_hline(int y, int l, int r, uint8_t sel) {
	if ((unsigned)y >= GMCGA_HEIGHT) return;
	if (l < 0) l = 0;
	if (r >= GMCGA_WIDTH) r = GMCGA_WIDTH - 1;
	if (l > r) return;

	const int base = y * GMCGA_STRIDE;
	int bl = l >> 2, br = r >> 2;

	if (bl == br) {
		const uint8_t fb = fill_pattern(sel, y, bl);
		uint8_t v = s_screenmem[base + bl];
		for (int x = l; x <= r; x++) {
			int shift = (x & 3) << 1;
			v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
		}
		store_byte(base + bl, v);
		return;
	}
	// Left partial byte
	{
		const uint8_t fb = fill_pattern(sel, y, bl);
		uint8_t v = s_screenmem[base + bl];
		for (int x = l, xe = (bl + 1) << 2; x < xe; x++) {
			int shift = (x & 3) << 1;
			v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
		}
		store_byte(base + bl, v);
	}
	// Whole bytes in the middle (period-4: same pattern byte every column)
	for (int b = bl + 1; b < br; b++) store_byte(base + b, fill_pattern(sel, y, b));
	// Right partial byte
	{
		const uint8_t fb = fill_pattern(sel, y, br);
		uint8_t v = s_screenmem[base + br];
		for (int x = br << 2; x <= r; x++) {
			int shift = (x & 3) << 1;
			v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
		}
		store_byte(base + br, v);
	}
}

// Flood fill (op14 PAINT): a port of the native fill, which works by words and not per pixel.
// Byte offsets are global screen bytes: the picture window occupies bytes 5..74 (x+20 pixels)
// and the margins read as black. Only white (3) is fillable; anything else is a boundary.
// Each 16-bit word is painted as (pattern & M) | (screen ^ M), where M is the white run in that
// word; since white is all 1s, the XOR preserves every pixel outside M. Spans re-read the
// screen as they scan, so freshly painted dither-black pixels limit later spans, and the
// traversal order in gm_span_fill.h is part of the result.

// Per-pixel 2-bit masks over the byteswapped (MSB-first) screen word, plus the masks of
// everything right / left of pixel i, which blank the far side of the seed during the in-word
// boundary scans.
static const uint16_t kPfPixMask[8] = {
	0xC000, 0x3000, 0x0C00, 0x0300, 0x00C0, 0x0030, 0x000C, 0x0003
};
static const uint16_t kPfRightOf[8] = {
	0x3FFF, 0x0FFF, 0x03FF, 0x00FF, 0x003F, 0x000F, 0x0003, 0x0000
};
static const uint16_t kPfLeftOf[8] = {
	0x0000, 0xC000, 0xF000, 0xFC00, 0xFF00, 0xFFC0, 0xFFF0, 0xFFFC
};

// Window bounds, set before every picture: rows 0..0x9f, global bytes 5..0x4a.
#define PF_RIGHT_BYTE 0x4a
#define PF_BOTTOM_ROW 0x9f

// Flood-fill clip. In v1, op15 (RESET) sets a clip rectangle, but the fill consults only its
// right byte and bottom row, so the rest is not kept. Both default to the picture window, which
// leaves the fill unbounded within it when op15 is never emitted (the whole v1 corpus).
static int s_pfRightByte  = PF_RIGHT_BYTE;
static int s_pfBottomRow  = PF_BOTTOM_ROW;

static inline uint8_t mirror_2bpp_byte(uint8_t b) {
	return (uint8_t)(((b & 0x03) << 6) | ((b & 0x0c) << 2) |
					 ((b & 0x30) >> 2) | ((b & 0xc0) >> 6));
}

// Read / write the logical screen word at even global byte offset `gb` of row `y`. Pixel k
// (0..7, left to right) sits in bits (15-2k):(14-2k), as in the native's byteswapped word.
// Bytes outside the picture window read as black and ignore writes, like the CGA margins.
static uint16_t pf_get_word(int y, int gb) {
	uint16_t w = 0;
	if ((unsigned)y < GMCGA_HEIGHT) {
		for (int i = 0; i < 2; i++) {
			const int b = gb + i;
			if (b >= 5 && b <= 74)
				w |= (uint16_t)mirror_2bpp_byte(s_screenmem[y * GMCGA_STRIDE + (b - 5)])
					 << (8 - 8 * i);
		}
	}
	return w;
}

static void pf_put_word(int y, int gb, uint16_t w) {
	if ((unsigned)y >= GMCGA_HEIGHT) return;
	for (int i = 0; i < 2; i++) {
		const int b = gb + i;
		if (b >= 5 && b <= 74)
			store_byte(y * GMCGA_STRIDE + (b - 5),
					   mirror_2bpp_byte((uint8_t)(w >> (8 - 8 * i))));
	}
}

// Is the pixel at global column `x` of row `y` white? The native addresses a pixel as an even
// byte B and a pixel P within that word; x == 4*B + P, and the fill works in x throughout.
static inline bool pf_white(int y, int x) {
	const uint16_t w = pf_get_word(y, (x >> 3) * 2);
	return (w & kPfPixMask[x & 7]) == kPfPixMask[x & 7];
}

// The fill-pattern word for row `y` under selector `sel`, in the same byteswapped bit order as
// pf_get_word. Row parity picks the subindex entry; the word is the pattern byte doubled. `gb`
// is the word's even global byte column but does not affect the result: v2 patterns are the
// same at every column, and v1 reads entry[0] (see s_v1PatRaw).
static inline uint16_t pf_pattern_word(uint8_t sel, int y, int gb) {
	const uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
	if (s_v1) {
		if (sub >= 30) return 0;
		const uint8_t b = s_v1PatRaw[sub][0];   // period-4: entry[0] both halves
		return (uint16_t)((b << 8) | b);
	}
	const uint8_t m = (sub < 76) ? s_fillTableRaw[sub] : 0x00;
	return (uint16_t)((m << 8) | m);
}

// Scan the word `A` from pixel 7 leftward for the first non-white pixel. On a hit, store its
// index in `*edge` and return the mask of pixels right of it. An all-white `A` is returned
// unchanged and *edge keeps its previous value, as in the native.
static inline uint16_t pf_scan_l(uint16_t A, uint8_t *edge) {
	if (A != 0xffff)
		for (int i = 7; i >= 0; i--)
			if ((A & kPfPixMask[i]) != kPfPixMask[i]) {
				*edge = (uint8_t)i;
				return kPfRightOf[i];
			}
	return A;
}

// Mirror image of pf_scan_l: scan from pixel 0 rightward, return the mask of pixels left of the
// boundary.
static inline uint16_t pf_scan_r(uint16_t A, uint8_t *edge) {
	if (A != 0xffff)
		for (int i = 0; i < 8; i++)
			if ((A & kPfPixMask[i]) != kPfPixMask[i]) {
				*edge = (uint8_t)i;
				return kPfLeftOf[i];
			}
	return A;
}

// Fill tracing (GMCGA_TRACE_FILL) is compiled in only for the -DGM_TRACE test
// tools; elsewhere the flag is a constant and the logging folds away.
#ifdef GM_TRACE
static bool s_pfTrace = false;  // debug: log queue pushes (GMCGA_TRACE_FILL)
#else
static const bool s_pfTrace = false;
#endif
static int  s_pfWordSeq = 0;    // debug: per-fill word-paint sequence counter

// Paint the word at (row `y`, byte `gb`): pixels selected by `M`, always a run of white pixels,
// take the pattern and the rest of the word is preserved.
static inline void pf_paint_word(int y, int gb, uint8_t sel, uint16_t S, uint16_t M) {
	const uint16_t pat = pf_pattern_word(sel, y, gb);
	if (s_pfTrace) {
		const uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
		fprintf(stderr, "WORD seq=%d row=%d gb=%d sub=%d pat=%04x M=%04x\n",
				++s_pfWordSeq, y, gb, sub, pat, M);
	}
	pf_put_word(y, gb, (uint16_t)((pat & M) | (S ^ M)));
}

// Grow and paint the span around the white seed at column `x` of
// row `y`, a word at a time, each word masked to the white run found in it.
// Returns the span's first and last white pixel in *l and *r.
static void pf_scan_paint(int y, int x, uint8_t sel, int *l, int *r) {
	int si = (x >> 3) * 2;                               // word holding the seed
	uint8_t lP = 0xff;                                   // 0xff = no left edge yet
	uint8_t rP = 8;                                      // 8 = open to the right
	uint16_t S = pf_get_word(y, si);

	// Seed word: find the nearest boundary on each side of the seed pixel (the
	// far side blanked to white for each scan) and paint the run between them.
	const uint8_t seedP = (uint8_t)(x & 7);
	const uint16_t mL = pf_scan_l((uint16_t)(S | kPfRightOf[seedP]), &lP);
	const uint16_t mR = pf_scan_r((uint16_t)(kPfLeftOf[seedP] | S), &rP);
	pf_paint_word(y, si, sel, S, (uint16_t)(mR & mL));

	int li = si;
	while (lP == 0xff) {                                 // extend left
		li -= 2;
		S = pf_get_word(y, li);
		const uint16_t M = pf_scan_l(S, &lP);
		pf_paint_word(y, li, sel, S, M);
	}
	*l = li * 4 + lP + 1;                                // first white pixel, right of the boundary
	while (rP == 8 && si < s_pfRightByte) {              // extend right
		si += 2;
		S = pf_get_word(y, si);
		const uint16_t M = pf_scan_r(S, &rP);
		pf_paint_word(y, si, sel, S, M);
	}
	*r = si * 4 + rP - 1;                                // last white pixel, left of the boundary
}

// The fill policy for gm_span_fill.h. Positions are global pixel columns. The native uses
// (even byte, pixel) pairs with 8-bit bytes; no wrap can occur because the margins outside bytes
// 5..74 read black, so every column formed lies between 18 and 301 and the leftward scan needs
// no clip.
struct CgaFillGeom {
	// A full queue stops the fill.
	static const bool kAbortOnOverflow = true;

	int     y;
	uint8_t sel;

	void setRow(int row) { y = row; }
	bool isSet(int x) const { return pf_white(y, x); }
	void scanPaint(int x, int &l, int &r) const { pf_scan_paint(y, x, sel, &l, &r); }

	// Down pushes stop at the bottom row. Up pushes are never rejected, so spans on row 0 push
	// an up entry; its target row reads black and the entry retires without effect, as in the
	// native.
	bool acceptSpan(uint8_t row, int, int, uint8_t dir) const {
		return dir == GM_SPAN_UP || row < s_pfBottomRow;
	}

	void notePush(uint8_t row, int l, int r, uint8_t dir, uint8_t head, uint8_t tail) const {
		if (s_pfTrace)
			fprintf(stderr, "PUSH y=%d l=%d r=%d d=%d h=%d t=%d\n",
					row, l, r, dir, head, tail);
	}
};

static void gmcga_flood_fill(int seed_x, int seed_y, uint8_t sel) {
	if ((uint8_t)seed_y > s_pfBottomRow)
		return;
	CgaFillGeom geom = { 0, sel };
	GmSpanFill<CgaFillGeom> fill(geom);
	fill.fill(seed_x + 20, seed_y);                      // to global pixel column
}

// Line (op10) and box (op9); the geometry is shared with the PCjr renderer.
static void draw_line(int x0, int y0, int x1, int y1, uint8_t pen_val) {
	gmvDrawLine(x0, y0, x1, y1,
				[pen_val](int x, int y) { write_2bpp(x, y, pen_val); });
}

// Circle (op11); the geometry is shared with the PCjr renderer.
static void draw_circle(int cx, int cy, int radius, uint8_t pen_val) {
	gmvDrawCircle(cx, cy, radius,
				  [pen_val](int x, int y) { write_2bpp(x, y, pen_val); });
}

// Brush / glyph column blitter: 8 rows of 8-pixel data, each byte MSB-first (bit 7 = leftmost
// pixel). A set bit writes the fill-pattern pixel at that position; a clear bit leaves the
// background.

static void blit_col(const uint8_t *bytes8, int px, int py, uint8_t fill_sel) {
	// As in the native, each row is bit-doubled to a 16-bit 2-bpp mask and shifted by the
	// sub-pixel offset, spilling across up to three output bytes, then the pattern is painted
	// through the mask a byte at a time. The dither phase advances only for bytes actually
	// painted, while the byte cursor always advances, so a blank first or middle byte puts the
	// spill byte one phase earlier than its column implies. This is what colours the off-grid
	// Crimson Crown crypt spill byte magenta (phase 3) instead of cyan (phase 0).
	const int lb0 = px >> 2;          // logical byte column of the brush anchor
	const int sub = px & 3;           // sub-pixel offset (== (px+20) & 3)
	const int p0  = (lb0 + 1) & 3;    // global byte phase of the first output byte
	for (int row = 0; row < 8; row++, py++) {
		if ((unsigned)py >= GMCGA_HEIGHT) continue;
		const uint8_t b = bytes8[row];
		if (!b) continue;
		uint16_t mask16 = 0;
		for (int p = 0; p < 8; p++)
			mask16 = (uint16_t)((mask16 << 2) | (((b >> (7 - p)) & 1) ? 0x3 : 0x0));
		const uint32_t vs = ((uint32_t)mask16 << 8) >> (sub << 1);
		const uint8_t m[3] = { (uint8_t)(vs >> 16), (uint8_t)(vs >> 8), (uint8_t)vs };
		int phase = p0;
		for (int k = 0; k < 3; k++) {
			if (k == 0) {
				if (!m[0]) continue;          // first byte uses p0 directly
			} else {
				if (!m[k]) continue;          // blank byte: cursor moves, phase does not
				phase = (phase + 1) & 3;       // painted byte: advance the dither phase
			}
			const int lb = lb0 + k;
			// Feed fill_pattern a synthetic column so its (logByte+1)&3 yields
			// `phase`; for v2 the pattern is phase-independent, so this is inert.
			const uint8_t fb = fill_pattern(fill_sel, py, (phase + 3) & 3);
			for (int pos = 0; pos < 4; pos++) {
				if (((m[k] >> ((3 - pos) << 1)) & 0x3) == 0) continue;
				// lb can be -1 (brush stamped at x-1), so multiply rather than shift.
				const int x = lb * 4 + pos;
				if ((unsigned)x >= GMCGA_WIDTH) continue;
				const uint8_t pix = (fb >> ((x & 3) << 1)) & 0x3;
				write_2bpp(x, py, pix);
			}
		}
	}
}

// Brush stamp (op12). Each brush is 32 bytes; the 8-byte quadrants are, in order, upper-left
// at (x, y), lower-left at (x, y+8), upper-right at (x+8, y) and lower-right at (x+8, y+8).

static void draw_brush(int x, int y, uint8_t brush, uint8_t fill_idx) {
	gmvStampBrush(s_brushBitmaps, x, y, brush,
				  [fill_idx](const uint8_t *q, int px, int py) { blit_col(q, px, py, fill_idx); });
}

// Text glyph. op3 (TEXT_CHAR) inverts the background under each set glyph pixel (value ^= 3),
// which paints black letters on the white subtitle cloud; the pen colour is not used. op5
// (TEXT_OUTLINE) takes the fill-pattern pixel, like a brush stamp. Glyph bytes use the brush
// bit order, and the text cursor advances 8 pixels per character.

static void draw_glyph(int x, int y, uint8_t ch, bool normal, uint8_t fill_sel) {
	gmvDrawGlyph(s_fontGlyphs, x, y, ch, [normal, fill_sel](int px, int py) {
		if ((unsigned)py >= GMCGA_HEIGHT || (unsigned)px >= GMCGA_WIDTH) return;
		const uint8_t fb = normal ? 0 : fill_pattern(fill_sel, py, px >> 2);
		const uint8_t pix = normal ? (uint8_t)(read_2bpp(px, py) ^ 0x3)
								   : (fb >> ((px & 3) << 1)) & 0x3;
		write_2bpp(px, py, pix);
	});
}

// op15 fill rectangle (OPCODE_RESET). Left/right are Apple hi-res columns (0..39) and top/bottom
// pixel rows, because the shared picture data was authored for the Apple screen layout. Apple
// column c covers pixels [c*7 .. c*7+6].

static GmFillBounds s_fill = { 0, 39, 0, 159 };

static void fill_rect(uint8_t fill_idx) {
	const int lp = s_fill.left * 7;
	const int rp = (s_fill.right + 1) * 7 - 1;
	for (int y = s_fill.top; y <= s_fill.bottom && y < GMCGA_HEIGHT; y++)
		fill_hline(y, lp, rp, fill_idx);
}

// SET_PEN_COLOR (op2) carries a colour index in its low nibble, which the interpreter draws
// lines in as index & 3. Pen colour 5 is therefore cyan, not magenta, as room RG_07's strokes
// over a magenta fill band show.

static const uint8_t kPenTo2bpp[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };

#ifdef GM_TRACE
// Debug aid (GMCGA_TRACE_DIR): write the framebuffer around fill number n.
static void trace_dump_fill(const char *dir, int n, const char *suffix) {
	if (!dir)
		return;
	char p[512];
	snprintf(p, sizeof(p), "%s/fill_%03d%s.raw", dir, n, suffix);
	if (FILE *f = fopen(p, "wb")) {
		fwrite(s_screenmem, 1, sizeof(s_screenmem), f);
		fclose(f);
	}
}
#endif

// Opcode interpreter: the CGA side of the shared gmRunStream (gm_stream.h).

struct CgaRenderer {
	int16_t  pen_x, pen_y;
	uint8_t  pen_val;   // 2-bpp pixel value (0-3) for lines
	uint8_t  brush;     // brush index 0-7
	uint8_t  fill_idx;  // SET_FILL_COLOR selector byte (-> subindex table)
	uint16_t text_x;
	uint8_t  text_y;

	uint16_t scaleX(uint16_t x) const { return x; }
	int penX() const { return pen_x; }
	int penY() const { return pen_y; }

	void begin() {
		// The op list is dropped by gmcgaResetScreen() when a new page starts, not here: item
		// overlays append their bytes to the room's pending reveal, so the whole composite is
		// revealed in paint order. Clearing here would leave only the last picture animated
		// on a blank page.
		pen_val  = 0;    // pen colour 4 (black) maps to 2-bpp 0
		// The dispatcher seeds the pen position with (140,6); Transylvania's title stream
		// draws its first line before any MOVE_TO.
		pen_x    = 140;
		pen_y    = 6;
		brush    = 5;    // default brush matches Apple renderer init
		fill_idx = 0;    // cold-start selector 0 -> solid white (0xff), as in NOVEL.EXE
		text_x   = 140; text_y = 96;
		s_fill.reset(159);
		// The flood-fill clip resets to the picture window before each image; op15 may then
		// narrow it (v1 only).
		s_pfRightByte = PF_RIGHT_BYTE; s_pfBottomRow = PF_BOTTOM_ROW;
	}
	// When not animating, keep the visible page in step with the final page so a
	// later slow-drawn overlay composes onto the right background.
	void end() { s_slow.syncIfNotRecording(); }

	void beforeOp(const uint8_t *p, const uint8_t *end, uint8_t op, uint8_t param) {
#ifdef GM_TRACE
		if (getenv("GMCGA_OPLOG")) {
			static int oc = 0;
			fprintf(stderr, "OP #%d op=%u param=%u nextbytes=%02x %02x %02x\n",
					++oc, op, param,
					(p < end) ? p[0] : 0,
					(p + 1 < end) ? p[1] : 0,
					(p + 2 < end) ? p[2] : 0);
		}
#else
		(void)p; (void)end; (void)op; (void)param;
#endif
	}
	void afterOp(uint8_t op, uint8_t param) {
#ifdef GM_TRACE
		// Debug: report which op first writes a watched pixel (GMCGA_WATCH="x,y").
		static int opn = 0;
		opn++;
		if (const char *w = getenv("GMCGA_WATCH")) {
			int wx, wy;
			if (sscanf(w, "%d,%d", &wx, &wy) == 2) {
				static int last = -2;
				int cur = read_2bpp(wx, wy);
				if (cur != last) {
					fprintf(stderr, "WATCH op#%d op=%u param=%u fill_idx=%u "
							"-> pixel(%d,%d) %d->%d\n",
							opn, op, param, fill_idx, wx, wy, last, cur);
					last = cur;
				}
			}
		}
#else
		(void)op; (void)param;
#endif
	}

	void setTextPos(uint16_t x, uint8_t y) { text_x = x; text_y = y; }
	// op3 is solid (inverting) text, op5 takes the fill pattern.
	void drawText(uint8_t ch, bool normal) {
		draw_glyph(text_x, text_y, ch, normal, fill_idx);
	}
	void advanceText() { text_x += 8; }
	void setPen(uint8_t param) { pen_val = (param < 8) ? kPenTo2bpp[param] : 0; }
	void setShape(uint8_t param) { brush = param; }
	void setFill(uint8_t b) { fill_idx = b; }
	// 2-operand no-op in Talisman.
	bool op7(const uint8_t *&p, const uint8_t *e) { return gmSkipOp7(p, e); }
	void moveTo(uint16_t x, uint8_t y) { pen_x = (int16_t)x; pen_y = (int16_t)y; }
	void lineTo(uint16_t x, uint8_t y) {
		draw_line(pen_x, pen_y, (int)x, (int)y, pen_val);
		pen_x = (int16_t)x; pen_y = (int16_t)y;
	}
	void circle(uint8_t radius) { draw_circle(pen_x, pen_y, radius, pen_val); }
	// Brushes stamp one pixel left of the stream coordinate, as in the native (text glyphs do
	// not).
	void drawBrush(uint16_t x, uint8_t y) { draw_brush((int)x - 1, (int)y, brush, fill_idx); }
	void delay(uint8_t units) { s_slow.recordDelay(units); }   // pacing hint only

	void paint(uint16_t x, uint8_t y) {
#ifdef GM_TRACE
		// Debug aid: dump the framebuffer before AND after each fill for
		// comparison against a DOSBox trace of the native interpreter.  The
		// before-dumps confirm the canvas going into a fill matches; the
		// after-dumps isolate whether a divergence is the fill itself (vs a
		// later primitive).  See test/gmcgav1/compare_op14.py.
		static int n = 0;
		n++;
		const char *td = getenv("GMCGA_TRACE_DIR");
		const char *tf = getenv("GMCGA_TRACE_FILL");
		s_pfTrace = tf && atoi(tf) == n;
		s_pfWordSeq = 0;
		trace_dump_fill(td, n, "");
		gmcga_flood_fill((int)x, (int)y, fill_idx);
		trace_dump_fill(td, n, "_after");
#else
		gmcga_flood_fill((int)x, (int)y, fill_idx);
#endif
	}

	bool opReset(uint8_t param, const uint8_t *&p, const uint8_t *e) {
		if (s_v1) {
			// v1: op15 sets up the flood-fill clip instead of painting a rectangle. Param 0 is
			// a no-op, 1 resets the clip to the window, and >=2 reads 4 bytes (only the right
			// byte and bottom row matter to the fill). No v1 picture in the corpus emits it;
			// the read keeps the stream aligned for any that does.
			switch (param) {
			case 1:
				s_pfRightByte = PF_RIGHT_BYTE; s_pfBottomRow = PF_BOTTOM_ROW;
				break;
			default:
				if (param >= 2) {
					GmFillBounds clip;
					if (!clip.read(p, e))
						return true;
					s_pfRightByte  = clip.right;
					s_pfBottomRow  = clip.bottom;
				}
				break; // param 0: nothing
			}
			return false;
		}
		// v2 / Apple Talisman: op15 is the fill-rectangle opcode.
		switch (param) {
		case 1: // set rect to full picture area
			s_fill.reset(159);
			break;
		case 2: // read 4 bytes: right, left, bottom, top
			if (!s_fill.read(p, e))
				return true;
			break;
		default: // param 0 / 3: fill the rect with the current pattern
			fill_rect(fill_idx);
			break;
		}
		return false;
	}
};

void gmcgaDrawImage(const uint8_t *data, size_t size) {
	CgaRenderer r;
	gmRunStream(r, data, size);
}

// 2-bpp to RGBA: the CGA palette 1, low intensity colours the game selects on a colour adapter,
// not the brighter 55ffff/ff55ff/ffffff.

static const uint32_t kGmcgaColor[4] = {
	0x000000ffu, // 0: black
	0x00aaaaffu, // 1: cyan
	0xaa00aaffu, // 2: magenta
	0xaaaaaaffu, // 3: grey (CGA "white" at low intensity)
};

static void blit_page(const uint8_t *page, uint32_t *out, int w, int h) {
	for (int y = 0; y < h; y++) {
		const int sy = (y * GMCGA_HEIGHT) / h;
		const uint8_t *row = &page[sy * GMCGA_STRIDE];
		for (int x = 0; x < w; x++) {
			const int sx = (x * GMCGA_WIDTH) / w;
			out[y * w + x] = kGmcgaColor[(row[sx >> 2] >> ((sx & 3) << 1)) & 0x3];
		}
	}
}

void gmcgaBlitToSurface(uint32_t *out, int w, int h) {
	blit_page(s_screenmem, out, w, h);
}

// Slow-draw control. These mirror the gm* entry points in graphics_magician.h, so the host
// drives whichever renderer is active with the same logic.

void gmcgaSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmcgaSlowDrawActive() { return s_slow.active(); }
bool gmcgaAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmcgaFinishSlowDraw() { s_slow.finish(); }
bool gmcgaSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmcgaBlitSlowToSurface(uint32_t *out, int w, int h) {
	blit_page(s_slowScreen, out, w, h);
}

} // namespace Comprehend
} // namespace Glk
