// The last step of the Apple II renderers' RGBA conversion: sampling a scanline's 560 artifact
// colours down to the surface width with the Apple palette.

#ifndef GLK_COMPREHEND_GM_BLIT_H
#define GLK_COMPREHEND_GM_BLIT_H

#include <cstdint>
#include "gm_artifact.h"

namespace Glk {
namespace Comprehend {

// Write one output scanline of `w` pixels (0xRRGGBBAA) from the 560 artifact
// colour indices of a row.  With `oneToOneWhenWide`, a surface w >= 560 reads
// column x directly (clamped to 559); otherwise column x*560/w.
inline void gmSampleRow560(const uint8_t colors560[560], uint32_t *out, int w,
						   bool oneToOneWhenWide) {
	const bool direct = oneToOneWhenWide && w >= 560;
	for (int x = 0; x < w; x++) {
		int sx = direct ? x : (x * 560) / w;
		if (sx > 559) sx = 559;
		uint32_t rgb = gm_apple2_palette[colors560[sx] & 0x0f];
		out[x] = (rgb << 8) | 0xff;
	}
}

} // namespace Comprehend
} // namespace Glk

#endif
